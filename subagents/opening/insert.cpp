// Insertion polish: at every point i of a tour, try inserting a closed loop of k moves (k=2..K) that returns
// the knight to the same point, then replay the rest of the tour (same (delta,q) moves). Keep the best.
#include "../../src/eng.h"
struct Mv { int d, q; };
static vector<Mv> parse(const string& line) {
  vector<Mv> t; S s = start_state(); const char* p = line.c_str(); int w; char sq[8];
  while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
    int q = (sq[0]-'a') + 8*(sq[1]-'1'); int n = group_n(s, s.pos); int d = w / max(1, n);
    t.push_back({d, q}); apply_move(s, d, q); p++;
  }
  return t;
}
static string text(const vector<Mv>& seq) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < seq.size(); i++) { int w = apply_move(s, seq[i].d, seq[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(seq[i].q).c_str()); out += buf; }
  return out;
}
// replay moves from state s; return final T or -1 if illegal
static vector<pair<int,int>> MV;
static bool legal(const S& s, int d, int q, bool last) {
  if (q == H8 && !last) return false;
  pinned_moves(s, 3, last, MV);
  for (auto& [dd, qq] : MV) if (dd == d && qq == q) return true;
  return false;
}
static long long replay(S s, const vector<Mv>& t, size_t from) {
  for (size_t i = from; i < t.size(); i++) {
    if (!legal(s, t[i].d, t[i].q, i + 1 == t.size())) return -1;
    apply_move(s, t[i].d, t[i].q);
  }
  return s.T;
}
long long NODES = 0; int K = 2; long long bestT; vector<Mv> bestSeq, cur, tour; size_t at;
static void dfs(const S& s, int depth, int target) {
  if (depth > 0 && s.pos == target) {
    long long T = replay(s, tour, at);
    if (T > bestT) { bestT = T; bestSeq.assign(tour.begin(), tour.begin() + at); bestSeq.insert(bestSeq.end(), cur.begin(), cur.end());
      bestSeq.insert(bestSeq.end(), tour.begin() + at, tour.end()); fprintf(stderr, "  insert at %zu len %d -> %lld\n", at, depth, T); }
  }
  NODES++; if (depth >= K) return;
  vector<pair<int,int>> mv; pinned_moves(s, 3, false, mv);
  for (auto& [d, q] : mv) { if (q == H8) continue; S c = s; apply_move(c, d, q); cur.push_back({d, q}); dfs(c, depth + 1, target); cur.pop_back(); }
}
int main(int argc, char** argv) {
  init_board(); FILE* f = fopen(argv[1], "r"); char buf[200000]; fgets(buf, sizeof buf, f); fgets(buf, sizeof buf, f); fclose(f);
  K = argc > 2 ? atoi(argv[2]) : 2; int from = argc > 3 ? atoi(argv[3]) : 0;
  tour = parse(buf); bestT = replay(start_state(), tour, 0); fprintf(stderr, "start %lld (%zu moves)\n", bestT, tour.size());
  for (int round = 0; round < 20; round++) {
    long long before = bestT; vector<Mv> base = tour;
    S s = start_state();
    for (at = 0; at < base.size(); at++) {
      if ((int)at >= from) dfs(s, 0, s.pos);
      apply_move(s, base[at].d, base[at].q);
    }
    if (bestT <= before) break;
    tour = bestSeq; fprintf(stderr, "round %d: %lld\n", round, bestT);
  }
  fprintf(stderr, "nodes %lld\n", NODES); printf("%lld\n%s\n", bestT, text(tour).c_str());
}
