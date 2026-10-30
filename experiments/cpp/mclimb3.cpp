// Macro-level beam (each step = static walk + one sit + exit). Score favours high reachable altitude
// near h8 and high h8; records static finishes.
#include "eng.h"
struct Node { S s; int par; vector<pair<int, string>> mv; };
vector<vector<Node>> layers;
int W, DEPTH, KMAX, MODE;
void bfs(const S& s, int prev[64]) {
  for (int i = 0; i < 64; i++) prev[i] = -2;
  prev[s.pos] = -1; int qu[64], h = 0, t = 0; qu[t++] = s.pos;
  while (h < t) { int u = qu[h++];
    for (int k = 0; k < NBN[u]; k++) { int q = NBQ[u][k];
      if (q == H8 || prev[q] != -2 || s.vis[q] >= 3) continue;
      if (!jump_ok(s, u, q, NBT[u][k])) continue; prev[q] = u; qu[t++] = q; } }
}
vector<pair<int, string>> full_moves(int d, int idx) {
  vector<vector<pair<int, string>>*> parts;
  while (d >= 0 && idx >= 0) { parts.push_back(&layers[d][idx].mv); idx = layers[d][idx].par; d--; }
  vector<pair<int, string>> out;
  for (int i = (int)parts.size() - 1; i >= 0; i--) out.insert(out.end(), parts[i]->begin(), parts[i]->end());
  return out;
}
int main(int argc, char** argv) {
  init_board();
  W = atoi(argv[1]); DEPTH = atoi(argv[2]); KMAX = atoi(argv[3]); MODE = atoi(argv[4]);
  layers.push_back({{start_state(), -1, {}}});
  int bestX = -1000, bestM = -1000;
  for (int d = 0; d < DEPTH; d++) {
    auto& L = layers.back();
    vector<Node> cand; vector<double> sc; unordered_set<uint64_t> seen;
    for (int i = 0; i < (int)L.size(); i++) {
      const S& s = L[i].s; int prev[64]; bfs(s, prev);
      for (int u = 0; u < 64; u++) if (prev[u] != -2)
        for (int k = 0; k < NBN[u]; k++) if (NBQ[u][k] == H8 && jump_ok(s, u, H8, NBT[u][k]) && s.alt[H8] > bestX) {
          bestX = s.alt[H8]; auto t = full_moves((int)layers.size() - 1, i);
          vector<int> path; int x = u; while (x != s.pos) { path.push_back(x); x = prev[x]; } reverse(path.begin(), path.end());
          for (int p : path) t.push_back({0, nm(p)}); t.push_back({0, "h8"});
          printf("FINISH X=%d macro=%d: ", bestX, d); for (size_t j = 0; j < t.size(); j++) printf("%s(%d, %s)", j ? ", " : "", t[j].first, t[j].second.c_str()); printf("\n"); fflush(stdout);
        }
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
            bool h8r; int M; static_reach(c, 3, h8r, M);
            // best entry readiness: max over h8 neighbours N reachable of (alt N)
            int eta = c.alt[H8];
            double v;
            if (MODE == 0) v = M * 100.0 + eta;
            else if (MODE == 1) v = min(M + 2, eta) * 100.0 + M * 10 + eta;
            else v = (M + eta) * 50.0 - abs(eta - M - 2) * 30.0;
            vector<pair<int, string>> mv = walk; mv.push_back({waited, nm(q)});
            cand.push_back({c, i, mv}); sc.push_back(v);
            if (M > bestM) { bestM = M; }
          }
        }
      }
    }
    if (cand.empty()) break;
    vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
    if ((int)ord.size() > W) { nth_element(ord.begin(), ord.begin() + W, ord.end(), [&](int a, int b) { return sc[a] > sc[b]; }); ord.resize(W); }
    sort(ord.begin(), ord.end(), [&](int a, int b) { return sc[a] > sc[b]; });
    vector<Node> keep; for (int o : ord) keep.push_back(std::move(cand[o])); layers.push_back(std::move(keep));
    bool h8r; int M; static_reach(layers.back()[0].s, 3, h8r, M);
    printf("macro %d cand %zu topM=%d eta=%d bestM=%d bestX=%d\n", d + 1, cand.size(), M, layers.back()[0].s.alt[H8], bestM, bestX);
    if (d + 1 == DEPTH) { for (int b = 0; b < 3; b++) { const S& t = layers.back()[b].s; bool hr; int MM; uint64_t rm = static_reach(t, 3, hr, MM);
      printf("state %d knight %s M=%d\n", b, nm(t.pos).c_str(), MM);
      for (int y = 7; y >= 0; y--) { for (int x = 0; x < 8; x++) { int i = x + 8*y; printf("%4d%c", t.alt[i], (rm >> i & 1) ? '*' : ' '); } printf("\n"); }
      auto mv = full_moves((int)layers.size() - 1, b); for (size_t j = 0; j < mv.size(); j++) printf("%s(%d, %s)", j ? ", " : "", mv[j].first, mv[j].second.c_str()); printf("\n"); } }
    fflush(stdout);
  }
}
