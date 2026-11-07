// Structure-aware searches for Altered States (raw score only).
//   --mode lns   state-insertion large-neighbourhood search: embed an absent state as a
//                cheapest King's walk (DP over cells, cost = cells whose letter changes,
//                optional penalty for cells on present states' witness paths), write it,
//                repair with a short low-temperature SA, accept by late acceptance.
//   --mode lahc  late-acceptance hill climbing with random 1-cell / swap moves
//   --mode tabu  tabu search over the full 1-cell + swap neighbourhood
//   --mode ga    memetic GA: SA-polished population, block crossover (best of 8 D4 images
//                of parent B), SA polish, steady-state replacement
// Build: g++ -O3 -march=native -std=c++17 -pthread -o lns/build/lns lns/lns.cpp
#include <bits/stdc++.h>
using namespace std;
typedef uint32_t u32;
typedef uint64_t u64;

static const u32 FULL = (1u << 25) - 1;
static const u32 COL0 = 0x108421, COL4 = COL0 << 4;
static u32 NB[25];
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
static inline u32 nbr_exact(u32 m) { u32 r = 0; while (m) { r |= NB[__builtin_ctz(m)]; m &= m - 1; } return r; }
static inline u32 step(u32 cur, const St& s, int i, const u32* mask) {
  return (s.same[i] ? nbr_exact(cur) : block(cur)) & mask[s.c[i]];
}
static inline bool present(const u32* mask, const St& s) {
  u32 cur = mask[s.c[0]];
  for (int i = 1; i < s.L && cur; i++) cur = step(cur, s, i, mask);
  return cur != 0;
}

struct Grid {
  int g[25]; u32 mask[26]; bool pres[64]; int score;
  void full_eval() {
    memset(mask, 0, sizeof(mask));
    for (int i = 0; i < 25; i++) if (g[i] >= 0) mask[g[i]] |= 1u << i;
    score = 0;
    for (int s = 0; s < NS; s++) { pres[s] = present(mask, ST[s]); if (pres[s]) score += ST[s].L; }
  }
  string str() const { string r; for (int i = 0; i < 25; i++) r += g[i] < 0 ? '.' : char('A' + g[i]); return r; }
};
struct Move { int n; int cell[2]; int to[2]; int from[2]; };
struct Delta { int dscore; u64 aff; bool np[64]; };
static void apply_cells(Grid& G, const Move& m, bool fw) {
  for (int k = 0; k < m.n; k++) {
    int c = m.cell[k], from = fw ? m.from[k] : m.to[k], to = fw ? m.to[k] : m.from[k];
    if (from >= 0) G.mask[from] &= ~(1u << c);
    if (to >= 0) G.mask[to] |= 1u << c;
    G.g[c] = to;
  }
}
static void try_move(Grid& G, const Move& m, Delta& d) {
  u64 aff = 0;
  for (int k = 0; k < m.n; k++) { if (m.from[k] >= 0) aff |= SW[m.from[k]]; if (m.to[k] >= 0) aff |= SW[m.to[k]]; }
  apply_cells(G, m, true);
  d.aff = aff; d.dscore = 0;
  for (u64 a = aff; a; a &= a - 1) {
    int s = __builtin_ctzll(a);
    d.np[s] = present(G.mask, ST[s]);
    d.dscore += (d.np[s] ? ST[s].L : 0) - (G.pres[s] ? ST[s].L : 0);
  }
}
static void commit(Grid& G, const Delta& d) {
  for (u64 a = d.aff; a; a &= a - 1) { int s = __builtin_ctzll(a); G.pres[s] = d.np[s]; }
  G.score += d.dscore;
}
static void revert(Grid& G, const Move& m) { apply_cells(G, m, false); }

// ------------------------------------------------------------------ options/shared
struct Opt {
  string mode = "lns"; double time = 60; int threads = 2; u64 seed = 1;
  int lahc_len = 2000; double protect = 0.5; long long repair = 20000; double Trep = 0.6;
  int pop = 24; long long polish = 300000; int tenure = 12; string init;
} O;
struct Shared {
  mutex mu; atomic<int> best{-1}; string best_grid;
  chrono::steady_clock::time_point t0; atomic<long long> evals{0}, iters{0};
  double el() const { return chrono::duration<double>(chrono::steady_clock::now() - t0).count(); }
  void offer(const Grid& G, const char* who) {
    if (G.score <= best) return;
    lock_guard<mutex> lk(mu);
    if (G.score <= best) return;
    best = G.score; best_grid = G.str();
    printf("[%8.2fs] %-5s new best %3d  %s\n", el(), who, best.load(), best_grid.c_str()); fflush(stdout);
  }
} SH;
static inline bool time_up() { return SH.el() >= O.time; }
static int rand_letter(mt19937_64& rng) { return (rng() & 1) ? POOL[rng() % POOL.size()] : LETTERS[rng() % LETTERS.size()] - 'A'; }
static void random_grid(Grid& G, mt19937_64& rng) { for (int i = 0; i < 25; i++) G.g[i] = POOL[rng() % POOL.size()]; G.full_eval(); }
static void load_grid(Grid& G, const string& s) { for (int i = 0; i < 25; i++) G.g[i] = toupper(s[i]) - 'A'; G.full_eval(); }
static void random_move(const Grid& G, Move& m, mt19937_64& rng) {
  if (rng() % 10 < 3) {
    int a, b; do { a = rng() % 25; b = rng() % 25; } while (a == b || G.g[a] == G.g[b]);
    m.n = 2; m.cell[0] = a; m.cell[1] = b; m.from[0] = G.g[a]; m.to[0] = G.g[b]; m.from[1] = G.g[b]; m.to[1] = G.g[a];
  } else {
    int c = rng() % 25, l; do { l = rand_letter(rng); } while (l == G.g[c]);
    m.n = 1; m.cell[0] = c; m.from[0] = G.g[c]; m.to[0] = l;
  }
}
// short SA (geometric T0 -> T0/10), keeps the best grid seen
static long long sa_polish(Grid& G, long long n, double T0, mt19937_64& rng) {
  uniform_real_distribution<double> U(0, 1);
  Grid best = G; Move m; Delta d;
  double T = T0, r = pow(0.1, 1.0 / max(1LL, n));
  for (long long i = 0; i < n; i++, T *= r) {
    random_move(G, m, rng); try_move(G, m, d);
    if (d.dscore >= 0 || U(rng) < exp(d.dscore / T)) { commit(G, d); if (G.score > best.score) best = G; }
    else revert(G, m);
  }
  G = best; return n;
}

// ---------------------------------------------------------- state insertion (LNS)
// cells lying on some witness walk of a present state (forward ∩ backward reachability)
static void witness_load(const Grid& G, double* load) {
  for (int c = 0; c < 25; c++) load[c] = 0;
  u32 F[16], B[16];
  for (int s = 0; s < NS; s++) {
    if (!G.pres[s]) continue;
    const St& S = ST[s];
    F[0] = G.mask[S.c[0]];
    for (int i = 1; i < S.L; i++) F[i] = step(F[i - 1], S, i, G.mask);
    B[S.L - 1] = F[S.L - 1];
    for (int i = S.L - 2; i >= 0; i--) {
      u32 nb = (S.c[i] == S.c[i + 1]) ? nbr_exact(B[i + 1]) : block(B[i + 1]);
      B[i] = F[i] & nb;
    }
    u32 on = 0; for (int i = 0; i < S.L; i++) on |= B[i];
    double w = double(S.L) / __builtin_popcount(on);  // spread the state's value over its cells
    for (u32 t = on; t; t &= t - 1) load[__builtin_ctz(t)] += w;
  }
}
// cheapest King's walk spelling state s; noise randomises ties. Returns false on a
// letter conflict (walk revisits a cell needing two different letters).
static bool embed(const Grid& G, int s, const double* load, double pw, mt19937_64& rng, int* path) {
  const St& S = ST[s];
  static thread_local double cost[16][25]; static thread_local int from[16][25];
  uniform_real_distribution<double> U(0, 0.3);
  auto cc = [&](int c, int l) { return G.g[c] == l ? 0.0 : 1.0 + pw * load[c] + U(rng); };
  for (int c = 0; c < 25; c++) { cost[0][c] = cc(c, S.c[0]); from[0][c] = -1; }
  for (int i = 1; i < S.L; i++)
    for (int c = 0; c < 25; c++) {
      double b = 1e18; int bd = -1;
      for (u32 t = NB[c]; t; t &= t - 1) { int d = __builtin_ctz(t); if (cost[i - 1][d] < b) { b = cost[i - 1][d]; bd = d; } }
      cost[i][c] = b + cc(c, S.c[i]); from[i][c] = bd;
    }
  int bc = 0; for (int c = 1; c < 25; c++) if (cost[S.L - 1][c] < cost[S.L - 1][bc]) bc = c;
  for (int i = S.L - 1; i >= 0; i--) { path[i] = bc; bc = from[i][bc]; }
  int want[25]; for (int c = 0; c < 25; c++) want[c] = -1;
  for (int i = 0; i < S.L; i++) { int c = path[i]; if (want[c] >= 0 && want[c] != S.c[i]) return false; want[c] = S.c[i]; }
  return true;
}

static void run_lns(int tid) {
  mt19937_64 rng(O.seed * 7919 + tid);
  Grid cur; if (O.init.empty()) { random_grid(cur, rng); sa_polish(cur, 2000000, 3.0, rng); } else load_grid(cur, O.init);
  SH.offer(cur, "lns");
  vector<int> hist(O.lahc_len, cur.score);
  long long it = 0, ev = 0; double load[25]; int path[16];
  while (!time_up()) {
    Grid G = cur;
    // pick absent state weighted by length
    int tot = 0; for (int s = 0; s < NS; s++) if (!G.pres[s]) tot += ST[s].L;
    if (tot == 0) break;
    int r = rng() % tot, s = 0;
    for (s = 0; s < NS; s++) if (!G.pres[s]) { if (r < ST[s].L) break; r -= ST[s].L; }
    witness_load(G, load);
    double pw = (rng() & 1) ? O.protect : 0.0;
    bool ok = false;
    for (int t = 0; t < 12 && !ok; t++) ok = embed(G, s, load, pw, rng, path);
    if (!ok) { it++; continue; }
    for (int i = 0; i < ST[s].L; i++) G.g[path[i]] = ST[s].c[i];
    G.full_eval();
    ev += sa_polish(G, O.repair, O.Trep, rng);
    int v = it % O.lahc_len;
    if (G.score >= cur.score || G.score >= hist[v]) cur = G;
    hist[v] = cur.score;
    SH.offer(cur, "lns");
    it++;
  }
  SH.evals += ev; SH.iters += it;
}

static void run_lahc(int tid) {
  mt19937_64 rng(O.seed * 7919 + tid);
  Grid G; if (O.init.empty()) random_grid(G, rng); else load_grid(G, O.init);
  vector<int> hist(O.lahc_len, G.score);
  Move m; Delta d; long long it = 0, idle = 0;
  while (true) {
    if ((it & 4095) == 0 && time_up()) break;
    random_move(G, m, rng); try_move(G, m, d);
    int ns = G.score + d.dscore, v = it % O.lahc_len;
    if (ns >= G.score || ns >= hist[v]) { if (ns > G.score) idle = 0; commit(G, d); if (G.score > SH.best) SH.offer(G, "lahc"); }
    else revert(G, m);
    hist[v] = G.score; it++;
    if (++idle > 50LL * O.lahc_len * 100) { random_grid(G, rng); fill(hist.begin(), hist.end(), G.score); idle = 0; }
  }
  SH.evals += it;
}

static void run_tabu(int tid) {
  mt19937_64 rng(O.seed * 7919 + tid);
  Grid G; if (O.init.empty()) random_grid(G, rng); else load_grid(G, O.init);
  static thread_local long long tabu[25][26];
  memset(tabu, 0, sizeof(tabu));
  Move m; Delta d; long long it = 0, ev = 0, since = 0; int lbest = G.score;
  while (!time_up()) {
    it++;
    int bd = INT_MIN, nb = 0; Move bm{};
    auto consider = [&](const Move& mv) {
      try_move(G, mv, d); revert(G, mv); ev++;
      bool tb = false;
      for (int k = 0; k < mv.n; k++) if (tabu[mv.cell[k]][mv.to[k]] > it) tb = true;
      if (tb && G.score + d.dscore <= lbest) return;
      if (d.dscore > bd) { bd = d.dscore; bm = mv; nb = 1; }
      else if (d.dscore == bd && rng() % (++nb) == 0) bm = mv;
    };
    for (int c = 0; c < 25; c++) for (char ch : LETTERS) {
      int l = ch - 'A'; if (l == G.g[c]) continue;
      m.n = 1; m.cell[0] = c; m.from[0] = G.g[c]; m.to[0] = l; consider(m);
    }
    for (int a = 0; a < 25; a++) for (int b = a + 1; b < 25; b++) {
      if (G.g[a] == G.g[b]) continue;
      m.n = 2; m.cell[0] = a; m.cell[1] = b; m.from[0] = G.g[a]; m.to[0] = G.g[b]; m.from[1] = G.g[b]; m.to[1] = G.g[a]; consider(m);
    }
    try_move(G, bm, d); commit(G, d);
    for (int k = 0; k < bm.n; k++) tabu[bm.cell[k]][bm.from[k]] = it + O.tenure + rng() % O.tenure;
    if (G.score > lbest) { lbest = G.score; since = 0; SH.offer(G, "tabu"); }
    if (++since > 3000) { random_grid(G, rng); lbest = G.score; since = 0; memset(tabu, 0, sizeof(tabu)); }
  }
  SH.evals += ev; SH.iters += it;
}

// ------------------------------------------------------------------- memetic GA
static int sym_cell(int t, int r, int c) {
  int rr = r, cc = c;
  if (t & 4) swap(rr, cc);
  if (t & 1) rr = 4 - rr;
  if (t & 2) cc = 4 - cc;
  return rr * 5 + cc;
}
static void run_ga(int tid) {
  mt19937_64 rng(O.seed * 7919 + tid);
  vector<Grid> P(O.pop);
  for (auto& g : P) { random_grid(g, rng); sa_polish(g, O.polish, 3.0, rng); SH.offer(g, "ga"); if (time_up()) return; }
  long long gen = 0;
  while (!time_up()) {
    // tournament selection
    auto pick = [&]() { int a = rng() % O.pop, b = rng() % O.pop; return P[a].score >= P[b].score ? a : b; };
    int A = pick(), B = pick(); if (A == B) B = (A + 1) % O.pop;
    int r0 = rng() % 5, r1 = r0 + rng() % (5 - r0), c0 = rng() % 5, c1 = c0 + rng() % (5 - c0);
    Grid best; best.score = -1;
    for (int t = 0; t < 8; t++) {
      Grid ch;
      for (int r = 0; r < 5; r++) for (int c = 0; c < 5; c++) {
        bool inA = r >= r0 && r <= r1 && c >= c0 && c <= c1;
        ch.g[r * 5 + c] = inA ? P[A].g[r * 5 + c] : P[B].g[sym_cell(t, r, c)];
      }
      ch.full_eval();
      if (ch.score > best.score) best = ch;
    }
    sa_polish(best, O.polish, 1.0, rng);
    SH.offer(best, "ga");
    string bs = best.str(); bool dup = false;
    for (auto& g : P) if (g.str() == bs) dup = true;
    int w = 0; for (int i = 1; i < O.pop; i++) if (P[i].score < P[w].score) w = i;
    if (!dup && best.score >= P[w].score) P[w] = best;
    gen++;
  }
  SH.iters += gen;
  if (tid == 0) {
    lock_guard<mutex> lk(SH.mu);
    printf("ga final population scores:"); for (auto& g : P) printf(" %d", g.score); printf("\n");
  }
}

int main(int argc, char** argv) {
  string states = "states.txt";
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { return string(argv[++i]); };
    if (a == "--mode") O.mode = nx(); else if (a == "--time") O.time = stod(nx());
    else if (a == "--threads") O.threads = stoi(nx()); else if (a == "--seed") O.seed = stoull(nx());
    else if (a == "--lahc") O.lahc_len = stoi(nx()); else if (a == "--protect") O.protect = stod(nx());
    else if (a == "--repair") O.repair = stoll(nx()); else if (a == "--Trep") O.Trep = stod(nx());
    else if (a == "--pop") O.pop = stoi(nx()); else if (a == "--polish") O.polish = stoll(nx());
    else if (a == "--tenure") O.tenure = stoi(nx()); else if (a == "--init") O.init = nx();
    else if (a == "--states") states = nx(); else { fprintf(stderr, "unknown %s\n", a.c_str()); return 1; }
  }
  ifstream f(states); string w;
  while (f >> w) { St s; s.name = w; s.L = w.size(); for (int i = 0; i < s.L; i++) { s.c[i] = w[i] - 'A'; s.same[i] = i && w[i] == w[i - 1]; } ST.push_back(s); }
  NS = ST.size();
  for (int s = 0; s < NS; s++) for (int i = 0; i < ST[s].L; i++) { SW[ST[s].c[i]] |= 1ull << s; POOL.push_back(ST[s].c[i]); }
  for (int c = 0; c < 25; c++) NB[c] = block(1u << c) & ~(1u << c);
  SH.t0 = chrono::steady_clock::now();
  void (*fn)(int) = O.mode == "lns" ? run_lns : O.mode == "lahc" ? run_lahc : O.mode == "tabu" ? run_tabu : O.mode == "ga" ? run_ga : nullptr;
  if (!fn) { fprintf(stderr, "bad mode\n"); return 1; }
  vector<thread> th; for (int t = 0; t < O.threads; t++) th.emplace_back(fn, t);
  for (auto& t : th) t.join();
  printf("RESULT mode=%s time=%.0fs threads=%d seed=%llu best=%d grid=%s evals=%lld iters=%lld\n", O.mode.c_str(), O.time, O.threads,
         (unsigned long long)O.seed, SH.best.load(), SH.best_grid.c_str(), SH.evals.load(), SH.iters.load());
}
