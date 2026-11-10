// Exhaustive DFS over the first moves from a1 (any sits, any zero hops) with T <= Tt and length <= L, that
// reach exactly the 8,123 tour's altitudes after move 12 (knight anywhere in the same zero-wait component as b4
// is not required: we require pos == record pos at move M). Cost = visits on points that sit in the record
// after move M (+ illegal if the record's moves M+1..44 can no longer be replayed). Prints the cheapest.
#include "../../src/eng.h"
struct Mv { int d, q; };
vector<Mv> tour, cur, bestSeq; S target; int L, M, Tt, WS[64], best = 1 << 30; long long NODES = 0; int REPLAY_TO = 44;
#include <unordered_map>
std::unordered_map<uint64_t, uint8_t> memo;
static vector<Mv> parse(const string& line) {
  vector<Mv> t; S s = start_state(); const char* p = line.c_str(); int w; char sq[8];
  while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) { int q = (sq[0]-'a') + 8*(sq[1]-'1'); int n = group_n(s, s.pos); int d = w / max(1, n);
    t.push_back({d, q}); apply_move(s, d, q); p++; }
  return t;
}
static int cost(const S& s) { int c = 0; for (int p = 0; p < 64; p++) c += WS[p] * s.vis[p]; return c; }
static bool replay_ok(S s) {
  vector<pair<int,int>> mv;
  for (int i = M; i < REPLAY_TO; i++) { pinned_moves(s, 3, false, mv); bool ok = false; for (auto& [d, q] : mv) if (d == tour[i].d && q == tour[i].q) ok = true; if (!ok) return false; apply_move(s, tour[i].d, tour[i].q); }
  return true;
}
static void dfs(const S& s, int depth) {
  NODES++;
  if (s.T == Tt && s.pos == target.pos && !memcmp(s.alt, target.alt, sizeof s.alt)) {
    int c = cost(s); if (c < best && replay_ok(s)) { best = c; bestSeq = cur; fprintf(stderr, "cost %d len %d\n", c, depth); }
  }
  if (depth >= L) return;
  if (cost(s) >= best) return;
  uint64_t h = hashS(s); auto it = memo.find(h); if (it != memo.end() && it->second <= depth) return; memo[h] = depth;
  vector<pair<int,int>> mv; pinned_moves(s, 3, false, mv);
  for (auto& [d, q] : mv) {
    if (q == H8) continue;
    S c = s; apply_move(c, d, q); if (c.T > Tt) continue;
    cur.push_back({d, q}); dfs(c, depth + 1); cur.pop_back();
  }
}
int main(int argc, char** argv) {
  init_board(); FILE* f = fopen(argv[1], "r"); static char buf[200000]; char* r = fgets(buf, sizeof buf, f); r = fgets(buf, sizeof buf, f); fclose(f); (void)r;
  M = atoi(argv[2]); L = atoi(argv[3]); tour = parse(buf); if (argc > 4) REPLAY_TO = atoi(argv[4]);
  S s = start_state(); for (int i = 0; i < M; i++) apply_move(s, tour[i].d, tour[i].q); target = s; Tt = s.T;
  S u = s; for (int p = 0; p < 64; p++) WS[p] = 0;
  for (size_t i = M; i < tour.size(); i++) { if (tour[i].d > 0) WS[u.pos] = 1; apply_move(u, tour[i].d, tour[i].q); }
  WS[target.pos] = 0; best = cost(target) + 1;  // only report routes at least as cheap as the record
  fprintf(stderr, "record cost %d at move %d (T %d)\n", cost(target), M, Tt);
  dfs(start_state(), 0);
  fprintf(stderr, "nodes %lld, best %d\n", NODES, best);
  string out; S z = start_state(); char b[64];
  for (size_t i = 0; i < bestSeq.size(); i++) { int w = apply_move(z, bestSeq[i].d, bestSeq[i].q); snprintf(b, sizeof b, "%s(%d, %s)", i ? ", " : "", w, nm(bestSeq[i].q).c_str()); out += b; }
  printf("%s\n", out.c_str());
}
