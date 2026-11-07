// Explorer C for Altered States: weighted annealing with rich moves, a set-feasibility
// oracle, and a local search over *sets of states* (rather than over grids).
//
// Modes:
//   sa        long single-grid annealing with rich moves (true score objective + partial credit)
//   oracle    --set A,B,C : try to find a grid containing every state of the set
//   setsearch local search over target sets, each step solved by the oracle (warm-started)
//
// Build: g++ -O3 -march=native -std=c++17 -o explore_c/build/xc explore_c/xc.cpp
#include <bits/stdc++.h>
using namespace std;
typedef uint32_t u32;
typedef uint64_t u64;

static const u32 FULL = (1u << 25) - 1;
static const u32 COL0 = 0x108421;
static const u32 COL4 = COL0 << 4;
static u32 NB[25];
static vector<int> NBL[25];
static const string LETTERS = "ABCDEFGHIJKLMNOPRSTUVWXYZ";

struct St { string name; int L; int c[16]; bool same[16]; };
static vector<St> ST;
static int NS;
static u64 SW[26];
static vector<int> POOL;

static inline u32 block(u32 m) {
  u32 h = m | ((m & ~COL4) << 1) | ((m & ~COL0) >> 1);
  return (h | (h << 5) | (h >> 5)) & FULL;
}
static inline u32 nbr_exact(u32 m) {
  u32 r = 0;
  while (m) { r |= NB[__builtin_ctz(m)]; m &= m - 1; }
  return r;
}
static inline u32 step(u32 cur, const St& s, int i, const u32* mask) {
  return (s.same[i] ? nbr_exact(cur) : block(cur)) & mask[s.c[i]];
}
static inline bool present(const u32* mask, const St& s) {
  u32 cur = mask[s.c[0]];
  for (int i = 1; i < s.L && cur; i++) cur = step(cur, s, i, mask);
  return cur != 0;
}
static inline int longest_piece(const u32* mask, const St& s) {
  int best = 0;
  for (int j = 0; j < s.L && s.L - j > best; j++) {
    u32 cur = mask[s.c[j]];
    if (!cur) continue;
    int len = 1;
    for (int i = j + 1; i < s.L; i++) { cur = step(cur, s, i, mask); if (!cur) break; len++; }
    best = max(best, len);
  }
  return best;
}

static double LAMBDA = 0.5;
static double W[64];      // per-state weight
static bool PARTIAL[64];  // compute partial credit for this state?

struct Grid {
  int g[25]; u32 mask[26]; double contrib[64]; bool pres[64]; double obj; int score;
  void set_masks() { memset(mask, 0, sizeof(mask)); for (int i = 0; i < 25; i++) mask[g[i]] |= 1u << i; }
  inline void eval_state(int s, double& c, bool& p) const {
    if (!PARTIAL[s] || LAMBDA <= 0) { p = present(mask, ST[s]); c = p ? W[s] * ST[s].L : 0; return; }
    int f = longest_piece(mask, ST[s]);
    p = (f == ST[s].L);
    c = W[s] * (p ? ST[s].L : LAMBDA * f * f / ST[s].L);
  }
  void full_eval() {
    set_masks(); obj = 0; score = 0;
    for (int s = 0; s < NS; s++) { eval_state(s, contrib[s], pres[s]); obj += contrib[s]; if (pres[s]) score += ST[s].L; }
  }
  string str() const { string r; for (int i = 0; i < 25; i++) r += char('A' + g[i]); return r; }
};
static void load_grid(Grid& G, const string& s) {
  for (int i = 0; i < 25; i++) G.g[i] = toupper(s[i]) - 'A';
  G.full_eval();
}

struct Move { int n; int cell[5]; int from[5]; int to[5]; };
struct Delta { double dobj; int dscore; u64 aff; double nc[64]; bool np[64]; };

static void apply_cells(Grid& G, const Move& m, bool fwd) {
  // remove all old letters first, then add new (handles permutations)
  for (int k = 0; k < m.n; k++) { int c = m.cell[k]; G.mask[fwd ? m.from[k] : m.to[k]] &= ~(1u << c); }
  for (int k = 0; k < m.n; k++) { int c = m.cell[k]; int t = fwd ? m.to[k] : m.from[k]; G.mask[t] |= 1u << c; G.g[c] = t; }
}
static void try_move(Grid& G, const Move& m, Delta& d) {
  u64 aff = 0;
  for (int k = 0; k < m.n; k++) aff |= SW[m.from[k]] | SW[m.to[k]];
  apply_cells(G, m, true);
  d.aff = aff; d.dobj = 0; d.dscore = 0;
  for (u64 a = aff; a; a &= a - 1) {
    int s = __builtin_ctzll(a);
    G.eval_state(s, d.nc[s], d.np[s]);
    d.dobj += d.nc[s] - G.contrib[s];
    d.dscore += (d.np[s] ? ST[s].L : 0) - (G.pres[s] ? ST[s].L : 0);
  }
}
static void commit(Grid& G, const Delta& d) {
  for (u64 a = d.aff; a; a &= a - 1) { int s = __builtin_ctzll(a); G.contrib[s] = d.nc[s]; G.pres[s] = d.np[s]; }
  G.obj += d.dobj; G.score += d.dscore;
}
static void revert(Grid& G, const Move& m) { apply_cells(G, m, false); }

static mt19937_64 rng(1);
static inline int rnd(int n) { return rng() % n; }
static int rand_letter() { return (rng() & 1) ? POOL[rnd(POOL.size())] : LETTERS[rnd(LETTERS.size())] - 'A'; }

// move-type probabilities (cumulative over: letter, swap, rot2x2, shift, copy, path)
static double PM[6] = {0.45, 0.25, 0.07, 0.05, 0.08, 0.10};
static vector<int> TARGET_STATES;  // states the path move draws from (all if empty)

static bool gen_move(const Grid& G, Move& m) {
  double u = (rng() >> 11) * (1.0 / 9007199254740992.0);
  int t = 0; double acc = PM[0];
  while (t < 5 && u > acc) acc += PM[++t];
  m.n = 0;
  auto add = [&](int c, int to) { if (G.g[c] == to) return; m.cell[m.n] = c; m.from[m.n] = G.g[c]; m.to[m.n] = to; m.n++; };
  switch (t) {
    case 0: { int c = rnd(25), l; do l = rand_letter(); while (l == G.g[c]); add(c, l); break; }
    case 1: { int a = rnd(25), b = rnd(25); if (a == b) return false; add(a, G.g[b]); add(b, G.g[a]); break; }
    case 2: { int r = rnd(4), c = rnd(4); int q[4] = {r * 5 + c, r * 5 + c + 1, r * 5 + c + 6, r * 5 + c + 5};
              bool cw = rng() & 1; for (int k = 0; k < 4; k++) add(q[k], G.g[q[(k + (cw ? 3 : 1)) % 4]]); break; }
    case 3: { int i = rnd(5); bool row = rng() & 1, fw = rng() & 1; int q[5];
              for (int k = 0; k < 5; k++) q[k] = row ? i * 5 + k : k * 5 + i;
              for (int k = 0; k < 5; k++) add(q[k], G.g[q[(k + (fw ? 4 : 1)) % 5]]); break; }
    case 4: { int c = rnd(25); int d = NBL[c][rnd(NBL[c].size())]; add(c, G.g[d]); break; }
    case 5: { int s = TARGET_STATES.empty() ? rnd(NS) : TARGET_STATES[rnd(TARGET_STATES.size())];
              int len = 2 + rnd(2); if (len > ST[s].L) len = ST[s].L; int st = rnd(ST[s].L - len + 1);
              int cells[3]; cells[0] = rnd(25);
              for (int k = 1; k < len; k++) {
                int tries = 0, d;
                do { d = NBL[cells[k - 1]][rnd(NBL[cells[k - 1]].size())]; tries++; }
                while ((d == cells[0] || (k == 2 && d == cells[1])) && tries < 10);
                if (tries >= 10) return false;
                cells[k] = d;
              }
              for (int k = 0; k < len; k++) add(cells[k], ST[s].c[st + k]);
              break; }
  }
  return m.n > 0;
}

static double now(chrono::steady_clock::time_point t0) { return chrono::duration<double>(chrono::steady_clock::now() - t0).count(); }

// Anneal G in place for `iters` moves from T0 to T1. Tracks the best TRUE-score grid in best.
// If stop_obj > 0, returns early once obj >= stop_obj (oracle success).
static void anneal(Grid& G, long long iters, double T0, double T1, Grid& best, double stop_obj = -1) {
  uniform_real_distribution<double> U(0, 1);
  double ratio = pow(T1 / T0, 1.0 / max(1LL, iters)), T = T0;
  Move m; Delta d;
  for (long long it = 0; it < iters; it++, T *= ratio) {
    if (!gen_move(G, m)) continue;
    try_move(G, m, d);
    if (d.dobj >= 0 || U(rng) < exp(d.dobj / T)) {
      commit(G, d);
      if (G.score > best.score) best = G;
      if (stop_obj > 0 && G.obj >= stop_obj - 1e-9) return;
    } else revert(G, m);
  }
}

static void set_weights_all() { for (int s = 0; s < NS; s++) { W[s] = 1; PARTIAL[s] = true; } }
// target set: weight 1 + partial credit; others small weight, no partial credit
static double set_weights_target(const vector<bool>& in, double eps) {
  double full = 0;
  for (int s = 0; s < NS; s++) {
    if (in[s]) { W[s] = 1; PARTIAL[s] = true; full += ST[s].L; }
    else { W[s] = eps; PARTIAL[s] = false; }
  }
  return full;
}

static int find_state(const string& n) { for (int s = 0; s < NS; s++) if (ST[s].name == n) return s; return -1; }

static void print_grid(const char* tag, const Grid& G, double t) {
  printf("[%8.1fs] %s %d %s :", t, tag, G.score, G.str().c_str());
  for (int s = 0; s < NS; s++) if (G.pres[s]) printf(" %s", ST[s].name.c_str());
  printf("\n"); fflush(stdout);
}

int main(int argc, char** argv) {
  string mode = "sa", init, setarg, states = "states.txt", scramble;
  double time_lim = 60, T0 = 3, T1 = 0.25, eps = 0.05;
  long long iters = 200000000, oiters = 4000000;
  u64 seed = 1;
  int maxadd = 3, maxdrop = 2, slack = 0;
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { return string(argv[++i]); };
    if (a == "--mode") mode = nx(); else if (a == "--init") init = nx(); else if (a == "--set") setarg = nx();
    else if (a == "--time") time_lim = stod(nx()); else if (a == "--T0") T0 = stod(nx()); else if (a == "--T1") T1 = stod(nx());
    else if (a == "--iters") iters = stoll(nx()); else if (a == "--oiters") oiters = stoll(nx());
    else if (a == "--seed") seed = stoull(nx()); else if (a == "--lambda") LAMBDA = stod(nx());
    else if (a == "--eps") eps = stod(nx()); else if (a == "--maxadd") maxadd = stoi(nx());
    else if (a == "--maxdrop") maxdrop = stoi(nx()); else if (a == "--slack") slack = stoi(nx());
    else if (a == "--states") states = nx(); else if (a == "--scramble") scramble = nx();
    else if (a == "--pm") { string v = nx(); stringstream ss(v); for (int k = 0; k < 6; k++) { string x; getline(ss, x, ','); PM[k] = stod(x); } }
    else { fprintf(stderr, "unknown %s\n", a.c_str()); return 1; }
  }
  rng.seed(seed * 7919 + 17);
  { ifstream f(states); string w;
    while (f >> w) { St s; s.name = w; s.L = w.size(); for (int i = 0; i < s.L; i++) { s.c[i] = w[i] - 'A'; s.same[i] = i && w[i] == w[i - 1]; } ST.push_back(s); } }
  NS = ST.size();
  for (int s = 0; s < NS; s++) for (int i = 0; i < ST[s].L; i++) { SW[ST[s].c[i]] |= 1ull << s; POOL.push_back(ST[s].c[i]); }
  for (int c = 0; c < 25; c++) { NB[c] = block(1u << c) & ~(1u << c); for (int d = 0; d < 25; d++) if (NB[c] >> d & 1) NBL[c].push_back(d); }
  auto t0 = chrono::steady_clock::now();
  Grid best; best.score = -1;

  if (mode == "sa") {
    set_weights_all();
    int run = 0;
    while (now(t0) < time_lim) {
      Grid G;
      if (init.empty()) { for (int i = 0; i < 25; i++) G.g[i] = POOL[rnd(POOL.size())]; }
      else { for (int i = 0; i < 25; i++) G.g[i] = init[i] - 'A';
             for (char ch : scramble) { (void)ch; } }
      if (!scramble.empty()) {  // scramble listed cells (comma separated indices) or "rN" = N random cells
        if (scramble[0] == 'r') { int n = stoi(scramble.substr(1)); for (int k = 0; k < n; k++) G.g[rnd(25)] = POOL[rnd(POOL.size())]; }
        else { stringstream ss(scramble); string x; while (getline(ss, x, ',')) G.g[stoi(x)] = POOL[rnd(POOL.size())]; }
      }
      G.full_eval();
      Grid rb = G;
      long long left = (long long)iters;
      anneal(G, left, T0, T1, rb);
      run++;
      printf("run %d end %d best-in-run %d (%.0fs)\n", run, G.score, rb.score, now(t0));
      if (rb.score > best.score) { best = rb; print_grid("sa best", best, now(t0)); }
      fflush(stdout);
    }
  } else if (mode == "oracle") {
    vector<bool> in(NS, false);
    stringstream ss(setarg); string x;
    while (getline(ss, x, ',')) { int s = find_state(x); if (s < 0) { fprintf(stderr, "no state %s\n", x.c_str()); return 1; } in[s] = true; }
    double full = set_weights_target(in, eps);
    for (int s = 0; s < NS; s++) if (in[s]) TARGET_STATES.push_back(s);
    int run = 0;
    while (now(t0) < time_lim) {
      Grid G; for (int i = 0; i < 25; i++) G.g[i] = POOL[rnd(POOL.size())]; G.full_eval();
      Grid rb = G;
      anneal(G, oiters, T0, T1, rb, full);
      double miss = 0; for (int s = 0; s < NS; s++) if (in[s] && !G.pres[s]) miss += ST[s].L;
      run++;
      if (rb.score > best.score) { best = rb; }
      printf("run %d missing %.0f  true %d %s\n", run, miss, G.score, G.str().c_str());
      if (miss == 0) { print_grid("FEASIBLE", G, now(t0)); break; }
    }
    print_grid("best true", best, now(t0));
  } else if (mode == "setsearch") {
    // current grid & its present set
    Grid G;
    if (init.empty()) { set_weights_all(); for (int i = 0; i < 25; i++) G.g[i] = POOL[rnd(POOL.size())]; G.full_eval(); Grid rb = G; anneal(G, 20000000, 3, 0.25, rb); G = rb; }
    else load_grid(G, init);
    set_weights_all(); G.full_eval();
    best = G;
    print_grid("start", G, now(t0));
    vector<int> tabu(NS, -1000);
    long long step_no = 0, succ = 0;
    while (now(t0) < time_lim) {
      step_no++;
      vector<bool> in(NS); for (int s = 0; s < NS; s++) in[s] = G.pres[s];
      int cur_len = G.score;
      vector<int> absent, pres;
      for (int s = 0; s < NS; s++) (in[s] ? pres : absent).push_back(s);
      int nadd = 1 + rnd(maxadd), ndrop = rnd(maxdrop + 1);
      int newlen = cur_len;
      vector<int> added, dropped;
      for (int k = 0; k < nadd && !absent.empty(); k++) { int j = rnd(absent.size()); int s = absent[j]; absent.erase(absent.begin() + j); in[s] = true; newlen += ST[s].L; added.push_back(s); }
      for (int k = 0; k < ndrop && !pres.empty(); k++) {
        int j = rnd(pres.size()); int s = pres[j]; pres.erase(pres.begin() + j);
        if (step_no - tabu[s] < 5) continue;
        in[s] = false; newlen -= ST[s].L; dropped.push_back(s);
      }
      if (newlen < cur_len - slack) continue;  // only explore sets that are not much worse
      double full = set_weights_target(in, eps);
      TARGET_STATES.clear(); for (int s = 0; s < NS; s++) if (in[s]) TARGET_STATES.push_back(s);
      // oracle: warm start from current grid (perturbed), then a couple of cold attempts if warm fails
      bool ok = false; Grid found;
      for (int attempt = 0; attempt < 2 && !ok; attempt++) {
        Grid H = G;
        if (attempt == 1) for (int i = 0; i < 25; i++) H.g[i] = POOL[rnd(POOL.size())];
        H.full_eval();
        Grid rb = H; rb.score = -1;
        anneal(H, oiters, attempt == 0 ? 1.5 : 3.0, 0.2, rb, full);
        if (H.obj >= full - 1e-9) { ok = true; found = H; }
        // also record true-score improvements found en route
        if (rb.score > best.score) { set_weights_all(); rb.full_eval(); if (rb.score > best.score) { best = rb; print_grid("BEST(enroute)", best, now(t0)); } set_weights_target(in, eps); }
      }
      set_weights_all();
      if (ok) {
        found.full_eval();
        succ++;
        for (int s : dropped) tabu[s] = step_no;
        if (found.score >= cur_len - slack) {
          G = found;
          if (G.score > best.score) { best = G; print_grid("BEST", best, now(t0)); }
        }
      }
      if (step_no % 20 == 0) { printf("step %lld succ %lld cur %d best %d (%.0fs)\n", step_no, succ, G.score, best.score, now(t0)); fflush(stdout); }
    }
  }
  set_weights_all(); best.full_eval();
  print_grid("FINAL", best, now(t0));
  return 0;
}
