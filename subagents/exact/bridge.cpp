// Exact "bridge" search for the opening: fix the first k moves of a tour and enumerate every strict
// continuation that reaches the tour's dance-start position exactly (the altitudes and the knight's
// square after its first K moves), with visits that leave room for the tour's own remaining moves.
// The best such continuation, followed by the tour's own remaining moves, is a legal strict tour,
// so a bridge that spends more time than the tour's own gives a longer tour.
//
// Pruning (all exact):
//   - visits: a point x may be visited at most cap(x) = 3 - (visits of x in moves K+1..end) times in
//     total by the end of the bridge; h8 never;
//   - a point below its target altitude can only rise as the opposite of a sit, so its opposite must
//     still have a visit left (or hold the knight);
//   - transposition table on (altitudes, visits, position): a state reached again with no more time
//     spent is skipped.
//   - depth: at most `extra` moves more than the tour's own bridge (and the visits bound it anyway).
//
// build: g++ -O2 -std=c++17 -o bridge subagents/exact/bridge.cpp
// usage: ./bridge TOURFILE K k [extra=6] [lgTT=24] [time=1800]
#include "../../src/eng.h"
#include <chrono>

static vector<pair<int, int>> load(const char* f) {  // (delta, q) per move, replayed on the engine
  std::ifstream in(f); string l, line; while (getline(in, l)) if (l.find('(') != string::npos) { line = l; break; }
  vector<pair<int, int>> out; S s = start_state(); size_t k = 0;
  while ((k = line.find('(', k)) != string::npos) {
    int w; char sq[3]; if (sscanf(line.c_str() + k, "(%d, %2s)", &w, sq) != 2) break; k++;
    int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w ? w / n : 0;
    out.push_back({d, q}); apply_move(s, d, q);
  }
  return out;
}

static S target; static int cap[64], maxDepth; static long long nodes = 0, hits = 0, found = 0;
static int bestT = -1; static vector<pair<int, int>> path, bestPath;
struct TE { uint64_t h; int32_t T; };
static vector<TE> tt; static uint64_t mask;
static double TLIM; static std::chrono::steady_clock::time_point t0; static bool timedOut = false;

static bool feasible(const S& s) {
  for (int x = 0; x < 64; x++) {
    if (s.vis[x] > cap[x]) return false;
    if (s.alt[x] < target.alt[x]) { int o = 63 - x; if (s.pos != o && s.vis[o] >= cap[o]) return false; }
  }
  return true;
}

static void dfs(const S& s, int depth) {
  if (timedOut) return;
  if ((++nodes & 0xFFFFF) == 0 &&
      std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() > TLIM) { timedOut = true; return; }
  if (s.pos == target.pos && memcmp(s.alt, target.alt, sizeof s.alt) == 0) {
    found++;
    if (s.T > bestT) { bestT = s.T; bestPath = path; printf("  bridge reaching the target with T=%d (%zu moves)\n", s.T, path.size()); fflush(stdout); }
  }
  if (depth == maxDepth) return;
  uint64_t h = hashS(s, true); TE& e = tt[h & mask];
  if (e.h == h) { if (s.T <= e.T) { hits++; return; } e.T = s.T; } else { e.h = h; e.T = s.T; }
  vector<pair<int, int>> mv; pinned_moves(s, 3, false, mv);
  for (auto& [d, q] : mv) {
    if (s.vis[q] + 1 > cap[q]) continue;
    S n = s; apply_move(n, d, q);
    if (!feasible(n)) continue;
    path.push_back({d, q}); dfs(n, depth + 1); path.pop_back();
  }
}

int main(int argc, char** argv) {
  init_board();
  if (argc < 4) { fprintf(stderr, "usage: %s TOUR K k [extra] [lgTT] [time]\n", argv[0]); return 1; }
  auto tour = load(argv[1]); int K = atoi(argv[2]), k = atoi(argv[3]);
  int extra = argc > 4 ? atoi(argv[4]) : 6, lg = argc > 5 ? atoi(argv[5]) : 24; TLIM = argc > 6 ? atof(argv[6]) : 1800;
  S s = start_state(); S sk;
  for (int i = 0; i < K; i++) { if (i == k) sk = s; apply_move(s, tour[i].first, tour[i].second); }
  if (k == K) sk = s;
  target = s;
  int dv[64] = {0}; for (size_t i = K; i < tour.size(); i++) dv[tour[i].second]++;
  for (int x = 0; x < 64; x++) cap[x] = 3 - dv[x];
  cap[H8] = 0;
  long long tail = 0; { S t = s; for (size_t i = K; i < tour.size(); i++) apply_move(t, tour[i].first, tour[i].second); tail = t.T - s.T; }
  maxDepth = (K - k) + extra;
  tt.assign(1ULL << lg, TE{0, 0}); mask = (1ULL << lg) - 1;
  printf("tour %s: target after %d moves (T=%d, knight %s), remaining moves add %lld; bridge from move %d (T=%d), depth <= %d\n",
         argv[1], K, s.T, nm(s.pos).c_str(), tail, k, sk.T, maxDepth);
  t0 = std::chrono::steady_clock::now();
  dfs(sk, 0);
  double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  printf("%s k=%d: nodes %lld, table hits %lld, bridges %lld, best bridge T=%d (tour's own %d) -> tour %lld, %.1fs\n",
         timedOut ? "TIMEOUT" : "DONE", k, nodes, hits, found, bestT, s.T, bestT + tail, el);
  if (bestT > s.T) {
    S t = sk; string o; int i = 0;
    auto emit = [&](int d, int q) { int T0 = t.T; apply_move(t, d, q); char b[32]; snprintf(b, 32, "%s(%d, %s)", i++ ? ", " : "", t.T - T0, nm(q).c_str()); o += b; };
    S r = start_state(); t = r;
    for (int j = 0; j < k; j++) emit(tour[j].first, tour[j].second);
    for (auto& [d, q] : bestPath) emit(d, q);
    for (size_t j = K; j < tour.size(); j++) emit(tour[j].first, tour[j].second);
    printf("IMPROVED TOUR %d\n%s\n", t.T, o.c_str());
  }
}
