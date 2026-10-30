// Beam search maximizing the largest same-altitude group at the finish.
// usage: grp W depth mode prefixfile [lostW]
#include "eng.h"

struct Rec { int par; uint8_t q; int waited; };
int W, DEPTH, MODE; double LOSTW = 1000;
vector<vector<Rec>> hist;
vector<pair<int, string>> prefix;

vector<pair<int, string>> moves_of(int d, int idx) {
  vector<pair<int, string>> ms;
  while (d >= 0 && idx >= 0) { auto& r = hist[d][idx]; ms.push_back({r.waited, nm(r.q)}); idx = r.par; d--; }
  reverse(ms.begin(), ms.end());
  vector<pair<int, string>> out = prefix; out.insert(out.end(), ms.begin(), ms.end()); return out;
}

int largest_group(const S& s) {
  unordered_map<int, int> c; int b = 0;
  for (int i = 0; i < 64; i++) b = max(b, ++c[s.alt[i]]);
  return b;
}

double score(const S& s) {
  int K = s.alt[s.pos];
  int lost = 0, band = 0, below = 0;
  unordered_map<int, int> c;
  for (int i = 0; i < 64; i++) {
    int a = s.alt[i];
    if (a > K + 2) lost++;
    else if (a >= K - 2) band++;
    else below++;
    c[a]++;
  }
  vector<int> cnt; for (auto& kv : c) cnt.push_back(kv.second);
  sort(cnt.rbegin(), cnt.rend());
  int top2 = cnt[0] + (cnt.size() > 1 ? cnt[1] : 0);
  if (MODE == 0) return -LOSTW * lost + band * 10 + top2;
  if (MODE == 1) return -LOSTW * lost + top2 * 10 + band;
  return -LOSTW * lost + top2 * 10 - K * 0.1;
}

int main(int argc, char** argv) {
  init_board();
  W = atoi(argv[1]); DEPTH = atoi(argv[2]); MODE = atoi(argv[3]);
  if (argc > 5) LOSTW = atof(argv[5]);
  S s0 = start_state();
  { ifstream f(argv[4]); int t; string q;
    while (f >> t >> q) { int qi = (q[0] - 'a') + 8 * (q[1] - '1'); int n = group_n(s0, s0.pos); apply_move(s0, t / n, qi); prefix.push_back({t, q}); } }
  vector<S> beam{s0}; vector<int> beamIdx{-1};
  int best = 0; vector<pair<int, string>> bestTour;
  vector<pair<int, int>> mv;
  for (int d = 0; d < DEPTH; d++) {
    unordered_map<uint64_t, int> seen;
    vector<S> cand; vector<Rec> crec; vector<double> csc;
    for (size_t bi = 0; bi < beam.size(); bi++) {
      const S& s = beam[bi];
      pinned_moves(s, 3, true, mv);
      for (auto [delta, q] : mv) {
        S c = s; int w = apply_move(c, delta, q);
        if (q == H8) {
          int g = largest_group(c);
          if (g > best) { best = g; bestTour = moves_of(d - 1, beamIdx[bi]); bestTour.push_back({w, "h8"});
            printf("FINISH group=%d depth=%d h8alt=%d\n", g, d + 1, c.alt[H8]); fflush(stdout); }
          continue;
        }
        uint64_t h = hashS(c); if (seen.count(h)) continue; seen[h] = 1;
        cand.push_back(c); crec.push_back({beamIdx[bi], (uint8_t)q, w}); csc.push_back(score(c));
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
      const S& t = beam[0]; int K = t.alt[t.pos], lost = 0;
      for (int i = 0; i < 64; i++) if (t.alt[i] > K + 2) lost++;
      printf("depth %d score=%.1f K=%d lost=%d largest=%d best=%d\n", d + 1, csc[ord[0]], K, lost, largest_group(t), best); fflush(stdout);
    }
  }
  printf("BEST group=%d\n", best);
  for (size_t i = 0; i < bestTour.size(); i++) printf("%s(%d, %s)", i ? ", " : "", bestTour[i].first, bestTour[i].second.c_str());
  printf("\n");
}
