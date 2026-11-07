// Enumerate compact layouts of a core set of states (default NORTH/SOUTH x CAROLINA/DAKOTA).
// SA over grids whose squares hold a core letter or a blank; energy = 100 * (core states
// missing) + (squares used). Every time all core states are present with <= --maxcells
// squares used, the layout is reduced to a minimal one (blank any square not needed),
// canonicalised under the 8 rotations/reflections and stored. Prints one layout per line.
// Build: g++ -O3 -march=native -std=c++17 -o build/compact compact.cpp
#include <bits/stdc++.h>
using namespace std;
typedef uint32_t u32;
static const u32 FULL = (1u << 25) - 1, COL0 = 0x108421, COL4 = COL0 << 4;
static u32 NB[25];
static inline u32 block(u32 m) {
  u32 h = m | ((m & ~COL4) << 1) | ((m & ~COL0) >> 1);
  return (h | (h << 5) | (h >> 5)) & FULL;
}
struct St { string w; };
static vector<string> CORE;
static bool present(const int* g, const string& w) {
  u32 mask[27] = {0};
  for (int i = 0; i < 25; i++) if (g[i] >= 0) mask[g[i]] |= 1u << i;
  u32 cur = mask[w[0] - 'A'];
  for (size_t i = 1; i < w.size() && cur; i++) {
    u32 nb;
    if (w[i] == w[i - 1]) { nb = 0; for (u32 t = cur; t; t &= t - 1) nb |= NB[__builtin_ctz(t)]; }
    else nb = block(cur);
    cur = nb & mask[w[i] - 'A'];
  }
  return cur != 0;
}
static int missing(const int* g) { int m = 0; for (auto& w : CORE) if (!present(g, w)) m++; return m; }
static string canon(const int* g) {
  string best;
  for (int t = 0; t < 8; t++) {
    string s(25, '.');
    for (int r = 0; r < 5; r++) for (int c = 0; c < 5; c++) {
      int rr = r, cc = c;
      for (int k = 0; k < t % 4; k++) { int nr = cc, nc = 4 - rr; rr = nr; cc = nc; }
      if (t >= 4) cc = 4 - cc;
      int v = g[r * 5 + c];
      s[rr * 5 + cc] = v < 0 ? '.' : char('A' + v);
    }
    if (best.empty() || s < best) best = s;
  }
  return best;
}
int main(int argc, char** argv) {
  double T = 120; int maxcells = 14; unsigned long long seed = 1;
  string core = "NORTHCAROLINA,SOUTHCAROLINA,NORTHDAKOTA,SOUTHDAKOTA";
  for (int i = 1; i < argc; i++) {
    string a = argv[i];
    if (a == "--time") T = stod(argv[++i]);
    else if (a == "--maxcells") maxcells = stoi(argv[++i]);
    else if (a == "--seed") seed = stoull(argv[++i]);
    else if (a == "--core") core = argv[++i];
  }
  { stringstream ss(core); string w; while (getline(ss, w, ',')) CORE.push_back(w); }
  for (int c = 0; c < 25; c++) NB[c] = block(1u << c) & ~(1u << c);
  string letters;
  for (auto& w : CORE) for (char ch : w) if (letters.find(ch) == string::npos) letters += ch;
  mt19937_64 rng(seed);
  uniform_real_distribution<double> U(0, 1);
  set<string> seen;
  auto t0 = chrono::steady_clock::now();
  auto el = [&]() { return chrono::duration<double>(chrono::steady_clock::now() - t0).count(); };
  long long runs = 0;
  while (el() < T) {
    runs++;
    int g[25];
    for (int i = 0; i < 25; i++) g[i] = letters[rng() % letters.size()] - 'A';
    auto energy = [&](const int* h) { int u = 0; for (int i = 0; i < 25; i++) u += h[i] >= 0; return 100 * missing(h) + u; };
    int E = energy(g);
    double temp = 3.0;
    for (int it = 0; it < 200000; it++, temp *= 0.99997) {
      int c = rng() % 25, old = g[c];
      int nv = (rng() % 4 == 0) ? -1 : letters[rng() % letters.size()] - 'A';
      if (nv == old) continue;
      g[c] = nv;
      int E2 = energy(g);
      if (E2 <= E || U(rng) < exp((E - E2) / temp)) {
        E = E2;
        if (E <= maxcells) {  // all present (E < 100) and compact
          int h[25]; memcpy(h, g, sizeof h);
          vector<int> order(25); iota(order.begin(), order.end(), 0); shuffle(order.begin(), order.end(), rng);
          for (int k : order) { if (h[k] < 0) continue; int o = h[k]; h[k] = -1; if (missing(h)) h[k] = o; }
          string s = canon(h);
          if (seen.insert(s).second) { puts(s.c_str()); fflush(stdout); }
        }
      } else g[c] = old;
    }
  }
  fprintf(stderr, "%lld runs, %zu distinct layouts with <= %d squares\n", runs, seen.size(), maxcells);
}
