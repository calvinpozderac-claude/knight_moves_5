// Go-Explore / MAP-Elites quality-diversity search for the swamp-knight puzzle (Knight Moves 5).
//
// Archive: an open-addressing table of cells keyed by a coarse state descriptor
//   (stage, knight altitude relative to h8, move-count bucket, size of h8's group, visits left,
//    opposite pairs still able to sit, dance flag [part of stage]).
// Each cell keeps its elite: the path (from a1) to the best state seen with that descriptor
// (fitness = minutes T, or with --fit pot an idealised-dance estimate for dance states), plus
// `val` = best finished tour known to pass through the cell, and selection counters.
// Loop (each worker thread): pick a cell (weight = novelty (rarely chosen / recently improved) x
// exp((val - best)/TEMP)), restore its state by replaying its path, explore with a batch of random
// playouts or one NRPA level-L search (default level 1), offer every visited state to the archive,
// and record every finished tour (pool, best, values along the path).
// Seeds: all tours in the --seed files (default: data/tours records + curriculum.txt).
// Checkpoint/resume: --ckpt FILE (written every --ckpt-every s and at exit), --resume FILE.
//
// build: g++ -O3 -march=native -std=c++17 -pthread -o goexplore goexplore.cpp
// run from the repo root (seeds and the checker are found by relative path); see RUN.md.
#include "../../src/eng.h"
#include <atomic>
#include <chrono>
#include <climits>
#include <csignal>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

struct Mv { int delta, q; };
static inline uint16_t enc(Mv m) { return (uint16_t)(m.q | (m.delta << 6)); }
static inline Mv dec(uint16_t x) { return {x >> 6, x & 63}; }
typedef vector<uint16_t> Path;

// ---------------- options
static int MB = 4, GB = 3, VB = 6, PB = 2, DMIN = 40, FITMODE = 0;
static int ITERS = 100, NLEVEL = 1, BATCH = 32, XSTEPS = 0, POOLMIN = 7500;
static double ALPHA = 1.0, BIAS = 0, PNRPA = 0.5, TEMP = 200, UNK = 400, RB = 1.0;
static std::atomic<bool> STOP{false};
static std::atomic<long long> PLAYOUTS{0}, FINISHES{0}, IMPROVES{0};

// ---------------- NRPA policy (same coding as src/nrpa.cpp)
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
static inline double move_e(const Policy& pol, const S& s, int d, int q) {
  double x = pol.e[code_of(s, d, q)];
  if (BIAS != 0 && d) x *= exp(BIAS * d * group_n(s, s.pos) / 100.0);
  return x;
}

// ---------------- descriptor
// idealised dance estimate (copied from src/nrpa.cpp dance_pot), only meaningful for dance states
static double dance_pot(const S& s) {
  int a = s.alt[s.pos], nG[2] = {0, 0};
  for (int i = 0; i < 64; i++) { if (s.alt[i] == a) nG[0]++; else if (s.alt[i] == a - 2) nG[1]++; }
  if (nG[1] == 0) return -1e18;
  int ah = s.alt[H8]; if (ah != a && ah != a - 2) return -1e18;
  int grp[64], cap[64]; bool alive[64], open_[64];
  for (int i = 0; i < 64; i++) {
    alive[i] = s.alt[i] == a || s.alt[i] == a - 2; grp[i] = s.alt[i] == a ? 0 : 1;
    cap[i] = alive[i] ? 3 - s.vis[i] + (i == s.pos) : 0;
  }
  cap[H8] = cap[A1] = 0;
  for (int i = 0; i < 64; i++) open_[i] = alive[i] && !alive[63 - i];
  double fut = 0; int g = 0;
  for (int step = 0; step < 200; step++) {
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
  return s.T + fut;
}

struct Desc { uint64_t key; int fit; int stage, rel, gh, vl, pairs, nUp, nLo; };
static inline Desc describe(const S& s, int m) {
  Desc d;
  int ah = s.alt[H8], a = s.alt[s.pos];
  int gh = 0, vl = 0, nUp = 0, nLo = 0;
  for (int i = 0; i < 64; i++) {
    int x = s.alt[i];
    gh += x == ah; nUp += x == a; nLo += x == a - 2;
    if (i != H8 && s.vis[i] < 3) vl += 3 - s.vis[i];
  }
  int pairs = 0;  // opposite pairs (excluding a1/h8) with a member that can still be visited inside the dance band
  for (int i = 1; i < 32; i++) {
    int j = 63 - i;
    if ((s.vis[i] < 3 && abs(s.alt[i] - ah) <= 2) || (s.vis[j] < 3 && abs(s.alt[j] - ah) <= 2)) pairs++;
  }
  bool dance = (ah == a || ah == a - 2) && nLo > 0 && nUp + nLo >= DMIN;
  int stage = ah >= 17 ? 0 : dance ? 2 : 1;
  int rel = max(-8, min(8, a - ah)) + 8;
  uint64_t mb = min(m / MB, 63), gb = min(gh / GB, 31), vb = min(vl / VB, 31), pb = min(pairs / PB, 31);
  d.key = (1ULL << 63) | (uint64_t)stage | ((uint64_t)rel << 2) | (mb << 7) | (gb << 13) | (vb << 18) | (pb << 23);
  d.fit = (FITMODE == 1 && dance) ? (int)dance_pot(s) : s.T;
  d.stage = stage; d.rel = rel - 8; d.gh = gh; d.vl = vl; d.pairs = pairs; d.nUp = nUp; d.nLo = nLo;
  return d;
}

// ---------------- archive
struct Cell {
  std::atomic<uint64_t> key{0};
  std::atomic<int32_t> fit{INT_MIN}, val{0};
  std::atomic<uint32_t> chosen{0}, sinceImp{0};
  std::atomic<uint8_t> ready{0}, dead{0};
  Path path;  // guarded by the stripe lock
};
static inline uint64_t mix64(uint64_t x) {
  x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33; return x;
}
static inline void amax(std::atomic<int32_t>& a, int32_t v) {
  int32_t c = a.load(std::memory_order_relaxed);
  while (v > c && !a.compare_exchange_weak(c, v)) {}
}
struct Archive {
  size_t N, mask, maxCells; unique_ptr<Cell[]> c; std::atomic<size_t> count{0};
  static const int NSTRIPE = 4096; std::mutex stripe[NSTRIPE];
  explicit Archive(int log2) : N(1ULL << log2), mask(N - 1), maxCells(N * 7 / 10), c(new Cell[N]) {}
  long find(uint64_t key, bool create) {
    size_t i = mix64(key) & mask;
    for (size_t n = 0; n < N; n++, i = (i + 1) & mask) {
      uint64_t k = c[i].key.load(std::memory_order_acquire);
      if (k == key) return (long)i;
      if (k == 0) {
        if (!create || count.load(std::memory_order_relaxed) >= maxCells) return -1;
        uint64_t z = 0;
        if (c[i].key.compare_exchange_strong(z, key)) { count++; return (long)i; }
        if (z == key) return (long)i;
      }
    }
    return -1;
  }
  // offer a state reached by pre + seq[0..len); returns true if the cell improved
  bool offer(long slot, int fit, const Path& pre, const vector<Mv>* seq, size_t len) {
    Cell& x = c[slot];
    if (fit <= x.fit.load(std::memory_order_relaxed)) return false;
    std::lock_guard<std::mutex> lk(stripe[slot % NSTRIPE]);
    if (fit <= x.fit.load()) return false;
    x.path.resize(pre.size() + len);
    std::copy(pre.begin(), pre.end(), x.path.begin());
    for (size_t i = 0; i < len; i++) x.path[pre.size() + i] = enc((*seq)[i]);
    x.fit.store(fit); x.sinceImp.store(0); x.ready.store(1, std::memory_order_release);
    IMPROVES++;
    return true;
  }
  Path get_path(long slot) { std::lock_guard<std::mutex> lk(stripe[slot % NSTRIPE]); return c[slot].path; }
};
static unique_ptr<Archive> AR;

// ---------------- tours, output, verification
static string tour_text(const Path& p) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < p.size(); i++) {
    Mv m = dec(p[i]); int w = apply_move(s, m.delta, m.q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(m.q).c_str()); out += buf;
  }
  return out;
}
static vector<Path> read_tours(const string& fn, bool quiet = false) {
  vector<Path> out; FILE* f = fopen(fn.c_str(), "r");
  if (!f) { if (!quiet) fprintf(stderr, "cannot open %s\n", fn.c_str()); return out; }
  string line; int ch;
  auto parse = [&]() {
    Path t; S s = start_state(); const char* p = line.c_str(); int w; char sq[8];
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); if (q < 0 || q > 63) break;
      int n = group_n(s, s.pos); int d = w / max(1, n);
      if (d * max(1, n) != w || d >= 1024) { t.clear(); break; }
      t.push_back(enc({d, q})); apply_move(s, d, q); p++;
    }
    if (t.size() > 1 && dec(t.back()).q == H8) out.push_back(t);
  };
  while ((ch = fgetc(f)) != EOF) { if (ch == '\n') { parse(); line.clear(); } else line += (char)ch; }
  if (!line.empty()) parse();
  fclose(f); return out;
}
static int tour_T(const Path& p) { S s = start_state(); for (auto x : p) { Mv m = dec(x); apply_move(s, m.delta, m.q); } return s.T; }
static uint64_t path_hash(const Path& p) {
  uint64_t h = 1469598103934665603ULL; for (auto x : p) { h ^= x; h *= 1099511628211ULL; } return h ^ p.size();
}
static string CHECKER = "tools/check_tour.py";
static bool verify(const string& text, string& msg) {
  string cmd = "python3 " + CHECKER + " \"" + text + "\" 2>&1";
  FILE* p = popen(cmd.c_str(), "r"); if (!p) { msg = "popen failed"; return false; }
  char buf[4096]; msg.clear(); while (fgets(buf, sizeof buf, p)) msg += buf; pclose(p);
  while (!msg.empty() && msg.back() == '\n') msg.pop_back();
  size_t mv = msg.find("maxvisits=");
  bool ok = msg.find("ok=True") != string::npos && msg.find("unnecessary=[]") != string::npos && mv != string::npos && atoi(msg.c_str() + mv + 10) <= 3;
  return ok;
}

static std::mutex poolMtx, bestMtx;
static unordered_set<uint64_t> SEEN;  // tours already known (seeds + pool), guarded by poolMtx
static string POOLFILE, OUTFILE = "best.txt", RECDIR = ".";
static std::atomic<int> SEARCHBEST{0}, SEEDBEST{0};
static Path searchBestPath;
static const int RECORD = 8123;
static std::chrono::steady_clock::time_point T0;
static double elapsed() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count(); }

static void record_finish(const Path& tour, int T) {
  FINISHES++;
  if (T >= POOLMIN && !POOLFILE.empty()) {
    uint64_t h = path_hash(tour); bool fresh;
    { std::lock_guard<std::mutex> lk(poolMtx); fresh = SEEN.insert(h).second; }
    if (fresh) {
      string txt = tour_text(tour);
      std::lock_guard<std::mutex> lk(poolMtx);
      FILE* f = fopen(POOLFILE.c_str(), "a"); if (f) { fprintf(f, "%d\t%s\n", T, txt.c_str()); fclose(f); }
    }
  }
  if (T <= SEARCHBEST.load()) return;
  std::lock_guard<std::mutex> lk(bestMtx);
  if (T <= SEARCHBEST.load()) return;
  string txt = tour_text(tour), msg; bool ok = true;
  if (T >= POOLMIN) ok = verify(txt, msg); else msg = "(not checked: below --pool-min)";
  if (!ok) { printf("[%6.0fs] WARNING: tour of %d minutes FAILED the checker: %s\n", elapsed(), T, msg.c_str()); fflush(stdout); return; }
  SEARCHBEST.store(T); searchBestPath = tour;
  FILE* f = fopen(OUTFILE.c_str(), "w"); if (f) { fprintf(f, "%d\n%s\n", T, txt.c_str()); fclose(f); }
  printf("[%6.0fs] NEW SEARCH BEST %d minutes (%zu moves) %s\n", elapsed(), T, tour.size(), msg.c_str());
  if (T > RECORD && ok) {
    string fn = RECDIR + "/NEW_RECORD_" + to_string(T) + ".txt";
    FILE* g = fopen(fn.c_str(), "w"); if (g) { fprintf(g, "%d\n%s\n", T, txt.c_str()); fclose(g); }
    printf("*** NEW RECORD %d -> %s ***\n", T, fn.c_str());
  }
  fflush(stdout);
}

// ---------------- exploration
struct Ctx {
  Path pre; vector<long> preSlots; int m0 = 0;
  vector<long> stepSlots; std::mt19937 rng;
};

struct Result { double score = -1e18; long long minutes = 0; bool finished = false; vector<Mv> seq; };

// one playout from root; every visited state is offered to the archive; a finish is recorded
static Result playout(const S& root, const Policy& pol, Ctx& cx) {
  S s = root; Result r; static thread_local vector<pair<int, int>> mv; static thread_local vector<double> w;
  long long finT = -1; size_t finLen = 0; Mv finMove{0, H8};
  cx.stepSlots.clear();
  while (!STOP) {
    if (XSTEPS > 0 && (int)r.seq.size() >= XSTEPS) break;
    pinned_moves(s, 3, true, mv);
    w.clear(); double z = 0; int nonfin = 0;
    for (auto& [d, q] : mv) {
      if (q == H8) { int T = s.T + (d ? d * group_n(s, s.pos) : 0); if (T >= finT) { finT = T; finLen = r.seq.size(); finMove = {d, q}; } w.push_back(0); continue; }
      double e = move_e(pol, s, d, q); w.push_back(e); z += e; nonfin++;
    }
    if (nonfin == 0) break;
    double u = std::uniform_real_distribution<double>(0, z)(cx.rng), acc = 0; int pick = -1;
    for (size_t i = 0; i < mv.size(); i++) { if (mv[i].second == H8) continue; acc += w[i]; pick = (int)i; if (acc >= u) break; }
    r.seq.push_back({mv[pick].first, mv[pick].second});
    apply_move(s, mv[pick].first, mv[pick].second);
    Desc d = describe(s, cx.m0 + (int)r.seq.size());
    long slot = AR->find(d.key, true);
    cx.stepSlots.push_back(slot);
    if (slot >= 0) AR->offer(slot, d.fit, cx.pre, &r.seq, r.seq.size());
  }
  PLAYOUTS++;
  if (finT >= 0) {
    r.seq.resize(finLen); r.seq.push_back(finMove);
    r.finished = true; r.minutes = finT; r.score = (double)finT;
    Path tour = cx.pre; for (auto& m : r.seq) tour.push_back(enc(m));
    for (long sl : cx.preSlots) if (sl >= 0) amax(AR->c[sl].val, (int)finT);
    for (size_t i = 0; i < finLen; i++) if (cx.stepSlots[i] >= 0) amax(AR->c[cx.stepSlots[i]].val, (int)finT);
    record_finish(tour, (int)finT);
  } else {
    r.minutes = s.T; r.score = -1e7 + s.T * exp(-h8dist(s));
  }
  return r;
}

static void adapt(Policy& pol, const S& root, const vector<Mv>& seq, double alpha) {
  static thread_local vector<pair<uint32_t, float>> upd; upd.clear();
  static thread_local vector<pair<int, int>> mv; static thread_local vector<double> e;
  S s = root;
  for (const Mv& m : seq) {
    if (m.q == H8) break;
    pinned_moves(s, 3, true, mv);
    e.clear(); double z = 0;
    for (auto& [d, q] : mv) { double x = q == H8 ? 0 : move_e(pol, s, d, q); e.push_back(x); z += x; }
    for (size_t i = 0; i < mv.size(); i++) if (mv[i].second != H8) upd.push_back({code_of(s, mv[i].first, mv[i].second), (float)(-alpha * e[i] / z)});
    upd.push_back({code_of(s, m.delta, m.q), (float)alpha});
    apply_move(s, m.delta, m.q);
  }
  for (auto& [c, d] : upd) pol.w[c] += d;
  for (auto& [c, d] : upd) pol.e[c] = expf(pol.w[c]);
}
static Result nrpa(int level, const S& root, Policy pol, Ctx& cx) {
  if (level == 0) return playout(root, pol, cx);
  Result best;
  for (int i = 0; i < ITERS && !STOP; i++) {
    Result r = level == 1 ? playout(root, pol, cx) : nrpa(level - 1, root, pol, cx);
    if (r.score >= best.score) best = r;
    adapt(pol, root, best.seq, ALPHA);
  }
  return best;
}

// ---------------- selection snapshot
struct Snap { vector<uint32_t> idx; vector<double> cum; size_t perStage[3] = {0, 0, 0}; size_t withVal = 0; };
static std::mutex snapMtx; static shared_ptr<const Snap> SNAP;
static bool UNIFORM = false;
static shared_ptr<const Snap> build_snap() {
  auto sn = make_shared<Snap>(); double acc = 0;
  int B = max(SEARCHBEST.load(), SEEDBEST.load());
  for (size_t i = 0; i < AR->N; i++) {
    Cell& x = AR->c[i];
    if (!x.ready.load(std::memory_order_acquire) || x.dead.load(std::memory_order_relaxed)) continue;
    uint64_t k = x.key.load(std::memory_order_relaxed); sn->perStage[min<int>(k & 3, 2)]++;
    int v = x.val.load(std::memory_order_relaxed); if (v > 0) sn->withVal++;
    double w;
    if (UNIFORM) w = 1;
    else {
      double nov = 1.0 / sqrt(1.0 + x.chosen.load(std::memory_order_relaxed)) + RB / sqrt(1.0 + x.sinceImp.load(std::memory_order_relaxed));
      double vv = v > 0 ? v : B - UNK;
      w = nov * exp(max(-40.0, (vv - B) / TEMP));
    }
    acc += w; sn->idx.push_back((uint32_t)i); sn->cum.push_back(acc);
  }
  return sn;
}

static void worker(int tid, unsigned seed) {
  Ctx cx; cx.rng.seed(seed * 1000003u + tid * 7919u + 17);
  Policy zero;  // uniform playouts (with --bias as prior)
  vector<pair<int, int>> mv;
  while (!STOP) {
    shared_ptr<const Snap> sn; { std::lock_guard<std::mutex> lk(snapMtx); sn = SNAP; }
    if (!sn || sn->idx.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
    double u = std::uniform_real_distribution<double>(0, sn->cum.back())(cx.rng);
    size_t j = std::upper_bound(sn->cum.begin(), sn->cum.end(), u) - sn->cum.begin(); if (j >= sn->idx.size()) j = sn->idx.size() - 1;
    long slot = sn->idx[j]; Cell& c = AR->c[slot];
    c.chosen++; c.sinceImp++;
    cx.pre = AR->get_path(slot); cx.m0 = (int)cx.pre.size();
    S root = start_state(); cx.preSlots.clear();
    cx.preSlots.push_back(AR->find(describe(root, 0).key, false));
    for (size_t i = 0; i < cx.pre.size(); i++) {
      Mv m = dec(cx.pre[i]); apply_move(root, m.delta, m.q);
      cx.preSlots.push_back(AR->find(describe(root, (int)i + 1).key, false));
    }
    pinned_moves(root, 3, false, mv);
    if (mv.empty()) { c.dead.store(1); continue; }
    if (std::uniform_real_distribution<double>(0, 1)(cx.rng) < PNRPA) nrpa(NLEVEL, root, zero, cx);
    else for (int b = 0; b < BATCH && !STOP; b++) playout(root, zero, cx);
  }
}

// ---------------- seeding, checkpoints
static void seed_tour(const Path& t) {
  S s = start_state(); vector<long> slots; vector<Mv> seq;
  for (auto x : t) seq.push_back(dec(x));
  Path empty;
  Desc d = describe(s, 0); long sl = AR->find(d.key, true); slots.push_back(sl);
  if (sl >= 0) AR->offer(sl, d.fit, empty, &seq, 0);
  for (size_t i = 0; i + 1 < seq.size(); i++) {
    apply_move(s, seq[i].delta, seq[i].q);
    d = describe(s, (int)i + 1); sl = AR->find(d.key, true); slots.push_back(sl);
    if (sl >= 0) AR->offer(sl, d.fit, empty, &seq, i + 1);
  }
  apply_move(s, seq.back().delta, seq.back().q);
  for (long x : slots) if (x >= 0) amax(AR->c[x].val, s.T);
  amax(*(std::atomic<int32_t>*)&SEEDBEST, s.T);
  SEEN.insert(path_hash(t));
}
static void save_ckpt(const string& fn) {
  string tmp = fn + ".tmp"; FILE* f = fopen(tmp.c_str(), "wb"); if (!f) { fprintf(stderr, "cannot write %s\n", tmp.c_str()); return; }
  fwrite("GEX1", 1, 4, f);
  long long n = 0; fwrite(&n, sizeof n, 1, f);
  for (size_t i = 0; i < AR->N; i++) {
    Cell& x = AR->c[i]; if (!x.ready.load()) continue;
    Path p = AR->get_path((long)i);
    uint64_t k = x.key.load(); int32_t fit = x.fit.load(), val = x.val.load(); uint32_t ch = x.chosen.load(), si = x.sinceImp.load();
    uint8_t dead = x.dead.load(); uint32_t len = (uint32_t)p.size();
    fwrite(&k, 8, 1, f); fwrite(&fit, 4, 1, f); fwrite(&val, 4, 1, f); fwrite(&ch, 4, 1, f); fwrite(&si, 4, 1, f); fwrite(&dead, 1, 1, f);
    fwrite(&len, 4, 1, f); if (len) fwrite(p.data(), 2, len, f); n++;
  }
  { std::lock_guard<std::mutex> lk(bestMtx); int32_t sb = SEARCHBEST.load(); uint32_t len = (uint32_t)searchBestPath.size();
    fwrite(&sb, 4, 1, f); fwrite(&len, 4, 1, f); if (len) fwrite(searchBestPath.data(), 2, len, f); }
  fseek(f, 4, SEEK_SET); fwrite(&n, sizeof n, 1, f); fclose(f);
  rename(tmp.c_str(), fn.c_str());
  printf("[%6.0fs] checkpoint: %lld cells -> %s\n", elapsed(), n, fn.c_str()); fflush(stdout);
}
static bool load_ckpt(const string& fn) {
  FILE* f = fopen(fn.c_str(), "rb"); if (!f) { fprintf(stderr, "cannot open checkpoint %s\n", fn.c_str()); return false; }
  char mg[4]; long long n = 0;
  if (fread(mg, 1, 4, f) != 4 || memcmp(mg, "GEX1", 4) || fread(&n, sizeof n, 1, f) != 1) { fprintf(stderr, "bad checkpoint %s\n", fn.c_str()); fclose(f); return false; }
  long long loaded = 0;
  for (long long r = 0; r < n; r++) {
    uint64_t k; int32_t fit, val; uint32_t ch, si, len; uint8_t dead;
    if (fread(&k, 8, 1, f) != 1 || fread(&fit, 4, 1, f) != 1 || fread(&val, 4, 1, f) != 1 || fread(&ch, 4, 1, f) != 1 ||
        fread(&si, 4, 1, f) != 1 || fread(&dead, 1, 1, f) != 1 || fread(&len, 4, 1, f) != 1) break;
    Path p(len); if (len && fread(p.data(), 2, len, f) != len) break;
    long sl = AR->find(k, true); if (sl < 0) continue;
    Cell& x = AR->c[sl]; x.path = p; x.fit = fit; amax(x.val, val); x.chosen = ch; x.sinceImp = si; x.dead = dead; x.ready = 1; loaded++;
    // values feed the seed best too (the archive's best known finish)
    if (val > SEEDBEST.load()) SEEDBEST.store(val);
  }
  int32_t sb = 0; uint32_t len = 0;
  if (fread(&sb, 4, 1, f) == 1 && fread(&len, 4, 1, f) == 1) {
    Path p(len); if (!len || fread(p.data(), 2, len, f) == len) { SEARCHBEST.store(sb); searchBestPath = p; }
  }
  fclose(f);
  printf("resumed %lld cells from %s (search best %d)\n", loaded, fn.c_str(), SEARCHBEST.load());
  return true;
}

static void on_signal(int) { STOP = true; }

static void trace(const string& fn) {
  auto ts = read_tours(fn); if (ts.empty()) { fprintf(stderr, "no finished tour in %s\n", fn.c_str()); return; }
  S s = start_state(); const Path& t = ts[0];
  for (size_t i = 0; i < t.size(); i++) {
    Mv m = dec(t[i]); int w = apply_move(s, m.delta, m.q);
    Desc d = describe(s, (int)i + 1);
    printf("%3zu (%3d,%s) T=%5d stage=%d rel=%+d h8grp=%2d visleft=%3d pairs=%2d up=%2d lo=%2d fit=%d key=%016llx\n", i + 1, w, nm(m.q).c_str(), s.T,
           d.stage, d.rel, d.gh, d.vl, d.pairs, d.nUp, d.nLo, d.fit, (unsigned long long)d.key);
  }
}

int main(int argc, char** argv) {
  init_board(); D0 = 1; D0 = h8dist(start_state());
  for (int p = 0; p < 64; p++) { for (int q = 0; q < 64; q++) KIDX[p][q] = 0; for (int k = 0; k < NBN[p]; k++) KIDX[p][NBQ[p][k]] = k; }
  int THREADS = 1, TLOG2 = 21; double TIME = 0, CKPTEVERY = 600, REPORT = 10, SNAPEVERY = 1; unsigned RNG = 1;
  string ckpt, resume, traceFile; vector<string> seedFiles; bool noSeeds = false;
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--threads") THREADS = stoi(nx()); else if (a == "--time") TIME = stod(nx()); else if (a == "--rng") RNG = (unsigned)stoul(nx());
    else if (a == "--seed") seedFiles.push_back(nx()); else if (a == "--no-seeds") noSeeds = true;
    else if (a == "--ckpt") ckpt = nx(); else if (a == "--ckpt-every") CKPTEVERY = stod(nx()); else if (a == "--resume") resume = nx();
    else if (a == "--pool") POOLFILE = nx(); else if (a == "--pool-min") POOLMIN = stoi(nx()); else if (a == "--out") OUTFILE = nx();
    else if (a == "--record-dir") RECDIR = nx(); else if (a == "--checker") CHECKER = nx();
    else if (a == "--table-log2") TLOG2 = stoi(nx());
    else if (a == "--level") NLEVEL = stoi(nx()); else if (a == "--iters") ITERS = stoi(nx()); else if (a == "--alpha") ALPHA = stod(nx());
    else if (a == "--p-nrpa") PNRPA = stod(nx()); else if (a == "--batch") BATCH = stoi(nx()); else if (a == "--explore-steps") XSTEPS = stoi(nx());
    else if (a == "--bias") BIAS = stod(nx());
    else if (a == "--temp") TEMP = stod(nx()); else if (a == "--unk") UNK = stod(nx()); else if (a == "--recent") RB = stod(nx());
    else if (a == "--uniform") UNIFORM = true;
    else if (a == "--fit") { string f = nx(); FITMODE = f == "pot" ? 1 : 0; }
    else if (a == "--mb") MB = stoi(nx()); else if (a == "--gb") GB = stoi(nx()); else if (a == "--vb") VB = stoi(nx());
    else if (a == "--pb") PB = stoi(nx()); else if (a == "--dmin") DMIN = stoi(nx());
    else if (a == "--report") REPORT = stod(nx()); else if (a == "--snap-every") SNAPEVERY = stod(nx());
    else if (a == "--trace") traceFile = nx();
    else { fprintf(stderr, "unknown option %s (see RUN.md)\n", a.c_str()); return 1; }
  }
  if (!traceFile.empty()) { trace(traceFile); return 0; }
  T0 = std::chrono::steady_clock::now();
  signal(SIGINT, on_signal); signal(SIGTERM, on_signal);
  // self-check: the record replays to 8,123 in this engine
  { auto r = read_tours("data/tours/record_8123.txt", true);
    if (!r.empty()) { int T = tour_T(r[0]); printf("self-check: record_8123 replays to %d minutes%s\n", T, T == RECORD ? " (ok)" : " (MISMATCH)"); if (T != RECORD) return 1; } }
  AR.reset(new Archive(TLOG2));
  printf("archive table 2^%d slots (max %zu cells, ~%zu MB base)\n", TLOG2, AR->maxCells, AR->N * sizeof(Cell) >> 20);
  if (!resume.empty() && !load_ckpt(resume)) return 1;
  if (!noSeeds) {
    if (seedFiles.empty()) seedFiles = {"data/tours/record_8123.txt", "data/tours/record_8002.txt", "data/tours/record_7911.txt", "data/tours/record_7200.txt",
                                        "data/tours/record_7066.txt", "data/tours/tour_6303.txt", "data/tours/from_scratch_2952.txt", "data/tours/curriculum.txt"};
    size_t nt = 0;
    for (auto& fn : seedFiles) { auto ts = read_tours(fn); for (auto& t : ts) seed_tour(t); nt += ts.size(); printf("seeded %zu tours from %s\n", ts.size(), fn.c_str()); }
    printf("seeded %zu tours: %zu cells, best seed %d\n", nt, AR->count.load(), SEEDBEST.load());
  }
  if (AR->count.load() == 0) { Path e; vector<Mv> none; S s = start_state(); Desc d = describe(s, 0); long sl = AR->find(d.key, true); AR->offer(sl, d.fit, e, &none, 0); }
  if (resume.empty() && SEARCHBEST.load() == 0) SEARCHBEST.store(0);
  printf("Go-Explore: %d threads, time %.0fs, p-nrpa %.2f (level %d, %d iters), batch %d, temp %.0f, unk %.0f, fit %s, buckets mb=%d gb=%d vb=%d pb=%d dmin=%d\n",
         THREADS, TIME, PNRPA, NLEVEL, ITERS, BATCH, TEMP, UNK, FITMODE ? "pot" : "T", MB, GB, VB, PB, DMIN);
  fflush(stdout);
  { auto sn = build_snap(); std::lock_guard<std::mutex> lk(snapMtx); SNAP = sn; }
  vector<std::thread> th; for (int t = 0; t < THREADS; t++) th.emplace_back(worker, t, RNG);
  double lastSnap = 0, lastRep = 0, lastCk = 0; long long lastPl = 0;
  while (!STOP && (TIME <= 0 || elapsed() < TIME)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    double e = elapsed();
    if (e - lastSnap >= SNAPEVERY) { auto sn = build_snap(); { std::lock_guard<std::mutex> lk(snapMtx); SNAP = sn; } lastSnap = e; }
    if (e - lastRep >= REPORT) {
      shared_ptr<const Snap> sn; { std::lock_guard<std::mutex> lk(snapMtx); sn = SNAP; }
      long long pl = PLAYOUTS.load();
      printf("[%6.0fs] cells %zu (opening %zu, merge %zu, dance %zu; %zu with a finish), playouts %lld (%.0f/s), finishes %lld, improvements %lld, search best %d, seed best %d\n",
             e, AR->count.load(), sn->perStage[0], sn->perStage[1], sn->perStage[2], sn->withVal, pl, (pl - lastPl) / max(1e-9, e - lastRep), FINISHES.load(),
             IMPROVES.load(), SEARCHBEST.load(), SEEDBEST.load());
      fflush(stdout); lastRep = e; lastPl = pl;
    }
    if (!ckpt.empty() && e - lastCk >= CKPTEVERY) { save_ckpt(ckpt); lastCk = e; }
  }
  STOP = true;
  for (auto& t : th) t.join();
  if (!ckpt.empty()) save_ckpt(ckpt);
  printf("done after %.0fs: %zu cells, %lld playouts, %lld finishes, search best %d (seed best %d)\n", elapsed(), AR->count.load(), PLAYOUTS.load(),
         FINISHES.load(), SEARCHBEST.load(), SEEDBEST.load());
}
