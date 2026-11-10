// Re-route the first M moves of a tour: keep its sitting moves (point, sink) in order, but choose every
// zero-wait stretch and every landing freely, to minimise the visit-pair loss at move M:
//   loss = sum over opposite pairs (except a1/h8) of min(v[p], v[63-p])   (bound on dance sits = 93 - loss - ...)
// Beam search segment by segment. Writes the best distinct prefixes (cost order) to OUTPREFIX_<k>.txt.
// usage: reroute TOUR M SLACK BEAM NOUT OUTPREFIX
#include "../../src/eng.h"
#include <functional>
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
static int MODE = 0, CAP[64], WSIT[64], TPOS = -1;
static int loss(const S& s) {
  if (MODE == 1) { int L = 0; for (int p = 0; p < 64; p++) L += WSIT[p] * (s.vis[p] - (p == TPOS && s.vis[p] > 0 ? 1 : 0)); return L; }
  int L = 0; for (int p = 1; p < 32; p++) L += min(s.vis[p], s.vis[63 - p]); return L; }
struct Part { S s; vector<Mv> mv; int cost; };
int SLACK, BEAM;
static void zero_nb(const S& s, int u, vector<int>& out) {
  out.clear();
  for (int k = 0; k < NBN[u]; k++) { int q = NBQ[u][k]; if (q == H8) continue; if (jump_ok(s, u, q, NBT[u][k])) out.push_back(q); }
}
int main(int argc, char** argv) {
  init_board(); FILE* f = fopen(argv[1], "r"); static char buf[200000]; char* r = fgets(buf, sizeof buf, f); r = fgets(buf, sizeof buf, f); fclose(f); (void)r;
  int M = atoi(argv[2]); SLACK = atoi(argv[3]); BEAM = atoi(argv[4]); int NOUT = atoi(argv[5]); string outp = argv[6];
  vector<Mv> tour = parse(buf);
  S s = start_state(); vector<pair<int,int>> sits;  // (point, delta)
  for (int i = 0; i < M; i++) { if (tour[i].d > 0) sits.push_back({s.pos, tour[i].d}); apply_move(s, tour[i].d, tour[i].q); }
  S target = s; TPOS = s.pos;
  if (getenv("MODE1") || getenv("MODE3")) { MODE = 1; S u = s; for (int p = 0; p < 64; p++) { CAP[p] = 3; WSIT[p] = 0; }
    for (size_t i = M; i < tour.size(); i++) { if (tour[i].d > 0) WSIT[u.pos] = 1; CAP[tour[i].q]--; apply_move(u, tour[i].d, tour[i].q); } }
  if (getenv("PERM")) { vector<pair<int,int>> o; const char* p = getenv("PERM"); for (; *p; p++) if (*p >= '0' && *p <= '9') o.push_back(sits[*p - '0']);
    for (size_t i = 0; i < o.size(); i++) sits[i] = o[i]; }
  if (getenv("MODE3")) { for (int p = 0; p < 64; p++) CAP[p] = 3; }   // sitter-weighted cost, record dance not kept
  if (getenv("MODE1") || getenv("MODE3")) {} else if (getenv("MODE2")) { MODE = 1; S u = start_state(); for (int p = 0; p < 64; p++) { CAP[p] = 3; WSIT[p] = 0; }
    for (size_t i = 0; i < tour.size(); i++) { if (tour[i].d > 0) WSIT[u.pos] = 1; apply_move(u, tour[i].d, tour[i].q); } WSIT[TPOS] = 0; }
  else for (int p = 0; p < 64; p++) CAP[p] = 3; fprintf(stderr, "M=%d target pos %s T=%d loss(record prefix)=%d, %zu sits\n", M, nm(target.pos).c_str(), target.T, loss(target), sits.size());
  vector<Part> beam = {{start_state(), {}, 0}};
  for (size_t k = 0; k <= sits.size(); k++) {
    int tgt = k < sits.size() ? sits[k].first : target.pos;
    vector<Part> next; unordered_set<uint64_t> seen;
    for (const Part& P : beam) {
      // distances to tgt in the zero-wait graph of this stage
      int dist[64]; for (int i = 0; i < 64; i++) dist[i] = 99; dist[tgt] = 0;
      vector<int> nb; bool ch = true;
      while (ch) { ch = false; for (int u = 0; u < 64; u++) { zero_nb(P.s, u, nb); for (int q : nb) if (dist[q] + 1 < dist[u]) { dist[u] = dist[q] + 1; ch = true; } } }
      if (dist[P.s.pos] >= 99) continue;
      int maxlen = dist[P.s.pos] + SLACK;
      // DFS over zero walks
      vector<Mv> path;
      std::function<void(const S&, int)> dfs = [&](const S& c, int depth) {
        if (c.pos == tgt) {
          if (k < sits.size()) {
            vector<pair<int,int>> mv; pinned_moves(c, 3, false, mv);
            for (auto& [d, q] : mv) if (d == sits[k].second && q != H8 && c.vis[q] < CAP[q]) {
              S n = c; apply_move(n, d, q); uint64_t h = hashS(n);
              if (!seen.insert(h).second) continue;
              Part np{n, P.mv, loss(n)}; np.mv.insert(np.mv.end(), path.begin(), path.end()); np.mv.push_back({d, q}); next.push_back(np);
            }
          } else {
            bool same = true; for (int i = 0; i < 64; i++) if (c.alt[i] != target.alt[i]) same = false;
            uint64_t h = hashS(c);
            if (same && seen.insert(h).second) { Part np{c, P.mv, loss(c)}; np.mv.insert(np.mv.end(), path.begin(), path.end()); next.push_back(np); }
          }
        }
        if (depth >= maxlen) return;
        vector<int> nb2; zero_nb(c, c.pos, nb2);
        for (int q : nb2) {
          if (c.vis[q] >= CAP[q]) continue;
          if (depth + 1 + dist[q] > maxlen) continue;
          S n = c; apply_move(n, 0, q); path.push_back({0, q}); dfs(n, depth + 1); path.pop_back();
        }
      };
      dfs(P.s, 0);
    }
    sort(next.begin(), next.end(), [](const Part& a, const Part& b) { return a.cost != b.cost ? a.cost < b.cost : a.mv.size() < b.mv.size(); });
    if ((int)next.size() > BEAM) next.resize(BEAM);
    beam = next;
    fprintf(stderr, "segment %zu (to %s): %zu partials, best cost %d\n", k, nm(tgt).c_str(), beam.size(), beam.empty() ? -1 : beam[0].cost);
    if (beam.empty()) return 1;
  }
  for (int i = 0; i < NOUT && i < (int)beam.size(); i++) {
    string fn = outp + "_" + to_string(i) + ".txt"; FILE* o = fopen(fn.c_str(), "w");
    vector<Mv> full = beam[i].mv; for (size_t j = M; j < tour.size(); j++) full.push_back(tour[j]);
    S z = start_state(); for (auto& m : full) apply_move(z, m.d, m.q);
    fprintf(o, "%d\n%s\n", z.T, text(full).c_str()); fclose(o);
    // also: pair-bound on sits from M: sum over pairs of 3 - min(...) style
    printf("%d cost %d len %zu T %d  %s\n", i, beam[i].cost, beam[i].mv.size(), beam[i].s.T, fn.c_str());
  }
}
