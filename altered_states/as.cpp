// Altered States (Jane Street puzzle) search engine.
//
// A 5x5 grid of letters scores the total length of the US states that can be
// spelled by King's moves (consecutive letters on distinct, adjacent squares;
// squares may be revisited). This program searches for high-scoring grids.
//
// Modes (--mode):
//   hc     random-restart steepest-ascent hill climbing (1-cell changes + swaps)
//   sa     simulated annealing, restarted every --iters moves
//   pt     parallel tempering (replica exchange), one ladder per thread
//   ils    iterated local search: perturb k cells of the incumbent, climb, accept if not worse
//   kopt   exhaustively check every change of up to --k cells of --init (local-optimality proof)
//   score  print the score of --init
//
// Objective: raw score, plus (when --lambda > 0) partial credit for absent states,
// lambda * f^2 / L, where f is the longest contiguous piece of the state that can be
// spelled. Only the raw score is ever reported as "score".
//
// Build: g++ -O3 -march=native -std=c++17 -pthread -o build/as as.cpp
#include <bits/stdc++.h>
using namespace std;
typedef uint32_t u32;
typedef uint64_t u64;

static const u32 FULL = (1u << 25) - 1;
static const u32 COL0 = 0x108421;      // bits 0,5,10,15,20
static const u32 COL4 = COL0 << 4;     // bits 4,9,14,19,24
static u32 NB[25];
static const string LETTERS = "ABCDEFGHIJKLMNOPRSTUVWXYZ";  // every letter used by a state (no Q)

struct St { string name; int L; int c[16]; bool same[16]; };
static vector<St> ST;
static int NS;
static u64 SW[26];  // bitset of states containing each letter
static int LETTER_W[26];

static inline u32 block(u32 m) {  // m plus all King-neighbours of m
  u32 h = m | ((m & ~COL4) << 1) | ((m & ~COL0) >> 1);
  return (h | (h << 5) | (h >> 5)) & FULL;
}
static inline u32 nbr_exact(u32 m) {  // cells adjacent to some cell of m (a cell is not its own neighbour)
  u32 r = 0;
  while (m) { r |= NB[__builtin_ctz(m)]; m &= m - 1; }
  return r;
}
static inline u32 step(u32 cur, const St& s, int i, const u32* mask) {
  u32 nb = s.same[i] ? nbr_exact(cur) : block(cur);  // block() is exact when letters differ
  return nb & mask[s.c[i]];
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
    for (int i = j + 1; i < s.L; i++) {
      cur = step(cur, s, i, mask);
      if (!cur) break;
      len++;
    }
    best = max(best, len);
  }
  return best;
}

static double LAMBDA = 0.0;

struct Grid {
  int g[25];
  u32 mask[26];
  double contrib[64];
  bool pres[64];
  double obj;
  int score;

  void set_masks() {
    memset(mask, 0, sizeof(mask));
    for (int i = 0; i < 25; i++) if (g[i] >= 0) mask[g[i]] |= 1u << i;
  }
  inline void eval_state(int s, double& c, bool& p) const {
    if (LAMBDA <= 0) { p = present(mask, ST[s]); c = p ? ST[s].L : 0; return; }
    int f = longest_piece(mask, ST[s]);
    p = (f == ST[s].L);
    c = p ? ST[s].L : LAMBDA * f * f / ST[s].L;
  }
  void full_eval() {
    set_masks();
    obj = 0; score = 0;
    for (int s = 0; s < NS; s++) {
      eval_state(s, contrib[s], pres[s]);
      obj += contrib[s];
      if (pres[s]) score += ST[s].L;
    }
  }
  string str() const {
    string r;
    for (int i = 0; i < 25; i++) r += g[i] < 0 ? '.' : char('A' + g[i]);
    return r;
  }
};

// A tentative change of up to 2 cells, evaluated incrementally (only the states that
// contain one of the letters involved can change).
struct Move { int n; int cell[2]; int to[2]; int from[2]; };

struct Delta { double dobj; int dscore; u64 aff; double nc[64]; bool np[64]; };

static void apply_cells(Grid& G, const Move& m, bool forward) {
  for (int k = 0; k < m.n; k++) {
    int c = m.cell[k];
    int from = forward ? m.from[k] : m.to[k];
    int to = forward ? m.to[k] : m.from[k];
    if (from >= 0) G.mask[from] &= ~(1u << c);
    if (to >= 0) G.mask[to] |= 1u << c;
    G.g[c] = to;
  }
}
static void try_move(Grid& G, const Move& m, Delta& d) {
  u64 aff = 0;
  for (int k = 0; k < m.n; k++) {
    if (m.from[k] >= 0) aff |= SW[m.from[k]];
    if (m.to[k] >= 0) aff |= SW[m.to[k]];
  }
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
  for (u64 a = d.aff; a; a &= a - 1) {
    int s = __builtin_ctzll(a);
    G.contrib[s] = d.nc[s]; G.pres[s] = d.np[s];
  }
  G.obj += d.dobj; G.score += d.dscore;
}
static void revert(Grid& G, const Move& m) { apply_cells(G, m, false); }

// ---------------------------------------------------------------- shared state
struct Shared {
  mutex mu;
  atomic<int> best{-1};
  string best_grid;
  chrono::steady_clock::time_point t0;
  atomic<long long> evals{0};
  atomic<long long> restarts{0};
  map<int, int> local_opt_hist;  // score of each finished run -> count
  double elapsed() const {
    return chrono::duration<double>(chrono::steady_clock::now() - t0).count();
  }
  void offer(const Grid& G, const char* who) {
    if (G.score <= best) return;
    lock_guard<mutex> lk(mu);
    if (G.score <= best) return;
    best = G.score; best_grid = G.str();
    printf("[%8.2fs] %-5s new best %3d  %s\n", elapsed(), who, best.load(), best_grid.c_str());
    fflush(stdout);
  }
  void record_run(int sc) { lock_guard<mutex> lk(mu); local_opt_hist[sc]++; restarts++; }
};
static Shared SH;

struct Opt {
  string mode = "sa";
  double time = 60;
  int threads = 4;
  u64 seed = 1;
  double T0 = 3.0, T1 = 0.3;
  long long iters = 2000000;
  double pswap = 0.3;
  int replicas = 16;
  double Tmin = 0.3, Tmax = 6.0;
  int sweep = 2000;
  int kick = 4;
  int k = 2;
  string init;
  bool freq_init = true;
};
static Opt O;

static vector<int> POOL;  // letters repeated by their number of occurrences in state names (built in main)
static int rand_letter(mt19937_64& rng) {
  if (O.freq_init) return POOL[rng() % POOL.size()];
  return LETTERS[rng() % LETTERS.size()] - 'A';
}
static int rand_letter_uniform(mt19937_64& rng) { return LETTERS[rng() % LETTERS.size()] - 'A'; }

static void random_grid(Grid& G, mt19937_64& rng) {
  for (int i = 0; i < 25; i++) G.g[i] = rand_letter(rng);
  G.full_eval();
}
static void load_grid(Grid& G, const string& s) {
  if (s.size() != 25) { fprintf(stderr, "grid must have 25 chars\n"); exit(1); }
  for (int i = 0; i < 25; i++) G.g[i] = (s[i] == '.' || s[i] == '_') ? -1 : toupper(s[i]) - 'A';
  G.full_eval();
}

static void random_move(const Grid& G, Move& m, mt19937_64& rng) {
  if ((rng() % 1000) < O.pswap * 1000) {
    int a, b;
    do { a = rng() % 25; b = rng() % 25; } while (a == b || G.g[a] == G.g[b]);
    m.n = 2; m.cell[0] = a; m.cell[1] = b;
    m.from[0] = G.g[a]; m.to[0] = G.g[b]; m.from[1] = G.g[b]; m.to[1] = G.g[a];
  } else {
    int c = rng() % 25, l;
    do { l = (rng() & 1) ? rand_letter_uniform(rng) : rand_letter(rng); } while (l == G.g[c]);
    m.n = 1; m.cell[0] = c; m.from[0] = G.g[c]; m.to[0] = l;
  }
}

static inline bool time_up() { return SH.elapsed() >= O.time; }

// ------------------------------------------------------------------- hill climbing
// Steepest ascent over all 1-cell letter changes and all swaps. Returns when no move
// strictly improves the objective.
static void climb(Grid& G, Delta& d, long long& evals) {
  for (;;) {
    double bestd = 1e-9; Move bm{}; bool found = false;
    Move m;
    for (int c = 0; c < 25; c++)
      for (char ch : LETTERS) {
        int l = ch - 'A';
        if (l == G.g[c]) continue;
        m.n = 1; m.cell[0] = c; m.from[0] = G.g[c]; m.to[0] = l;
        try_move(G, m, d); revert(G, m); evals++;
        if (d.dobj > bestd) { bestd = d.dobj; bm = m; found = true; }
      }
    for (int a = 0; a < 25; a++)
      for (int b = a + 1; b < 25; b++) {
        if (G.g[a] == G.g[b]) continue;
        m.n = 2; m.cell[0] = a; m.cell[1] = b;
        m.from[0] = G.g[a]; m.to[0] = G.g[b]; m.from[1] = G.g[b]; m.to[1] = G.g[a];
        try_move(G, m, d); revert(G, m); evals++;
        if (d.dobj > bestd) { bestd = d.dobj; bm = m; found = true; }
      }
    if (!found) return;
    try_move(G, bm, d); commit(G, d);
  }
}

static void run_hc(int tid) {
  mt19937_64 rng(O.seed * 1000003 + tid);
  Grid G; Delta d; long long ev = 0;
  while (!time_up()) {
    random_grid(G, rng);
    climb(G, d, ev);
    SH.offer(G, "hc");
    SH.record_run(G.score);
  }
  SH.evals += ev;
}

// ------------------------------------------------------------- simulated annealing
static void run_sa(int tid) {
  mt19937_64 rng(O.seed * 1000003 + tid);
  uniform_real_distribution<double> U(0, 1);
  Grid G; Delta d; Move m; long long ev = 0;
  while (!time_up()) {
    if (O.init.empty()) random_grid(G, rng); else load_grid(G, O.init);
    int run_best = G.score;
    double ratio = pow(O.T1 / O.T0, 1.0 / O.iters);
    double T = O.T0;
    for (long long it = 0; it < O.iters; it++, T *= ratio) {
      if ((it & 4095) == 0 && time_up()) break;
      random_move(G, m, rng);
      try_move(G, m, d); ev++;
      if (d.dobj >= 0 || U(rng) < exp(d.dobj / T)) {
        commit(G, d);
        if (G.score > run_best) { run_best = G.score; SH.offer(G, "sa"); }
      } else revert(G, m);
    }
    if (!time_up()) SH.record_run(run_best);
  }
  SH.evals += ev;
}

// ------------------------------------------------------------- parallel tempering
static void run_pt(int tid) {
  mt19937_64 rng(O.seed * 1000003 + tid);
  uniform_real_distribution<double> U(0, 1);
  int R = O.replicas;
  vector<Grid> G(R); vector<double> T(R);
  for (int r = 0; r < R; r++) {
    T[r] = O.Tmin * pow(O.Tmax / O.Tmin, R == 1 ? 0.0 : double(r) / (R - 1));
    random_grid(G[r], rng);
  }
  Delta d; Move m; long long ev = 0, swaps_try = 0, swaps_ok = 0;
  while (!time_up()) {
    for (int r = 0; r < R; r++) {
      for (int it = 0; it < O.sweep; it++) {
        random_move(G[r], m, rng);
        try_move(G[r], m, d); ev++;
        if (d.dobj >= 0 || U(rng) < exp(d.dobj / T[r])) {
          commit(G[r], d);
          if (G[r].score > SH.best) SH.offer(G[r], "pt");
        } else revert(G[r], m);
      }
    }
    for (int r = (rng() & 1); r + 1 < R; r += 2) {
      double a = (G[r + 1].obj - G[r].obj) * (1.0 / T[r] - 1.0 / T[r + 1]);
      swaps_try++;
      if (a >= 0 || U(rng) < exp(a)) { swap(G[r], G[r + 1]); swaps_ok++; }
    }
  }
  SH.evals += ev;
  if (tid == 0) printf("pt: swap acceptance %.3f\n", swaps_ok / max(1.0, double(swaps_try)));
}

// ---------------------------------------------------------- iterated local search
static void run_ils(int tid) {
  mt19937_64 rng(O.seed * 1000003 + tid);
  uniform_real_distribution<double> U(0, 1);
  Grid cur, G; Delta d; long long ev = 0;
  if (O.init.empty()) random_grid(cur, rng); else load_grid(cur, O.init);
  climb(cur, d, ev);
  SH.offer(cur, "ils");
  long long since_improve = 0;
  while (!time_up()) {
    G = cur;
    int k = 1 + rng() % O.kick;
    for (int j = 0; j < k; j++) G.g[rng() % 25] = rand_letter(rng);
    G.full_eval();
    climb(G, d, ev);
    SH.record_run(G.score);
    if (G.obj >= cur.obj - 1e-9) {
      if (G.score > cur.score) since_improve = 0;
      cur = G;
      SH.offer(cur, "ils");
    }
    if (++since_improve > 20000) {  // stuck: restart from scratch
      random_grid(cur, rng); climb(cur, d, ev); since_improve = 0;
    }
  }
  SH.evals += ev;
}

// ------------------------------------------------- exhaustive k-cell neighbourhood
// Every grid that differs from --init in at most k cells (any letters, including
// blanks being filled) is scored; prints any that beat or tie --init.
static void run_kopt() {
  Grid base; load_grid(base, O.init);
  printf("kopt: base score %d %s, k=%d\n", base.score, base.str().c_str(), O.k);
  vector<int> L;
  for (char ch : LETTERS) L.push_back(ch - 'A');
  int nL = L.size();
  atomic<long long> evals{0}, ties{0}, better{0};
  mutex mu;
  map<int, long long> hist;
  // enumerate cell subsets of size k, split by first cell across threads
  auto worker = [&](int tid) {
    Grid G = base;
    map<int, long long> h;
    vector<int> cells(O.k);
    long long ev = 0;
    function<void(int, int)> rec_cells;
    function<void(int)> rec_letters = [&](int j) {
      if (j == O.k) {
        int sc = 0;
        for (int s = 0; s < NS; s++) if (present(G.mask, ST[s])) sc += ST[s].L;
        ev++; h[sc]++;
        if (sc >= base.score) {
          string s = G.str();
          if (s != base.str()) {
            lock_guard<mutex> lk(mu);
            if (sc > base.score) better++; else ties++;
            if (sc > base.score || ties <= 20)
              printf("  %s %d %s\n", sc > base.score ? "BETTER" : "tie   ", sc, s.c_str());
          }
        }
        return;
      }
      int c = cells[j], orig = base.g[c];
      for (int li = 0; li < nL; li++) {
        int l = L[li];
        if (l == orig) continue;
        if (orig >= 0) G.mask[orig] &= ~(1u << c);
        G.mask[l] |= 1u << c; G.g[c] = l;
        rec_letters(j + 1);
        G.mask[l] &= ~(1u << c);
        if (orig >= 0) G.mask[orig] |= 1u << c;
        G.g[c] = orig;
      }
    };
    rec_cells = [&](int j, int start) {
      if (j == O.k) { rec_letters(0); return; }
      for (int c = start; c < 25; c++) {
        if (j == 0 && c % O.threads != tid) continue;
        cells[j] = c;
        rec_cells(j + 1, c + 1);
      }
    };
    rec_cells(0, 0);
    evals += ev;
    lock_guard<mutex> lk(mu);
    for (auto& [k, v] : h) hist[k] += v;
  };
  vector<thread> th;
  for (int t = 0; t < O.threads; t++) th.emplace_back(worker, t);
  for (auto& t : th) t.join();
  printf("kopt: %lld grids within %d cells checked in %.1fs: %lld better, %lld ties\n",
         evals.load(), O.k, SH.elapsed(), better.load(), ties.load());
  printf("kopt: top of score histogram:");
  int shown = 0;
  for (auto it = hist.rbegin(); it != hist.rend() && shown < 8; ++it, ++shown)
    printf(" %d:%lld", it->first, it->second);
  printf("\n");
}

static void load_states(const string& path) {
  ifstream f(path);
  string w;
  while (f >> w) {
    St s; s.name = w; s.L = w.size();
    for (int i = 0; i < s.L; i++) {
      s.c[i] = w[i] - 'A';
      s.same[i] = i > 0 && w[i] == w[i - 1];
    }
    ST.push_back(s);
  }
  NS = ST.size();
  if (NS == 0 || NS > 64) { fprintf(stderr, "bad states file\n"); exit(1); }
  for (int s = 0; s < NS; s++)
    for (int i = 0; i < ST[s].L; i++) { SW[ST[s].c[i]] |= 1ull << s; LETTER_W[ST[s].c[i]]++; }
  for (char ch : LETTERS) for (int w = 0; w < LETTER_W[ch - 'A']; w++) POOL.push_back(ch - 'A');
}

int main(int argc, char** argv) {
  string states = "states.txt";
  for (int i = 1; i < argc; i++) {
    string a = argv[i];
    auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--mode") O.mode = nx();
    else if (a == "--time") O.time = stod(nx());
    else if (a == "--threads") O.threads = stoi(nx());
    else if (a == "--seed") O.seed = stoull(nx());
    else if (a == "--lambda") LAMBDA = stod(nx());
    else if (a == "--T0") O.T0 = stod(nx());
    else if (a == "--T1") O.T1 = stod(nx());
    else if (a == "--iters") O.iters = stoll(nx());
    else if (a == "--pswap") O.pswap = stod(nx());
    else if (a == "--replicas") O.replicas = stoi(nx());
    else if (a == "--Tmin") O.Tmin = stod(nx());
    else if (a == "--Tmax") O.Tmax = stod(nx());
    else if (a == "--sweep") O.sweep = stoi(nx());
    else if (a == "--kick") O.kick = stoi(nx());
    else if (a == "--k") O.k = stoi(nx());
    else if (a == "--init") O.init = nx();
    else if (a == "--uniform") O.freq_init = false;
    else if (a == "--states") states = nx();
    else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 1; }
  }
  load_states(states);
  for (int c = 0; c < 25; c++) NB[c] = block(1u << c) & ~(1u << c);
  SH.t0 = chrono::steady_clock::now();

  if (O.mode == "score") {
    Grid G; load_grid(G, O.init);
    printf("score %d %s\n", G.score, G.str().c_str());
    for (int s = 0; s < NS; s++) if (G.pres[s]) printf("  %s\n", ST[s].name.c_str());
    return 0;
  }
  if (O.mode == "kopt") { run_kopt(); return 0; }

  void (*fn)(int) = nullptr;
  if (O.mode == "hc") fn = run_hc;
  else if (O.mode == "sa") fn = run_sa;
  else if (O.mode == "pt") fn = run_pt;
  else if (O.mode == "ils") fn = run_ils;
  else { fprintf(stderr, "unknown mode\n"); return 1; }
  vector<thread> th;
  for (int t = 0; t < O.threads; t++) th.emplace_back(fn, t);
  for (auto& t : th) t.join();
  printf("RESULT mode=%s lambda=%g time=%.0fs threads=%d seed=%llu best=%d grid=%s evals=%lld runs=%lld\n",
         O.mode.c_str(), LAMBDA, O.time, O.threads, (unsigned long long)O.seed, SH.best.load(),
         SH.best_grid.c_str(), SH.evals.load(), SH.restarts.load());
  if (!SH.local_opt_hist.empty()) {
    printf("run-end score histogram (top):");
    int shown = 0;
    for (auto it = SH.local_opt_hist.rbegin(); it != SH.local_opt_hist.rend() && shown < 10; ++it, ++shown)
      printf(" %d:%d", it->first, it->second);
    printf("\n");
  }
  return 0;
}
