// AlphaZero-style self-play / evaluation for the swamp-knight puzzle (maximize total time).
// Modes:
//   az selfplay <weights> <out.bin> <episodes> <sims> <seed> <tempMoves> <bestfile>
//   az eval     <weights> <sims> <seed> <bestfile>
// State starts after a fixed opening read from phaseA.txt.
#include "eng.h"
#include <random>

static const int NA = 24;
static int ADX[NA], ADY[NA], ADZ[NA];
static const int D_IN = 64 * 21 + 64 + 10;  // 1418
static const int H = 256;
static const double TSCALE = 10000.0;
static inline float score_value(double T) { return (float)min(1.0, log(max(T, 1.0)) / log(20000.0)); }

static void init_actions() {
  int k = 0;
  int kn[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
  for (auto& d : kn) { ADX[k] = d[0]; ADY[k] = d[1]; ADZ[k] = 0; k++; }
  int ad[4][2] = {{1,0},{0,1},{-1,0},{0,-1}};
  for (auto& d : ad) for (int z : {2, -2}) { ADX[k] = d[0]; ADY[k] = d[1]; ADZ[k] = z; k++; }
  for (auto& d : ad) for (int z : {1, -1}) { ADX[k] = 2 * d[0]; ADY[k] = 2 * d[1]; ADZ[k] = z; k++; }
}

// returns delta (sink) or -1 if illegal; sets q
static inline int act_legal(const S& s, int a, int& q) {
  int p = s.pos, x = p % 8 + ADX[a], y = p / 8 + ADY[a];
  if (x < 0 || x > 7 || y < 0 || y > 7) return -1;
  q = x + 8 * y;
  if (q != H8 && s.vis[q] >= 3) return -1;
  int O = 63 - p;
  if (s.alt[q] == s.alt[p] && q != O) return ADZ[a] == 0 ? 0 : -1;  // same group: only flat knight move
  int delta = ADZ[a] - (s.alt[q] - s.alt[p]);
  return delta >= 0 ? delta : -1;
}

// ---------------- network ----------------
struct Net {
  vector<float> W1, b1, W2, b2, Wp, bp, wv; float bv;
  bool load(const char* fn) {
    FILE* f = fopen(fn, "rb"); if (!f) return false;
    auto rd = [&](vector<float>& v, size_t n) { v.resize(n); return fread(v.data(), 4, n, f) == n; };
    bool ok = rd(W1, (size_t)D_IN * H) && rd(b1, H) && rd(W2, (size_t)H * H) && rd(b2, H) && rd(Wp, (size_t)H * NA) && rd(bp, NA) && rd(wv, H);
    ok = ok && fread(&bv, 4, 1, f) == 1; fclose(f); return ok;
  }
} NET;

struct Feat { int idx[64 * 2 + 1]; int nidx; float g[10]; };

static void featurize(const S& s, Feat& F) {
  int k = s.pos, A = s.alt[k], n = 0;
  int c0 = 0, cm1 = 0, cm2 = 0, cp1 = 0, cp2 = 0, rem = 0;
  F.nidx = 0;
  for (int i = 0; i < 64; i++) {
    int r = s.alt[i] - A; int rc = r < -8 ? -8 : (r > 8 ? 8 : r);
    F.idx[F.nidx++] = i * 21 + (rc + 8);
    F.idx[F.nidx++] = i * 21 + 17 + s.vis[i];
    if (r == 0) { c0++; if (i != 63 - k) n++; } else if (r == -1) cm1++; else if (r == -2) cm2++; else if (r == 1) cp1++; else if (r == 2) cp2++;
    if (i != 63) rem += 3 - s.vis[i];
  }
  F.idx[F.nidx++] = 64 * 21 + k;
  auto cl = [](int v, int m) { return (float)(v < -m ? -m : (v > m ? m : v)) / m; };
  F.g[0] = n / 64.f; F.g[1] = cl(s.alt[63] - A, 16); F.g[2] = s.T / (float)TSCALE;
  F.g[3] = c0 / 64.f; F.g[4] = cm1 / 64.f; F.g[5] = cm2 / 64.f; F.g[6] = cp1 / 64.f; F.g[7] = cp2 / 64.f;
  F.g[8] = rem / 189.f; F.g[9] = cl(s.alt[63 - k] - A, 16);
}

static void forward(const Feat& F, float* logits, float& value) {
  alignas(32) float h1[H], h2[H];
  for (int j = 0; j < H; j++) h1[j] = NET.b1[j];
  for (int t = 0; t < F.nidx; t++) { const float* w = &NET.W1[(size_t)F.idx[t] * H]; for (int j = 0; j < H; j++) h1[j] += w[j]; }
  for (int g = 0; g < 10; g++) { const float* w = &NET.W1[(size_t)(64 * 21 + 64 + g) * H]; float v = F.g[g]; for (int j = 0; j < H; j++) h1[j] += v * w[j]; }
  for (int j = 0; j < H; j++) h1[j] = h1[j] > 0 ? h1[j] : 0;
  for (int j = 0; j < H; j++) h2[j] = NET.b2[j];
  for (int i = 0; i < H; i++) { float a = h1[i]; if (a == 0) continue; const float* w = &NET.W2[(size_t)i * H]; for (int j = 0; j < H; j++) h2[j] += a * w[j]; }
  for (int j = 0; j < H; j++) h2[j] = h2[j] > 0 ? h2[j] : 0;
  for (int a = 0; a < NA; a++) logits[a] = NET.bp[a];
  float v = NET.bv;
  for (int i = 0; i < H; i++) { float x = h2[i]; if (x == 0) continue; const float* w = &NET.Wp[(size_t)i * NA]; for (int a = 0; a < NA; a++) logits[a] += x * w[a]; v += x * NET.wv[i]; }
  value = 1.f / (1.f + expf(-v));
}

// ---------------- MCTS ----------------
struct Node {
  S s; bool hasState = false;
  int parent = -1, act = -1; float prior = 0;
  int N = 0; double W = 0;
  bool expanded = false, terminal = false; float tval = 0;
  int first = -1, nch = 0;
};
static vector<Node> T;
static double QMIN, QMAX;
static double CPUCT = 1.5;
static std::mt19937 rng;

static void ensure_state(int id) {
  Node& nd = T[id]; if (nd.hasState) return;
  const S& ps = T[nd.parent].s; int q; int delta = act_legal(ps, nd.act, q);
  nd.s = ps; apply_move(nd.s, delta, q); nd.hasState = true;
  if (q == H8) { nd.terminal = true; nd.tval = score_value(nd.s.T); }
}

// expand node: returns value estimate
static float expand(int id) {
  Node& nd = T[id];
  if (nd.terminal) return nd.tval;
  int legal[NA], nl = 0;
  for (int a = 0; a < NA; a++) { int q; if (act_legal(nd.s, a, q) >= 0) legal[nl++] = a; }
  if (nl == 0) { nd.terminal = true; nd.tval = 0; return 0; }
  Feat F; featurize(nd.s, F); float lg[NA], v; forward(F, lg, v);
  float mx = -1e30f; for (int i = 0; i < nl; i++) mx = max(mx, lg[legal[i]]);
  float sum = 0, pr[NA]; for (int i = 0; i < nl; i++) { pr[i] = expf(lg[legal[i]] - mx); sum += pr[i]; }
  int first = (int)T.size();
  for (int i = 0; i < nl; i++) { Node c; c.parent = id; c.act = legal[i]; c.prior = pr[i] / sum; T.push_back(c); }
  T[id].first = first; T[id].nch = nl; T[id].expanded = true;
  return v;
}

static void add_noise(int id, double alpha, double eps) {
  Node& nd = T[id]; if (nd.nch == 0) return;
  std::gamma_distribution<double> g(alpha, 1.0); vector<double> d(nd.nch); double s = 0;
  for (auto& x : d) { x = g(rng); s += x; }
  for (int i = 0; i < nd.nch; i++) T[nd.first + i].prior = (float)((1 - eps) * T[nd.first + i].prior + eps * d[i] / s);
}

static void run_sims(int root, int sims) {
  for (int it = 0; it < sims; it++) {
    int id = root;
    while (T[id].expanded && !T[id].terminal) {
      Node& nd = T[id]; double best = -1e18; int bc = -1;
      double qp = nd.N ? nd.W / nd.N : 0;
      double rng_q = QMAX - QMIN;
      double qpn = rng_q > 1e-9 ? (qp - QMIN) / rng_q : 0.5;
      double sq = sqrt((double)nd.N + 1);
      for (int c = nd.first; c < nd.first + nd.nch; c++) {
        Node& ch = T[c];
        double q = ch.N ? ((rng_q > 1e-9) ? (ch.W / ch.N - QMIN) / rng_q : 0.5) : qpn;
        double u = q + CPUCT * ch.prior * sq / (1 + ch.N);
        if (u > best) { best = u; bc = c; }
      }
      id = bc; ensure_state(id);
    }
    float v = T[id].terminal ? T[id].tval : expand(id);
    QMIN = min(QMIN, (double)v); QMAX = max(QMAX, (double)v);
    while (id >= 0) { T[id].N++; T[id].W += v; id = T[id].parent; }
  }
}

static S start_with_opening(vector<pair<int, string>>& moves) {
  S s = start_state();
  ifstream f("phaseA.txt"); int t; string q;
  while (f >> t >> q) { int qi = (q[0] - 'a') + 8 * (q[1] - '1'); int n = group_n(s, s.pos); apply_move(s, t / n, qi); moves.push_back({t, q}); }
  return s;
}

#pragma pack(push, 1)
static vector<pair<int, string>> read_tour(const char* fn) {
  vector<pair<int, string>> mv; FILE* f = fopen(fn, "r"); if (!f) return mv;
  char line[200000]; if (!fgets(line, sizeof line, f)) { fclose(f); return mv; }
  if (!fgets(line, sizeof line, f)) { fclose(f); return mv; }
  fclose(f); char* p = line; int t; char sq[8];
  while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &t, sq) == 2) { mv.push_back({t, string(sq, 2)}); p++; }
  return mv;
}
#pragma pack(push, 1)
struct Sample { int16_t alt[64]; uint8_t vis[64]; uint8_t pos; int32_t T; uint32_t mask; float pi[NA]; float z; };
#pragma pack(pop)

int main(int argc, char** argv) {
  init_board(); init_actions();
  string mode = argv[1];
  if (!NET.load(argv[2])) { fprintf(stderr, "cannot load weights\n"); return 1; }
  if (mode == "selfplay") {
    const char* out = argv[3]; int EP = atoi(argv[4]), SIMS = atoi(argv[5]); rng.seed(atoi(argv[6]));
    int TEMPM = atoi(argv[7]); const char* bestf = argv[8];
    vector<pair<int, string>> pref; double PSTART = 0; if (argc > 10) { pref = read_tour(argv[9]); PSTART = atof(argv[10]); }
    int OPEN_LEN = 0; { vector<pair<int, string>> tmp; start_with_opening(tmp); OPEN_LEN = tmp.size(); }
    FILE* fo = fopen(out, "wb");
    long long bestT = -1;
    for (int ep = 0; ep < EP; ep++) {
      vector<pair<int, string>> moves; S s = start_with_opening(moves);
      std::uniform_real_distribution<double> U01(0, 1);
      if (!pref.empty() && (int)pref.size() > OPEN_LEN + 1 && U01(rng) < PSTART) {
        int k = OPEN_LEN + (int)(U01(rng) * (pref.size() - 1 - OPEN_LEN));
        s = start_state(); moves.clear();
        for (int i = 0; i < k; i++) { int qi = (pref[i].second[0] - 'a') + 8 * (pref[i].second[1] - '1'); int n = group_n(s, s.pos); apply_move(s, pref[i].first / n, qi); moves.push_back(pref[i]); }
      }
      vector<Sample> traj; vector<float> rootv; int mv = 0; bool done = false; long long score = 0;
      while (!done) {
        T.clear(); T.reserve(SIMS * 8 + 16); Node r; r.s = s; r.hasState = true; T.push_back(r);
        QMIN = 1e9; QMAX = -1e9;
        expand(0);
        if (T[0].terminal) { score = 0; break; }  // stuck
        add_noise(0, 0.3, 0.25);
        run_sims(0, SIMS);
        Sample sm; for (int i = 0; i < 64; i++) { sm.alt[i] = s.alt[i]; sm.vis[i] = s.vis[i]; } sm.pos = s.pos; sm.T = s.T; sm.mask = 0;
        for (int a = 0; a < NA; a++) sm.pi[a] = 0;
        int tot = 0; for (int c = T[0].first; c < T[0].first + T[0].nch; c++) { sm.mask |= 1u << T[c].act; sm.pi[T[c].act] = T[c].N; tot += T[c].N; }
        for (int a = 0; a < NA; a++) sm.pi[a] /= max(1, tot);
        traj.push_back(sm); rootv.push_back((float)(T[0].W / max(1, T[0].N)));
        // choose action
        int pick = -1;
        if (mv < TEMPM) {
          std::uniform_real_distribution<double> U(0, 1); double r0 = U(rng) * tot, acc = 0;
          for (int c = T[0].first; c < T[0].first + T[0].nch; c++) { acc += T[c].N; if (acc >= r0) { pick = c; break; } }
        }
        if (pick < 0) { int bn = -1; for (int c = T[0].first; c < T[0].first + T[0].nch; c++) if (T[c].N > bn) { bn = T[c].N; pick = c; } }
        int a = T[pick].act, q; int delta = act_legal(s, a, q);
        int before = s.T; apply_move(s, delta, q); moves.push_back({s.T - before, nm(q)}); mv++;
        if (q == H8) { score = s.T; done = true; }
        if (mv > 400) break;
      }
      float z = score > 0 ? score_value(score) : 0.f;
      for (size_t i = 0; i < traj.size(); i++) { traj[i].z = 0.5f * z + 0.5f * rootv[i]; fwrite(&traj[i], sizeof(Sample), 1, fo); }
      printf("EP %d moves %d T %lld\n", ep, mv, score); fflush(stdout);
      if (score > bestT) {
        bestT = score;
        FILE* fb = fopen(bestf, "w"); fprintf(fb, "%lld\n", score);
        for (size_t i = 0; i < moves.size(); i++) fprintf(fb, "%s(%d, %s)", i ? ", " : "", moves[i].first, moves[i].second.c_str());
        fprintf(fb, "\n"); fclose(fb);
      }
    }
    fclose(fo);
  } else if (mode == "test") {
    vector<pair<int, string>> moves; S s = start_with_opening(moves);
    Feat F; featurize(s, F); float lg[NA], v; forward(F, lg, v);
    printf("pos %d T %d v %.6f lg", s.pos, s.T, v); for (int a = 0; a < NA; a++) printf(" %.5f", lg[a]); printf("\n");
    int legal = 0; for (int a = 0; a < NA; a++) { int q; if (act_legal(s, a, q) >= 0) legal |= 1 << a; } printf("mask %d\n", legal);
    return 0;
  } else if (mode == "eval") {
    int SIMS = atoi(argv[3]); rng.seed(atoi(argv[4])); const char* bestf = argv[5];
    vector<pair<int, string>> moves; S s = start_with_opening(moves); long long score = 0; int mv = 0;
    while (true) {
      T.clear(); T.reserve((size_t)SIMS * 8 + 16); Node r; r.s = s; r.hasState = true; T.push_back(r);
      QMIN = 1e9; QMAX = -1e9; expand(0);
      if (T[0].terminal) break;
      run_sims(0, SIMS);
      int pick = -1, bn = -1; for (int c = T[0].first; c < T[0].first + T[0].nch; c++) if (T[c].N > bn) { bn = T[c].N; pick = c; }
      int a = T[pick].act, q; int delta = act_legal(s, a, q); int before = s.T; apply_move(s, delta, q);
      moves.push_back({s.T - before, nm(q)}); mv++;
      if (q == H8) { score = s.T; break; }
    }
    printf("EVAL moves %d T %lld\n", mv, score);
    FILE* fb = fopen(bestf, "w"); fprintf(fb, "%lld\n", score);
    for (size_t i = 0; i < moves.size(); i++) fprintf(fb, "%s(%d, %s)", i ? ", " : "", moves[i].first, moves[i].second.c_str());
    fprintf(fb, "\n"); fclose(fb);
  }
}
