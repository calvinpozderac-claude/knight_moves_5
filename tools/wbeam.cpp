// Beam search / weighted A* over strict tours, ranked by T + w*(UB - T), where UB is the Theorem 3
// prefix bound (tools/prefix_bound.py, an earlier version of ubF). Layers by depth; the best W states
// of each layer are kept; finishing jumps are scored exactly. Results in proofs/UPPER_BOUND.md.
// build: g++ -O2 -std=c++17 -o wbeam tools/wbeam.cpp     usage: ./wbeam w W out.txt
#include "../src/eng.h"
#include <chrono>
struct Rec { uint8_t own; int16_t del, h, mb; uint8_t cov; };
struct X {
  S s; int m = 0, Hm = 0, B = 0, BnK = 0, Bcov = 0, Z = 0, h8s = 0, h8alt = 0;
  vector<Rec> recs; int par = -1; uint8_t mq = 0, md = 0;  // parent index and move (target, sink)
  double f = 0, ub = 0;
};
static double prefix_ub(const X& x) {
  const S& s = x.s;
  int dM = 1; for (int i = 0; i < 63; i++) dM += 3 - s.vis[i];
  double cov = 0, bon = 0; int mus = x.m;
  for (auto& r : x.recs) { if (r.cov) cov += r.del; bon += max(0, 2 - r.h); mus = max<int>(mus, r.mb); }
  double psi = max(0, x.Hm - 2) - cov + max(0, 2 - (s.alt[s.pos] - x.m)) + bon;
  double used = x.B + x.BnK + x.Bcov + psi;
  double w8 = x.h8s == 2 ? 0 : ((x.h8s == 1 ? x.h8alt : s.alt[H8]) - 2 - mus);
  double K = used + 2.0 * dM - w8, Bmax = x.B + 2.0 * dM;
  auto f = [&](double Bp) { double u = max(0.0, 2 * Bp - K); return 65 * Bp - 7.0 / 6 * u - u * u / 12; };
  auto cl = [&](double v) { return min(max(v, (double)x.B), Bmax); };
  double best = max(max(f(cl((K + 188) / 2)), f(x.B)), f(cl(K / 2)));
  return 421 + x.Z + 2.0 * dM + best;
}
static void stepX(const X& x, int d, int q, X& y) {
  y = x; const S& s = x.s; int P = s.pos, O = 63 - P; int k = s.alt[P];
  auto inG = [&](int i) { return s.alt[i] == k && i != O; };
  int C = 0; for (auto& r : y.recs) if (!r.cov && inG(r.own)) { r.cov = 1; C += r.del; }
  bool h8in = inG(H8);
  if (x.h8s == 0 && h8in) y.h8alt = k;
  int low = k - d; bool fl = low < x.m;
  apply_move(y.s, d, q);
  if (fl) {
    vector<Rec> nr; if (d > 2) nr.push_back({(uint8_t)O, (int16_t)(x.m - low), (int16_t)(k - x.m), (int16_t)x.m, 0});
    for (auto& r : y.recs) if (!r.cov) nr.push_back(r);
    y.recs.swap(nr); y.m = low; y.Hm = y.s.alt[q] - low;
    y.B += x.m - low; if (d <= 2) y.BnK += x.m - low;
    y.h8s = x.h8s == 2 ? 2 : ((x.h8s == 1 || h8in) ? 2 : 0);
  } else {
    y.Hm = max(x.Hm, y.s.alt[q] - x.m);
    y.h8s = x.h8s == 2 ? 2 : ((x.h8s == 1 || h8in) ? 1 : 0);
  }
  y.Bcov += C; y.Z += y.s.alt[q] - low;
}
static bool alive_gap(const S& s) {
  int k = s.alt[s.pos]; int a[64]; for (int i = 0; i < 64; i++) a[i] = s.alt[i]; std::sort(a, a + 64);
  int c = k; for (int v : a) { if (v <= c + 2) c = max(c, v); else break; }
  if (s.alt[H8] <= c + 2) return true;
  for (int x = 0; x < 64; x++) if (s.alt[x] <= c && s.alt[63 - x] <= c) return true;
  return false;
}
int main(int argc, char** argv) {
  init_board();
  double w = atof(argv[1]); int W = atoi(argv[2]); const char* out = argc > 3 ? argv[3] : "best.txt";
  if (argc > 4 && string(argv[4]) == "trace") {  // check against the Python bound along a tour
    std::ifstream in(argv[5]); string l; getline(in, l); getline(in, l);
    X x; x.s = start_state(); printf("0 %.2f\n", prefix_ub(x)); size_t kk = 0; int i = 0;
    while ((kk = l.find('(', kk)) != string::npos) { int t; char sq[3]; sscanf(l.c_str() + kk, "(%d, %2s)", &t, sq); kk++;
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(x.s, x.s.pos); int d = t ? t / n : 0;
      X y; stepX(x, d, q, y); x = y; i++; if (i % 20 == 0 || i < 4) printf("%d %.2f\n", i, prefix_ub(x)); }
    return 0;
  }
  vector<vector<X>> layers; layers.push_back({}); { X x; x.s = start_state(); x.ub = prefix_ub(x); layers[0].push_back(x); }
  long long bestT = -1; int bestL = -1, bestI = -1, bestD = 0; vector<pair<int, int>> mv;
  auto t0 = std::chrono::steady_clock::now();
  for (int L = 0; L < 189 && !layers[L].empty(); L++) {
    vector<X> nxt; std::unordered_set<uint64_t> seen;
    for (int i = 0; i < (int)layers[L].size(); i++) {
      const X& x = layers[L][i];
      pinned_moves(x.s, 3, true, mv);
      for (auto& [d, q] : mv) {
        if (q == H8) {  // finishing jump: exact length
          int n = group_n(x.s, x.s.pos); long long T = x.s.T + (long long)d * (d > 0 ? n : 0);
          if (T > bestT) { bestT = T; bestL = L; bestI = i; bestD = d; }
          continue;
        }
        X y; stepX(x, d, q, y);
        if (!alive_gap(y.s)) continue;
        uint64_t h = hashS(y.s); if (!seen.insert(h).second) continue;
        y.ub = prefix_ub(y); y.f = y.s.T + w * (y.ub - y.s.T); y.par = i; y.mq = q; y.md = d;
        nxt.push_back(std::move(y));
      }
    }
    if ((int)nxt.size() > W) { std::nth_element(nxt.begin(), nxt.begin() + W, nxt.end(), [](const X& a, const X& b) { return a.f > b.f; }); nxt.resize(W); }
    for (auto& y : nxt) y.recs.shrink_to_fit();
    layers.push_back(std::move(nxt));
    if (L % 20 == 0) { double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      double mx = 0; for (auto& y : layers[L + 1]) mx = max(mx, (double)y.s.T);
      fprintf(stderr, "depth %d: %zu states, best finish %lld, max T in beam %.0f (%.0fs)\n", L + 1, layers[L + 1].size(), bestT, mx, el); }
  }
  if (bestT < 0) { printf("no finish\n"); return 0; }
  // rebuild the best tour
  vector<pair<int, int>> path; int L = bestL, i = bestI; path.push_back({H8, bestD});
  while (L > 0) { const X& x = layers[L][i]; path.push_back({x.mq, x.md}); i = x.par; L--; }
  std::reverse(path.begin(), path.end());
  S s = start_state(); string o;
  for (size_t k = 0; k < path.size(); k++) { int T0 = s.T; apply_move(s, path[k].second, path[k].first);
    char b[32]; snprintf(b, 32, "%s(%d, %s)", k ? ", " : "", s.T - T0, nm(path[k].first).c_str()); o += b; }
  FILE* f = fopen(out, "w"); fprintf(f, "%d\n%s\n", s.T, o.c_str()); fclose(f);
  printf("w %.2f W %d: best tour %d minutes, %zu moves -> %s\n", w, W, s.T, path.size(), out);
}
