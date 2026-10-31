// Large-neighbourhood search / simulated annealing over whole Knight Moves 5 tours.
// A tour is a sequence of (target square, dz) moves; dz = alt[q]-alt[p] right after the jump.
// The wait of each move is implied (pinned): delta = dz - (alt[q]-alt[p]) must be >= 0.
// Destroy+repair: from the state before move i, DFS up to D pinned moves; at every DFS node whose square
// equals the square before move j of a source tour Y (j in a window), replay Y[j..] re-deriving waits.
// When the replay breaks, a small "patch" DFS tries to re-sync further down Y (recursively).
// Every state passed is checked for a finishing jump into h8 (tour truncated there).
// usage: lns --tours f1 f2 ... --time SEC --out FILE [--seed N] [--t0 30] [--t1 1]
#include "../../src/eng.h"
#include <chrono>
#include <unordered_set>

struct M { uint8_t q; int8_t dz; };
static int TYP[64][64];
static inline bool operator==(const M& a, const M& b) { return a.q == b.q && a.dz == b.dz; }

// apply move m to s; false if illegal (s may be garbage then)
static inline bool stepM(S& s, M m) {
  int p = s.pos, q = m.q, t = TYP[p][q];
  if (t < 0) return false;
  if (q != H8 && s.vis[q] >= 3) return false;
  if (!legal_dz(t, m.dz)) return false;
  int a = s.alt[p];
  if (s.alt[q] == a && q != 63 - p) { if (m.dz != 0) return false; apply_move(s, 0, q); return true; }
  int delta = m.dz - (s.alt[q] - a);
  if (delta < 0) return false;
  apply_move(s, delta, q);
  return true;
}

static vector<M> parse_tour(const string& line, long long* Tout) {
  vector<M> out; S s = start_state();
  size_t k = 0;
  while ((k = line.find('(', k)) != string::npos) {
    int w; char sq[3];
    if (sscanf(line.c_str() + k, "(%d, %2s)", &w, sq) != 2) break;
    k++;
    int q = (sq[0] - 'a') + 8 * (sq[1] - '1');
    int p = s.pos; int n = group_n(s, p);
    vector<pair<int, int>> mv; pinned_moves(s, 3, true, mv);
    bool found = false;
    for (auto& [d, qq] : mv) if (qq == q && d * n == w) {
      apply_move(s, d, q); out.push_back({(uint8_t)q, (int8_t)(s.alt[q] - s.alt[p])}); found = true; break;
    }
    if (!found) { fprintf(stderr, "parse: move %zu (%d,%s) not pinned\n", out.size() + 1, w, sq); exit(1); }
  }
  if (Tout) *Tout = s.T;
  return out;
}

static string fmt_tour(const vector<M>& X, long long* Tout) {
  S s = start_state(); string o;
  for (size_t i = 0; i < X.size(); i++) {
    long long T0 = s.T;
    if (!stepM(s, X[i])) { fprintf(stderr, "fmt: illegal at %zu\n", i); exit(1); }
    char b[32]; snprintf(b, sizeof b, "%s(%lld, %s)", i ? ", " : "", s.T - T0, nm(X[i].q).c_str()); o += b;
  }
  if (Tout) *Tout = s.T;
  return o;
}

// full replay from a1; returns T or -1 if illegal / not ending on h8 / h8 early
static long long score_full(const vector<M>& X) {
  S s = start_state();
  for (size_t i = 0; i < X.size(); i++) {
    if (X[i].q == H8 && i + 1 != X.size()) return -1;
    if (!stepM(s, X[i])) return -1;
  }
  return (s.pos == H8) ? s.T : -1;
}

static std::mt19937_64 rng;
static double U() { return std::uniform_real_distribution<double>(0, 1)(rng); }
static int RI(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }

static int H8NB[16], H8NBN = 0;

struct Search {
  const vector<M>* Y; vector<int> P0;  // source tour and square before each move
  const vector<M>* X;                    // tour to avoid reproducing exactly
  vector<M> path; long long bestT = -1; vector<M> best;
  long long nodes = 0, budget = 200000;
  int PD = 2, R = 4;                     // patch depth and resync range
  std::unordered_set<uint64_t> memo;
  vector<pair<int, int>> mvbuf[16];

  void setY(const vector<M>& y) {
    Y = &y; P0.assign(y.size() + 1, 0); P0[0] = A1;
    for (size_t i = 0; i < y.size(); i++) P0[i + 1] = y[i].q;
  }
  void record(long long T, M fin) {
    if (T <= bestT) return;
    path.push_back(fin);
    if (!(path.size() == X->size() && std::equal(path.begin(), path.end(), X->begin()))) { bestT = T; best = path; }
    path.pop_back();
  }
  void finish_check(const S& s) {
    int p = s.pos;
    if (TYP[p][H8] < 0) return;
    int t = TYP[p][H8], a = s.alt[p];
    if (s.alt[H8] == a) { if (t == 0) record(s.T, {(uint8_t)H8, 0}); return; }
    int diff = s.alt[H8] - a, n = -1;
    for (int dz : {-2, -1, 0, 1, 2}) {
      if (!legal_dz(t, dz)) continue;
      int d = dz - diff; if (d < 0) continue;
      if (d > 0 && n < 0) n = group_n(s, p);
      record(s.T + (long long)d * (d > 0 ? n : 0), {(uint8_t)H8, (int8_t)dz});
    }
  }
  // replay Y[j..] from s; on break, patch with depth pd
  void replay(S s, int j, int pd) {
    uint64_t key = hashS(s) ^ ((uint64_t)j * 0x9E3779B97F4A7C15ULL) ^ ((uint64_t)s.T << 20) ^ (uint64_t)pd;
    if (!memo.insert(key).second) return;
    size_t base = path.size();
    const vector<M>& y = *Y;
    int k = j;
    for (; k < (int)y.size(); k++) {
      nodes++;
      finish_check(s);
      if (y[k].q == H8) break;
      S s2 = s;
      if (!stepM(s2, y[k])) { if (pd > 0) patch(s, k, pd - 1); break; }
      path.push_back(y[k]); s = s2;
    }
    path.resize(base);
  }
  void patch_dfs(S& s, int k, int d, int pd) {
    if (nodes > budget) return;
    if (d > 0) {
      for (int m = k; m <= min<int>(k + R, (int)Y->size() - 1); m++) if (P0[m] == s.pos) replay(s, m, pd);
    }
    if (d >= PD) return;
    auto& mv = mvbuf[8 + d]; pinned_moves(s, 3, false, mv);
    auto local = mv;
    for (auto& [dl, q] : local) {
      S s2 = s; int p = s.pos; apply_move(s2, dl, q);
      path.push_back({(uint8_t)q, (int8_t)(s2.alt[q] - s2.alt[p])});
      nodes++; finish_check(s2);
      patch_dfs(s2, k, d + 1, pd);
      path.pop_back();
    }
  }
  void patch(const S& s, int k, int pd) { S c = s; patch_dfs(c, k, 0, pd); }

  // window DFS from s (state before X-move i), resync with Y at j in [jlo, jhi]
  // onX: path so far equals Y[i..i+d) (Y==X case) -> skip identity
  int D, jlo, jhi, i0; bool sameXY;
  void win_dfs(S& s, int d, bool onX, int pd) {
    if (nodes > budget) return;
    if (d > 0) finish_check(s);
    for (int j = max(jlo, 0); j <= min<int>(jhi, (int)Y->size() - 1); j++) {
      if (P0[j] != s.pos) continue;
      if (sameXY && onX && j == i0 + d) continue;
      replay(s, j, pd);
    }
    if (d >= D) return;
    vector<pair<int, int>> mv; pinned_moves(s, 3, false, mv);
    for (auto& [dl, q] : mv) {
      S s2 = s; int p = s.pos; apply_move(s2, dl, q);
      M m{(uint8_t)q, (int8_t)(s2.alt[q] - s2.alt[p])};
      bool on2 = onX && sameXY && i0 + d < (int)Y->size() && (*Y)[i0 + d] == m;
      path.push_back(m); nodes++;
      win_dfs(s2, d + 1, on2, pd);
      path.pop_back();
    }
  }
};

static vector<S> states_of(const vector<M>& X) {
  vector<S> st(X.size() + 1); S s = start_state(); st[0] = s;
  for (size_t i = 0; i < X.size(); i++) { stepM(s, X[i]); st[i + 1] = s; }
  return st;
}

struct Elite { long long T; vector<M> X; };

int main(int argc, char** argv) {
  init_board();
  for (int i = 0; i < 64; i++) for (int j = 0; j < 64; j++) TYP[i][j] = -1;
  for (int i = 0; i < 64; i++) for (int k = 0; k < NBN[i]; k++) TYP[i][NBQ[i][k]] = NBT[i][k];
  vector<string> files; double TIME = 60, T0 = 30, T1 = 1; string out = "best_lns.txt"; uint64_t seed = 1;
  long long BUDGET = 200000; int DMAX = 5, XOVER = 1;
  for (int a = 1; a < argc; a++) {
    string s = argv[a];
    if (s == "--tours") { while (a + 1 < argc && argv[a + 1][0] != '-') files.push_back(argv[++a]); }
    else if (s == "--time") TIME = atof(argv[++a]);
    else if (s == "--t0") T0 = atof(argv[++a]);
    else if (s == "--t1") T1 = atof(argv[++a]);
    else if (s == "--out") out = argv[++a];
    else if (s == "--seed") seed = atoll(argv[++a]);
    else if (s == "--budget") BUDGET = atoll(argv[++a]);
    else if (s == "--dmax") DMAX = atoi(argv[++a]);
    else if (s == "--xover") XOVER = atoi(argv[++a]);
  }
  rng.seed(seed);
  vector<Elite> recs;
  for (auto& f : files) {
    std::ifstream in(f); string l, tl; while (getline(in, l)) if (l.find('(') != string::npos) { tl = l; break; }
    long long T; auto X = parse_tour(tl, &T);
    long long T2 = score_full(X);
    printf("%s: %zu moves, %lld minutes (replay %lld)\n", f.c_str(), X.size(), T, T2);
    recs.push_back({T2, X});
  }
  std::sort(recs.begin(), recs.end(), [](const Elite& a, const Elite& b) { return a.T > b.T; });
  auto t0 = std::chrono::steady_clock::now();
  auto el = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); };
  Elite gbest = recs[0];
  auto save = [&](const Elite& e, const char* why) {
    long long T; string s = fmt_tour(e.X, &T);
    FILE* f = fopen(out.c_str(), "w"); fprintf(f, "%lld\n%s\n", T, s.c_str()); fclose(f);
    printf("[%.0fs] NEW BEST %lld (%s) len %zu\n", el(), T, why, e.X.size()); fflush(stdout);
  };
  vector<Elite> elite = recs;  // pool
  auto add_elite = [&](const Elite& e) {
    for (auto& x : elite) if (x.T == e.T && x.X.size() == e.X.size() && std::equal(x.X.begin(), x.X.end(), e.X.begin())) return;
    elite.push_back(e);
    std::sort(elite.begin(), elite.end(), [](const Elite& a, const Elite& b) { return a.T > b.T; });
    if (elite.size() > 40) elite.resize(40);
  };

  // ---- crossovers: prefix of A (k moves) + small DFS + resync anywhere in B
  if (XOVER) {
    for (size_t a = 0; a < recs.size(); a++) for (size_t b = 0; b < recs.size(); b++) {
      auto stA = states_of(recs[a].X);
      Search S1; S1.setY(recs[b].X); S1.X = &recs[b].X; S1.budget = 2000000;
      long long bt = -1; vector<M> bs;
      for (size_t k = 1; k < recs[a].X.size(); k++) {
        S1.path.assign(recs[a].X.begin(), recs[a].X.begin() + k);
        S1.bestT = -1; S1.best.clear(); S1.nodes = 0; S1.memo.clear();
        S1.D = 2; S1.jlo = 1; S1.jhi = (int)recs[b].X.size() - 1; S1.i0 = -100; S1.sameXY = false; S1.PD = 2;
        S st = stA[k]; S1.win_dfs(st, 0, false, 1);
        if (S1.bestT > bt) { bt = S1.bestT; bs = S1.best; }
      }
      printf("xover %lld x %lld: best %lld\n", recs[a].T, recs[b].T, bt); fflush(stdout);
      if (bt > 0) { Elite e{bt, bs}; if (score_full(bs) != bt) printf("  MISMATCH %lld\n", score_full(bs)); else { add_elite(e); if (bt > gbest.T) { gbest = e; save(gbest, "xover"); } } }
    }
  }

  // ---- SA
  Elite cur = gbest; long long it = 0, acc = 0, impr = 0;
  long long stat_gain[8] = {0}, stat_cnt[8] = {0};
  double lastImpr = el(); int restarts = 0;
  vector<S> st = states_of(cur.X);
  while (el() < TIME) {
    it++;
    double frac = el() / TIME;
    double temp = T0 * pow(T1 / T0, fmod(frac * 4, 1.0));  // 4 annealing cycles
    int N = (int)cur.X.size();
    Search SR; SR.setY(cur.X); SR.X = &cur.X; SR.budget = BUDGET;
    int op = RI(0, 6);
    if (op >= 4) {  // segment ops on the whole tour, then replay with patching at the seams
      int lo = RI(0, 1) ? 34 : 1;
      if (N - 2 - lo < 6) continue;
      int a = RI(lo, N - 4), len = RI(1, min(24, N - 2 - a)), b = a + len;
      vector<M> Y2;
      int from = a;
      if (op == 4) {  // move segment [a,b) to position c (after b or before a)
        int c = RI(lo, N - 2);
        if (c >= a && c <= b) continue;
        if (c > b) { Y2.assign(cur.X.begin(), cur.X.begin() + a); Y2.insert(Y2.end(), cur.X.begin() + b, cur.X.begin() + c);
          Y2.insert(Y2.end(), cur.X.begin() + a, cur.X.begin() + b); Y2.insert(Y2.end(), cur.X.begin() + c, cur.X.end()); }
        else { Y2.assign(cur.X.begin(), cur.X.begin() + c); Y2.insert(Y2.end(), cur.X.begin() + a, cur.X.begin() + b);
          Y2.insert(Y2.end(), cur.X.begin() + c, cur.X.begin() + a); Y2.insert(Y2.end(), cur.X.begin() + b, cur.X.end()); from = c; }
      } else if (op == 5) {  // reverse segment [a,b): moves go back along the same squares, dz kept
        Y2 = cur.X;
        for (int k = a; k < b; k++) { int src = a + b - 1 - k; bool sit = st[src + 1].T > st[src].T; Y2[k] = {(uint8_t)(src == 0 ? A1 : cur.X[src - 1].q), (int8_t)(sit ? cur.X[src].dz : -cur.X[src].dz)}; }
        // the move into position b-1's target must lead to cur.X[b-1].q's original start: fix the last one
        Y2[b - 1].q = (a == 0 ? A1 : cur.X[a - 1].q);
        // reversed walk: start at P(a) = X[a-1].q, then visit X[b-2].q ... X[a-1].q? (patching repairs the seams)
      } else {  // duplicate segment [a,b) right after itself (repeat a dance block)
        Y2.assign(cur.X.begin(), cur.X.begin() + b); Y2.insert(Y2.end(), cur.X.begin() + a, cur.X.begin() + b);
        Y2.insert(Y2.end(), cur.X.begin() + b, cur.X.end()); from = b;
      }
      if (from >= (int)Y2.size()) continue;
      SR.setY(Y2); SR.PD = RI(1, 3); SR.R = RI(2, 8);
      SR.path.assign(cur.X.begin(), cur.X.begin() + from);
      S s0 = st[from]; SR.replay(s0, from, RI(2, 4));
      if (SR.bestT < 0) continue;
      long long dT = SR.bestT - cur.T;
      if (dT >= 0 || U() < exp(dT / temp)) {
        if (score_full(SR.best) != SR.bestT) { printf("MISMATCH op %d\n", op); continue; }
        acc++; stat_cnt[op]++; if (dT > 0) stat_gain[op] += dT;
        cur = {SR.bestT, SR.best}; st = states_of(cur.X);
        if (cur.T > gbest.T) { gbest = cur; impr++; lastImpr = el(); char w[64]; snprintf(w, 64, "op %d a %d len %d", op, a, len); save(gbest, w); }
        if (cur.T >= gbest.T - 30) add_elite(cur);
      }
      continue;
    }
    int i = RI(0, N - 2);
    SR.path.assign(cur.X.begin(), cur.X.begin() + i);
    SR.i0 = i; SR.sameXY = true; SR.PD = RI(1, 3); SR.R = RI(2, 6);
    int pd = RI(0, 2);
    if (op == 0) { SR.D = RI(1, DMAX); SR.jlo = i; SR.jhi = i + SR.D + 2; }        // window replace / delete / insert
    else if (op == 1) { SR.D = RI(1, 3); SR.jlo = i + 1; SR.jhi = i + 12; }       // skip up to 12 moves
    else if (op == 2) { SR.D = RI(2, DMAX + 1); SR.jlo = i - 4; SR.jhi = i + 4; } // detours, repeat
    else { SR.D = RI(1, 2); SR.jlo = 0; SR.jhi = N - 1; }                         // jump anywhere in own tour
    S s0 = st[i];
    SR.win_dfs(s0, 0, true, pd);
    if (SR.bestT < 0) continue;
    long long dT = SR.bestT - cur.T;
    if (dT >= 0 || U() < exp(dT / temp)) {
      if (score_full(SR.best) != SR.bestT) { printf("MISMATCH op %d\n", op); continue; }
      acc++; stat_cnt[op]++;
      if (dT > 0) stat_gain[op] += dT;
      cur = {SR.bestT, SR.best}; st = states_of(cur.X);
      if (cur.T > gbest.T) { gbest = cur; impr++; lastImpr = el(); char w[64]; snprintf(w, 64, "op %d i %d D %d", op, i, SR.D); save(gbest, w); }
      if (cur.T >= gbest.T - 30) add_elite(cur);
    }
    if (it % 2000 == 0) {
      printf("[%.0fs] it %lld acc %lld cur %lld best %lld temp %.1f elite %zu gains %lld/%lld/%lld/%lld/%lld/%lld/%lld acc %lld/%lld/%lld/%lld/%lld/%lld/%lld\n", el(), it, acc, cur.T, gbest.T, temp,
             elite.size(), stat_gain[0], stat_gain[1], stat_gain[2], stat_gain[3], stat_gain[4], stat_gain[5], stat_gain[6],
             stat_cnt[0], stat_cnt[1], stat_cnt[2], stat_cnt[3], stat_cnt[4], stat_cnt[5], stat_cnt[6]);
      fflush(stdout);
    }
    // restart from elite if stuck far below best
    if (cur.T < gbest.T - 400 || (el() - lastImpr > 120 && U() < 0.001)) {
      cur = elite[RI(0, (int)elite.size() - 1)]; st = states_of(cur.X); restarts++; lastImpr = el();
    }
  }
  printf("done it %lld acc %lld best %lld restarts %d\n", it, acc, gbest.T, restarts);
  printf("elite:"); for (auto& e : elite) printf(" %lld", e.T); printf("\n");
  // dump elite
  string ef = out + ".elite";
  FILE* f = fopen(ef.c_str(), "w");
  for (auto& e : elite) { long long T; string s = fmt_tour(e.X, &T); fprintf(f, "%lld\n%s\n", T, s.c_str()); }
  fclose(f);
}
