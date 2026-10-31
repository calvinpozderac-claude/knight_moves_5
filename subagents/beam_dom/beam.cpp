// Large beam search with dominance pruning and diversity caps for the swamp-knight puzzle.
//
// Depth-synchronised beam over pinned moves (src/eng.h). Every child of every beam node is scored
//   score = T + est(state) [+ noise]
// where est is an optimistic dance estimate (see estimate()). Children are taken in score order; a child
// is dropped if
//   - a node already accepted (this layer or an earlier one, in the transposition table) has the same
//     altitudes and knight position, at least the same T and elementwise <= visits (dominance), or
//   - its diversity buckets are full: at most cap-recent nodes may share the same last-K-moves signature,
//     at most cap-group nodes the same group structure (points level with the knight / 2 below it).
// Moves into h8 are finished tours: the best is written to --out, good ones to --pool.
//
// Nodes are compact: per node a trace pointer (parent + move, 8 bytes in a shared trace tree that is
// garbage collected), T, a recent-move history, and (unless --replay) a packed state of 152 bytes. With
// --replay the state is rebuilt by replaying the path, so a node costs 16 bytes + its trace.
//
// Restart mode (--restarts R, 0 = until --time): every restart draws k in [kmin, kmax], optionally takes
// the best tour so far as seed (--reseed-best), multiplies the width by --widen and perturbs scores with
// Gaussian noise (--noise-restart) plus random tie-breaking.
//
// Build: g++ -O3 -march=native -std=c++17 -pthread -o beam beam.cpp
#include "../../src/eng.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <thread>

struct Mv { int delta, q; };
static const uint32_t NONE = 0xffffffffu;

// ---------------------------------------------------------------- options
static int WIDTH = 10000, THREADS = 1, KMIN = 0, KMAX = -1, RESTARTS = 1, RECENTK = 6, TTLOG2 = 20, MAXDEPTH = 400;
static int POOLPER = 20, LOGEVERY = 1;
static double TIME = 0, NOISE0 = 0, NOISER = 20, WIDEN = 1, MAXWIDTH = 2e7, CAPR_FRAC = 0.05, CAPG_FRAC = 0.10;
static int MERGE_LOOK = 6, MERGE_SKIP = 48; static bool NEEDH8 = true; static double WALK_W = 0.3; static int LATE_D = 90, ADJ2_D = 110;
// moves made so far (every move adds one visit; the start counts one): uniform across a beam layer
static inline int depth_of(const S& s) { int v = 0; for (int i = 0; i < 64; i++) v += s.vis[i]; return v - 1; } static bool ADJ = true, ADJ2 = true; static int ADJ2_MIN = 40;
static double DANCE_W = 0.8, UNREACH_PEN = 300, CLIMB_W = 30, OUTSIDE_W = 0, RESEED = 0;
static long long POOLMIN = 7500;
static bool REPLAY = false, KEEPSEED = false, KEEPTT = false, RANDTIE = false;
static unsigned RNG = 1;
static string seedFile, outFile = "beam_best.txt", poolFile;

static std::atomic<bool> STOP{false};
static std::chrono::steady_clock::time_point T0;
static double elapsed() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count(); }
static void on_sig(int) { STOP = true; }

// ---------------------------------------------------------------- tours
static vector<Mv> read_tour(const string& fn) {  // the first line with moves (finished or not)
  FILE* f = fopen(fn.c_str(), "r"); if (!f) { fprintf(stderr, "cannot open %s\n", fn.c_str()); exit(1); }
  static char line[400000]; vector<Mv> t;
  while (fgets(line, sizeof line, f)) {
    S s = start_state(); char* p = line; int w; char sq[8];
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
      t.push_back({d, q}); apply_move(s, d, q); p++;
    }
    if (!t.empty()) break;
  }
  fclose(f); return t;
}
// replays seq with legality checks (every move must be a pinned move); returns T or -1
static long long replay_check(const vector<Mv>& seq, string* err = nullptr) {
  S s = start_state(); vector<pair<int, int>> mv;
  for (size_t i = 0; i < seq.size(); i++) {
    pinned_moves(s, 3, true, mv); bool ok = false;
    for (auto& m : mv) if (m.first == seq[i].delta && m.second == seq[i].q) ok = true;
    if (!ok) { if (err) *err = "illegal move " + to_string(i) + " to " + nm(seq[i].q); return -1; }
    if (seq[i].q == H8 && i + 1 != seq.size()) { if (err) *err = "h8 before the end"; return -1; }
    apply_move(s, seq[i].delta, seq[i].q);
  }
  return s.T;
}
static string tour_text(const vector<Mv>& seq) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < seq.size(); i++) {
    int w = apply_move(s, seq[i].delta, seq[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(seq[i].q).c_str()); out += buf;
  }
  return out;
}

// ---------------------------------------------------------------- estimate
// Idealised dance on the levels U (group 0) and U-2 (group 1): sits alternate between the groups, each
// worth 4 x the group size; every point gives one sit per remaining arrival (+1 if the knight stands on
// it); points whose opposite partner is outside the groups ("open") sit first, otherwise opening a point
// removes its partner from its group. Same rule as dance_pot in src/nrpa.cpp, but O(64 + sits).
// sink > 0: first sink the knight's group by `sink` (virtually; the opposite point rises), the knight
// then jumps, so it does not sit first.
static int ideal_dance(const S& s, int U, bool knightSits, int sink = 0) {
  int8_t grp[64]; int n[2] = {0, 0};
  const int a = s.alt[s.pos], O = 63 - s.pos; const bool rise = s.alt[O] != a;
  for (int i = 0; i < 64; i++) {
    int al = s.alt[i];
    if (sink) { if (al == a && i != O) al -= sink; else if (i == O && rise) al += sink; }
    grp[i] = al == U ? 0 : al == U - 2 ? 1 : -1; if (grp[i] >= 0) n[grp[i]]++;
  }
  if (!n[0] || !n[1]) return 0;
  if (NEEDH8 && grp[H8] < 0) return 0;  // the tour ends on h8, so h8 must be in the dance
  int cap[64];
  for (int i = 0; i < 64; i++) cap[i] = (grp[i] >= 0 && i != H8 && i != A1) ? max(0, 3 - (int)s.vis[i] + (i == s.pos)) : 0;
  if (ADJ) {  // a sit is followed by a jump onto an orthogonal neighbour in the other group: no such neighbour, no sits
    for (int i = 0; i < 64; i++) if (cap[i] > 0) {
      bool ok = false;
      for (int k = 0; k < NBN[i] && !ok; k++) { int q = NBQ[i][k]; if (NBT[i][k] == 1 && grp[q] == (grp[i] ^ 1) && (q == H8 || s.vis[q] < 3)) ok = true; }
      if (!ok) cap[i] = 0;
    }
    if (ADJ2 && depth_of(s) >= ADJ2_D && n[0] + n[1] >= ADJ2_MIN) {  // (late in the dance) every sit at i is followed by an arrival at an orthogonal neighbour in the other group
      int c2[64];
      for (int i = 0; i < 64; i++) {
        c2[i] = cap[i]; if (cap[i] <= 0) continue;
        int sum = 0; for (int k = 0; k < NBN[i]; k++) { int q = NBQ[i][k]; if (NBT[i][k] == 1 && grp[q] == (grp[i] ^ 1)) sum += q == H8 ? 1 : max(0, 3 - (int)s.vis[q]); }
        c2[i] = min(cap[i], sum + (i == s.pos));
      }
      memcpy(cap, c2, sizeof cap);
    }
  }
  int Ocap[2] = {0, 0}; uint8_t item[2][5][32]; int ni[2][5] = {{0}}; bool used[32] = {false};
  for (int i = 0; i < 64; i++) {
    if (grp[i] < 0) continue;
    int j = 63 - i;
    if (grp[j] < 0) { Ocap[grp[i]] += cap[i]; continue; }
    if (i > j) continue;
    if (grp[i] == grp[j]) { int b = cap[i] >= cap[j] ? i : j; if (cap[b] > 0) { int g = grp[b], c = min(cap[b], 4); item[g][c][ni[g][c]++] = (uint8_t)b; } }
    else {
      if (cap[i] > 0) { int g = grp[i], c = min(cap[i], 4); item[g][c][ni[g][c]++] = (uint8_t)i; }
      if (cap[j] > 0) { int g = grp[j], c = min(cap[j], 4); item[g][c][ni[g][c]++] = (uint8_t)j; }
    }
  }
  int fut = 0, g = 0;
  auto open_pt = [&](int i) { int j = 63 - i; used[min(i, j)] = true; n[grp[j]]--; fut += 4 * n[grp[i]]; Ocap[grp[i]] += cap[i] - 1; };
  if (knightSits && grp[s.pos] == 0 && cap[s.pos] > 0) {
    int p = s.pos, j = 63 - p;
    if (grp[j] >= 0) open_pt(p); else { Ocap[0]--; fut += 4 * n[0]; }
    g = 1;
  }
  for (int step = 0; step < 300; step++) {
    if (Ocap[g] > 0) { Ocap[g]--; fut += 4 * n[g]; g ^= 1; continue; }
    int pick = -1;
    for (int c = 4; c >= 1 && pick < 0; c--)
      while (ni[g][c] > 0) { int i = item[g][c][--ni[g][c]]; if (!used[min(i, 63 - i)]) { pick = i; break; } }
    if (pick < 0) break;
    open_pt(pick); g ^= 1;
  }
  return fut;
}
// Greedy dance walk on the levels U (knight's group, upper) and U-2: the knight sits (4 x group size;
// the opposite point leaves the groups), then jumps onto an orthogonal neighbour in the other group that
// still has a visit (preferring points whose partner is already gone, then more visits left, then more
// onward neighbours); with no such neighbour it hops (no wait) by a knight move inside its group. Unlike
// ideal_dance it respects adjacency, so it is close to what a real dance can get. Returns the minutes up
// to the last time the knight stood next to h8 (where the tour can end).
static int walk_dance(const S& s, int U) {
  int8_t grp[64]; int8_t vl[64]; int n[2] = {0, 0};
  for (int i = 0; i < 64; i++) {
    grp[i] = s.alt[i] == U ? 0 : s.alt[i] == U - 2 ? 1 : -1; if (grp[i] >= 0) n[grp[i]]++;
    vl[i] = (grp[i] >= 0 && i != H8 && i != A1) ? (int8_t)max(0, 3 - (int)s.vis[i]) : 0;
  }
  if (grp[s.pos] != 0 || !n[1] || grp[H8] < 0) return 0;
  static uint64_t H8NB = 0;
  if (!H8NB) for (int k = 0; k < NBN[H8]; k++) H8NB |= 1ULL << NBQ[H8][k];
  int p = s.pos, fut = 0, best = (H8NB >> p & 1) ? 0 : -1;
  auto viable = [&](int q, int g) {  // q (in group g) can be sat on and left by an orthogonal jump
    for (int k = 0; k < NBN[q]; k++) { int r = NBQ[q][k]; if (NBT[q][k] == 1 && grp[r] == (g ^ 1) && vl[r] > 0) return true; }
    return false;
  };
  for (int step = 0; step < 400; step++) {
    int g = grp[p]; if (g < 0) break;
    // choose the orthogonal neighbour to jump to after sitting
    int bq = -1, bs = -1;
    for (int k = 0; k < NBN[p]; k++) {
      int q = NBQ[p][k]; if (NBT[p][k] != 1 || grp[q] != (g ^ 1) || vl[q] <= 0) continue;
      int onward = 0; for (int k2 = 0; k2 < NBN[q]; k2++) { int r = NBQ[q][k2]; if (NBT[q][k2] == 1 && r != p && grp[r] == g && vl[r] > 0) onward++; }
      int sc = (grp[63 - q] < 0 ? 1000 : 0) + 100 * vl[q] + 10 * onward + (q == 63 - p ? -2000 : 0);
      if (sc > bs) { bs = sc; bq = q; }
    }
    if (bq < 0) {  // hop: shortest chain of knight moves inside the group (no wait) to a point that can sit
      int par[64], qu[64], qh = 0, qt = 0, found = -1; uint64_t seen = 1ULL << p; qu[qt++] = p; par[p] = -1;
      while (qh < qt && found < 0) {
        int u = qu[qh++];
        for (int k = 0; k < NBN[u]; k++) {
          int r = NBQ[u][k]; if (NBT[u][k] != 0 || grp[r] != g || vl[r] <= 0 || (seen >> r & 1)) continue;
          seen |= 1ULL << r; par[r] = u; qu[qt++] = r;
          if (viable(r, g)) { found = r; break; }
        }
      }
      if (found < 0) break;
      for (int r = found; r != p; r = par[r]) { vl[r]--; if (H8NB >> r & 1) best = max(best, fut); }
      p = found; continue;
    }
    // sit at p: its partner leaves, then the group sinks 4 (the other group becomes the upper one)
    int j = 63 - p; if (grp[j] >= 0) { n[grp[j]]--; grp[j] = -1; vl[j] = 0; }
    fut += 4 * n[g];
    if (H8NB >> p & 1) best = max(best, fut);
    vl[bq]--; p = bq;
    if (H8NB >> p & 1) best = max(best, fut);
  }
  return max(best, 0);
}

// Relaxed reachability: BFS from the knight over points with visits left, where u -> q is allowed if some
// wait at u (sinking u's group only) makes the jump legal. Optimistic, but it sees h8 being walled in by
// spent points, and maxAlt (the highest point reached) measures progress of the climb.
static bool relaxed_reach(const S& s, int& maxAlt) {
  uint64_t seen = 1ULL << s.pos; int st[64], sp = 0; st[sp++] = s.pos; maxAlt = s.alt[s.pos]; bool h8 = false;
  while (sp) {
    int u = st[--sp]; int au = s.alt[u], O = 63 - u;
    for (int k = 0; k < NBN[u]; k++) {
      int q = NBQ[u][k], t = NBT[u][k];
      if (q != H8 && ((seen >> q & 1) || s.vis[q] >= 3)) continue;
      int diff = s.alt[q] - au; bool ok;
      if (diff == 0 && q != O) ok = t == 0;              // same group: sinks along, only a knight move
      else ok = t == 0 ? diff <= 0 : t == 1 ? diff <= 2 : diff <= 1;
      if (q == H8 && diff >= -4 && diff <= 4) ok = true;  // h8 near the knight's level: sits can line it up
      if (!ok) continue;
      if (q == H8) { h8 = true; continue; }
      seen |= 1ULL << q; st[sp++] = q; if (s.alt[q] > maxAlt) maxAlt = s.alt[q];
    }
  }
  return h8;
}
// est = best idealised dance over the level pairs that could carry the rest of the tour (the knight's
// level and h8's level as either the upper or the lower group), minus a penalty if h8 can no longer be
// reached (relaxed), minus CLIMB_W per unit the knight is still below h8 (climb phase)
static double estimate(const S& s) {
  int a = s.alt[s.pos], h = s.alt[H8];
  int d = max(ideal_dance(s, a, true), ideal_dance(s, a + 2, false));
  if (h != a && h != a - 2 && h != a + 2) d = max(d, max(ideal_dance(s, h, false), ideal_dance(s, h + 2, false)));
  if (MERGE_LOOK > 0) {  // one sink ahead: the knight's group merges into a lower level (skipped once the dance has formed)
    int cnt[24] = {0};  // points at a + 4 - j
    for (int i = 0; i < 64; i++) { int j = a + 4 - s.alt[i]; if (j >= 0 && j < 24) cnt[j]++; }
    if (cnt[4] + cnt[6] < MERGE_SKIP)
      for (int dl = 1; dl <= MERGE_LOOK && dl + 6 < 24; dl++) {
        if (cnt[4 + dl] + cnt[4 + dl - 2] + cnt[4 + dl + 2] == 0) continue;  // nothing to merge with
        d = max(d, max(ideal_dance(s, a - dl + 2, false, dl), ideal_dance(s, a - dl, false, dl)));
      }
  }
  double e = DANCE_W * d;
  if (WALK_W > 0 && depth_of(s) >= LATE_D) {  // late in the dance: blend in the adjacency-respecting greedy walk
    int w = walk_dance(s, a);
    if (w > 0) e = (1 - WALK_W) * e + WALK_W * w;
  }
  int maxAlt; bool h8r = relaxed_reach(s, maxAlt);
  if (!h8r) e -= UNREACH_PEN;
  if (CLIMB_W) e -= CLIMB_W * max(0, h - maxAlt - 2);
  if (OUTSIDE_W) {  // points outside both dance levels that still have visits (could still be merged)
    int o = 0;
    for (int i = 0; i < 64; i++) if (i != H8 && s.vis[i] < 3 && s.alt[i] != a && s.alt[i] != a - 2 && s.alt[i] != a + 2) o++;
    e += OUTSIDE_W * o;
  }
  return e;
}

// ---------------------------------------------------------------- compact state + TT
struct PState { AT alt[64]; uint64_t vis2[2]; uint8_t pos; };  // 152 bytes
static inline void pack_vis(const uint8_t* v, uint64_t o[2]) {
  o[0] = o[1] = 0; for (int i = 0; i < 64; i++) o[i >> 5] |= (uint64_t)(v[i] & 3) << ((i & 31) * 2);
}
static inline void pack(const S& s, PState& p) { memcpy(p.alt, s.alt, sizeof p.alt); pack_vis(s.vis, p.vis2); p.pos = s.pos; }
static inline void unpack(const PState& p, int32_t T, S& s) {
  memcpy(s.alt, p.alt, sizeof p.alt); for (int i = 0; i < 64; i++) s.vis[i] = (p.vis2[i >> 5] >> ((i & 31) * 2)) & 3;
  s.pos = p.pos; s.T = T;
}
static inline bool vis_le(const uint64_t a[2], const uint64_t b[2]) {  // a <= b elementwise (2-bit fields)
  for (int w = 0; w < 2; w++) for (int k = 0; k < 32; k++) if (((a[w] >> (2 * k)) & 3) > ((b[w] >> (2 * k)) & 3)) return false;
  return true;
}
struct TTE { uint64_t key; uint64_t vis[2]; int32_t T; uint16_t depth; uint16_t used; };
struct TTable {
  vector<TTE> e; uint64_t mask = 0;
  void init(int log2) { e.assign((size_t)4 << log2, TTE{0, {0, 0}, 0, 0, 0}); mask = (1ULL << log2) - 1; }
  void clear() { std::fill(e.begin(), e.end(), TTE{0, {0, 0}, 0, 0, 0}); }
  size_t b(uint64_t k) const { return (size_t)(((k >> 21) ^ k) & mask) * 4; }
  uint16_t gen = 1;  // entries from earlier restarts (--keep-tt) prune only states they strictly dominate
  bool dominated(uint64_t key, const uint64_t vis[2], int32_t T) const {
    size_t o = b(key);
    for (int i = 0; i < 4; i++) {
      const TTE& x = e[o + i];
      if (x.used && x.key == key && x.T >= T && vis_le(x.vis, vis)) {
        if (x.used == gen || x.T > T || x.vis[0] != vis[0] || x.vis[1] != vis[1]) return true;
      }
    }
    return false;
  }
  bool any_key(uint64_t key) const { size_t o = b(key); for (int i = 0; i < 4; i++) if (e[o + i].used && e[o + i].key == key) return true; return false; }
  void insert(uint64_t key, const uint64_t vis[2], int32_t T, int depth) {
    size_t o = b(key); int slot = -1, oldest = -1;
    for (int i = 0; i < 4; i++) {
      TTE& x = e[o + i];
      if (x.used && x.key == key && T >= x.T && vis_le(vis, x.vis)) { slot = i; break; }
      if (!x.used) { if (slot < 0) slot = i; continue; }
      if (oldest < 0 || x.depth < e[o + oldest].depth) oldest = i;
    }
    if (slot < 0) slot = oldest;
    TTE& x = e[o + slot]; x.key = key; x.vis[0] = vis[0]; x.vis[1] = vis[1]; x.T = T; x.depth = (uint16_t)depth; x.used = gen;
  }
};

// ---------------------------------------------------------------- trace tree
struct TNode { uint32_t parent; uint16_t delta; uint8_t q; uint8_t pad; };
static vector<TNode> TR;
static uint32_t tr_push(uint32_t parent, int delta, int q) { TR.push_back({parent, (uint16_t)delta, (uint8_t)q, 0}); return (uint32_t)TR.size() - 1; }
static void tr_path(uint32_t t, vector<Mv>& out) {
  out.clear(); while (t != NONE) { out.push_back({TR[t].delta, TR[t].q}); t = TR[t].parent; }
  std::reverse(out.begin(), out.end());
}
static void state_of_trace(uint32_t t, S& s) {
  static thread_local vector<Mv> p; tr_path(t, p); s = start_state(); for (auto& m : p) apply_move(s, m.delta, m.q);
}

// ---------------------------------------------------------------- beam
struct Meta { uint32_t trace; int32_t T; uint64_t hist; };
struct Cand { float score; uint32_t parent; uint64_t key; uint32_t sigR, sigG; int32_t T; uint16_t delta; uint8_t q, seed; };
struct Fin { int32_t T; uint32_t parent; uint16_t delta; uint8_t q; };
struct Ord { float score; uint32_t tb, idx; };

static inline uint32_t mix32(uint64_t x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33; return (uint32_t)x; }

static std::mutex OUTMTX;
static long long gBest = 0; static vector<Mv> gBestSeq; static long long gFinished = 0;
static void report_finish(const vector<Mv>& seq, long long T, const char* tag) {
  string err; long long chk = replay_check(seq, &err);
  if (chk != T) { fprintf(stderr, "internal replay mismatch (%lld vs %lld): %s\n", chk, T, err.c_str()); return; }
  if (T > gBest) {
    gBest = T; gBestSeq = seq;
    FILE* f = fopen(outFile.c_str(), "w");
    if (f) { fprintf(f, "%lld\n%s\n", T, tour_text(seq).c_str()); fclose(f); }
    printf("[%7.1fs] NEW BEST %lld minutes (%zu jumps, %s) -> %s\n", elapsed(), T, seq.size(), tag, outFile.c_str()); fflush(stdout);
  }
}

struct BeamResult { long long best = 0; long long nodes = 0; int depth = 0; };

static BeamResult run_beam(const vector<Mv>& seed, int k, int width, double noise, unsigned rngSeed, TTable& tt) {
  BeamResult res; std::mt19937_64 rng(rngSeed);
  const int capR = CAPR_FRAC > 0 ? max(1, (int)(CAPR_FRAC * width)) : 0;
  const int capG = CAPG_FRAC > 0 ? max(1, (int)(CAPG_FRAC * width)) : 0;
  const uint64_t histMask = RECENTK >= 8 ? ~0ULL : ((1ULL << (6 * RECENTK)) - 1);
  // root: the first k moves of the seed
  TR.clear(); uint32_t rootTr = NONE; S root = start_state(); uint64_t h0 = 0;
  for (int i = 0; i < k; i++) { rootTr = tr_push(rootTr, seed[i].delta, seed[i].q); apply_move(root, seed[i].delta, seed[i].q); h0 = (h0 << 6) | seed[i].q; }
  if (!KEEPTT) tt.clear(); else tt.gen++;
  vector<Meta> meta{{rootTr, root.T, h0 & 0xffffffffffffULL}}, nmeta;
  vector<PState> st, nst; if (!REPLAY) { st.resize(1); pack(root, st[0]); }
  int seedIdx = (KEEPSEED && k < (int)seed.size()) ? 0 : -1;
  vector<Cand> cand; vector<Ord> ord; vector<uint32_t> cntR(1 << 20), cntG(1 << 20);
  vector<vector<Cand>> tc(THREADS); vector<vector<Fin>> tf(THREADS);
  normal_distribution<double> gauss(0, 1);
  vector<Mv> path;
  for (int depth = k; depth < MAXDEPTH && !meta.empty() && !STOP; depth++) {
    if (TIME > 0 && elapsed() >= TIME) { STOP = true; break; }
    double tl = elapsed();
    const int N = (int)meta.size();
    const int seedMove = (seedIdx >= 0 && depth < (int)seed.size()) ? depth : -1;
    // ---- expand (parallel)
    std::atomic<int> next{0};
    auto expand = [&](int tid) {
      auto& C = tc[tid]; auto& F = tf[tid]; C.clear(); F.clear();
      std::mt19937_64 r2(rngSeed * 7919 + depth * 104729 + tid);
      vector<pair<int, int>> mv; S s, c;
      while (!STOP) {
        int i0 = next.fetch_add(256); if (i0 >= N) break;
        for (int i = i0; i < min(N, i0 + 256); i++) {
          if (REPLAY) state_of_trace(meta[i].trace, s); else unpack(st[i], meta[i].T, s);
          pinned_moves(s, 3, true, mv);
          for (auto& [d, q] : mv) {
            if (q == H8) { int w = d ? d * group_n(s, s.pos) : 0; F.push_back({s.T + w, (uint32_t)i, (uint16_t)d, (uint8_t)q}); continue; }
            c = s; apply_move(c, d, q);
            double sc = c.T + estimate(c);
            if (noise > 0) sc += noise * gauss(r2);
            uint64_t hist = ((meta[i].hist << 6) | q) & 0xffffffffffffULL;
            AT a = c.alt[c.pos]; uint64_t mU = 0, mD = 0;
            for (int j = 0; j < 64; j++) { if (c.alt[j] == a) mU |= 1ULL << j; else if (c.alt[j] == a - 2) mD |= 1ULL << j; }
            Cand x; x.score = (float)sc; x.parent = (uint32_t)i; x.key = hashS(c, false);
            x.sigR = mix32((hist & histMask) * 0x9E3779B97F4A7C15ULL + RECENTK);
            x.sigG = mix32(mU * 0x9E3779B97F4A7C15ULL ^ (mD + 0x632BE59BD9B4E019ULL) * 0xBF58476D1CE4E5B9ULL);
            x.T = c.T; x.delta = (uint16_t)d; x.q = (uint8_t)q;
            x.seed = (i == seedIdx && seedMove >= 0 && seed[seedMove].delta == d && seed[seedMove].q == q);
            C.push_back(x);
          }
        }
      }
    };
    if (THREADS == 1) expand(0); else { vector<std::thread> th; for (int t = 0; t < THREADS; t++) th.emplace_back(expand, t); for (auto& t : th) t.join(); }
    if (STOP) break;
    double te = elapsed();
    // ---- finished tours of this layer
    {
      vector<Fin> fin; for (auto& F : tf) fin.insert(fin.end(), F.begin(), F.end());
      sort(fin.begin(), fin.end(), [](const Fin& a, const Fin& b) { return a.T > b.T; });
      gFinished += fin.size(); int written = 0;
      for (size_t j = 0; j < fin.size(); j++) {
        if (fin[j].T < POOLMIN && fin[j].T <= gBest) break;
        bool isBest = fin[j].T > gBest;
        if (!isBest && written >= POOLPER) break;
        tr_path(meta[fin[j].parent].trace, path); path.push_back({fin[j].delta, fin[j].q});
        if (isBest) report_finish(path, fin[j].T, "beam");
        if (!poolFile.empty() && fin[j].T >= POOLMIN && written < POOLPER) {
          FILE* f = fopen(poolFile.c_str(), "a"); if (f) { fprintf(f, "%d\t%s\n", fin[j].T, tour_text(path).c_str()); fclose(f); }
          written++;
        }
      }
      if (!fin.empty() && fin[0].T > res.best) res.best = fin[0].T;
    }
    // ---- select
    cand.clear(); for (auto& C : tc) cand.insert(cand.end(), C.begin(), C.end());
    const size_t M = cand.size(); ord.resize(M);
    for (size_t j = 0; j < M; j++) ord[j] = {cand[j].score, RANDTIE || noise > 0 ? (uint32_t)rng() : mix32(cand[j].key ^ ((uint64_t)cand[j].parent << 20) ^ cand[j].q), (uint32_t)j};
    auto better = [](const Ord& a, const Ord& b) { return a.score != b.score ? a.score > b.score : a.tb > b.tb; };
    size_t sorted = min(M, (size_t)width * 4);
    if (sorted < M) std::nth_element(ord.begin(), ord.begin() + sorted, ord.end(), better);
    std::sort(ord.begin(), ord.begin() + sorted, better);
    std::fill(cntR.begin(), cntR.end(), 0); std::fill(cntG.begin(), cntG.end(), 0);
    nmeta.clear(); vector<uint32_t> acc; acc.reserve(min(M, (size_t)width));
    long long domDrop = 0, divDrop = 0; int newSeedIdx = -1;
    S tmp; uint64_t cv[2];
    auto child_vis = [&](const Cand& x, uint64_t out[2]) {
      if (REPLAY) { state_of_trace(meta[x.parent].trace, tmp); tmp.vis[x.q]++; pack_vis(tmp.vis, out); }
      else { out[0] = st[x.parent].vis2[0]; out[1] = st[x.parent].vis2[1]; out[x.q >> 5] += 1ULL << ((x.q & 31) * 2); }
    };
    auto accept = [&](uint32_t j) {
      const Cand& x = cand[j]; child_vis(x, cv); tt.insert(x.key, cv, x.T, depth + 1);
      uint32_t t = tr_push(meta[x.parent].trace, x.delta, x.q);
      nmeta.push_back({t, x.T, ((meta[x.parent].hist << 6) | x.q) & 0xffffffffffffULL}); acc.push_back(j);
      cntR[x.sigR & 0xfffff]++; cntG[x.sigG & 0xfffff]++;
    };
    float seedScore = 0; long long seedRank = -1;
    if (seedMove >= 0) for (size_t j = 0; j < M; j++) if (cand[j].seed) {
      newSeedIdx = 0; seedScore = cand[j].score; seedRank = 0;
      for (size_t o = 0; o < M; o++) if (cand[o].score > seedScore) seedRank++;
      accept((uint32_t)j); break;
    }
    for (size_t o = 0; o < M && (int)nmeta.size() < width; o++) {
      if (o == sorted) {  // ran out of the sorted prefix: sort the rest
        std::sort(ord.begin() + sorted, ord.end(), better); sorted = M;
      }
      const Cand& x = cand[ord[o].idx];
      if (x.seed && newSeedIdx >= 0) continue;
      if ((capR && cntR[x.sigR & 0xfffff] >= (uint32_t)capR) || (capG && cntG[x.sigG & 0xfffff] >= (uint32_t)capG)) { divDrop++; continue; }
      if (tt.any_key(x.key)) { child_vis(x, cv); if (tt.dominated(x.key, cv, x.T)) { domDrop++; continue; } }
      accept(ord[o].idx);
    }
    // ---- materialise the new layer
    if (!REPLAY) {
      nst.resize(nmeta.size());
      auto mat = [&](int tid) {
        S s;
        for (size_t j = tid; j < nmeta.size(); j += THREADS) {
          const Cand& x = cand[acc[j]]; unpack(st[x.parent], meta[x.parent].T, s); apply_move(s, x.delta, x.q); pack(s, nst[j]);
        }
      };
      if (THREADS == 1) mat(0); else { vector<std::thread> th; for (int t = 0; t < THREADS; t++) th.emplace_back(mat, t); for (auto& t : th) t.join(); }
      st.swap(nst);
    }
    float topScore = M ? cand[ord[0].idx].score : 0;
    int32_t topT = 0; for (auto& m : nmeta) topT = max(topT, m.T);
    meta.swap(nmeta); seedIdx = newSeedIdx;
    res.nodes += M; res.depth = depth + 1;
    // ---- trace garbage collection
    if (TR.size() > max<size_t>(1 << 24, (size_t)width * 16)) {
      vector<uint8_t> mark(TR.size(), 0);
      for (auto& m : meta) { uint32_t t = m.trace; while (t != NONE && !mark[t]) { mark[t] = 1; t = TR[t].parent; } }
      vector<uint32_t> remap(TR.size(), NONE); vector<TNode> nt; nt.reserve(TR.size() / 4);
      for (size_t t = 0; t < TR.size(); t++) if (mark[t]) {
        TNode x = TR[t]; if (x.parent != NONE) x.parent = remap[x.parent]; remap[t] = (uint32_t)nt.size(); nt.push_back(x);
      }
      for (auto& m : meta) m.trace = remap[m.trace];
      TR.swap(nt);
    }
    if (LOGEVERY > 0 && (depth % LOGEVERY == 0 || meta.empty()))
      printf("  d %3d beam %8zu cand %9zu dom %8lld div %8lld | top score %6.0f maxT %5d | fin best %lld | %.2fs (exp %.2fs) trace %zu\n",
             depth + 1, meta.size(), M, domDrop, divDrop, topScore, topT, res.best, elapsed() - tl, te - tl, TR.size());
    if (LOGEVERY > 0 && seedRank >= 0 && depth % LOGEVERY == 0) printf("        seed node: score %.0f, %lld candidates score higher\n", seedScore, seedRank);
    fflush(stdout);
  }
  return res;
}

int main(int argc, char** argv) {
  init_board(); T0 = std::chrono::steady_clock::now();
  bool DIAG = false;
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--width") WIDTH = (int)stod(nx()); else if (a == "--threads") THREADS = stoi(nx()); else if (a == "--time") TIME = stod(nx());
    else if (a == "--seed-tour") seedFile = nx(); else if (a == "--k") KMIN = KMAX = stoi(nx());
    else if (a == "--kmin") KMIN = stoi(nx()); else if (a == "--kmax") KMAX = stoi(nx());
    else if (a == "--restarts") RESTARTS = stoi(nx()); else if (a == "--widen") WIDEN = stod(nx()); else if (a == "--max-width") MAXWIDTH = stod(nx());
    else if (a == "--noise") NOISE0 = stod(nx()); else if (a == "--noise-restart") NOISER = stod(nx()); else if (a == "--rand-tie") RANDTIE = true;
    else if (a == "--reseed-best") RESEED = stod(nx());
    else if (a == "--recent-k") RECENTK = stoi(nx()); else if (a == "--cap-recent") CAPR_FRAC = stod(nx()); else if (a == "--cap-group") CAPG_FRAC = stod(nx());
    else if (a == "--dance-w") DANCE_W = stod(nx()); else if (a == "--unreach-pen") UNREACH_PEN = stod(nx());
    else if (a == "--climb-w") CLIMB_W = stod(nx()); else if (a == "--outside-w") OUTSIDE_W = stod(nx()); else if (a == "--merge-look") MERGE_LOOK = stoi(nx()); else if (a == "--merge-skip") MERGE_SKIP = stoi(nx()); else if (a == "--no-adj") ADJ = false; else if (a == "--no-adj2") ADJ2 = false; else if (a == "--adj2-min") ADJ2_MIN = stoi(nx()); else if (a == "--no-need-h8") NEEDH8 = false; else if (a == "--walk-w") WALK_W = stod(nx()); else if (a == "--walk-d") LATE_D = stoi(nx()); else if (a == "--adj2-d") ADJ2_D = stoi(nx());
    else if (a == "--tt-log2") TTLOG2 = stoi(nx()); else if (a == "--max-depth") MAXDEPTH = stoi(nx());
    else if (a == "--replay") REPLAY = true; else if (a == "--keep-seed") KEEPSEED = true; else if (a == "--keep-tt") KEEPTT = true;
    else if (a == "--out") outFile = nx(); else if (a == "--pool") poolFile = nx(); else if (a == "--pool-min") POOLMIN = stoll(nx());
    else if (a == "--pool-per-layer") POOLPER = stoi(nx()); else if (a == "--log-every") LOGEVERY = stoi(nx());
    else if (a == "--rng") RNG = (unsigned)stoul(nx()); else if (a == "--diag") DIAG = true;
    else { fprintf(stderr, "unknown option %s (see RUN.md)\n", a.c_str()); return 1; }
  }
  if (KMAX < KMIN) KMAX = KMIN;
  signal(SIGINT, on_sig); signal(SIGTERM, on_sig);
  vector<Mv> seed;
  if (!seedFile.empty()) {
    seed = read_tour(seedFile); string err; long long T = replay_check(seed, &err);
    printf("seed %s: %zu moves, replay %lld minutes%s%s\n", seedFile.c_str(), seed.size(), T, err.empty() ? "" : " ERROR ", err.c_str());
    if (T < 0) return 1;
    if (!seed.empty() && seed.back().q == H8) { gBest = 0; }
  }
  if (KMAX > 0 && seed.empty()) { fprintf(stderr, "--k/--kmin/--kmax need --seed-tour\n"); return 1; }
  if (DIAG) {  // T, estimate and T + estimate along the seed
    S s = start_state();
    for (size_t i = 0; i <= seed.size(); i++) {
      if (i > 0) apply_move(s, seed[i - 1].delta, seed[i - 1].q);
      if (i == seed.size() || seed[i].q == H8) { printf("%3zu T %5d (end)\n", i, s.T); break; }
      double e = estimate(s); int ma; bool h8r = relaxed_reach(s, ma);
      printf("%3zu T %5d walk %5d est %6.0f T+est %6.0f h8reach %d pos %s alt %d h8alt %d\n", i, s.T, walk_dance(s, s.alt[s.pos]), e, s.T + e, h8r, nm(s.pos).c_str(), s.alt[s.pos], s.alt[H8]);
    }
    return 0;
  }
  printf("beam: width %d, threads %d, k %d-%d, restarts %d, widen %.2f, noise %.1f/%.1f, caps recent(%d) %.3f group %.3f, %s, tt 2^%d x4\n",
         WIDTH, THREADS, KMIN, KMAX, RESTARTS, WIDEN, NOISE0, NOISER, RECENTK, CAPR_FRAC, CAPG_FRAC, REPLAY ? "replay" : "stored states", TTLOG2);
  fflush(stdout);
  TTable tt; tt.init(TTLOG2);
  std::mt19937_64 rng(RNG);
  double w = WIDTH;
  for (int r = 0; (RESTARTS <= 0 || r < RESTARTS) && !STOP; r++) {
    vector<Mv> sd = seed; string from = seedFile;
    if (RESEED > 0 && !gBestSeq.empty() && std::uniform_real_distribution<double>(0, 1)(rng) < RESEED) { sd = gBestSeq; from = "best"; }
    int hi = min<int>(KMAX, sd.empty() ? 0 : (int)sd.size() - 1), lo = min(KMIN, hi);
    int k = std::uniform_int_distribution<int>(lo, hi)(rng);
    double noise = r == 0 ? NOISE0 : NOISER;
    int width = (int)min(w, MAXWIDTH);
    printf("[%7.1fs] restart %d: seed %s, k %d, width %d, noise %.1f\n", elapsed(), r, from.empty() ? "a1" : from.c_str(), k, width, noise); fflush(stdout);
    BeamResult br = run_beam(sd, k, width, noise, RNG * 1000003u + r, tt);
    printf("[%7.1fs] restart %d done: best finish %lld, depth %d, %lld candidates (overall best %lld)\n", elapsed(), r, br.best, br.depth, br.nodes, gBest);
    fflush(stdout);
    w *= WIDEN;
  }
  printf("done: best %lld minutes after %.1fs, %lld finishes seen -> %s\n", gBest, elapsed(), gFinished, outFile.c_str());
}
