// Learned move prior for NRPA (GNRPA-style bias): per-move features + a small MLP (or linear) scorer.
// Shared by gen.cpp (training data) and pnrpa.cpp (search). Include after ../../src/eng.h.
#pragma once
#include <string>
#include <vector>

static const int PF = 42;  // number of features per move
static double PRIOR_D0 = 1;  // 3D distance a1 -> h8 at the start (set by prior_init)

static inline void prior_init() {
  S s = start_state(); int p = s.pos;
  double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  PRIOR_D0 = sqrt(dx * dx + dy * dy + dz * dz);
}

static inline int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

// features of the move (delta, q) from state s (q != H8). f must hold PF floats.
static inline void move_features(const S& s, int delta, int q, float* f) {
  for (int i = 0; i < PF; i++) f[i] = 0;
  int p = s.pos, a = s.alt[p], O = 63 - p;
  int nsink = 0; if (delta > 0) nsink = group_n(s, p);
  int wait = nsink * delta;
  bool oppLeaves = delta > 0 && s.alt[O] == a;
  bool h8Sinks = delta > 0 && s.alt[H8] == a;
  S c = s; apply_move(c, delta, q);
  int aq = c.alt[q], dz = aq - c.alt[p];
  f[0] = wait / 100.f;
  f[1] = log1pf((float)wait) / 5.f;
  f[2] = delta > 0;
  f[3] = min(delta, 8) / 4.f;
  f[4] = delta == 4;
  f[5] = nsink / 64.f;
  f[6 + clampi(dz, -2, 2) + 2] = 1;  // 6..10: dz one-hot (-2..2)
  // groups after the move
  int ngq = 0, nlo = 0, nhi = 0, vlq = 0, vllo = 0, sumvis = 0, Oq = 63 - q;
  for (int i = 0; i < 64; i++) {
    int ai = c.alt[i]; sumvis += c.vis[i];
    if (ai == aq) { if (i != Oq) ngq++; if (i != H8) vlq += 3 - min<int>(c.vis[i], 3); }
    else if (ai == aq - 2) { nlo++; if (i != H8) vllo += 3 - min<int>(c.vis[i], 3); }
    else if (ai == aq + 2) nhi++;
  }
  f[11] = ngq / 64.f; f[12] = nlo / 64.f; f[13] = nhi / 64.f;
  // largest group at another altitude
  int bestN = 0, bestA = aq;
  {
    int alts[64]; for (int i = 0; i < 64; i++) alts[i] = c.alt[i];
    sort(alts, alts + 64);
    for (int i = 0; i < 64;) { int j = i; while (j < 64 && alts[j] == alts[i]) j++;
      if (alts[i] != aq && j - i > bestN) { bestN = j - i; bestA = alts[i]; } i = j; }
  }
  int gap = clampi(bestA - aq, -6, 6);
  f[14] = gap / 6.f; f[15] = gap == -2; f[16] = gap == 2; f[17] = bestN / 64.f;
  int vlQ = 3 - min<int>(c.vis[q], 3); f[18 + clampi(vlQ, 0, 2)] = 1;          // 18..20
  int vlO = Oq == H8 ? 3 : 3 - min<int>(c.vis[Oq], 3); f[21 + clampi(vlO, 0, 3)] = 1;  // 21..24
  f[25] = c.alt[Oq] == aq;
  f[26] = oppLeaves;
  f[27] = oppLeaves ? (O == H8 ? 0.f : (3 - min<int>(s.vis[O], 3)) / 3.f) : 0.f;
  bool h8r; int mx; uint64_t reach = static_reach(c, 3, h8r, mx);
  f[28] = h8r; f[29] = __builtin_popcountll(reach) / 64.f;
  double dx = 7 - q % 8, dy = 7 - q / 8, dd = (double)c.alt[H8] - aq;
  f[30] = (float)(sqrt(dx * dx + dy * dy + dd * dd) / PRIOR_D0);
  f[31] = clampi(c.alt[H8] - aq, -10, 10) / 10.f;
  f[32] = c.alt[H8] == aq; f[33] = c.alt[H8] == aq - 2;
  f[34] = sumvis / 130.f;
  static thread_local vector<pair<int, int>> mv;
  pinned_moves(c, 3, true, mv);
  int mob = 0; bool fin = false; for (auto& m : mv) { if (m.second == H8) fin = true; else mob++; }
  f[35] = mob / 10.f; f[36] = mob == 0 && !fin; f[37] = fin;
  f[38] = vlq / 100.f; f[39] = vllo / 100.f;
  f[40] = h8Sinks;
  f[41] = (delta == 0) * f[34];
}

// The scorer: x' = (x - mean) / sd; out = w2 . tanh(W1 x' + b1)  (H > 0)  or  w . x'  (H == 0)
struct PriorModel {
  int F = 0, H = 0; vector<float> mean, sd, W1, b1, w2;
  bool load(const std::string& fn) {
    FILE* fp = fopen(fn.c_str(), "r"); if (!fp) return false;
    if (fscanf(fp, "%d %d", &F, &H) != 2 || F != PF) { fprintf(stderr, "prior file %s: bad header (F must be %d)\n", fn.c_str(), PF); fclose(fp); return false; }
    auto rd = [&](vector<float>& v, size_t n) { v.resize(n); for (auto& x : v) if (fscanf(fp, "%f", &x) != 1) return false; return true; };
    bool ok = rd(mean, F) && rd(sd, F);
    if (H > 0) ok = ok && rd(W1, (size_t)H * F) && rd(b1, H) && rd(w2, H); else ok = ok && rd(w2, F);
    fclose(fp); if (!ok) fprintf(stderr, "prior file %s: truncated\n", fn.c_str());
    return ok;
  }
  float score(const float* f) const {
    float x[PF]; for (int i = 0; i < F; i++) x[i] = (f[i] - mean[i]) / sd[i];
    if (H == 0) { float o = 0; for (int i = 0; i < F; i++) o += w2[i] * x[i]; return o; }
    float o = 0;
    for (int h = 0; h < H; h++) { const float* w = &W1[(size_t)h * F]; float z = b1[h]; for (int i = 0; i < F; i++) z += w[i] * x[i]; o += w2[h] * tanhf(z); }
    return o;
  }
};
