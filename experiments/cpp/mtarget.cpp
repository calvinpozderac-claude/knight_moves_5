// Macro-level target-guided beam: each step = static walk + one sit + exit.
// usage: mtarget W depth targetfile [kmax] [mobw]
#include "eng.h"

struct Tg { int sq; char op; int v; double w; };
vector<Tg> tg;
int W, DEPTH, KMAX = 30; double MOBW = 0.0;

struct Node { S s; int par; vector<pair<int, string>> mv; };
vector<vector<Node>> layers;

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

void bfs(const S& s, int prev[64]) {
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

vector<pair<int, string>> full_moves(int d, int idx) {
  vector<vector<pair<int, string>>*> parts;
  while (d >= 0 && idx >= 0) { parts.push_back(&layers[d][idx].mv); idx = layers[d][idx].par; d--; }
  vector<pair<int, string>> out;
  for (int i = (int)parts.size() - 1; i >= 0; i--) out.insert(out.end(), parts[i]->begin(), parts[i]->end());
  return out;
}

int main(int argc, char** argv) {
  init_board();
  W = atoi(argv[1]); DEPTH = atoi(argv[2]);
  ifstream f(argv[3]); string sq; char op; int v; double w;
  while (f >> sq >> op >> v >> w) tg.push_back({(sq[0] - 'a') + 8 * (sq[1] - '1'), op, v, w});
  if (argc > 4) KMAX = atoi(argv[4]);
  if (argc > 5) MOBW = atof(argv[5]);
  layers.push_back({{start_state(), -1, {}}});
  int bestX = -1000; vector<pair<int, string>> bestTour;
  double bestPen = 1e9;
  for (int d = 0; d < DEPTH; d++) {
    auto& L = layers.back();
    vector<Node> cand; vector<double> sc;
    unordered_set<uint64_t> seen;
    for (int i = 0; i < (int)L.size(); i++) {
      const S& s = L[i].s;
      int prev[64]; bfs(s, prev);
      // finishing check
      for (int u = 0; u < 64; u++) {
        if (prev[u] == -2) continue;
        for (int k = 0; k < NBN[u]; k++)
          if (NBQ[u][k] == H8 && jump_ok(s, u, H8, NBT[u][k]) && s.alt[H8] > bestX) {
            bestX = s.alt[H8];
            bestTour = full_moves((int)layers.size() - 1, i);
            vector<int> path; int x = u; while (x != s.pos) { path.push_back(x); x = prev[x]; }
            reverse(path.begin(), path.end());
            for (int p : path) bestTour.push_back({0, nm(p)});
            bestTour.push_back({0, "h8"});
            printf("FINISH X=%d at macro depth %d\n", bestX, d); fflush(stdout);
          }
      }
      for (int P = 0; P < 64; P++) {
        if (prev[P] == -2) continue;
        S wst = s; vector<pair<int, string>> walkmv;
        { vector<int> path; int x = P; while (x != s.pos) { path.push_back(x); x = prev[x]; }
          reverse(path.begin(), path.end());
          for (int p : path) { wst.vis[p]++; wst.pos = p; walkmv.push_back({0, nm(p)}); } }
        AT a = wst.alt[P]; int O = 63 - P;
        for (int k2 = 0; k2 < NBN[P]; k2++) {
          int q = NBQ[P][k2], t = NBT[P][k2];
          if (q == H8 || wst.vis[q] >= 3) continue;
          if (wst.alt[q] == a && q != O) continue;  // group move: skip
          int diff = wst.alt[q] - a;
          int dzs[2], nd = 0;
          if (t == 0) dzs[nd++] = 0; else if (t == 1) { dzs[nd++] = 2; dzs[nd++] = -2; } else { dzs[nd++] = 1; dzs[nd++] = -1; }
          for (int j = 0; j < nd; j++) {
            int kk = dzs[j] - diff;
            if (kk < 1 || kk > KMAX) continue;
            S c = wst; int waited = apply_move(c, kk, q);
            uint64_t h = hashS(c, false);
            if (seen.count(h)) continue; seen.insert(h);
            vector<pair<int, string>> mv = walkmv; mv.push_back({waited, nm(q)});
            bool h8r; int M; uint64_t rm = static_reach(c, 3, h8r, M);
            double scv = -penalty(c) + MOBW * __builtin_popcountll(rm);
            cand.push_back({c, i, mv}); sc.push_back(scv);
          }
        }
      }
    }
    if (cand.empty()) break;
    vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
    if ((int)ord.size() > W) { nth_element(ord.begin(), ord.begin() + W, ord.end(), [&](int a, int b) { return sc[a] > sc[b]; }); ord.resize(W); }
    sort(ord.begin(), ord.end(), [&](int a, int b) { return sc[a] > sc[b]; });
    vector<Node> keep; for (int o : ord) keep.push_back(std::move(cand[o]));
    layers.push_back(std::move(keep));
    const S& t = layers.back()[0].s;
    double pen = penalty(t);
    printf("macro %d cand %zu pen=%.1f eta=%d knight=%s@%d bestX=%d\n", d + 1, cand.size(), pen, t.alt[H8], nm(t.pos).c_str(), t.alt[t.pos], bestX);
    if (pen < bestPen || (d + 1) % 5 == 0) {
      bestPen = min(bestPen, pen);
      for (int y = 7; y >= 0; y--) { for (int x = 0; x < 8; x++) printf("%4d", t.alt[x + 8 * y]); printf("\n"); }
    }
    fflush(stdout);
  }
  printf("BEST X=%d\n", bestX);
  for (size_t i = 0; i < bestTour.size(); i++) printf("%s(%d, %s)", i ? ", " : "", bestTour[i].first, bestTour[i].second.c_str());
  printf("\n");
}
