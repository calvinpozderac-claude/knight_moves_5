// Altered States, explorer B: constructive "core + fill" search.
//
// Loop until --time:
//   1. compact: simulated annealing over grids with blanks; objective =
//      100 * (core states present, partial credit for long pieces) + number of blank cells.
//      Produces an embedding of the --core states that uses as few cells as possible.
//   2. fill: freeze the core's cells, choose letters for the blank cells to maximise the
//      true score (exhaustive when <= 4 blanks, else SA).
//   3. polish: SA over the whole grid from the filled grid (true score, may drop the core).
// Prints every layout (blanks, fill score, polish score) and the best true score.
//
// Build: g++ -O3 -march=native -std=c++17 -o build/core core.cpp
#include <bits/stdc++.h>
using namespace std;
typedef uint32_t u32;

static const u32 FULL = (1u << 25) - 1;
static const u32 COL0 = 0x108421;
static const u32 COL4 = COL0 << 4;
static u32 NB[25];
static const string LETTERS = "ABCDEFGHIJKLMNOPRSTUVWXYZ";

struct St { string name; int L; int c[16]; bool same[16]; };
static vector<St> ST;
static int NS;
static vector<int> CORE;          // indices of core states
static vector<int> POOL, CPOOL;   // letter pools: all states / core states, by frequency

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
  u32 nb = s.same[i] ? nbr_exact(cur) : block(cur);
  return nb & mask[s.c[i]];
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
static inline bool present(const u32* mask, const St& s) {
  u32 cur = mask[s.c[0]];
  for (int i = 1; i < s.L && cur; i++) cur = step(cur, s, i, mask);
  return cur != 0;
}

struct G {
  int g[25];
  u32 mask[26];
  void masks() {
    memset(mask, 0, sizeof(mask));
    for (int i = 0; i < 25; i++) if (g[i] >= 0) mask[g[i]] |= 1u << i;
  }
  string str() const {
    string r;
    for (int i = 0; i < 25; i++) r += g[i] < 0 ? '.' : char('A' + g[i]);
    return r;
  }
};

static int raw_score(const G& x) {
  int sc = 0;
  for (int s = 0; s < NS; s++) if (present(x.mask, ST[s])) sc += ST[s].L;
  return sc;
}
static double LAMBDA = 0.5;
static double true_obj(const G& x, int& raw) {
  double o = 0; raw = 0;
  for (int s = 0; s < NS; s++) {
    int f = longest_piece(x.mask, ST[s]);
    if (f == ST[s].L) { o += f; raw += f; }
    else o += LAMBDA * f * f / ST[s].L;
  }
  return o;
}
static double compact_obj(const G& x, int& ncore) {
  double o = 0; ncore = 0;
  for (int s : CORE) {
    int f = longest_piece(x.mask, ST[s]);
    if (f == ST[s].L) { o += 100; ncore++; }
    else o += 50.0 * f * f / (ST[s].L * ST[s].L);
  }
  for (int i = 0; i < 25; i++) if (x.g[i] < 0) o += 1;
  return o;
}

static mt19937_64 rng;
static double U() { return (rng() >> 11) * (1.0 / 9007199254740992.0); }

// SA over the cells in `freec`; mode 0 = compact objective, 1 = true objective
static G anneal(G x, const vector<int>& freec, int mode, long long iters, double T0, double T1,
                int& best_raw, G& best_raw_grid) {
  x.masks();
  int raw = 0, nc = 0;
  double cur = mode == 0 ? compact_obj(x, nc) : true_obj(x, raw);
  G best = x; double bestobj = cur;
  if (mode == 1 && raw > best_raw) { best_raw = raw; best_raw_grid = x; }
  double ratio = pow(T1 / T0, 1.0 / iters), T = T0;
  int nf = freec.size();
  for (long long it = 0; it < iters; it++, T *= ratio) {
    G y = x;
    if (nf >= 2 && U() < 0.2) {
      int a = freec[rng() % nf], b = freec[rng() % nf];
      if (a == b || y.g[a] == y.g[b]) continue;
      swap(y.g[a], y.g[b]);
    } else {
      int c = freec[rng() % nf];
      int l;
      if (mode == 0) l = U() < 0.25 ? -1 : CPOOL[rng() % CPOOL.size()];
      else l = U() < 0.5 ? POOL[rng() % POOL.size()] : LETTERS[rng() % LETTERS.size()] - 'A';
      if (l == y.g[c]) continue;
      y.g[c] = l;
    }
    y.masks();
    int r2 = 0, n2 = 0;
    double o = mode == 0 ? compact_obj(y, n2) : true_obj(y, r2);
    if (o >= cur || U() < exp((o - cur) / T)) {
      x = y; cur = o;
      if (mode == 1 && r2 > best_raw) { best_raw = r2; best_raw_grid = y; }
      if (o > bestobj) { bestobj = o; best = y; }
    }
  }
  return mode == 0 ? best : x;
}

// exhaustive fill of k blank cells (k <= 4)
static void exhaustive_fill(G x, const vector<int>& blanks, size_t j, int& best, G& bestg) {
  if (j == blanks.size()) {
    x.masks();
    int sc = raw_score(x);
    if (sc > best) { best = sc; bestg = x; }
    return;
  }
  for (char ch : LETTERS) {
    x.g[blanks[j]] = ch - 'A';
    exhaustive_fill(x, blanks, j + 1, best, bestg);
  }
}

int main(int argc, char** argv) {
  string core = "NORTHCAROLINA,SOUTHCAROLINA,NORTHDAKOTA,SOUTHDAKOTA";
  double tlim = 60; unsigned long long seed = 1;
  long long it_compact = 1500000, it_fill = 400000, it_polish = 1500000;
  string states = "../states.txt";
  for (int i = 1; i < argc; i++) {
    string a = argv[i];
    auto nx = [&]() { return string(argv[++i]); };
    if (a == "--core") core = nx();
    else if (a == "--time") tlim = stod(nx());
    else if (a == "--seed") seed = stoull(nx());
    else if (a == "--states") states = nx();
    else if (a == "--compact") it_compact = stoll(nx());
    else if (a == "--fill") it_fill = stoll(nx());
    else if (a == "--polish") it_polish = stoll(nx());
    else if (a == "--lambda") LAMBDA = stod(nx());
  }
  ifstream f(states);
  string w;
  while (f >> w) {
    St s; s.name = w; s.L = w.size();
    for (int i = 0; i < s.L; i++) { s.c[i] = w[i] - 'A'; s.same[i] = i > 0 && w[i] == w[i - 1]; }
    ST.push_back(s);
  }
  NS = ST.size();
  for (int c = 0; c < 25; c++) NB[c] = block(1u << c) & ~(1u << c);
  stringstream ss(core);
  string tok;
  while (getline(ss, tok, ',')) {
    for (int s = 0; s < NS; s++) if (ST[s].name == tok) CORE.push_back(s);
  }
  for (auto& s : ST) for (int i = 0; i < s.L; i++) POOL.push_back(s.c[i]);
  for (int s : CORE) for (int i = 0; i < ST[s].L; i++) CPOOL.push_back(ST[s].c[i]);
  rng.seed(seed);
  printf("core (%zu states): %s\n", CORE.size(), core.c_str());
  auto t0 = chrono::steady_clock::now();
  auto el = [&]() { return chrono::duration<double>(chrono::steady_clock::now() - t0).count(); };
  vector<int> all(25);
  iota(all.begin(), all.end(), 0);
  int best = 0; G bestg{}; int layouts = 0, failed = 0;
  map<int, int> blank_hist;
  int best_fill = 0;
  while (el() < tlim) {
    G x;
    for (int i = 0; i < 25; i++) x.g[i] = CPOOL[rng() % CPOOL.size()];
    int dummy = 0; G dg;
    G c = anneal(x, all, 0, it_compact, 30, 0.3, dummy, dg);
    c.masks();
    int nc = 0; compact_obj(c, nc);
    if (nc < (int)CORE.size()) { failed++; continue; }
    vector<int> blanks;
    for (int i = 0; i < 25; i++) if (c.g[i] < 0) blanks.push_back(i);
    layouts++; blank_hist[blanks.size()]++;
    string layout = c.str();
    int fbest = 0; G fg = c;
    if (blanks.size() <= 3) exhaustive_fill(c, blanks, 0, fbest, fg);
    else {
      for (int b : blanks) c.g[b] = POOL[rng() % POOL.size()];
      anneal(c, blanks, 1, it_fill, 3, 0.2, fbest, fg);
    }
    best_fill = max(best_fill, fbest);
    int pbest = fbest; G pg = fg;
    if (it_polish > 0) anneal(fg, all, 1, it_polish, 1.0, 0.2, pbest, pg);
    printf("[%7.1fs] layout %3d blanks %2zu %s fill %3d polish %3d %s\n", el(), layouts,
           blanks.size(), layout.c_str(), fbest, pbest, pg.str().c_str());
    if (pbest > best) { best = pbest; bestg = pg; printf("  NEW BEST %d %s\n", best, bestg.str().c_str()); }
    fflush(stdout);
  }
  printf("RESULT core=%s layouts=%d failed=%d best_fill=%d best=%d grid=%s\n", core.c_str(), layouts,
         failed, best_fill, best, bestg.str().c_str());
  printf("blank histogram:");
  for (auto& [k, v] : blank_hist) printf(" %d:%d", k, v);
  printf("\n");
}
