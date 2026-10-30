// AlphaZero-style self-play / evaluation for the swamp-knight puzzle (maximize total time).
// Modes:
//   az selfplay <weights> <out.bin> <episodes> <sims> <seed> <tempMoves> <bestfile> [<restartTour> <restartProb>]
//   az eval     <weights> <sims> <seed> <outfile>                 greedy play with a big search
//   az beam     <weights> <width> <lambda> <outfile> [filter]     beam ranked by T + lambda*10000*value
//   az improve  <weights> <tour> <sims> <stride> <beta> <outfile> [prefix]
//   az test     <weights>
// Settings read from environment variables (az_train.py sets these for you):
//   OPENING=1|0        1: start after the fixed opening in OPENING_FILE; 0: start at a1, T=0
//   OPENING_FILE       default data/opening_23.txt (relative to the working directory)
//   CPUCT              PUCT exploration constant (default 1.5; Q is min-max normalized to [0,1])
//   TAU1, TAU2         move-selection temperature for the first <tempMoves> moves, and after (0 = greedy)
//   DIRA, DIRE         Dirichlet root noise alpha and weight (default 0.3, 0.25)
//   NET_H, NET_LAYERS  network hidden width and number of hidden layers (default 256, 2)
//   SP                 1 (default): single-player deterministic search (max/exact values, solved subtrees,
//                      tree reuse, caches, tours found inside the search); 0: classic AlphaZero averaging
//   EST_W              single-player value target: weight of the search estimate above the achieved floor (0.5)
//   FOLLOW_EXACT       single-player: near-greedy moves (temperature <= 0.1) follow the best proven line (default 1)
//   QMAX_W             weight of the best result vs the average in node values (default 0.5 with SP=1)
//   EVAL_CACHE, SOLVED_CACHE   cache sizes per worker (default 200000 network outputs, 1000000 solved values)
//   CREDIT             fraction of its minutes a stuck tour keeps (training-only partial credit, default 0)
//   DISTK              if > 0, a stuck tour keeps exp(-DISTK * d) of its minutes instead, d = 3D distance
//                      from the knight to h8 divided by the a1 -> h8 distance at the start (so d = 1 at a1)
//   PREFIX_TOURS, PREFIX_LEN   curriculum: each self-play game starts after the first PREFIX_LEN moves of a
//                      tour picked at random from PREFIX_TOURS (one tour per line). With PREFIX_FROM_END=1 it
//                      starts PREFIX_LEN moves before that tour's finish; PREFIX_LEN=-1 picks a random point.
//                      PREFIX_BALANCE: tours are grouped by their first 10 moves and a group is picked with
//                      probability ~ size^PREFIX_BALANCE (default 1 = every tour equally likely; 0 = every opening)
//   TOURS_OUT          if set, self-play appends every finished tour to this file ("minutes tour" per line)
#include "eng.h"
#include <array>
#include <map>
#include <unordered_map>
#include <random>

static const int NA = 24;
static int ADX[NA], ADY[NA], ADZ[NA];
static const int D_IN = 64 * 21 + 64 + 10;  // 1418
static int H = 256, NLAYERS = 2;  // hidden width and number of hidden layers (env NET_H, NET_LAYERS)
static const double TSCALE = 10000.0;
static inline float score_value(double T) { return (float)min(1.0, log(max(T, 1.0)) / log(20000.0)); }

static void init_actions() {
  int k = 0;
  int kn[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
  for (auto& d : kn) { ADX[k] = d[0]; ADY[k] = d[1]; ADZ[k] = 0; k++; }
  int ad[4][2] = {{1,0},{0,1},{-1,0},{0,-1}};
  for (auto& d : ad) for (int z : {2, -2}) { ADX[k] = d[0]; ADY[k] = d[1]; ADZ[k] = z; k++; }
  for (auto& d : ad) for (int z : {1, -1}) { ADX[k] = 2 * d[0]; ADY[k] = 2 * d[1]; ADZ[k] = z; k++; }
}

// returns delta (sink) or -1 if illegal; sets q
static inline int act_legal(const S& s, int a, int& q) {
  int p = s.pos, x = p % 8 + ADX[a], y = p / 8 + ADY[a];
  if (x < 0 || x > 7 || y < 0 || y > 7) return -1;
  q = x + 8 * y;
  if (q != H8 && s.vis[q] >= 3) return -1;
  int O = 63 - p;
  if (s.alt[q] == s.alt[p] && q != O) return ADZ[a] == 0 ? 0 : -1;  // same group: only flat knight move
  int delta = ADZ[a] - (s.alt[q] - s.alt[p]);
  return delta >= 0 ? delta : -1;
}

// ---------------- network ----------------
struct Net {
  // file layout: W1 (D_IN x H), b1, then per extra hidden layer W (H x H), b, then Wp (H x NA), bp, wv (H), bv
  vector<float> W1, b1, Wp, bp, wv; vector<vector<float>> Wh, bh; float bv;
  bool load(const char* fn) {
    FILE* f = fopen(fn, "rb"); if (!f) return false;
    auto rd = [&](vector<float>& v, size_t n) { v.resize(n); return fread(v.data(), 4, n, f) == n; };
    bool ok = rd(W1, (size_t)D_IN * H) && rd(b1, H);
    Wh.assign(NLAYERS - 1, {}); bh.assign(NLAYERS - 1, {});
    for (int l = 0; l < NLAYERS - 1 && ok; l++) ok = rd(Wh[l], (size_t)H * H) && rd(bh[l], H);
    ok = ok && rd(Wp, (size_t)H * NA) && rd(bp, NA) && rd(wv, H) && fread(&bv, 4, 1, f) == 1;
    char extra; if (ok && fread(&extra, 1, 1, f) == 1) ok = false;  // file is bigger: wrong NET_H / NET_LAYERS
    fclose(f); return ok;
  }
} NET;

struct Feat { int idx[64 * 2 + 1]; int nidx; float g[10]; };

static void featurize(const S& s, Feat& F) {
  int k = s.pos, A = s.alt[k], n = 0;
  int c0 = 0, cm1 = 0, cm2 = 0, cp1 = 0, cp2 = 0, rem = 0;
  F.nidx = 0;
  for (int i = 0; i < 64; i++) {
    int r = s.alt[i] - A; int rc = r < -8 ? -8 : (r > 8 ? 8 : r);
    F.idx[F.nidx++] = i * 21 + (rc + 8);
    F.idx[F.nidx++] = i * 21 + 17 + s.vis[i];
    if (r == 0) { c0++; if (i != 63 - k) n++; } else if (r == -1) cm1++; else if (r == -2) cm2++; else if (r == 1) cp1++; else if (r == 2) cp2++;
    if (i != 63) rem += 3 - s.vis[i];
  }
  F.idx[F.nidx++] = 64 * 21 + k;
  auto cl = [](int v, int m) { return (float)(v < -m ? -m : (v > m ? m : v)) / m; };
  F.g[0] = n / 64.f; F.g[1] = cl(s.alt[63] - A, 16); F.g[2] = s.T / (float)TSCALE;
  F.g[3] = c0 / 64.f; F.g[4] = cm1 / 64.f; F.g[5] = cm2 / 64.f; F.g[6] = cp1 / 64.f; F.g[7] = cp2 / 64.f;
  F.g[8] = rem / 189.f; F.g[9] = cl(s.alt[63 - k] - A, 16);
}

static void forward(const Feat& F, float* logits, float& value) {
  static vector<float> ha, hb; ha.assign(H, 0); hb.assign(H, 0);
  float* h = ha.data(); float* h2 = hb.data();
  for (int j = 0; j < H; j++) h[j] = NET.b1[j];
  for (int t = 0; t < F.nidx; t++) { const float* w = &NET.W1[(size_t)F.idx[t] * H]; for (int j = 0; j < H; j++) h[j] += w[j]; }
  for (int g = 0; g < 10; g++) { const float* w = &NET.W1[(size_t)(64 * 21 + 64 + g) * H]; float v = F.g[g]; for (int j = 0; j < H; j++) h[j] += v * w[j]; }
  for (int j = 0; j < H; j++) h[j] = h[j] > 0 ? h[j] : 0;
  for (int l = 0; l < NLAYERS - 1; l++) {
    const vector<float>& W = NET.Wh[l];
    for (int j = 0; j < H; j++) h2[j] = NET.bh[l][j];
    for (int i = 0; i < H; i++) { float a = h[i]; if (a == 0) continue; const float* w = &W[(size_t)i * H]; for (int j = 0; j < H; j++) h2[j] += a * w[j]; }
    for (int j = 0; j < H; j++) h2[j] = h2[j] > 0 ? h2[j] : 0;
    std::swap(h, h2);
  }
  for (int a = 0; a < NA; a++) logits[a] = NET.bp[a];
  float v = NET.bv;
  for (int i = 0; i < H; i++) { float x = h[i]; if (x == 0) continue; const float* w = &NET.Wp[(size_t)i * NA]; for (int a = 0; a < NA; a++) logits[a] += x * w[a]; v += x * NET.wv[i]; }
  value = v;
}

// ---------------- MCTS ----------------
struct Node {
  S s; bool hasState = false;
  int parent = -1, act = -1; float prior = 0;
  int N = 0; double W = 0;
  bool expanded = false, terminal = false; float tval = 0; float r = 0; double bestG = -1e18;
  int first = -1, nch = 0; bool pendingEval = false;
  // single-player extras: best result over paths that reached a real end (achievable), and whether the
  // whole subtree has been searched (then exactG is this node's true value)
  double exactG = -1e18; bool solved = false; int nsolved = 0;
  double finG = -1e18;  // best result over paths that reached h8 (the only lines worth following)
};
static vector<Node> T;
static double QMIN, QMAX;
static double CPUCT = 1.5; static double ZMIX = 0.5; static double BETA = 0.0;
static double EST_W = 0.5;
static bool FOLLOW_EXACT = true;  // single-player: near-greedy moves follow the best proven line when there is one  // single-player target: weight of the search estimate above the achieved floor
// exploration knobs (env vars): TAU1 = move temperature for the first TEMPM moves, TAU2 = after (0 = greedy)
static double TAU1 = 1.0, TAU2 = 0.0, DIRA = 0.3, DIRE = 0.25;
static double CREDIT = 0.0;  // env CREDIT: fraction of its minutes a stuck tour keeps (training only)
// env DISTK > 0: a stuck tour instead keeps exp(-DISTK * d) of its minutes, where d is the straight-line
// 3D distance (x, y, altitude) from the knight to h8, divided by that distance at the start (a1 -> h8).
static double DISTK = 0.0, D0 = 1.0;
static inline double h8_dist_raw(const S& s) {
  int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  return sqrt(dx * dx + dy * dy + dz * dz);
}
static inline double h8_dist(const S& s) { return h8_dist_raw(s) / D0; }
static inline double stuck_credit(const S& s) { return DISTK > 0 ? exp(-DISTK * h8_dist(s)) : CREDIT; }
static double envd(const char* k, double d) { const char* v = getenv(k); return v ? atof(v) : d; }
static std::mt19937 rng;
// Single-player deterministic mode (env SP, default 1): see the self-play section in main for what it changes.
static bool SP = true;
static inline uint64_t state_key(const S& s) { return hashS(s, true) ^ (0x9E3779B97F4A7C15ULL * (uint64_t)(s.T + 1)); }
static unordered_map<uint64_t, pair<double, double>> SOLVED_CACHE;  // (exact, best finish)  // exact future value of positions whose subtree was fully searched
static size_t SOLVED_CAP = 1000000;

static void ensure_state(int id) {
  Node& nd = T[id]; if (nd.hasState) return;
  const S& ps = T[nd.parent].s; int q; int delta = act_legal(ps, nd.act, q);
  nd.s = ps; apply_move(nd.s, delta, q); nd.hasState = true;
  nd.r = (float)((nd.s.T - ps.T) / TSCALE);
  if (q == H8) { nd.terminal = true; nd.tval = 0.f; }
}

// expansion in two halves so evaluations can be batched: prep (terminal check + features), finish (priors)
static bool expand_prep(int id, Feat& F, float& tv) {
  Node& nd = T[id];
  if (nd.terminal) { tv = nd.tval; return false; }
  bool any = false;
  for (int a = 0; a < NA && !any; a++) { int q; if (act_legal(nd.s, a, q) >= 0) any = true; }
  if (!any) { nd.terminal = true; nd.tval = (float)(-(1.0 - stuck_credit(nd.s)) * nd.s.T / TSCALE); tv = nd.tval; return false; }
  featurize(nd.s, F); return true;
}
static void expand_finish(int id, const float* lg) {
  int legal[NA], nl = 0;
  for (int a = 0; a < NA; a++) { int q; if (act_legal(T[id].s, a, q) >= 0) legal[nl++] = a; }
  float mx = -1e30f; for (int i = 0; i < nl; i++) mx = max(mx, lg[legal[i]]);
  float sum = 0, pr[NA]; for (int i = 0; i < nl; i++) { pr[i] = expf(lg[legal[i]] - mx); sum += pr[i]; }
  int first = (int)T.size();
  for (int i = 0; i < nl; i++) { Node c; c.parent = id; c.act = legal[i]; c.prior = pr[i] / sum; T.push_back(c); }
  T[id].first = first; T[id].nch = nl; T[id].expanded = true;
}
// expand node: returns value estimate
static float expand(int id) {
  Feat F; float tv; if (!expand_prep(id, F, tv)) return tv;
  float lg[NA], v; forward(F, lg, v); expand_finish(id, lg); return v;
}

static void add_noise(int id, double alpha, double eps) {
  Node& nd = T[id]; if (nd.nch == 0) return;
  std::gamma_distribution<double> g(alpha, 1.0); vector<double> d(nd.nch); double s = 0;
  for (auto& x : d) { x = g(rng); s += x; }
  for (int i = 0; i < nd.nch; i++) T[nd.first + i].prior = (float)((1 - eps) * T[nd.first + i].prior + eps * d[i] / s);
}

// value of moving into ch, seen from its parent: exact if solved, else a mean/max blend (BETA = max weight)
static inline double node_q(const Node& ch) {
  if (ch.solved) return ch.r + ch.exactG;
  double mean = ch.W / ch.N;
  if (ch.bestG < -1e17) return ch.r + mean;  // only virtual visits so far: no real result yet
  double v = (1 - BETA) * mean + BETA * ch.bestG;
  if (SP && ch.exactG > v) v = ch.exactG;  // a proven line is replayable exactly: never rate a move below it
  return ch.r + v;
}
static int select_child(int id) {
  Node& nd = T[id]; double best = -HUGE_VAL; int bc = nd.first;
  double rng_q = QMAX - QMIN;
  double qp = nd.N ? (SP && nd.bestG > -1e17 ? (1 - BETA) * nd.W / nd.N + BETA * nd.bestG : nd.W / nd.N) : 0;
  double qpn = rng_q > 1e-9 ? (qp - QMIN) / rng_q : 0.5;
  double sq = sqrt((double)nd.N + 1);
  for (int c = nd.first; c < nd.first + nd.nch; c++) {
    Node& ch = T[c];
    double q;
    if (ch.N || ch.solved) { double qe = node_q(ch); q = rng_q > 1e-9 ? (qe - QMIN) / rng_q : 0.5; }
    else q = qpn;
    double u = q + CPUCT * ch.prior * sq / (1 + ch.N);
    if (u > best) { best = u; bc = c; }
  }
  return bc;
}
// back up value g from node id to the root; exact = g comes from a real ending (terminal or solved)
static void backup(int id, double g, bool exact = false, double fin = -1e18) {
  while (id >= 0) {
    Node& nd = T[id]; nd.N++; nd.W += g; if (g > nd.bestG) nd.bestG = g;
    if (exact && g > nd.exactG) nd.exactG = g;
    if (fin > -1e17 && fin > nd.finG) nd.finG = fin;
    if (nd.parent >= 0) { double qe = node_q(nd); QMIN = min(QMIN, qe); QMAX = max(QMAX, qe); }
    g = nd.r + g; if (fin > -1e17) fin = nd.r + fin; id = nd.parent;
  }
}
// no position is worth less than getting stuck from it, which loses the minutes earned so far
static inline float value_floor(const S& s) { return (float)(-s.T / TSCALE); }
// mark id solved (its exactG is its true value) and solve ancestors whose children are now all solved
static void mark_solved(int id) {
  while (id >= 0 && !T[id].solved) {
    Node& nd = T[id]; nd.solved = true;
    if (!nd.terminal && SOLVED_CACHE.size() < SOLVED_CAP) SOLVED_CACHE[state_key(nd.s)] = {nd.exactG, nd.finG};
    if (nd.parent >= 0) { double qe = nd.r + nd.exactG; QMIN = min(QMIN, qe); QMAX = max(QMAX, qe); }
    int p = nd.parent; if (p < 0) break;
    Node& pn = T[p]; pn.nsolved++;
    if (!(pn.expanded && pn.nsolved == pn.nch)) break;
    double best = -1e18, bfin = -1e18;
    for (int c = pn.first; c < pn.first + pn.nch; c++) { best = max(best, T[c].r + T[c].exactG); if (T[c].finG > -1e17) bfin = max(bfin, T[c].r + T[c].finG); }
    pn.exactG = max(pn.exactG, best); pn.finG = max(pn.finG, bfin);
    id = p;
  }
}
// keep only the subtree under child c, which becomes the new root (index 0)
static void reroot(int c) {
  vector<Node> nt; nt.reserve(T.size());
  Node r = T[c]; r.parent = -1; nt.push_back(r);
  vector<pair<int, int>> q{{c, 0}}; size_t h = 0;
  while (h < q.size()) {
    auto [o, n] = q[h++]; const Node& on = T[o];
    if (!on.expanded || on.nch == 0) continue;
    int nf = (int)nt.size(); nt[n].first = nf;
    for (int k = 0; k < on.nch; k++) { Node x = T[on.first + k]; x.parent = n; nt.push_back(x); q.push_back({on.first + k, nf + k}); }
  }
  T.swap(nt);
  QMIN = 1e9; QMAX = -1e9;
  for (size_t i = 1; i < T.size(); i++) if (T[i].N || T[i].solved) { double qe = node_q(T[i]); QMIN = min(QMIN, qe); QMAX = max(QMAX, qe); }
}
static void run_sims(int root, int sims) {
  for (int it = 0; it < sims; it++) {
    int id = root;
    while (T[id].expanded && !T[id].terminal) { id = select_child(id); ensure_state(id); }
    double g = T[id].terminal ? T[id].tval : expand(id);
    backup(id, g);
  }
}

// ---- batched evaluation over a pipe (GPU self-play) ----
#pragma pack(push, 1)
struct EvalReq { int16_t idx[64 * 2 + 1]; float g[10]; };
#pragma pack(pop)
static FILE *INF_REQ = nullptr, *INF_RESP = nullptr;
static void remote_eval(const vector<EvalReq>& reqs, vector<float>& out) {
  int32_t n = (int32_t)reqs.size();
  if (fwrite(&n, 4, 1, INF_REQ) != 1 || fwrite(reqs.data(), sizeof(EvalReq), n, INF_REQ) != (size_t)n) { fprintf(stderr, "inference pipe write failed\n"); exit(1); }
  fflush(INF_REQ);
  out.resize((size_t)n * (NA + 1));
  if (fread(out.data(), 4, out.size(), INF_RESP) != out.size()) { fprintf(stderr, "inference pipe read failed\n"); exit(1); }
}
static EvalReq pack_feat(const Feat& F) {
  EvalReq r; for (int i = 0; i < 64 * 2 + 1; i++) r.idx[i] = (int16_t)F.idx[i]; for (int g = 0; g < 10; g++) r.g[g] = F.g[g]; return r;
}

static int USE_OPENING = 1;  // env OPENING=0: every game starts at a1, T=0
static string OPENING_FILE = "data/opening_23.txt";
static S start_with_opening(vector<pair<int, string>>& moves) {
  S s = start_state();
  if (!USE_OPENING) return s;
  ifstream f(OPENING_FILE.c_str());
  if (!f) { fprintf(stderr, "cannot open opening file %s (set OPENING_FILE, or OPENING=0 to start at a1)\n", OPENING_FILE.c_str()); exit(1); }
  int t; string q;
  while (f >> t >> q) { int qi = (q[0] - 'a') + 8 * (q[1] - '1'); int n = group_n(s, s.pos); apply_move(s, t / n, qi); moves.push_back({t, q}); }
  return s;
}

static vector<pair<int, string>> read_tour(const char* fn) {
  vector<pair<int, string>> mv; FILE* f = fopen(fn, "r"); if (!f) return mv;
  char line[200000]; if (!fgets(line, sizeof line, f)) { fclose(f); return mv; }
  if (!fgets(line, sizeof line, f)) { fclose(f); return mv; }
  fclose(f); char* p = line; int t; char sq[8];
  while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &t, sq) == 2) { mv.push_back({t, string(sq, 2)}); p++; }
  return mv;
}
// a set of tours, one per line ("(t, sq), ..." pairs; anything before the first '(' is ignored)
static vector<vector<pair<int, string>>> read_tour_set(const char* fn) {
  vector<vector<pair<int, string>>> out; FILE* f = fopen(fn, "r");
  if (!f) { fprintf(stderr, "cannot open tour file %s\n", fn); exit(1); }
  static char line[200000];
  while (fgets(line, sizeof line, f)) {
    vector<pair<int, string>> mv; char* p = line; int t; char sq[8];
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &t, sq) == 2) { mv.push_back({t, string(sq, 2)}); p++; }
    if (mv.size() > 1) out.push_back(mv);
  }
  fclose(f); return out;
}
#pragma pack(push, 1)
struct Sample { int16_t alt[64]; uint8_t vis[64]; uint8_t pos; int32_t T; uint32_t mask; float pi[NA]; float z; };
#pragma pack(pop)

int main(int argc, char** argv) {
  init_board(); init_actions();
  CPUCT = envd("CPUCT", 1.5); TAU1 = envd("TAU1", 1.0); TAU2 = envd("TAU2", 0.0); DIRA = envd("DIRA", 0.3); DIRE = envd("DIRE", 0.25);
  SP = envd("SP", 1) != 0; BETA = envd("QMAX_W", SP ? 0.5 : 0.0); EST_W = envd("EST_W", 0.5); FOLLOW_EXACT = envd("FOLLOW_EXACT", 1) != 0;
  USE_OPENING = (int)envd("OPENING", 1); CREDIT = envd("CREDIT", 0.0); DISTK = envd("DISTK", 0.0);
  D0 = h8_dist_raw(start_state());
  if (const char* of = getenv("OPENING_FILE")) OPENING_FILE = of;
  if (argc < 3) {
    fprintf(stderr, "usage: az selfplay|eval|beam|improve|test <weights> ...  (see the comment at the top of az.cpp)\n");
    return 1;
  }
  H = (int)envd("NET_H", 256); NLAYERS = max(1, (int)envd("NET_LAYERS", 2));
  string mode = argv[1];
  if (!NET.load(argv[2])) { fprintf(stderr, "cannot load weights %s for NET_H=%d NET_LAYERS=%d\n", argv[2], H, NLAYERS); return 1; }
  if (mode == "selfplay") {
    const char* out = argv[3]; int EP = atoi(argv[4]), SIMS = atoi(argv[5]); rng.seed(atoi(argv[6]));
    int TEMPM = atoi(argv[7]); const char* bestf = argv[8];
    vector<pair<int, string>> pref; double PSTART = 0; if (argc > 10) { pref = read_tour(argv[9]); PSTART = atof(argv[10]); }
    int OPEN_LEN = 0; { vector<pair<int, string>> tmp; start_with_opening(tmp); OPEN_LEN = tmp.size(); }
    // curriculum: start each game after the first PREFIX_LEN moves of a random tour from PREFIX_TOURS
    vector<vector<pair<int, string>>> pset; int PREFIX_LEN = (int)envd("PREFIX_LEN", 0);
    bool PREFIX_FROM_END = envd("PREFIX_FROM_END", 0) != 0;
    if (const char* pt = getenv("PREFIX_TOURS")) pset = read_tour_set(pt);
    // group the start tours by their first 10 moves; a group is picked with probability proportional to
    // size^PREFIX_BALANCE (1 = every tour equally likely, 0 = every opening equally likely), then a tour in it
    vector<vector<int>> pgroups; vector<double> pgcum;
    if (!pset.empty()) {
      double PB = envd("PREFIX_BALANCE", 1.0); map<string, vector<int>> g;
      for (int i = 0; i < (int)pset.size(); i++) {
        string key; for (int j = 0; j < min(10, (int)pset[i].size()); j++) key += to_string(pset[i][j].first) + pset[i][j].second + ",";
        g[key].push_back(i);
      }
      double acc = 0;
      for (auto& kv : g) { pgroups.push_back(kv.second); acc += pow((double)kv.second.size(), PB); pgcum.push_back(acc); }
    }
    const char* tours_out = getenv("TOURS_OUT");  // if set, every finished tour is appended here
    FILE* fo = fopen(out, "wb");
    long long bestT = -1; double bestTarget = -1;
    std::uniform_real_distribution<double> U01(0, 1);
    // where a game starts: the opening (or a1), a restart point on the best tour, or a curriculum prefix
    auto make_start = [&](vector<pair<int, string>>& moves) -> S {
      S s = start_with_opening(moves);
      if (!pref.empty() && (int)pref.size() > OPEN_LEN + 1 && U01(rng) < PSTART) {
        int k = OPEN_LEN + (int)(U01(rng) * (pref.size() - 1 - OPEN_LEN));
        s = start_state(); moves.clear();
        for (int i = 0; i < k; i++) { int qi = (pref[i].second[0] - 'a') + 8 * (pref[i].second[1] - '1'); int n = group_n(s, s.pos); apply_move(s, pref[i].first / n, qi); moves.push_back(pref[i]); }
      }
      if (!pset.empty() && PREFIX_LEN != 0) {
        std::uniform_real_distribution<double> UG(0, pgcum.back());
        int gi = (int)(lower_bound(pgcum.begin(), pgcum.end(), UG(rng)) - pgcum.begin()); gi = min(gi, (int)pgroups.size() - 1);
        std::uniform_int_distribution<int> UI(0, (int)pgroups[gi].size() - 1); const auto& tr = pset[pgroups[gi][UI(rng)]];
        int L = (int)tr.size(), k;
        if (PREFIX_LEN < 0) { std::uniform_int_distribution<int> UK(0, L - 1); k = UK(rng); }  // random branch point
        else if (PREFIX_FROM_END) k = max(0, L - PREFIX_LEN);                                    // PREFIX_LEN moves before h8
        else k = min(PREFIX_LEN, L - 1);                                                       // PREFIX_LEN moves in
        s = start_state(); moves.clear();
        for (int i = 0; i < k; i++) { int qi = (tr[i].second[0] - 'a') + 8 * (tr[i].second[1] - '1'); int n = group_n(s, s.pos); apply_move(s, tr[i].first / n, qi); moves.push_back(tr[i]); }
      }
      return s;
    };
    // Self-play engine. All EP games advance together; leaf positions are evaluated in batches (here on the
    // CPU, or by the trainer over a pipe when INFER_REQ is set). In single-player mode (SP=1):
    //  - values back up as a mean/max blend (QMAX_W), plus the best exact result (paths that really ended)
    //  - fully searched subtrees are solved: their exact value is final, they are not searched again, and a
    //    solved root means the best move is known exactly
    //  - the subtree under the chosen move is kept for the next move
    //  - network outputs and solved values are cached by position
    //  - every h8 reached inside the search is a real tour; the best per game is reported
    //  - value targets never go below the best result actually achieved from that position
    bool remote = getenv("INFER_REQ") != nullptr;
    int KLEAF = max(1, (int)envd("INFER_LEAVES", remote ? 8 : 1));
    size_t EVAL_CAP = (size_t)envd("EVAL_CACHE", 200000); SOLVED_CAP = (size_t)envd("SOLVED_CACHE", 1000000);
    unordered_map<uint64_t, array<float, NA + 1>> evalCache;
    if (remote) {
      INF_REQ = fopen(getenv("INFER_REQ"), "wb"); INF_RESP = fopen(getenv("INFER_RESP"), "rb");
      if (!INF_REQ || !INF_RESP) { fprintf(stderr, "cannot open inference pipes\n"); return 1; }
    }
    struct Game {
      S s; vector<pair<int, string>> moves; vector<Sample> traj; vector<float> rootEst; vector<double> rootExact;
      int mv = 0; long long score = 0; bool done = false, rootPending = false; int phase = 0, simsDone = 0, pending = 0;
      vector<Node> tree; double qmin = 1e9, qmax = -1e9;
      long long bestFound = 0; vector<pair<int, string>> bestFoundTour;
    };
    struct Pend { int game, leaf; uint64_t key; vector<pair<int, double>> vl; };
    // a terminal at h8 inside the tree: the played moves plus the tree path form a complete tour
    auto found_tour = [&](Game& G, int id) {
      if (!SP || T[id].s.pos != H8 || T[id].s.T <= G.bestFound) return;
      G.bestFound = T[id].s.T; vector<pair<int, string>> tail;
      for (int x = id; T[x].parent >= 0; x = T[x].parent) tail.push_back({T[x].s.T - T[T[x].parent].s.T, nm(T[x].s.pos)});
      G.bestFoundTour = G.moves; G.bestFoundTour.insert(G.bestFoundTour.end(), tail.rbegin(), tail.rend());
    };
    auto best_exact_child = [&]() {
      int bc = -1; double bv = -1e18;
      for (int c = T[0].first; c < T[0].first + T[0].nch; c++) {
        double v = T[c].r + T[c].exactG;
        if (v > bv + 1e-12 || (fabs(v - bv) <= 1e-12 && bc >= 0 && T[c].N > T[bc].N)) { bv = v; bc = c; }
      }
      return bc;
    };
    // after the search at a root (tree in T): record the training sample and choose the move
    auto record_and_pick = [&](Game& G) -> int {
      const S& s = G.s; bool rootSolved = SP && T[0].solved;
      Sample sm; for (int i = 0; i < 64; i++) { sm.alt[i] = s.alt[i]; sm.vis[i] = s.vis[i]; } sm.pos = s.pos; sm.T = s.T; sm.mask = 0;
      for (int a = 0; a < NA; a++) sm.pi[a] = 0;
      int tot = 0; for (int c = T[0].first; c < T[0].first + T[0].nch; c++) { sm.mask |= 1u << T[c].act; sm.pi[T[c].act] = T[c].N; tot += T[c].N; }
      int bestc = rootSolved ? best_exact_child() : -1;
      if (rootSolved) { for (int a = 0; a < NA; a++) sm.pi[a] = 0; sm.pi[T[bestc].act] = 1; }
      else for (int a = 0; a < NA; a++) sm.pi[a] /= max(1, tot);
      G.traj.push_back(sm);
      double est = rootSolved ? T[0].exactG : (T[0].N ? (SP && T[0].bestG > -1e17 ? (1 - BETA) * T[0].W / T[0].N + BETA * T[0].bestG : T[0].W / T[0].N) : 0.0);
      G.rootEst.push_back((float)est); G.rootExact.push_back(T[0].exactG);
      if (rootSolved) return bestc;
      int pick = -1;
      double tau = G.mv < TEMPM ? TAU1 : TAU2;
      if (tau > 0) {  // sample in proportion to N^(1/tau)
        double wt[NA], wsum = 0;
        for (int i = 0; i < T[0].nch; i++) { int c = T[0].first + i; wt[i] = T[c].N > 0 ? pow((double)T[c].N, 1.0 / tau) : 0.0; wsum += wt[i]; }
        std::uniform_real_distribution<double> U(0, 1); double r0 = U(rng) * wsum, acc = 0;
        for (int i = 0; i < T[0].nch; i++) { acc += wt[i]; if (wt[i] > 0 && acc >= r0) { pick = T[0].first + i; break; } }
      }
      if (pick < 0) { int bn = -1; for (int c = T[0].first; c < T[0].first + T[0].nch; c++) if (T[c].N > bn) { bn = T[c].N; pick = c; } }
      // deterministic single player: a proven complete line from here can always be replayed, so near-greedy
      // play follows the best one found (later searches can still replace it with a better proven line)
      if (SP && FOLLOW_EXACT && tau <= 0.1 && T[0].finG > -1e17) {
        int bc = -1; double bv = -1e18;
        for (int c = T[0].first; c < T[0].first + T[0].nch; c++) if (T[c].finG > -1e17 && T[c].r + T[c].finG > bv) { bv = T[c].r + T[c].finG; bc = c; }
        if (bc >= 0) pick = bc;
      }
      return pick;
    };
    int epcount = 0;
    auto write_tour = [&](FILE* f, const vector<pair<int, string>>& mvs) {
      for (size_t i = 0; i < mvs.size(); i++) fprintf(f, "%s(%d, %s)", i ? ", " : "", mvs[i].first, mvs[i].second.c_str());
      fprintf(f, "\n");
    };
    auto finish_episode = [&](Game& G) {
      int ep = epcount++; const S& s = G.s; long long score = G.score;
      long long Tend = s.T; double dist = h8_dist(s), cr = stuck_credit(s);
      int n = (int)G.traj.size();
      if (!SP) {
        for (int i = 0; i < n; i++) {
          float z = (float)(score > 0 ? (score - G.traj[i].T) / TSCALE : (cr * Tend - G.traj[i].T) / TSCALE);
          G.traj[i].z = (float)(ZMIX * z + (1 - ZMIX) * G.rootEst[i]);
        }
      } else {  // exact floor: best of what was played and what any search found, passed back along the game
        double next = score > 0 ? 0.0 : -(1.0 - cr) * Tend / TSCALE;
        for (int i = n - 1; i >= 0; i--) {
          double Tn = i + 1 < n ? G.traj[i + 1].T : Tend;
          double ex = max(G.rootExact[i], (Tn - G.traj[i].T) / TSCALE + next);
          G.traj[i].z = (float)max(ex, EST_W * G.rootEst[i] + (1 - EST_W) * ex);
          next = ex;
        }
      }
      for (int i = 0; i < n; i++) fwrite(&G.traj[i], sizeof(Sample), 1, fo);
      long long found = max(G.bestFound, score);
      printf("EP %d moves %d T %lld end %lld dist %.4f found %lld\n", ep, G.mv, score, Tend, score > 0 ? 0.0 : dist, found); fflush(stdout);
      const vector<pair<int, string>>& bestTour = G.bestFound > score ? G.bestFoundTour : G.moves;
      if (tours_out) {
        FILE* ft = fopen(tours_out, "a");
        if (score > 0) { fprintf(ft, "%lld ", score); write_tour(ft, G.moves); }
        if (G.bestFound > score) { fprintf(ft, "%lld ", G.bestFound); write_tour(ft, G.bestFoundTour); }
        fclose(ft);
      }
      double target = score > 0 ? (double)score : cr * Tend;
      if (G.bestFound > target) target = (double)G.bestFound;
      if (target > bestTarget) {
        bestTarget = target;
        FILE* fb = fopen((string(bestf) + ".target").c_str(), "w"); fprintf(fb, "%.1f\n", target);
        write_tour(fb, G.bestFound > score ? G.bestFoundTour : G.moves); fclose(fb);
      }
      if (found > bestT) { bestT = found; FILE* fb = fopen(bestf, "w"); fprintf(fb, "%lld\n", found); write_tour(fb, bestTour); fclose(fb); }
    };
    auto evaluate = [&](const vector<EvalReq>& reqs, const vector<Feat>& feats, vector<float>& out) {
      if (remote) { remote_eval(reqs, out); return; }
      out.resize(feats.size() * (NA + 1));
      for (size_t k = 0; k < feats.size(); k++) forward(feats[k], &out[k * (NA + 1)], out[k * (NA + 1) + NA]);
    };

    vector<Game> games(EP);
    for (auto& G : games) G.s = make_start(G.moves);
    int active = EP; vector<EvalReq> reqs; vector<Feat> feats; vector<Pend> owners; vector<float> evout;
    while (active > 0) {
      reqs.clear(); feats.clear(); owners.clear();
      for (int gi = 0; gi < EP; gi++) {
        Game& G = games[gi]; if (G.done) continue;
        std::swap(T, G.tree); QMIN = G.qmin; QMAX = G.qmax;
        // 1) search finished (or root solved): record the position and make the move
        if (G.phase == 1 && !G.rootPending && G.pending == 0 && (G.simsDone >= SIMS || (SP && T[0].solved))) {
          int pick = record_and_pick(G);
          int a = T[pick].act, q; int delta = act_legal(G.s, a, q);
          int before = G.s.T; apply_move(G.s, delta, q); G.moves.push_back({G.s.T - before, nm(q)}); G.mv++;
          if (q == H8) { G.score = G.s.T; G.done = true; } else if (G.mv > 400) G.done = true;
          else if (SP) { ensure_state(pick); reroot(pick); }
          else T.clear();
          G.phase = 0;
        }
        // 2) new move: set up the root (reused subtree, cached output, or one evaluation)
        if (!G.done && G.phase == 0) {
          if (T.empty()) { Node r; r.s = G.s; r.hasState = true; T.push_back(r); QMIN = 1e9; QMAX = -1e9; }
          T.reserve(T.size() + (size_t)SIMS * 8 + 16);
          if (T[0].solved && !T[0].expanded) T[0].solved = false;  // solved via cache: expand to find the move
          G.phase = 1; G.simsDone = 0; G.pending = 0; G.rootPending = false;
          if (T[0].expanded) { if (!T[0].solved) add_noise(0, DIRA, DIRE); }
          else {
            Feat F; float tv;
            if (!expand_prep(0, F, tv)) G.done = true;  // stuck
            else {
              uint64_t key = SP ? state_key(T[0].s) : 0; auto it = SP ? evalCache.find(key) : evalCache.end();
              if (it != evalCache.end()) { expand_finish(0, it->second.data()); add_noise(0, DIRA, DIRE); }
              else { reqs.push_back(pack_feat(F)); feats.push_back(F); owners.push_back({gi, 0, key, {}}); G.rootPending = true; }
            }
          }
        }
        // 3) simulations: collect up to KLEAF leaves (virtual visits keep them apart); real endings, solved
        //    nodes and cached positions are handled right away
        if (!G.done && G.phase == 1 && !G.rootPending) {
          int tries = 0;
          while (G.simsDone + G.pending < SIMS && G.pending < KLEAF && tries++ < 4 * KLEAF) {
            if (SP && T[0].solved) break;
            vector<int> path{0}; int id = 0;
            while (T[id].expanded && !T[id].terminal && !T[id].solved) { id = select_child(id); ensure_state(id); path.push_back(id); }
            if (T[id].terminal || T[id].solved) {
              double g = T[id].terminal ? T[id].tval : T[id].exactG;
              double fin = T[id].terminal ? (T[id].s.pos == H8 ? g : -1e18) : T[id].finG;
              backup(id, g, true, fin); if (T[id].terminal) found_tour(G, id);
              if (SP) mark_solved(id);
              G.simsDone++; continue;
            }
            if (T[id].pendingEval) break;
            Feat F; float tv;
            if (!expand_prep(id, F, tv)) { backup(id, tv, true); if (SP) mark_solved(id); G.simsDone++; continue; }  // stuck
            uint64_t key = 0;
            if (SP) {
              key = state_key(T[id].s);
              auto is = SOLVED_CACHE.find(key);
              if (is != SOLVED_CACHE.end()) { backup(id, is->second.first, true, is->second.second); mark_solved(id); G.simsDone++; continue; }
              auto ie = evalCache.find(key);
              if (ie != evalCache.end()) { expand_finish(id, ie->second.data()); backup(id, max(ie->second[NA], value_floor(T[id].s))); G.simsDone++; continue; }
            }
            Pend pd{gi, id, key, {}};
            bool haveQ = QMAX >= QMIN;
            for (int x : path) { double amt = (x == 0 || !haveQ) ? 0.0 : QMIN - T[x].r; T[x].N++; T[x].W += amt; pd.vl.push_back({x, amt}); }
            T[id].pendingEval = true;
            reqs.push_back(pack_feat(F)); feats.push_back(F); owners.push_back(pd); G.pending++;
          }
        }
        if (G.done) { finish_episode(G); active--; vector<Node>().swap(T); }
        G.qmin = QMIN; G.qmax = QMAX; std::swap(T, G.tree);
      }
      if (reqs.empty()) continue;
      evaluate(reqs, feats, evout);
      for (size_t k = 0; k < owners.size(); k++) {
        Pend& pd = owners[k]; Game& G = games[pd.game];
        std::swap(T, G.tree); QMIN = G.qmin; QMAX = G.qmax;
        const float* lg = &evout[k * (NA + 1)]; float v = lg[NA];
        if (SP && evalCache.size() < EVAL_CAP) { array<float, NA + 1> e; std::copy(lg, lg + NA + 1, e.begin()); evalCache[pd.key] = e; }
        if (G.rootPending && pd.leaf == 0) { expand_finish(0, lg); add_noise(0, DIRA, DIRE); G.rootPending = false; }
        else {
          for (auto& [x, amt] : pd.vl) { T[x].N--; T[x].W -= amt; }
          T[pd.leaf].pendingEval = false; expand_finish(pd.leaf, lg);
          backup(pd.leaf, SP ? max(v, value_floor(T[pd.leaf].s)) : v); G.simsDone++; G.pending--;
        }
        G.qmin = QMIN; G.qmax = QMAX; std::swap(T, G.tree);
      }
      if (SP && evalCache.size() >= EVAL_CAP) evalCache.clear();
    }
    if (remote) { fclose(INF_REQ); fclose(INF_RESP); }
    fclose(fo);
  } else if (mode == "test") {
    vector<pair<int, string>> moves; S s = start_with_opening(moves);
    Feat F; featurize(s, F); float lg[NA], v; forward(F, lg, v);
    printf("pos %d T %d v %.6f lg", s.pos, s.T, v); for (int a = 0; a < NA; a++) printf(" %.5f", lg[a]); printf("\n");
    int legal = 0; for (int a = 0; a < NA; a++) { int q; if (act_legal(s, a, q) >= 0) legal |= 1 << a; } printf("mask %d\n", legal);
    return 0;
  } else if (mode == "improve") {
    // az improve <weights> <tourfile> <sims> <stride> <beta> <outfile> [startPrefix]
    vector<pair<int, string>> best = read_tour(argv[3]); int SIMS = atoi(argv[4]), STRIDE = atoi(argv[5]); BETA = atof(argv[6]);
    const char* outf = argv[7]; int P0 = argc > 8 ? atoi(argv[8]) : 23;
    auto replay = [&](const vector<pair<int, string>>& mv, int k) { S s = start_state(); for (int i = 0; i < k; i++) { int qi = (mv[i].second[0] - 'a') + 8 * (mv[i].second[1] - '1'); int n = group_n(s, s.pos); apply_move(s, mv[i].first / n, qi); } return s; };
    long long bestT = replay(best, best.size()).T;
    printf("start best %lld (%zu moves)\n", bestT, best.size()); fflush(stdout);
    for (int k = P0; k < (int)best.size() - 1; k += STRIDE) {
      S s = replay(best, k); vector<pair<int, string>> moves(best.begin(), best.begin() + k); long long score = 0;
      while (true) {
        T.clear(); Node r; r.s = s; r.hasState = true; T.push_back(r); QMIN = 1e9; QMAX = -1e9; expand(0);
        if (T[0].terminal) break;
        run_sims(0, SIMS);
        int pick = -1, bn = -1; for (int c = T[0].first; c < T[0].first + T[0].nch; c++) if (T[c].N > bn) { bn = T[c].N; pick = c; }
        int a = T[pick].act, q; int delta = act_legal(s, a, q); int before = s.T; apply_move(s, delta, q);
        moves.push_back({s.T - before, nm(q)});
        if (q == H8) { score = s.T; break; }
      }
      printf("prefix %d -> %lld\n", k, score); fflush(stdout);
      if (score > bestT) {
        bestT = score; best = moves;
        FILE* fb = fopen(outf, "w"); fprintf(fb, "%lld\n", score);
        for (size_t i = 0; i < moves.size(); i++) fprintf(fb, "%s(%d, %s)", i ? ", " : "", moves[i].first, moves[i].second.c_str());
        fprintf(fb, "\n"); fclose(fb); printf("IMPROVED %lld\n", score); fflush(stdout);
      }
    }
    return 0;
  } else if (mode == "beam") {
    // az beam <weights> <width> <lambda> <outfile>
    int WID = atoi(argv[3]); double LAM = atof(argv[4]); const char* outf = argv[5]; int FILT = argc > 6 ? atoi(argv[6]) : 0;
    int KN2[2] = {(5)+8*6, 6+8*5}, OR2[2] = {6+8*7, 7+8*6}, D22[2] = {5+8*7, 7+8*5};  // f7,g6 ; g8,h7 ; f8,h6
    auto fin_ok = [&](const S& c) {
      int K = c.alt[c.pos], h = c.alt[H8]; if (abs(h - K) > 2) return false;
      for (int q : KN2) if (c.alt[q] == h && c.vis[q] < 3) return true;
      for (int q : OR2) if (abs(c.alt[q] - h) == 2 && c.vis[q] < 3) return true;
      for (int q : D22) if (abs(c.alt[q] - h) == 1 && c.vis[q] < 3) return true;
      return false; };
    vector<pair<int, string>> open0; S s0 = start_with_opening(open0);
    struct BR { int par; int8_t a; int waited; };
    vector<vector<BR>> hist; vector<S> beam{s0}; vector<int> bidx{-1};
    long long bestT = -1; vector<pair<int, string>> bestMoves;
    auto moves_of = [&](int d, int idx) { vector<pair<int, string>> ms; vector<int> acts;
      while (d >= 0 && idx >= 0) { ms.push_back({hist[d][idx].waited, ""}); acts.push_back(hist[d][idx].a); idx = hist[d][idx].par; d--; }
      reverse(ms.begin(), ms.end()); reverse(acts.begin(), acts.end());
      vector<pair<int, string>> out = open0; S t = s0;
      for (size_t i = 0; i < acts.size(); i++) { int q; int dl = act_legal(t, acts[i], q); int b = t.T; apply_move(t, dl, q); out.push_back({t.T - b, nm(q)}); }
      return out; };
    for (int d = 0; d < 300; d++) {
      unordered_set<uint64_t> seen; vector<S> cand; vector<BR> crec; vector<double> sc;
      for (size_t bi = 0; bi < beam.size(); bi++) {
        const S& st = beam[bi];
        for (int a = 0; a < NA; a++) {
          int q; int dl = act_legal(st, a, q); if (dl < 0) continue;
          S c = st; apply_move(c, dl, q); int w = c.T - st.T;
          if (q == H8) {
            if (c.T > bestT) { bestT = c.T; bestMoves = moves_of(d - 1, bidx[bi]); bestMoves.push_back({w, "h8"});
              printf("FINISH T=%lld depth %d\n", bestT, d + 1); fflush(stdout);
              FILE* fb = fopen(outf, "w"); fprintf(fb, "%lld\n", bestT);
              for (size_t i = 0; i < bestMoves.size(); i++) fprintf(fb, "%s(%d, %s)", i ? ", " : "", bestMoves[i].first, bestMoves[i].second.c_str());
              fprintf(fb, "\n"); fclose(fb); }
            continue;
          }
          uint64_t h = hashS(c); if (seen.count(h)) continue; seen.insert(h);
          // need at least one legal move to be worth keeping
          bool any = false; for (int a2 = 0; a2 < NA && !any; a2++) { int q2; if (act_legal(c, a2, q2) >= 0) any = true; }
          if (!any) continue;
          if (FILT && d >= 1 && !fin_ok(c)) continue;
          double v = 0; if (LAM != 0) { Feat F; featurize(c, F); float lg[NA]; float vv; forward(F, lg, vv); v = vv; }
          cand.push_back(c); crec.push_back({bidx[bi], (int8_t)a, w}); sc.push_back(c.T + LAM * TSCALE * v);
        }
      }
      if (cand.empty()) break;
      vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
      if ((int)ord.size() > WID) { nth_element(ord.begin(), ord.begin() + WID, ord.end(), [&](int x, int y) { return sc[x] > sc[y]; }); ord.resize(WID); }
      vector<S> nb; vector<int> ni; vector<BR> hr;
      for (int i = 0; i < (int)ord.size(); i++) { nb.push_back(cand[ord[i]]); hr.push_back(crec[ord[i]]); ni.push_back(i); }
      hist.push_back(hr); beam.swap(nb); bidx.swap(ni);
      if ((d + 1) % 10 == 0) { double mx = -1e18; for (int i : ord) mx = max(mx, sc[i]); printf("depth %d cand %zu topscore %.0f best %lld\n", d + 1, cand.size(), mx, bestT); fflush(stdout); }
    }
    printf("BEST %lld\n", bestT);
    return 0;
  } else if (mode == "eval") {
    int SIMS = atoi(argv[3]); rng.seed(atoi(argv[4])); const char* bestf = argv[5];
    vector<pair<int, string>> moves; S s = start_with_opening(moves); long long score = 0; int mv = 0;
    while (true) {
      T.clear(); T.reserve((size_t)SIMS * 8 + 16); Node r; r.s = s; r.hasState = true; T.push_back(r);
      QMIN = 1e9; QMAX = -1e9; expand(0);
      if (T[0].terminal) break;
      run_sims(0, SIMS);
      int pick = -1, bn = -1; for (int c = T[0].first; c < T[0].first + T[0].nch; c++) if (T[c].N > bn) { bn = T[c].N; pick = c; }
      int a = T[pick].act, q; int delta = act_legal(s, a, q); int before = s.T; apply_move(s, delta, q);
      moves.push_back({s.T - before, nm(q)}); mv++;
      if (q == H8) { score = s.T; break; }
    }
    printf("EVAL moves %d T %lld\n", mv, score);
    FILE* fb = fopen(bestf, "w"); fprintf(fb, "%lld\n", score);
    for (size_t i = 0; i < moves.size(); i++) fprintf(fb, "%s(%d, %s)", i ? ", " : "", moves[i].first, moves[i].second.c_str());
    fprintf(fb, "\n"); fclose(fb);
  }
}
