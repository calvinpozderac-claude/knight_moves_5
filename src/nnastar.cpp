// Shared library for the NN-guided A* pipeline (build/libnnastar.so, driven by nnastar/run.py via ctypes).
//
// Three parts:
//  1. Workers (background threads, low priority): produce labelled training samples.
//     - seed jobs: take a known tour, keep its first k moves, finish the tour with an NRPA search of a
//       random level (0 = one random rollout, 1 = 100 rollouts, 2 = 10,000) from fresh weights. Every state
//       on the path is labelled with the best final score known to be reachable from it: the suffix
//       maximum of the path's own score and of a few probe rollouts launched from states along the path
//       (prefix states also get the seed tour's score, since the seed tour passes through them).
//     - label jobs: states the A* search has expanded, finished with an NRPA search; the state gets the
//       best score found (0 if nothing finishes). This keeps the network accurate where the search looks.
//  2. A* (best-first) search engine. Nodes are (parent, move) records, so a state is rebuilt by replaying
//     its path from the root; that keeps a node at 12 bytes. The open list is a binary max-heap of
//     (f, node), capped in size (the worst entries are dropped). Duplicate states (same altitudes, visits
//     and position) are skipped unless they arrive with more minutes. Children are handed to Python in
//     batches for the network to score; Python then pushes them with their f values. Every child that
//     jumps into h8 is a finished tour: the best one is kept, and its score is propagated up the tree
//     (node.best = best finished score below the node), which also gives training labels (harvest).
//  3. Shared sample format (Sample, 204 bytes) used for everything handed to Python.
#include "eng.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <thread>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

struct Mv { int delta, q; };

#pragma pack(push, 1)
struct Sample {
  int16_t alt[64];
  uint8_t vis[64];
  uint8_t pos;
  uint8_t src;     // 0 seed prefix, 1 seed path, 2 A* state (label job), 3 label-job path, 4 A* tree, 5 known tour
  uint16_t depth;  // moves from a1
  int32_t T;       // minutes so far
  float label;     // best final score known to be reachable (0: none found)
};
#pragma pack(pop)
static_assert(sizeof(Sample) == 204, "Sample layout");

static inline void to_sample(const S& s, int depth, int src, float label, Sample& o) {
  memcpy(o.alt, s.alt, sizeof o.alt); memcpy(o.vis, s.vis, sizeof o.vis);
  o.pos = s.pos; o.src = (uint8_t)src; o.depth = (uint16_t)min(depth, 65535); o.T = s.T; o.label = label;
}

// ============================================================================================
// NRPA (from src/nrpa.cpp, tour objective only)
// ============================================================================================
static int KIDX[64][64];
static const int NCODE = 64 * 16 * 32 * 4;
struct Policy {
  vector<float> w, e;
  Policy() : w(NCODE, 0.f), e(NCODE, 1.f) {}
};
static inline uint32_t code_of(const S& s, int delta, int q) {
  return (((uint32_t)s.pos * 16 + KIDX[s.pos][q]) * 32 + (uint32_t)min(delta, 31)) * 4 + (uint32_t)min<int>(s.vis[q], 3);
}
static double D0 = 1;
static double h8dist(const S& s) {
  int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  return sqrt(dx * dx + dy * dy + dz * dz) / D0;
}
struct Result { double score = -1e18; long long minutes = 0; bool finished = false; vector<Mv> seq; };
static std::atomic<bool> STOP{false};
static std::atomic<long long> ROLLOUTS{0};

// one rollout; passes chances to finish but remembers the best one (the tour ends there)
static Result playout(const S& root, const Policy& pol, std::mt19937& rng) {
  static thread_local vector<pair<int, int>> mv; static thread_local vector<double> w;
  S s = root; Result r;
  long long finT = -1; size_t finLen = 0; Mv finMove{0, H8};
  while (true) {
    pinned_moves(s, 3, true, mv);
    w.clear(); double z = 0; int nonfin = 0;
    for (auto& [d, q] : mv) {
      if (q == H8) {
        long long t = s.T + (long long)d * (d ? group_n(s, s.pos) : 0);
        if (t >= finT) { finT = t; finLen = r.seq.size(); finMove = {d, q}; }
        w.push_back(0); continue;
      }
      double e = pol.e[code_of(s, d, q)]; w.push_back(e); z += e; nonfin++;
    }
    if (nonfin == 0) break;
    double u = std::uniform_real_distribution<double>(0, z)(rng), acc = 0; int pick = -1;
    for (size_t i = 0; i < mv.size(); i++) { if (mv[i].second == H8) continue; acc += w[i]; pick = (int)i; if (acc >= u) break; }
    r.seq.push_back({mv[pick].first, mv[pick].second});
    apply_move(s, mv[pick].first, mv[pick].second);
    if (r.seq.size() > 4000) break;  // safety
  }
  ROLLOUTS++;
  if (finT >= 0) { r.seq.resize(finLen); r.seq.push_back(finMove); r.finished = true; r.minutes = finT; r.score = (double)finT; }
  else { r.minutes = s.T; r.score = -1e7 + s.T * exp(-h8dist(s)); }
  return r;
}

static void adapt(Policy& pol, const S& root, const vector<Mv>& seq, double alpha) {
  static thread_local vector<pair<uint32_t, float>> upd; upd.clear();
  static thread_local vector<pair<int, int>> mv;
  static thread_local vector<double> e;
  S s = root;
  for (const Mv& m : seq) {
    if (m.q == H8) break;
    pinned_moves(s, 3, true, mv);
    e.clear(); double z = 0;
    for (auto& [d, q] : mv) { double x = q == H8 ? 0 : pol.e[code_of(s, d, q)]; e.push_back(x); z += x; }
    for (size_t i = 0; i < mv.size(); i++) if (mv[i].second != H8) upd.push_back({code_of(s, mv[i].first, mv[i].second), (float)(-alpha * e[i] / z)});
    upd.push_back({code_of(s, m.delta, m.q), (float)alpha});
    apply_move(s, m.delta, m.q);
  }
  for (auto& [c, d] : upd) pol.w[c] += d;
  for (auto& [c, d] : upd) pol.e[c] = expf(pol.w[c]);
}

static Result nrpa(int level, const S& root, Policy pol, std::mt19937& rng) {
  if (level == 0) return playout(root, pol, rng);
  Result best;
  for (int i = 0; i < 100 && !STOP; i++) {
    Result r = level == 1 ? playout(root, pol, rng) : nrpa(level - 1, root, pol, rng);
    if (r.score >= best.score) best = r;
    adapt(pol, root, best.seq, 1.0);
  }
  return best;
}

static string tour_text(const vector<Mv>& seq) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < seq.size(); i++) {
    int w = apply_move(s, seq[i].delta, seq[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(seq[i].q).c_str()); out += buf;
  }
  return out;
}

// parse "(t, sq), ..." lines; finished tours only unless partial
static vector<vector<Mv>> parse_tours(const char* fn, bool finishedOnly) {
  vector<vector<Mv>> out; FILE* f = fopen(fn, "r"); if (!f) return out;
  static char line[400000];
  while (fgets(line, sizeof line, f)) {
    vector<Mv> t; S s = start_state(); char* p = line; int w; char sq[8]; bool bad = false;
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
      if (q < 0 || q > 63) { bad = true; break; }
      t.push_back({d, q}); apply_move(s, d, q); p++;
      if (q == H8) break;
    }
    if (bad || t.empty()) continue;
    if (finishedOnly && t.back().q != H8) continue;
    out.push_back(t);
  }
  fclose(f); return out;
}

// ============================================================================================
// Workers
// ============================================================================================
static vector<vector<Mv>> TOURS; static vector<long long> TOUR_SCORE; static vector<double> TOUR_W;
static double P_LEVEL[4] = {0.2, 0.4, 0.39, 0.01};  // seed jobs: probability of NRPA level 0 / 1 / 2 / 3
static int LABEL_LEVEL = 1, LABEL_LEVEL_SHALLOW = 2, SHALLOW_DEPTH = 36;  // label jobs (deeper search near the start)
static int PROBES = 4, PROBE_STATES = 12;  // probe rollouts per probed state, probed states per path
static double PREFIX_RATE = 0.05, LABEL_PATH_RATE = 0.25, FRAC_LABEL = 0.75, ANCHOR_POW = 1.0;
static int NICE = 10;
static std::atomic<int> WORKERS_RUNNING{0};
static vector<std::thread> WTH;
static std::atomic<bool> WSTOP{false};

struct LJob { S s; int depth; };
static std::deque<LJob> LQ; static std::mutex LQ_M; static size_t LQ_CAP = 20000;
static vector<Sample> OUTQ; static std::mutex OUT_M; static size_t OUT_CAP = 2000000;
static std::atomic<long long> ST_PATHS{0}, ST_PATHS_FIN{0}, ST_LJOBS{0}, ST_LJOBS_FIN{0}, ST_SAMPLES{0}, ST_DROPPED{0};
static std::atomic<long long> ST_LEVEL_PATHS[4], ST_LEVEL_FIN[4];
static Policy* UNIFORM = nullptr;
static int32_t lt_update(const S& s, int32_t fut);  // best-future table (defined below)

static void emit(vector<Sample>& buf) {
  if (buf.empty()) return;
  while (!WSTOP) {
    {
      std::lock_guard<std::mutex> lk(OUT_M);
      if (OUTQ.size() + buf.size() <= OUT_CAP) { OUTQ.insert(OUTQ.end(), buf.begin(), buf.end()); ST_SAMPLES += buf.size(); buf.clear(); return; }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));  // Python is not draining: back off
  }
  buf.clear();
}

// label the states of a path (root + seq) by suffix maximum, with probe rollouts from a few states
// labels[i] = best final score known reachable from state i (0 if none)
static void label_path(const S& root, const vector<Mv>& seq, double ownScore, vector<S>& states, vector<float>& labels, std::mt19937& rng) {
  states.clear(); states.push_back(root);
  S s = root;
  for (const Mv& m : seq) { if (m.q == H8) break; apply_move(s, m.delta, m.q); states.push_back(s); }
  int n = (int)states.size();
  labels.assign(n, 0.f);
  vector<double> probe(n, 0.0);
  if (PROBES > 0 && n > 0) {
    for (int k = 0; k < PROBE_STATES; k++) {
      int i = std::uniform_int_distribution<int>(0, n - 1)(rng);
      for (int r = 0; r < PROBES; r++) { Result pr = playout(states[i], *UNIFORM, rng); if (pr.finished) probe[i] = max(probe[i], (double)pr.minutes); }
    }
  }
  double run = 0;
  for (int i = n - 1; i >= 0; i--) {
    double own = ownScore;  // the path itself finishes from every one of its states (0 if it does not finish)
    run = max(run, max(own, probe[i]));
    labels[i] = (float)run;
  }
}

static void worker_main(int tid, unsigned seed) {
  setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), NICE);
  std::mt19937 rng(seed * 7919u + tid * 104729u + 17);
  vector<Sample> buf; vector<S> states; vector<float> labels;
  std::uniform_real_distribution<double> U(0, 1);
  WORKERS_RUNNING++;
  while (!WSTOP) {
    bool haveLabel = false; LJob job;
    if (U(rng) < FRAC_LABEL) {
      std::lock_guard<std::mutex> lk(LQ_M);
      if (!LQ.empty()) { job = LQ.back(); LQ.pop_back(); haveLabel = true; }
    }
    if (haveLabel) {
      int L = job.depth < SHALLOW_DEPTH ? LABEL_LEVEL_SHALLOW : LABEL_LEVEL;
      Result r = nrpa(L, job.s, Policy(), rng);
      if (WSTOP) break;
      ST_LJOBS++; if (r.finished) ST_LJOBS_FIN++;
      label_path(job.s, r.seq, r.finished ? (double)r.minutes : 0.0, states, labels, rng);
      for (size_t i = 0; i < states.size(); i++) if (labels[i] > 0) lt_update(states[i], max<int32_t>(0, (int32_t)labels[i] - states[i].T));
      for (size_t i = 0; i < states.size(); i++) {
        if (i > 0 && U(rng) >= LABEL_PATH_RATE) continue;
        Sample o; to_sample(states[i], job.depth + (int)i, i == 0 ? 2 : 3, labels[i], o); buf.push_back(o);
      }
      emit(buf);
      continue;
    }
    if (TOURS.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
    // seed job
    std::discrete_distribution<size_t> pickT(TOUR_W.begin(), TOUR_W.end());
    size_t ti = pickT(rng);
    const vector<Mv>& tour = TOURS[ti];
    int len = (int)tour.size();
    int k = (int)floor(pow(U(rng), ANCHOR_POW) * (len - 1));  // anchor: keep the first k moves (< len: never the finish)
    if (k > len - 1) k = len - 1;
    double u = U(rng) * (P_LEVEL[0] + P_LEVEL[1] + P_LEVEL[2] + P_LEVEL[3]); int L = 0;
    while (L < 3 && u >= P_LEVEL[L]) { u -= P_LEVEL[L]; L++; }
    S root = start_state(); vector<S> pre; pre.push_back(root);
    for (int i = 0; i < k; i++) { apply_move(root, tour[i].delta, tour[i].q); pre.push_back(root); }
    Result r = nrpa(L, root, Policy(), rng);
    if (WSTOP) break;
    ST_PATHS++; ST_LEVEL_PATHS[L]++; if (r.finished) { ST_PATHS_FIN++; ST_LEVEL_FIN[L]++; }
    label_path(root, r.seq, r.finished ? (double)r.minutes : 0.0, states, labels, rng);
    float preLabel = max((float)TOUR_SCORE[ti], labels.empty() ? 0.f : labels[0]);
    for (int i = 0; i < k; i++) lt_update(pre[i], max<int32_t>(0, (int32_t)preLabel - pre[i].T));
    for (size_t i = 0; i < states.size(); i++) if (labels[i] > 0) lt_update(states[i], max<int32_t>(0, (int32_t)labels[i] - states[i].T));
    for (int i = 0; i < k; i++) {
      if (U(rng) >= PREFIX_RATE) continue;
      Sample o; to_sample(pre[i], i, 0, preLabel, o); buf.push_back(o);
    }
    for (size_t i = 0; i < states.size(); i++) {
      float lab = labels[i]; if (i == 0) lab = max(lab, preLabel);
      Sample o; to_sample(states[i], k + (int)i, 1, lab, o); buf.push_back(o);
    }
    emit(buf);
  }
  WORKERS_RUNNING--;
}

// ============================================================================================
// A* engine
// ============================================================================================
static const uint32_t NIL = 0xFFFFFFFFu;
struct Node { uint32_t parent; uint16_t delta; uint8_t q; uint8_t flags; float best; };
static_assert(sizeof(Node) == 12, "Node layout");
// open-list entry: f = fraw - novelty penalty; fraw = network value (+ noise, depth bonus);
// a1, a2 = the node's ancestors NOV_K1 and NOV_K2 moves up (subtrees whose expansions are counted)
struct HE { float f; uint32_t id; float fraw; uint32_t a1, a2; };
static inline bool operator<(const HE& a, const HE& b) { return a.f < b.f; }
static Node* NODES = nullptr; static uint64_t NNODE = 0, MAXN = 0;
static vector<HE> HEAP; static uint64_t MAXOPEN = 0; static double PRUNE_KEEP = 0.75;
static S ROOT; static int ROOT_DEPTH = 0; static vector<Mv> ROOT_PREFIX;
#pragma pack(push, 1)
struct DE { uint64_t k; int32_t T; };
#pragma pack(pop)
static DE* DD = nullptr; static uint64_t DMASK = 0, DCOUNT = 0, DCAP = 0;
struct Pending { uint32_t parent; uint16_t delta; uint8_t q; uint32_t a1, a2; };
static vector<Pending> PEND[2];
static double P_LABEL = 0.05;
static long long BEST_SCORE = -1; static vector<Mv> BEST_SEQ; static long long POOL_MIN = 7500;
static std::set<long long> SEEN_SCORES; static vector<pair<long long, string>> FOUND;
static vector<Sample> TREE_OUT;  // A* finished paths (labelled with node.best), drained by Python
static long long A_EXP = 0, A_CHILD = 0, A_FIN = 0, A_DEAD = 0, A_DUP = 0, A_STALE = 0, A_GC = 0, A_PRUNE = 0, A_MAXDEPTH = 0, A_DDCLEAR = 0;
static std::mt19937_64 ARNG(12345);
static vector<pair<uint32_t, float>> RESC;  // rescoring: (heap index, old fraw)

// Novelty: every expansion is counted against the node's ancestors NOV_K1 and NOV_K2 moves up, and an
// entry's priority drops by NOVELTY_W * (log2(1 + count1) + log2(1 + count2)). Heavily searched subtrees
// sink, so the search spreads out. Counts only grow, so stale priorities are fixed lazily when popped.
static double NOVELTY_W = 0; static int NOV_K1 = 8, NOV_K2 = 32; static const int CNT_LOG2 = 25;
static vector<uint32_t> CNT1, CNT2; static long long A_LAZY = 0;
static inline uint32_t cb(uint32_t id) { return (id * 2654435761u) >> (32 - CNT_LOG2); }
static inline float novelty_pen(const HE& e) {
  if (NOVELTY_W <= 0 || CNT1.empty()) return 0.f;
  return (float)(NOVELTY_W * (log2f(1.f + (float)CNT1[cb(e.a1)]) + log2f(1.f + (float)CNT2[cb(e.a2)])));
}
static void clear_counts() {
  if (NOVELTY_W > 0) { CNT1.assign(1u << CNT_LOG2, 0); CNT2.assign(1u << CNT_LOG2, 0); }
  else { vector<uint32_t>().swap(CNT1); vector<uint32_t>().swap(CNT2); }
}

static inline uint64_t skey(const S& s) { uint64_t h = hashS(s, true); return h ? h : 1; }
// dedup lookup: returns stored T or INT32_MIN
static inline int32_t dd_get(uint64_t k) {
  uint64_t i = k & DMASK;
  while (true) { if (DD[i].k == 0) return INT32_MIN; if (DD[i].k == k) return DD[i].T; i = (i + 1) & DMASK; }
}
static inline void dd_put(uint64_t k, int32_t T) {
  if (DCOUNT * 10 >= DCAP * 7) { memset(DD, 0, sizeof(DE) * DCAP); DCOUNT = 0; A_DDCLEAR++; }  // full: forget (dedup is only an optimisation)
  uint64_t i = k & DMASK;
  while (true) {
    if (DD[i].k == 0) { DD[i].k = k; DD[i].T = T; DCOUNT++; return; }
    if (DD[i].k == k) { if (T > DD[i].T) DD[i].T = T; return; }
    i = (i + 1) & DMASK;
  }
}

// rebuild the state of node id; returns depth (moves from a1); fills path (moves from the root) if given
// chain (if given) receives the node ids from id up to (not including) the root
static int rebuild(uint32_t id, S& s, vector<Mv>* path = nullptr, vector<uint32_t>* chain = nullptr) {
  static thread_local vector<Mv> mv; mv.clear();
  if (chain) chain->clear();
  for (uint32_t c = id; c != 0; c = NODES[c].parent) { mv.push_back({NODES[c].delta, NODES[c].q}); if (chain) chain->push_back(c); }
  s = ROOT;
  for (size_t i = mv.size(); i-- > 0;) apply_move(s, mv[i].delta, mv[i].q);
  if (path) { path->assign(mv.rbegin(), mv.rend()); }
  return ROOT_DEPTH + (int)mv.size();
}

static void gc() {
  uint64_t nw = (NNODE + 63) / 64;
  vector<uint64_t> mark(nw, 0);
  auto markup = [&](uint32_t id) {
    while (id != NIL && !((mark[id >> 6] >> (id & 63)) & 1)) { mark[id >> 6] |= 1ULL << (id & 63); id = NODES[id].parent; }
  };
  markup(0);
  for (auto& h : HEAP) markup(h.id);
  for (int sl = 0; sl < 2; sl++) for (auto& p : PEND[sl]) markup(p.parent);
  vector<uint32_t> rk(nw + 1, 0);
  for (uint64_t w = 0; w < nw; w++) rk[w + 1] = rk[w] + (uint32_t)__builtin_popcountll(mark[w]);
  auto rank = [&](uint32_t id) { return rk[id >> 6] + (uint32_t)__builtin_popcountll(mark[id >> 6] & ((1ULL << (id & 63)) - 1)); };
  for (uint64_t i = 0; i < NNODE; i++) {
    if (!((mark[i >> 6] >> (i & 63)) & 1)) continue;
    Node nd = NODES[i]; if (nd.parent != NIL) nd.parent = rank(nd.parent);
    NODES[rank((uint32_t)i)] = nd;
  }
  NNODE = rk[nw];
  for (auto& h : HEAP) { h.id = rank(h.id); h.a1 = rank(h.a1); h.a2 = rank(h.a2); }  // ancestors are always kept
  for (int sl = 0; sl < 2; sl++) for (auto& p : PEND[sl]) { p.parent = rank(p.parent); p.a1 = rank(p.a1); p.a2 = rank(p.a2); }
  RESC.clear();
  clear_counts();  // node ids changed
  A_GC++;
}

static void prune_heap(uint64_t keep) {
  if (HEAP.size() <= keep) return;
  std::nth_element(HEAP.begin(), HEAP.begin() + keep, HEAP.end(), [](const HE& a, const HE& b) { return a.f > b.f; });
  HEAP.resize(keep); std::make_heap(HEAP.begin(), HEAP.end());
  RESC.clear();
  A_PRUNE++;
}

static void record_finish(uint32_t parentId, const S& ps, int delta, long long score) {
  A_FIN++;
  for (uint32_t c = parentId; c != NIL; c = NODES[c].parent) { if (NODES[c].best >= (float)score) break; NODES[c].best = (float)score; }
  bool better = score > BEST_SCORE;
  bool pool = score >= POOL_MIN && !SEEN_SCORES.count(score);
  if (!better && !pool) return;
  vector<Mv> path; S s; rebuild(parentId, s, &path);
  vector<Mv> full = ROOT_PREFIX; full.insert(full.end(), path.begin(), path.end()); full.push_back({delta, H8});
  if (better) { BEST_SCORE = score; BEST_SEQ = full; }
  if (score >= POOL_MIN) { SEEN_SCORES.insert(score); FOUND.push_back({score, tour_text(full)}); }
  // training samples along the path: label = best finished score below each node
  vector<uint32_t> ids; for (uint32_t c = parentId; c != NIL; c = NODES[c].parent) ids.push_back(c);
  S st = ROOT; int d = ROOT_DEPTH;
  for (size_t j = ids.size(); j-- > 0;) {
    uint32_t c = ids[j];
    if (c != 0) { apply_move(st, NODES[c].delta, NODES[c].q); d++; }
    Sample o; to_sample(st, d, 4, max(NODES[c].best, (float)score), o);
    if (TREE_OUT.size() < 4000000) TREE_OUT.push_back(o);
  }
  (void)ps;
}

extern "C" {

void km_init(unsigned seed) {
  init_board(); D0 = 1; D0 = h8dist(start_state());
  for (int p = 0; p < 64; p++) { for (int q = 0; q < 64; q++) KIDX[p][q] = 0; for (int k = 0; k < NBN[p]; k++) KIDX[p][NBQ[p][k]] = k; }
  if (!UNIFORM) UNIFORM = new Policy();
  ARNG.seed(seed);
}

// generic parameter setter (returns 0 if unknown)
int km_set(const char* name, double v) {
  string n = name;
  if (n == "p_level0") P_LEVEL[0] = v; else if (n == "p_level1") P_LEVEL[1] = v; else if (n == "p_level2") P_LEVEL[2] = v;
  else if (n == "p_level3") P_LEVEL[3] = v;
  else if (n == "label_level") LABEL_LEVEL = (int)v; else if (n == "label_level_shallow") LABEL_LEVEL_SHALLOW = (int)v;
  else if (n == "shallow_depth") SHALLOW_DEPTH = (int)v;
  else if (n == "probes") PROBES = (int)v; else if (n == "probe_states") PROBE_STATES = (int)v;
  else if (n == "prefix_rate") PREFIX_RATE = v; else if (n == "label_path_rate") LABEL_PATH_RATE = v;
  else if (n == "frac_label") FRAC_LABEL = v; else if (n == "anchor_pow") ANCHOR_POW = v;
  else if (n == "nice") NICE = (int)v; else if (n == "lq_cap") LQ_CAP = (size_t)v; else if (n == "out_cap") OUT_CAP = (size_t)v;
  else if (n == "novelty_w") NOVELTY_W = v; else if (n == "novelty_k1") NOV_K1 = max(1, (int)v); else if (n == "novelty_k2") NOV_K2 = max(1, (int)v);
  else if (n == "p_label") P_LABEL = v; else if (n == "pool_min") POOL_MIN = (long long)v; else if (n == "prune_keep") PRUNE_KEEP = v;
  else return 0;
  return 1;
}

// load finished tours from a file (weight w per tour); returns the number loaded
int km_load_tours(const char* fn, double w) {
  auto v = parse_tours(fn, true); int n = 0;
  for (auto& t : v) {
    S s = start_state(); for (auto& m : t) apply_move(s, m.delta, m.q);
    TOURS.push_back(t); TOUR_SCORE.push_back(s.T); TOUR_W.push_back(w); n++;
  }
  return n;
}
int km_num_tours() { return (int)TOURS.size(); }

// samples for every state of every loaded tour (label = the tour's score); returns count written
long long km_tour_samples(Sample* out, long long cap, long long min_score) {
  long long n = 0;
  for (size_t t = 0; t < TOURS.size(); t++) {
    if (TOUR_SCORE[t] < min_score) continue;
    S s = start_state();
    for (size_t i = 0; i < TOURS[t].size() && n < cap; i++) {
      Sample o; to_sample(s, (int)i, 5, (float)TOUR_SCORE[t], o); out[n++] = o;
      apply_move(s, TOURS[t][i].delta, TOURS[t][i].q);
    }
  }
  return n;
}

void km_workers_start(int threads, unsigned seed) {
  WSTOP = false; STOP = false;
  for (int t = 0; t < threads; t++) WTH.emplace_back(worker_main, t, seed);
}
void km_workers_stop() {
  WSTOP = true; STOP = true;
  for (auto& t : WTH) t.join();
  WTH.clear(); STOP = false;
}
// move up to cap samples into out; returns count
long long km_drain(Sample* out, long long cap) {
  std::lock_guard<std::mutex> lk(OUT_M);
  long long n = min<long long>(cap, (long long)OUTQ.size());
  if (n <= 0) return 0;
  memcpy(out, OUTQ.data() + (OUTQ.size() - n), sizeof(Sample) * n);
  OUTQ.resize(OUTQ.size() - n);
  return n;
}
// stats: paths, finished paths, label jobs, finished label jobs, samples, rollouts, queued samples, label queue,
// then paths and finished paths per level 0..3
void km_worker_stats(double* o) {
  o[0] = ST_PATHS; o[1] = ST_PATHS_FIN; o[2] = ST_LJOBS; o[3] = ST_LJOBS_FIN; o[4] = ST_SAMPLES; o[5] = ROLLOUTS;
  { std::lock_guard<std::mutex> lk(OUT_M); o[6] = OUTQ.size(); }
  { std::lock_guard<std::mutex> lk(LQ_M); o[7] = LQ.size(); }
  for (int l = 0; l < 4; l++) { o[8 + 2 * l] = ST_LEVEL_PATHS[l]; o[9 + 2 * l] = ST_LEVEL_FIN[l]; }
}

// ---------------- A* ----------------
// start a search from the state after the first `prefix_len` moves of tour line `tour_idx` of file fn
// (fn may be empty: start at a1). max_nodes: node table size; max_open: open-list cap; dedup_log2: log2 of
// the duplicate table's slot count. Returns 0 on success.
int km_astar_init(const char* fn, int prefix_len, unsigned long long max_nodes, unsigned long long max_open, int dedup_log2) {
  if (NODES) { free(NODES); NODES = nullptr; }
  if (DD) { free(DD); DD = nullptr; }
  MAXN = max_nodes; MAXOPEN = max_open;
  NODES = (Node*)malloc(sizeof(Node) * MAXN); if (!NODES) return 1;
  DCAP = 1ULL << dedup_log2; DMASK = DCAP - 1; DCOUNT = 0;
  DD = (DE*)calloc(DCAP, sizeof(DE)); if (!DD) return 2;
  ROOT = start_state(); ROOT_PREFIX.clear(); ROOT_DEPTH = 0;
  if (fn && fn[0] && prefix_len > 0) {
    auto v = parse_tours(fn, false); if (v.empty()) return 3;
    for (int i = 0; i < prefix_len && i < (int)v[0].size(); i++) {
      if (v[0][i].q == H8) break;
      apply_move(ROOT, v[0][i].delta, v[0][i].q); ROOT_PREFIX.push_back(v[0][i]);
    }
    ROOT_DEPTH = (int)ROOT_PREFIX.size();
  }
  NNODE = 1; NODES[0] = {NIL, 0, 0, 0, -1.f};
  HEAP.clear(); HEAP.push_back({1e9f, 0, 1e9f, 0, 0});
  PEND[0].clear(); PEND[1].clear(); RESC.clear();
  clear_counts(); A_LAZY = 0;
  dd_put(skey(ROOT), ROOT.T);
  BEST_SCORE = -1; BEST_SEQ.clear(); SEEN_SCORES.clear(); FOUND.clear(); TREE_OUT.clear();
  A_EXP = A_CHILD = A_FIN = A_DEAD = A_DUP = A_STALE = A_GC = A_PRUNE = A_MAXDEPTH = A_DDCLEAR = 0;
  return 0;
}

// pop up to B nodes, expand them on `threads` threads, write the children to out (cap entries) and keep
// them pending in slot (0/1) until km_astar_push. Returns the number of children.
long long km_astar_expand(int slot, int B, int threads, Sample* out, long long cap) {
  PEND[slot].clear();
  vector<uint32_t> pop;
  while ((int)pop.size() < B && !HEAP.empty()) {
    std::pop_heap(HEAP.begin(), HEAP.end()); HE e = HEAP.back(); HEAP.pop_back();
    if (NOVELTY_W > 0 && !CNT1.empty()) {
      float fn = e.fraw - novelty_pen(e);
      if (fn < e.f - 0.5f && !HEAP.empty() && fn < HEAP.front().f) {  // its subtree got busier: re-queue
        e.f = fn; HEAP.push_back(e); std::push_heap(HEAP.begin(), HEAP.end()); A_LAZY++; continue;
      }
      CNT1[cb(e.a1)]++; CNT2[cb(e.a2)]++;
    }
    pop.push_back(e.id);
  }
  if (pop.empty()) return 0;
  RESC.clear();
  struct Ch { uint32_t parent; uint16_t delta; uint8_t q; uint32_t a1, a2; uint64_t key; Sample smp; };
  struct Fin { uint32_t parent; int delta; long long score; };
  int nt = max(1, min(threads, (int)pop.size() / 8 + 1));
  vector<vector<Ch>> lch(nt); vector<vector<Fin>> lfin(nt); vector<vector<LJob>> llab(nt);
  vector<long long> ldead(nt, 0), lstale(nt, 0), lmaxd(nt, 0);
  vector<uint64_t> seeds(nt); for (auto& x : seeds) x = ARNG();
  auto work = [&](int t) {
    std::mt19937_64 rng(seeds[t]);
    std::uniform_real_distribution<double> U(0, 1);
    vector<pair<int, int>> mv, mv2; vector<uint32_t> chain;
    for (size_t j = t; j < pop.size(); j += nt) {
      uint32_t id = pop[j]; S s; int depth = rebuild(id, s, nullptr, &chain);
      // the children's ancestors k moves up = this node's ancestors k-1 moves up (chain[0] is this node)
      uint32_t ca1 = (size_t)(NOV_K1 - 1) < chain.size() ? chain[NOV_K1 - 1] : 0;
      uint32_t ca2 = (size_t)(NOV_K2 - 1) < chain.size() ? chain[NOV_K2 - 1] : 0;
      if (id != 0 && dd_get(skey(s)) > s.T) { lstale[t]++; continue; }  // reached again with more minutes
      lmaxd[t] = max<long long>(lmaxd[t], depth);
      if (U(rng) < P_LABEL) llab[t].push_back({s, depth});
      pinned_moves(s, 3, true, mv);
      for (auto& [d, q] : mv) {
        if (q == H8) { long long sc = s.T + (long long)d * (d ? group_n(s, s.pos) : 0); lfin[t].push_back({id, d, sc}); continue; }
        S c = s; apply_move(c, d, q);
        pinned_moves(c, 3, true, mv2);
        if (mv2.empty()) { ldead[t]++; continue; }
        Ch ch; ch.parent = id; ch.delta = (uint16_t)d; ch.q = (uint8_t)q; ch.a1 = ca1; ch.a2 = ca2; ch.key = skey(c);
        to_sample(c, depth + 1, 4, 0.f, ch.smp);
        lch[t].push_back(ch);
      }
    }
  };
  if (nt == 1) work(0);
  else { vector<std::thread> th; for (int t = 0; t < nt; t++) th.emplace_back(work, t); for (auto& x : th) x.join(); }
  A_EXP += (long long)pop.size();
  long long n = 0;
  for (int t = 0; t < nt; t++) {
    A_DEAD += ldead[t]; A_STALE += lstale[t]; A_MAXDEPTH = max(A_MAXDEPTH, lmaxd[t]);
    for (auto& f : lfin[t]) { S dummy; record_finish(f.parent, dummy, f.delta, f.score); }
    for (auto& c : lch[t]) {
      int32_t old = dd_get(c.key);
      if (old != INT32_MIN && old >= c.smp.T) { A_DUP++; continue; }
      if (n >= cap) break;
      dd_put(c.key, c.smp.T);
      out[n++] = c.smp; PEND[slot].push_back({c.parent, c.delta, c.q, c.a1, c.a2});
    }
  }
  A_CHILD += n;
  if (P_LABEL > 0) {
    std::lock_guard<std::mutex> lk(LQ_M);
    for (int t = 0; t < nt; t++) for (auto& j : llab[t]) { LQ.push_back(j); if (LQ.size() > LQ_CAP) LQ.pop_front(); }
  }
  return n;
}

// push the pending children of slot with their f values
void km_astar_push(int slot, const float* f, long long n) {
  vector<Pending>& P = PEND[slot];
  n = min<long long>(n, (long long)P.size());
  if (NNODE + (uint64_t)n + 16 > MAXN) {
    gc();
    if (NNODE + (uint64_t)n + 16 > MAXN * 0.9) { prune_heap((uint64_t)(HEAP.size() * 0.5)); gc(); }
  }
  for (long long i = 0; i < n; i++) {
    if (NNODE >= MAXN) break;
    uint32_t id = (uint32_t)NNODE++;
    NODES[id] = {P[i].parent, P[i].delta, P[i].q, 0, -1.f};
    HE e{f[i], id, f[i], P[i].a1, P[i].a2}; e.f = f[i] - novelty_pen(e);
    HEAP.push_back(e); std::push_heap(HEAP.begin(), HEAP.end());
  }
  P.clear();
  if (HEAP.size() > MAXOPEN) prune_heap((uint64_t)(MAXOPEN * PRUNE_KEEP));
}

// stats: expansions, children, finishes, dead, dup, stale, gc, prune, nodes, open, best, maxdepth, dedup
// entries, dedup clears, best f in the open list
void km_astar_stats(double* o) {
  o[0] = A_EXP; o[1] = A_CHILD; o[2] = A_FIN; o[3] = A_DEAD; o[4] = A_DUP; o[5] = A_STALE; o[6] = A_GC; o[7] = A_PRUNE;
  o[8] = NNODE; o[9] = HEAP.size(); o[10] = BEST_SCORE; o[11] = A_MAXDEPTH; o[12] = DCOUNT; o[13] = A_DDCLEAR;
  o[14] = HEAP.empty() ? 0 : HEAP.front().f; o[15] = A_LAZY;
}
int km_astar_best(char* buf, int cap) {
  string t = BEST_SEQ.empty() ? "" : tour_text(BEST_SEQ);
  snprintf(buf, cap, "%s", t.c_str()); return (int)t.size();
}
// finished tours >= pool_min found since the last call, as "score\ttour\n" lines; returns bytes needed
long long km_astar_found(char* buf, long long cap) {
  string s; for (auto& [sc, t] : FOUND) { s += to_string(sc); s += '\t'; s += t; s += '\n'; }
  if ((long long)s.size() + 1 > cap) return (long long)s.size() + 1;
  memcpy(buf, s.c_str(), s.size() + 1); FOUND.clear(); return (long long)s.size();
}
long long km_astar_tree_samples(Sample* out, long long cap) {
  long long n = min<long long>(cap, (long long)TREE_OUT.size());
  memcpy(out, TREE_OUT.data(), sizeof(Sample) * n);
  TREE_OUT.erase(TREE_OUT.begin(), TREE_OUT.begin() + n);
  return n;
}
// harvest: up to k random tree nodes with a finished descendant, labelled with that best score
long long km_astar_harvest(Sample* out, long long k, int tries) {
  long long n = 0;
  for (int t = 0; t < tries && n < k && NNODE > 1; t++) {
    uint32_t id = (uint32_t)(ARNG() % NNODE);
    if (NODES[id].best <= 0) continue;
    S s; int d = rebuild(id, s); to_sample(s, d, 4, NODES[id].best, out[n++]);
  }
  return n;
}

// rescoring after a network update: export the top k open entries (states), then import new f values;
// the other entries are shifted by the median change
long long km_astar_rescore_export(long long k, int threads, Sample* out) {
  RESC.clear();
  if (HEAP.empty()) return 0;
  k = min<long long>(k, (long long)HEAP.size());
  std::sort(HEAP.begin(), HEAP.end(), [](const HE& a, const HE& b) { return a.f > b.f; });  // a sorted array is a valid heap
  for (long long i = 0; i < k; i++) RESC.push_back({(uint32_t)i, HEAP[i].fraw});
  int nt = max(1, threads);
  auto work = [&](int t) {
    for (long long i = t; i < k; i += nt) { S s; int d = rebuild(HEAP[i].id, s); to_sample(s, d, 4, 0.f, out[i]); }
  };
  vector<std::thread> th; for (int t = 0; t < nt; t++) th.emplace_back(work, t); for (auto& x : th) x.join();
  return k;
}
double km_astar_rescore_import(const float* f, long long n) {
  if (RESC.empty() || n != (long long)RESC.size()) return 0;
  vector<float> dl; dl.reserve(n);
  for (long long i = 0; i < n; i++) {
    HE& e = HEAP[RESC[i].first];
    dl.push_back(f[i] - RESC[i].second); e.fraw = f[i]; e.f = f[i] - novelty_pen(e);
  }
  // shift the rest by the median change among the lowest-scored quarter of the re-scored entries
  // (the entries closest to the ones that were not re-scored)
  vector<float> tail(dl.begin() + (dl.size() * 3) / 4, dl.end());
  if (tail.empty()) tail = dl;
  std::nth_element(tail.begin(), tail.begin() + tail.size() / 2, tail.end());
  float shift = tail[tail.size() / 2];
  for (size_t i = (size_t)n; i < HEAP.size(); i++) { HEAP[i].f += shift; HEAP[i].fraw += shift; }
  std::make_heap(HEAP.begin(), HEAP.end());
  RESC.clear();
  return shift;
}

}  // extern "C"

// global best-future table: for every position (altitudes, visits, knight; minutes so far excluded) the most
// future minutes known to be reachable from it. A position's label is then T + that maximum, whichever path
// reached it and whenever the better continuation was found. Workers record every position of every finished
// path; km_label_merge records samples and raises their labels; km_label_lookup only raises labels.
static DE* LT = nullptr; static uint64_t LTMASK = 0, LTCOUNT = 0, LTCAP = 0; static std::mutex LT_M;
static long long LT_CLEARS = 0;
static inline uint64_t lt_key(const S& s) { uint64_t k = hashS(s, true) ^ 0x5bd1e9955bd1e995ULL; return k ? k : 1; }
// fut < 0: lookup only. Returns the best known future for s (or -1).
static int32_t lt_update(const S& s, int32_t fut) {
  if (!LT) return -1;
  uint64_t k = lt_key(s);
  std::lock_guard<std::mutex> lk(LT_M);
  if (fut >= 0 && LTCOUNT * 10 >= LTCAP * 7) { memset(LT, 0, sizeof(DE) * LTCAP); LTCOUNT = 0; LT_CLEARS++; }
  uint64_t i = k & LTMASK;
  while (true) {
    if (LT[i].k == 0) { if (fut >= 0) { LT[i].k = k; LT[i].T = fut; LTCOUNT++; } return fut; }
    if (LT[i].k == k) { if (fut > LT[i].T) LT[i].T = fut; return LT[i].T; }
    i = (i + 1) & LTMASK;
  }
}
static inline void sample_state(const Sample& a, S& s) {
  memcpy(s.alt, a.alt, sizeof s.alt); memcpy(s.vis, a.vis, sizeof s.vis); s.pos = a.pos; s.T = a.T;
}
static long long label_pass(Sample* a, long long n, bool record) {
  long long raised = 0;
  for (long long j = 0; j < n; j++) {
    S s; sample_state(a[j], s);
    int32_t fut = (record && a[j].label > 0) ? max<int32_t>(0, (int32_t)lrintf(a[j].label) - a[j].T) : -1;
    int32_t best = lt_update(s, fut);
    if (best >= 0 && (float)(a[j].T + best) > a[j].label) { a[j].label = (float)(a[j].T + best); raised++; }
  }
  return raised;
}
extern "C" {
void km_label_init(int log2slots) {
  std::lock_guard<std::mutex> lk(LT_M);
  if (LT) free(LT);
  LTCAP = 1ULL << log2slots; LTMASK = LTCAP - 1; LTCOUNT = 0;
  LT = (DE*)calloc(LTCAP, sizeof(DE));
}
long long km_label_merge(Sample* a, long long n) { return label_pass(a, n, true); }
long long km_label_lookup(Sample* a, long long n) { return label_pass(a, n, false); }
void km_label_stats(double* o) { o[0] = (double)LTCOUNT; o[1] = (double)LTCAP; o[2] = (double)LT_CLEARS; }

// play a tour text (for checking): returns minutes, or -1 if illegal under the engine's rules
long long km_tour_minutes(const char* text) {
  S s = start_state(); const char* p = text; int w; char sq[8];
  vector<pair<int, int>> mv;
  while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
    int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
    pinned_moves(s, 3, true, mv); bool ok = false;
    for (auto& [dd, qq] : mv) if (dd == d && qq == q) ok = true;
    if (!ok) return -1;
    apply_move(s, d, q); p++;
    if (q == H8) return s.T;
  }
  return -1;
}

}  // extern "C"
