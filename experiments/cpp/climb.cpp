// Beam search maximizing the highest altitude the knight can statically reach (any point except h8).
#include "eng.h"
int main(int argc, char** argv) {
  init_board();
  int W = atoi(argv[1]), DEPTH = atoi(argv[2]); int mode = argc > 3 ? atoi(argv[3]) : 0;
  vector<S> beam{start_state()};
  int best = 0; vector<pair<int,int>> mv;
  for (int d = 0; d < DEPTH; d++) {
    unordered_set<uint64_t> seen; vector<S> cand; vector<double> sc;
    for (auto& s : beam) {
      pinned_moves(s, 3, false, mv);
      for (auto [delta, q] : mv) {
        S c = s; apply_move(c, delta, q);
        uint64_t h = hashS(c); if (seen.count(h)) continue; seen.insert(h);
        bool h8r; int M; uint64_t rm = static_reach(c, 3, h8r, M);
        if (M > best) { best = M; printf("depth %d: knight can reach altitude %d\n", d + 1, M); fflush(stdout); }
        // secondary: sum of top-3 reachable altitudes, plus count of high points anywhere
        int hi = 0; for (int i = 0; i < 64; i++) if (i != H8 && c.alt[i] >= M - 2) hi++;
        double v = M * 100 + (mode == 1 ? hi : 0) + __builtin_popcountll(rm) * 0.1;
        cand.push_back(c); sc.push_back(v);
      }
    }
    if (cand.empty()) break;
    vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
    if ((int)ord.size() > W) { nth_element(ord.begin(), ord.begin() + W, ord.end(), [&](int a, int b){return sc[a] > sc[b];}); ord.resize(W); }
    vector<S> nb; for (int o : ord) nb.push_back(cand[o]); beam.swap(nb);
  }
  printf("BEST reachable altitude %d\n", best);
}
