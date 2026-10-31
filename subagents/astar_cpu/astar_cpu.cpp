// astar_cpu: CPU-only port of the NN-guided A* search (src/nnastar.cpp + nnastar/*.py), with the GPU ResNet
// replaced by a small MLP over hand-made features, evaluated inline in C++.
//
// Modes (first argument):
//   gen     generate labelled positions: cut a known tour at a random move, finish it with an NRPA search of
//           random level from fresh weights, label every state on the path with the best final score known
//           reachable from it (suffix maximum + probe rollouts). Writes 204-byte Samples (nnastar format).
//   feat    turn Sample files into a feature matrix for train.py (labels raised to the best future known for
//           the same position, exact duplicates dropped).
//   search  best-first search ordered by the model's value + noise - novelty penalty, from a1 or after the first
//           k moves of a seed tour. Optional label workers (NRPA from expanded states) and an online refit thread
//           (Adam on the MLP) that publishes new weights and re-scores the best open entries.
//   check   replay a tour file through the engine (prints minutes, -1 if illegal).
//   bench   time feature extraction + model evaluation.
// Build: g++ -O3 -march=native -std=c++17 -pthread -o astar_cpu astar_cpu.cpp
// See RUN.md for commands.
#include "../../src/eng.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

struct Mv { int delta, q; };
static std::atomic<bool> STOP{false};
static void on_sig(int) { STOP = true; }
static auto T0 = std::chrono::steady_clock::now();
static double elapsed() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count(); }

// ============================================================================================
// NRPA (from src/nnastar.cpp, tour objective only)
// ============================================================================================
static int KIDX[64][64];
static const int NCODE = 64 * 16 * 32 * 4;
struct Policy { vector<float> w, e; Policy() : w(NCODE, 0.f), e(NCODE, 1.f) {} };
static inline uint32_t code_of(const S& s, int delta, int q) {
  return (((uint32_t)s.pos * 16 + KIDX[s.pos][q]) * 32 + (uint32_t)min(delta, 31)) * 4 + (uint32_t)min<int>(s.vis[q], 3);
}
static double D0 = 1;
static double h8dist(const S& s) {
  int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  return sqrt(dx * dx + dy * dy + dz * dz) / D0;
}
struct Result { double score = -1e18; long long minutes = 0; bool finished = false; vector<Mv> seq; };
static std::atomic<long long> ROLLOUTS{0};

static Result playout(const S& root, const Policy& pol, std::mt19937& rng) {
  static thread_local vector<pair<int, int>> mv; static thread_local vector<double> w;
  S s = root; Result r;
  long long finT = -1; size_t finLen = 0; Mv finMove{0, H8};
  while (true) {
    pinned_moves(s, 3, true, mv);
    w.clear(); double z = 0; int nonfin = 0;
    for (auto& [d, q] : mv) {
      if (q == H8) {
        long long t = s.T + (long long)d * (d ? group_n(s, s.pos) : 0);
        if (t >= finT) { finT = t; finLen = r.seq.size(); finMove = {d, q}; }
        w.push_back(0); continue;
      }
      double e = pol.e[code_of(s, d, q)]; w.push_back(e); z += e; nonfin++;
    }
    if (nonfin == 0) break;
    double u = std::uniform_real_distribution<double>(0, z)(rng), acc = 0; int pick = -1;
    for (size_t i = 0; i < mv.size(); i++) { if (mv[i].second == H8) continue; acc += w[i]; pick = (int)i; if (acc >= u) break; }
    r.seq.push_back({mv[pick].first, mv[pick].second});
    apply_move(s, mv[pick].first, mv[pick].second);
    if (r.seq.size() > 4000) break;
  }
  ROLLOUTS++;
  if (finT >= 0) { r.seq.resize(finLen); r.seq.push_back(finMove); r.finished = true; r.minutes = finT; r.score = (double)finT; }
  else { r.minutes = s.T; r.score = -1e7 + s.T * exp(-h8dist(s)); }
  return r;
}

static void adapt(Policy& pol, const S& root, const vector<Mv>& seq, double alpha) {
  static thread_local vector<pair<uint32_t, float>> upd; upd.clear();
  static thread_local vector<pair<int, int>> mv;
  static thread_local vector<double> e;
  S s = root;
  for (const Mv& m : seq) {
    if (m.q == H8) break;
    pinned_moves(s, 3, true, mv);
    e.clear(); double z = 0;
    for (auto& [d, q] : mv) { double x = q == H8 ? 0 : pol.e[code_of(s, d, q)]; e.push_back(x); z += x; }
    for (size_t i = 0; i < mv.size(); i++) if (mv[i].second != H8) upd.push_back({code_of(s, mv[i].first, mv[i].second), (float)(-alpha * e[i] / z)});
    upd.push_back({code_of(s, m.delta, m.q), (float)alpha});
    apply_move(s, m.delta, m.q);
  }
  for (auto& [c, d] : upd) pol.w[c] += d;
  for (auto& [c, d] : upd) pol.e[c] = expf(pol.w[c]);
}

static Result nrpa(int level, const S& root, Policy pol, std::mt19937& rng) {
  if (level == 0) return playout(root, pol, rng);
  Result best;
  for (int i = 0; i < 100 && !STOP; i++) {
    Result r = level == 1 ? playout(root, pol, rng) : nrpa(level - 1, root, pol, rng);
    if (r.score >= best.score) best = r;
    adapt(pol, root, best.seq, 1.0);
  }
  return best;
}

// ============================================================================================
// Tours: text, parsing, validation
// ============================================================================================
static string tour_text(const vector<Mv>& seq) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < seq.size(); i++) {
    int w = apply_move(s, seq[i].delta, seq[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(seq[i].q).c_str()); out += buf;
  }
  return out;
}
// parse one line of "(t, sq), ..." (any text before the first '(' is ignored)
static bool parse_line(const char* line, vector<Mv>& t) {
  t.clear(); S s = start_state(); const char* p = line; int w; char sq[8];
  while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
    if (sq[0] < 'a' || sq[0] > 'h' || sq[1] < '1' || sq[1] > '8') return false;
    int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
    t.push_back({d, q}); apply_move(s, d, q); p++;
    if (q == H8) break;
  }
  return !t.empty();
}
static vector<vector<Mv>> parse_tours(const char* fn, bool finishedOnly) {
  vector<vector<Mv>> out; FILE* f = fopen(fn, "r"); if (!f) { fprintf(stderr, "cannot open %s\n", fn); return out; }
  static thread_local vector<char> line(1 << 20);
  vector<Mv> t;
  while (fgets(line.data(), (int)line.size(), f)) {
    if (!parse_line(line.data(), t)) continue;
    if (finishedOnly && t.back().q != H8) continue;
    out.push_back(t);
  }
  fclose(f); return out;
}
// minutes of a move sequence from a1 if every move is a legal pinned move and it ends on h8, else -1
static long long seq_minutes(const vector<Mv>& seq) {
  S s = start_state(); vector<pair<int, int>> mv;
  for (size_t i = 0; i < seq.size(); i++) {
    pinned_moves(s, 3, true, mv); bool ok = false;
    for (auto& [d, q] : mv) if (d == seq[i].delta && q == seq[i].q) ok = true;
    if (!ok) return -1;
    apply_move(s, seq[i].delta, seq[i].q);
    if (seq[i].q == H8) return i + 1 == seq.size() ? s.T : -1;
  }
  return -1;
}
static long long text_minutes(const string& text) {
  vector<Mv> t; if (!parse_line(text.c_str(), t)) return -1;
  long long m = seq_minutes(t); if (m < 0) return -1;
  return tour_text(t) == text ? m : -1;  // the waits written must be the engine's
}

// ============================================================================================
// Samples (same 204-byte layout as src/nnastar.cpp)
// ============================================================================================
#pragma pack(push, 1)
struct Sample {
  int16_t alt[64];
  uint8_t vis[64];
  uint8_t pos;
  uint8_t src;     // 0 seed prefix, 1 seed path, 2 label-job root, 3 label-job path, 4 A* tree, 5 known tour
  uint16_t depth;  // moves from a1
  int32_t T;
  float label;     // best final score known reachable (0: none found)
};
#pragma pack(pop)
static_assert(sizeof(Sample) == 204, "Sample layout");
static inline void to_sample(const S& s, int depth, int src, float label, Sample& o) {
  memcpy(o.alt, s.alt, sizeof o.alt); memcpy(o.vis, s.vis, sizeof o.vis);
  o.pos = s.pos; o.src = (uint8_t)src; o.depth = (uint16_t)min(depth, 65535); o.T = s.T; o.label = label;
}
static inline void from_sample(const Sample& a, S& s) {
  memcpy(s.alt, a.alt, sizeof s.alt); memcpy(s.vis, a.vis, sizeof s.vis); s.pos = a.pos; s.T = a.T;
}
static vector<Sample> read_samples(const string& fn, size_t cap = SIZE_MAX) {
  vector<Sample> v; FILE* f = fopen(fn.c_str(), "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", fn.c_str()); return v; }
  fseek(f, 0, SEEK_END); long long n = ftell(f) / (long long)sizeof(Sample); fseek(f, 0, SEEK_SET);
  n = min<long long>(n, (long long)cap); v.resize(n);
  size_t got = fread(v.data(), sizeof(Sample), n, f); v.resize(got); fclose(f); return v;
}

// ============================================================================================
// Features (keep in sync with nothing else: train.py reads the matrix written by `feat`)
// ============================================================================================
static const int FEAT_VERSION = 1;
static const int NSCAL = 32, NBLK = 8, NHIST = 34;
static const int NF = NSCAL + NBLK * 64 + NHIST;  // 578
static inline float clip8(int r) { return (float)max(-8, min(8, r)) / 8.f; }

static void features(const S& s, int depth, float* x) {
  memset(x, 0, sizeof(float) * NF);
  const int p = s.pos, a = s.alt[p], ah = s.alt[H8];
  int vl[64], vlTot = 0;
  for (int i = 0; i < 64; i++) { vl[i] = i == H8 ? 0 : max(0, 3 - (int)s.vis[i]); vlTot += vl[i]; }
  bool h8r; int maxAlt; uint64_t R = static_reach(s, 3, h8r, maxAlt);
  int cnt[17] = {0}, cntv[17] = {0}, h8g = 0, rvl = 0;
  for (int i = 0; i < 63; i++) {
    int r = s.alt[i] - a; if (r >= -8 && r <= 8) { cnt[r + 8]++; cntv[r + 8] += vl[i]; }
    if (s.alt[i] == ah) h8g++;
    if (R >> i & 1) rvl += vl[i];
  }
  x[0] = s.T / 1000.f; x[1] = depth / 100.f; x[2] = vlTot / 100.f; x[3] = (a - ah) / 10.f; x[4] = group_n(s, p) / 10.f;
  static const int RR[8] = {-4, -3, -2, -1, 1, 2, 3, 4};
  for (int k = 0; k < 8; k++) x[5 + k] = cnt[RR[k] + 8] / 10.f;
  x[13] = h8g / 10.f; x[14] = h8r ? 1.f : 0.f; x[15] = __builtin_popcountll(R) / 64.f; x[16] = rvl / 100.f; x[17] = (maxAlt - a) / 10.f;
  int both = 0, one = 0, smax = 0, smin = 0;
  for (int i = 0; i < 32; i++) {
    int u = vl[i], w = vl[63 - i];
    if (u > 0 && w > 0) both++; else if (u > 0 || w > 0) one++;
    smax += max(u, w); smin += min(u, w);
  }
  x[18] = both / 32.f; x[19] = one / 32.f; x[20] = smax / 50.f; x[21] = smin / 50.f;
  // optimistic dance bound: groups at L and L-2 among reachable points; each opposite pair supplies the visits of
  // one member (README); minutes per sit ~ 4 x (half the points of both groups). sits[L], size[L] for every L.
  int cs = 0, cz = 0, us = 0, uz = 0, ms = 0; double mmin = 0;
  {
    int lo = a, hi = a;
    for (int i = 0; i < 63; i++) if (R >> i & 1) { lo = min(lo, (int)s.alt[i]); hi = max(hi, (int)s.alt[i]); }
    hi += 2; int W = hi - lo + 1;
    if (W <= 1024) {
      int sits[1024], size[1024]; memset(sits, 0, sizeof(int) * W); memset(size, 0, sizeof(int) * W);
      for (int i = 0; i < 32; i++) {
        int o = 63 - i, eL[4], eV[4], ne = 0;
        for (int m : {i, o}) {
          if (m == H8 || !(R >> m & 1)) continue;
          for (int L : {(int)s.alt[m], (int)s.alt[m] + 2}) {
            size[L - lo]++;
            if (vl[m] <= 0) continue;
            int k = 0; while (k < ne && eL[k] != L) k++;
            if (k == ne) { eL[ne] = L; eV[ne] = vl[m]; ne++; } else eV[k] = max(eV[k], vl[m]);
          }
        }
        for (int k = 0; k < ne; k++) sits[eL[k] - lo] += eV[k];
      }
      cs = sits[a - lo]; cz = size[a - lo]; us = sits[a + 2 - lo]; uz = size[a + 2 - lo];
      for (int k = 0; k < W; k++) { double m = 2.0 * sits[k] * size[k]; if (m > mmin) { mmin = m; ms = sits[k]; } }
    }
  }
  (void)uz;
  x[22] = cs / 50.f; x[23] = us / 50.f; x[24] = ms / 50.f; x[25] = (float)(mmin / 1000.0); x[26] = 2.f * cs * cz / 1000.f;
  x[27] = vl[p] / 3.f;
  static thread_local vector<pair<int, int>> mv; pinned_moves(s, 3, true, mv);
  int zw = 0; for (auto& m : mv) zw += m.first == 0;
  x[28] = mv.size() / 16.f; x[29] = zw / 16.f;
  int h8n = 0; for (int k = 0; k < NBN[H8]; k++) { int q = NBQ[H8][k]; if (vl[q] > 0 && (R >> q & 1)) h8n++; }
  x[30] = h8n / 8.f; x[31] = 1.f;
  float* b = x + NSCAL;
  for (int i = 0; i < 64; i++) {
    int r = s.alt[i] - a;
    b[i] = vl[i] / 3.f;
    b[128 + i] = clip8(r);
    b[192 + i] = r == 0; b[256 + i] = r == -2; b[320 + i] = r == 2;
    b[384 + i] = (R >> i & 1) ? 1.f : 0.f;
    b[448 + i] = clip8(s.alt[i] - ah);
  }
  b[64 + p] = 1.f;
  float* h = x + NSCAL + NBLK * 64;
  for (int k = 0; k < 17; k++) { h[k] = cnt[k] / 10.f; h[17 + k] = cntv[k] / 30.f; }
}

// ============================================================================================
// Model: 1-hidden-layer MLP, two outputs (finish logit, future minutes / 1000)
// ============================================================================================
struct Model {
  int F = NF, H = 0;
  vector<float> mu, sd, W1, b1, W2, b2;  // normalized-input weights (W1[f*H+h], W2[h*2+o])
  vector<float> W1f, b1f;                // folded with mu/sd for fast inference on raw features
  bool ok() const { return H > 0; }
  void fold() {
    W1f.assign((size_t)F * H, 0.f); b1f = b1;
    for (int f = 0; f < F; f++) {
      float is = 1.f / sd[f];
      for (int h = 0; h < H; h++) { W1f[(size_t)f * H + h] = W1[(size_t)f * H + h] * is; b1f[h] -= mu[f] * is * W1[(size_t)f * H + h]; }
    }
  }
  void init(int h, unsigned seed) {
    H = h; mu.assign(F, 0.f); sd.assign(F, 1.f); std::mt19937 rng(seed); std::normal_distribution<float> N(0, 1);
    W1.resize((size_t)F * H); for (auto& w : W1) w = N(rng) * sqrtf(2.f / F);
    b1.assign(H, 0.f); W2.resize(H * 2); for (auto& w : W2) w = N(rng) * sqrtf(1.f / H); b2.assign(2, 0.f); fold();
  }
  // raw features -> (logit, fut); the first layer skips zero inputs (most are one-hot)
  template <int HH> void fwd(const float* x, float& logit, float& fut) const {
    float hb[HH];
    for (int h = 0; h < HH; h++) hb[h] = b1f[h];
    const float* W = W1f.data();
    for (int f = 0; f < F; f++) {
      float v = x[f]; if (v == 0.f) continue;
      const float* w = W + (size_t)f * HH;
      for (int h = 0; h < HH; h++) hb[h] += v * w[h];
    }
    float o0 = b2[0], o1 = b2[1];
    for (int h = 0; h < HH; h++) { float r = hb[h] > 0 ? hb[h] : 0; o0 += r * W2[h * 2]; o1 += r * W2[h * 2 + 1]; }
    logit = o0; fut = o1;
  }
  void forward(const float* x, float& logit, float& fut) const {
    if (H == 16) return fwd<16>(x, logit, fut);
    if (H == 32) return fwd<32>(x, logit, fut);
    if (H == 64) return fwd<64>(x, logit, fut);
    if (H == 128) return fwd<128>(x, logit, fut);
    float hb[512]; int Hh = H;
    for (int h = 0; h < Hh; h++) hb[h] = b1f[h];
    for (int f = 0; f < F; f++) {
      float v = x[f]; if (v == 0.f) continue;
      const float* w = &W1f[(size_t)f * Hh];
      for (int h = 0; h < Hh; h++) hb[h] += v * w[h];
    }
    float o0 = b2[0], o1 = b2[1];
    for (int h = 0; h < Hh; h++) { float r = hb[h] > 0 ? hb[h] : 0; o0 += r * W2[h * 2]; o1 += r * W2[h * 2 + 1]; }
    logit = o0; fut = o1;
  }
  bool load(const string& fn) {
    FILE* f = fopen(fn.c_str(), "r"); if (!f) return false;
    char tag[64]; int ver, FF, HH;
    if (fscanf(f, "%63s %d %d %d", tag, &ver, &FF, &HH) != 4 || string(tag) != "ASTAR_MLP" || ver != FEAT_VERSION || FF != NF || HH <= 0 || HH > 512) {
      fprintf(stderr, "%s: bad header (need ASTAR_MLP %d %d H)\n", fn.c_str(), FEAT_VERSION, NF); fclose(f); return false;
    }
    F = FF; H = HH;
    auto rd = [&](vector<float>& v, size_t n) { v.resize(n); for (auto& x : v) if (fscanf(f, "%f", &x) != 1) return false; return true; };
    bool good = rd(mu, F) && rd(sd, F) && rd(W1, (size_t)F * H) && rd(b1, H) && rd(W2, (size_t)H * 2) && rd(b2, 2);
    fclose(f); if (!good) { fprintf(stderr, "%s: truncated\n", fn.c_str()); H = 0; return false; }
    for (auto& v : sd) if (!(v > 1e-6f)) v = 1.f;
    fold(); return true;
  }
  bool save(const string& fn) const {
    string tmp = fn + ".tmp"; FILE* f = fopen(tmp.c_str(), "w"); if (!f) return false;
    fprintf(f, "ASTAR_MLP %d %d %d\n", FEAT_VERSION, F, H);
    auto wr = [&](const vector<float>& v) { for (size_t i = 0; i < v.size(); i++) fprintf(f, "%.7g%c", v[i], (i + 1) % 16 == 0 || i + 1 == v.size() ? '\n' : ' '); };
    wr(mu); wr(sd); wr(W1); wr(b1); wr(W2); wr(b2); fclose(f);
    return rename(tmp.c_str(), fn.c_str()) == 0;
  }
};

// search value: p_finish^gamma * (T + future)
static double GAMMA = 0.5, DEPTH_BONUS = 0.0;
static float value_of(const Model* m, const S& s, int depth, float* xb) {
  features(s, depth, xb);
  if (!m || !m->ok()) {  // fallback heuristic (no model): T + 60 min per optimistic sit
    return (float)(s.T + 60.0 * xb[24] * 50.0) + (float)(DEPTH_BONUS * depth);
  }
  float lg, fu; m->forward(xb, lg, fu);
  double p = 1.0 / (1.0 + exp(-(double)lg));
  return (float)(pow(p, GAMMA) * (s.T + 1000.0 * max(0.f, fu)) + DEPTH_BONUS * depth);
}

// ============================================================================================
// Adam trainer (online refit; same loss as train.py)
// ============================================================================================
struct Trainer {
  Model m; double lr = 3e-4, wd = 1e-5, fut_w = 1.0, b1a = 0.9, b2a = 0.999; long long t = 0;
  vector<float> mW1, vW1, mb1, vb1, mW2, vW2, mb2, vb2, gW1, gb1, gW2, gb2;
  void reset(const Model& src) {
    m = src; size_t n1 = (size_t)m.F * m.H;
    mW1.assign(n1, 0); vW1.assign(n1, 0); gW1.assign(n1, 0); mb1.assign(m.H, 0); vb1.assign(m.H, 0); gb1.assign(m.H, 0);
    mW2.assign(m.H * 2, 0); vW2.assign(m.H * 2, 0); gW2.assign(m.H * 2, 0); mb2.assign(2, 0); vb2.assign(2, 0); gb2.assign(2, 0); t = 0;
  }
  // batch of (features, T, label); returns (loss_fin, loss_fut)
  pair<double, double> step(const vector<float>& X, const vector<float>& Tv, const vector<float>& L) {
    int B = (int)Tv.size(), F = m.F, H = m.H;
    fill(gW1.begin(), gW1.end(), 0.f); fill(gb1.begin(), gb1.end(), 0.f); fill(gW2.begin(), gW2.end(), 0.f); fill(gb2.begin(), gb2.end(), 0.f);
    vector<float> z(F), hp(H), hr(H), gh(H); double lf = 0, lu = 0; int nfin = 0;
    for (int bi = 0; bi < B; bi++) {
      const float* x = &X[(size_t)bi * F];
      for (int f = 0; f < F; f++) z[f] = (x[f] - m.mu[f]) / m.sd[f];
      for (int h = 0; h < H; h++) hp[h] = m.b1[h];
      for (int f = 0; f < F; f++) { float v = z[f]; if (v == 0.f) continue; const float* w = &m.W1[(size_t)f * H]; for (int h = 0; h < H; h++) hp[h] += v * w[h]; }
      float o0 = m.b2[0], o1 = m.b2[1];
      for (int h = 0; h < H; h++) { hr[h] = hp[h] > 0 ? hp[h] : 0; o0 += hr[h] * m.W2[h * 2]; o1 += hr[h] * m.W2[h * 2 + 1]; }
      float fin = L[bi] > 0 ? 1.f : 0.f; float pz = 1.f / (1.f + expf(-o0));
      lf += -(fin * logf(pz + 1e-7f) + (1 - fin) * logf(1 - pz + 1e-7f));
      float g0 = (pz - fin) / B, g1 = 0;
      if (fin > 0) {
        float tgt = max(0.f, L[bi] - Tv[bi]) / 1000.f, d = o1 - tgt, beta = 0.5f;
        lu += fabsf(d) < beta ? 0.5f * d * d / beta : fabsf(d) - 0.5f * beta; nfin++;
        g1 = (float)fut_w * (fabsf(d) < beta ? d / beta : (d > 0 ? 1.f : -1.f)) / B;
      }
      gb2[0] += g0; gb2[1] += g1;
      for (int h = 0; h < H; h++) { gW2[h * 2] += hr[h] * g0; gW2[h * 2 + 1] += hr[h] * g1; gh[h] = hp[h] > 0 ? m.W2[h * 2] * g0 + m.W2[h * 2 + 1] * g1 : 0.f; }
      for (int h = 0; h < H; h++) gb1[h] += gh[h];
      for (int f = 0; f < F; f++) { float v = z[f]; if (v == 0.f) continue; float* g = &gW1[(size_t)f * H]; for (int h = 0; h < H; h++) g[h] += v * gh[h]; }
    }
    t++;
    double c1 = 1 - pow(b1a, t), c2 = 1 - pow(b2a, t);
    auto upd = [&](vector<float>& w, vector<float>& g, vector<float>& mm, vector<float>& vv, bool decay) {
      for (size_t i = 0; i < w.size(); i++) {
        float gi = g[i] + (decay ? (float)wd * w[i] : 0.f);
        mm[i] = (float)(b1a * mm[i] + (1 - b1a) * gi); vv[i] = (float)(b2a * vv[i] + (1 - b2a) * gi * gi);
        w[i] -= (float)(lr * (mm[i] / c1) / (sqrt(vv[i] / c2) + 1e-8));
      }
    };
    upd(m.W1, gW1, mW1, vW1, true); upd(m.b1, gb1, mb1, vb1, false); upd(m.W2, gW2, mW2, vW2, true); upd(m.b2, gb2, mb2, vb2, false);
    return {lf / B, nfin ? lu / nfin : 0.0};
  }
};

// best-future table: position (alt, vis, pos; not T) -> most future minutes known reachable
struct BestFuture {
  std::unordered_map<uint64_t, int32_t> mp; std::mutex mx; size_t cap = 20000000;
  static uint64_t key(const S& s) { uint64_t k = hashS(s, true) ^ 0x5bd1e9955bd1e995ULL; return k ? k : 1; }
  void put(const S& s, int32_t fut) {
    if (fut < 0) return;
    std::lock_guard<std::mutex> lk(mx);
    if (mp.size() >= cap) mp.clear();
    auto it = mp.find(key(s)); if (it == mp.end()) mp.emplace(key(s), fut); else if (fut > it->second) it->second = fut;
  }
  int32_t get(const S& s) { std::lock_guard<std::mutex> lk(mx); auto it = mp.find(key(s)); return it == mp.end() ? -1 : it->second; }
};

// ============================================================================================
// Option parsing helpers
// ============================================================================================
struct Args {
  vector<string> a; size_t i = 0;
  bool has() const { return i < a.size(); }
};
static void die(const string& m) { fprintf(stderr, "%s\n", m.c_str()); exit(1); }

// tours file list "FILE[:weight]"
struct TourSet { vector<vector<Mv>> tours; vector<long long> score; vector<double> w; };
static void load_tours(TourSet& ts, const string& spec) {
  string fn = spec; double w = 1;
  size_t c = spec.rfind(':'); if (c != string::npos && c + 1 < spec.size() && (isdigit(spec[c + 1]) || spec[c + 1] == '.')) { fn = spec.substr(0, c); w = stod(spec.substr(c + 1)); }
  auto v = parse_tours(fn.c_str(), true); int n = 0;
  for (auto& t : v) { long long m = seq_minutes(t); if (m < 0) continue; ts.tours.push_back(t); ts.score.push_back(m); ts.w.push_back(w); n++; }
  printf("loaded %d tours from %s (weight %.2f each)\n", n, fn.c_str(), w);
}

// label a path's states by suffix maximum of its own score and probe rollouts
static Policy* UNIFORM = nullptr;
static int PROBES = 4, PROBE_STATES = 12;
static void label_path(const S& root, const vector<Mv>& seq, double ownScore, vector<S>& states, vector<float>& labels, std::mt19937& rng) {
  states.clear(); states.push_back(root); S s = root;
  for (const Mv& m : seq) { if (m.q == H8) break; apply_move(s, m.delta, m.q); states.push_back(s); }
  int n = (int)states.size(); labels.assign(n, 0.f); vector<double> probe(n, 0.0);
  for (int k = 0; k < PROBE_STATES && PROBES > 0; k++) {
    int i = std::uniform_int_distribution<int>(0, n - 1)(rng);
    for (int r = 0; r < PROBES; r++) { Result pr = playout(states[i], *UNIFORM, rng); if (pr.finished) probe[i] = max(probe[i], (double)pr.minutes); }
  }
  double run = 0;
  for (int i = n - 1; i >= 0; i--) { run = max(run, max(ownScore, probe[i])); labels[i] = (float)run; }
}

// ============================================================================================
// gen
// ============================================================================================
static int mode_gen(Args& A) {
  TourSet ts; string out = "tmp/gen.bin"; int threads = 1; double tmax = 60, pl[4] = {0.25, 0.40, 0.34, 0.01}, prefixRate = 0.05; unsigned seed = 1; int nice_ = 10;
  bool tourSamples = true;
  for (; A.has(); A.i++) {
    string o = A.a[A.i]; auto nx = [&]() { if (A.i + 1 >= A.a.size()) die("missing value for " + o); return A.a[++A.i]; };
    if (o == "--tours") load_tours(ts, nx()); else if (o == "--out") out = nx(); else if (o == "--threads") threads = stoi(nx());
    else if (o == "--time") tmax = stod(nx()); else if (o == "--seed") seed = stoul(nx()); else if (o == "--nice") nice_ = stoi(nx());
    else if (o == "--levels") { string s = nx(); if (sscanf(s.c_str(), "%lf,%lf,%lf,%lf", &pl[0], &pl[1], &pl[2], &pl[3]) != 4) die("--levels p0,p1,p2,p3"); }
    else if (o == "--prefix-rate") prefixRate = stod(nx()); else if (o == "--probes") PROBES = stoi(nx()); else if (o == "--probe-states") PROBE_STATES = stoi(nx());
    else if (o == "--no-tour-samples") tourSamples = false;
    else die("gen: unknown option " + o);
  }
  if (ts.tours.empty()) die("gen: need --tours FILE[:w]");
  FILE* fo = fopen(out.c_str(), "ab"); if (!fo) die("cannot write " + out);
  std::mutex mx; long long nS = 0, nP = 0, nFin = 0, lvP[4] = {0}, lvF[4] = {0};
  if (tourSamples) {  // every state of every known tour, labelled with the tour's score
    vector<Sample> buf;
    for (size_t t = 0; t < ts.tours.size(); t++) {
      S s = start_state();
      for (size_t i = 0; i < ts.tours[t].size(); i++) { Sample o; to_sample(s, (int)i, 5, (float)ts.score[t], o); buf.push_back(o); apply_move(s, ts.tours[t][i].delta, ts.tours[t][i].q); }
    }
    fwrite(buf.data(), sizeof(Sample), buf.size(), fo); nS += buf.size();
  }
  auto worker = [&](int tid) {
    setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), nice_);
    std::mt19937 rng(seed * 7919u + tid * 104729u + 17); std::uniform_real_distribution<double> U(0, 1);
    std::discrete_distribution<size_t> pickT(ts.w.begin(), ts.w.end());
    vector<Sample> buf; vector<S> states; vector<float> labels;
    while (!STOP && elapsed() < tmax) {
      size_t ti = pickT(rng); const vector<Mv>& tour = ts.tours[ti]; int len = (int)tour.size();
      int k = (int)floor(U(rng) * (len - 1)); if (k > len - 1) k = len - 1;
      double u = U(rng) * (pl[0] + pl[1] + pl[2] + pl[3]); int L = 0; while (L < 3 && u >= pl[L]) { u -= pl[L]; L++; }
      S root = start_state(); vector<S> pre; pre.push_back(root);
      for (int i = 0; i < k; i++) { apply_move(root, tour[i].delta, tour[i].q); pre.push_back(root); }
      Result r = nrpa(L, root, Policy(), rng);
      if (STOP) break;
      label_path(root, r.seq, r.finished ? (double)r.minutes : 0.0, states, labels, rng);
      float preLabel = max((float)ts.score[ti], labels.empty() ? 0.f : labels[0]);
      buf.clear();
      for (int i = 0; i < k; i++) { if (U(rng) >= prefixRate) continue; Sample o; to_sample(pre[i], i, 0, preLabel, o); buf.push_back(o); }
      for (size_t i = 0; i < states.size(); i++) { float lab = labels[i]; if (i == 0) lab = max(lab, preLabel); Sample o; to_sample(states[i], k + (int)i, 1, lab, o); buf.push_back(o); }
      std::lock_guard<std::mutex> lk(mx);
      fwrite(buf.data(), sizeof(Sample), buf.size(), fo); nS += buf.size(); nP++; lvP[L]++; if (r.finished) { nFin++; lvF[L]++; }
    }
  };
  vector<std::thread> th; for (int t = 0; t < threads; t++) th.emplace_back(worker, t);
  double lastLog = 0;
  while (!STOP && elapsed() < tmax) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (elapsed() - lastLog > 30) { lastLog = elapsed(); std::lock_guard<std::mutex> lk(mx); fflush(fo);
      printf("[%5.0fs] paths %lld (finished %lld), samples %lld, rollouts %lld\n", elapsed(), nP, nFin, nS, (long long)ROLLOUTS); fflush(stdout); }
  }
  STOP = true; for (auto& t : th) t.join();
  fclose(fo);
  printf("gen done: %lld paths (%lld finished; per level 0-3: %lld/%lld %lld/%lld %lld/%lld %lld/%lld), %lld samples appended to %s, %.0fs\n",
         nP, nFin, lvF[0], lvP[0], lvF[1], lvP[1], lvF[2], lvP[2], lvF[3], lvP[3], nS, out.c_str(), elapsed());
  return 0;
}

// ============================================================================================
// feat: samples -> feature matrix (header: "AFEAT" ver N F; rows: F floats, T, label, depth, src)
// ============================================================================================
static int mode_feat(Args& A) {
  vector<string> ins; string out = "tmp/feat.bin"; size_t cap = SIZE_MAX; int threads = 1;
  for (; A.has(); A.i++) {
    string o = A.a[A.i]; auto nx = [&]() { if (A.i + 1 >= A.a.size()) die("missing value for " + o); return A.a[++A.i]; };
    if (o == "--in") ins.push_back(nx()); else if (o == "--out") out = nx(); else if (o == "--max") cap = stoull(nx()); else if (o == "--threads") threads = stoi(nx());
    else die("feat: unknown option " + o);
  }
  vector<Sample> all;
  for (auto& f : ins) { auto v = read_samples(f); printf("%s: %zu samples\n", f.c_str(), v.size()); all.insert(all.end(), v.begin(), v.end()); }
  std::unordered_map<uint64_t, int32_t> bf; bf.reserve(all.size());
  for (auto& a : all) if (a.label > 0) { S s; from_sample(a, s); uint64_t k = BestFuture::key(s); int32_t fut = max<int32_t>(0, (int32_t)lrintf(a.label) - a.T); auto it = bf.find(k); if (it == bf.end()) bf.emplace(k, fut); else it->second = max(it->second, fut); }
  std::unordered_set<uint64_t> seen; vector<Sample> keep; long long raised = 0;
  for (auto& a : all) {
    S s; from_sample(a, s); uint64_t k = BestFuture::key(s);
    uint64_t kk = k ^ ((uint64_t)(uint32_t)a.T * 0x9E3779B97F4A7C15ULL);
    if (!seen.insert(kk).second) continue;
    auto it = bf.find(k); Sample b = a;
    if (it != bf.end() && (float)(a.T + it->second) > b.label) { b.label = (float)(a.T + it->second); raised++; }
    keep.push_back(b);
  }
  std::mt19937 rng(7); shuffle(keep.begin(), keep.end(), rng); if (keep.size() > cap) keep.resize(cap);
  FILE* fo = fopen(out.c_str(), "wb"); if (!fo) die("cannot write " + out);
  int hdr[4] = {0x54414641, FEAT_VERSION, (int)keep.size(), NF}; fwrite(hdr, sizeof hdr, 1, fo);
  const int RW = NF + 4; size_t chunk = 65536;
  vector<float> buf;
  for (size_t s0 = 0; s0 < keep.size(); s0 += chunk) {
    size_t n = min(chunk, keep.size() - s0); buf.assign(n * RW, 0.f);
    auto work = [&](int t) {
      for (size_t j = t; j < n; j += threads) { const Sample& a = keep[s0 + j]; S s; from_sample(a, s); float* r = &buf[j * RW];
        features(s, a.depth, r); r[NF] = (float)a.T; r[NF + 1] = a.label; r[NF + 2] = a.depth; r[NF + 3] = a.src; }
    };
    vector<std::thread> th; for (int t = 0; t < threads; t++) th.emplace_back(work, t); for (auto& x : th) x.join();
    fwrite(buf.data(), sizeof(float), buf.size(), fo);
  }
  fclose(fo);
  long long fin = 0; for (auto& a : keep) fin += a.label > 0;
  printf("feat: %zu samples in, %zu unique out (%lld finishable, %lld labels raised by the best-future table), %d features -> %s\n",
         all.size(), keep.size(), fin, raised, NF, out.c_str());
  return 0;
}

// ============================================================================================
// search
// ============================================================================================
static const uint32_t NIL = 0xFFFFFFFFu;
struct Node { uint32_t parent; uint16_t delta; uint8_t q; uint8_t flags; float best; };
static_assert(sizeof(Node) == 12, "Node layout");
struct HE { float f; uint32_t id; float fraw; uint32_t a1, a2; };
static inline bool operator<(const HE& a, const HE& b) { return a.f < b.f; }
#pragma pack(push, 1)
struct DE { uint64_t k; int32_t T; };
#pragma pack(pop)

struct Search {
  // config
  uint64_t MAXN = 60000000, MAXOPEN = 20000000; int DLOG2 = 25; double PRUNE_KEEP = 0.75;
  double NOISE = 60, NOVELTY_W = 10; int K1 = 8, K2 = 32, CNT_LOG2 = 24;
  int BATCH = 1024;  // pops per thread per round
  // state
  Node* NODES = nullptr; uint64_t NNODE = 0;
  vector<HE> HEAP; DE* DD = nullptr; uint64_t DMASK = 0, DCOUNT = 0, DCAP = 0;
  vector<uint32_t> CNT1, CNT2;
  S ROOT; int ROOT_DEPTH = 0; vector<Mv> ROOT_PREFIX;
  long long EXP = 0, CHILD = 0, FIN = 0, DEAD = 0, DUP = 0, STALE = 0, GC = 0, PRUNE = 0, LAZY = 0, MAXDEPTH = 0, DDCLEAR = 0;

  inline uint32_t cb(uint32_t id) const { return (id * 2654435761u) >> (32 - CNT_LOG2); }
  inline float novelty_pen(const HE& e) const {
    if (NOVELTY_W <= 0 || CNT1.empty()) return 0.f;
    return (float)(NOVELTY_W * (log2f(1.f + (float)CNT1[cb(e.a1)]) + log2f(1.f + (float)CNT2[cb(e.a2)])));
  }
  void clear_counts() { if (NOVELTY_W > 0) { CNT1.assign(1u << CNT_LOG2, 0); CNT2.assign(1u << CNT_LOG2, 0); } }
  static inline uint64_t skey(const S& s) { uint64_t h = hashS(s, true); return h ? h : 1; }
  inline int32_t dd_get(uint64_t k) const {
    uint64_t i = k & DMASK;
    while (true) { if (DD[i].k == 0) return INT32_MIN; if (DD[i].k == k) return DD[i].T; i = (i + 1) & DMASK; }
  }
  void dd_put(uint64_t k, int32_t T) {
    if (DCOUNT * 10 >= DCAP * 7) { memset(DD, 0, sizeof(DE) * DCAP); DCOUNT = 0; DDCLEAR++; }
    uint64_t i = k & DMASK;
    while (true) {
      if (DD[i].k == 0) { DD[i].k = k; DD[i].T = T; DCOUNT++; return; }
      if (DD[i].k == k) { if (T > DD[i].T) DD[i].T = T; return; }
      i = (i + 1) & DMASK;
    }
  }
  bool alloc() {
    NODES = (Node*)malloc(sizeof(Node) * MAXN); DCAP = 1ULL << DLOG2; DMASK = DCAP - 1; DD = (DE*)calloc(DCAP, sizeof(DE));
    HEAP.reserve(MAXOPEN + 1);
    return NODES && DD;
  }
  void reset(const vector<Mv>& prefix) {
    ROOT = start_state(); ROOT_PREFIX = prefix; ROOT_DEPTH = (int)prefix.size();
    for (auto& m : prefix) apply_move(ROOT, m.delta, m.q);
    NNODE = 1; NODES[0] = {NIL, 0, 0, 0, -1.f};
    HEAP.clear(); HEAP.push_back({1e9f, 0, 1e9f, 0, 0});
    memset(DD, 0, sizeof(DE) * DCAP); DCOUNT = 0; dd_put(skey(ROOT), ROOT.T);
    clear_counts();
  }
  int rebuild(uint32_t id, S& s, vector<Mv>* path = nullptr, vector<uint32_t>* chain = nullptr) const {
    static thread_local vector<Mv> mv; mv.clear(); if (chain) chain->clear();
    for (uint32_t c = id; c != 0; c = NODES[c].parent) { mv.push_back({NODES[c].delta, NODES[c].q}); if (chain) chain->push_back(c); }
    s = ROOT; for (size_t i = mv.size(); i-- > 0;) apply_move(s, mv[i].delta, mv[i].q);
    if (path) path->assign(mv.rbegin(), mv.rend());
    return ROOT_DEPTH + (int)mv.size();
  }
  void gc() {
    uint64_t nw = (NNODE + 63) / 64; vector<uint64_t> mark(nw, 0);
    auto markup = [&](uint32_t id) { while (id != NIL && !((mark[id >> 6] >> (id & 63)) & 1)) { mark[id >> 6] |= 1ULL << (id & 63); id = NODES[id].parent; } };
    markup(0); for (auto& h : HEAP) markup(h.id);
    vector<uint32_t> rk(nw + 1, 0);
    for (uint64_t w = 0; w < nw; w++) rk[w + 1] = rk[w] + (uint32_t)__builtin_popcountll(mark[w]);
    auto rank = [&](uint32_t id) { return rk[id >> 6] + (uint32_t)__builtin_popcountll(mark[id >> 6] & ((1ULL << (id & 63)) - 1)); };
    for (uint64_t i = 0; i < NNODE; i++) {
      if (!((mark[i >> 6] >> (i & 63)) & 1)) continue;
      Node nd = NODES[i]; if (nd.parent != NIL) nd.parent = rank(nd.parent); NODES[rank((uint32_t)i)] = nd;
    }
    NNODE = rk[nw];
    for (auto& h : HEAP) { h.id = rank(h.id); h.a1 = rank(h.a1); h.a2 = rank(h.a2); }  // ancestors are always marked
    clear_counts(); GC++;
  }
  void prune_heap(uint64_t keep) {
    if (HEAP.size() <= keep) return;
    std::nth_element(HEAP.begin(), HEAP.begin() + keep, HEAP.end(), [](const HE& a, const HE& b) { return a.f > b.f; });
    HEAP.resize(keep); std::make_heap(HEAP.begin(), HEAP.end()); PRUNE++;
  }
};

// online replay (samples) shared between the A* thread, label workers and the trainer
struct Replay {
  vector<Sample> off, on; size_t onCap = 1000000, onPos = 0; std::mutex mx; long long added = 0;
  void add(const Sample& s) { std::lock_guard<std::mutex> lk(mx); if (on.size() < onCap) on.push_back(s); else { on[onPos] = s; onPos = (onPos + 1) % onCap; } added++; }
};

struct LJob { S s; int depth; vector<Mv> path; };  // path: full move list from a1

static int mode_search(Args& A) {
  Search X; int threads = 1, labelThreads = 0, labelLevel = 1, labelLevelShallow = 2, shallowDepth = 36;
  double tmax = 60, pLabel = 0.02, restartStale = 0, logEvery = 10, refitEvery = 0, onlineFrac = 0.5, refitLr = 3e-4, pTree = 0.3;
  long long poolMin = 7500, record = 8123; long long rescoreK = 200000;
  string modelFn, seedFn, outFn = "tmp/search_best.txt", poolFn = "tmp/pool.txt", checker = "tools/check_tour.py", recDir = ".", saveModel;
  vector<string> replayFns; size_t replayCap = 2000000; int pfxLo = 0, pfxHi = 0; unsigned seed = 1; int refitBatch = 256;
  for (; A.has(); A.i++) {
    string o = A.a[A.i]; auto nx = [&]() { if (A.i + 1 >= A.a.size()) die("missing value for " + o); return A.a[++A.i]; };
    if (o == "--model") modelFn = nx(); else if (o == "--threads") threads = max(1, stoi(nx())); else if (o == "--time") tmax = stod(nx());
    else if (o == "--seed-tour") seedFn = nx();
    else if (o == "--prefix") { string s = nx(); if (sscanf(s.c_str(), "%d:%d", &pfxLo, &pfxHi) != 2) { pfxLo = pfxHi = stoi(s); } }
    else if (o == "--out") outFn = nx(); else if (o == "--pool") poolFn = nx(); else if (o == "--pool-min") poolMin = stoll(nx());
    else if (o == "--record") record = stoll(nx()); else if (o == "--record-dir") recDir = nx(); else if (o == "--checker") checker = nx();
    else if (o == "--noise") X.NOISE = stod(nx()); else if (o == "--novelty-w") X.NOVELTY_W = stod(nx());
    else if (o == "--novelty-k1") X.K1 = max(1, stoi(nx())); else if (o == "--novelty-k2") X.K2 = max(1, stoi(nx()));
    else if (o == "--gamma") GAMMA = stod(nx()); else if (o == "--depth-bonus") DEPTH_BONUS = stod(nx());
    else if (o == "--max-nodes") X.MAXN = stoull(nx()); else if (o == "--max-open") X.MAXOPEN = stoull(nx()); else if (o == "--dedup-log2") X.DLOG2 = stoi(nx());
    else if (o == "--batch") X.BATCH = max(1, stoi(nx()));
    else if (o == "--restart-stale") restartStale = stod(nx()); else if (o == "--log-every") logEvery = stod(nx());
    else if (o == "--rng") seed = stoul(nx());
    else if (o == "--label-threads") labelThreads = stoi(nx()); else if (o == "--p-label") pLabel = stod(nx());
    else if (o == "--label-level") labelLevel = stoi(nx()); else if (o == "--label-level-shallow") labelLevelShallow = stoi(nx()); else if (o == "--shallow-depth") shallowDepth = stoi(nx());
    else if (o == "--refit") refitEvery = stod(nx()); else if (o == "--refit-lr") refitLr = stod(nx()); else if (o == "--refit-batch") refitBatch = stoi(nx());
    else if (o == "--online-frac") onlineFrac = stod(nx()); else if (o == "--replay") replayFns.push_back(nx()); else if (o == "--replay-cap") replayCap = stoull(nx());
    else if (o == "--p-tree") pTree = stod(nx()); else if (o == "--rescore") rescoreK = stoll(nx()); else if (o == "--save-model") saveModel = nx();
    else die("search: unknown option " + o);
  }
  auto model = std::make_shared<Model>();
  if (!modelFn.empty()) { if (!model->load(modelFn)) die("cannot load model " + modelFn); printf("model %s: %d features, %d hidden\n", modelFn.c_str(), model->F, model->H); }
  else printf("no --model: using the fallback heuristic (T + 60 min per optimistic sit)\n");
  if (refitEvery > 0 && !model->ok()) die("--refit needs --model (train one with train.py)");
  vector<Mv> seedTour;
  if (!seedFn.empty()) { auto v = parse_tours(seedFn.c_str(), false); if (v.empty()) die("no tour in " + seedFn); seedTour = v[0]; }
  if (pfxHi < pfxLo) pfxHi = pfxLo;
  if (pfxHi > 0 && seedTour.empty()) die("--prefix needs --seed-tour");
  if (pfxHi >= (int)seedTour.size() && pfxHi > 0) pfxHi = (int)seedTour.size() - 1;
  if (!X.alloc()) die("out of memory (lower --max-nodes / --dedup-log2)");
  printf("search: %d threads, %d label threads, refit every %.0fs, noise %.0f, novelty %.1f (k %d/%d), gamma %.2f, depth bonus %.2f, prefix %d-%d, "
         "max nodes %llu, max open %llu, memory ~%.1f GB\n", threads, labelThreads, refitEvery, X.NOISE, X.NOVELTY_W, X.K1, X.K2, GAMMA, DEPTH_BONUS, pfxLo, pfxHi,
         (unsigned long long)X.MAXN, (unsigned long long)X.MAXOPEN,
         (X.MAXN * 12.0 + X.MAXOPEN * 20.0 + (1ULL << X.DLOG2) * 12.0 + 2.0 * (4ULL << X.CNT_LOG2)) / 1e9);
  fflush(stdout);
  std::mt19937_64 rng(seed); std::mt19937 rng32((unsigned)seed * 31 + 7);
  auto pick_prefix = [&]() { int k = pfxLo == pfxHi ? pfxLo : std::uniform_int_distribution<int>(pfxLo, pfxHi)(rng32); return vector<Mv>(seedTour.begin(), seedTour.begin() + k); };
  X.reset(pick_prefix());
  printf("root: %d moves, T %d, at %s\n", X.ROOT_DEPTH, X.ROOT.T, nm(X.ROOT.pos).c_str());

  // ---- results ----
  long long bestScore = -1; vector<Mv> bestSeq; std::unordered_set<uint64_t> poolSeen; long long poolN = 0;
  std::mutex resMx;
  auto report_tour = [&](const vector<Mv>& full, long long score, const char* how) {  // any thread
    std::lock_guard<std::mutex> lk(resMx);
    if (score <= bestScore && score < poolMin) return;
    string txt = tour_text(full); long long m = text_minutes(txt);
    if (m != score) { printf("WARNING: %s tour failed engine replay (%lld vs %lld), dropped\n", how, m, score); return; }
    uint64_t h = std::hash<string>()(txt);
    if (score >= poolMin && poolSeen.insert(h).second) { FILE* f = fopen(poolFn.c_str(), "a"); if (f) { fprintf(f, "%lld\t%s\n", score, txt.c_str()); fclose(f); } poolN++; }
    if (score > bestScore) {
      bestScore = score; bestSeq = full;
      FILE* f = fopen(outFn.c_str(), "w"); if (f) { fprintf(f, "%lld\n%s\n", score, txt.c_str()); fclose(f); }
      printf("[%6.0fs] NEW BEST %lld (%zu moves, %s)\n", elapsed(), score, full.size(), how); fflush(stdout);
      if (score > record && !checker.empty()) {
        string tf = outFn + ".check.txt"; FILE* g = fopen(tf.c_str(), "w"); if (g) { fprintf(g, "%s", txt.c_str()); fclose(g); }
        string cmd = "python3 " + checker + " \"$(cat " + tf + ")\" 2>&1"; FILE* pp = popen(cmd.c_str(), "r"); string res; char b[4096];
        if (pp) { while (fgets(b, sizeof b, pp)) res += b; pclose(pp); }
        bool ok = res.find("ok=True") != string::npos && res.find("unnecessary=[]") != string::npos && res.find("maxvisits=3") != string::npos;
        printf("checker: %s", res.c_str());
        if (ok) { string rf = recDir + "/NEW_RECORD_" + to_string(score) + ".txt"; FILE* r = fopen(rf.c_str(), "w"); if (r) { fprintf(r, "%lld\n%s\n", score, txt.c_str()); fclose(r); } printf("NEW RECORD %lld verified -> %s\n", score, rf.c_str()); }
        fflush(stdout);
      }
    }
  };

  // ---- learning infrastructure ----
  Replay RP; BestFuture BF; std::mutex modelMx; std::shared_ptr<Model> pending; std::atomic<bool> haveNew{false};
  std::atomic<long long> trainSteps{0}; double lastLf = 0, lastLu = 0; std::mutex lossMx;
  bool learn = refitEvery > 0;
  if (learn) {
    for (auto& f : replayFns) {
      auto v = read_samples(f);
      if (v.size() > replayCap) { std::mt19937 r(3); shuffle(v.begin(), v.end(), r); v.resize(replayCap); }
      for (auto& a : v) if (a.label > 0) { S s; from_sample(a, s); BF.put(s, max<int32_t>(0, (int32_t)lrintf(a.label) - a.T)); }
      RP.off.insert(RP.off.end(), v.begin(), v.end()); printf("replay: %zu samples from %s\n", v.size(), f.c_str());
    }
  }
  std::deque<LJob> LQ; std::mutex LQmx; std::atomic<long long> ljobs{0}, ljobsFin{0};
  vector<std::thread> bg;
  for (int t = 0; t < labelThreads; t++) bg.emplace_back([&, t]() {
    setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 5);
    std::mt19937 r((unsigned)seed * 1009u + t * 7u + 3); vector<S> states; vector<float> labels; std::uniform_real_distribution<double> U(0, 1);
    while (!STOP) {
      LJob j; bool got = false;
      { std::lock_guard<std::mutex> lk(LQmx); if (!LQ.empty()) { j = std::move(LQ.back()); LQ.pop_back(); got = true; } }
      if (!got) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
      Result res = nrpa(j.depth < shallowDepth ? labelLevelShallow : labelLevel, j.s, Policy(), r);
      if (STOP) break;
      ljobs++; if (res.finished) ljobsFin++;
      if (res.finished && res.minutes >= min(poolMin, bestScore + 1)) { vector<Mv> full = j.path; full.insert(full.end(), res.seq.begin(), res.seq.end()); report_tour(full, res.minutes, "label worker"); }
      if (!learn) continue;
      label_path(j.s, res.seq, res.finished ? (double)res.minutes : 0.0, states, labels, r);
      for (size_t i = 0; i < states.size(); i++) {
        if (labels[i] > 0) BF.put(states[i], max<int32_t>(0, (int32_t)labels[i] - states[i].T));
        if (i > 0 && U(r) >= 0.25) continue;
        Sample o; to_sample(states[i], j.depth + (int)i, i == 0 ? 2 : 3, labels[i], o); RP.add(o);
      }
    }
  });
  if (learn) bg.emplace_back([&]() {
    setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 5);
    Trainer tr; { std::lock_guard<std::mutex> lk(modelMx); tr.reset(*model); } tr.lr = refitLr;
    std::mt19937 r((unsigned)seed * 77u + 1); std::uniform_real_distribution<double> U(0, 1);
    vector<float> Xb, Tb, Lb; double lastPub = elapsed(); double alf = 0, alu = 0; int na = 0;
    while (!STOP) {
      size_t non; { std::lock_guard<std::mutex> lk(RP.mx); non = RP.on.size(); }
      if (non < 1000 && RP.off.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(200)); continue; }
      int B = refitBatch; Xb.resize((size_t)B * NF); Tb.resize(B); Lb.resize(B);
      for (int b = 0; b < B; b++) {
        Sample a;
        bool useOn = non >= 1000 && (RP.off.empty() || U(r) < onlineFrac);
        if (useOn) { std::lock_guard<std::mutex> lk(RP.mx); a = RP.on[std::uniform_int_distribution<size_t>(0, RP.on.size() - 1)(r)]; }
        else a = RP.off[std::uniform_int_distribution<size_t>(0, RP.off.size() - 1)(r)];
        S s; from_sample(a, s); int32_t fut = BF.get(s); float lab = a.label; if (fut >= 0 && (float)(a.T + fut) > lab) lab = (float)(a.T + fut);
        features(s, a.depth, &Xb[(size_t)b * NF]); Tb[b] = (float)a.T; Lb[b] = lab;
      }
      auto [lf, lu] = tr.step(Xb, Tb, Lb); alf += lf; alu += lu; na++; trainSteps++;
      if (elapsed() - lastPub >= refitEvery) {
        lastPub = elapsed(); auto nm_ = std::make_shared<Model>(tr.m); nm_->fold();
        { std::lock_guard<std::mutex> lk(modelMx); pending = nm_; } haveNew = true;
        { std::lock_guard<std::mutex> lk(lossMx); lastLf = alf / max(1, na); lastLu = alu / max(1, na); } alf = alu = 0; na = 0;
        if (!saveModel.empty()) nm_->save(saveModel);
      }
    }
  });

  // ---- main A* loop ----
  struct Ch { uint32_t parent; uint16_t delta; uint8_t q; uint32_t a1, a2; uint64_t key; int32_t T; float fraw; };
  struct Fin { uint32_t parent; int delta; long long score; };
  vector<vector<Ch>> lch(threads); vector<vector<Fin>> lfin(threads); vector<vector<LJob>> llab(threads);
  vector<long long> ldead(threads), lstale(threads), lmaxd(threads);
  double lastLog = elapsed(), lastImprove = elapsed(), lastRestart = elapsed(); long long lastBest = -1; int restarts = 0;
  long long lastExp = 0; double lastExpT = elapsed();
  std::normal_distribution<float> Nn(0, 1);

  auto record_finish = [&](uint32_t parentId, int delta, long long score) {
    X.FIN++;
    bool raised = false;
    for (uint32_t c = parentId; c != NIL; c = X.NODES[c].parent) { if (X.NODES[c].best >= (float)score) break; X.NODES[c].best = (float)score; raised = true; }
    bool good = score > bestScore || score >= poolMin;
    if (!good && !(learn && raised)) return;
    vector<Mv> path; vector<uint32_t> chain; S s; X.rebuild(parentId, s, &path, &chain);
    if (good) { vector<Mv> full = X.ROOT_PREFIX; full.insert(full.end(), path.begin(), path.end()); full.push_back({delta, H8}); report_tour(full, score, "A*"); }
    if (learn && raised) {  // tree samples: every state on the path, labelled with the best finish below it
      S st = X.ROOT; int d = X.ROOT_DEPTH; std::uniform_real_distribution<double> U(0, 1);
      for (size_t j = chain.size() + 1; j-- > 0;) {
        uint32_t c = j == chain.size() ? 0 : chain[j];
        if (c != 0) { apply_move(st, X.NODES[c].delta, X.NODES[c].q); d++; }
        float lab = max(X.NODES[c].best, (float)score);
        if (lab > 0) BF.put(st, max<int32_t>(0, (int32_t)lrintf(lab) - st.T));
        if (U(rng) < pTree) { Sample o; to_sample(st, d, 4, lab, o); RP.add(o); }
      }
    }
  };

  auto rescore = [&](const Model* m) {
    if (X.HEAP.empty() || rescoreK <= 0) return 0.0;
    long long k = min<long long>(rescoreK, (long long)X.HEAP.size());
    std::sort(X.HEAP.begin(), X.HEAP.end(), [](const HE& a, const HE& b) { return a.f > b.f; });
    vector<float> nf(k); vector<uint64_t> seeds(threads); for (auto& s : seeds) s = rng();
    auto work = [&](int t) {
      std::mt19937_64 r(seeds[t]); std::normal_distribution<float> N(0, 1); float xb[NF];
      for (long long i = t; i < k; i += threads) { S s; int d = X.rebuild(X.HEAP[i].id, s); nf[i] = value_of(m, s, d, xb) + (float)X.NOISE * N(r); }
    };
    vector<std::thread> th; for (int t = 0; t < threads; t++) th.emplace_back(work, t); for (auto& x : th) x.join();
    vector<float> dl(k); for (long long i = 0; i < k; i++) { dl[i] = nf[i] - X.HEAP[i].fraw; X.HEAP[i].fraw = nf[i]; X.HEAP[i].f = nf[i] - X.novelty_pen(X.HEAP[i]); }
    vector<float> tail(dl.begin() + (k * 3) / 4, dl.end()); if (tail.empty()) tail = dl;
    std::nth_element(tail.begin(), tail.begin() + tail.size() / 2, tail.end()); float shift = tail[tail.size() / 2];
    for (size_t i = k; i < X.HEAP.size(); i++) { X.HEAP[i].f += shift; X.HEAP[i].fraw += shift; }
    std::make_heap(X.HEAP.begin(), X.HEAP.end());
    return (double)shift;
  };

  while (!STOP && elapsed() < tmax) {
    // adopt new weights from the trainer
    if (haveNew.exchange(false)) {
      std::shared_ptr<Model> nm_; { std::lock_guard<std::mutex> lk(modelMx); nm_ = pending; }
      if (nm_) { model = nm_; double t0 = elapsed(); double sh = rescore(model.get());
        printf("[%6.0fs] adopted refit weights (%lld steps, loss fin %.4f fut %.4f); re-scored %lld open entries in %.1fs, shift %.1f\n",
               elapsed(), (long long)trainSteps, lastLf, lastLu, min<long long>(rescoreK, (long long)X.HEAP.size()), elapsed() - t0, sh); fflush(stdout); }
    }
    // restart when empty or stale
    bool stale = restartStale > 0 && elapsed() - max(lastImprove, lastRestart) > restartStale;
    if (X.HEAP.empty() || stale) {
      restarts++; X.reset(pick_prefix()); lastRestart = elapsed();
      printf("[%6.0fs] restart %d (%s): root after %d moves\n", elapsed(), restarts, stale ? "stale" : "open list empty", X.ROOT_DEPTH); fflush(stdout);
    }
    // pop
    vector<uint32_t> pop; int B = X.BATCH * threads;
    while ((int)pop.size() < B && !X.HEAP.empty()) {
      std::pop_heap(X.HEAP.begin(), X.HEAP.end()); HE e = X.HEAP.back(); X.HEAP.pop_back();
      if (X.NOVELTY_W > 0) {
        float fn = e.fraw - X.novelty_pen(e);
        if (fn < e.f - 0.5f && !X.HEAP.empty() && fn < X.HEAP.front().f) { e.f = fn; X.HEAP.push_back(e); std::push_heap(X.HEAP.begin(), X.HEAP.end()); X.LAZY++; continue; }
        X.CNT1[X.cb(e.a1)]++; X.CNT2[X.cb(e.a2)]++;
      }
      pop.push_back(e.id);
    }
    if (pop.empty()) continue;
    const Model* M = model.get();
    int nt = max(1, min(threads, (int)pop.size() / 8 + 1));
    vector<uint64_t> seeds(nt); for (auto& s : seeds) s = rng();
    auto work = [&](int t) {
      lch[t].clear(); lfin[t].clear(); llab[t].clear(); ldead[t] = lstale[t] = 0;
      std::mt19937_64 r(seeds[t]); std::uniform_real_distribution<double> U(0, 1); std::normal_distribution<float> N(0, 1);
      vector<pair<int, int>> mv, mv2; vector<uint32_t> chain; float xb[NF];
      for (size_t j = t; j < pop.size(); j += nt) {
        uint32_t id = pop[j]; S s; int depth = X.rebuild(id, s, nullptr, &chain);
        uint32_t ca1 = (size_t)(X.K1 - 1) < chain.size() ? chain[X.K1 - 1] : 0;
        uint32_t ca2 = (size_t)(X.K2 - 1) < chain.size() ? chain[X.K2 - 1] : 0;
        if (id != 0 && X.dd_get(X.skey(s)) > s.T) { lstale[t]++; continue; }
        lmaxd[t] = max<long long>(lmaxd[t], depth);
        if (labelThreads > 0 && U(r) < pLabel) { LJob jb; jb.s = s; jb.depth = depth; vector<Mv> p; S tmp; X.rebuild(id, tmp, &p); jb.path = X.ROOT_PREFIX; jb.path.insert(jb.path.end(), p.begin(), p.end()); llab[t].push_back(std::move(jb)); }
        pinned_moves(s, 3, true, mv);
        for (auto& [d, q] : mv) {
          if (q == H8) { long long sc = s.T + (long long)d * (d ? group_n(s, s.pos) : 0); lfin[t].push_back({id, d, sc}); continue; }
          S c = s; apply_move(c, d, q);
          pinned_moves(c, 3, true, mv2);
          if (mv2.empty()) { ldead[t]++; continue; }
          uint64_t key = X.skey(c); int32_t old = X.dd_get(key);
          if (old != INT32_MIN && old >= c.T) continue;  // counted as dup in the merge below only if it survives here
          float v = value_of(M, c, depth + 1, xb) + (X.NOISE > 0 ? (float)X.NOISE * N(r) : 0.f);
          lch[t].push_back({id, (uint16_t)d, (uint8_t)q, ca1, ca2, key, c.T, v});
        }
      }
    };
    if (nt == 1) work(0); else { vector<std::thread> th; for (int t = 0; t < nt; t++) th.emplace_back(work, t); for (auto& x : th) x.join(); }
    X.EXP += (long long)pop.size();
    // merge
    size_t need = 0; for (int t = 0; t < nt; t++) need += lch[t].size();
    if (X.NNODE + need + 16 > X.MAXN) { X.gc(); if (X.NNODE + need + 16 > X.MAXN * 0.9) { X.prune_heap((uint64_t)(X.HEAP.size() * 0.5)); X.gc(); } }
    for (int t = 0; t < nt; t++) {
      X.DEAD += ldead[t]; X.STALE += lstale[t]; X.MAXDEPTH = max(X.MAXDEPTH, lmaxd[t]);
      for (auto& f : lfin[t]) record_finish(f.parent, f.delta, f.score);
      for (auto& c : lch[t]) {
        int32_t old = X.dd_get(c.key);
        if (old != INT32_MIN && old >= c.T) { X.DUP++; continue; }
        if (X.NNODE >= X.MAXN) break;
        X.dd_put(c.key, c.T);
        uint32_t id = (uint32_t)X.NNODE++; X.NODES[id] = {c.parent, c.delta, c.q, 0, -1.f};
        HE e{c.fraw, id, c.fraw, c.a1, c.a2}; e.f = c.fraw - X.novelty_pen(e);
        X.HEAP.push_back(e); std::push_heap(X.HEAP.begin(), X.HEAP.end()); X.CHILD++;
      }
    }
    if (X.HEAP.size() > X.MAXOPEN) X.prune_heap((uint64_t)(X.MAXOPEN * X.PRUNE_KEEP));
    if (labelThreads > 0) { std::lock_guard<std::mutex> lk(LQmx); for (int t = 0; t < nt; t++) for (auto& j : llab[t]) { LQ.push_back(std::move(j)); if (LQ.size() > 20000) LQ.pop_front(); } }
    { std::lock_guard<std::mutex> lk(resMx); if (bestScore > lastBest) { lastBest = bestScore; lastImprove = elapsed(); } }
    if (elapsed() - lastLog >= logEvery) {
      double dt = elapsed() - lastExpT; double rate = (X.EXP - lastExp) / max(1e-9, dt); lastExp = X.EXP; lastExpT = elapsed(); lastLog = elapsed();
      size_t lq; { std::lock_guard<std::mutex> lk(LQmx); lq = LQ.size(); }
      printf("[%6.0fs] exp %lld (%.0f/s) children %lld fin %lld dead %lld dup %lld stale %lld lazy %lld | nodes %llu open %zu top f %.0f maxdepth %lld gc %lld prune %lld dd %llu | best %lld pool %lld",
             elapsed(), X.EXP, rate, X.CHILD, X.FIN, X.DEAD, X.DUP, X.STALE, X.LAZY, (unsigned long long)X.NNODE, X.HEAP.size(), X.HEAP.empty() ? 0.0 : X.HEAP.front().f,
             X.MAXDEPTH, X.GC, X.PRUNE, (unsigned long long)X.DCOUNT, bestScore, poolN);
      if (labelThreads > 0) printf(" | label jobs %lld (fin %lld) queue %zu", (long long)ljobs, (long long)ljobsFin, lq);
      if (learn) printf(" | train steps %lld replay on %zu", (long long)trainSteps, RP.on.size());
      printf("\n"); fflush(stdout);
    }
  }
  STOP = true; for (auto& t : bg) t.join();
  if (learn && !saveModel.empty() && model->ok()) model->save(saveModel);
  printf("done after %.0fs: best %lld, %lld expansions (%.0f/s), %lld pool tours (>= %lld) in %s, best in %s\n", elapsed(), bestScore, X.EXP, X.EXP / max(1e-9, elapsed()),
         poolN, poolMin, poolFn.c_str(), outFn.c_str());
  return 0;
}

// ============================================================================================
static int mode_check(Args& A) {
  if (!A.has()) die("check FILE");
  auto v = parse_tours(A.a[A.i].c_str(), false);
  for (auto& t : v) { long long m = seq_minutes(t); printf("%lld minutes, %zu moves%s\n", m, t.size(), m >= 0 && text_minutes(tour_text(t)) == m ? "" : " (ILLEGAL)"); }
  return 0;
}
static int mode_bench(Args& A) {
  string modelFn, tour = "../../data/tours/record_8123.txt"; int reps = 200;
  for (; A.has(); A.i++) { string o = A.a[A.i]; if (o == "--model") modelFn = A.a[++A.i]; else if (o == "--tour") tour = A.a[++A.i]; else if (o == "--reps") reps = stoi(A.a[++A.i]); else die("bench: unknown " + o); }
  Model m; if (!modelFn.empty() && !m.load(modelFn)) die("cannot load model"); if (!m.ok()) m.init(64, 1);
  auto v = parse_tours(tour.c_str(), true); if (v.empty()) die("no tour");
  vector<S> st; S s = start_state(); for (auto& mv : v[0]) { st.push_back(s); apply_move(s, mv.delta, mv.q); }
  float xb[NF]; double acc = 0; auto t0 = std::chrono::steady_clock::now();
  for (int r = 0; r < reps; r++) for (size_t i = 0; i < st.size(); i++) acc += value_of(&m, st[i], (int)i, xb);
  double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  printf("%zu evals in %.3fs: %.2f us/eval (features + MLP %d hidden), checksum %.1f\n", st.size() * reps, dt, 1e6 * dt / (st.size() * reps), m.H, acc);
  if (!modelFn.empty()) {
    printf("values along the tour (move: T, value):");
    for (size_t i = 0; i < st.size(); i += 10) printf(" %zu:%d/%.0f", i, st[i].T, value_of(&m, st[i], (int)i, xb));
    printf("\n");
  }
  return 0;
}

int main(int argc, char** argv) {
  init_board(); D0 = 1; D0 = h8dist(start_state());
  for (int p = 0; p < 64; p++) { for (int q = 0; q < 64; q++) KIDX[p][q] = 0; for (int k = 0; k < NBN[p]; k++) KIDX[p][NBQ[p][k]] = k; }
  UNIFORM = new Policy();
  signal(SIGINT, on_sig); signal(SIGTERM, on_sig);
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 2) { fprintf(stderr, "usage: astar_cpu gen|feat|search|check|bench [options] (see RUN.md)\n"); return 1; }
  Args A; for (int i = 2; i < argc; i++) A.a.push_back(argv[i]);
  string m = argv[1];
  if (m == "gen") return mode_gen(A);
  if (m == "feat") return mode_feat(A);
  if (m == "search") return mode_search(A);
  if (m == "check") return mode_check(A);
  if (m == "bench") return mode_bench(A);
  die("unknown mode " + m);
}
