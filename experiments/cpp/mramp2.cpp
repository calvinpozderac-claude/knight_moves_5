// Macro-level beam with a smooth "final ramp" potential for high finishing altitude.
// usage: mramp W depth kmax lam1 lam2 [topK finD]
#include "eng.h"
struct Node { S s; int par; vector<pair<int, string>> mv; };
vector<vector<Node>> layers;
int W, DEPTH, KMAX, TOPK = 10, FIND = 1; double L1, L2;
int ENQ[8], ENT[8], NE = 0, ANQ[8], ANT[8], NA = 0;

void bfs(const S& s, int prev[64]) {
  for (int i = 0; i < 64; i++) prev[i] = -2;
  prev[s.pos] = -1; int qu[64], h = 0, t = 0; qu[t++] = s.pos;
  while (h < t) { int u = qu[h++];
    for (int k = 0; k < NBN[u]; k++) { int q = NBQ[u][k];
      if (q == H8 || prev[q] != -2 || s.vis[q] >= 3) continue;
      if (!jump_ok(s, u, q, NBT[u][k])) continue; prev[q] = u; qu[t++] = q; } }
}
uint64_t fwd(const S& s, int src) {
  uint64_t seen = 1ULL << src; int st[64], sp = 0; st[sp++] = src;
  while (sp) { int u = st[--sp];
    for (int k = 0; k < NBN[u]; k++) { int q = NBQ[u][k];
      if (q == H8 || q == A1 || (seen >> q & 1) || s.vis[q] >= 3) continue;
      if (!jump_ok(s, u, q, NBT[u][k])) continue; seen |= 1ULL << q; st[sp++] = q; } }
  return seen;
}
uint64_t bwd(const S& s, int dst) {
  uint64_t seen = 1ULL << dst; int st[64], sp = 0; st[sp++] = dst;
  while (sp) { int v = st[--sp];
    for (int k = 0; k < NBN[v]; k++) { int u = NBQ[v][k];
      if (u == H8 || u == A1 || (seen >> u & 1) || s.vis[u] >= 3) continue;
      if (!legal_dz(NBT[v][k], s.alt[v] - s.alt[u])) continue; seen |= 1ULL << u; st[sp++] = u; } }
  return seen;
}
static inline int mis(int t, int dz) {  // distance of dz from legal set
  if (t == 0) return abs(dz);
  if (t == 1) return min(abs(dz - 2), abs(dz + 2));
  return min(abs(dz - 1), abs(dz + 1));
}
// min mismatch of a single jump from set A to set B (0 if they intersect)
int gapAB(const S& s, uint64_t A, uint64_t B) {
  if (A & B) return 0;
  int best = 1000;
  for (int u = 0; u < 64; u++) if (A >> u & 1)
    for (int k = 0; k < NBN[u]; k++) { int v = NBQ[u][k]; if (!(B >> v & 1)) continue;
      int m = mis(NBT[u][k], s.alt[v] - s.alt[u]); if (m < best) best = m; }
  return best;
}
double potential(const S& s, int* outX = nullptr) {
  int eta = s.alt[H8], alpha = s.alt[A1];
  uint64_t Bk[8], Fn[8];
  for (int e = 0; e < NE; e++) Bk[e] = (s.vis[ENQ[e]] < 3) ? bwd(s, ENQ[e]) : 0;
  for (int a = 0; a < NA; a++) Fn[a] = (s.vis[ANQ[a]] < 3 && s.alt[ANQ[a]] != alpha) ? fwd(s, ANQ[a]) : 0;
  uint64_t Fk = fwd(s, s.pos) | (1ULL << s.pos);
  bool canA1 = false;
  for (int k = 0; k < NBN[A1]; k++) { int u = NBQ[A1][k]; if ((Fk >> u & 1) && jump_ok(s, u, A1, NBT[A1][k])) canA1 = true; }
  if (s.pos == A1) canA1 = true;
  double best = -1e9; int bx = -1000;
  for (int e = 0; e < NE; e++) {
    if (!Bk[e]) continue;
    int E = ENQ[e], te = ENT[e];
    int dzE[2], ne = 0; if (te == 0) dzE[ne++] = 0; else if (te == 1) { dzE[ne++] = 2; dzE[ne++] = -2; } else { dzE[ne++] = 1; dzE[ne++] = -1; }
    for (int i = 0; i < ne; i++) {
      int Xt = s.alt[E] + dzE[i];
      int D = Xt - eta;
      if (D < 0) continue;
      if (D == 0) {  // no a1 sit needed: knight must reach E
        int g = gapAB(s, Fk, Bk[e]);
        double v = Xt - L2 * g; if (v > best) { best = v; bx = Xt; }
        continue;
      }
      if (s.vis[A1] >= 3) continue;
      for (int a = 0; a < NA; a++) {
        if (!Fn[a]) continue;
        int N = ANQ[a], tn = ANT[a];
        int dzN[2], nn = 0; if (tn == 0) dzN[nn++] = 0; else if (tn == 1) { dzN[nn++] = 2; dzN[nn++] = -2; } else { dzN[nn++] = 1; dzN[nn++] = -1; }
        int g = -1;
        for (int j = 0; j < nn; j++) {
          int need = alpha - D + dzN[j];
          int m = abs(s.alt[N] - need);
          if (g < 0) g = gapAB(s, Fn[a], Bk[e]);
          double v = Xt - L1 * m - L2 * g - (canA1 ? 0 : 3);
          if (v > best) { best = v; bx = Xt; }
        }
      }
    }
  }
  if (outX) *outX = bx;
  return best;
}
vector<pair<int, string>> full_moves(int d, int idx) {
  vector<vector<pair<int, string>>*> parts;
  while (d >= 0 && idx >= 0) { parts.push_back(&layers[d][idx].mv); idx = layers[d][idx].par; d--; }
  vector<pair<int, string>> out;
  for (int i = (int)parts.size() - 1; i >= 0; i--) out.insert(out.end(), parts[i]->begin(), parts[i]->end());
  return out;
}

int finX; vector<pair<int, string>> finTail, curTail;
void fin_dfs(const S& s, int D) {
  int prev[64]; bfs(s, prev);
  for (int u = 0; u < 64; u++) if (prev[u] != -2)
    for (int k = 0; k < NBN[u]; k++) if (NBQ[u][k] == H8 && jump_ok(s, u, H8, NBT[u][k]) && s.alt[H8] > finX) {
      finX = s.alt[H8]; finTail = curTail; vector<int> path; int x = u; while (x != s.pos) { path.push_back(x); x = prev[x]; }
      reverse(path.begin(), path.end()); for (int p : path) finTail.push_back({0, nm(p)}); finTail.push_back({0, "h8"});
    }
  if (D == 0) return;
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
        int kk = dzs[j] - diff; if (kk < 1 || kk > 40) continue;
        S c = w; int waited = apply_move(c, kk, q);
        size_t L = curTail.size(); curTail.insert(curTail.end(), walk.begin(), walk.end()); curTail.push_back({waited, nm(q)});
        fin_dfs(c, D - 1); curTail.resize(L);
      }
    }
  }
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
  W = atoi(argv[1]); DEPTH = atoi(argv[2]); KMAX = atoi(argv[3]); L1 = atof(argv[4]); L2 = atof(argv[5]);
  if (argc > 6) TOPK = atoi(argv[6]); if (argc > 7) FIND = atoi(argv[7]);
  for (int k = 0; k < NBN[H8]; k++) { ENQ[NE] = NBQ[H8][k]; ENT[NE] = NBT[H8][k]; NE++; }
  for (int k = 0; k < NBN[A1]; k++) { ANQ[NA] = NBQ[A1][k]; ANT[NA] = NBT[A1][k]; NA++; }
  layers.push_back({{start_state(), -1, {}}});
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
            vector<pair<int, string>> mv = walk; mv.push_back({waited, nm(q)});
            cand.push_back({c, i, mv}); sc.push_back(potential(c));
          }
        }
      }
    }
    if (cand.empty()) break;
    vector<int> ord(cand.size()); iota(ord.begin(), ord.end(), 0);
    if ((int)ord.size() > W) { nth_element(ord.begin(), ord.begin() + W, ord.end(), [&](int a, int b) { return sc[a] > sc[b]; }); ord.resize(W); }
    sort(ord.begin(), ord.end(), [&](int a, int b) { return sc[a] > sc[b]; });
    double topsc = sc[ord[0]];
    vector<Node> keep; for (int o : ord) keep.push_back(std::move(cand[o])); layers.push_back(std::move(keep));
    for (int i = 0; i < min((int)layers.back().size(), TOPK); i++) {
      const S& s = layers.back()[i].s; int px2; potential(s, &px2); if (px2 <= bestX) continue;
      finX = -1000; curTail.clear(); fin_dfs(s, FIND);
      if (finX > bestX) { bestX = finX; auto t = full_moves((int)layers.size() - 1, i); t.insert(t.end(), finTail.begin(), finTail.end());
        printf("LOOKAHEAD FINISH X=%d: ", bestX); for (size_t j = 0; j < t.size(); j++) printf("%s(%d, %s)", j ? ", " : "", t[j].first, t[j].second.c_str()); printf("\n"); fflush(stdout); }
    }
    int px; potential(layers.back()[0].s, &px);
    printf("macro %d cand %zu top=%.1f potX=%d eta=%d bestX=%d\n", d + 1, cand.size(), topsc, px, layers.back()[0].s.alt[H8], bestX); fflush(stdout);
  }
  // final check
  for (int i = 0; i < (int)layers.back().size(); i++) check_finish(layers.back()[i].s, (int)layers.size() - 1, i);
  printf("BEST X=%d\n", bestX);
}
