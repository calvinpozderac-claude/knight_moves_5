// Coverage-heuristic search (C++ port of the "Updated A*" notebook), multi-threaded.
//
// Moves (as in the notebook's possible_moves): the necessary sink for every orthogonal (dz +-2), two-away
// (dz +-1) and knight jump to a lower point, plus same-level knight jumps after waiting any multiple of the
// group size n, up to how far the group could usefully sink. Every jump is re-checked after the sink.
// Heuristic (the notebook's reach_end + score): a backward breadth-first search from h8 finds the points from
// which h8 can still be reached (each step may sink the point's own group once; points whose height an
// earlier step relies on are "restricted"). A position scores the number of distinct points visited if the
// knight (or one of its moves) is on that set, else 0; the search orders positions by score / total visits.
// Search: a parallel BFS from the start (or a prefix) until --seed positions are open, then every thread runs
// the notebook's randomised best-first search on its share of the frontier: draw l in [lmin, lmax], pick one
// of the l best positions with probability ~ exp((score - best) / tau). Positions live in a bucket queue
// (score quantised to 1/4096), so the top l are found in O(l). Finished tours must pass a necessity check
// (every wait: some other wait makes the tour illegal) before they count; improvements go to --out.
//
// usage: coverage_search [--threads 16] [--seed 16000000] [--time 7200] [--open-cap 2000000]
//                        [--node-cap 25000000] [--tau 1] [--lmin 5] [--lmax 150] [--out runs/coverage_best.txt]
//                        [--prefix "(0, b1), ..."] [--rng 1]
#include "eng.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <thread>

static const int SC = 4096;  // score buckets
static const uint32_t NIL = 0xFFFFFFFFu, LOCAL = 0x80000000u;

// ---------------------------------------------------------------- compact state
struct CS {
  int16_t alt[64];
  uint8_t vis[16];  // 2 bits per point
  uint8_t pos;
  int32_t T;
};
static inline void pack(const S& s, CS& c) {
  memcpy(c.alt, s.alt, sizeof c.alt); memset(c.vis, 0, 16);
  for (int i = 0; i < 64; i++) c.vis[i >> 2] |= (uint8_t)(min<int>(s.vis[i], 3) << ((i & 3) * 2));
  c.pos = s.pos; c.T = s.T;
}
static inline void unpack(const CS& c, S& s) {
  memcpy(s.alt, c.alt, sizeof c.alt);
  for (int i = 0; i < 64; i++) s.vis[i] = (c.vis[i >> 2] >> ((i & 3) * 2)) & 3;
  s.pos = c.pos; s.T = c.T;
}

// ---------------------------------------------------------------- moves
struct Mv { int t, q; };  // t: minutes waited, q: target
static const int ORTH[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
static const int KN[8][2] = {{1, 2}, {2, 1}, {-1, 2}, {2, -1}, {1, -2}, {-2, 1}, {-1, -2}, {-2, -1}};
static inline bool onb(int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8; }
static inline int grp_n(const S& s, int p) {
  int A = s.alt[p], n = 0;
  for (int i = 0; i < 64; i++) n += s.alt[i] == A;
  if (s.alt[63 - p] == A && 63 - p != p) n--;
  return n;
}

static void gen_moves(const S& s, vector<Mv>& out) {
  out.clear();
  int p = s.pos, px = p % 8, py = p / 8, A = s.alt[p], O = 63 - p, n = grp_n(s, p);
  for (auto& d : ORTH) {
    for (int k = 1; k <= 2; k++) {
      int x = px + k * d[0], y = py + k * d[1]; if (!onb(x, y)) continue;
      int X = x + 8 * y; if (s.vis[X] >= 3 && X != H8) continue;
      int Y = s.alt[X]; if (Y == A) continue;
      int dz = k == 1 ? 2 : 1;
      for (int sg : {1, -1}) { int dl = A + sg * dz - Y; if (dl >= 0) out.push_back({n * dl, X}); }
    }
  }
  for (auto& d : KN) {
    int x = px + d[0], y = py + d[1]; if (!onb(x, y)) continue;
    int X = x + 8 * y; if (s.vis[X] >= 3 && X != H8) continue;
    if (s.alt[X] < A) out.push_back({n * (A - s.alt[X]), X});
  }
  // same-level knight jumps after waiting multiples of n
  int first[8], nf = 0;
  for (auto& d : KN) {
    int x = px + d[0], y = py + d[1]; if (!onb(x, y)) continue;
    int X = x + 8 * y; if ((s.vis[X] < 3 || X == H8) && s.alt[X] == A) first[nf++] = X;
  }
  if (!nf) return;
  uint64_t seen = 1ULL << p; int st[64], sp = 0; st[sp++] = p;
  int minY = INT32_MAX; bool any = false;
  while (sp) {
    int u = st[--sp], ux = u % 8, uy = u / 8;
    for (auto& d : KN) {
      int x = ux + d[0], y = uy + d[1]; if (!onb(x, y)) continue;
      int X2 = x + 8 * y;
      if (s.vis[X2] >= 3 || s.alt[X2] != A || (seen >> X2 & 1)) continue;
      seen |= 1ULL << X2; st[sp++] = X2;
      int x2 = X2 % 8, y2 = X2 / 8;
      for (auto& e : ORTH) for (int k = 1; k <= 2; k++) {
        int xx = x2 + k * e[0], yy = y2 + k * e[1]; if (!onb(xx, yy)) continue;
        int X = xx + 8 * yy; if (s.vis[X] >= 3) continue;
        if (A != s.alt[X] || X == O) { minY = min(minY, s.alt[X] - (k == 1 ? 2 : 1)); any = true; }
      }
      for (auto& e : KN) {
        int xx = x2 + e[0], yy = y2 + e[1]; if (!onb(xx, yy)) continue;
        int X = xx + 8 * yy; if (s.vis[X] >= 3) continue;
        minY = min(minY, (int)s.alt[X]); any = true;
      }
    }
  }
  if (!any) return;
  int maxT = (A - minY) * n;
  for (int t = 0; t <= maxT; t += n) for (int i = 0; i < nf; i++) out.push_back({t, first[i]});
}

// apply (t, q); false if the jump is not legal after the sink
static inline bool do_move(S& s, const Mv& m) {
  int p = s.pos, n = grp_n(s, p);
  if (m.t % n) return false;
  int dl = m.t / n;
  if (dl > 0) {
    int A = s.alt[p], O = 63 - p; bool rise = s.alt[O] != A;
    for (int i = 0; i < 64; i++) if (s.alt[i] == A && i != O) s.alt[i] -= dl;
    if (rise) s.alt[O] += dl;
    s.T += m.t;
  }
  int dx = abs(m.q % 8 - p % 8), dy = abs(m.q / 8 - p / 8), dz = abs(s.alt[m.q] - s.alt[p]);
  int a[3] = {dx, dy, dz}; std::sort(a, a + 3);
  if (!(a[0] == 0 && a[1] == 1 && a[2] == 2)) return false;
  s.vis[m.q]++; s.pos = (uint8_t)m.q;
  return true;
}

// ---------------------------------------------------------------- heuristic
// backward BFS from h8; reach = points that can still reach h8, rmask[i] = restricted set of point i
static uint64_t reach_end(const S& s, uint64_t* rmask) {
  uint64_t gm[64];  // same-altitude masks
  for (int i = 0; i < 64; i++) gm[i] = 0;
  for (int i = 0; i < 64; i++) { if (gm[i]) continue; uint64_t m = 0; for (int j = i; j < 64; j++) if (s.alt[j] == s.alt[i]) m |= 1ULL << j; for (int j = i; j < 64; j++) if (m >> j & 1) gm[j] = m; }
  uint64_t reach = 0, queued = 1ULL << H8;
  int q[64], qh = 0, qt = 0; q[qt++] = H8; rmask[H8] = (1ULL << H8) | 1ULL;
  while (qh < qt) {
    int u = q[qh++]; reach |= 1ULL << u;
    int ux = u % 8, uy = u / 8, A = s.alt[u]; uint64_t R = rmask[u];
    auto tryX = [&](int X, int exact, int lo) {  // exact: |dz| needed; lo: Y >= A - lo allowed by sinking X's group
      if ((queued >> X & 1) || s.vis[X] >= 3) return;
      int Y = s.alt[X];
      bool ok = (exact == 0 ? Y == A : abs(Y - A) == exact) || (!(R >> X & 1) && Y >= A - lo && (exact == 0 || Y != A));
      if (!ok) return;
      queued |= 1ULL << X; rmask[X] = R | gm[X] | (1ULL << (63 - X)); q[qt++] = X;
    };
    for (auto& d : ORTH) {
      int x = ux + d[0], y = uy + d[1]; if (onb(x, y)) tryX(x + 8 * y, 2, 2);
      x = ux + 2 * d[0]; y = uy + 2 * d[1]; if (onb(x, y)) tryX(x + 8 * y, 1, 1);
    }
    for (auto& d : KN) { int x = ux + d[0], y = uy + d[1]; if (onb(x, y)) tryX(x + 8 * y, 0, 0); }
  }
  return reach;
}

// notebook score / total visits
static float score_of(const S& s, vector<Mv>& mv) {
  uint64_t rm[64]; uint64_t reach = reach_end(s, rm);
  int distinct = 0, total = 0;
  for (int i = 0; i < 64; i++) { distinct += s.vis[i] > 0; total += s.vis[i]; }
  int best = 0;
  if (reach >> s.pos & 1) best = distinct;
  else {
    gen_moves(s, mv);
    for (auto& m : mv) if ((reach >> m.q & 1) && (m.t == 0 || !(rm[m.q] >> s.pos & 1))) best = max(best, distinct + (s.vis[m.q] == 0));
  }
  return total ? (float)best / total : 0.f;
}

// ---------------------------------------------------------------- necessity check (exact enough: doubles)
static bool legal_from(double* alt, int pos, uint8_t* vis, const vector<Mv>& mv, size_t i0, int overrideT) {
  double a[64]; memcpy(a, alt, sizeof a); uint8_t v[64]; memcpy(v, vis, 64); int p = pos;
  for (size_t j = i0; j < mv.size(); j++) {
    double t = j == i0 ? overrideT / 2.0 : mv[j].t;  // overrideT in half-minutes
    double A = a[p]; int O = 63 - p; int n = 0; bool oin = false;
    for (int k = 0; k < 64; k++) if (fabs(a[k] - A) < 1e-9) { if (k == O && k != p) oin = true; else n++; }
    if (t > 0) { double d = t / n; for (int k = 0; k < 64; k++) if (fabs(a[k] - A) < 1e-9 && !(k == O && oin)) a[k] -= d; if (!oin) a[O] += d; }
    int q = mv[j].q; double dz = fabs(a[q] - a[p]);
    int dx = abs(q % 8 - p % 8), dy = abs(q / 8 - p / 8);
    double b[3] = {(double)dx, (double)dy, dz}; std::sort(b, b + 3);
    if (fabs(b[0]) > 1e-9 || fabs(b[1] - 1) > 1e-9 || fabs(b[2] - 2) > 1e-9) return false;
    if (q != H8 && ++v[q] > 3) return false;
    p = q;
  }
  return true;
}
static bool all_waits_necessary(const vector<Mv>& mv) {
  double a[64]; uint8_t v[64]; int p = A1;
  for (int i = 0; i < 64; i++) { a[i] = ALT0[i]; v[i] = 0; } v[A1] = 1;
  for (size_t i = 0; i < mv.size(); i++) {
    if (mv[i].t > 0) {
      bool nec = !legal_from(a, p, v, mv, i, 0) || !legal_from(a, p, v, mv, i, 2 * mv[i].t + 1);
      if (!nec) return false;
    }
    // advance the prefix
    double A = a[p]; int O = 63 - p, n = 0; bool oin = false;
    for (int k = 0; k < 64; k++) if (fabs(a[k] - A) < 1e-9) { if (k == O && k != p) oin = true; else n++; }
    if (mv[i].t > 0) { double d = (double)mv[i].t / n; for (int k = 0; k < 64; k++) if (fabs(a[k] - A) < 1e-9 && !(k == O && oin)) a[k] -= d; if (!oin) a[O] += d; }
    if (mv[i].q != H8) v[mv[i].q]++;
    p = mv[i].q;
  }
  return true;
}

// ---------------------------------------------------------------- nodes, best tour
#pragma pack(push, 1)
struct Node { uint32_t parent; uint16_t t; uint8_t q; };
#pragma pack(pop)
static vector<Node> GN;  // global (BFS) nodes; node 0 = root
static vector<Mv> PREFIX;
static std::atomic<long long> BEST{-1};
static std::mutex BEST_M;
static string OUTF = "runs/coverage_best.txt";
static std::atomic<bool> STOP{false};
static std::atomic<long long> ST_EXP{0}, ST_FIN{0}, ST_REJ{0}, ST_GC{0}, ST_PRUNE{0};
static std::chrono::steady_clock::time_point T0;
static double elapsed() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count(); }

static string tour_text(const vector<Mv>& mv) {
  string o; char b[32];
  for (size_t i = 0; i < mv.size(); i++) { snprintf(b, sizeof b, "%s(%d, %s)", i ? ", " : "", mv[i].t, nm(mv[i].q).c_str()); o += b; }
  return o;
}
// path of a node (global or local table), then the finishing move
static void offer(const vector<Node>* local, uint32_t id, Mv fin, long long minutes) {
  if (minutes <= BEST.load()) return;
  vector<Mv> rev; rev.push_back(fin);
  for (uint32_t c = id; c != 0;) {
    const Node& nd = (c & LOCAL) ? (*local)[c & ~LOCAL] : GN[c];
    rev.push_back({nd.t, nd.q}); c = nd.parent;
  }
  vector<Mv> mv = PREFIX; mv.insert(mv.end(), rev.rbegin(), rev.rend());
  if (!all_waits_necessary(mv)) { ST_REJ++; return; }
  std::lock_guard<std::mutex> lk(BEST_M);
  if (minutes <= BEST.load()) return;
  BEST = minutes;
  FILE* f = fopen(OUTF.c_str(), "w"); if (f) { fprintf(f, "%lld\n%s\n", minutes, tour_text(mv).c_str()); fclose(f); }
  printf("[%7.0fs] NEW BEST %lld minutes (%zu moves)\n", elapsed(), minutes, mv.size()); fflush(stdout);
}

// ---------------------------------------------------------------- duplicate table
struct Dedup {
  vector<uint64_t> k; vector<int32_t> t; uint64_t mask = 0, cnt = 0;
  void init(int lg) { k.assign(1ULL << lg, 0); t.assign(1ULL << lg, 0); mask = (1ULL << lg) - 1; cnt = 0; }
  // true if new or better
  bool put(uint64_t h, int32_t T) {
    if (!h) h = 1;
    if (cnt * 10 > (mask + 1) * 7) { std::fill(k.begin(), k.end(), 0); cnt = 0; }
    for (uint64_t i = h & mask;; i = (i + 1) & mask) {
      if (k[i] == 0) { k[i] = h; t[i] = T; cnt++; return true; }
      if (k[i] == h) { if (T > t[i]) { t[i] = T; return true; } return false; }
    }
  }
};

// ---------------------------------------------------------------- per-thread best-first search
struct Entry { CS st; uint32_t node; float pr; };
struct Searcher {
  vector<vector<Entry>> B; int top = 0; size_t count = 0;
  vector<Node> L; Dedup dd; std::mt19937_64 rng;
  size_t openCap, nodeCap; double tau; int lmin, lmax;
  long long exp = 0;
  void push(const Entry& e) {
    int b = (int)(e.pr * SC); b = max(0, min(SC, b));
    B[b].push_back(e); count++; if (b > top) top = b;
  }
  bool pop(Entry& out) {
    while (top > 0 && B[top].empty()) top--;
    if (count == 0) return false;
    int l = std::uniform_int_distribution<int>(lmin, lmax)(rng);
    // the l best entries: walk buckets from the top
    static thread_local vector<pair<int, int>> cand; static thread_local vector<double> w;
    cand.clear(); w.clear();
    float prTop = -1;
    for (int b = top; b >= 0 && (int)cand.size() < l; b--)
      for (int j = (int)B[b].size() - 1; j >= 0 && (int)cand.size() < l; j--) {
        if (prTop < 0) prTop = B[b][j].pr;
        cand.push_back({b, j}); w.push_back(exp_w(B[b][j].pr - prTop));
      }
    std::discrete_distribution<int> dist(w.begin(), w.end());
    auto [b, j] = cand[dist(rng)];
    out = B[b][j]; B[b][j] = B[b].back(); B[b].pop_back(); count--;
    return true;
  }
  double exp_w(float d) const { return std::exp(d / tau); }
  void prune() {  // drop the lowest buckets until 80% of the cap
    size_t target = openCap * 8 / 10;
    for (int b = 0; b <= SC && count > target; b++) {
      size_t take = min(B[b].size(), count - target);
      B[b].resize(B[b].size() - take); count -= take;
      if (B[b].empty()) vector<Entry>().swap(B[b]);
    }
    ST_PRUNE++;
  }
  void gc() {  // keep local nodes that open entries depend on
    vector<uint8_t> mark(L.size(), 0);
    for (auto& bk : B) for (auto& e : bk) for (uint32_t c = e.node; (c & LOCAL) && c != NIL && !mark[c & ~LOCAL]; c = L[c & ~LOCAL].parent) mark[c & ~LOCAL] = 1;
    vector<uint32_t> nid(L.size(), NIL); uint32_t m = 0;
    for (size_t i = 0; i < L.size(); i++) if (mark[i]) nid[i] = m++;
    for (size_t i = 0; i < L.size(); i++) if (mark[i]) {
      Node nd = L[i]; if (nd.parent & LOCAL) nd.parent = LOCAL | nid[nd.parent & ~LOCAL];
      L[nid[i]] = nd;
    }
    L.resize(m);
    for (auto& bk : B) for (auto& e : bk) if (e.node & LOCAL) e.node = LOCAL | nid[e.node & ~LOCAL];
    ST_GC++;
  }
  void run() {
    vector<Mv> mv, mv2; S s, c;
    while (!STOP && pop_into(s)) {}
    (void)mv; (void)mv2; (void)c;
  }
  bool pop_into(S& s) {
    Entry e; if (!pop(e)) return false;
    unpack(e.st, s);
    static thread_local vector<Mv> mv, mv2;
    gen_moves(s, mv);
    for (auto& m : mv) {
      S c = s;
      if (!do_move(c, m)) continue;
      if (m.q == H8) { ST_FIN++; offer(&L, e.node, m, c.T); continue; }
      if (!dd.put(hashS(c, true), c.T)) continue;
      Entry ch; pack(c, ch.st); ch.pr = score_of(c, mv2);
      L.push_back({e.node, (uint16_t)m.t, (uint8_t)m.q}); ch.node = LOCAL | (uint32_t)(L.size() - 1);
      push(ch);
    }
    exp++; ST_EXP++;
    if (count > openCap) prune();
    if (L.size() > nodeCap) gc();
    return true;
  }
};

static void on_sigint(int) { STOP = true; }

int main(int argc, char** argv) {
  init_board();
  int TH = 16; long long SEED = 16000000; double TIME = 7200, TAU = 1; size_t OPENCAP = 2000000, NODECAP = 25000000;
  int LMIN = 5, LMAX = 150; unsigned RNG = 1; string prefix;
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { return string(argv[++i]); };
    if (a == "--threads") TH = stoi(nx()); else if (a == "--seed") SEED = (long long)stod(nx());
    else if (a == "--time") TIME = stod(nx()); else if (a == "--tau") TAU = stod(nx());
    else if (a == "--open-cap") OPENCAP = (size_t)stod(nx()); else if (a == "--node-cap") NODECAP = (size_t)stod(nx());
    else if (a == "--lmin") LMIN = stoi(nx()); else if (a == "--lmax") LMAX = stoi(nx());
    else if (a == "--out") OUTF = nx(); else if (a == "--prefix") prefix = nx(); else if (a == "--rng") RNG = (unsigned)stoul(nx());
    else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 1; }
  }
  signal(SIGINT, on_sigint);
  T0 = std::chrono::steady_clock::now();
  S root = start_state();
  if (!prefix.empty()) {  // "(t, sq), ..." in minutes
    const char* p = prefix.c_str(); int w; char sq[8];
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
      Mv m{w, (sq[0] - 'a') + 8 * (sq[1] - '1')};
      if (!do_move(root, m)) { fprintf(stderr, "illegal prefix move (%d, %s)\n", w, sq); return 1; }
      PREFIX.push_back(m); p++;
    }
  }
  printf("coverage search: %d threads, BFS seed %lld positions, %.0fs, open cap %zu/thread, tau %.2f, l in [%d, %d]\n",
         TH, SEED, TIME, OPENCAP, TAU, LMIN, LMAX); fflush(stdout);

  // ---------------- parallel BFS
  GN.push_back({NIL, 0, 0});
  vector<CS> layer(1); pack(root, layer[0]); vector<uint32_t> layerId{0};
  Dedup gdd; gdd.init(27); gdd.put(hashS(root, true), root.T);
  int depth = 0;
  while (!STOP && (long long)layer.size() < SEED) {
    struct Ch { CS st; uint32_t parent; uint16_t t; uint8_t q; uint64_t h; };
    vector<vector<Ch>> loc(TH);
    std::atomic<size_t> nextI{0};
    auto work = [&](int t) {
      vector<Mv> mv; S s;
      for (size_t i; (i = nextI.fetch_add(256)) < layer.size();)
        for (size_t j = i; j < min(layer.size(), i + 256); j++) {
          unpack(layer[j], s); gen_moves(s, mv);
          for (auto& m : mv) {
            S c = s; if (!do_move(c, m)) continue;
            if (m.q == H8) { ST_FIN++; offer(nullptr, layerId[j], m, c.T); continue; }
            Ch ch; pack(c, ch.st); ch.parent = layerId[j]; ch.t = (uint16_t)m.t; ch.q = (uint8_t)m.q; ch.h = hashS(c, true);
            loc[t].push_back(ch);
          }
        }
    };
    vector<std::thread> th; for (int t = 0; t < TH; t++) th.emplace_back(work, t); for (auto& x : th) x.join();
    vector<CS> nl; vector<uint32_t> nid;
    for (auto& v : loc) for (auto& ch : v) {
      if (!gdd.put(ch.h, ch.st.T)) continue;
      GN.push_back({ch.parent, ch.t, ch.q}); nl.push_back(ch.st); nid.push_back((uint32_t)(GN.size() - 1));
    }
    if (nl.empty()) break;
    layer.swap(nl); layerId.swap(nid); depth++;
    printf("[%7.0fs] BFS depth %d: %zu positions (best finished %lld)\n", elapsed(), depth, layer.size(), BEST.load()); fflush(stdout);
  }
  gdd = Dedup();

  // ---------------- score the frontier, deal it out by score
  vector<float> pr(layer.size());
  {
    std::atomic<size_t> nextI{0};
    auto work = [&]() { vector<Mv> mv; S s; for (size_t i; (i = nextI.fetch_add(1024)) < layer.size();) for (size_t j = i; j < min(layer.size(), i + 1024); j++) { unpack(layer[j], s); pr[j] = score_of(s, mv); } };
    vector<std::thread> th; for (int t = 0; t < TH; t++) th.emplace_back(work); for (auto& x : th) x.join();
  }
  vector<uint32_t> order(layer.size()); std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return pr[a] > pr[b]; });
  printf("[%7.0fs] frontier scored: %zu positions, best score %.3f, median %.3f\n", elapsed(), layer.size(),
         order.empty() ? 0.f : pr[order[0]], order.empty() ? 0.f : pr[order[order.size() / 2]]); fflush(stdout);
  vector<Searcher> W(TH);
  for (int t = 0; t < TH; t++) {
    W[t].B.assign(SC + 1, {}); W[t].rng.seed(RNG * 1000003ULL + t); W[t].openCap = OPENCAP; W[t].nodeCap = NODECAP;
    W[t].tau = TAU; W[t].lmin = LMIN; W[t].lmax = LMAX; W[t].dd.init(23);
  }
  for (size_t r = 0; r < order.size(); r++) { uint32_t j = order[r]; W[r % TH].push({layer[j], layerId[j], pr[j]}); }
  vector<CS>().swap(layer); vector<uint32_t>().swap(layerId);

  // ---------------- heuristic-guided search
  vector<std::thread> th; for (int t = 0; t < TH; t++) th.emplace_back([&, t]() { W[t].run(); });
  long long lastE = 0; double lastT = elapsed();
  while (!STOP && elapsed() < TIME) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (elapsed() - lastT >= 30) {
      size_t open = 0, nodes = 0; for (auto& w : W) { open += w.count; nodes += w.L.size(); }
      long long e = ST_EXP.load();
      printf("[%7.0fs] %.1fk exp/s | open %.1fM | nodes %.1fM | finishes %lld (rejected %lld) | best %lld | gc %lld prune %lld\n",
             elapsed(), (e - lastE) / (elapsed() - lastT) / 1e3, open / 1e6, nodes / 1e6, ST_FIN.load(), ST_REJ.load(), BEST.load(),
             ST_GC.load(), ST_PRUNE.load()); fflush(stdout);
      lastE = e; lastT = elapsed();
    }
  }
  STOP = true;
  for (auto& x : th) x.join();
  printf("done after %.0fs: %lld expansions, best %lld (%s)\n", elapsed(), ST_EXP.load(), BEST.load(), OUTF.c_str());
}
