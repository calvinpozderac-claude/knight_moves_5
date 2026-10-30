// Beam search for high finishing altitude using a "final a1-sit" ramp potential.
// usage: ramp W depth lam [prefixfile] [mode]
#include "eng.h"

struct Rec { int par; uint8_t q; int waited; };
int W, DEPTH, MODE = 0; double LAM;
vector<vector<Rec>> hist;
vector<pair<int, string>> prefix;
static int ENTRYQ[6], ENTRYT[6], NE = 0;   // h8 neighbours
static int A1Q[8], A1T[8], NA = 0;         // a1 neighbours

vector<pair<int, string>> moves_of(int d, int idx) {
  vector<pair<int, string>> ms;
  while (d >= 0 && idx >= 0) { auto& r = hist[d][idx]; ms.push_back({r.waited, nm(r.q)}); idx = r.par; d--; }
  reverse(ms.begin(), ms.end());
  vector<pair<int, string>> out = prefix; out.insert(out.end(), ms.begin(), ms.end()); return out;
}

// backward static reachability to target E (points u with a static path u -> ... -> E), excluding h8, a1
uint64_t back_reach(const S& s, int E) {
  uint64_t seen = 1ULL << E; int st[64], sp = 0; st[sp++] = E;
  while (sp) {
    int v = st[--sp];
    for (int k = 0; k < NBN[v]; k++) {
      int u = NBQ[v][k];  // neighbour relation symmetric
      if (u == H8 || u == A1 || (seen >> u & 1) || s.vis[u] >= 3) continue;
      if (!legal_dz(NBT[v][k], s.alt[v] - s.alt[u])) continue;  // u -> v legal
      seen |= 1ULL << u; st[sp++] = u;
    }
  }
  return seen;
}

struct Pot { double score; int X; int D; int N; int E; };

Pot potential(const S& s) {
  Pot best{-1e9, -1000, 0, -1, -1};
  int eta = s.alt[H8], alpha = s.alt[A1];
  if (s.vis[A1] >= 3) return best;
  for (int e = 0; e < NE; e++) {
    int E = ENTRYQ[e], te = ENTRYT[e];
    if (s.vis[E] >= 3) continue;
    uint64_t br = back_reach(s, E);
    int dzE[2], ne = 0;
    if (te == 0) dzE[ne++] = 0; else if (te == 1) { dzE[ne++] = 2; dzE[ne++] = -2; } else { dzE[ne++] = 1; dzE[ne++] = -1; }
    for (int a = 0; a < NA; a++) {
      int N = A1Q[a], tn = A1T[a];
      if (!(br >> N & 1)) continue;
      if (s.alt[N] == alpha) continue;  // N would sink with a1
      int dzN[2], nn = 0;
      if (tn == 0) dzN[nn++] = 0; else if (tn == 1) { dzN[nn++] = 2; dzN[nn++] = -2; } else { dzN[nn++] = 1; dzN[nn++] = -1; }
      for (int i = 0; i < ne; i++)
        for (int j = 0; j < nn; j++) {
          int X = s.alt[E] + dzE[i];
          int D = alpha - (s.alt[N] - dzN[j]);  // a1 must sink to alt[N]-dzN
          int slack = (eta + D) - X;
          double sc = X - LAM * abs(slack) - (D < 1 ? LAM * (1 - D) : 0);
          if (sc > best.score) best = {sc, X, D, N, E};
        }
    }
  }
  return best;
}

int main(int argc, char** argv) {
  init_board();
  W = atoi(argv[1]); DEPTH = atoi(argv[2]); LAM = atof(argv[3]);
  for (int k = 0; k < NBN[H8]; k++) { ENTRYQ[NE] = NBQ[H8][k]; ENTRYT[NE] = NBT[H8][k]; NE++; }
  for (int k = 0; k < NBN[A1]; k++) { A1Q[NA] = NBQ[A1][k]; A1T[NA] = NBT[A1][k]; NA++; }
  S s0 = start_state();
  if (argc > 4 && string(argv[4]) != "-") {
    ifstream f(argv[4]); int t; string q;
    while (f >> t >> q) { int qi = (q[0] - 'a') + 8 * (q[1] - '1'); int n = group_n(s0, s0.pos); apply_move(s0, t / n, qi); prefix.push_back({t, q}); }
  }
  if (argc > 5) MODE = atoi(argv[5]);
  vector<S> beam{s0}; vector<int> beamIdx{-1};
  int bestX = -1000; vector<pair<int, string>> bestTour;
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
          if (c.alt[H8] > bestX) { bestX = c.alt[H8]; bestTour = moves_of(d - 1, beamIdx[bi]); bestTour.push_back({w, "h8"});
            printf("FINISH X=%d depth=%d\n", bestX, d + 1); fflush(stdout); }
          continue;
        }
        uint64_t h = hashS(c); if (seen.count(h)) continue; seen[h] = 1;
        Pot p = potential(c);
        bool h8r; int M; uint64_t rm = static_reach(c, 3, h8r, M);
        double sc = p.score + (MODE == 1 ? 0.01 * M : 0) + ((rm >> A1) & 1 ? 0.5 : 0);
        if (h8r && c.alt[H8] > bestX) {
          // reconstruct static path to h8
          int prev[64]; for (int i = 0; i < 64; i++) prev[i] = -2; prev[c.pos] = -1;
          deque<int> dq{(int)c.pos}; int last = -1;
          while (!dq.empty() && last < 0) { int u = dq.front(); dq.pop_front();
            for (int k = 0; k < NBN[u]; k++) { int qq = NBQ[u][k]; if (!jump_ok(c, u, qq, NBT[u][k])) continue;
              if (qq == H8) { last = u; break; }
              if (prev[qq] != -2 || c.vis[qq] >= 3) continue; prev[qq] = u; dq.push_back(qq); } }
          if (last >= 0) {
            vector<int> path; int x = last; while (x != c.pos) { path.push_back(x); x = prev[x]; } reverse(path.begin(), path.end());
            bestX = c.alt[H8]; bestTour = moves_of(d - 1, beamIdx[bi]); bestTour.push_back({w, nm(q)});
            for (int x2 : path) bestTour.push_back({0, nm(x2)}); bestTour.push_back({0, "h8"});
            printf("STATIC FINISH X=%d depth=%d\n", bestX, d + 1); fflush(stdout);
          }
        }
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
    if ((d + 1) % 5 == 0) {
      Pot p = potential(beam[0]);
      printf("depth %d top score=%.1f potX=%d D=%d N=%s E=%s eta=%d knight=%s@%d best=%d\n", d + 1, p.score, p.X, p.D,
             p.N >= 0 ? nm(p.N).c_str() : "-", p.E >= 0 ? nm(p.E).c_str() : "-", beam[0].alt[H8], nm(beam[0].pos).c_str(), beam[0].alt[beam[0].pos], bestX);
      fflush(stdout);
    }
  }
  printf("BEST X=%d\n", bestX);
  for (size_t i = 0; i < bestTour.size(); i++) printf("%s(%d, %s)", i ? ", " : "", bestTour[i].first, bestTour[i].second.c_str());
  printf("\n");
}
