// Macro beam minimizing "cost-to-go" to a complete final ramp for target X.
// usage: ctg W depth kmax X [prefixfile] [raiseW]
#include "eng.h"
struct Node { S s; int par; vector<pair<int, string>> mv; };
vector<vector<Node>> layers;
int W, DEPTH, KMAX, XT, DUMPL = -1; double RW = 2.0; bool WANTPATH = false; vector<pair<int,int>> LASTPATH;
int ANQ[8], ANT[8], NA = 0;

void bfs(const S& s, int prev[64]) {
  for (int i = 0; i < 64; i++) prev[i] = -2;
  prev[s.pos] = -1; int qu[64], h = 0, t = 0; qu[t++] = s.pos;
  while (h < t) { int u = qu[h++];
    for (int k = 0; k < NBN[u]; k++) { int q = NBQ[u][k];
      if (q == H8 || prev[q] != -2 || s.vis[q] >= 3) continue;
      if (!jump_ok(s, u, q, NBT[u][k])) continue; prev[q] = u; qu[t++] = q; } }
}
// cost to go: Dijkstra over (point, height) from escape squares to h8 at XT
static const int HLO = -40, HN = 80;
double ctg(const S& s) {
  int eta = s.alt[H8], alpha = s.alt[A1];
  int D = XT - eta; if (D < 0) return 1e9;
  double extra = 0;
  if (D == 0) extra = 0;
  if (s.vis[A1] >= 3 && D > 0) return 1e9;
  int a1f = alpha - D;
  static double dist[64][HN]; static int par[64][HN];
  for (int i = 0; i < 64; i++) for (int h = 0; h < HN; h++) { dist[i][h] = 1e18; par[i][h] = -1; }
  typedef pair<double, int> PI; priority_queue<PI, vector<PI>, greater<PI>> pq;
  auto cost = [&](int P, int h) { int c = s.alt[P]; return h > c ? RW * (h - c) : (double)(c - h); };
  if (D > 0) {
    for (int a = 0; a < NA; a++) {
      int N = ANQ[a], t = ANT[a]; if (s.vis[N] >= 3) continue;
      int dzs[2], nd = 0; if (t == 0) dzs[nd++] = 0; else if (t == 1) { dzs[nd++] = 2; dzs[nd++] = -2; } else { dzs[nd++] = 1; dzs[nd++] = -1; }
      for (int j = 0; j < nd; j++) { int h = a1f + dzs[j]; if (h - HLO < 0 || h - HLO >= HN) continue;
        double c = cost(N, h) + (s.alt[N] == alpha ? 3 : 0);  // N sinking with a1 is awkward
        if (c < dist[N][h - HLO]) { dist[N][h - HLO] = c; pq.push({c, N * HN + (h - HLO)}); } }
    }
  } else {
    // knight just needs a ramp from its current position
    int h = s.alt[s.pos]; dist[s.pos][h - HLO] = 0; pq.push({0, s.pos * HN + (h - HLO)});
  }
  while (!pq.empty()) {
    auto [c, code] = pq.top(); pq.pop();
    int P = code / HN, hi = code % HN, h = hi + HLO;
    if (c > dist[P][hi]) continue;
    for (int k = 0; k < NBN[P]; k++) {
      int Q = NBQ[P][k], t = NBT[P][k];
      int dzs[2], nd = 0; if (t == 0) dzs[nd++] = 0; else if (t == 1) { dzs[nd++] = 2; dzs[nd++] = -2; } else { dzs[nd++] = 1; dzs[nd++] = -1; }
      for (int j = 0; j < nd; j++) {
        int h2 = h + dzs[j];
        if (Q == H8) { if (h2 == XT) { if (WANTPATH) { LASTPATH.clear(); LASTPATH.push_back({H8, h2}); int cc = code; while (cc >= 0) { LASTPATH.push_back({cc / HN, cc % HN + HLO}); cc = par[cc / HN][cc % HN]; } reverse(LASTPATH.begin(), LASTPATH.end()); } return c + extra; } continue; }
        if (Q == A1 || s.vis[Q] >= 3) continue;
        if (h2 > XT + 2 || h2 - HLO < 0 || h2 - HLO >= HN) continue;
        double c2 = c + cost(Q, h2);
        if (c2 < dist[Q][h2 - HLO]) { dist[Q][h2 - HLO] = c2; par[Q][h2 - HLO] = code; pq.push({c2, Q * HN + (h2 - HLO)}); }
      }
    }
  }
  return 1e9;
}
vector<pair<int, string>> full_moves(int d, int idx) {
  vector<vector<pair<int, string>>*> parts;
  while (d >= 0 && idx >= 0) { parts.push_back(&layers[d][idx].mv); idx = layers[d][idx].par; d--; }
  vector<pair<int, string>> out;
  for (int i = (int)parts.size() - 1; i >= 0; i--) out.insert(out.end(), parts[i]->begin(), parts[i]->end());
  return out;
}
int bestX = -1000;
void check_finish(const S& s, int d, int i) {
  int prev[64]; bfs(s, prev);
  for (int u = 0; u < 64; u++) if (prev[u] != -2)
    for (int k = 0; k < NBN[u]; k++) if (NBQ[u][k] == H8 && jump_ok(s, u, H8, NBT[u][k]) && s.alt[H8] > bestX) {
      bestX = s.alt[H8]; auto t = full_moves(d, i);
      vector<int> path; int x = u; while (x != s.pos) { path.push_back(x); x = prev[x]; } reverse(path.begin(), path.end());
      for (int p : path) t.push_back({0, nm(p)}); t.push_back({0, "h8"});
      printf("FINISH X=%d: ", bestX); for (size_t j = 0; j < t.size(); j++) printf("%s(%d, %s)", j ? ", " : "", t[j].first, t[j].second.c_str()); printf("\n"); fflush(stdout);
    }
}
int main(int argc, char** argv) {
  init_board();
  W = atoi(argv[1]); DEPTH = atoi(argv[2]); KMAX = atoi(argv[3]); XT = atoi(argv[4]);
  if (argc > 6) RW = atof(argv[6]); if (argc > 7) DUMPL = atoi(argv[7]);
  for (int k = 0; k < NBN[A1]; k++) { ANQ[NA] = NBQ[A1][k]; ANT[NA] = NBT[A1][k]; NA++; }
  S s0 = start_state(); vector<pair<int, string>> pre;
  if (argc > 5 && string(argv[5]) != "-") { ifstream f(argv[5]); int t; string q;
    while (f >> t >> q) { int qi = (q[0] - 'a') + 8 * (q[1] - '1'); int n = group_n(s0, s0.pos); apply_move(s0, t / n, qi); pre.push_back({t, q}); } }
  layers.push_back({{s0, -1, pre}});
  printf("start ctg %.1f\n", ctg(s0)); fflush(stdout);
  for (int d = 0; d < DEPTH; d++) {
    auto& L = layers.back();
    vector<Node> cand; vector<double> sc; unordered_set<uint64_t> seen;
    for (int i = 0; i < (int)L.size(); i++) {
      const S& s = L[i].s; int prev[64]; bfs(s, prev);
      check_finish(s, (int)layers.size() - 1, i);
      for (int P = 0; P < 64; P++) {
        if (prev[P] == -2) continue;
        S w = s; vector<pair<int, string>> walk; vector<int> path; int x = P;
        while (x != s.pos) { path.push_back(x); x = prev[x]; } reverse(path.begin(), path.end());
        for (int p : path) { w.vis[p]++; w.pos = p; walk.push_back({0, nm(p)}); }
        AT a = w.alt[P]; int O = 63 - P;
        for (int k2 = 0; k2 < NBN[P]; k2++) {
          int q = NBQ[P][k2], t = NBT[P][k2];
          if (q == H8 || w.vis[q] >= 3) continue;
          if (w.alt[q] == a && q != O) continue;
          int diff = w.alt[q] - a; int dzs[2], nd = 0;
          if (t == 0) dzs[nd++] = 0; else if (t == 1) { dzs[nd++] = 2; dzs[nd++] = -2; } else { dzs[nd++] = 1; dzs[nd++] = -1; }
          for (int j = 0; j < nd; j++) {
            int kk = dzs[j] - diff; if (kk < 1 || kk > KMAX) continue;
            S c = w; int waited = apply_move(c, kk, q);
            uint64_t h = hashS(c, false); if (seen.count(h)) continue; seen.insert(h);
            double v = ctg(c); if (v >= 1e8) continue;
            vector<pair<int, string>> mv = walk; mv.push_back({waited, nm(q)});
            cand.push_back({c, i, mv}); sc.push_back(-v);
          }
        }
      }
    }
    if (cand.empty()) break;
    vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
    if ((int)ord.size() > W) { nth_element(ord.begin(), ord.begin() + W, ord.end(), [&](int a, int b) { return sc[a] > sc[b]; }); ord.resize(W); }
    sort(ord.begin(), ord.end(), [&](int a, int b) { return sc[a] > sc[b]; });
    double top = -sc[ord[0]];
    vector<Node> keep; for (int o : ord) keep.push_back(std::move(cand[o])); layers.push_back(std::move(keep));
    printf("macro %d cand %zu best ctg %.1f eta=%d bestX=%d\n", d + 1, cand.size(), top, layers.back()[0].s.alt[H8], bestX); fflush(stdout);
    if (d + 1 == DUMPL) {
      const S& t = layers.back()[0].s; WANTPATH = true; double cc = ctg(t); WANTPATH = false;
      printf("DUMP ctg %.1f knight %s eta %d a1 %d vis(a1) %d\n", cc, nm(t.pos).c_str(), t.alt[H8], t.alt[A1], t.vis[A1]);
      for (int y = 7; y >= 0; y--) { for (int x = 0; x < 8; x++) printf("%4d(%d)", t.alt[x+8*y], t.vis[x+8*y]); printf("\n"); }
      printf("RAMP: "); for (auto& pr : LASTPATH) printf("%s:%d ", nm(pr.first).c_str(), pr.second); printf("\n");
      auto mv = full_moves((int)layers.size() - 1, 0); printf("MOVES "); for (auto& m : mv) printf("%d %s ", m.first, m.second.c_str()); printf("\n");
      return 0;
    }
  }
  for (int i = 0; i < (int)layers.back().size(); i++) check_finish(layers.back()[i].s, (int)layers.size() - 1, i);
  printf("BEST X=%d\n", bestX);
}
