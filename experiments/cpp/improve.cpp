// Iterated exhaustive macro-DFS improvement toward a target terrain.
// usage: improve targetfile Dmax kmax [prefixfile]
// prefix file: whitespace separated "t sq" pairs (waits in minutes)
#include "eng.h"

struct Tg { int sq; char op; int v; double w; };
vector<Tg> tg;
int DMAX, KMAX;

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

struct MM { S s; vector<pair<int, string>> mv; };

void macro_children(const S& s, vector<MM>& out) {
  out.clear();
  int prev[64]; bfs(s, prev);
  for (int P = 0; P < 64; P++) {
    if (prev[P] == -2) continue;
    S w = s; vector<pair<int, string>> walk;
    vector<int> path; int x = P; while (x != s.pos) { path.push_back(x); x = prev[x]; }
    reverse(path.begin(), path.end());
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
        MM m; m.s = w; int waited = apply_move(m.s, kk, q);
        m.mv = walk; m.mv.push_back({waited, nm(q)});
        out.push_back(std::move(m));
      }
    }
  }
}

double bestPen; vector<pair<int, string>> bestSeq; S bestState;
unordered_map<uint64_t, int> memo;
vector<pair<int, string>> curSeq;
long long nodes;

void dfs(const S& s, int depth) {
  nodes++;
  double p = penalty(s);
  if (p < bestPen - 1e-9 || (fabs(p - bestPen) < 1e-9 && curSeq.size() < bestSeq.size() && p < 1e8)) {
    if (p < bestPen - 1e-9) { bestPen = p; bestSeq = curSeq; bestState = s; }
  }
  if (depth == 0) return;
  uint64_t h = hashS(s, true);
  auto it = memo.find(h); if (it != memo.end() && it->second >= depth) return; memo[h] = depth;
  vector<MM> ch; macro_children(s, ch);
  for (auto& m : ch) {
    size_t L = curSeq.size();
    curSeq.insert(curSeq.end(), m.mv.begin(), m.mv.end());
    dfs(m.s, depth - 1);
    curSeq.resize(L);
  }
}

int main(int argc, char** argv) {
  init_board();
  ifstream f(argv[1]); string sq; char op; int v; double w;
  while (f >> sq >> op >> v >> w) tg.push_back({(sq[0] - 'a') + 8 * (sq[1] - '1'), op, v, w});
  DMAX = atoi(argv[2]); KMAX = atoi(argv[3]);
  S s = start_state(); vector<pair<int, string>> seq;
  if (argc > 4) {
    ifstream pf(argv[4]); int t; string q;
    while (pf >> t >> q) {
      int qi = (q[0] - 'a') + 8 * (q[1] - '1');
      int n = group_n(s, s.pos); int delta = t / n;
      apply_move(s, delta, qi); seq.push_back({t, q});
    }
  }
  double cur = penalty(s);
  printf("start pen %.1f\n", cur);
  while (cur > 0) {
    bool improved = false;
    for (int D = 1; D <= DMAX && !improved; D++) {
      bestPen = cur; bestSeq.clear(); memo.clear(); nodes = 0; curSeq.clear();
      dfs(s, D);
      if (bestPen < cur - 1e-9) {
        s = bestState; seq.insert(seq.end(), bestSeq.begin(), bestSeq.end()); cur = bestPen; improved = true;
        printf("D=%d nodes=%lld pen -> %.1f  knight %s@%d\n", D, nodes, cur, nm(s.pos).c_str(), s.alt[s.pos]); fflush(stdout);
      }
    }
    if (!improved) break;
  }
  printf("FINAL pen %.1f\n", cur);
  for (int y = 7; y >= 0; y--) { for (int x = 0; x < 8; x++) printf("%4d", s.alt[x + 8 * y]); printf("\n"); }
  printf("SEQ ");
  for (auto& m : seq) printf("%d %s ", m.first, m.second.c_str());
  printf("\n");
}
