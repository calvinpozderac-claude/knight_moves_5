// Nested Monte Carlo Search (Cazenave 2009), Lazy NMCS (Cazenave 2016) and single-player MCTS with
// max-backup (SP-MCTS, Schadd et al. 2008) for the swamp-knight puzzle, over the pinned moves of
// src/eng.h (the wait before each jump is implied by the jump).
//
// Playout policy: softmax over cheap move features, divided by a temperature --tau:
//   logit = (fw * minutes_waited/100 + fr * h8_close + fv * visits_left_at_target + fg * group_after/64 + fd * dance + fo * opp_out) / tau
//   h8_close     = h8's altitude after the move is within 2 of the knight's new altitude
//                  (--reach-bfs: instead, h8 is reachable from the child by zero-wait jumps; slower)
//   visits_left  = 3 - visits of the target before the jump (1..3)
//   group_after  = size of the target's altitude group after the move (what the next sit would sink)
//   dance (--fd) = the move sinks and the sunk group ends 2 below the target (the dance step)
//   opp_out (--fo) = the move sinks and the point opposite the knight is already more than 2 away
// A playout keeps going past chances to finish but remembers the best one (its tour ends there), as in
// src/nrpa.cpp. Finished tours score their minutes; unfinished ones score below all finished ones
// (-1e7 + minutes * exp(-distance to h8)).
//
// Methods (--method):
//   nmcs  : NMCS level --level with memorisation of the best sequence. Level 1 evaluates every move by
//           --base-n playouts (best kept); level L evaluates every move by a level L-1 search.
//   lnmcs : Lazy NMCS. At every level >= --lazy-min-level (default 2) each move is first rated by
//           --lazy-n cheap playouts (--lazy-stat max|mean); only the best --lazy-k moves (at least
//           --lazy-frac of them) plus the move of the memorised best sequence get the level L-1 search.
//   mcts  : SP-MCTS. UCT (or PUCT with the softmax prior, --puct) on Q = (1-mix)*max + mix*mean of the
//           normalised playout values in the subtree (--mix 0 = pure max-backup). After --commit-iters
//           iterations the root advances one move along the best sequence found (0 = never). Finishing
//           moves are terminal; visited terminals and fully explored subtrees are skipped.
//   playout: plain playouts from the start (baseline; also --method playout --bench N for speed).
// Start: a1, or after k moves of a seed tour (--seed-tours FILE --anchor-min A --anchor-max B, k drawn per
// restart). --warm starts NMCS/MCTS with the seed's own continuation as best (so it can only improve).
// --threads N independent searches (root parallelism), restarting until --time SECONDS or Ctrl-C/SIGTERM.
// Output: --out FILE (best: line 1 minutes, line 2 tour), --pool FILE (minutes<TAB>tour per line, every
// distinct top-level improvement and every restart result >= --pool-min), and new bests >= --verify-min
// are checked with python3 --checker; if one beats --record it goes to NEW_RECORD_<m>.txt in --record-dir.
#include "../../src/eng.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <unistd.h>

struct Mv { int delta, q; };

// ---------------------------------------------------------------- state with an altitude histogram
static const int HOFF = 1024, HN = 2048;
struct X { S s; uint8_t h[HN]; };
static inline int hix(int v) { v += HOFF; return v < 0 ? 0 : (v >= HN ? HN - 1 : v); }
static X make_x(const S& s) { X x; x.s = s; memset(x.h, 0, sizeof x.h); for (int i = 0; i < 64; i++) x.h[hix(s.alt[i])]++; return x; }
static inline int xapply(X& x, int d, int q) {
  S& s = x.s;
  if (d > 0) {
    int p = s.pos, a = s.alt[p], O = 63 - p, ao = s.alt[O]; bool rise = ao != a;
    int w = apply_move(s, d, q), n = w / d;
    x.h[hix(a)] -= n; x.h[hix(a - d)] += n;
    if (rise) { x.h[hix(ao)]--; x.h[hix(ao + d)]++; }
    return w;
  }
  return apply_move(s, 0, q);
}

// ---------------------------------------------------------------- parameters
static double FW = 0.5, FR = 6.0, FV = 2.0, FG = 1.0, FD = 8.0, FO = 0.0, TAU = 1.0, UNFPEN = 2000;
static bool REACHBFS = false;
static double D0 = 1;
static double h8dist(const S& s) {
  int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  return sqrt(dx * dx + dy * dy + dz * dz) / D0;
}

static std::atomic<bool> STOP{false};
static std::atomic<long long> PLAYOUTS{0};

struct Result {
  double score = -1e18, soft = -1e18; long long minutes = 0; bool finished = false; vector<Mv> seq;
};
static inline void set_unfinished(Result& r, const S& s) {
  double e = s.T * exp(-h8dist(s));
  r.finished = false; r.minutes = s.T; r.score = -1e7 + e; r.soft = min(-1.0, -UNFPEN + e);  // below every finished tour
}
static inline void set_finished(Result& r, long long T) { r.finished = true; r.minutes = T; r.score = r.soft = (double)T; }

// logits of the moves at x (finishing moves get -inf; they are handled separately)
static void move_logits(const X& x, const vector<pair<int, int>>& mv, vector<double>& lg) {
  const S& s = x.s; int p = s.pos, a = s.alt[p], O = 63 - p, ao = s.alt[O]; bool rise = ao != a;
  int n = (int)x.h[hix(a)] - (ao == a ? 1 : 0);
  auto na = [&](int i, int d) -> int {
    int v = s.alt[i]; if (d == 0) return v;
    if (v == a && i != O) return v - d;
    if (i == O && rise) return v + d;
    return v;
  };
  auto cnt = [&](int v, int d) -> int {
    int c = x.h[hix(v)];
    if (d > 0) { if (v == a) c -= n; if (v == a - d) c += n; if (rise) { if (ao == v) c--; if (ao + d == v) c++; } }
    return c;
  };
  lg.clear();
  for (auto& [d, q] : mv) {
    if (q == H8) { lg.push_back(-1e30); continue; }
    int v = na(q, d);
    int g = cnt(v, d) - (na(63 - q, d) == v ? 1 : 0);
    double h8c;
    if (REACHBFS) { S c = s; apply_move(c, d, q); bool hr; int mx; static_reach(c, 3, hr, mx); h8c = hr; }
    else h8c = abs(na(H8, d) - v) <= 2;
    double dance = d > 0 && v == a - d + 2;  // sunk group ends 2 below the target (the dance step)
    double oppOut = d > 0 && rise && abs(ao - a) > 2;  // the point opposite the sit is already out of the groups
    double z = FW * (d * n) / 100.0 + FR * h8c + FV * (3 - s.vis[q]) + FG * g / 64.0 + FD * dance + FO * oppOut;
    lg.push_back(z / TAU);
  }
}

// one playout from x; the returned seq is relative to x
static Result playout(const X& root, std::mt19937& rng) {
  static thread_local vector<pair<int, int>> mv; static thread_local vector<double> lg;
  X x = root; Result r; r.seq.reserve(160);
  long long finT = -1; size_t finLen = 0; Mv finMove{0, H8};
  while (true) {
    pinned_moves(x.s, 3, true, mv);
    if (mv.empty()) break;
    move_logits(x, mv, lg);
    double mxl = -1e300; int nonfin = 0;
    for (size_t i = 0; i < mv.size(); i++) {
      if (mv[i].second == H8) {
        long long T = x.s.T + (long long)mv[i].first * group_n(x.s, x.s.pos);
        if (T >= finT) { finT = T; finLen = r.seq.size(); finMove = {mv[i].first, H8}; }
        continue;
      }
      nonfin++; mxl = max(mxl, lg[i]);
    }
    if (nonfin == 0) break;
    double z = 0; for (size_t i = 0; i < mv.size(); i++) { lg[i] = mv[i].second == H8 ? 0 : exp(lg[i] - mxl); z += lg[i]; }
    double u = std::uniform_real_distribution<double>(0, z)(rng), acc = 0; int pick = -1;
    for (size_t i = 0; i < mv.size(); i++) { if (mv[i].second == H8) continue; acc += lg[i]; pick = (int)i; if (acc >= u) break; }
    r.seq.push_back({mv[pick].first, mv[pick].second});
    xapply(x, mv[pick].first, mv[pick].second);
  }
  PLAYOUTS++;
  if (finT >= 0) { r.seq.resize(finLen); r.seq.push_back(finMove); set_finished(r, finT); }
  else set_unfinished(r, x.s);
  return r;
}

// result of a fixed sequence from x
static Result as_result(const X& root, const vector<Mv>& seq) {
  Result r; r.seq = seq; X x = root; for (const Mv& m : seq) xapply(x, m.delta, m.q);
  if (!seq.empty() && seq.back().q == H8) set_finished(r, x.s.T); else set_unfinished(r, x.s);
  return r;
}

static string tour_text(const vector<Mv>& seq) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < seq.size(); i++) {
    int w = apply_move(s, seq[i].delta, seq[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(seq[i].q).c_str()); out += buf;
  }
  return out;
}

// ---------------------------------------------------------------- reporting (shared by all threads)
static std::mutex MTX;
static Result GBEST; static vector<Mv> GBEST_FULL;
static string OUTF = "nmcs_best.txt", POOLF, CHECKER = "tools/check_tour.py", RECDIR = ".";
static long long POOLMIN = 7500, VERIFYMIN = 8000, RECORD = 8123;
static unordered_set<string> POOLSEEN;
static std::chrono::steady_clock::time_point T0;
static double elapsed() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count(); }

static bool run_checker(const string& tour, string& line) {
  FILE* f = fopen(CHECKER.c_str(), "r"); if (!f) { line = "(checker not found)"; return false; } fclose(f);
  string tmp = RECDIR + "/.nmcs_check_" + to_string(getpid()) + ".txt";
  FILE* t = fopen(tmp.c_str(), "w"); if (!t) { line = "(cannot write temp)"; return false; }
  fprintf(t, "%s\n", tour.c_str()); fclose(t);
  string cmd = "python3 " + CHECKER + " \"$(cat " + tmp + ")\" 2>&1";
  FILE* p = popen(cmd.c_str(), "r"); line.clear(); char buf[4096];
  if (p) { while (fgets(buf, sizeof buf, p)) line += buf; pclose(p); }
  remove(tmp.c_str());
  while (!line.empty() && (line.back() == '\n')) line.pop_back();
  return line.find("ok=True") != string::npos && line.find("unnecessary=[]") != string::npos &&
         (line.find("maxvisits=3") != string::npos || line.find("maxvisits=2") != string::npos || line.find("maxvisits=1") != string::npos);
}

// full: the whole tour from a1
static void report(const vector<Mv>& full, const char* what) {
  Result r = as_result(make_x(start_state()), full);
  if (!r.finished) return;
  std::lock_guard<std::mutex> lk(MTX);
  string txt = tour_text(full);
  if (!POOLF.empty() && r.minutes >= POOLMIN && POOLSEEN.insert(txt).second) {
    FILE* f = fopen(POOLF.c_str(), "a"); if (f) { fprintf(f, "%lld\t%s\n", r.minutes, txt.c_str()); fclose(f); }
  }
  if (!GBEST.finished || r.minutes > GBEST.minutes) {
    GBEST = r; GBEST_FULL = full;
    string tmp = OUTF + ".tmp"; FILE* f = fopen(tmp.c_str(), "w");
    if (f) { fprintf(f, "%lld\n%s\n", r.minutes, txt.c_str()); fclose(f); rename(tmp.c_str(), OUTF.c_str()); }
    printf("[%7.1fs] NEW BEST %lld minutes (%zu jumps, %s, %lld playouts)\n", elapsed(), r.minutes, full.size(), what, (long long)PLAYOUTS);
    if (r.minutes >= VERIFYMIN) {
      string line; bool ok = run_checker(txt, line);
      printf("  checker: %s\n", line.c_str());
      if (ok && r.minutes > RECORD) {
        string fn = RECDIR + "/NEW_RECORD_" + to_string(r.minutes) + ".txt"; FILE* g = fopen(fn.c_str(), "w");
        if (g) { fprintf(g, "%lld\n%s\n", r.minutes, txt.c_str()); fclose(g); printf("  NEW RECORD -> %s\n", fn.c_str()); }
      }
    }
    fflush(stdout);
  }
}

// ---------------------------------------------------------------- NMCS / Lazy NMCS
static int BASEN = 1, LAZY = 0, LAZYN = 4, LAZYK = 3, LAZYMIN = 2, LAZYMEAN = 0, TOPLEVEL = 2;
static double LAZYFRAC = 0;
static thread_local vector<Mv> TL_PREFIX;

static Result bottom(const X& c, std::mt19937& rng) {
  Result best;
  for (int i = 0; i < BASEN; i++) { Result r = playout(c, rng); if (r.score > best.score) best = std::move(r); }
  return best;
}

static Result nmcs(int level, const X& root, std::mt19937& rng, const Result* init) {
  if (level <= 0) return bottom(root, rng);
  X x = root; Result best; if (init) best = *init;
  vector<Mv> played; vector<pair<int, int>> mv; vector<int> chosen; vector<pair<double, int>> rate;
  auto consider = [&](Result&& r, Mv m) {  // r.seq is relative to the child after m
    if (r.score > best.score) {
      r.seq.insert(r.seq.begin(), m); r.seq.insert(r.seq.begin(), played.begin(), played.end());
      best = std::move(r);
      if (level == TOPLEVEL && best.finished) {
        vector<Mv> full = TL_PREFIX; full.insert(full.end(), best.seq.begin(), best.seq.end());
        report(full, LAZY ? "lnmcs" : "nmcs");
      }
    }
  };
  while (!STOP) {
    pinned_moves(x.s, 3, true, mv);
    if (mv.empty()) break;
    Mv memo{-1, -1}; if (best.seq.size() > played.size()) memo = best.seq[played.size()];
    chosen.clear();
    bool lazyHere = LAZY && level >= LAZYMIN && level >= 2;
    if (lazyHere) {
      rate.clear();
      for (size_t i = 0; i < mv.size(); i++) {
        auto [d, q] = mv[i]; X c = x; xapply(c, d, q);
        if (q == H8) { Result r; set_finished(r, c.s.T); r.seq.clear(); consider(std::move(r), {d, q}); continue; }
        double acc = LAZYMEAN ? 0 : -1e18;
        for (int k = 0; k < LAZYN; k++) {
          Result r = playout(c, rng);
          if (LAZYMEAN) acc += r.soft; else acc = max(acc, r.score);
          consider(std::move(r), {d, q});
        }
        rate.push_back({acc, (int)i});
      }
      sort(rate.begin(), rate.end(), [](auto& a, auto& b) { return a.first > b.first; });
      int keep = max(LAZYK, (int)ceil(LAZYFRAC * rate.size()));
      for (int j = 0; j < (int)rate.size() && j < keep; j++) chosen.push_back(rate[j].second);
      for (size_t i = 0; i < mv.size(); i++)
        if (mv[i].first == memo.delta && mv[i].second == memo.q && mv[i].second != H8 &&
            find(chosen.begin(), chosen.end(), (int)i) == chosen.end()) chosen.push_back((int)i);
    } else {
      for (size_t i = 0; i < mv.size(); i++) chosen.push_back((int)i);
    }
    for (int i : chosen) {
      if (STOP) break;
      auto [d, q] = mv[i]; X c = x; xapply(c, d, q);
      if (q == H8) { if (!lazyHere) { Result r; set_finished(r, c.s.T); consider(std::move(r), {d, q}); } continue; }
      consider(level == 1 ? bottom(c, rng) : nmcs(level - 1, c, rng, nullptr), {d, q});
    }
    if (best.seq.size() <= played.size()) break;
    Mv m = best.seq[played.size()]; xapply(x, m.delta, m.q); played.push_back(m);
    if (m.q == H8) break;
  }
  if (best.score < -1e17) { best.seq.clear(); set_unfinished(best, x.s); best.seq = played; }
  return best;
}

// ---------------------------------------------------------------- SP-MCTS with max-backup
struct Node {
  int32_t child = -1; int16_t nch = 0; int16_t delta = 0; uint8_t q = 0, flags = 0;  // 1 expanded, 2 terminal, 4 exhausted
  int32_t n = 0; float prior = 0; double mx = -1e18, sum = 0;
};
static double UCTC = 0.3, MIX = 0.0, SPAN = 1000, FPU = 0.05; static int PUCT = 0, EXPANDN = 1;
static long long COMMIT = 5000; static long long MAXNODES = 4000000;

static Result mcts(const X& root0, std::mt19937& rng, const Result* init) {
  vector<Node> T; T.reserve(min<long long>(MAXNODES, 1 << 20)); T.emplace_back(); int root = 0;
  X cur = root0; vector<Mv> committed;
  Result best; if (init) best = *init;
  double hiV = best.score > -1e17 ? best.soft : -1e18;
  auto norm = [&](double v) { double lo = hiV - SPAN; double t = (v - lo) / SPAN; return t < 0 ? 0 : (t > 1 ? 1 : t); };
  vector<pair<int, int>> mv; vector<double> lg; vector<int> path; vector<Mv> tseq;
  auto expand = [&](int ni, const X& x) {
    pinned_moves(x.s, 3, true, mv);
    if ((long long)T.size() + (long long)mv.size() > MAXNODES) return false;
    T[ni].flags |= 1;
    if (mv.empty()) { T[ni].flags |= 2; return true; }
    move_logits(x, mv, lg);
    double mxl = -1e300; for (size_t i = 0; i < mv.size(); i++) if (mv[i].second != H8) mxl = max(mxl, lg[i]);
    double z = 0; for (size_t i = 0; i < mv.size(); i++) { lg[i] = mv[i].second == H8 ? 0 : exp(lg[i] - mxl); z += lg[i]; }
    int c0 = (int)T.size(); T[ni].child = c0; T[ni].nch = (int16_t)mv.size();
    for (size_t i = 0; i < mv.size(); i++) {
      Node c; c.delta = (int16_t)mv[i].first; c.q = (uint8_t)mv[i].second;
      c.prior = z > 0 ? (float)(lg[i] / z) : 0.f; if (mv[i].second == H8) { c.flags = 1 | 2; c.prior = 0.2f; }
      T.push_back(c);
    }
    return true;
  };
  long long sinceCommit = 0;
  while (!STOP) {
    // ---- one iteration
    X x = cur; int ni = root; path.clear(); path.push_back(ni); tseq.clear();
    bool deadend = false;
    while ((T[ni].flags & 1) && !(T[ni].flags & 2)) {
      Node& P = T[ni]; int bi = -1; double bv = -1e300;
      double lnN = log((double)max(1, P.n)), sqN = sqrt((double)max(1, P.n));
      double qp = P.n ? (1 - MIX) * norm(P.mx) + MIX * norm(P.sum / P.n) : 0.5;
      for (int k = 0; k < P.nch; k++) {
        const Node& c = T[P.child + k];
        if (c.flags & 4) continue;
        double v;
        if (PUCT) {
          double qc = c.n ? (1 - MIX) * norm(c.mx) + MIX * norm(c.sum / c.n) : qp - FPU;
          v = qc + UCTC * c.prior * sqN / (1 + c.n);
        } else {
          if (c.n == 0) v = 1e9 + c.prior + 1e-3 * std::uniform_real_distribution<double>(0, 1)(rng);
          else v = (1 - MIX) * norm(c.mx) + MIX * norm(c.sum / c.n) + UCTC * sqrt(lnN / c.n);
        }
        if (v > bv) { bv = v; bi = P.child + k; }
      }
      if (bi < 0) { deadend = true; break; }  // every child exhausted (marked below)
      xapply(x, T[bi].delta, T[bi].q); tseq.push_back({T[bi].delta, T[bi].q}); ni = bi; path.push_back(ni);
    }
    Result r;
    if (deadend) { T[ni].flags |= 4; }
    else if (T[ni].flags & 2) {
      if (T[ni].q == H8 && !path.empty() && ni != root) set_finished(r, x.s.T); else set_unfinished(r, x.s);
      T[ni].flags |= 4;
    } else {
      if (T[ni].n + 1 >= EXPANDN && expand(ni, x) && !(T[ni].flags & 2)) {
        // step into one child chosen by prior (finishing children are evaluated directly)
        Node& P = T[ni]; double u = std::uniform_real_distribution<double>(0, 1)(rng), acc = 0; int bi = P.child;
        for (int k = 0; k < P.nch; k++) { acc += T[P.child + k].prior / 1.2; bi = P.child + k; if (acc >= u) break; }
        xapply(x, T[bi].delta, T[bi].q); tseq.push_back({T[bi].delta, T[bi].q}); ni = bi; path.push_back(ni);
        if (T[ni].q == H8) { set_finished(r, x.s.T); T[ni].flags |= 4; }
        else { r = playout(x, rng); }
      } else if (T[ni].flags & 2) { set_unfinished(r, x.s); T[ni].flags |= 4; }
      else r = playout(x, rng);
    }
    sinceCommit++;
    if (!deadend) {
      // sequence relative to root0
      vector<Mv> seq = committed; seq.insert(seq.end(), tseq.begin(), tseq.end());
      seq.insert(seq.end(), r.seq.begin(), r.seq.end());  // empty for terminal nodes
      r.seq = std::move(seq);
      if (r.score > best.score) {
        best = r; hiV = max(hiV, best.soft);
        if (best.finished) { vector<Mv> full = TL_PREFIX; full.insert(full.end(), best.seq.begin(), best.seq.end()); report(full, "mcts"); }
      }
      hiV = max(hiV, r.soft);
      for (int p : path) { Node& N = T[p]; N.n++; N.sum += r.soft; if (r.soft > N.mx) N.mx = r.soft; }
    }
    // exhaustion propagates upward
    for (int j = (int)path.size() - 1; j >= 0; j--) {
      Node& N = T[path[j]];
      if (!(N.flags & 4) && (N.flags & 1) && !(N.flags & 2)) {
        bool all = true; for (int k = 0; k < N.nch; k++) if (!(T[N.child + k].flags & 4)) { all = false; break; }
        if (all) N.flags |= 4;
      }
      if (!(N.flags & 4)) break;
    }
    // ---- commit one move along the best sequence
    bool rootDone = T[root].flags & 4;
    if ((COMMIT > 0 && sinceCommit >= COMMIT) || rootDone) {
      sinceCommit = 0;
      if (best.seq.size() <= committed.size()) break;
      Mv m = best.seq[committed.size()];
      if (!(T[root].flags & 1)) expand(root, cur);
      int nr = -1;
      for (int k = 0; k < T[root].nch; k++) { const Node& c = T[T[root].child + k]; if (c.delta == m.delta && c.q == m.q) nr = T[root].child + k; }
      committed.push_back(m); xapply(cur, m.delta, m.q);
      if (m.q == H8 || nr < 0) break;
      // keep only the new root's subtree
      vector<Node> NT; NT.push_back(T[nr]); NT[0].flags &= ~4;
      vector<pair<int, int>> todo{{nr, 0}};  // (old, new)
      for (size_t h = 0; h < todo.size(); h++) {
        auto [o, nn] = todo[h]; const Node& O = T[o];
        if (O.child < 0 || O.nch == 0) continue;
        int c0 = (int)NT.size(); NT[nn].child = c0;
        for (int k = 0; k < O.nch; k++) { NT.push_back(T[O.child + k]); todo.push_back({O.child + k, c0 + k}); }
      }
      T.swap(NT); root = 0;
      // re-derive exhaustion for the new root lazily (flags of descendants are still valid)
    }
  }
  return best;
}

// ---------------------------------------------------------------- seeds
static vector<vector<Mv>> read_tours(const char* fn) {
  vector<vector<Mv>> out; FILE* f = fopen(fn, "r"); if (!f) { fprintf(stderr, "cannot open %s\n", fn); exit(1); }
  static char line[400000];
  while (fgets(line, sizeof line, f)) {
    vector<Mv> t; S s = start_state(); char* p = line; int w; char sq[8]; bool bad = false;
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
      if (d * n != w) bad = true;
      t.push_back({d, q}); apply_move(s, d, q); p++;
    }
    if (!bad && t.size() > 1 && t.back().q == H8) out.push_back(t);
  }
  fclose(f); return out;
}

static void on_signal(int) { STOP = true; }

int main(int argc, char** argv) {
  init_board(); D0 = 1; D0 = h8dist(start_state());
  string method = "lnmcs", seedFile; int LEVEL = 2, THREADS = 1, AMIN = 0, AMAX = 0, BENCH = 0; bool WARM = false;
  double TIME = 0; unsigned RNG = 1; int SEEDTOP = 0, MAXRESTARTS = 0;
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--method") method = nx(); else if (a == "--level") LEVEL = stoi(nx());
    else if (a == "--threads") THREADS = stoi(nx()); else if (a == "--time") TIME = stod(nx()); else if (a == "--rng") RNG = (unsigned)stoul(nx());
    else if (a == "--seed-tours") seedFile = nx(); else if (a == "--seed-top") SEEDTOP = stoi(nx());
    else if (a == "--anchor-min") AMIN = stoi(nx()); else if (a == "--anchor-max") AMAX = stoi(nx());
    else if (a == "--anchor") { AMIN = AMAX = stoi(nx()); }
    else if (a == "--warm") WARM = true; else if (a == "--restarts") MAXRESTARTS = stoi(nx());
    else if (a == "--fw") FW = stod(nx()); else if (a == "--fr") FR = stod(nx()); else if (a == "--fv") FV = stod(nx());
    else if (a == "--fg") FG = stod(nx()); else if (a == "--fd") FD = stod(nx()); else if (a == "--fo") FO = stod(nx()); else if (a == "--tau") TAU = stod(nx()); else if (a == "--reach-bfs") REACHBFS = true;
    else if (a == "--unf-pen") UNFPEN = stod(nx());
    else if (a == "--base-n") BASEN = stoi(nx()); else if (a == "--lazy-n") LAZYN = stoi(nx()); else if (a == "--lazy-k") LAZYK = stoi(nx());
    else if (a == "--lazy-frac") LAZYFRAC = stod(nx()); else if (a == "--lazy-min-level") LAZYMIN = stoi(nx());
    else if (a == "--lazy-stat") { string s = nx(); LAZYMEAN = s == "mean"; }
    else if (a == "--c") UCTC = stod(nx()); else if (a == "--mix") MIX = stod(nx()); else if (a == "--span") SPAN = stod(nx());
    else if (a == "--puct") PUCT = 1; else if (a == "--fpu") FPU = stod(nx()); else if (a == "--expand-n") EXPANDN = stoi(nx());
    else if (a == "--commit-iters") COMMIT = stoll(nx()); else if (a == "--max-nodes") MAXNODES = stoll(nx());
    else if (a == "--out") OUTF = nx(); else if (a == "--pool") POOLF = nx(); else if (a == "--pool-min") POOLMIN = stoll(nx());
    else if (a == "--checker") CHECKER = nx(); else if (a == "--verify-min") VERIFYMIN = stoll(nx());
    else if (a == "--record") RECORD = stoll(nx()); else if (a == "--record-dir") RECDIR = nx();
    else if (a == "--bench") BENCH = stoi(nx());
    else { fprintf(stderr, "unknown option %s (see the comment at the top of nmcs.cpp)\n", a.c_str()); return 1; }
  }
  if (AMAX < AMIN) AMAX = AMIN;
  if (method != "nmcs" && method != "lnmcs" && method != "mcts" && method != "playout") { fprintf(stderr, "--method nmcs|lnmcs|mcts|playout\n"); return 1; }
  LAZY = method == "lnmcs"; TOPLEVEL = LEVEL;
  signal(SIGINT, on_signal); signal(SIGTERM, on_signal);

  vector<vector<Mv>> seeds; if (!seedFile.empty()) seeds = read_tours(seedFile.c_str());
  if (SEEDTOP > 0 && (int)seeds.size() > SEEDTOP) {
    sort(seeds.begin(), seeds.end(), [](const vector<Mv>& a, const vector<Mv>& b) {
      return as_result(make_x(start_state()), a).minutes > as_result(make_x(start_state()), b).minutes; });
    seeds.resize(SEEDTOP);
  }
  if (AMAX > 0 && seeds.empty()) { fprintf(stderr, "--anchor needs --seed-tours\n"); return 1; }
  // self-check: replay every seed through the engine (with the histogram) and the text round trip
  for (size_t i = 0; i < seeds.size() && i < 3; i++) {
    Result r = as_result(make_x(start_state()), seeds[i]);
    X x = make_x(start_state()); bool hok = true;
    for (auto& m : seeds[i]) { xapply(x, m.delta, m.q); X y = make_x(x.s); if (memcmp(x.h, y.h, sizeof x.h)) hok = false; }
    printf("seed %zu: %lld minutes, %zu jumps, finished %d, histogram %s\n", i, r.minutes, seeds[i].size(), (int)r.finished, hok ? "ok" : "MISMATCH");
  }
  if (!seeds.empty()) printf("%zu seed tours\n", seeds.size());
  printf("method %s level %d threads %d anchor %d-%d warm %d | tau %.2f fw %.2f fr %.2f fv %.2f fg %.2f fd %.2f fo %.2f%s | base-n %d lazy n%d k%d frac %.2f min-level %d %s | "
         "mcts c %.2f mix %.2f span %.0f puct %d commit %lld max-nodes %lld\n",
         method.c_str(), LEVEL, THREADS, AMIN, AMAX, (int)WARM, TAU, FW, FR, FV, FG, FD, FO, REACHBFS ? " reach-bfs" : "", BASEN, LAZYN, LAZYK, LAZYFRAC, LAZYMIN,
         LAZYMEAN ? "mean" : "max", UCTC, MIX, SPAN, PUCT, COMMIT, MAXNODES);
  fflush(stdout);
  T0 = std::chrono::steady_clock::now();

  if (BENCH > 0) {  // playout speed and quality from the anchor of the first seed (or a1)
    S s = start_state(); if (!seeds.empty()) for (int i = 0; i < AMIN; i++) apply_move(s, seeds[0][i].delta, seeds[0][i].q);
    X root = make_x(s); std::mt19937 rng(RNG); double tot = 0, len = 0; long long fin = 0, mxm = 0;
    for (int i = 0; i < BENCH; i++) { Result r = playout(root, rng); len += r.seq.size(); if (r.finished) { fin++; tot += r.minutes; mxm = max(mxm, r.minutes); } }
    double dt = elapsed();
    printf("bench: %d playouts in %.2fs (%.0f/s), finished %.1f%%, mean finished minutes %.1f, max %lld, mean length %.1f\n",
           BENCH, dt, BENCH / dt, 100.0 * fin / BENCH, fin ? tot / fin : 0.0, mxm, len / BENCH);
    return 0;
  }

  std::atomic<int> restarts{0};
  auto worker = [&](int tid) {
    std::mt19937 rng(RNG * 1000003u + tid * 7919u + 17);
    while (!STOP) {
      int rs = restarts++; if (MAXRESTARTS > 0 && rs >= MAXRESTARTS) break;
      vector<Mv> seed; if (!seeds.empty()) seed = seeds[std::uniform_int_distribution<size_t>(0, seeds.size() - 1)(rng)];
      int k = 0;
      if (AMAX > 0) { int hi = min<int>(AMAX, (int)seed.size() - 1), lo = min(AMIN, hi); k = std::uniform_int_distribution<int>(lo, hi)(rng); }
      S s = start_state(); vector<Mv> prefix(seed.begin(), seed.begin() + k), rest(seed.begin() + k, seed.end());
      for (auto& m : prefix) apply_move(s, m.delta, m.q);
      X root = make_x(s); TL_PREFIX = prefix;
      Result init; bool useInit = WARM && !rest.empty(); if (useInit) init = as_result(root, rest);
      double t0 = elapsed(); long long p0 = PLAYOUTS;
      Result r;
      if (method == "mcts") r = mcts(root, rng, useInit ? &init : nullptr);
      else if (method == "playout") {
        for (long long i = 0; !STOP && i < 100000; i++) { Result q = playout(root, rng); if (q.score > r.score) { r = q; if (r.finished) { vector<Mv> f = prefix; f.insert(f.end(), r.seq.begin(), r.seq.end()); report(f, "playout"); } } }
      } else r = nmcs(LEVEL, root, rng, useInit ? &init : nullptr);
      vector<Mv> full = prefix; full.insert(full.end(), r.seq.begin(), r.seq.end());
      if (r.finished) report(full, method.c_str());
      std::lock_guard<std::mutex> lk(MTX);
      if (elapsed() - t0 < 1.0 && !(r.finished && r.minutes >= GBEST.minutes) && !STOP) continue;  // quiet for tiny restarts
      printf("[%7.1fs] thread %d restart %d (anchor %d): %s %lld minutes, %zu jumps, %.1fs, %lld playouts (best %lld)%s\n", elapsed(), tid, rs, k,
             r.finished ? "finished" : "unfinished", r.minutes, full.size(), elapsed() - t0, (long long)PLAYOUTS - p0,
             GBEST.finished ? GBEST.minutes : 0LL, STOP ? " [stopped]" : "");
      fflush(stdout);
    }
  };
  vector<std::thread> th; for (int t = 0; t < THREADS; t++) th.emplace_back(worker, t);
  std::atomic<bool> done{false};
  std::thread watcher([&]() { while (!done && !STOP) { if (TIME > 0 && elapsed() >= TIME) STOP = true; std::this_thread::sleep_for(std::chrono::milliseconds(100)); } });
  for (auto& t : th) t.join();
  done = true; watcher.join();
  printf("done: best %lld minutes after %.1fs, %lld playouts (%.0f/s)\n", GBEST.finished ? GBEST.minutes : 0LL, elapsed(), (long long)PLAYOUTS,
         PLAYOUTS / max(1e-9, elapsed()));
}
