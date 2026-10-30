// Exhaustive macro DFS: walk statically, sit at a corner point (x<=3,y<=3), exit.
// Records the highest h8 altitude at which h8 becomes statically reachable.
// usage: corner maxDepth kmax freeA1 [region]
#include "eng.h"

int MAXD, KMAX, FREEA1, REGION;
int bestX = -1000; int bestM = -1000;
vector<pair<int, string>> cur, bestTour;
long long nodes = 0;
unordered_map<uint64_t, int> seenDepth;  // state hash -> remaining depth explored

bool inRegion(int i) {
  int x = i % 8, y = i / 8;
  if (REGION == 0) return x <= 3 && y <= 3;
  if (REGION == 1) return x + y <= 5;
  return x + y <= 7;
}

// BFS with parents through vis<3 points (no h8)
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

void walk(S& s, int prev[64], int P, vector<pair<int, string>>& mv) {
  vector<int> path; int x = P; while (x != s.pos) { path.push_back(x); x = prev[x]; }
  reverse(path.begin(), path.end());
  for (int q : path) { s.vis[q]++; s.pos = q; mv.push_back({0, nm(q)}); }
}

void dfs(const S& s, int depth) {
  nodes++;
  int prev[64]; bfs(s, prev);
  // track max reachable altitude
  { int mx = -1000; int arg = -1; for (int u = 0; u < 64; u++) if (prev[u] != -2 && s.alt[u] > mx) { mx = s.alt[u]; arg = u; }
    if (mx > bestM) { bestM = mx; printf("M=%d at %s sits=%d nodes=%lld\n", mx, nm(arg).c_str(), MAXD - depth, nodes);
      S c = s; vector<pair<int, string>> mvv = cur; walk(c, prev, arg, mvv);
      for (size_t i = 0; i < mvv.size(); i++) printf("%s(%d, %s)", i ? ", " : "", mvv[i].first, mvv[i].second.c_str());
      printf("\n"); fflush(stdout); } }
  if (false)
  for (int u = 0; u < 64; u++) {
    if (prev[u] == -2) continue;
    for (int k = 0; k < NBN[u]; k++)
      if (NBQ[u][k] == H8 && jump_ok(s, u, H8, NBT[u][k])) {
        if (s.alt[H8] > bestX) {
          bestX = s.alt[H8];
          S c = s; vector<pair<int, string>> mv = cur; walk(c, prev, u, mv); mv.push_back({0, "h8"});
          bestTour = mv;
          printf("X=%d sits=%d nodes=%lld\n", bestX, MAXD - depth, nodes);
          for (size_t i = 0; i < mv.size(); i++) printf("%s(%d, %s)", i ? ", " : "", mv[i].first, mv[i].second.c_str());
          printf("\n"); fflush(stdout);
        }
        goto done_goal;
      }
  }
done_goal:
  if (depth == 0) return;
  uint64_t h = hashS(s, true);
  auto it = seenDepth.find(h);
  if (it != seenDepth.end() && it->second >= depth) return;
  seenDepth[h] = depth;
  for (int P = 0; P < 64; P++) {
    if (prev[P] == -2 || !inRegion(P)) continue;
    S w = s; vector<pair<int, string>> mv;
    walk(w, prev, P, mv);
    int n = group_n(w, P);
    AT a = w.alt[P]; int O = 63 - P;
    vector<pair<int, int>> ex;  // (k, q)
    for (int k2 = 0; k2 < NBN[P]; k2++) {
      int q = NBQ[P][k2], t = NBT[P][k2];
      if (q == H8 || w.vis[q] >= 3) continue;
      if (w.alt[q] == a && q != O) {
        if (t == 0 && FREEA1 && P == A1) for (int k = 1; k <= KMAX; k++) ex.push_back({k, q});
        continue;
      }
      int diff = w.alt[q] - a;
      int dzs[2]; int nd = 0;
      if (t == 0) dzs[nd++] = 0; else if (t == 1) { dzs[nd++] = 2; dzs[nd++] = -2; } else { dzs[nd++] = 1; dzs[nd++] = -1; }
      for (int j = 0; j < nd; j++) { int k = dzs[j] - diff; if (k >= 1 && k <= KMAX) ex.push_back({k, q}); }
    }
    for (auto [k, q] : ex) {
      S c = w; int waited = apply_move(c, k, q);
      size_t L = cur.size();
      cur.insert(cur.end(), mv.begin(), mv.end());
      cur.push_back({waited, nm(q)});
      dfs(c, depth - 1);
      cur.resize(L);
    }
  }
}

int main(int argc, char** argv) {
  init_board();
  MAXD = atoi(argv[1]); KMAX = atoi(argv[2]); FREEA1 = atoi(argv[3]); REGION = argc > 4 ? atoi(argv[4]) : 0;
  S s = start_state();
  for (int d = 1; d <= MAXD; d++) {
    seenDepth.clear();
    int saveMax = MAXD; MAXD = d;
    dfs(s, d);
    MAXD = saveMax;
    printf("== depth %d done, nodes %lld, best M %d\n", d, nodes, bestM); fflush(stdout);
  }
}
