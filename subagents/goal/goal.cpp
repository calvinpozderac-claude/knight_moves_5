// Goal-conditioned opening search for the swamp-knight puzzle.
//
//   goal design : take a tour (default: the 8,123 record), find its dance start, describe that
//                 target configuration (two groups two apart containing h8, knight on top, visits),
//                 score it, and generate variant targets that should yield more. Writes a targets file.
//   goal search : beam search (multithreaded, --time) from a1 or from the first k moves of a tour,
//                 ordered by T minus a distance to the nearest target configuration. Every dance start
//                 it reaches becomes a candidate prefix; the best are written in the format
//                 `build/nrpa --prefix-file` reads, and optionally finished here with NRPA.
//   goal score  : score the dance starts along a tour (table per move).
//   goal selfcheck : replay the record (must give 8,123) and test the parser / writer.
//
// Scorers of a dance-start state s (T = minutes so far):
//   ideal  = T + idealised dance (README "--objective opening"; copy of dance_pot in src/nrpa.cpp)
//   bound2 = ideal, refined: a point can only sit if it has an orthogonal neighbour in the other group
//            (the knight leaves a sit by an orthogonal +2 jump), a1/h8 never sit, and each opposite pair
//            supplies one sitting point (sitting kills the partner); the final jump into h8 is added
//   dnrpa  = T + best dance found by a small NRPA restricted to dance moves (sit 4 then jump onto the
//            other group, zero-wait same-altitude hops, any jump into h8) on the real engine: includes
//            adjacency, opposite pairs, joins, and visit caps exactly; noisy, a lower bound
//
// Build: g++ -O3 -march=native -std=c++17 -pthread -o goal goal.cpp
#include "../../src/eng.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <map>

struct Mv { int delta, q; };
static std::atomic<bool> STOP{false};
static void on_sigint(int) { STOP = true; }

// ---------------------------------------------------------------- tours I/O
static vector<Mv> parse_moves(const char* line) {
  vector<Mv> t; S s = start_state(); const char* p = line; int w; char sq[8];
  while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
    int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
    t.push_back({d, q}); apply_move(s, d, q); p++;
  }
  return t;
}
static vector<Mv> read_first_moves(const string& fn) {
  FILE* f = fopen(fn.c_str(), "r"); if (!f) { fprintf(stderr, "cannot open %s\n", fn.c_str()); exit(1); }
  static char line[400000]; vector<Mv> t;
  while (fgets(line, sizeof line, f)) { t = parse_moves(line); if (!t.empty()) break; }
  fclose(f); return t;
}
static string tour_text(const vector<Mv>& seq) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < seq.size(); i++) {
    int w = apply_move(s, seq[i].delta, seq[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(seq[i].q).c_str()); out += buf;
  }
  return out;
}
static S replay(const vector<Mv>& seq, size_t k) { S s = start_state(); for (size_t i = 0; i < k && i < seq.size(); i++) apply_move(s, seq[i].delta, seq[i].q); return s; }
// is (delta, q) one of the legal pinned moves at s?
static bool legal_move(const S& s, Mv m) {
  vector<pair<int, int>> mv; pinned_moves(s, 3, true, mv);
  for (auto& [d, q] : mv) if (d == m.delta && q == m.q) return true;
  return false;
}
static int sq_of(const string& n) { return (n[0] - 'a') + 8 * (n[1] - '1'); }
static int ntype(int p, int q) {  // 0 knight, 1 orthogonal, 2 two-away, -1 none
  for (int k = 0; k < NBN[p]; k++) if (NBQ[p][k] == q) return NBT[p][k];
  return -1;
}

// ---------------------------------------------------------------- dance-start test and scorers
static int DMIN = 50;
// knight in the upper of two groups two apart, h8 in one of them, together >= DMIN points
static bool is_dance_start(const S& s, int* nU = nullptr, int* nL = nullptr, int dmin = -1) {
  if (dmin < 0) dmin = DMIN;
  int a = s.alt[s.pos], u = 0, l = 0;
  for (int i = 0; i < 64; i++) { if (s.alt[i] == a) u++; else if (s.alt[i] == a - 2) l++; }
  if (nU) *nU = u; if (nL) *nL = l;
  if (l == 0) return false;
  int ah = s.alt[H8]; if (ah != a && ah != a - 2) return false;
  return u + l >= dmin;
}

// Idealised dance from a dance start. mode 0 = "ideal" (copy of dance_pot in src/nrpa.cpp: sits alternate between
// the groups, points whose partner is gone sit first, each opposite pair supplies one sitting point).
// mode 1 = "bound2": also a point can only sit if it has an orthogonal neighbour left in the other group (the
// knight leaves a 4-unit sit by an orthogonal +2 jump), and the final partial sit before h8 (3n) is added.
// mode 2 = "bound3": bound2 plus joins: a point outside the groups whose altitude a sinking group reaches
// joins that group (that is how the record's groups grow from 57 to 62 points in its first dance moves); the
// knight's opposite rises when outside the groups, so it may move out of reach.
static double dance_bound(const S& s, int mode, int dmin = -1) {
  if (!is_dance_start(s, nullptr, nullptr, dmin)) return -1e18;
  int a = s.alt[s.pos], nG[2] = {0, 0}, lvl[2] = {a, a - 2};
  int grp[64], cap[64], alt[64]; bool alive[64], open_[64], dead[64];
  for (int i = 0; i < 64; i++) {
    alt[i] = s.alt[i]; dead[i] = false;
    alive[i] = alt[i] == a || alt[i] == a - 2; grp[i] = alt[i] == a ? 0 : 1;
    if (alive[i]) nG[grp[i]]++;
    cap[i] = alive[i] ? 3 - s.vis[i] + (i == s.pos) : 0;
  }
  cap[H8] = cap[A1] = 0;
  for (int i = 0; i < 64; i++) open_[i] = alive[i] && !alive[63 - i];
  auto can_sit = [&](int i) {
    if (mode == 0) return true;
    for (int k = 0; k < NBN[i]; k++) { int q = NBQ[i][k]; if (NBT[i][k] == 1 && alive[q] && grp[q] != grp[i] && s.vis[q] < 3) return true; }
    return false;
  };
  double fut = 0, lastN = 0; int g = 0;
  for (int step = 0; step < 200; step++) {
    int best = -1, bc = 0;
    if (step == 0) best = s.pos;
    else {
      for (int i = 0; i < 64; i++) if (alive[i] && grp[i] == g && open_[i] && cap[i] > bc && can_sit(i)) { best = i; bc = cap[i]; }
      if (best < 0) for (int i = 0; i < 64; i++) if (alive[i] && grp[i] == g && cap[i] > bc && can_sit(i)) { best = i; bc = cap[i]; }
    }
    if (best < 0 || cap[best] <= 0) break;
    int n = nG[g], p = 63 - best;
    if (alive[p]) { if (grp[p] == g) n--; nG[grp[p]]--; alive[p] = false; dead[p] = true; cap[p] = 0; }
    else if (mode == 2 && !dead[p]) alt[p] += 4;  // a point outside the groups rises with the sit
    open_[best] = true;
    fut += 4.0 * n; lastN = n; cap[best]--;
    if (mode == 2) {
      lvl[g] -= 4;
      for (int i = 0; i < 64; i++) if (!alive[i] && !dead[i] && (alt[i] == lvl[0] || alt[i] == lvl[1])) {
        int gg = alt[i] == lvl[0] ? 0 : 1; alive[i] = true; grp[i] = gg; nG[gg]++;
        cap[i] = (i == A1 || i == H8) ? 0 : 3 - s.vis[i]; open_[i] = !alive[63 - i];
      }
    }
    g ^= 1;
  }
  if (mode > 0) fut += 3.0 * lastN;
  return s.T + fut;
}
static int SCORE_MODE = 2;  // the scorer used to rank targets, variants and candidate prefixes
static inline double dscore(const S& s) { return dance_bound(s, SCORE_MODE); }

// ---------------------------------------------------------------- NRPA (dance-restricted or full)
static int KIDX[64][64];
static const int NCODE = 64 * 16 * 32 * 4;
struct Policy { vector<float> w, e; Policy() : w(NCODE, 0.f), e(NCODE, 1.f) {} };
static inline uint32_t code_of(const S& s, int delta, int q) {
  return (((uint32_t)s.pos * 16 + KIDX[s.pos][q]) * 32 + (uint32_t)min(delta, 31)) * 4 + (uint32_t)min<int>(s.vis[q], 3);
}
struct Result { double score = -1e18; long long minutes = 0; bool finished = false; vector<Mv> seq; };
static double D0 = 1;
static double h8dist(const S& s) {
  int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  return sqrt(dx * dx + dy * dy + dz * dz) / D0;
}
// dance moves: sit 4 and jump onto the group 2 below the start altitude, zero-wait same-altitude hops, h8
static inline void gen_moves(const S& s, bool dance, vector<pair<int, int>>& mv) {
  pinned_moves(s, 3, true, mv);
  if (!dance) return;
  int a = s.alt[s.pos]; size_t j = 0;
  for (size_t i = 0; i < mv.size(); i++) {
    auto [d, q] = mv[i];
    if (q == H8 || (d == 0 && s.alt[q] == a) || (d == 4 && s.alt[q] == a - 2)) mv[j++] = mv[i];
  }
  mv.resize(j);
}
struct NrpaCfg { bool dance = false; int iters = 100; double alpha = 1.0; long long* rollouts = nullptr; };
static Result playout(const S& root, const Policy& pol, std::mt19937& rng, const NrpaCfg& C) {
  S s = root; Result r; vector<pair<int, int>> mv; vector<double> w;
  long long finT = -1; size_t finLen = 0; Mv finMove{0, H8};
  while (true) {
    gen_moves(s, C.dance, mv);
    w.clear(); double z = 0; int nonfin = 0;
    for (auto& [d, q] : mv) {
      if (q == H8) { long long t = s.T + (long long)d * (d ? group_n(s, s.pos) : 0); if (t >= finT) { finT = t; finLen = r.seq.size(); finMove = {d, q}; } w.push_back(0); continue; }
      double e = pol.e[code_of(s, d, q)]; w.push_back(e); z += e; nonfin++;
    }
    if (nonfin == 0) break;
    double u = std::uniform_real_distribution<double>(0, z)(rng), acc = 0; int pick = -1;
    for (size_t i = 0; i < mv.size(); i++) { if (mv[i].second == H8) continue; acc += w[i]; pick = (int)i; if (acc >= u) break; }
    r.seq.push_back({mv[pick].first, mv[pick].second});
    apply_move(s, mv[pick].first, mv[pick].second);
  }
  if (C.rollouts) (*C.rollouts)++;
  if (finT >= 0) { r.seq.resize(finLen); r.seq.push_back(finMove); r.finished = true; r.minutes = finT; r.score = (double)finT; }
  else { r.minutes = s.T; r.score = -1e7 + s.T * exp(-h8dist(s)); }
  return r;
}
static void adapt(Policy& pol, const S& root, const vector<Mv>& seq, double alpha, bool dance) {
  vector<pair<uint32_t, float>> upd; vector<pair<int, int>> mv; vector<double> e;
  S s = root;
  for (const Mv& m : seq) {
    if (m.q == H8) break;
    gen_moves(s, dance, mv);
    e.clear(); double z = 0;
    for (auto& [d, q] : mv) { double x = q == H8 ? 0 : pol.e[code_of(s, d, q)]; e.push_back(x); z += x; }
    for (size_t i = 0; i < mv.size(); i++) if (mv[i].second != H8) upd.push_back({code_of(s, mv[i].first, mv[i].second), (float)(-alpha * e[i] / z)});
    upd.push_back({code_of(s, m.delta, m.q), (float)alpha});
    apply_move(s, m.delta, m.q);
  }
  for (auto& [c, d] : upd) pol.w[c] += d;
  for (auto& [c, d] : upd) pol.e[c] = expf(pol.w[c]);
}
static Result nrpa(int level, const S& root, Policy pol, std::mt19937& rng, const NrpaCfg& C) {
  if (level == 0) return playout(root, pol, rng, C);
  Result best;
  for (int i = 0; i < C.iters && !STOP; i++) {
    Result r = level == 1 ? playout(root, pol, rng, C) : nrpa(level - 1, root, pol, rng, C);
    if (r.score >= best.score) best = r;
    adapt(pol, root, best.seq, C.alpha, C.dance);
  }
  return best;
}
// T + best restricted dance from s (max over `runs` NRPA searches of the given level); -1e18 if none finishes
static double dance_nrpa(const S& s, int level, int runs, unsigned seed, vector<Mv>* bestSeq = nullptr) {
  NrpaCfg C; C.dance = true; double best = -1e18;
  for (int r = 0; r < runs; r++) {
    std::mt19937 rng(seed * 7919u + r); Policy pol;
    Result x = nrpa(level, s, pol, rng, C);
    if (x.finished && x.minutes > best) { best = (double)x.minutes; if (bestSeq) *bestSeq = x.seq; }
  }
  return best;
}


// "dbeam": a beam search over the real engine restricted to dance moves (sit 4 then jump onto the other
// group, zero-wait same-altitude hops, any jump into h8), ordered by T + bound3 of the state (no size limit).
// Returns the best finished total (T + the dance) and optionally its moves; it accounts for adjacency, hop
// visits, opposite pairs and joins exactly, so it separates dance starts the bounds rate equal.
static int DBEAM = 300;
static double dance_beam(const S& root, int width, vector<Mv>* bestSeq = nullptr) {
  struct N { S s; double pri; int parent; Mv m; };
  vector<N> cur{{root, 0, -1, {0, 0}}}; vector<vector<pair<int, Mv>>> hist; double best = -1e18; int bestLayer = -1, bestIdx = -1; Mv bestM{0, 0};
  vector<pair<int, int>> mv;
  for (int depth = 0; depth < 200 && !cur.empty(); depth++) {
    vector<N> nx;
    for (int i = 0; i < (int)cur.size(); i++) {
      gen_moves(cur[i].s, true, mv);
      for (auto& [d, q] : mv) {
        S c = cur[i].s; apply_move(c, d, q);
        if (q == H8) { if (c.T > best) { best = c.T; bestLayer = depth; bestIdx = i; bestM = {d, q}; } continue; }
        double b = dance_bound(c, 2, 0); if (b < -1e17) continue;
        nx.push_back({c, b, i, {d, q}});
      }
    }
    hist.push_back({}); for (auto& n : cur) hist.back().push_back({n.parent, n.m});
    sort(nx.begin(), nx.end(), [](const N& a, const N& b) { return a.pri != b.pri ? a.pri > b.pri : a.s.T > b.s.T; });
    std::unordered_set<uint64_t> seen; vector<N> keep;
    for (auto& n : nx) { if ((int)keep.size() >= width) break; if (n.pri <= best) break; if (seen.insert(hashS(n.s)).second) keep.push_back(n); }
    cur.swap(keep);
  }
  if (bestSeq && bestLayer >= 0) {
    vector<Mv> p{bestM}; int idx = bestIdx;
    for (int L = bestLayer; L > 0; L--) { p.push_back(hist[L][idx].second); idx = hist[L][idx].first; }
    reverse(p.begin(), p.end()); *bestSeq = p;
  }
  return best;
}

// ---------------------------------------------------------------- printing a configuration
static void print_config(FILE* f, const S& s) {
  int a = s.alt[s.pos], nU, nL; bool ds = is_dance_start(s, &nU, &nL);
  fprintf(f, "T=%d knight %s alt %d (h8 alt %d)  upper group (alt %d): %d points, lower (alt %d): %d points%s\n", s.T, nm(s.pos).c_str(), a,
          s.alt[H8], a, nU, a - 2, nL, ds ? "  [dance start]" : "");
  fprintf(f, "   cell = U/L (upper/lower group) or altitude relative to the upper group, then visits; * = knight\n");
  for (int y = 7; y >= 0; y--) {
    fprintf(f, "  %d", y + 1);
    for (int x = 0; x < 8; x++) {
      int i = x + 8 * y; char g[8];
      if (s.alt[i] == a) snprintf(g, sizeof g, "U"); else if (s.alt[i] == a - 2) snprintf(g, sizeof g, "L"); else snprintf(g, sizeof g, "%+d", s.alt[i] - a);
      fprintf(f, " %3s%c%d", g, i == s.pos ? '*' : ' ', s.vis[i]);
    }
    fprintf(f, "\n");
  }
  fprintf(f, "     ");
  for (int x = 0; x < 8; x++) fprintf(f, "    %c ", 'a' + x);
  fprintf(f, "\n");
  // opposite pairs
  int both = 0, bothUsed = 0, one = 0, none = 0, v2 = 0, visU = 0; string bl;
  for (int i = 0; i < 32; i++) {
    int o = 63 - i; bool ai = s.alt[i] == a || s.alt[i] == a - 2, ao = s.alt[o] == a || s.alt[o] == a - 2;
    if (ai && ao) { both++; if (s.vis[i] - (i == s.pos) > 0 && s.vis[o] - (o == s.pos) > 0) { bothUsed++; bl += " " + nm(i) + "/" + nm(o); } }
    else if (ai || ao) one++; else none++;
  }
  for (int i = 0; i < 64; i++) if (s.alt[i] == a || s.alt[i] == a - 2) { if (s.vis[i] >= 2) v2++; visU += s.vis[i]; }
  fprintf(f, "  opposite pairs: %d with both points in the groups (%d of them both visited already:%s), %d with one, %d with none\n",
          both, bothUsed, bl.c_str(), one, none);
  fprintf(f, "  group points with 2+ visits: %d; visits already used on group points: %d\n", v2, visU);
  string out; for (int i = 0; i < 64; i++) if (!(s.alt[i] == a || s.alt[i] == a - 2)) out += " " + nm(i);
  fprintf(f, "  points outside the groups:%s\n", out.c_str());
}

// ---------------------------------------------------------------- targets file
// one target per line: name \t est \t T \t pos \t alt(64, comma) \t vis(64, comma)
struct Target { string name; double est = 0; S s; bool key[64]; };
static void target_keys(Target& t) {
  int a = t.s.alt[t.s.pos];
  for (int i = 0; i < 64; i++) t.key[i] = t.s.alt[i] == a || t.s.alt[i] == a - 2;
}
static string target_line(const string& name, double est, const S& s) {
  string o = name + "\t" + to_string((long long)llround(est)) + "\t" + to_string(s.T) + "\t" + nm(s.pos) + "\t";
  for (int i = 0; i < 64; i++) o += (i ? "," : "") + to_string(s.alt[i]);
  o += "\t";
  for (int i = 0; i < 64; i++) o += (i ? "," : "") + to_string(s.vis[i]);
  return o;
}
static vector<Target> read_targets(const string& fn) {
  vector<Target> out; FILE* f = fopen(fn.c_str(), "r"); if (!f) { fprintf(stderr, "cannot open %s\n", fn.c_str()); exit(1); }
  static char line[100000];
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#' || strlen(line) < 10) continue;
    vector<string> col; string cur; for (char* p = line; *p && *p != '\n'; p++) { if (*p == '\t') { col.push_back(cur); cur.clear(); } else cur += *p; }
    col.push_back(cur);
    if (col.size() < 6) continue;
    Target t; t.name = col[0]; t.est = atof(col[1].c_str()); t.s.T = atoi(col[2].c_str()); t.s.pos = (uint8_t)sq_of(col[3]);
    const char* p = col[4].c_str(); for (int i = 0; i < 64; i++) { t.s.alt[i] = (AT)strtol(p, (char**)&p, 10); if (*p == ',') p++; }
    p = col[5].c_str(); for (int i = 0; i < 64; i++) { t.s.vis[i] = (uint8_t)strtol(p, (char**)&p, 10); if (*p == ',') p++; }
    target_keys(t); out.push_back(t);
  }
  fclose(f); return out;
}

// ---------------------------------------------------------------- options
struct Opts {
  string tour = "data/tours/record_8123.txt", targets, outDir = "subagents/goal/tmp/run", startTour;
  int k = -1, startK = 0, variants = 12, edits = 6, scoreLevel = 1, scoreRuns = 2, threads = 1, beam = 20000, maxDepth = 60,
      topOut = 50, finishTop = 0, finishLevel = 2, maxPerTarget = 0, kmin = 30, kmax = 50;
  double time = 60, finishTime = 0, wAlt = 40, wVis = 100, wKnight = 20, wT = 1, noise = 0, poolMin = 7500, altCap = 6;
  unsigned rng = 1; bool expandDance = false, visKey = true;
};
static Opts O;

// ---------------------------------------------------------------- design mode
struct Edit { string desc; S s; double b2 = -1e18; };
static vector<Edit> single_edits(const S& b) {
  vector<Edit> out; int a = b.alt[b.pos];
  auto alive = [&](const S& s, int i) { return s.alt[i] == a || s.alt[i] == a - 2; };
  for (int i = 0; i < 64; i++) {
    if (i == H8) continue;
    if (alive(b, i) && b.vis[i] > (i == b.pos ? 1 : 0) && !(i == A1 && b.vis[i] <= 1)) {  // the opening avoids one visit of i
      Edit e; e.s = b; e.s.vis[i]--; e.desc = "free a visit of " + nm(i) + " (" + to_string(b.vis[i]) + "->" + to_string(b.vis[i] - 1) + ")"; out.push_back(e);
    }
    if (!alive(b, i) && i != A1) for (int g = 0; g < 2; g++) {  // the opening also merges i into a group
      Edit e; e.s = b; e.s.alt[i] = (AT)(a - 2 * g); e.desc = "merge " + nm(i) + " into the " + (g ? "lower" : "upper") + " group"; out.push_back(e);
    }
    if (alive(b, i) && i != b.pos) {  // i in the other group
      Edit e; e.s = b; e.s.alt[i] = (AT)(b.alt[i] == a ? a - 2 : a); e.desc = "move " + nm(i) + " to the " + (b.alt[i] == a ? "lower" : "upper") + " group"; out.push_back(e);
    }
    if (b.alt[i] == a && i != b.pos && b.vis[i] < 3) {  // the knight ends the opening on i instead
      Edit e; e.s = b; e.s.vis[b.pos]--; e.s.vis[i]++; e.s.pos = (uint8_t)i; e.desc = "knight starts the dance on " + nm(i); out.push_back(e);
    }
  }
  return out;
}

static int run_design() {
  vector<Mv> t = read_first_moves(O.tour); S fin = replay(t, t.size());
  printf("tour %s: %zu moves, %d minutes\n\n", O.tour.c_str(), t.size(), fin.T);
  printf("move  T     knight up+low  ideal   bound2  bound3  dnrpa(L%d)\n", O.scoreLevel);
  int firstDance = -1, bestK = -1; double bestK3 = -1e18;
  for (int k = O.kmin; k <= O.kmax && k <= (int)t.size(); k++) {
    S s = replay(t, k); int nU, nL; bool ds = is_dance_start(s, &nU, &nL);
    if (ds && firstDance < 0) firstDance = k;
    double i1 = dance_bound(s, 0), b2 = dance_bound(s, 1), b3 = dance_bound(s, 2), dn = ds ? dance_nrpa(s, O.scoreLevel, 1, O.rng + k) : -1e18;
    if (b3 > bestK3 + 0.5) { bestK3 = b3; bestK = k; }
    printf("%4d %5d   %s   %2d+%2d  %6.0f  %6.0f  %6.0f  %6.0f\n", k, s.T, nm(s.pos).c_str(), nU, nL, i1 > -1e17 ? i1 : 0, b2 > -1e17 ? b2 : 0, b3 > -1e17 ? b3 : 0,
           dn > -1e17 ? dn : 0);
  }
  int k = O.k >= 0 ? O.k : firstDance;
  printf("first dance start: move %d; best %s: move %d\n", firstDance, SCORE_MODE == 2 ? "bound3" : "bound2", bestK);
  if (k < 0) { fprintf(stderr, "no dance start between moves %d and %d (lower --dmin?)\n", O.kmin, O.kmax); return 1; }
  S base = replay(t, k);
  printf("\nTARGET = state after move %d\n", k);
  print_config(stdout, base);
  double b2 = dscore(base);
  double dn = dance_nrpa(base, O.scoreLevel, O.scoreRuns, O.rng);
  printf("  scores: ideal %.0f  bound2 %.0f  bound3 %.0f  dnrpa(L%d x%d) %.0f   (the real tour reached %d); ranking uses %s\n\n", dance_bound(base, 0),
         dance_bound(base, 1), dance_bound(base, 2), O.scoreLevel, O.scoreRuns, dn, fin.T, SCORE_MODE == 2 ? "bound3" : SCORE_MODE == 1 ? "bound2" : "ideal");

  // single edits ranked by bound2
  vector<Edit> E = single_edits(base);
  for (auto& e : E) e.b2 = dscore(e.s);
  sort(E.begin(), E.end(), [](const Edit& x, const Edit& y) { return x.b2 > y.b2; });
  printf("single edits by score gain (top 25 of %zu):\n", E.size());
  for (size_t i = 0; i < E.size() && i < 25; i++) printf("  %+6.0f  %s\n", E[i].b2 > -1e17 ? E[i].b2 - b2 : -99999, E[i].desc.c_str());

  // greedy combination: repeatedly apply the best edit by bound2 (ties broken by dnrpa of the top 4)
  string tfile = O.targets.empty() ? O.outDir + "/targets.txt" : O.targets;
  FILE* tf = fopen(tfile.c_str(), "w"); if (!tf) { fprintf(stderr, "cannot write %s (does the folder exist?)\n", tfile.c_str()); return 1; }
  fprintf(tf, "# targets from %s move %d: name, estimate (scorer), T, knight, altitudes a1..h8, visits a1..h8\n", O.tour.c_str(), k);
  fprintf(tf, "%s\n", target_line("base_k" + to_string(k), b2, base).c_str());
  S cur = base; double curB = b2; string path;
  printf("\ngreedy variants (each adds one edit; dnrpa = L%d dance search, max of %d):\n", O.scoreLevel, O.scoreRuns);
  for (int step = 0; step < O.edits && !STOP; step++) {
    vector<Edit> F = single_edits(cur);
    for (auto& e : F) e.b2 = dscore(e.s);
    sort(F.begin(), F.end(), [](const Edit& x, const Edit& y) { return x.b2 > y.b2; });
    int pick = -1; double pd = -1e18;
    for (int j = 0; j < (int)F.size() && j < 4; j++) {
      if (F[j].b2 <= curB) break;
      double d = dance_nrpa(F[j].s, O.scoreLevel, 1, O.rng + 31 * step + j);
      double v = F[j].b2 + 0.25 * (d > -1e17 ? d - F[j].b2 : -2000);
      if (v > pd) { pd = v; pick = j; }
    }
    if (pick < 0) { printf("  no improving edit left\n"); break; }
    cur = F[pick].s; curB = F[pick].b2; path += (path.empty() ? "" : "; ") + F[pick].desc;
    double d = dance_nrpa(cur, O.scoreLevel, O.scoreRuns, O.rng + 1000 + step);
    printf("  v%d: score %.0f (%+.0f)  dnrpa %.0f   %s\n", step + 1, curB, curB - b2, d, F[pick].desc.c_str());
    fprintf(tf, "%s\n", target_line("greedy" + to_string(step + 1), curB, cur).c_str());
  }
  // the best single edits as their own targets
  int nv = 0;
  for (size_t i = 0; i < E.size() && nv < O.variants; i++) {
    if (E[i].b2 <= b2) break;
    string n = E[i].desc; for (char& c : n) if (c == ' ' || c == '(' || c == ')' || c == '>' || c == '-') c = '_';
    fprintf(tf, "%s\n", target_line("single_" + n, E[i].b2, E[i].s).c_str()); nv++;
  }
  fclose(tf);
  printf("\nfinal greedy variant:\n"); print_config(stdout, cur);
  printf("wrote %s (base + greedy chain + %d single-edit variants)\n", tfile.c_str(), nv);
  return 0;
}

// ---------------------------------------------------------------- search mode
static vector<Target> TG;
// distance (in minutes) from s to target t, in the frame where h8's altitude is 0
static inline double target_dist(const S& s, const Target& t, double* altD = nullptr, int* visX = nullptr) {
  int hs = s.alt[H8], ht = t.s.alt[H8]; double ad = 0; int vx = 0;
  for (int i = 0; i < 64; i++) {
    if (!t.key[i]) continue;
    int d = abs((s.alt[i] - hs) - (t.s.alt[i] - ht)); ad += min<double>(d, O.altCap);
    int over = (int)s.vis[i] - (int)t.s.vis[i]; if (over > 0) vx += over;
  }
  // the knight: altitude relative to h8 and board distance to the target's knight square
  int kd = abs((s.alt[s.pos] - hs) - (t.s.alt[t.s.pos] - ht));
  int bx = abs(s.pos % 8 - t.s.pos % 8), by = abs(s.pos / 8 - t.s.pos / 8);
  if (altD) *altD = ad; if (visX) *visX = vx;
  return O.wAlt * ad + O.wVis * vx + O.wKnight * (min(kd, 8) + 0.5 * max(bx, by));
}
// priority: T + target estimate - distance, max over targets
static inline double priority(const S& s, int* which = nullptr) {
  double best = -1e18; int w = -1;
  for (size_t j = 0; j < TG.size(); j++) {
    double v = O.wT * s.T + (TG[j].est - O.wT * TG[j].s.T) - target_dist(s, TG[j]);
    if (v > best) { best = v; w = (int)j; }
  }
  if (which) *which = w;
  return best;
}

struct Link { int32_t parent; int16_t delta; uint8_t q; };
struct Cand { float pri; uint32_t parent; int16_t delta; uint8_t q; uint64_t h; };
struct Found { double score; long long T; int depth; int target; double dist; vector<Mv> seq; uint64_t h; };

static int run_search() {
  if (O.targets.empty()) { fprintf(stderr, "search needs --targets FILE (make one with `goal design`)\n"); return 1; }
  TG = read_targets(O.targets);
  if (TG.empty()) { fprintf(stderr, "no targets in %s\n", O.targets.c_str()); return 1; }
  printf("%zu targets from %s:", TG.size(), O.targets.c_str());
  for (auto& t : TG) printf(" %s(%.0f)", t.name.c_str(), t.est);
  printf("\n");
  vector<Mv> prefix;
  if (!O.startTour.empty()) { vector<Mv> st = read_first_moves(O.startTour); prefix.assign(st.begin(), st.begin() + min<size_t>(O.startK, st.size())); }
  S root = replay(prefix, prefix.size());
  printf("start: %zu moves fixed (T %d, knight %s), beam %d, max depth %d, %d threads, %.0fs\n", prefix.size(), root.T, nm(root.pos).c_str(), O.beam,
         O.maxDepth, O.threads, O.time);
  string cmd = "mkdir -p '" + O.outDir + "'"; if (system(cmd.c_str())) {}
  auto t0 = std::chrono::steady_clock::now();
  auto elapsed = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); };

  vector<vector<Link>> layers;  // layers[d][i]: how node i of depth d was reached from depth d-1
  vector<S> cur{root}; layers.push_back({Link{-1, 0, 0}});
  std::mutex fm; vector<Found> found; std::unordered_map<uint64_t, size_t> foundIdx; long long nodes = 0;
  vector<int> perTarget(TG.size(), 0);
  auto path_to = [&](int depth, int idx) {  // moves from root to node idx at depth
    vector<Mv> p(depth);
    for (int d = depth; d > 0; d--) { const Link& L = layers[d][idx]; p[d - 1] = {L.delta, L.q}; idx = L.parent; }
    return p;
  };
  auto write_out = [&]() {
    std::lock_guard<std::mutex> lk(fm);
    sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.score > b.score; });
    foundIdx.clear(); for (size_t i = 0; i < found.size(); i++) foundIdx[found[i].h] = i;
    FILE* f = fopen((O.outDir + "/prefixes.txt").c_str(), "w");
    if (!f) return;
    fprintf(f, "# score\tT\tmoves\ttarget\tdist\tprefix (prefix moves include the fixed start)\n");
    for (auto& x : found) {
      vector<Mv> full = prefix; full.insert(full.end(), x.seq.begin(), x.seq.end());
      fprintf(f, "%.0f\t%lld\t%zu\t%s\t%.0f\t%s\n", x.score, x.T, full.size(), TG[x.target].name.c_str(), x.dist, tour_text(full).c_str());
    }
    fclose(f);
    for (int i = 0; i < (int)found.size() && i < O.topOut; i++) {
      char fn[64]; snprintf(fn, sizeof fn, "/prefix_%03d.txt", i + 1);
      FILE* g = fopen((O.outDir + fn).c_str(), "w"); if (!g) continue;
      vector<Mv> full = prefix; full.insert(full.end(), found[i].seq.begin(), found[i].seq.end());
      fprintf(g, "%.0f\n%s\n", found[i].score, tour_text(full).c_str()); fclose(g);
    }
  };

  double lastWrite = 0;
  for (int depth = 1; depth <= O.maxDepth && !STOP && !cur.empty(); depth++) {
    if (O.time > 0 && elapsed() > O.time) break;
    int T = max(1, O.threads); vector<vector<Cand>> loc(T); std::atomic<size_t> nextChunk{0}; const size_t CH = 256;
    auto work = [&](int tid) {
      vector<pair<int, int>> mv; std::mt19937 rng(O.rng * 101u + tid * 7u + depth * 1000003u);
      std::uniform_real_distribution<double> U(-1, 1);
      while (true) {
        size_t c0 = nextChunk.fetch_add(CH); if (c0 >= cur.size()) break;
        for (size_t pi = c0; pi < min(cur.size(), c0 + CH); pi++) {
          const S& ps = cur[pi];
          pinned_moves(ps, 3, false, mv);
          for (auto& [d, q] : mv) {
            S c = ps; apply_move(c, d, q);
            int w; double pr = priority(c, &w);
            if (is_dance_start(c)) {  // a candidate prefix
              double sc = dscore(c);
              uint64_t h = hashS(c);
              double dist = target_dist(c, TG[w]);
              std::lock_guard<std::mutex> lk(fm);
              auto it = foundIdx.find(h);
              if (it == foundIdx.end() || found[it->second].score < sc) {
                if (O.maxPerTarget > 0 && it == foundIdx.end() && perTarget[w] >= O.maxPerTarget) {}
                else {
                  vector<Mv> p = path_to(depth - 1, (int)pi); p.push_back({d, q});
                  if (it == foundIdx.end()) { foundIdx[h] = found.size(); found.push_back({sc, c.T, depth, w, dist, p, h}); perTarget[w]++; }
                  else found[it->second] = {sc, c.T, depth, w, dist, p, h};
                }
              }
              if (!O.expandDance) continue;
            }
            if (O.noise > 0) pr += O.noise * U(rng);
            loc[tid].push_back({(float)pr, (uint32_t)pi, (int16_t)d, (uint8_t)q, hashS(c, O.visKey)});
          }
        }
      }
    };
    vector<std::thread> th; for (int t = 0; t < T; t++) th.emplace_back(work, t);
    for (auto& x : th) x.join();
    vector<Cand> all; size_t tot = 0; for (auto& v : loc) tot += v.size(); all.reserve(tot);
    for (auto& v : loc) { all.insert(all.end(), v.begin(), v.end()); vector<Cand>().swap(v); }
    nodes += (long long)tot;
    size_t keepN = min<size_t>(all.size(), (size_t)O.beam * 3);
    if (keepN < all.size()) std::nth_element(all.begin(), all.begin() + keepN, all.end(), [](const Cand& a, const Cand& b) { return a.pri > b.pri; });
    all.resize(keepN);
    sort(all.begin(), all.end(), [](const Cand& a, const Cand& b) { return a.pri > b.pri; });
    std::unordered_set<uint64_t> seen; vector<S> nxt; vector<Link> L;
    for (auto& c : all) {
      if ((int)nxt.size() >= O.beam) break;
      if (!seen.insert(c.h).second) continue;
      S s = cur[c.parent]; apply_move(s, c.delta, c.q); nxt.push_back(s); L.push_back({(int32_t)c.parent, c.delta, c.q});
    }
    layers.push_back(L); cur.swap(nxt);
    // report
    double bestF = found.empty() ? 0 : 0; for (auto& x : found) bestF = max(bestF, x.score);
    double bd = 1e18, bpri = cur.empty() ? 0 : all.front().pri; int bw = -1;
    if (!cur.empty()) { priority(cur[0], &bw); bd = target_dist(cur[0], TG[bw]); }
    printf("[%6.1fs] depth %2d (move %2zu): %zu children, beam %zu, top priority %.0f (T %d, dist %.0f to %s), dance starts %zu, best score %.0f\n",
           elapsed(), depth, prefix.size() + depth, tot, cur.size(), bpri, cur.empty() ? 0 : cur[0].T, bd, bw >= 0 ? TG[bw].name.c_str() : "-", found.size(), bestF);
    fflush(stdout);
    if (elapsed() - lastWrite > 10) { write_out(); lastWrite = elapsed(); }
  }
  write_out();
  printf("search done after %.1fs, %lld nodes generated, %zu dance-start prefixes -> %s/prefixes.txt (top %d as prefix_NNN.txt)\n", elapsed(), nodes,
         found.size(), O.outDir.c_str(), O.topOut);
  for (int i = 0; i < (int)found.size() && i < 10; i++) {
    double ad; int vx;
    S s = root; for (auto& m : found[i].seq) apply_move(s, m.delta, m.q); target_dist(s, TG[found[i].target], &ad, &vx);
    printf("  #%d score %.0f  T %lld  moves %zu  target %s  altdist %.0f  extra visits %d\n", i + 1, found[i].score, found[i].T, prefix.size() + found[i].seq.size(),
           TG[found[i].target].name.c_str(), ad, vx);
  }

  // optional: finish the best prefixes with full NRPA (one prefix per thread at a time)
  if (O.finishTop > 0 && !found.empty()) {
    int nF = min<int>(O.finishTop, (int)found.size());
    printf("finishing the top %d prefixes with NRPA level %d (full move set)\n", nF, O.finishLevel); fflush(stdout);
    auto f0 = std::chrono::steady_clock::now();
    std::atomic<int> nextF{0}; std::mutex bm; long long bestT = 0; long long ro = 0;
    auto fw = [&](int tid) {
      while (!STOP) {
        if (O.finishTime > 0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - f0).count() > O.finishTime) break;
        int i = nextF.fetch_add(1); if (i >= nF) break;
        vector<Mv> full = prefix; full.insert(full.end(), found[i].seq.begin(), found[i].seq.end());
        S s = replay(full, full.size()); NrpaCfg C; C.dance = false; long long myro = 0; C.rollouts = &myro;
        std::mt19937 rng(O.rng * 7777u + i); Policy pol; Result r = nrpa(O.finishLevel, s, pol, rng, C);
        std::lock_guard<std::mutex> lk(bm); ro += myro;
        if (!r.finished) { printf("  prefix #%d: no finish\n", i + 1); continue; }
        full.insert(full.end(), r.seq.begin(), r.seq.end());
        string txt = tour_text(full);
        printf("  prefix #%d (score %.0f): finished %lld minutes, %zu moves\n", i + 1, found[i].score, r.minutes, full.size()); fflush(stdout);
        if (r.minutes >= O.poolMin) { FILE* f = fopen((O.outDir + "/pool.txt").c_str(), "a"); if (f) { fprintf(f, "%lld\t%s\n", r.minutes, txt.c_str()); fclose(f); } }
        if (r.minutes > bestT) {
          bestT = r.minutes; FILE* f = fopen((O.outDir + "/best.txt").c_str(), "w");
          if (f) { fprintf(f, "%lld\n%s\n", r.minutes, txt.c_str()); fclose(f); }
        }
      }
    };
    vector<std::thread> th; for (int t = 0; t < max(1, O.threads); t++) th.emplace_back(fw, t);
    for (auto& x : th) x.join();
    double fd = std::chrono::duration<double>(std::chrono::steady_clock::now() - f0).count();
    printf("finishing: best %lld minutes -> %s/best.txt (%.0f rollouts/s)\n", bestT, O.outDir.c_str(), ro / max(1e-9, fd));
  }
  return 0;
}

// ---------------------------------------------------------------- score / selfcheck
static int run_score() {
  vector<Mv> t = read_first_moves(O.tour); S fin = replay(t, t.size());
  printf("%s: %zu moves, %d minutes%s\n", O.tour.c_str(), t.size(), fin.T, t.back().q == H8 ? "" : " (unfinished)");
  for (int k = O.kmin; k <= min<int>(O.kmax, (int)t.size()); k++) {
    S s = replay(t, k); if (!is_dance_start(s)) continue;
    auto b0 = std::chrono::steady_clock::now(); double db = dance_beam(s, DBEAM);
    double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - b0).count();
    printf("move %d: T %d ideal %.0f bound2 %.0f bound3 %.0f dbeam(%d) %.0f (%.2fs)", k, s.T, dance_bound(s, 0), dance_bound(s, 1), dance_bound(s, 2), DBEAM, db, dt);
    if (O.scoreLevel > 0) printf(" dnrpa(L%d x%d) %.0f", O.scoreLevel, O.scoreRuns, dance_nrpa(s, O.scoreLevel, O.scoreRuns, O.rng + k));
    printf("\n"); fflush(stdout);
  }
  return 0;
}
static int run_selfcheck() {
  vector<Mv> t = read_first_moves(O.tour); S s = start_state(); bool ok = true;
  for (size_t i = 0; i < t.size(); i++) { if (!legal_move(s, t[i])) { printf("move %zu (%d, %s) is not a legal pinned move\n", i + 1, t[i].delta, nm(t[i].q).c_str()); ok = false; break; } apply_move(s, t[i].delta, t[i].q); }
  printf("replay %s: %zu moves, %d minutes, ends on %s, legal %s\n", O.tour.c_str(), t.size(), s.T, nm(s.pos).c_str(), ok ? "yes" : "no");
  string txt = tour_text(t); vector<Mv> back = parse_moves(txt.c_str());
  bool same = back.size() == t.size(); for (size_t i = 0; same && i < t.size(); i++) same = back[i].delta == t[i].delta && back[i].q == t[i].q;
  printf("text round trip: %s\n", same ? "ok" : "MISMATCH");
  return ok && same ? 0 : 1;
}

int main(int argc, char** argv) {
  init_board(); D0 = 1; D0 = h8dist(start_state());
  for (int p = 0; p < 64; p++) { for (int q = 0; q < 64; q++) KIDX[p][q] = 0; for (int k = 0; k < NBN[p]; k++) KIDX[p][NBQ[p][k]] = k; }
  signal(SIGINT, on_sigint); signal(SIGTERM, on_sigint);
  if (argc < 2) { fprintf(stderr, "usage: goal design|search|score|selfcheck [options] (see RUN.md)\n"); return 1; }
  string mode = argv[1];
  for (int i = 2; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--tour") O.tour = nx(); else if (a == "--k") O.k = stoi(nx()); else if (a == "--kmin") O.kmin = stoi(nx()); else if (a == "--kmax") O.kmax = stoi(nx());
    else if (a == "--targets") O.targets = nx(); else if (a == "--out-dir") O.outDir = nx();
    else if (a == "--variants") O.variants = stoi(nx()); else if (a == "--edits") O.edits = stoi(nx());
    else if (a == "--score-level") O.scoreLevel = stoi(nx()); else if (a == "--score-runs") O.scoreRuns = stoi(nx());
    else if (a == "--dmin") DMIN = stoi(nx()); else if (a == "--dbeam") DBEAM = stoi(nx());
    else if (a == "--scorer") { string m = nx(); SCORE_MODE = m == "ideal" ? 0 : m == "bound2" ? 1 : 2; }
    else if (a == "--start-tour") O.startTour = nx(); else if (a == "--start-k") O.startK = stoi(nx());
    else if (a == "--threads") O.threads = stoi(nx()); else if (a == "--time") O.time = stod(nx()); else if (a == "--rng") O.rng = (unsigned)stoul(nx());
    else if (a == "--beam") O.beam = stoi(nx()); else if (a == "--max-depth") O.maxDepth = stoi(nx());
    else if (a == "--w-alt") O.wAlt = stod(nx()); else if (a == "--w-vis") O.wVis = stod(nx()); else if (a == "--w-knight") O.wKnight = stod(nx());
    else if (a == "--w-t") O.wT = stod(nx()); else if (a == "--alt-cap") O.altCap = stod(nx()); else if (a == "--noise") O.noise = stod(nx());
    else if (a == "--expand-dance") O.expandDance = true; else if (a == "--dedup-novis") O.visKey = false;
    else if (a == "--top-out") O.topOut = stoi(nx()); else if (a == "--max-per-target") O.maxPerTarget = stoi(nx());
    else if (a == "--finish-top") O.finishTop = stoi(nx()); else if (a == "--finish-level") O.finishLevel = stoi(nx()); else if (a == "--pool-min") O.poolMin = stod(nx());
    else if (a == "--finish-time") O.finishTime = stod(nx());
    else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 1; }
  }
  if (mode == "design") { string cmd = "mkdir -p '" + O.outDir + "'"; if (system(cmd.c_str())) {} return run_design(); }
  if (mode == "search") return run_search();
  if (mode == "score") return run_score();
  if (mode == "selfcheck") return run_selfcheck();
  fprintf(stderr, "unknown mode %s\n", mode.c_str()); return 1;
}
