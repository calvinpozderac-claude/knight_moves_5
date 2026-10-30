// No-repeat tours (every point visited at most once). Beam search over pinned moves.
// usage: uniq W mode topK finDepth [seed]
//   mode 0: maximize total time   mode 1: maximize number of jumps
#include "eng.h"
struct Rec { int par; uint8_t q; int waited; };
int W, MODE, TOPK, FIND; unsigned SEED = 1;
vector<vector<Rec>> hist;
vector<pair<int, string>> moves_of(int d, int idx) {
  vector<pair<int, string>> ms;
  while (d >= 0 && idx >= 0) { auto& r = hist[d][idx]; ms.push_back({r.waited, nm(r.q)}); idx = r.par; d--; }
  reverse(ms.begin(), ms.end()); return ms;
}
int entry_gap(const S& s, uint64_t rm) {
  int eta = s.alt[H8], best = 50;
  for (int k = 0; k < NBN[H8]; k++) {
    int q = NBQ[H8][k], t = NBT[H8][k]; if (s.vis[q]) continue;
    int a = s.alt[q], g;
    if (t == 0) g = abs(a - eta); else if (t == 1) g = min(abs(a - eta - 2), abs(a - eta + 2)); else g = min(abs(a - eta - 1), abs(a - eta + 1));
    best = min(best, g + ((rm >> q & 1) ? 0 : 3));
  }
  return best;
}
long long bestT = -1; int bestN = -1; vector<pair<int, string>> bestTour;
vector<pair<int, string>> curTail, finTail; long long finBest; int finNodes;
void report(const vector<pair<int, string>>& t, long long T) {
  int n = t.size();
  bool better = MODE == 0 ? (T > bestT) : (n > bestN || (n == bestN && T > bestT));
  if (!better) return;
  bestT = T; bestN = n; bestTour = t;
  printf("FINISH jumps=%d T=%lld: ", n, T);
  for (size_t j = 0; j < t.size(); j++) printf("%s(%d, %s)", j ? ", " : "", t[j].first, t[j].second.c_str());
  printf("\n"); fflush(stdout);
}
void fin_dfs(const S& s, int D, const vector<pair<int, string>>& pre) {
  if (++finNodes > 400000) return;
  vector<pair<int, int>> mv; pinned_moves(s, 1, true, mv);
  for (auto [delta, q] : mv) {
    S c = s; int w = apply_move(c, delta, q);
    curTail.push_back({w, nm(q)});
    if (q == H8) { auto t = pre; t.insert(t.end(), curTail.begin(), curTail.end()); report(t, c.T); }
    else if (D > 1) fin_dfs(c, D - 1, pre);
    curTail.pop_back();
  }
}
int main(int argc, char** argv) {
  init_board();
  W = atoi(argv[1]); MODE = atoi(argv[2]); TOPK = atoi(argv[3]); FIND = atoi(argv[4]); if (argc > 5) SEED = atoi(argv[5]);
  mt19937 rng(SEED);
  vector<S> beam{start_state()}; vector<int> beamIdx{-1};
  vector<pair<int, int>> mv;
  for (int d = 0; d < 64; d++) {
    unordered_set<uint64_t> seen; vector<S> cand; vector<Rec> crec; vector<double> csc;
    for (size_t bi = 0; bi < beam.size(); bi++) {
      const S& s = beam[bi];
      pinned_moves(s, 1, true, mv);
      for (auto [delta, q] : mv) {
        S c = s; int w = apply_move(c, delta, q);
        if (q == H8) { auto t = moves_of(d - 1, beamIdx[bi]); t.push_back({w, "h8"}); report(t, c.T); continue; }
        uint64_t h = hashS(c); if (seen.count(h)) continue; seen.insert(h);
        bool h8r; int M; uint64_t rm = static_reach(c, 1, h8r, M);
        int mob = __builtin_popcountll(rm);
        int eg = entry_gap(c, rm);
        double sc;
        if (MODE == 0) sc = c.T * 1.0 + mob * 3 - eg * 15 + (rng() % 1000) * 1e-3;
        else sc = mob * 10.0 - eg * 6 + (rng() % 1000) * 0.01;
        if (mob < 2 && !h8r) sc -= 1e6;
        cand.push_back(c); crec.push_back({beamIdx[bi], (uint8_t)q, w}); csc.push_back(sc);
      }
    }
    if (cand.empty()) break;
    vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
    if ((int)ord.size() > W) { nth_element(ord.begin(), ord.begin() + W, ord.end(), [&](int a, int b) { return csc[a] > csc[b]; }); ord.resize(W); }
    sort(ord.begin(), ord.end(), [&](int a, int b) { return csc[a] > csc[b]; });
    vector<S> nb; vector<int> nidx; vector<Rec> hrec;
    for (int i = 0; i < (int)ord.size(); i++) { nb.push_back(cand[ord[i]]); hrec.push_back(crec[ord[i]]); nidx.push_back(i); }
    hist.push_back(hrec); beam.swap(nb); beamIdx.swap(nidx);
    for (int i = 0; i < min((int)beam.size(), TOPK); i++) {
      finNodes = 0; curTail.clear(); fin_dfs(beam[i], FIND, moves_of(d, i));
    }
    fprintf(stderr, "depth %d beam %zu best jumps %d T %lld\n", d + 1, beam.size(), bestN, bestT);
  }
  printf("BEST jumps=%d T=%lld\n", bestN, bestT);
}
