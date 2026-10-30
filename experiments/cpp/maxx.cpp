// Beam search for the highest finishing altitude of h8.
// usage: maxx W depth mode freeMax [lam]
#include "eng.h"

struct Rec { int par; int16_t delta; uint8_t q; int waited; };

int W, DEPTH, MODE, FREEMAX; double LAM = 1.0;
vector<vector<Rec>> hist;

vector<pair<int, string>> moves_of(int d, int idx) {
  vector<pair<int, string>> ms;
  while (d >= 0 && idx >= 0) { auto& r = hist[d][idx]; ms.push_back({r.waited, nm(r.q)}); idx = r.par; d--; }
  reverse(ms.begin(), ms.end());
  return ms;
}

// static path to h8 from s (BFS), returns list of squares (excluding start) ending with h8, or empty
vector<int> static_path_h8(const S& s, int cap) {
  int prev[64]; for (int i = 0; i < 64; i++) prev[i] = -2;
  prev[s.pos] = -1; deque<int> dq{(int)s.pos};
  while (!dq.empty()) {
    int u = dq.front(); dq.pop_front();
    for (int k = 0; k < NBN[u]; k++) {
      int q = NBQ[u][k];
      if (!jump_ok(s, u, q, NBT[u][k])) continue;
      if (q == H8) { vector<int> path{H8}; int x = u; while (x != s.pos) { path.push_back(x); x = prev[x]; } reverse(path.begin(), path.end()); return path; }
      if (prev[q] != -2 || s.vis[q] >= cap) continue;
      prev[q] = u; dq.push_back(q);
    }
  }
  return {};
}

double score(const S& s, int M, bool h8r) {
  int eta = s.alt[H8];
  if (MODE == 0) return min(eta, M + 2) * 100.0 + eta * 10.0 + M + (h8r ? 50 : 0);
  if (MODE == 1) return eta * 10.0 + LAM * M;               // linear tradeoff
  if (MODE == 2) return min(eta, M + 2) * 100.0 + eta * 1.0 - s.alt[A1] * 1.0;
  return 0;
}

int main(int argc, char** argv) {
  init_board();
  W = atoi(argv[1]); DEPTH = atoi(argv[2]); MODE = atoi(argv[3]); FREEMAX = atoi(argv[4]);
  if (argc > 5) LAM = atof(argv[5]);
  vector<S> beam{start_state()};
  vector<int> beamIdx{-1};
  int bestX = -1000; vector<pair<int, string>> bestTour;
  vector<pair<int, int>> mv;
  for (int d = 0; d < DEPTH; d++) {
    unordered_map<uint64_t, int> seen;
    vector<S> cand; vector<Rec> crec; vector<double> csc;
    for (size_t bi = 0; bi < beam.size(); bi++) {
      const S& s = beam[bi];
      pinned_moves(s, 3, true, mv);
      // free group moves (knight move into same group with extra sink)
      if (FREEMAX > 0) {
        int p = s.pos; AT a = s.alt[p];
        for (int k = 0; k < NBN[p]; k++) {
          int q = NBQ[p][k];
          if (NBT[p][k] != 0 || q == H8 || s.vis[q] >= 3) continue;
          if (s.alt[q] != a) continue;
          for (int dd = 1; dd <= FREEMAX; dd++) mv.push_back({dd, q});
        }
      }
      for (auto [delta, q] : mv) {
        S c = s;
        int w = apply_move(c, delta, q);
        if (q == H8) {
          int X = c.alt[H8];
          if (X > bestX) {
            bestX = X; bestTour = moves_of(d - 1, beamIdx[bi]); bestTour.push_back({w, "h8"});
            printf("FINISH X=%d depth=%d\n", X, d + 1); fflush(stdout);
          }
          continue;
        }
        uint64_t h = hashS(c);
        if (seen.count(h)) continue;
        bool h8r; int M;
        static_reach(c, 3, h8r, M);
        double sc = score(c, M, h8r);
        seen[h] = cand.size();
        cand.push_back(c); crec.push_back({beamIdx[bi], (int16_t)delta, (uint8_t)q, w}); csc.push_back(sc);
        if (h8r && c.alt[H8] > bestX) {
          vector<int> path = static_path_h8(c, 3);
          if (!path.empty()) {
            // record tour: moves to c, then static path
            hist.push_back({}); // temp placeholder not used
            hist.pop_back();
            vector<pair<int, string>> t = moves_of(d - 1, beamIdx[bi]);
            t.push_back({w, nm(q)});
            for (int x : path) t.push_back({0, nm(x)});
            bestX = c.alt[H8]; bestTour = t;
            printf("STATIC FINISH X=%d depth=%d\n", bestX, d + 1); fflush(stdout);
          }
        }
      }
    }
    if (cand.empty()) break;
    vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
    if ((int)ord.size() > W) {
      nth_element(ord.begin(), ord.begin() + W, ord.end(), [&](int a, int b) { return csc[a] > csc[b]; });
      ord.resize(W);
    }
    sort(ord.begin(), ord.end(), [&](int a, int b) { return csc[a] > csc[b]; });
    vector<S> nb; vector<int> nidx; vector<Rec> hrec;
    for (int i = 0; i < (int)ord.size(); i++) { nb.push_back(cand[ord[i]]); hrec.push_back(crec[ord[i]]); nidx.push_back(i); }
    hist.push_back(hrec);
    beam.swap(nb); beamIdx.swap(nidx);
    const S& top = beam[0]; bool h8r; int M; static_reach(top, 3, h8r, M);
    printf("depth %d cand %zu top: eta=%d M=%d knight=%s@%d best=%d\n", d + 1, cand.size(), top.alt[H8], M, nm(top.pos).c_str(), top.alt[top.pos], bestX);
    fflush(stdout);
  }
  printf("BEST X=%d\n", bestX);
  for (size_t i = 0; i < bestTour.size(); i++) printf("%s(%d, %s)", i ? ", " : "", bestTour[i].first, bestTour[i].second.c_str());
  printf("\n");
}
