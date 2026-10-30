// Beam search for highest finishing altitude, with finishing lookahead.
// usage: maxx2 W depth mode freeMax topK finDepth finW [prefixfile]
#include "eng.h"

struct Rec { int par; uint8_t q; int waited; };
int W, DEPTH, MODE, FREEMAX, TOPK, FIND, FINW;
vector<vector<Rec>> hist;
vector<pair<int, string>> prefix;

vector<pair<int, string>> moves_of(int d, int idx) {
  vector<pair<int, string>> ms;
  while (d >= 0 && idx >= 0) { auto& r = hist[d][idx]; ms.push_back({r.waited, nm(r.q)}); idx = r.par; d--; }
  reverse(ms.begin(), ms.end());
  vector<pair<int, string>> out = prefix; out.insert(out.end(), ms.begin(), ms.end());
  return out;
}

// entry gap: how far h8's neighbours are from a legal entry altitude, weighted by reachability
int entry_gap(const S& s, int M) {
  int eta = s.alt[H8], best = 1000;
  for (int k = 0; k < NBN[H8]; k++) {
    int q = NBQ[H8][k], t = NBT[H8][k];
    if (s.vis[q] >= 3) continue;
    int g;
    int a = s.alt[q];
    if (t == 0) g = abs(a - eta);
    else if (t == 1) g = min(abs(a - eta - 2), abs(a - eta + 2));
    else g = min(abs(a - eta - 1), abs(a - eta + 1));
    int r = max(0, a - (M + 2));
    best = min(best, g + r);
  }
  return best;
}

double score(const S& s, int M, bool h8r) {
  int eta = s.alt[H8];
  int eg = entry_gap(s, M);
  switch (MODE) {
    case 0: return min(eta, M + 2) * 100.0 + eta + M;
    case 1: return min(eta, M + 2) * 100.0 + eta * 3 + M - eg * 20;
    case 2: return eta * 100.0 - eg * 100.0 + M;               // keep eta high but entry close
    case 3: return min(eta, M + 2) * 100.0 - eg * 40 + eta * 5;
  }
  return 0;
}

void gen_moves(const S& s, vector<pair<int, int>>& mv) {
  pinned_moves(s, 3, true, mv);
  if (FREEMAX > 0) {
    int p = s.pos; AT a = s.alt[p];
    for (int k = 0; k < NBN[p]; k++) {
      int q = NBQ[p][k];
      if (NBT[p][k] != 0 || q == H8 || s.vis[q] >= 3 || s.alt[q] != a) continue;
      for (int dd = 1; dd <= FREEMAX; dd++) mv.push_back({dd, q});
    }
  }
}

// finishing lookahead: small beam from s, preferring higher M; returns best X found and its tail moves
int finish_from(const S& s0, vector<pair<int, string>>& tail) {
  struct FS { S s; int par; int w; uint8_t q; };
  vector<vector<FS>> layers; layers.push_back({{s0, -1, 0, 0}});
  int best = -1000; int bl = -1, bi = -1, bw = 0;
  vector<pair<int, int>> mv;
  unordered_set<uint64_t> seen;
  for (int d = 0; d < FIND; d++) {
    vector<FS> nxt; vector<double> sc;
    auto& L = layers.back();
    for (int i = 0; i < (int)L.size(); i++) {
      pinned_moves(L[i].s, 3, true, mv);
      for (auto [delta, q] : mv) {
        S c = L[i].s; int w = apply_move(c, delta, q);
        if (q == H8) { if (c.alt[H8] > best) { best = c.alt[H8]; bl = d; bi = i; bw = w; } continue; }
        // don't allow h8 to drop or a1 sits in the finishing phase
        if (c.alt[H8] < s0.alt[H8]) continue;
        uint64_t h = hashS(c); if (seen.count(h)) continue; seen.insert(h);
        bool h8r; int M; static_reach(c, 3, h8r, M);
        nxt.push_back({c, i, w, (uint8_t)q}); sc.push_back(M * 10.0 - entry_gap(c, M) * 30.0 + (h8r ? 1000 : 0));
      }
    }
    if (nxt.empty()) break;
    vector<int> ord(nxt.size()); iota(ord.begin(), ord.end(), 0);
    sort(ord.begin(), ord.end(), [&](int a, int b) { return sc[a] > sc[b]; });
    if ((int)ord.size() > FINW) ord.resize(FINW);
    vector<FS> keep; for (int o : ord) keep.push_back(nxt[o]);
    layers.push_back(keep);
  }
  if (best > -1000) {
    tail.clear(); tail.push_back({bw, "h8"});
    int d = bl, i = bi;
    while (d >= 0) { auto& f = layers[d][i]; if (f.par < 0) break; tail.push_back({f.w, nm(f.q)}); i = f.par; d--; }
    reverse(tail.begin(), tail.end());
  }
  return best;
}

int main(int argc, char** argv) {
  init_board();
  W = atoi(argv[1]); DEPTH = atoi(argv[2]); MODE = atoi(argv[3]); FREEMAX = atoi(argv[4]);
  TOPK = atoi(argv[5]); FIND = atoi(argv[6]); FINW = atoi(argv[7]);
  S s0 = start_state();
  if (argc > 8) {  // prefix moves file: lines "t q"
    ifstream f(argv[8]); int t; string q;
    while (f >> t >> q) {
      int qi = (q[0] - 'a') + 8 * (q[1] - '1');
      // find delta = t / n
      int n = group_n(s0, s0.pos); int delta = t / n;
      apply_move(s0, delta, qi); prefix.push_back({t, q});
    }
  }
  vector<S> beam{s0}; vector<int> beamIdx{-1};
  int bestX = -1000; vector<pair<int, string>> bestTour;
  vector<pair<int, int>> mv;
  for (int d = 0; d < DEPTH; d++) {
    unordered_map<uint64_t, int> seen;
    vector<S> cand; vector<Rec> crec; vector<double> csc;
    for (size_t bi = 0; bi < beam.size(); bi++) {
      const S& s = beam[bi];
      gen_moves(s, mv);
      for (auto [delta, q] : mv) {
        S c = s; int w = apply_move(c, delta, q);
        if (q == H8) {
          if (c.alt[H8] > bestX) {
            bestX = c.alt[H8]; bestTour = moves_of(d - 1, beamIdx[bi]); bestTour.push_back({w, "h8"});
            printf("FINISH X=%d depth=%d\n", bestX, d + 1); fflush(stdout);
          }
          continue;
        }
        uint64_t h = hashS(c);
        if (seen.count(h)) continue;
        bool h8r; int M; static_reach(c, 3, h8r, M);
        seen[h] = cand.size();
        cand.push_back(c); crec.push_back({beamIdx[bi], (uint8_t)q, w}); csc.push_back(score(c, M, h8r));
      }
    }
    if (cand.empty()) break;
    vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
    if ((int)ord.size() > W) { nth_element(ord.begin(), ord.begin() + W, ord.end(), [&](int a, int b) { return csc[a] > csc[b]; }); ord.resize(W); }
    sort(ord.begin(), ord.end(), [&](int a, int b) { return csc[a] > csc[b]; });
    vector<S> nb; vector<int> nidx; vector<Rec> hrec;
    for (int i = 0; i < (int)ord.size(); i++) { nb.push_back(cand[ord[i]]); hrec.push_back(crec[ord[i]]); nidx.push_back(i); }
    hist.push_back(hrec); beam.swap(nb); beamIdx.swap(nidx);
    // finishing lookahead on top states with eta > bestX
    int tried = 0;
    for (int i = 0; i < (int)beam.size() && tried < TOPK; i++) {
      if (beam[i].alt[H8] <= bestX) continue;
      tried++;
      vector<pair<int, string>> tail;
      int X = finish_from(beam[i], tail);
      if (X > bestX) {
        bestX = X; bestTour = moves_of(d, i); bestTour.insert(bestTour.end(), tail.begin(), tail.end());
        printf("LOOKAHEAD FINISH X=%d depth=%d\n", X, d + 1); fflush(stdout);
      }
    }
    const S& top = beam[0]; bool h8r; int M; static_reach(top, 3, h8r, M);
    if ((d + 1) % 5 == 0) { printf("depth %d cand %zu top: eta=%d M=%d eg=%d knight=%s@%d best=%d\n", d + 1, cand.size(), top.alt[H8], M, entry_gap(top, M), nm(top.pos).c_str(), top.alt[top.pos], bestX); fflush(stdout); }
  }
  printf("BEST X=%d\n", bestX);
  for (size_t i = 0; i < bestTour.size(); i++) printf("%s(%d, %s)", i ? ", " : "", bestTour[i].first, bestTour[i].second.c_str());
  printf("\n");
}
