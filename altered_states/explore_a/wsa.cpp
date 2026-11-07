// Exploration A: weighted-objective simulated annealing for Altered States.
// Search optimises sum_s W[s] * L_s * present_s (+ optional partial credit), while the
// TRUE score (all weights 1) is tracked and reported. Modes:
//   wsa   fixed weights (--weights NAME=w,...; w=0 forbids credit, w<0 penalises)
//   gls   guided local search: annealing cycles from the current grid; after each cycle
//         absent states' weights rise, present states' weights relax toward 1 (or below)
//   rsub  many short annealing runs, each with random weights (drop / double states),
//         each followed by a true-score polish; distinct top grids are logged
// Every new true best is printed. Single-threaded.
#include <bits/stdc++.h>
using namespace std;
typedef uint32_t u32; typedef uint64_t u64;
static const u32 FULL = (1u << 25) - 1, COL0 = 0x108421, COL4 = COL0 << 4;
static u32 NB[25];
static const string LETTERS = "ABCDEFGHIJKLMNOPRSTUVWXYZ";
struct St { string name; int L; int c[16]; bool same[16]; };
static vector<St> ST; static int NS; static u64 SW[26]; static vector<int> POOL;
static inline u32 block(u32 m) { u32 h = m | ((m & ~COL4) << 1) | ((m & ~COL0) >> 1); return (h | (h << 5) | (h >> 5)) & FULL; }
static inline u32 nbx(u32 m) { u32 r = 0; while (m) { r |= NB[__builtin_ctz(m)]; m &= m - 1; } return r; }
static inline u32 stp(u32 cur, const St& s, int i, const u32* mask) { return (s.same[i] ? nbx(cur) : block(cur)) & mask[s.c[i]]; }
static inline int longest(const u32* mask, const St& s) {
  int best = 0;
  for (int j = 0; j < s.L && s.L - j > best; j++) {
    u32 cur = mask[s.c[j]]; if (!cur) continue; int len = 1;
    for (int i = j + 1; i < s.L; i++) { cur = stp(cur, s, i, mask); if (!cur) break; len++; }
    best = max(best, len);
  }
  return best;
}
static inline bool present(const u32* mask, const St& s) {
  u32 cur = mask[s.c[0]];
  for (int i = 1; i < s.L && cur; i++) cur = stp(cur, s, i, mask);
  return cur != 0;
}
static double LAMBDA = 0.0;
static double W[64];
struct Grid {
  int g[25]; u32 mask[26]; double contrib[64]; bool pres[64]; double obj; int score;
  void set_masks() { memset(mask, 0, sizeof(mask)); for (int i = 0; i < 25; i++) mask[g[i]] |= 1u << i; }
  inline void ev(int s, double& c, bool& p) const {
    if (LAMBDA <= 0) { p = present(mask, ST[s]); c = p ? W[s] * ST[s].L : 0; return; }
    int f = longest(mask, ST[s]); p = f == ST[s].L;
    c = p ? W[s] * ST[s].L : LAMBDA * max(W[s], 0.0) * f * f / ST[s].L;
  }
  void full() { set_masks(); obj = 0; score = 0; for (int s = 0; s < NS; s++) { ev(s, contrib[s], pres[s]); obj += contrib[s]; if (pres[s]) score += ST[s].L; } }
  string str() const { string r; for (int i = 0; i < 25; i++) r += char('A' + g[i]); return r; }
};
struct Move { int n, cell[2], to[2], from[2]; };
struct Delta { double dobj; int dscore; u64 aff; double nc[64]; bool np[64]; };
static void apply(Grid& G, const Move& m, bool fw) {
  for (int k = 0; k < m.n; k++) { int c = m.cell[k], f = fw ? m.from[k] : m.to[k], t = fw ? m.to[k] : m.from[k];
    G.mask[f] &= ~(1u << c); G.mask[t] |= 1u << c; G.g[c] = t; }
}
static void trym(Grid& G, const Move& m, Delta& d) {
  u64 aff = 0; for (int k = 0; k < m.n; k++) aff |= SW[m.from[k]] | SW[m.to[k]];
  apply(G, m, true); d.aff = aff; d.dobj = 0; d.dscore = 0;
  for (u64 a = aff; a; a &= a - 1) { int s = __builtin_ctzll(a); G.ev(s, d.nc[s], d.np[s]);
    d.dobj += d.nc[s] - G.contrib[s]; d.dscore += (d.np[s] ? ST[s].L : 0) - (G.pres[s] ? ST[s].L : 0); }
}
static void commit(Grid& G, const Delta& d) { for (u64 a = d.aff; a; a &= a - 1) { int s = __builtin_ctzll(a); G.contrib[s] = d.nc[s]; G.pres[s] = d.np[s]; } G.obj += d.dobj; G.score += d.dscore; }
static mt19937_64 rng;
static int rl() { return (rng() & 1) ? LETTERS[rng() % LETTERS.size()] - 'A' : POOL[rng() % POOL.size()]; }
static void rmove(const Grid& G, Move& m) {
  if (rng() % 10 < 3) { int a, b; do { a = rng() % 25; b = rng() % 25; } while (a == b || G.g[a] == G.g[b]);
    m.n = 2; m.cell[0] = a; m.cell[1] = b; m.from[0] = G.g[a]; m.to[0] = G.g[b]; m.from[1] = G.g[b]; m.to[1] = G.g[a]; }
  else { int c = rng() % 25, l; do l = rl(); while (l == G.g[c]); m.n = 1; m.cell[0] = c; m.from[0] = G.g[c]; m.to[0] = l; }
}
// ---- global best tracking
static int BEST = -1; static string BESTG; static chrono::steady_clock::time_point T0C;
static double el() { return chrono::duration<double>(chrono::steady_clock::now() - T0C).count(); }
static map<string, int> TOPG;  // distinct present-state sets >= threshold
static map<string, string> TOPGRID;
static string canon(const string& g) {
  string best = g;
  for (int t = 0; t < 8; t++) { string h(25, ' ');
    for (int r = 0; r < 5; r++) for (int c = 0; c < 5; c++) { int rr = r, cc = c;
      if (t & 1) swap(rr, cc); if (t & 2) rr = 4 - rr; if (t & 4) cc = 4 - cc; h[rr * 5 + cc] = g[r * 5 + c]; }
    best = min(best, h); }
  return best;
}
static void note(const Grid& G, const char* who) {
  if (G.score >= 118) { u64 k = 0; for (int s = 0; s < NS; s++) if (G.pres[s]) k |= 1ull << s;
    string key = to_string(k); if (!TOPG.count(key)) { TOPG[key] = G.score; TOPGRID[key] = G.str(); } }
  if (G.score > BEST) { BEST = G.score; BESTG = G.str(); printf("[%8.1fs] %-6s TRUE best %3d %s\n", el(), who, BEST, BESTG.c_str()); fflush(stdout); }
}
// annealing from G (in place) under current W; returns nothing, tracks true best
static void anneal(Grid& G, long long iters, double T0, double T1, const char* who, double tlimit) {
  uniform_real_distribution<double> U(0, 1); Move m; Delta d;
  double ratio = pow(T1 / T0, 1.0 / iters), T = T0;
  for (long long it = 0; it < iters; it++, T *= ratio) {
    if ((it & 65535) == 0 && el() > tlimit) return;
    rmove(G, m); trym(G, m, d);
    if (d.dobj >= 0 || U(rng) < exp(d.dobj / T)) { commit(G, d); if (G.score > BEST || (G.score >= 118 && d.dscore > 0)) note(G, who); }
    else apply(G, m, false);
  }
}
static void randgrid(Grid& G) { for (int i = 0; i < 25; i++) G.g[i] = POOL[rng() % POOL.size()]; G.full(); }
static int idx(const string& n) { for (int s = 0; s < NS; s++) if (ST[s].name == n) return s; fprintf(stderr, "no state %s\n", n.c_str()); exit(1); }

int main(int argc, char** argv) {
  string mode = "wsa", weights, init, states = "states.txt";
  double tlim = 60, T0 = 3, T1 = 0.3, up = 0.25, relax = 0.5, wmax = 4, wmin = 0.25, pdrop = 0.15, pdouble = 0.15;
  long long iters = 20000000, polish = 3000000; u64 seed = 1; int restart_every = 0; bool norm = false;
  for (int i = 1; i < argc; i++) { string a = argv[i]; auto nx = [&]() { return string(argv[++i]); };
    if (a == "--mode") mode = nx(); else if (a == "--time") tlim = stod(nx()); else if (a == "--seed") seed = stoull(nx());
    else if (a == "--iters") iters = stoll(nx()); else if (a == "--polish") polish = stoll(nx());
    else if (a == "--T0") T0 = stod(nx()); else if (a == "--T1") T1 = stod(nx()); else if (a == "--lambda") LAMBDA = stod(nx());
    else if (a == "--weights") weights = nx(); else if (a == "--init") init = nx(); else if (a == "--states") states = nx();
    else if (a == "--up") up = stod(nx()); else if (a == "--relax") relax = stod(nx()); else if (a == "--wmax") wmax = stod(nx());
    else if (a == "--wmin") wmin = stod(nx()); else if (a == "--pdrop") pdrop = stod(nx()); else if (a == "--pdouble") pdouble = stod(nx());
    else if (a == "--restart") restart_every = stoi(nx());
    else if (a == "--norm") norm = true;
    else { fprintf(stderr, "unknown %s\n", a.c_str()); return 1; } }
  { ifstream f(states); string w; while (f >> w) { St s; s.name = w; s.L = w.size(); for (int i = 0; i < s.L; i++) { s.c[i] = w[i] - 'A'; s.same[i] = i && w[i] == w[i - 1]; } ST.push_back(s); } }
  NS = ST.size();
  for (int s = 0; s < NS; s++) for (int i = 0; i < ST[s].L; i++) { SW[ST[s].c[i]] |= 1ull << s; POOL.push_back(ST[s].c[i]); }
  for (int c = 0; c < 25; c++) NB[c] = block(1u << c) & ~(1u << c);
  rng.seed(seed * 7919 + 17); T0C = chrono::steady_clock::now();
  for (int s = 0; s < NS; s++) W[s] = 1;
  double BASEW[64];
  if (!weights.empty()) { stringstream ss(weights); string tok;
    while (getline(ss, tok, ',')) { auto p = tok.find('='); W[idx(tok.substr(0, p))] = stod(tok.substr(p + 1)); } }
  memcpy(BASEW, W, sizeof(W));
  Grid G;
  auto start = [&]() { if (init.empty()) randgrid(G); else { for (int i = 0; i < 25; i++) G.g[i] = init[i] - 'A'; G.full(); } };

  if (mode == "wsa") {  // repeated annealing runs with fixed weights, each followed by a true-score polish
    int runs = 0;
    while (el() < tlim) {
      memcpy(W, BASEW, sizeof(W)); start(); anneal(G, iters, T0, T1, "wsa", tlim);
      int wsc = G.score;
      for (int s = 0; s < NS; s++) W[s] = 1; G.full(); anneal(G, polish, 1.0, 0.2, "polish", tlim);
      printf("run %d: weighted-run true %d -> polished %d %s\n", ++runs, wsc, G.score, G.str().c_str()); fflush(stdout);
    }
  } else if (mode == "gls") {
    start(); int cyc = 0;
    while (el() < tlim) {
      anneal(G, iters, T0, T1, "gls", tlim);
      // polish a copy under true weights
      Grid P = G; double SW_[64]; memcpy(SW_, W, sizeof(W)); for (int s = 0; s < NS; s++) W[s] = 1; P.full();
      anneal(P, polish, 0.8, 0.2, "polish", tlim); memcpy(W, SW_, sizeof(W));
      // update weights
      for (int s = 0; s < NS; s++) {
        if (BASEW[s] == 0) continue;
        if (!G.pres[s]) W[s] = min(wmax, W[s] * (1 + up));
        else W[s] = max(wmin, 1 + (W[s] - 1) * relax - (up * 0.5));
      }
      if (norm) {  // keep the length-weighted mean weight at 1 so weights stay relative
        double num = 0, den = 0; for (int s = 0; s < NS; s++) if (BASEW[s] != 0) { num += W[s] * ST[s].L; den += ST[s].L; }
        for (int s = 0; s < NS; s++) if (BASEW[s] != 0) W[s] = min(wmax, max(wmin, W[s] * den / num));
      }
      cyc++;
      if (restart_every && cyc % restart_every == 0) randgrid(G); else G.full();
      if (cyc % 10 == 0) { printf("cycle %d t=%.0f cur true %d, polished %d; W:", cyc, el(), G.score, P.score);
        for (int s = 0; s < NS; s++) if (fabs(W[s] - 1) > 0.3) printf(" %s=%.2f", ST[s].name.c_str(), W[s]); printf("\n"); fflush(stdout); }
    }
  } else if (mode == "rsub") {
    int runs = 0; map<int, int> hist;
    while (el() < tlim) {
      for (int s = 0; s < NS; s++) { double r = (rng() % 10000) / 10000.0; W[s] = r < pdrop ? 0 : (r < pdrop + pdouble ? 2 : 1); }
      string desc; for (int s = 0; s < NS; s++) if (W[s] != 1) desc += (W[s] == 0 ? "-" : "+") + ST[s].name.substr(0, 5) + " ";
      start(); anneal(G, iters, T0, T1, "rsub", tlim);
      for (int s = 0; s < NS; s++) W[s] = 1; G.full(); anneal(G, polish, 1.0, 0.2, "polish", tlim);
      hist[G.score]++; runs++;
      if (G.score >= 120) { printf("run %d polished %d %s  [%s]\n", runs, G.score, G.str().c_str(), desc.c_str()); fflush(stdout); }
    }
    printf("rsub runs %d, polished-score histogram (top):", runs); int k = 0;
    for (auto it = hist.rbegin(); it != hist.rend() && k < 12; ++it, ++k) printf(" %d:%d", it->first, it->second); printf("\n");
  }
  printf("distinct state sets scoring >=118: %zu\n", TOPG.size());
  vector<pair<int, string>> v; for (auto& [g, s] : TOPG) v.push_back({s, g}); sort(v.rbegin(), v.rend());
  for (int i = 0; i < (int)v.size() && i < 15; i++) { u64 k = stoull(v[i].second); printf("  %d %s :", v[i].first, TOPGRID[v[i].second].c_str());
    for (int s = 0; s < NS; s++) if (k >> s & 1) printf(" %s", ST[s].name.c_str()); printf("\n"); }
  printf("RESULT mode=%s best=%d grid=%s\n", mode.c_str(), BEST, BESTG.c_str());
}
