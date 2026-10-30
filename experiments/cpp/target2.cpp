// Target-guided beam search: shape the terrain toward a ramp, finish when h8 reachable.
// usage: target W depth targetfile [seed]
// target file lines: square op value weight   (op: = < >)  ; special line "X value" = desired h8
#include "eng.h"

struct Rec { int par; uint8_t q; int waited; };
int W, DEPTH; double MOBW = 0, A1W = 0;
vector<vector<Rec>> hist;
struct Tg { int sq; char op; int v; double w; };
vector<Tg> tg;

vector<pair<int, string>> moves_of(int d, int idx) {
  vector<pair<int, string>> ms;
  while (d >= 0 && idx >= 0) { auto& r = hist[d][idx]; ms.push_back({r.waited, nm(r.q)}); idx = r.par; d--; }
  reverse(ms.begin(), ms.end()); return ms;
}

double penalty(const S& s) {
  double p = 0;
  for (auto& t : tg) {
    int a = s.alt[t.sq];
    if (t.op == '=') p += t.w * abs(a - t.v);
    else if (t.op == '<') p += t.w * max(0, a - t.v);
    else p += t.w * max(0, t.v - a);
  }
  return p;
}

vector<int> static_path_h8(const S& s) {
  int prev[64]; for (int i = 0; i < 64; i++) prev[i] = -2;
  prev[s.pos] = -1; deque<int> dq{(int)s.pos};
  while (!dq.empty()) {
    int u = dq.front(); dq.pop_front();
    for (int k = 0; k < NBN[u]; k++) {
      int q = NBQ[u][k];
      if (!jump_ok(s, u, q, NBT[u][k])) continue;
      if (q == H8) { vector<int> path{H8}; int x = u; while (x != s.pos) { path.push_back(x); x = prev[x]; } reverse(path.begin(), path.end()); return path; }
      if (prev[q] != -2 || s.vis[q] >= 3) continue;
      prev[q] = u; dq.push_back(q);
    }
  }
  return {};
}

int main(int argc, char** argv) {
  init_board();
  W = atoi(argv[1]); DEPTH = atoi(argv[2]); if (argc > 4) MOBW = atof(argv[4]); if (argc > 5) A1W = atof(argv[5]);
  ifstream f(argv[3]); string sq; char op; int v; double w;
  while (f >> sq >> op >> v >> w) tg.push_back({(sq[0] - 'a') + 8 * (sq[1] - '1'), op, v, w});
  vector<S> beam{start_state()}; vector<int> beamIdx{-1};
  int bestX = -1000; vector<pair<int, string>> bestTour;
  vector<pair<int, int>> mv;
  for (int d = 0; d < DEPTH; d++) {
    unordered_map<uint64_t, int> seen;
    vector<S> cand; vector<Rec> crec; vector<double> csc;
    for (size_t bi = 0; bi < beam.size(); bi++) {
      const S& s = beam[bi];
      pinned_moves(s, 3, true, mv);
      for (auto [delta, q] : mv) {
        S c = s; int wt = apply_move(c, delta, q);
        if (q == H8) {
          if (c.alt[H8] > bestX) { bestX = c.alt[H8]; bestTour = moves_of(d - 1, beamIdx[bi]); bestTour.push_back({wt, "h8"});
            printf("FINISH X=%d depth=%d\n", bestX, d + 1); fflush(stdout); }
          continue;
        }
        uint64_t h = hashS(c); if (seen.count(h)) continue;
        seen[h] = cand.size();
        bool h8r; int M; static_reach(c, 3, h8r, M);
        uint64_t rm = static_reach(c, 3, h8r, M);
        double mob = __builtin_popcountll(rm);
        double sc = -penalty(c) + MOBW * mob + ((rm >> A1) & 1 ? A1W : 0) + 0.001 * M;
        if (h8r && c.alt[H8] > bestX) {
          vector<int> path = static_path_h8(c);
          if (!path.empty()) {
            bestX = c.alt[H8]; bestTour = moves_of(d - 1, beamIdx[bi]); bestTour.push_back({wt, nm(q)});
            for (int x : path) bestTour.push_back({0, nm(x)});
            printf("STATIC FINISH X=%d depth=%d\n", bestX, d + 1); fflush(stdout);
          }
        }
        cand.push_back(c); crec.push_back({beamIdx[bi], (uint8_t)q, wt}); csc.push_back(sc);
      }
    }
    if (cand.empty()) break;
    vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
    if ((int)ord.size() > W) { nth_element(ord.begin(), ord.begin() + W, ord.end(), [&](int a, int b) { return csc[a] > csc[b]; }); ord.resize(W); }
    sort(ord.begin(), ord.end(), [&](int a, int b) { return csc[a] > csc[b]; });
    vector<S> nb; vector<int> nidx; vector<Rec> hrec;
    for (int i = 0; i < (int)ord.size(); i++) { nb.push_back(cand[ord[i]]); hrec.push_back(crec[ord[i]]); nidx.push_back(i); }
    hist.push_back(hrec); beam.swap(nb); beamIdx.swap(nidx);
    if ((d + 1) % 10 == 0) {
      const S& t = beam[0];
      printf("depth %d pen=%.1f eta=%d knight=%s@%d best=%d\n", d + 1, penalty(t), t.alt[H8], nm(t.pos).c_str(), t.alt[t.pos], bestX);
      for (int y = 7; y >= 0; y--) { for (int x = 0; x < 8; x++) printf("%4d", t.alt[x + 8 * y]); printf("\n"); }
      fflush(stdout);
    }
  }
  printf("BEST X=%d\n", bestX);
  for (size_t i = 0; i < bestTour.size(); i++) printf("%s(%d, %s)", i ? ", " : "", bestTour[i].first, bestTour[i].second.c_str());
  printf("\n");
}
