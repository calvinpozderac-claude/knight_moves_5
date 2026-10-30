// Find many different openings that can still finish.
// Each attempt plays a random opening from a1 (minLen..maxLen jumps, sinks up to maxSink units),
// then looks for a way to h8: walk without waiting to any reachable square, sit there for 1..8
// units so the next jump becomes legal, repeat up to `depth` times, and finish with a no-wait walk
// into h8. Every success is appended to out as "<minutes> (t, sq), ...".
// usage: opening_search <tours> <seed> <out.txt> [minLen=12] [maxLen=30] [maxSink=6] [depth=3] [budget=4000]
#include "eng.h"

static std::mt19937 rng;
static int MAXSINK_C = 8, BUDGET;
static long nodes;

// zero-wait reachability from s.pos (squares with visits left; h8 excluded)
static void bfs(const S& s, int prev[64]) {
  for (int i = 0; i < 64; i++) prev[i] = -2;
  prev[s.pos] = -1; int qu[64], h = 0, t = 0; qu[t++] = s.pos;
  while (h < t) {
    int u = qu[h++];
    for (int k = 0; k < NBN[u]; k++) {
      int q = NBQ[u][k];
      if (q == H8 || prev[q] != -2 || s.vis[q] >= 3) continue;
      if (!jump_ok(s, u, q, NBT[u][k])) continue;
      prev[q] = u; qu[t++] = q;
    }
  }
}
static S walk_to(const S& s, const int prev[64], int P, vector<pair<int, int>>& mv) {
  vector<int> path; for (int x = P; x != s.pos; x = prev[x]) path.push_back(x);
  reverse(path.begin(), path.end());
  S w = s; for (int p : path) { w.vis[p]++; w.pos = p; mv.push_back({0, p}); }
  return w;
}
// how far h8 is from being enterable (0 = an entry square already has the right altitude)
static int entry_gap(const S& s) {
  int h = s.alt[H8], best = 99;
  for (int k = 0; k < NBN[H8]; k++) {
    int q = NBQ[H8][k], t = NBT[H8][k]; if (s.vis[q] >= 3) continue;
    int a = s.alt[q], g = t == 0 ? abs(a - h) : t == 1 ? min(abs(a - h - 2), abs(a - h + 2)) : min(abs(a - h - 1), abs(a - h + 1));
    best = min(best, g);
  }
  return best;
}
static bool complete(const S& s, int depth, vector<pair<int, int>>& tail) {
  if (++nodes > BUDGET) return false;
  int prev[64]; bfs(s, prev);
  for (int u = 0; u < 64; u++) {
    if (prev[u] == -2) continue;
    for (int k = 0; k < NBN[u]; k++)
      if (NBQ[u][k] == H8 && jump_ok(s, u, H8, NBT[u][k])) {
        walk_to(s, prev, u, tail); tail.push_back({0, H8}); return true;
      }
  }
  if (depth == 0) return false;
  struct C { double key; int P, delta, q; };
  vector<C> cs; vector<pair<int, int>> pm;
  for (int P = 0; P < 64; P++) {
    if (prev[P] == -2) continue;
    vector<pair<int, int>> dummy; S w = walk_to(s, prev, P, dummy);
    pinned_moves(w, 3, false, pm);
    for (auto [d, q] : pm) {
      if (d < 1 || d > MAXSINK_C) continue;
      S c = w; apply_move(c, d, q);
      cs.push_back({entry_gap(c) + std::uniform_real_distribution<double>(0, 1.5)(rng), P, d, q});
    }
  }
  sort(cs.begin(), cs.end(), [](const C& a, const C& b) { return a.key < b.key; });
  int tries = 0;
  for (auto& c : cs) {
    if (++tries > 12 || nodes > BUDGET) break;
    vector<pair<int, int>> mv; S w = walk_to(s, prev, c.P, mv);
    int waited = apply_move(w, c.delta, c.q); mv.push_back({waited, c.q});
    vector<pair<int, int>> rest;
    if (complete(w, depth - 1, rest)) { tail.insert(tail.end(), mv.begin(), mv.end()); tail.insert(tail.end(), rest.begin(), rest.end()); return true; }
  }
  return false;
}

int main(int argc, char** argv) {
  if (argc < 4) { fprintf(stderr, "usage: opening_search <tours> <seed> <out.txt> [minLen] [maxLen] [maxSink] [depth] [budget]\n"); return 1; }
  init_board();
  int N = atoi(argv[1]); rng.seed(atoi(argv[2])); const char* outf = argv[3];
  int minLen = argc > 4 ? atoi(argv[4]) : 12, maxLen = argc > 5 ? atoi(argv[5]) : 30;
  int maxSink = argc > 6 ? atoi(argv[6]) : 6, depth = argc > 7 ? atoi(argv[7]) : 3;
  BUDGET = argc > 8 ? atoi(argv[8]) : 4000;
  FILE* fo = fopen(outf, "a"); int found = 0; long attempts = 0;
  vector<pair<int, int>> pm;
  while (found < N) {
    attempts++;
    S s = start_state(); vector<pair<int, int>> mv;
    int L = std::uniform_int_distribution<int>(minLen, maxLen)(rng);
    for (int i = 0; i < L; i++) {
      pinned_moves(s, 3, false, pm);
      vector<pair<int, int>> ok; for (auto& x : pm) if (x.first <= maxSink) ok.push_back(x);
      if (ok.empty()) break;
      auto [d, q] = ok[std::uniform_int_distribution<int>(0, (int)ok.size() - 1)(rng)];
      int w = apply_move(s, d, q); mv.push_back({w, q});
    }
    if ((int)mv.size() < minLen) continue;
    nodes = 0; vector<pair<int, int>> tail;
    if (!complete(s, depth, tail)) continue;
    S t = s; long T = s.T; for (auto& x : tail) T += x.first;
    fprintf(fo, "%ld ", T);
    for (size_t i = 0; i < mv.size(); i++) fprintf(fo, "%s(%d, %s)", i ? ", " : "", mv[i].first, nm(mv[i].second).c_str());
    for (auto& x : tail) fprintf(fo, ", (%d, %s)", x.first, nm(x.second).c_str());
    fprintf(fo, "\n"); fflush(fo); found++;
  }
  fclose(fo);
  fprintf(stderr, "seed %s: %d tours from %ld openings tried\n", argv[2], found, attempts);
}
