// Prove that no strict tour lasts more than X minutes (computer-assisted Theorem 4).
//
// Depth-first search over every strict prefix from the start (all pinned moves, at most 3 visits per
// point, h8 only as the last jump). A prefix is not extended when
//   - its prefix bound ubF is <= X: every strict tour through it lasts at most ubF
//     (Lean: KM5.strict_prefix_bound, lean/KnightMoves5/StrictBound3.lean), or
//   - it is a pit: no legal tour continues past it (Lean: KM5.pit_dead, lean/KnightMoves5/Pit.lean), or
//   - its game state (altitudes, visits, position) was already searched completely from a prefix that
//     took at least as long: the same suffixes are legal from it, so their totals are no larger.
// Finishing jumps are evaluated exactly; one longer than X is reported as a counterexample.
// If the search ends without one, every strict tour lasts at most X.
//
// The state carried along is Theorem 3's XS (lean/KnightMoves5/StrictBound3.lean, XS.step), on the
// integer engine src/eng.h: strict waits are pinned, so every altitude stays an integer.
// tools/prefix_bound.py computes the same bound with exact fractions; `check` mode compares them.
//
// build: g++ -O2 -std=c++17 -o prove_bound tools/prove_bound.cpp
// usage: ./prove_bound X [log2 table size, default 26]     e.g. ./prove_bound 15120 27
//        ./prove_bound check X                             same search without the table, printing
//                                                          per-depth counts (compare with prefix_bound.py)
#include "../src/eng.h"
#include <chrono>

struct Rec { uint8_t own; int16_t del, h, mb; uint8_t cov; };
struct X {
  S s; int m = 0, Hm = 0, B = 0, BnK = 0, Bcov = 0, Z = 0, h8s = 0;
  vector<Rec> recs;
};

// ubF of the Lean file: 421 + Z + 2 dM + gB(K, clamp((K + 188) / 2, B, B + 2 dM)).
static double ubF(const X& x) {
  const S& s = x.s;
  int dM = 1; for (int i = 0; i < 64; i++) if (i != H8) dM += 3 - s.vis[i];
  int cov = 0, bon = 0, recMax = x.m;
  for (auto& r : x.recs) { if (r.cov) cov += r.del; bon += max(0, 2 - r.h); recMax = max<int>(recMax, r.mb); }
  int psi = max(0, x.Hm - 2) - cov + max(0, 2 - (s.alt[s.pos] - x.m)) + bon;
  int used = x.B + x.BnK + x.Bcov + psi;
  int w8 = x.h8s == 2 ? 0 : ((x.h8s == 1 ? x.m + x.Hm : s.alt[H8]) - 2 - recMax);
  double K = used + 2.0 * dM - w8;
  double b = min(max((K + 188) / 2, (double)x.B), x.B + 2.0 * dM);
  double u = max(0.0, 2 * b - K);
  return 421 + x.Z + 2.0 * dM + 65 * b - 7.0 / 6 * u - u * u / 12;
}

// XS.step for the move "sink d at the knight's point, then jump to q".
static void stepX(const X& x, int d, int q, X& y) {
  y = x; const S& s = x.s; int P = s.pos, O = 63 - P, k = s.alt[P];
  auto inG = [&](int i) { return s.alt[i] == k && i != O; };
  int C = 0; for (auto& r : y.recs) if (!r.cov && inG(r.own)) { r.cov = 1; C += r.del; }
  bool h8in = inG(H8);
  int low = k - d; bool fl = low < x.m;
  apply_move(y.s, d, q);
  if (fl) {
    vector<Rec> nr; if (d > 2) nr.push_back({(uint8_t)O, (int16_t)(x.m - low), (int16_t)(k - x.m), (int16_t)x.m, 0});
    for (auto& r : y.recs) if (!r.cov) nr.push_back(r);
    y.recs.swap(nr); y.m = low; y.Hm = y.s.alt[q] - low;
    y.B += x.m - low; if (d <= 2) y.BnK += x.m - low;
    y.h8s = x.h8s == 2 ? 2 : ((x.h8s == 1 || h8in) ? 2 : 0);
  } else {
    y.Hm = max(x.Hm, y.s.alt[q] - x.m);
    y.h8s = x.h8s == 2 ? 2 : ((x.h8s == 1 || h8in) ? 1 : 0);
  }
  y.Bcov += C; y.Z += y.s.alt[q] - low;
}

// Pit test (Lean: KM5.Pit with c the top of the run of altitudes at most 2 apart from the knight's).
static bool pit(const S& s) {
  int k = s.alt[s.pos]; int a[64]; for (int i = 0; i < 64; i++) a[i] = s.alt[i]; std::sort(a, a + 64);
  int c = k; for (int v : a) { if (v <= c + 2) c = max(c, v); else break; }
  if (s.alt[H8] <= c + 2) return false;
  for (int x = 0; x < 64; x++) if (s.alt[x] <= c && s.alt[63 - x] <= c) return false;
  return true;
}

static uint64_t hash2(const S& s) {  // a second, independent 64-bit hash: entries are keyed by 128 bits
  uint64_t h = 0x84222325cbf29ce4ULL ^ (s.pos * 0x9E3779B97F4A7C15ULL);
  for (int i = 0; i < 64; i++) { h = (h ^ (uint16_t)s.alt[i]) * 0x100000001b3ULL; h = (h ^ s.vis[i]) * 0x100000001b3ULL; }
  return h ^ (h >> 29);
}
struct TE { uint64_t a, b; int32_t T; };
static vector<TE> tt; static uint64_t ttmask; static bool useTT = true;
static double XT; static long long nodes = 0, pruned = 0, pits = 0, tthits = 0, fins = 0, stored = 0, bad = 0;
static long long bestFin = -1, cnt[200]; static int maxDepth = 0; static double maxPruned = -1e18;
static vector<pair<int, int>> MV[200];

static void dfs(const X& x, int L) {
  nodes++; cnt[L]++; maxDepth = max(maxDepth, L);
  uint64_t a = hashS(x.s, true), b = hash2(x.s), i = a & ttmask;
  if (useTT)
    for (int pr = 0; pr < 8; pr++) {
      TE& e = tt[(i + pr) & ttmask];
      if (e.a == a && e.b == b) { if (x.s.T <= e.T) { tthits++; return; } break; }
      if (e.a == 0 && e.b == 0) break;
    }
  pinned_moves(x.s, 3, true, MV[L]);
  vector<pair<int, int>> mv = MV[L];
  for (auto& [d, q] : mv) {
    if (q == H8) {
      long long T = x.s.T + (long long)d * group_n(x.s, x.s.pos); fins++;
      bestFin = max(bestFin, T);
      if (T > XT) { bad++; printf("COUNTEREXAMPLE: a strict tour of %lld minutes\n", T); }
      continue;
    }
    X y; stepX(x, d, q, y);
    if (pit(y.s)) { pits++; continue; }
    double u = ubF(y);
    if (u <= XT) { pruned++; maxPruned = max(maxPruned, u); continue; }
    dfs(y, L + 1);
  }
  if (!useTT) return;
  for (int pr = 0; pr < 8; pr++) {
    TE& e = tt[(i + pr) & ttmask];
    if (e.a == a && e.b == b) { e.T = max(e.T, x.s.T); return; }
    if (e.a == 0 && e.b == 0) { e = {a, b, x.s.T}; stored++; return; }
  }
  tt[i] = {a, b, x.s.T};  // neighbourhood full: overwrite (forgetting an entry only costs time)
}

int main(int argc, char** argv) {
  init_board();
  if (argc < 2) { fprintf(stderr, "usage: %s X [lg] | check X\n", argv[0]); return 1; }
  int lg = 26;
  if (string(argv[1]) == "check") { useTT = false; XT = atof(argv[2]); lg = 1; }
  else { XT = atof(argv[1]); if (argc > 2) lg = atoi(argv[2]); }
  tt.assign(1ULL << lg, TE{0, 0, 0}); ttmask = (1ULL << lg) - 1;
  auto t0 = std::chrono::steady_clock::now();
  X x0; x0.s = start_state();
  printf("X = %.0f, bound at the start %.4f\n", XT, ubF(x0));
  dfs(x0, 0);
  double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  printf("nodes %lld, pruned by the bound %lld (largest pruned bound %.4f), pits %lld, table hits %lld, "
         "finishes %lld (longest %lld), depth %d, %.1fs\n",
         nodes, pruned, maxPruned, pits, tthits, fins, bestFin, maxDepth, el);
  if (!useTT) { for (int L = 0; L <= maxDepth; L++) printf("%d:%lld ", L, cnt[L]); printf("\n"); }
  if (bad) printf("NOT PROVEN: %lld counterexamples\n", bad);
  else printf("PROVEN: every strict tour lasts at most %.4f minutes\n", max((double)bestFin, maxPruned));
  return bad ? 2 : 0;
}
