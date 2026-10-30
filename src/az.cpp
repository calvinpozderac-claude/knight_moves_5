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
//   CREDIT             fraction of its minutes a stuck tour keeps (training-only partial credit, default 0)
#include "eng.h"
#include <random>

static const int NA = 24;
static int ADX[NA], ADY[NA], ADZ[NA];
static const int D_IN = 64 * 21 + 64 + 10;  // 1418
static const int H = 256;
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
  vector<float> W1, b1, W2, b2, Wp, bp, wv; float bv;
  bool load(const char* fn) {
    FILE* f = fopen(fn, "rb"); if (!f) return false;
    auto rd = [&](vector<float>& v, size_t n) { v.resize(n); return fread(v.data(), 4, n, f) == n; };
    bool ok = rd(W1, (size_t)D_IN * H) && rd(b1, H) && rd(W2, (size_t)H * H) && rd(b2, H) && rd(Wp, (size_t)H * NA) && rd(bp, NA) && rd(wv, H);
    ok = ok && fread(&bv, 4, 1, f) == 1; fclose(f); return ok;
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
  alignas(32) float h1[H], h2[H];
  for (int j = 0; j < H; j++) h1[j] = NET.b1[j];
  for (int t = 0; t < F.nidx; t++) { const float* w = &NET.W1[(size_t)F.idx[t] * H]; for (int j = 0; j < H; j++) h1[j] += w[j]; }
  for (int g = 0; g < 10; g++) { const float* w = &NET.W1[(size_t)(64 * 21 + 64 + g) * H]; float v = F.g[g]; for (int j = 0; j < H; j++) h1[j] += v * w[j]; }
  for (int j = 0; j < H; j++) h1[j] = h1[j] > 0 ? h1[j] : 0;
  for (int j = 0; j < H; j++) h2[j] = NET.b2[j];
  for (int i = 0; i < H; i++) { float a = h1[i]; if (a == 0) continue; const float* w = &NET.W2[(size_t)i * H]; for (int j = 0; j < H; j++) h2[j] += a * w[j]; }
  for (int j = 0; j < H; j++) h2[j] = h2[j] > 0 ? h2[j] : 0;
  for (int a = 0; a < NA; a++) logits[a] = NET.bp[a];
  float v = NET.bv;
  for (int i = 0; i < H; i++) { float x = h2[i]; if (x == 0) continue; const float* w = &NET.Wp[(size_t)i * NA]; for (int a = 0; a < NA; a++) logits[a] += x * w[a]; v += x * NET.wv[i]; }
  value = v;
}

// ---------------- MCTS ----------------
struct Node {
  S s; bool hasState = false;
  int parent = -1, act = -1; float prior = 0;
  int N = 0; double W = 0;
  bool expanded = false, terminal = false; float tval = 0; float r = 0; double bestG = -1e18;
  int first = -1, nch = 0;
};
static vector<Node> T;
static double QMIN, QMAX;
static double CPUCT = 1.5; static double ZMIX = 0.5; static double BETA = 0.0;
// exploration knobs (env vars): TAU1 = move temperature for the first TEMPM moves, TAU2 = after (0 = greedy)
static double TAU1 = 1.0, TAU2 = 0.0, DIRA = 0.3, DIRE = 0.25;
static double CREDIT = 0.0;  // env CREDIT: fraction of its minutes a stuck tour keeps (training only)
static double envd(const char* k, double d) { const char* v = getenv(k); return v ? atof(v) : d; }
static std::mt19937 rng;

static void ensure_state(int id) {
  Node& nd = T[id]; if (nd.hasState) return;
  const S& ps = T[nd.parent].s; int q; int delta = act_legal(ps, nd.act, q);
  nd.s = ps; apply_move(nd.s, delta, q); nd.hasState = true;
  nd.r = (float)((nd.s.T - ps.T) / TSCALE);
  if (q == H8) { nd.terminal = true; nd.tval = 0.f; }
}

// expand node: returns value estimate
static float expand(int id) {
  Node& nd = T[id];
  if (nd.terminal) return nd.tval;
  int legal[NA], nl = 0;
  for (int a = 0; a < NA; a++) { int q; if (act_legal(nd.s, a, q) >= 0) legal[nl++] = a; }
  if (nl == 0) { nd.terminal = true; nd.tval = (float)(-(1.0 - CREDIT) * nd.s.T / TSCALE); return nd.tval; }
  Feat F; featurize(nd.s, F); float lg[NA], v; forward(F, lg, v);
  float mx = -1e30f; for (int i = 0; i < nl; i++) mx = max(mx, lg[legal[i]]);
  float sum = 0, pr[NA]; for (int i = 0; i < nl; i++) { pr[i] = expf(lg[legal[i]] - mx); sum += pr[i]; }
  int first = (int)T.size();
  for (int i = 0; i < nl; i++) { Node c; c.parent = id; c.act = legal[i]; c.prior = pr[i] / sum; T.push_back(c); }
  T[id].first = first; T[id].nch = nl; T[id].expanded = true;
  return v;
}

static void add_noise(int id, double alpha, double eps) {
  Node& nd = T[id]; if (nd.nch == 0) return;
  std::gamma_distribution<double> g(alpha, 1.0); vector<double> d(nd.nch); double s = 0;
  for (auto& x : d) { x = g(rng); s += x; }
  for (int i = 0; i < nd.nch; i++) T[nd.first + i].prior = (float)((1 - eps) * T[nd.first + i].prior + eps * d[i] / s);
}

static void run_sims(int root, int sims) {
  for (int it = 0; it < sims; it++) {
    int id = root;
    while (T[id].expanded && !T[id].terminal) {
      Node& nd = T[id]; double best = -1e18; int bc = -1;
      double rng_q = QMAX - QMIN;
      double qp = nd.N ? nd.W / nd.N : 0;
      double qpn = rng_q > 1e-9 ? (qp - QMIN) / rng_q : 0.5;
      double sq = sqrt((double)nd.N + 1);
      for (int c = nd.first; c < nd.first + nd.nch; c++) {
        Node& ch = T[c];
        double q;
        if (ch.N) { double qe = ch.r + (1 - BETA) * ch.W / ch.N + BETA * ch.bestG; q = rng_q > 1e-9 ? (qe - QMIN) / rng_q : 0.5; }
        else q = qpn;
        double u = q + CPUCT * ch.prior * sq / (1 + ch.N);
        if (u > best) { best = u; bc = c; }
      }
      id = bc; ensure_state(id);
    }
    double g = T[id].terminal ? T[id].tval : expand(id);
    while (id >= 0) {
      Node& nd = T[id]; nd.N++; nd.W += g; if (g > nd.bestG) nd.bestG = g;
      if (nd.parent >= 0) { double qe = nd.r + (1 - BETA) * nd.W / nd.N + BETA * nd.bestG; QMIN = min(QMIN, qe); QMAX = max(QMAX, qe); }
      g = nd.r + g; id = nd.parent;
    }
  }
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
#pragma pack(push, 1)
struct Sample { int16_t alt[64]; uint8_t vis[64]; uint8_t pos; int32_t T; uint32_t mask; float pi[NA]; float z; };
#pragma pack(pop)

int main(int argc, char** argv) {
  init_board(); init_actions();
  CPUCT = envd("CPUCT", 1.5); TAU1 = envd("TAU1", 1.0); TAU2 = envd("TAU2", 0.0); DIRA = envd("DIRA", 0.3); DIRE = envd("DIRE", 0.25);
  USE_OPENING = (int)envd("OPENING", 1); CREDIT = envd("CREDIT", 0.0);
  if (const char* of = getenv("OPENING_FILE")) OPENING_FILE = of;
  if (argc < 3) {
    fprintf(stderr, "usage: az selfplay|eval|beam|improve|test <weights> ...  (see the comment at the top of az.cpp)\n");
    return 1;
  }
  string mode = argv[1];
  if (!NET.load(argv[2])) { fprintf(stderr, "cannot load weights\n"); return 1; }
  if (mode == "selfplay") {
    const char* out = argv[3]; int EP = atoi(argv[4]), SIMS = atoi(argv[5]); rng.seed(atoi(argv[6]));
    int TEMPM = atoi(argv[7]); const char* bestf = argv[8];
    vector<pair<int, string>> pref; double PSTART = 0; if (argc > 10) { pref = read_tour(argv[9]); PSTART = atof(argv[10]); }
    int OPEN_LEN = 0; { vector<pair<int, string>> tmp; start_with_opening(tmp); OPEN_LEN = tmp.size(); }
    FILE* fo = fopen(out, "wb");
    long long bestT = -1;
    for (int ep = 0; ep < EP; ep++) {
      vector<pair<int, string>> moves; S s = start_with_opening(moves);
      std::uniform_real_distribution<double> U01(0, 1);
      if (!pref.empty() && (int)pref.size() > OPEN_LEN + 1 && U01(rng) < PSTART) {
        int k = OPEN_LEN + (int)(U01(rng) * (pref.size() - 1 - OPEN_LEN));
        s = start_state(); moves.clear();
        for (int i = 0; i < k; i++) { int qi = (pref[i].second[0] - 'a') + 8 * (pref[i].second[1] - '1'); int n = group_n(s, s.pos); apply_move(s, pref[i].first / n, qi); moves.push_back(pref[i]); }
      }
      vector<Sample> traj; vector<float> rootv; int mv = 0; bool done = false; long long score = 0;
      while (!done) {
        T.clear(); T.reserve(SIMS * 8 + 16); Node r; r.s = s; r.hasState = true; T.push_back(r);
        QMIN = 1e9; QMAX = -1e9;
        expand(0);
        if (T[0].terminal) { score = 0; break; }  // stuck
        add_noise(0, DIRA, DIRE);
        run_sims(0, SIMS);
        Sample sm; for (int i = 0; i < 64; i++) { sm.alt[i] = s.alt[i]; sm.vis[i] = s.vis[i]; } sm.pos = s.pos; sm.T = s.T; sm.mask = 0;
        for (int a = 0; a < NA; a++) sm.pi[a] = 0;
        int tot = 0; for (int c = T[0].first; c < T[0].first + T[0].nch; c++) { sm.mask |= 1u << T[c].act; sm.pi[T[c].act] = T[c].N; tot += T[c].N; }
        for (int a = 0; a < NA; a++) sm.pi[a] /= max(1, tot);
        traj.push_back(sm); rootv.push_back((float)(T[0].W / max(1, T[0].N)));
        // choose action
        int pick = -1;
        double tau = mv < TEMPM ? TAU1 : TAU2;
        if (tau > 0) {  // sample in proportion to N^(1/tau)
          double wt[NA], wsum = 0;
          for (int i = 0; i < T[0].nch; i++) { int c = T[0].first + i; wt[i] = T[c].N > 0 ? pow((double)T[c].N, 1.0 / tau) : 0.0; wsum += wt[i]; }
          std::uniform_real_distribution<double> U(0, 1); double r0 = U(rng) * wsum, acc = 0;
          for (int i = 0; i < T[0].nch; i++) { acc += wt[i]; if (wt[i] > 0 && acc >= r0) { pick = T[0].first + i; break; } }
        }
        if (pick < 0) { int bn = -1; for (int c = T[0].first; c < T[0].first + T[0].nch; c++) if (T[c].N > bn) { bn = T[c].N; pick = c; } }
        int a = T[pick].act, q; int delta = act_legal(s, a, q);
        int before = s.T; apply_move(s, delta, q); moves.push_back({s.T - before, nm(q)}); mv++;
        if (q == H8) { score = s.T; done = true; }
        if (mv > 400) break;
      }
      long long Tend = s.T;
      for (size_t i = 0; i < traj.size(); i++) {
        float z = (float)(score > 0 ? (score - traj[i].T) / TSCALE : (CREDIT * Tend - traj[i].T) / TSCALE);
        traj[i].z = (float)(ZMIX * z + (1 - ZMIX) * rootv[i]); fwrite(&traj[i], sizeof(Sample), 1, fo); }
      printf("EP %d moves %d T %lld end %lld\n", ep, mv, score, Tend); fflush(stdout);
      if (score > bestT) {
        bestT = score;
        FILE* fb = fopen(bestf, "w"); fprintf(fb, "%lld\n", score);
        for (size_t i = 0; i < moves.size(); i++) fprintf(fb, "%s(%d, %s)", i ? ", " : "", moves[i].first, moves[i].second.c_str());
        fprintf(fb, "\n"); fclose(fb);
      }
    }
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
