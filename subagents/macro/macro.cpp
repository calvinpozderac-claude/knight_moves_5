// Macro-move search for the swamp-knight puzzle (Knight Moves 5).
//
// A macro move = one sit (a pinned move with a positive wait) followed by the zero-wait hops to the
// next sitting square r. For each sit (delta, q) the hops are a minimum-cost path from q to r over
// zero-wait jumps (Dijkstra, cost of entering v = 1 + --use-w if v is "useful", i.e. still within 2
// units of the knight's altitude after the sit; paths never enter h8 and respect the 3-visit cap).
// The search state is "knight on a sitting square, about to sit"; at the root (only) a no-sit macro
// (pure repositioning) is also offered. Every state checks every chance to finish: pinned moves into
// h8 from the knight's square, and from every square the hops can reach (zero-wait or after a sit).
// A sit after which h8 looks unreachable is pruned (--no-h8-prune turns that off).
//
// Algorithms (--algo):
//   nrpa  (default) Nested Rollout Policy Adaptation over macros. A macro is chosen in two stages:
//         first the sit (code: from, to, delta, visits of target), then the endpoint r among the
//         min-cost endpoints (code: from, to, delta, r, visits of r). Priors: --sit-bias B adds
//         B x minutes/100 to a sit, --hop-pen H subtracts H x path cost from an endpoint.
//         After each restart, the last --tail K macros of its result are re-searched by the DFS below.
//         --seed-adapt N shifts the fresh weights N times toward the seed's own continuation (as macros);
//         --warm also starts the search with it as the best so far (local search around the seed).
//   beam  beam search over macros, states ranked by T + an idealised-dance estimate (pot), deduplicated.
//   dfs   depth-first branch and bound over the last K macros (K from --tail-min..--tail-max) of a seed
//         tour: prune when T + pot + --slack <= best; transposition table; --dfs-nodes budget per run.
//         With an admissible pot this is exact over the macro space; pot is the idealised dance of
//         nrpa.cpp's dance_pot (alternate sits 4 x group size; ignores adjacency and hop visits), which
//         is optimistic inside a two-group dance (outside a dance no pruning is done). --slack adds margin.
// Start: from a1 (--anchor-min 0) or after k raw moves of a seed tour (k from --anchor-min..--anchor-max).
// Output: --out FILE (line 1 minutes, line 2 tour) on every new best; --pool FILE gets every finished
// tour >= --pool-min as "minutes<TAB>tour"; a tour > --record that the exact checker accepts also goes to
// NEW_RECORD_<minutes>.txt next to --out. Stops on --time or SIGINT/SIGTERM.
//
// build: g++ -O3 -march=native -std=c++17 -pthread -o macro macro.cpp
// see RUN.md for commands.
#include "../../src/eng.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <thread>
#include <unordered_map>

struct Mv { int delta, q; };
struct Macro { int16_t d; int8_t q; int8_t r; };  // q = -1: no sit (root only), then hops to r

static int MAXHOPS = 4, USEW = 2, PB = 20, ITERS = 100;
static double HOPPEN = 3.0, SITBIAS = 0.0, ALPHA = 1.0, D0 = 1;
static bool PRUNE_H8 = true;
static std::atomic<bool> STOP{false};
static std::atomic<long long> PLAYOUTS{0}, DFSNODES{0};

// ---------------------------------------------------------------- min-cost zero-wait paths
struct Reach {
  int n; int8_t order[64];        // settled squares in order (order[0] = src)
  int16_t cost[64]; int8_t hops[64]; int8_t par[64];
};
static void reach_from(const S& c, int src, Reach& R) {
  static thread_local vector<int> heap; heap.clear();
  bool done[64];
  for (int i = 0; i < 64; i++) { R.cost[i] = 30000; R.hops[i] = 0; R.par[i] = -1; done[i] = false; }
  int az = c.alt[src];
  auto key = [](int cost, int hops, int sq) { return (cost << 12) | (hops << 6) | sq; };
  R.cost[src] = 0; R.n = 0;
  heap.push_back(key(0, 0, src));
  while (!heap.empty()) {
    std::pop_heap(heap.begin(), heap.end(), std::greater<int>()); int k = heap.back(); heap.pop_back();
    int u = k & 63; if (done[u]) continue; done[u] = true; R.order[R.n++] = u;
    if (R.hops[u] > MAXHOPS) continue;  // paths longer than max-hops + 1 are never needed
    for (int j = 0; j < NBN[u]; j++) {
      int v = NBQ[u][j];
      if (v == H8 || done[v] || c.vis[v] >= 3) continue;
      if (!legal_dz(NBT[u][j], c.alt[v] - c.alt[u])) continue;
      int nc = R.cost[u] + 1 + (abs(c.alt[v] - az) <= 2 ? USEW : 0), nh = R.hops[u] + 1;
      if (key(nc, nh, v) < key(R.cost[v], R.hops[v], v)) {
        R.cost[v] = nc; R.hops[v] = nh; R.par[v] = u;
        heap.push_back(key(nc, nh, v)); std::push_heap(heap.begin(), heap.end(), std::greater<int>());
      }
    }
  }
}
// hops from the Reach source to r, applied to c (appended to out if given)
static void apply_hops(S& c, const Reach& R, int r, vector<Mv>* out) {
  int path[64], n = 0;
  for (int v = r; R.par[v] >= 0; v = R.par[v]) path[n++] = v;
  for (int i = n - 1; i >= 0; i--) { apply_move(c, 0, path[i]); if (out) out->push_back({0, path[i]}); }
}

// pinned finishing moves (sink d >= 0 at v, then jump into h8), v = any square (not necessarily c.pos)
static int H8K[64];  // neighbour type of v to h8, -1 if not a neighbour
static inline int fin_moves(const S& c, int v, int out[4]) {
  int t = H8K[v]; if (t < 0) return 0;
  int a = c.alt[v], diff = c.alt[H8] - a, n = 0;
  if (c.alt[H8] == a) { if (t == 0) out[n++] = 0; return n; }
  if (t == 0) { if (-diff >= 0) out[n++] = -diff; }
  else if (t == 1) { for (int dz : {2, -2}) if (dz - diff >= 0) out[n++] = dz - diff; }
  else { for (int dz : {1, -1}) if (dz - diff >= 0) out[n++] = dz - diff; }
  return n;
}

// after sinking d at s.pos and jumping to q: can h8 still be reached? (cheap necessary-ish test: some
// neighbour of h8 with visits left is high enough, or h8's group still has a square to sit on)
static bool h8_alive_after(const S& s, int d, int q) {
  int p = s.pos, a = s.alt[p], O = 63 - p; bool rise = s.alt[O] != a;
  auto alt2 = [&](int i) { int x = s.alt[i]; if (d > 0) { if (x == a && i != O) x -= d; else if (i == O && rise) x += d; } return x; };
  int ah = alt2(H8);
  for (int k = 0; k < NBN[H8]; k++) {
    int v = NBQ[H8][k], t = NBT[H8][k];
    if (s.vis[v] + (v == q) >= 3 && v != q) continue;
    int up = t == 0 ? 0 : t == 1 ? 2 : 1;
    if (alt2(v) + up >= ah) return true;
  }
  for (int i = 1; i < 63; i++) if (alt2(i) == ah && (s.vis[i] + (i == q) < 3 || i == q)) return true;
  return false;
}

// ---------------------------------------------------------------- idealised dance estimate
// nrpa.cpp's dance_pot, future part only: knight in the upper of two groups 2 apart (h8 in one of them);
// sits alternate between the groups, each 4 x group size; squares whose partner is gone sit first.
// Returns -1 if s is not a dance state.
static int POT_DMIN = 0;
static double pot_future(const S& s) {
  int a = s.alt[s.pos], nG[2] = {0, 0};
  for (int i = 0; i < 64; i++) { if (s.alt[i] == a) nG[0]++; else if (s.alt[i] == a - 2) nG[1]++; }
  if (nG[1] == 0) return -1;
  int ah = s.alt[H8]; if (ah != a && ah != a - 2) return -1;
  if (nG[0] + nG[1] < POT_DMIN) return -1;
  int grp[64], cap[64]; bool alive[64], open_[64];
  for (int i = 0; i < 64; i++) {
    alive[i] = s.alt[i] == a || s.alt[i] == a - 2; grp[i] = s.alt[i] == a ? 0 : 1;
    cap[i] = alive[i] ? 3 - s.vis[i] + (i == s.pos) : 0;
  }
  cap[H8] = cap[A1] = 0;
  for (int i = 0; i < 64; i++) open_[i] = alive[i] && !alive[63 - i];
  double fut = 0; int g = 0;
  for (int step = 0; step < 300; step++) {
    int best = -1, bc = 0;
    if (step == 0) best = s.pos;
    else {
      for (int i = 0; i < 64; i++) if (alive[i] && grp[i] == g && open_[i] && cap[i] > bc) { best = i; bc = cap[i]; }
      if (best < 0) for (int i = 0; i < 64; i++) if (alive[i] && grp[i] == g && cap[i] > bc) { best = i; bc = cap[i]; }
    }
    if (best < 0 || cap[best] <= 0) break;
    int n = nG[g], p = 63 - best;
    if (!open_[best]) {
      if (alive[p]) { if (grp[p] == g) n--; nG[grp[p]]--; alive[p] = false; cap[p] = 0; }
      open_[best] = true;
    }
    fut += 4.0 * n; cap[best]--; g ^= 1;
  }
  return fut + 4.0 * max(nG[0], nG[1]);  // + the last sit into h8
}

// ---------------------------------------------------------------- policy
struct Policy { vector<float> w; Policy() : w(size_t(1) << PB, 0.f) {} };
static inline uint32_t hcode(uint64_t key) { return (uint32_t)((key * 0x9E3779B97F4A7C15ULL) >> (64 - PB)); }
static inline uint32_t sit_code(const S& s, int d, int q) {
  return hcode(((((1ull * 64 + s.pos) * 65 + (q + 1)) * 64 + min(d, 63)) * 4) + (q >= 0 ? min<int>(s.vis[q], 3) : 0));
}
static inline uint32_t end_code(int p, int q, int d, int r, int visr) {
  return hcode((((((2ull * 64 + p) * 65 + (q + 1)) * 64 + min(d, 63)) * 64 + r) * 4) + min(visr, 3));
}

struct Fin { long long T = -1; int len = 0; bool extra = false; Macro m{0, 0, 0}; int16_t d = 0; };
struct Result { double score = -1e18; long long minutes = 0; bool finished = false; vector<Macro> mac; int16_t finD = 0; };

static double h8dist(const S& s) {
  int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  return sqrt(dx * dx + dy * dy + dz * dz) / D0;
}

// sit options at s (two-stage policy, stage 1). Records pinned finishes from s.pos into fin.
struct SitOpt { int d, q, minutes; double e; uint32_t code; };
static void sit_options(const S& s, bool first, const Policy& pol, vector<SitOpt>& out, Fin* fin, int len) {
  static thread_local vector<pair<int, int>> mv;
  out.clear(); pinned_moves(s, 3, true, mv);
  int n = group_n(s, s.pos);
  for (auto& [d, q] : mv) {
    if (q == H8) { long long T = s.T + (long long)d * n; if (fin && T >= fin->T) { fin->T = T; fin->len = len; fin->extra = false; fin->d = d; } continue; }
    if (d == 0) continue;
    if (PRUNE_H8 && !h8_alive_after(s, d, q)) continue;
    uint32_t c = sit_code(s, d, q); int m = d * n;
    out.push_back({d, q, m, exp(pol.w[c] + SITBIAS * m / 100.0), c});
  }
  if (first) { uint32_t c = sit_code(s, 0, -1); out.push_back({0, -1, 0, exp(pol.w[c]), c}); }
}
// endpoint options after the sit (stage 2). Records finishes from reached squares into fin.
struct EndOpt { int r; double e; uint32_t code; };
static void end_options(const S& s0, const S& c, int d, int q, const Reach& R, const Policy& pol, vector<EndOpt>& out, Fin* fin, int len) {
  out.clear();
  int src = c.pos;
  for (int i = 0; i < R.n; i++) {
    int v = R.order[i];
    if (fin && H8K[v] >= 0) {
      int fm[4]; int k = fin_moves(c, v, fm);
      if (k) { int n = group_n(c, v);
        for (int j = 0; j < k; j++) { long long T = c.T + (long long)fm[j] * n;
          if (T >= fin->T) { fin->T = T; fin->len = len; fin->extra = true; fin->m = {(int16_t)d, (int8_t)q, (int8_t)v}; fin->d = fm[j]; } } }
    }
    if (R.hops[v] > MAXHOPS) continue;
    if (q < 0 && v == src) continue;  // no-sit macro must move
    uint32_t cd = end_code(s0.pos, q, d, v, c.vis[v]);
    out.push_back({v, exp(pol.w[cd] - HOPPEN * R.cost[v]), cd});
  }
}

static inline S after_sit(const S& s, int d, int q) { S c = s; if (q >= 0) apply_move(c, d, q); return c; }

// apply one macro to s (raw moves appended to out if given)
static void apply_macro(S& s, const Macro& m, vector<Mv>* out) {
  if (m.q >= 0) { apply_move(s, m.d, m.q); if (out) out->push_back({m.d, m.q}); }
  static thread_local Reach R; reach_from(s, s.pos, R);
  apply_hops(s, R, m.r, out);
}

template <class T> static int sample(const vector<T>& v, std::mt19937& rng) {
  double z = 0; for (auto& o : v) z += o.e;
  double u = std::uniform_real_distribution<double>(0, z)(rng), acc = 0;
  for (size_t i = 0; i < v.size(); i++) { acc += v[i].e; if (acc >= u) return (int)i; }
  return (int)v.size() - 1;
}

static Result finish_result(const S& last, const vector<Macro>& seq, const Fin& fin) {
  Result r;
  if (fin.T >= 0) {
    r.mac.assign(seq.begin(), seq.begin() + fin.len); if (fin.extra) r.mac.push_back(fin.m);
    r.finD = fin.d; r.finished = true; r.minutes = fin.T; r.score = (double)fin.T;
  } else { r.mac = seq; r.minutes = last.T; r.score = -1e7 + last.T * exp(-h8dist(last)); }
  return r;
}

static Result playout(const S& root, const Policy& pol, std::mt19937& rng) {
  static thread_local vector<SitOpt> so; static thread_local vector<EndOpt> eo; static thread_local Reach R;
  S s = root; vector<Macro> seq; Fin fin; bool first = true;
  while (true) {
    sit_options(s, first, pol, so, &fin, (int)seq.size());
    if (so.empty()) break;
    const SitOpt& o = so[sample(so, rng)];
    S c = after_sit(s, o.d, o.q);
    reach_from(c, c.pos, R);
    end_options(s, c, o.d, o.q, R, pol, eo, &fin, (int)seq.size());
    if (eo.empty()) { if (o.q < 0) { first = false; continue; } break; }
    int r = eo[sample(eo, rng)].r;
    apply_hops(c, R, r, nullptr);
    seq.push_back({(int16_t)o.d, (int8_t)o.q, (int8_t)r});
    s = c; first = false;
  }
  PLAYOUTS++;
  return finish_result(s, seq, fin);
}

static void adapt(Policy& pol, const S& root, const vector<Macro>& seq, double alpha) {
  static thread_local vector<pair<uint32_t, float>> upd; upd.clear();
  static thread_local vector<SitOpt> so; static thread_local vector<EndOpt> eo; static thread_local Reach R;
  S s = root; bool first = true;
  for (const Macro& m : seq) {
    sit_options(s, first, pol, so, nullptr, 0);
    int ci = -1; double z = 0;
    for (size_t i = 0; i < so.size(); i++) { z += so[i].e; if (so[i].d == m.d && so[i].q == m.q) ci = (int)i; }
    if (ci >= 0) { for (auto& o : so) upd.push_back({o.code, (float)(-alpha * o.e / z)}); upd.push_back({so[ci].code, (float)alpha}); }
    S c = after_sit(s, m.d, m.q);
    reach_from(c, c.pos, R);
    end_options(s, c, m.d, m.q, R, pol, eo, nullptr, 0);
    ci = -1; z = 0;
    for (size_t i = 0; i < eo.size(); i++) { z += eo[i].e; if (eo[i].r == m.r) ci = (int)i; }
    if (ci >= 0) { for (auto& o : eo) upd.push_back({o.code, (float)(-alpha * o.e / z)}); upd.push_back({eo[ci].code, (float)alpha}); }
    apply_hops(c, R, m.r, nullptr);
    s = c; first = false;
  }
  for (auto& [c, d] : upd) pol.w[c] += d;
}

static Result nrpa(int level, const S& root, Policy pol, std::mt19937& rng, const Result* init = nullptr) {
  if (level == 0) return playout(root, pol, rng);
  Result best; if (init) best = *init;
  for (int i = 0; i < ITERS && !STOP; i++) {
    Result r = level == 1 ? playout(root, pol, rng) : nrpa(level - 1, root, pol, rng);
    if (r.score >= best.score) best = r;
    adapt(pol, root, best.mac, ALPHA);
  }
  return best;
}

// ---------------------------------------------------------------- full macro enumeration (beam, dfs)
struct Child { S s; Macro m; double bound; };
static void enum_children(const S& s, bool first, vector<Child>& out, Fin& fin, int len, int maxhops) {
  static thread_local vector<pair<int, int>> mv; static thread_local Reach R;
  out.clear(); pinned_moves(s, 3, true, mv);
  int n = group_n(s, s.pos);
  vector<pair<int, int>> sits;
  for (auto& [d, q] : mv) {
    if (q == H8) { long long T = s.T + (long long)d * n; if (T > fin.T) { fin.T = T; fin.len = len; fin.extra = false; fin.d = d; } continue; }
    if (d == 0) continue;
    if (PRUNE_H8 && !h8_alive_after(s, d, q)) continue;
    sits.push_back({d, q});
  }
  if (first) sits.push_back({0, -1});
  for (auto& [d, q] : sits) {
    S c = after_sit(s, d, q);
    reach_from(c, c.pos, R);
    for (int i = 0; i < R.n; i++) {
      int v = R.order[i];
      if (H8K[v] >= 0) {
        int fm[4]; int k = fin_moves(c, v, fm);
        if (k) { int nv = group_n(c, v);
          for (int j = 0; j < k; j++) { long long T = c.T + (long long)fm[j] * nv;
            if (T > fin.T) { fin.T = T; fin.len = len; fin.extra = true; fin.m = {(int16_t)d, (int8_t)q, (int8_t)v}; fin.d = fm[j]; } } }
      }
      if (R.hops[v] > maxhops) continue;
      if (q < 0 && v == c.pos) continue;
      Child ch; ch.s = c; apply_hops(ch.s, R, v, nullptr); ch.m = {(int16_t)d, (int8_t)q, (int8_t)v};
      double p = pot_future(ch.s); ch.bound = p < 0 ? 1e12 : ch.s.T + p;
      out.push_back(ch);
    }
  }
}

// DFS branch and bound from root; best starts at `bestT` (only improvements are recorded)
struct DfsCtx {
  long long bestT; vector<Macro> bestMac; int16_t bestD = 0; bool found = false;
  long long nodes = 0, budget; int maxDepth; double slack;
  std::unordered_map<uint64_t, int> tt; vector<Macro> path;
};
static void dfs(const S& s, DfsCtx& C, int depth, bool first) {
  if (STOP || C.nodes >= C.budget) return;
  C.nodes++; DFSNODES++;
  {
    auto it = C.tt.find(hashS(s)); if (it != C.tt.end() && it->second >= s.T) return;
    if (C.tt.size() < 20000000) C.tt[hashS(s)] = s.T;
  }
  Fin fin; fin.T = C.bestT;
  vector<Child> ch; enum_children(s, first, ch, fin, (int)C.path.size(), MAXHOPS);
  if (fin.T > C.bestT) {
    C.bestT = fin.T; C.found = true; C.bestD = fin.d;
    C.bestMac = C.path; if (fin.extra) C.bestMac.push_back(fin.m);
  }
  if (depth >= C.maxDepth) return;
  std::sort(ch.begin(), ch.end(), [](const Child& a, const Child& b) { return a.bound > b.bound; });
  for (auto& c : ch) {
    if (c.bound + C.slack <= C.bestT) break;
    C.path.push_back(c.m); dfs(c.s, C, depth + 1, false); C.path.pop_back();
    if (STOP || C.nodes >= C.budget) return;
  }
}

// beam search from root; returns best finished result
static Result beam(const S& root, int width, double noise, std::mt19937& rng, int maxhops) {
  struct Node { S s; int parent; Macro m; };
  struct Cand { double score; int parent; Macro m; uint64_t h; };
  vector<vector<pair<int, Macro>>> hist;  // per level: (parent index, macro)
  vector<Node> cur{{root, -1, {0, 0, 0}}}; hist.push_back({{-1, {0, 0, 0}}});
  Result best; Fin bf; int bfLevel = -1, bfNode = -1;
  std::normal_distribution<double> nd(0, 1);
  for (int level = 0; !cur.empty() && !STOP; level++) {
    vector<Cand> cand; vector<Child> ch;
    for (int i = 0; i < (int)cur.size(); i++) {
      Fin fin; fin.T = bf.T;
      enum_children(cur[i].s, level == 0, ch, fin, 0, maxhops);
      if (fin.T > bf.T) { bf = fin; bfLevel = level; bfNode = i; }
      for (auto& c : ch) {
        double p = pot_future(c.s);
        double sc = (p < 0 ? c.s.T - 1e5 : c.s.T + p) + noise * nd(rng);
        cand.push_back({sc, i, c.m, hashS(c.s)});
      }
    }
    if (cand.empty()) break;
    std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });
    std::unordered_set<uint64_t> seen; vector<Node> nxt; vector<pair<int, Macro>> h;
    for (auto& c : cand) {
      if ((int)nxt.size() >= width) break;
      if (!seen.insert(c.h).second) continue;
      Node n; n.s = cur[c.parent].s; apply_macro(n.s, c.m, nullptr); n.parent = c.parent; n.m = c.m;
      nxt.push_back(n); h.push_back({c.parent, c.m});
    }
    hist.push_back(h); cur.swap(nxt);
    if (getenv("BEAM_TRACE")) printf("  level %d: %zu cands, beam %zu, top score %.0f T %d, best finish %lld\n", level, cand.size(), cur.size(), cand[0].score, cur.empty() ? 0 : cur[0].s.T, bf.T);
  }
  if (bfLevel < 0) return best;
  vector<Macro> mac; int idx = bfNode;
  for (int L = bfLevel; L >= 1; L--) { mac.push_back(hist[L][idx].second); idx = hist[L][idx].first; }
  std::reverse(mac.begin(), mac.end());
  if (bf.extra) mac.push_back(bf.m);
  best.mac = mac; best.finD = bf.d; best.finished = true; best.minutes = bf.T; best.score = (double)bf.T;
  return best;
}

// ---------------------------------------------------------------- tours (raw moves)
static vector<Mv> expand(const S& root, const vector<Macro>& mac, bool finished, int finD) {
  vector<Mv> out; S s = root;
  for (const Macro& m : mac) apply_macro(s, m, &out);
  if (finished) out.push_back({finD, H8});
  return out;
}
// exact replay with every rule of the engine; returns minutes, or -1 if illegal / not finished at h8
static long long validate(const vector<Mv>& t) {
  S s = start_state();
  for (size_t i = 0; i < t.size(); i++) {
    int p = s.pos, q = t[i].q, ty = -1;
    for (int k = 0; k < NBN[p]; k++) if (NBQ[p][k] == q) ty = NBT[p][k];
    if (ty < 0 || t[i].delta < 0) return -1;
    if (q == H8 && i + 1 != t.size()) return -1;
    if (q != H8 && s.vis[q] >= 3) return -1;
    S c = s; apply_move(c, t[i].delta, q);  // apply_move sinks then moves; check legality at the sunk state
    int altp = (t[i].delta > 0) ? c.alt[p] : s.alt[p];
    if (!legal_dz(ty, c.alt[q] - altp)) return -1;
    if (s.alt[q] == s.alt[p] && q != 63 - p && t[i].delta > 0) return -1;  // q would sink too: not pinned
    s = c;
  }
  return (!t.empty() && s.pos == H8) ? s.T : -1;
}
static string tour_text(const vector<Mv>& seq) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < seq.size(); i++) {
    int w = apply_move(s, seq[i].delta, seq[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(seq[i].q).c_str()); out += buf;
  }
  return out;
}
static vector<vector<Mv>> read_tours(const char* fn) {
  vector<vector<Mv>> out; FILE* f = fopen(fn, "r"); if (!f) { fprintf(stderr, "cannot open %s\n", fn); exit(1); }
  static char line[400000];
  while (fgets(line, sizeof line, f)) {
    vector<Mv> t; S s = start_state(); char* p = line; int w; char sq[8];
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
      t.push_back({d, q}); apply_move(s, d, q); p++;
    }
    if (t.size() > 1 && t.back().q == H8 && validate(t) >= 0) out.push_back(t);
  }
  fclose(f); return out;
}
static S replay(const vector<Mv>& t, size_t k) { S s = start_state(); for (size_t i = 0; i < k && i < t.size(); i++) apply_move(s, t[i].delta, t[i].q); return s; }
// indices of the sits (positive waits, not the final jump into h8)
static vector<int> sit_indices(const vector<Mv>& t) { vector<int> v; for (int i = 0; i + 1 < (int)t.size(); i++) if (t[i].delta > 0) v.push_back(i); return v; }

// a raw tour's moves from `start` as macros (leading zero-wait hops become a no-sit macro); the final
// jump into h8 is not a macro (its sink is t.back().delta). Rebuilding needs our min-cost hop paths to
// coincide with the tour's (check with expand + validate).
static vector<Macro> decompose(const vector<Mv>& t, size_t start) {
  vector<Macro> all; size_t i = start;
  while (i + 1 < t.size() && t[i].delta == 0) i++;
  if (i > start) all.push_back({0, -1, (int8_t)t[i - 1].q});
  while (i + 1 < t.size()) {
    size_t j = i + 1; while (j + 1 < t.size() && t[j].delta == 0) j++;
    all.push_back({(int16_t)t[i].delta, (int8_t)t[i].q, (int8_t)t[j - 1].q});
    i = j;
  }
  return all;
}

// ---------------------------------------------------------------- output
static std::mutex MTX;
static long long GBEST = 0; static vector<Mv> GBESTT;
static string OUTF = "macro_best.txt", POOLF = "macro_pool.txt", CHECKER = "tools/check_tour.py";
static long long POOLMIN = 7500, RECORD = 8123;
static auto T0 = std::chrono::steady_clock::now();
static double elapsed() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count(); }

static bool exact_check(const string& tour) {
  string cmd = "python3 " + CHECKER + " \"" + tour + "\" 2>&1";
  FILE* p = popen(cmd.c_str(), "r"); if (!p) return false;
  char buf[4096]; string o; while (fgets(buf, sizeof buf, p)) o += buf; pclose(p);
  printf("  checker: %s", o.c_str());
  return o.find("ok=True") != string::npos && o.find("unnecessary=[]") != string::npos && o.find("maxvisits=4") == string::npos;
}
// report a full raw tour; returns its minutes (-1 if invalid)
static long long report(const vector<Mv>& t, const char* tag) {
  long long T = validate(t);
  std::lock_guard<std::mutex> lk(MTX);
  if (T < 0) { printf("[%7.0fs] %s: INVALID tour produced (bug), skipped\n", elapsed(), tag); return -1; }
  string txt = tour_text(t);
  static std::unordered_set<size_t> pooled;  // one line per distinct tour
  if (!POOLF.empty() && T >= POOLMIN && pooled.insert(std::hash<string>()(txt)).second) { FILE* f = fopen(POOLF.c_str(), "a"); if (f) { fprintf(f, "%lld\t%s\n", T, txt.c_str()); fclose(f); } }
  if (T > GBEST) {
    GBEST = T; GBESTT = t;
    FILE* f = fopen(OUTF.c_str(), "w"); if (f) { fprintf(f, "%lld\n%s\n", T, txt.c_str()); fclose(f); }
    printf("[%7.0fs] NEW BEST %lld minutes (%zu jumps) from %s -> %s\n", elapsed(), T, t.size(), tag, OUTF.c_str());
    if (T > RECORD && exact_check(txt)) {
      string dir = OUTF.find('/') == string::npos ? "" : OUTF.substr(0, OUTF.rfind('/') + 1);
      string fn = dir + "NEW_RECORD_" + to_string(T) + ".txt";
      FILE* g = fopen(fn.c_str(), "w"); if (g) { fprintf(g, "%lld\n%s\n", T, txt.c_str()); fclose(g); }
      printf("  *** NEW RECORD %lld verified -> %s\n", T, fn.c_str());
    }
  }
  fflush(stdout);
  return T;
}

// re-search the last K macros of a raw tour with the DFS; reports an improvement
static long long POLISH_NODES = 200000; static int POLISH_DEPTH = 40; static double SLACK = 0;
static long long polish_tail(const vector<Mv>& t, int K, long long budget, const char* tag, long long* nodesOut = nullptr) {
  vector<int> si = sit_indices(t);
  if ((int)si.size() < K) K = (int)si.size();
  if (K <= 0) return -1;
  {  // skip a (tour, K) pair that was already polished
    static std::mutex m; static std::unordered_set<size_t> done; std::lock_guard<std::mutex> lk(m);
    if (!done.insert(std::hash<string>()(tour_text(t)) * 31 + K).second) { if (nodesOut) *nodesOut = 0; return -1; }
  }
  int j = si[si.size() - K];
  S root = replay(t, j);
  DfsCtx C; C.bestT = validate(t); C.budget = budget; C.maxDepth = POLISH_DEPTH; C.slack = SLACK;
  dfs(root, C, 0, true);
  if (nodesOut) *nodesOut = C.nodes;
  if (!C.found) return -1;
  vector<Mv> full(t.begin(), t.begin() + j), tail = expand(root, C.bestMac, true, C.bestD);
  full.insert(full.end(), tail.begin(), tail.end());
  char buf[128]; snprintf(buf, sizeof buf, "%s+dfs(K=%d,%lld nodes)", tag, K, C.nodes);
  return report(full, buf);
}

static void on_signal(int) { STOP = true; }

int main(int argc, char** argv) {
  init_board();
  for (int v = 0; v < 64; v++) H8K[v] = -1;
  for (int k = 0; k < NBN[H8]; k++) H8K[NBQ[H8][k]] = NBT[H8][k];
  D0 = 1; D0 = h8dist(start_state());
  string algo = "nrpa", seedFile; int LEVEL = 3, THREADS = 1, AMIN = 0, AMAX = 0, TAIL = 15, TMIN = 10, TMAX = 20, WIDTH = 2000;
  int SEEDADAPT = 0; bool WARM = false;
  double TIME = 0, SEEDBEST = 0, NOISE = 0; unsigned RNG = 1; long long DFSN = 5000000; bool SELFCHECK = false;
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--algo") algo = nx(); else if (a == "--level") LEVEL = stoi(nx()); else if (a == "--iters") ITERS = stoi(nx());
    else if (a == "--alpha") ALPHA = stod(nx()); else if (a == "--threads") THREADS = stoi(nx()); else if (a == "--time") TIME = stod(nx());
    else if (a == "--rng") RNG = (unsigned)stoul(nx()); else if (a == "--seed-tours") seedFile = nx(); else if (a == "--seed-best") SEEDBEST = stod(nx());
    else if (a == "--anchor-min") AMIN = stoi(nx()); else if (a == "--anchor-max") AMAX = stoi(nx());
    else if (a == "--max-hops") MAXHOPS = stoi(nx()); else if (a == "--use-w") USEW = stoi(nx());
    else if (a == "--hop-pen") HOPPEN = stod(nx()); else if (a == "--sit-bias") SITBIAS = stod(nx());
    else if (a == "--no-h8-prune") PRUNE_H8 = false; else if (a == "--pbits") PB = stoi(nx());
    else if (a == "--tail") TAIL = stoi(nx()); else if (a == "--polish-nodes") POLISH_NODES = stoll(nx());
    else if (a == "--tail-min") TMIN = stoi(nx()); else if (a == "--tail-max") TMAX = stoi(nx());
    else if (a == "--dfs-nodes") DFSN = stoll(nx()); else if (a == "--dfs-depth") POLISH_DEPTH = stoi(nx());
    else if (a == "--slack") SLACK = stod(nx()); else if (a == "--width") WIDTH = stoi(nx()); else if (a == "--noise") NOISE = stod(nx());
    else if (a == "--pot-dmin") POT_DMIN = stoi(nx());
    else if (a == "--out") OUTF = nx(); else if (a == "--pool") POOLF = nx(); else if (a == "--pool-min") POOLMIN = stoll(nx());
    else if (a == "--record") RECORD = stoll(nx()); else if (a == "--checker") CHECKER = nx();
    else if (a == "--selfcheck") SELFCHECK = true;
    else if (a == "--seed-adapt") SEEDADAPT = stoi(nx()); else if (a == "--warm") WARM = true;
    else { fprintf(stderr, "unknown option %s (see the comment at the top of macro.cpp)\n", a.c_str()); return 1; }
  }
  if (AMAX < AMIN) AMAX = AMIN;
  if (TMAX < TMIN) TMAX = TMIN;
  vector<vector<Mv>> seeds; if (!seedFile.empty()) seeds = read_tours(seedFile.c_str());
  if ((AMAX > 0 || algo == "dfs" || SELFCHECK) && seeds.empty()) { fprintf(stderr, "need --seed-tours with a finished tour\n"); return 1; }

  if (SELFCHECK) {  // replay the seed; decompose it into macros; check the generator contains them
    const vector<Mv>& t = seeds[0];
    printf("seed replay: %d minutes, %zu jumps (validate %lld)\n", replay(t, t.size()).T, t.size(), validate(t));
    vector<int> si = sit_indices(t);
    int start = AMIN, gen = 0, same = 0, tot = 0, hopsSeed = 0, hopsOurs = 0;
    S s = replay(t, start);
    // walk: zero-wait prefix, then macros
    size_t i = start; while (i < t.size() && t[i].delta == 0 && t[i].q != H8) { apply_move(s, 0, t[i].q); i++; hopsSeed++; }
    vector<Macro> mac;
    while (i + 1 < t.size()) {
      int d = t[i].delta, q = t[i].q; size_t j = i + 1;
      while (j + 1 < t.size() && t[j].delta == 0) j++;
      int r = (j - 1 > i) ? t[j - 1].q : q; tot++; hopsSeed += (int)(j - 1 - i);
      // does the generator offer (d, q, r)? same path?
      S c = after_sit(s, d, q); Reach R; reach_from(c, c.pos, R);
      bool offered = R.par[r] >= 0 || r == q; bool okhops = offered && R.hops[r] <= MAXHOPS;
      if (offered && okhops) gen++;
      if (offered && R.hops[r] == (int)(j - 1 - i)) same++;
      if (offered) hopsOurs += R.hops[r];
      S s2 = s; for (size_t k = i; k < j; k++) apply_move(s2, t[k].delta, t[k].q);
      mac.push_back({(int16_t)d, (int8_t)q, (int8_t)r});
      s = s2; i = j;
    }
    printf("from move %d: %d macros (%d sits in whole tour), %d offered by the generator (max-hops %d), %d with the same hop count; seed hops %d, our min-cost hops %d\n",
           start, tot, (int)si.size(), gen, MAXHOPS, same, hopsSeed, hopsOurs);
    // rebuild the seed's tail from its macros with our min-cost paths
    S root = replay(t, start); vector<Mv> full(t.begin(), t.begin() + start);
    // leading zero-wait hops of the tail form a no-sit macro
    size_t k0 = start; while (k0 < t.size() && t[k0].delta == 0 && t[k0].q != H8) k0++;
    vector<Macro> all; if (k0 > (size_t)start) all.push_back({0, -1, (int8_t)t[k0 - 1].q});
    all.insert(all.end(), mac.begin(), mac.end());
    vector<Mv> tail = expand(root, all, true, t.back().delta);
    full.insert(full.end(), tail.begin(), tail.end());
    printf("rebuilt from macros with min-cost paths: %lld minutes (validate), %zu jumps\n", validate(full), full.size());
    if (validate(full) >= 0) printf("rebuilt tour: %s\n", tour_text(full).c_str());
    S r0 = replay(t, start); printf("pot at move %d: T %d + pot %.0f = %.0f\n", start, r0.T, pot_future(r0), r0.T + pot_future(r0));
    // T + pot along the seed (at each sitting state, i.e. just before each sit): the DFS bound needs T + pot >= final
    printf("T+pot before each sit (macros left: estimate - final):");
    for (int j = 0; j < (int)si.size(); j++) { S x = replay(t, si[j]); double pf = pot_future(x);
      if (si[j] >= start) printf(" %d:%s", (int)si.size() - j, pf < 0 ? "-" : to_string((long long)(x.T + pf) - (long long)validate(t)).c_str()); }
    printf("\n");
    return 0;
  }

  signal(SIGINT, on_signal); signal(SIGTERM, on_signal);
  printf("macro search: algo %s, %d threads, level %d, iters %d, anchor %d-%d, max-hops %d, use-w %d, hop-pen %.2f, sit-bias %.2f, tail %d, %zu seeds\n",
         algo.c_str(), THREADS, LEVEL, ITERS, AMIN, AMAX, MAXHOPS, USEW, HOPPEN, SITBIAS, TAIL, seeds.size()); fflush(stdout);
  for (auto& t : seeds) { long long T = validate(t); std::lock_guard<std::mutex> lk(MTX); if (T > GBEST) { GBEST = T; GBESTT = t; } }
  if (GBEST) printf("best seed: %lld minutes (engine replay)\n", GBEST);
  long long seedBest = GBEST;
  if (algo != "dfs") { GBEST = 0; GBESTT.clear(); }  // search modes: the first finished result is always written
  else GBEST = seedBest;                            // dfs: only improvements on the seed are written

  auto pick_seed = [&](std::mt19937& rng) {
    std::lock_guard<std::mutex> lk(MTX);
    if (!GBESTT.empty() && std::uniform_real_distribution<double>(0, 1)(rng) < SEEDBEST) return GBESTT;
    if (seeds.empty()) return vector<Mv>();
    return seeds[std::uniform_int_distribution<size_t>(0, seeds.size() - 1)(rng)];
  };
  auto worker = [&](int tid) {
    std::mt19937 rng(RNG * 1000003u + tid);
    for (int restart = 0; !STOP; restart++) {
      vector<Mv> seed = pick_seed(rng);
      if (algo == "dfs") {  // sweep K = tail-min .. tail-max over the threads, on the best tour so far
        int K = TMIN + tid + restart * THREADS;
        if (K > TMAX) break;
        { std::lock_guard<std::mutex> lk(MTX); if (!GBESTT.empty()) seed = GBESTT; }
        double t0 = elapsed(); long long nodes = 0;
        long long T = polish_tail(seed, K, DFSN, "dfs", &nodes);
        std::lock_guard<std::mutex> lk(MTX);
        printf("[%7.0fs] thread %d dfs: last %d macros of a %lld-minute tour, %lld nodes, %.1fs, %s, %s (best %lld)\n", elapsed(), tid, K, validate(seed),
               nodes, elapsed() - t0, nodes < DFSN && !STOP ? "complete" : "budget hit", T > 0 ? "IMPROVED" : "no improvement", GBEST);
        fflush(stdout); continue;
      }
      int k = 0;
      if (!seed.empty() && AMAX > 0) k = std::uniform_int_distribution<int>(min<int>(AMIN, seed.size() - 1), min<int>(AMAX, seed.size() - 1))(rng);
      S root = replay(seed, k); vector<Mv> prefix(seed.begin(), seed.begin() + k);
      double t0 = elapsed();
      Result r;
      if (algo == "beam") r = beam(root, WIDTH, NOISE, rng, MAXHOPS);
      else {
        Policy pol; Result init; bool haveInit = false;
        if ((SEEDADAPT > 0 || WARM) && !seed.empty()) {  // shift the weights toward the seed's own continuation
          init.mac = decompose(seed, k); init.finD = (int16_t)seed.back().delta;
          vector<Mv> full = prefix, tail = expand(root, init.mac, true, init.finD); full.insert(full.end(), tail.begin(), tail.end());
          long long T = validate(full);
          if (T >= 0) {
            init.finished = true; init.minutes = T; init.score = (double)T; haveInit = WARM;
            for (int a = 0; a < SEEDADAPT; a++) adapt(pol, root, init.mac, ALPHA);
          }
        }
        r = nrpa(LEVEL, root, pol, rng, haveInit ? &init : nullptr);
      }
      long long T = -1;
      if (r.finished) {
        vector<Mv> full = prefix, tail = expand(root, r.mac, true, r.finD); full.insert(full.end(), tail.begin(), tail.end());
        T = report(full, algo.c_str());
        if (T > 0 && TAIL > 0 && !STOP) polish_tail(full, TAIL, POLISH_NODES, algo.c_str());
      }
      std::lock_guard<std::mutex> lk(MTX);
      printf("[%7.0fs] thread %d %s restart %d (anchor %d): %s %lld minutes, %zu macros, %.1fs (best %lld, %lld rollouts, %lld dfs nodes)\n", elapsed(), tid,
             algo.c_str(), restart, k, r.finished ? "finished" : "unfinished", r.finished ? T : r.minutes, r.mac.size(), elapsed() - t0, GBEST,
             (long long)PLAYOUTS, (long long)DFSNODES);
      fflush(stdout);
    }
  };
  std::atomic<int> running{THREADS};
  vector<std::thread> th; for (int t = 0; t < THREADS; t++) th.emplace_back([&, t]() { worker(t); running--; });
  while (!STOP && running > 0 && (TIME <= 0 || elapsed() < TIME)) std::this_thread::sleep_for(std::chrono::milliseconds(200));
  STOP = true;
  for (auto& t : th) t.join();
  printf("done: best %lld minutes after %.0fs (seed best %lld), %lld rollouts, %lld dfs nodes\n", GBEST, elapsed(), seedBest, (long long)PLAYOUTS, (long long)DFSNODES);
}
