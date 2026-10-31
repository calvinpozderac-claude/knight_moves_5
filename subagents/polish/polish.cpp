// Post-processing tour polisher for the swamp-knight puzzle (Knight Moves 5).
//
// A tour is viewed as a "sit skeleton": the key moves (every move that waits, plus the final move into
// h8), joined by stretches of zero-wait hops. While the knight hops without waiting no altitude changes,
// so a stretch can be re-routed along any zero-wait path without changing a single minute; only visits
// change. Every step below re-plays the rest of a tour through `replay`, which keeps each key move's
// square and post-jump altitude difference dz (re-deriving its wait, so changed group sizes give changed
// minutes) and re-routes a stretch by a min-cost zero-wait path (Dijkstra) whenever the original stretch
// no longer fits (wrong start square, illegal hop, or a visit the rest of the tour still needs).
//
// Steps, iterated until no gain:
//  (1) reroute: replay the whole tour re-routing every stretch with a cost that avoids points that sit
//      later (supersedes tools/reroute_zero_wait.py); same minutes, freed visits. Several cost variants.
//  (2) sit insertion: window search with w = 0 (insert up to --ext0 moves, e.g. extra sit/hop pairs)
//  (3) window search: for every window of w moves (w = 1..--wmax), exhaustive DFS over all replacement
//      sequences of up to w + --ext pinned moves (transposition table, visit caps that reserve the
//      landings the rest still needs); after every sit in the replacement the rest is replayed
//  (4) NRPA level 2/3 re-search of the last N moves (fresh and warm), copied from src/nrpa.cpp
// Accept: more minutes, or the same minutes with fewer jumps (frees visits).
//
// usage: polish --in FILE [--in FILE ...] [--top K] [--time S] [--per-tour S] [--threads N]
//          [--wmax 8] [--ext 2] [--ext0 4] [--node-limit 200000] [--nrpa-level 3] [--nrpa-tails 20,30,45,60]
//          [--no-nrpa] [--no-window] [--out best.txt] [--pool pool.txt] [--pool-min 7500]
//          [--polished FILE] [--rng 1] [--quiet]
// Input: any file with tours, one per line ("minutes<TAB>tour", or nrpa's "minutes\ntour", or bare
// tours); every line containing "(" is a tour. Output: --out best (minutes\ntour), --pool every improved
// tour (minutes<TAB>tour), --polished the final polished version of every input (minutes<TAB>tour).
#include "../../src/eng.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <set>

struct Mv { int d, q; };
typedef vector<Mv> Tour;
static int TYP[64][64];
static std::atomic<bool> STOP{false};
static void on_sig(int) { STOP = true; }
typedef std::chrono::steady_clock CLK;
static CLK::time_point DEADLINE;
static inline bool timeup() { return STOP || CLK::now() >= DEADLINE; }

// ---------------------------------------------------------------- validation / io
// full rule check for integer pinned tours: legal jumps, waits necessary (jump depends on the wait), caps,
// h8 only at the end. Returns minutes or -1.
static long long validate(const Tour& t, string* why = nullptr) {
  S s = start_state();
  for (size_t k = 0; k < t.size(); k++) {
    int p = s.pos, q = t[k].q, d = t[k].d;
    auto bad = [&](const char* m) { if (why) { char b[200]; snprintf(b, 200, "move %zu (%d,%s): %s", k + 1, d, nm(q).c_str(), m); *why = b; } return -1LL; };
    if (q < 0 || q > 63 || TYP[p][q] < 0) return bad("not a neighbour");
    if (d < 0) return bad("negative");
    if (d > 0 && s.alt[q] == s.alt[p]) return bad("wait does not change the jump");
    apply_move(s, d, q);
    if (!legal_dz(TYP[p][q], s.alt[q] - s.alt[p])) return bad("illegal jump");
    if (q == H8) { if (k + 1 != t.size()) return bad("h8 early"); }
    else if (s.vis[q] > 3) return bad("4th visit");
  }
  if (t.empty() || t.back().q != H8) { if (why) *why = "does not end on h8"; return -1; }
  return s.T;
}

static string tour_text(const Tour& t) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < t.size(); i++) {
    int w = apply_move(s, t[i].d, t[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(t[i].q).c_str()); out += buf;
  }
  return out;
}

// parse one line; returns empty tour on failure (fractional waits, illegal, ...)
static Tour parse_line(const string& line, string* why) {
  Tour t; S s = start_state(); size_t k = 0;
  while ((k = line.find('(', k)) != string::npos) {
    int w; char sq[3];
    if (sscanf(line.c_str() + k, "(%d, %2[a-h1-8])", &w, sq) != 2) { if (why) *why = "unparsable or fractional wait"; return {}; }
    size_t close = line.find(')', k); string inner = line.substr(k, close - k);
    if (inner.find('/') != string::npos) { if (why) *why = "fractional wait"; return {}; }
    k++;
    int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos);
    if (w % max(1, n)) { if (why) *why = "wait not a multiple of the group size"; return {}; }
    int d = w / max(1, n); t.push_back({d, q}); apply_move(s, d, q);
  }
  string w2; if (validate(t, &w2) < 0) { if (why) *why = w2; return {}; }
  return t;
}

// ---------------------------------------------------------------- skeleton + replay
struct Seg { int sfrom; vector<int> stretch; int from, q, dz, k; };  // k = index of the key move
struct Info { vector<S> st; vector<int> from, dz; };  // st[k] = state before move k (st[n] = final)
static Info info_of(const Tour& t) {
  Info I; S s = start_state();
  for (const Mv& m : t) {
    I.st.push_back(s); I.from.push_back(s.pos);
    S c = s; apply_move(c, m.d, m.q); I.dz.push_back(c.alt[m.q] - c.alt[s.pos]); s = c;
  }
  I.st.push_back(s); return I;
}
// key moves of t[a..n): waits and the final move; zero-wait moves before a key form its stretch
static vector<Seg> segs_of(const Tour& t, const Info& I, int a) {
  vector<Seg> out; Seg cur; cur.sfrom = a < (int)t.size() ? I.from[a] : -1;
  for (int k = a; k < (int)t.size(); k++) {
    if (t[k].d > 0 || k + 1 == (int)t.size()) {
      cur.from = I.from[k]; cur.q = t[k].q; cur.dz = I.dz[k]; cur.k = k; out.push_back(cur);
      cur = Seg(); cur.sfrom = k + 1 < (int)t.size() ? I.from[k + 1] : -1;
    } else cur.stretch.push_back(t[k].q);
  }
  return out;
}

// scratch for replay
static std::atomic<long long> ST_OK{0}, ST_FAIL{0}, ST_SEGS{0}, ST_CONV{0}, ST_ROUTE{0};
// min-cost zero-wait path from s.pos to target (landing cost 1 + pen); appends the hops to path.
static bool route(const S& s, int target, const int* res, const float* pen, vector<int>& path) {
  if (s.pos == target) return true;
  float dist[64]; int prev[64]; bool done[64];
  for (int i = 0; i < 64; i++) { dist[i] = 1e30f; prev[i] = -1; done[i] = false; }
  dist[s.pos] = 0;
  for (int it = 0; it < 64; it++) {
    int u = -1; float bu = 1e30f;
    for (int i = 0; i < 64; i++) if (!done[i] && dist[i] < bu) { bu = dist[i]; u = i; }
    if (u < 0) return false;
    if (u == target) break;
    done[u] = true;
    for (int k = 0; k < NBN[u]; k++) {
      int v = NBQ[u][k]; if (v == H8 || done[v]) continue;
      if (!legal_dz(NBT[u][k], s.alt[v] - s.alt[u])) continue;
      if (s.vis[v] + 1 + res[v] > 3) continue;
      float c = bu + 1.f + pen[v];
      if (c < dist[v]) { dist[v] = c; prev[v] = u; }
    }
  }
  if (dist[target] >= 1e29f) return false;
  int buf[64], n = 0; for (int v = target; v != s.pos; v = prev[v]) buf[n++] = v;
  for (int i = n - 1; i >= 0; i--) path.push_back(buf[i]);
  return true;
}

// replay segments from s. mode 0: keep a stretch verbatim when it still fits, else re-route;
// mode 1: always re-route. penMode: 0 none, else PEN for squares that key moves still use.
// out (optional) receives the moves. Returns false if the rest cannot be played.
static bool replay(S& s, const vector<Seg>& segs, size_t g0, int mode, float PEN, float PEN2, const uint8_t* dance,
                   vector<Mv>* out, const Tour* ot = nullptr, const Info* oi = nullptr) {
  int res[64]; float pen[64]; memset(res, 0, sizeof res);
  bool isFrom[64] = {false};
  for (size_t g = g0; g < segs.size(); g++) { if (segs[g].q != H8) res[segs[g].q]++; isFrom[segs[g].from] = true; }
  for (int i = 0; i < 64; i++) pen[i] = ((res[i] > 0 || isFrom[i]) ? PEN : 0.f) + (dance && dance[i] ? PEN2 : 0.f);
  vector<int> path;
  for (size_t g = g0; g < segs.size(); g++) {
    const Seg& sg = segs[g];
    bool done = false; ST_SEGS++;
    if (mode == 0 && s.pos == sg.sfrom) {  // try the stretch as it was
      S c = s; bool ok = true;
      for (int q : sg.stretch) {
        int p = c.pos;
        if (!legal_dz(TYP[p][q], c.alt[q] - c.alt[p]) || c.vis[q] + 1 + res[q] > 3) { ok = false; break; }
        c.vis[q]++; c.pos = q;
      }
      if (ok && c.pos == sg.from) { if (out) for (int q : sg.stretch) out->push_back({0, q}); s = c; done = true; }
    }
    if (!done) {
      path.clear(); ST_ROUTE++;
      if (!route(s, sg.from, res, pen, path)) return false;
      for (int q : path) { s.vis[q]++; s.pos = q; if (out) out->push_back({0, q}); }
    }
    // the key move, keeping dz (or the other sign if that wait would be negative)
    int p = s.pos, q = sg.q, t = TYP[p][q]; int a = s.alt[p], delta;
    if (s.alt[q] == a) { if (t != 0) return false; delta = 0; }
    else {
      int diff = s.alt[q] - a; delta = sg.dz - diff;
      if (delta < 0) { if (t == 0) return false; delta = -sg.dz - diff; if (delta < 0) return false; }
    }
    if (q != H8) { if (s.vis[q] + res[q] > 3) return false; res[q]--; }
    else if (g + 1 != segs.size()) return false;
    apply_move(s, delta, q);
    if (out) out->push_back({delta, q});
    if (oi && g + 1 < segs.size()) {  // converged to the original tour (up to a constant shift)? then its rest is exact
      const S& o = oi->st[sg.k + 1];
      if (o.pos == s.pos) {
        int sh = s.alt[0] - o.alt[0]; bool same = true;
        for (int i = 0; i < 64 && same; i++) same = s.alt[i] - o.alt[i] == sh && s.vis[i] <= o.vis[i];
        if (same) {
          if (out) for (size_t k = sg.k + 1; k < ot->size(); k++) out->push_back((*ot)[k]);
          s.T += oi->st.back().T - o.T; ST_CONV++; return true;
        }
      }
    }
  }
  return true;
}

static inline bool better(long long T1, size_t n1, long long T2, size_t n2) { return T1 > T2 || (T1 == T2 && n1 < n2); }

// squares in the final two groups (the dance) of a tour
static void dance_set(const Tour& t, uint8_t* d) {
  Info I = info_of(t); const S& f = I.st.back(); int a = f.alt[H8];
  for (int i = 0; i < 64; i++) d[i] = (f.alt[i] == a || f.alt[i] == a - 2 || f.alt[i] == a + 2) ? 1 : 0;
}

// step 1: replay the whole tour re-routing every stretch
static Tour reroute_all(const Tour& t, float PEN, float PEN2) {
  Info I = info_of(t); auto sg = segs_of(t, I, 0); uint8_t dn[64]; dance_set(t, dn);
  S s = start_state(); Tour out;
  if (!replay(s, sg, 0, 1, PEN, PEN2, dn, &out)) return {};
  if (validate(out) < 0) return {};
  return out;
}

// minutes after playing the key moves only (altitudes and minutes do not depend on the zero-wait routes);
// -1 if some key's wait would be negative or its jump impossible. Also checks the key landings' visit caps.
static long long key_minutes_alt(const S& s0, const vector<Seg>& segs);
static long long key_minutes(const S& s0, const vector<Seg>& segs, const int* res0) {
  for (int i = 0; i < 64; i++) if (i != H8 && s0.vis[i] + res0[i] > 3) return -1;
  return key_minutes_alt(s0, segs);
}
static long long key_minutes_alt(const S& s0, const vector<Seg>& segs) {
  AT alt[64]; memcpy(alt, s0.alt, sizeof alt); long long T = s0.T;
  for (const Seg& sg : segs) {
    int p = sg.from, q = sg.q, t = TYP[p][q]; int a = alt[p], delta;
    if (alt[q] == a) { if (t != 0) return -1; continue; }
    int diff = alt[q] - a; delta = sg.dz - diff;
    if (delta < 0) { if (t == 0) return -1; delta = -sg.dz - diff; if (delta < 0) return -1; }
    if (delta) {
      int O = 63 - p; bool rise = alt[O] != a; int n = 0;
      for (int i = 0; i < 64; i++) if (alt[i] == a && i != O) { alt[i] -= delta; n++; }
      if (rise) alt[O] += delta;
      T += (long long)n * delta;
    }
  }
  return T;
}

// ---------------------------------------------------------------- window search
struct WinRes { long long T = -1; Tour tour; long long nodes = 0, evals = 0; };
struct Window {
  const Tour* t; const Info* I; int i, w, L; long long nodeLimit; float PEN;
  vector<Seg> tail; int res0[64];
  long long bestT; size_t bestN; Tour best; long long nodes = 0, evals = 0;
  vector<Mv> path;
  // transposition table: fixed size, per-thread, stamped with a window id so it never needs clearing
  struct TTE { uint64_t key; int32_t T; uint16_t depth, stamp; };
  static const int TTB = 20;
  static thread_local vector<TTE> TT; static thread_local uint16_t STAMP;
  struct KCE { uint64_t key; long long T; uint16_t stamp; };
  static const int KCB = 16; static thread_local vector<KCE> KC;
  vector<pair<int, int>> mvbuf[16];
  void eval(const S& s) {
    if (tail.empty()) return;  // nothing left to replay: only finishing moves count (handled in dfs)
    evals++;
    long long Tq = -1; bool capok = true;
    for (int k = 0; k < 64; k++) if (k != H8 && s.vis[k] + res0[k] > 3) { capok = false; break; }
    if (capok) {  // the minutes depend on the altitudes only: cache them
      uint64_t h = 1469598103934665603ULL; const uint64_t* pa = (const uint64_t*)s.alt;
      for (int k = 0; k < 16; k++) h = (h ^ pa[k]) * 1099511628211ULL + (h >> 29);
      h ^= (uint64_t)s.T << 40;
      KCE& e = KC[h & ((1u << KCB) - 1)];
      if (e.stamp == STAMP && e.key == h) { Tq = e.T; ST_CONV++; }
      else { Tq = key_minutes_alt(s, tail); e = {h, Tq, STAMP}; }
    }
    if (Tq < bestT || (Tq == bestT && path.size() >= (size_t)w)) { ST_FAIL++; return; }
    S c = s; static thread_local vector<Mv> rest; rest.clear();
    bool ok = replay(c, tail, 0, 0, PEN, 0, nullptr, &rest, t, I);
    if (!ok) { c = s; rest.clear(); ok = replay(c, tail, 0, 1, PEN, 0, nullptr, &rest, t, I); }
    if (!ok) { ST_FAIL++; return; }
    ST_OK++;
    size_t n = i + path.size() + rest.size();
    if (better(c.T, n, bestT, bestN)) {
      Tour cand(t->begin(), t->begin() + i); cand.insert(cand.end(), path.begin(), path.end()); cand.insert(cand.end(), rest.begin(), rest.end());
      long long v = validate(cand); if (v != c.T) return;
      bestT = c.T; bestN = n; best = cand;
    }
  }
  void dfs(const S& s, int depth, bool lastSit) {
    if (++nodes > nodeLimit || ((nodes & 1023) == 0 && timeup())) return;
    if (depth == 0 || lastSit) eval(s);
    if (depth >= L) return;
    auto& mv = mvbuf[depth]; pinned_moves(s, 3, true, mv);
    for (auto [d, q] : mv) {
      if (nodes > nodeLimit) return;
      if (q == H8) {  // finishing here
        S c = s; apply_move(c, d, q); size_t n = i + path.size() + 1;
        if (better(c.T, n, bestT, bestN)) {
          Tour cand(t->begin(), t->begin() + i); cand.insert(cand.end(), path.begin(), path.end()); cand.push_back({d, q});
          if (validate(cand) == c.T) { bestT = c.T; bestN = n; best = cand; }
        }
        continue;
      }
      if (s.vis[q] + 1 + res0[q] > 3) continue;
      S c = s; apply_move(c, d, q);
      uint64_t h = hashS(c);
      TTE& e = TT[h & ((1u << TTB) - 1)];
      if (e.stamp == STAMP && e.key == h && e.T >= c.T && e.depth <= depth + 1) continue;
      e = {h, c.T, (uint16_t)(depth + 1), STAMP};
      path.push_back({d, q}); dfs(c, depth + 1, d > 0); path.pop_back();
    }
  }
  WinRes run() {
    tail = segs_of(*t, *I, i + w); memset(res0, 0, sizeof res0);
    for (auto& g : tail) if (g.q != H8) res0[g.q]++;
    bestT = I->st.back().T; bestN = t->size();
    if (TT.empty()) { TT.assign(1u << TTB, TTE{0, 0, 0, 0}); KC.assign(1u << KCB, KCE{0, 0, 0}); }
    if (++STAMP == 0) { for (auto& e : TT) e.stamp = 0; for (auto& e : KC) e.stamp = 0; STAMP = 1; }
    dfs(I->st[i], 0, false);
    WinRes r; r.nodes = nodes; r.evals = evals; if (!best.empty()) { r.T = bestT; r.tour = best; } return r;
  }
};

thread_local vector<Window::KCE> Window::KC;
thread_local vector<Window::TTE> Window::TT; thread_local uint16_t Window::STAMP = 0;

// ---------------------------------------------------------------- NRPA (from src/nrpa.cpp, single thread)
namespace nr {
static int KIDX[64][64];
static const int NCODE = 64 * 16 * 32 * 4;
struct Policy { vector<float> w, e; Policy() : w(NCODE, 0.f), e(NCODE, 1.f) {} };
static inline uint32_t code_of(const S& s, int delta, int q) {
  return (((uint32_t)s.pos * 16 + KIDX[s.pos][q]) * 32 + (uint32_t)min(delta, 31)) * 4 + (uint32_t)min<int>(s.vis[q], 3);
}
struct Result { double score = -1e18; long long minutes = 0; bool finished = false; vector<Mv> seq; };
static double D0 = 1;
static double h8dist(const S& s) { int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p]; return sqrt(dx * dx + dy * dy + dz * dz) / D0; }
static Result playout(const S& root, const Policy& pol, std::mt19937& rng) {
  S s = root; Result r; vector<pair<int, int>> mv; vector<double> w;
  long long finT = -1; size_t finLen = 0; Mv finMove{0, H8};
  while (true) {
    pinned_moves(s, 3, true, mv); w.clear(); double z = 0; int nonfin = 0;
    for (auto& [d, q] : mv) {
      if (q == H8) { S c = s; apply_move(c, d, q); if (c.T >= finT) { finT = c.T; finLen = r.seq.size(); finMove = {d, q}; } w.push_back(0); continue; }
      double e = pol.e[code_of(s, d, q)]; w.push_back(e); z += e; nonfin++;
    }
    if (nonfin == 0) break;
    double u = std::uniform_real_distribution<double>(0, z)(rng), acc = 0; int pick = -1;
    for (size_t i = 0; i < mv.size(); i++) { if (mv[i].second == H8) continue; acc += w[i]; pick = (int)i; if (acc >= u) break; }
    r.seq.push_back({mv[pick].first, mv[pick].second}); apply_move(s, mv[pick].first, mv[pick].second);
  }
  if (finT >= 0) { r.seq.resize(finLen); r.seq.push_back(finMove); r.finished = true; r.minutes = finT; r.score = (double)finT; }
  else { r.minutes = s.T; r.score = -1e7 + s.T * exp(-h8dist(s)); }
  return r;
}
static void adapt(Policy& pol, const S& root, const vector<Mv>& seq, double alpha) {
  static thread_local vector<pair<uint32_t, float>> upd; upd.clear();
  static thread_local vector<pair<int, int>> mv; static thread_local vector<double> e;
  S s = root;
  for (const Mv& m : seq) {
    if (m.q == H8) break;
    pinned_moves(s, 3, true, mv); e.clear(); double z = 0;
    for (auto& [d, q] : mv) { double x = q == H8 ? 0 : pol.e[code_of(s, d, q)]; e.push_back(x); z += x; }
    for (size_t i = 0; i < mv.size(); i++) if (mv[i].second != H8) upd.push_back({code_of(s, mv[i].first, mv[i].second), (float)(-alpha * e[i] / z)});
    upd.push_back({code_of(s, m.d, m.q), (float)alpha}); apply_move(s, m.d, m.q);
  }
  for (auto& [c, d] : upd) pol.w[c] += d;
  for (auto& [c, d] : upd) pol.e[c] = expf(pol.w[c]);
}
static Result nrpa(int level, const S& root, Policy pol, std::mt19937& rng, const Result* init) {
  if (level == 0) return playout(root, pol, rng);
  Result best; if (init) best = *init;
  for (int i = 0; i < 100 && !timeup(); i++) {
    Result r = level == 1 ? playout(root, pol, rng) : nrpa(level - 1, root, pol, rng, nullptr);
    if (r.score >= best.score) best = r;
    adapt(pol, root, best.seq, 1.0);
  }
  return best;
}
}  // namespace nr

// ---------------------------------------------------------------- driver
static int THREADS = 1, WMAX = 8, EXT = 2, EXT0 = 4, NRPA_LEVEL = 3; static long long NODELIM = 200000;
static bool DO_NRPA = true, DO_WIN = true, QUIET = false; static float PEN = 4;
static vector<int> TAILS = {20, 30, 45, 60};
static string OUT = "best.txt", POOL, POLISHED; static long long POOLMIN = 7500; static unsigned RNG = 1;
static std::mutex IOM; static long long GLOBAL_BEST = -1; static size_t GLOBAL_N = 0;
static double now_s(CLK::time_point t0) { return std::chrono::duration<double>(CLK::now() - t0).count(); }

static void report(const Tour& t, long long T, const char* how) {
  std::lock_guard<std::mutex> lk(IOM);
  string txt = tour_text(t);
  if (!POOL.empty() && T >= POOLMIN && strcmp(how, "input")) { FILE* f = fopen(POOL.c_str(), "a"); if (f) { fprintf(f, "%lld\t%s\n", T, txt.c_str()); fclose(f); } }
  if (better(T, t.size(), GLOBAL_BEST, GLOBAL_N)) {
    GLOBAL_BEST = T; GLOBAL_N = t.size(); FILE* f = fopen(OUT.c_str(), "w"); if (f) { fprintf(f, "%lld\n%s\n", T, txt.c_str()); fclose(f); }
    if (!QUIET) printf("  new overall best %lld (%s) -> %s\n", T, how, OUT.c_str());
  }
  fflush(stdout);
}

// one pass of window searches (all w, all i) over t; returns an improved tour or empty
static Tour window_pass(const Tour& t, long long& T, const char* tag) {
  Info I = info_of(t); int n = (int)t.size();
  struct Task { int w, i; };
  vector<Task> tasks;
  for (int w = 0; w <= WMAX; w++) for (int i = (w == 0 ? n - 1 : n - w); i >= 0; i--) tasks.push_back({w, i});
  std::atomic<size_t> next{0}; std::atomic<bool> found{false}; std::mutex m; Tour bestT; long long bestV = T; size_t bestN = t.size();
  std::atomic<long long> nodes{0}, evals{0}; auto t0 = CLK::now();
  auto worker = [&]() {
    while (!found && !timeup()) {
      size_t k = next++; if (k >= tasks.size()) break;
      Window W; W.t = &t; W.I = &I; W.i = tasks[k].i; W.w = tasks[k].w; W.L = tasks[k].w + (tasks[k].w == 0 ? EXT0 : EXT); W.nodeLimit = NODELIM; W.PEN = PEN;
      if (W.L > 15) W.L = 15;
      WinRes r = W.run(); nodes += r.nodes; evals += r.evals;
      if (r.T >= 0) {
        std::lock_guard<std::mutex> lk(m);
        if (better(r.T, r.tour.size(), bestV, bestN)) {
          bestV = r.T; bestN = r.tour.size(); bestT = r.tour; found = true;
          if (!QUIET) printf("  [%s] window w=%d at move %d: %lld -> %lld minutes, %zu -> %zu jumps\n", tag, W.w, W.i + 1, T, r.T, t.size(), r.tour.size());
        }
      }
    }
  };
  vector<std::thread> th; for (int k = 0; k < THREADS; k++) th.emplace_back(worker); for (auto& x : th) x.join();
  if (!QUIET) printf("  [%s] pass: %zu/%zu windows, %lld nodes, %lld tail replays, %.1fs (%.0f nodes/s)\n", tag, min(next.load(), tasks.size()), tasks.size(),
                     nodes.load(), evals.load(), now_s(t0), nodes.load() / max(1e-9, now_s(t0)));
  if (!QUIET && getenv("POLISH_STATS")) printf("  stats: ok %lld fail %lld segs %lld routes %lld conv %lld\n", ST_OK.load(), ST_FAIL.load(), ST_SEGS.load(), ST_ROUTE.load(), ST_CONV.load());
  fflush(stdout);
  if (found) { T = bestV; return bestT; }
  return {};
}

// NRPA re-search of the last N moves; returns an improved tour or empty
static Tour nrpa_tail(const Tour& t, long long T, int N, int level, bool warm, unsigned seed) {
  int n = (int)t.size(); if (N >= n) N = n - 1;
  int k = n - N; S root = start_state(); for (int i = 0; i < k; i++) apply_move(root, t[i].d, t[i].q);
  nr::Policy pol; std::mt19937 rng(seed);
  nr::Result init; vector<Mv> rest(t.begin() + k, t.end());
  if (warm) { for (int a = 0; a < 3; a++) nr::adapt(pol, root, rest, 1.0); init.seq = rest; init.finished = true; init.minutes = T; init.score = (double)T; }
  nr::Result r = nr::nrpa(level, root, pol, rng, warm ? &init : nullptr);
  if (!r.finished) return {};
  Tour out(t.begin(), t.begin() + k); out.insert(out.end(), r.seq.begin(), r.seq.end());
  long long v = validate(out); if (v < 0) return {};
  if (better(v, out.size(), T, t.size())) return out;
  return {};
}

static Tour polish(Tour t, double budget, int idx) {
  auto t0 = CLK::now(); DEADLINE = min(DEADLINE, t0 + std::chrono::milliseconds((long long)(budget * 1000)));
  long long T = validate(t); long long T0 = T;
  printf("tour %d: %lld minutes, %zu jumps, budget %.0fs\n", idx, T, t.size(), budget); fflush(stdout);
  report(t, T, "input");
  int round = 0; bool any = true; unsigned seed = RNG * 7919u + idx;
  while (any && !timeup()) {
    any = false; round++;
    // (1) reroute variants: (PEN on squares keys still use, PEN on dance squares)
    vector<pair<Tour, string>> variants = {{t, "as is"}};
    float pv[][2] = {{1000, 10}, {1000, 0}, {0, 0}, {5, 1}};
    for (auto& p : pv) {
      Tour r = reroute_all(t, p[0], p[1]);
      if (r.empty() || validate(r) != T) { if (!QUIET) printf("  reroute %g/%g failed\n", p[0], p[1]); continue; }
      bool dup = false; for (auto& v : variants) if (tour_text(v.first) == tour_text(r)) dup = true;
      if (dup) { if (!QUIET) printf("  reroute %g/%g: unchanged\n", p[0], p[1]); continue; }
      char b[64]; snprintf(b, 64, "reroute %g/%g", p[0], p[1]);
      if (!QUIET) printf("  %s: %zu -> %zu jumps\n", b, t.size(), r.size());
      if (r.size() < t.size()) { t = r; variants[0] = {t, b}; report(t, T, b); }
      else variants.push_back({r, b});
    }
    if (DO_WIN) {
      for (auto& v : variants) {
        if (timeup()) break;
        long long T2 = T; Tour r = window_pass(v.first, T2, v.second.c_str());
        if (!r.empty()) { t = r; T = T2; any = true; report(t, T, "window"); break; }
      }
      if (any) continue;
    }
    // (4) NRPA re-search of the last N moves
    if (DO_NRPA) {
      for (int lvl = 2; lvl <= NRPA_LEVEL && !any && !timeup(); lvl++)
        for (int N : TAILS) {
          if (timeup()) break;
          for (int warm = 0; warm < 2 && !timeup(); warm++) {
            auto ts = CLK::now(); Tour r = nrpa_tail(t, T, N, lvl, warm, seed++);
            if (!QUIET) printf("  nrpa L%d last %d (%s): %s in %.1fs\n", lvl, N, warm ? "warm" : "fresh", r.empty() ? "no gain" : "GAIN", now_s(ts));
            fflush(stdout);
            if (!r.empty()) { long long v = validate(r); printf("  nrpa: %lld -> %lld\n", T, v); t = r; T = v; any = true; report(t, T, "nrpa"); break; }
          }
          if (any) break;
        }
    }
  }
  printf("tour %d done: %lld -> %lld minutes (%zu jumps), %.1fs%s\n", idx, T0, T, t.size(), now_s(t0), timeup() ? " (time up)" : " (converged)");
  fflush(stdout);
  return t;
}

int main(int argc, char** argv) {
  init_board(); signal(SIGINT, on_sig); signal(SIGTERM, on_sig);
  for (int p = 0; p < 64; p++) { for (int q = 0; q < 64; q++) { TYP[p][q] = -1; nr::KIDX[p][q] = 0; } for (int k = 0; k < NBN[p]; k++) { TYP[p][NBQ[p][k]] = NBT[p][k]; nr::KIDX[p][NBQ[p][k]] = k; } }
  nr::D0 = 1; nr::D0 = nr::h8dist(start_state());
  vector<string> ins; double TIME = 60, PER = -1; int TOP = 0;
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--in") ins.push_back(nx()); else if (a == "--top") TOP = stoi(nx());
    else if (a == "--time") TIME = stod(nx()); else if (a == "--per-tour") PER = stod(nx());
    else if (a == "--threads") THREADS = stoi(nx()); else if (a == "--wmax") WMAX = stoi(nx()); else if (a == "--ext") EXT = stoi(nx()); else if (a == "--ext0") EXT0 = stoi(nx());
    else if (a == "--node-limit") NODELIM = stoll(nx()); else if (a == "--nrpa-level") NRPA_LEVEL = stoi(nx());
    else if (a == "--nrpa-tails") { TAILS.clear(); string s = nx(); size_t p = 0; while (p < s.size()) { TAILS.push_back(stoi(s.substr(p))); p = s.find(',', p); if (p == string::npos) break; p++; } }
    else if (a == "--no-nrpa") DO_NRPA = false; else if (a == "--no-window") DO_WIN = false;
    else if (a == "--pen") PEN = stof(nx());
    else if (a == "--out") OUT = nx(); else if (a == "--pool") POOL = nx(); else if (a == "--pool-min") POOLMIN = stoll(nx());
    else if (a == "--polished") POLISHED = nx(); else if (a == "--rng") RNG = (unsigned)stoul(nx()); else if (a == "--quiet") QUIET = true;
    else if (a.size() && a[0] != '-') ins.push_back(a);
    else { fprintf(stderr, "unknown option %s (see the comment at the top of polish.cpp)\n", a.c_str()); return 1; }
  }
  if (ins.empty()) { fprintf(stderr, "usage: polish --in FILE [options] (see polish.cpp)\n"); return 1; }
  // read every tour; dedupe; sort by minutes (best first)
  vector<pair<long long, Tour>> tours; std::set<string> seen; int skipped = 0;
  for (auto& fn : ins) {
    std::ifstream f(fn); if (!f) { fprintf(stderr, "cannot open %s\n", fn.c_str()); return 1; }
    string line;
    while (std::getline(f, line)) {
      if (line.find('(') == string::npos) continue;
      string why; Tour t = parse_line(line, &why);
      if (t.empty()) { skipped++; if (skipped <= 5) fprintf(stderr, "skipping a tour in %s: %s\n", fn.c_str(), why.c_str()); continue; }
      string key = tour_text(t); if (!seen.insert(key).second) continue;
      tours.push_back({validate(t), t});
    }
  }
  std::stable_sort(tours.begin(), tours.end(), [](auto& a, auto& b) { return a.first > b.first; });
  if (TOP > 0 && (int)tours.size() > TOP) tours.resize(TOP);
  printf("%zu tours to polish (%d skipped); threads %d, wmax %d, ext %d/%d, node limit %lld, nrpa level %d, time %.0fs\n",
         tours.size(), skipped, THREADS, WMAX, EXT0, EXT, NODELIM, DO_NRPA ? NRPA_LEVEL : 0, TIME);
  fflush(stdout);
  auto t0 = CLK::now(); DEADLINE = t0 + std::chrono::milliseconds((long long)(TIME * 1000));
  auto globalDeadline = DEADLINE;
  for (size_t k = 0; k < tours.size() && !STOP && CLK::now() < globalDeadline; k++) {
    double left = std::chrono::duration<double>(globalDeadline - CLK::now()).count();
    double budget = PER > 0 ? min(PER, left) : left / (tours.size() - k);
    DEADLINE = globalDeadline;
    Tour r = polish(tours[k].second, budget, (int)k + 1);
    long long v = validate(r);
    if (!POLISHED.empty()) { std::lock_guard<std::mutex> lk(IOM); FILE* f = fopen(POLISHED.c_str(), "a"); if (f) { fprintf(f, "%lld\t%s\n", v, tour_text(r).c_str()); fclose(f); } }
  }
  printf("done: best %lld minutes after %.0fs -> %s\n", GLOBAL_BEST, now_s(t0), OUT.c_str());
}
