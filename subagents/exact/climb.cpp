// Can the knight reach h8's altitude without jumping to some forbidden squares?
// From the position after the first k moves of a tour, iterative-deepening depth-first search over strict
// (pinned) moves that never jump to a forbidden square or to h8. Goal: the knight stands on a point
// (other than a1) at h8's altitude, so its next sit sinks h8. A transposition table skips positions already
// searched to at least the same remaining depth. Prints the first (shortest) climb found per depth limit.
//
// build: g++ -O2 -std=c++17 -o climb subagents/exact/climb.cpp
// usage: ./climb TOURFILE k "f7,f8,g8" [maxdepth=24] [lgTT=24] [time=600]
#include "../../src/eng.h"
#include <chrono>

static uint64_t FORB = 0; static long long nodes = 0;
struct TE { uint64_t h; int8_t rem; };
static vector<TE> tt; static uint64_t mask;
static vector<pair<int, int>> path, found;
static std::chrono::steady_clock::time_point t0; static double TLIM; static bool timedOut = false;

static bool dfs(const S& s, int rem) {
  if (s.pos != A1 && s.alt[s.pos] == s.alt[H8]) { found = path; return true; }
  if (rem == 0 || timedOut) return false;
  if ((++nodes & 0xFFFFF) == 0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() > TLIM) { timedOut = true; return false; }
  // a jump climbs at most 2: the knight needs at least ceil((alt(h8) - alt(knight)) / 2) more moves
  int gap = s.alt[H8] - s.alt[s.pos]; if (gap > 2 * rem) return false;
  uint64_t h = hashS(s, true); TE& e = tt[h & mask];
  if (e.h == h && e.rem >= rem) return false;
  e.h = h; e.rem = (int8_t)rem;
  vector<pair<int, int>> mv; pinned_moves(s, 3, false, mv);
  for (auto& [d, q] : mv) {
    if (FORB >> q & 1) continue;
    S n = s; apply_move(n, d, q);
    path.push_back({d, q});
    if (dfs(n, rem - 1)) return true;
    path.pop_back();
  }
  return false;
}

int main(int argc, char** argv) {
  init_board();
  if (argc < 4) { fprintf(stderr, "usage: %s TOUR k SQUARES [maxdepth] [lgTT] [time]\n", argv[0]); return 1; }
  std::ifstream in(argv[1]); string l, line; while (getline(in, l)) if (l.find('(') != string::npos) { line = l; break; }
  int k = atoi(argv[2]); string fs = argv[3];
  for (size_t p = 0; p + 1 < fs.size(); p += 3) FORB |= 1ULL << ((fs[p] - 'a') + 8 * (fs[p + 1] - '1'));
  int maxd = argc > 4 ? atoi(argv[4]) : 24, lg = argc > 5 ? atoi(argv[5]) : 24; TLIM = argc > 6 ? atof(argv[6]) : 600;
  S s = start_state(); size_t c = 0; int i = 0;
  while (i < k && (c = line.find('(', c)) != string::npos) {
    int w; char sq[3]; sscanf(line.c_str() + c, "(%d, %2s)", &w, sq); c++;
    int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); apply_move(s, w ? w / n : 0, q); i++;
  }
  printf("after %d moves: knight %s at %d, h8 at %d, T=%d; forbidden %s\n", k, nm(s.pos).c_str(), s.alt[s.pos], s.alt[H8], s.T, fs.c_str());
  tt.assign(1ULL << lg, TE{0, -1}); mask = (1ULL << lg) - 1; t0 = std::chrono::steady_clock::now();
  for (int d = 1; d <= maxd && !timedOut; d++) {
    if (dfs(s, d)) {
      S t = s; string o;
      for (auto& [dd, q] : found) { int T0 = t.T; apply_move(t, dd, q); char b[32]; snprintf(b, 32, " (%d,%s)", t.T - T0, nm(q).c_str()); o += b; }
      printf("CLIMB FOUND in %d moves (nodes %lld):%s -> knight %s at %d = h8\n", d, nodes, o.c_str(), nm(t.pos).c_str(), t.alt[t.pos]);
      return 0;
    }
    printf("  depth %d: no climb (nodes %lld, %.1fs)\n", d, nodes, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count()); fflush(stdout);
  }
  printf("%s: no climb within depth %d\n", timedOut ? "TIMEOUT" : "DONE", maxd);
}
