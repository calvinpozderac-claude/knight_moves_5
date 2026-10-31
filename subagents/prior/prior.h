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
// cheap: skip the static-reach and mobility features (28, 29, 35, 36, 37; left at 0), for models trained
// without them (train_prior.py --drop 28,29,35,36,37 writes flag 1 in the model header)
static const int CHEAP_FEATS[5] = {28, 29, 35, 36, 37};
static inline void move_features(const S& s, int delta, int q, float* f, bool cheap = false) {
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
  int amin = c.alt[0], amax = c.alt[0];
  for (int i = 1; i < 64; i++) { amin = min<int>(amin, c.alt[i]); amax = max<int>(amax, c.alt[i]); }
  for (int i = 0; i < 64; i++) {
    int ai = c.alt[i]; sumvis += c.vis[i];
    if (ai == aq) { if (i != Oq) ngq++; if (i != H8) vlq += 3 - min<int>(c.vis[i], 3); }
    else if (ai == aq - 2) { nlo++; if (i != H8) vllo += 3 - min<int>(c.vis[i], 3); }
    else if (ai == aq + 2) nhi++;
  }
  f[11] = ngq / 64.f; f[12] = nlo / 64.f; f[13] = nhi / 64.f;
  // largest group at another altitude
  int bestN = 0, bestA = aq;
  if (amax - amin < 1024) {  // counting; ties go to the lowest altitude (as the sorted scan did)
    uint8_t cntA[1024]; int R = amax - amin + 1; memset(cntA, 0, R);
    for (int i = 0; i < 64; i++) cntA[c.alt[i] - amin]++;
    for (int k = 0; k < R; k++) if (cntA[k] > bestN && k + amin != aq) { bestN = cntA[k]; bestA = k + amin; }
  } else {
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
  if (!cheap) { bool h8r; int mx; uint64_t reach = static_reach(c, 3, h8r, mx);
    f[28] = h8r; f[29] = __builtin_popcountll(reach) / 64.f; }
  double dx = 7 - q % 8, dy = 7 - q / 8, dd = (double)c.alt[H8] - aq;
  f[30] = (float)(sqrt(dx * dx + dy * dy + dd * dd) / PRIOR_D0);
  f[31] = clampi(c.alt[H8] - aq, -10, 10) / 10.f;
  f[32] = c.alt[H8] == aq; f[33] = c.alt[H8] == aq - 2;
  f[34] = sumvis / 130.f;
  if (!cheap) {
    static thread_local vector<pair<int, int>> mv;
    pinned_moves(c, 3, true, mv);
    int mob = 0; bool fin = false; for (auto& m : mv) { if (m.second == H8) fin = true; else mob++; }
    f[35] = mob / 10.f; f[36] = mob == 0 && !fin; f[37] = fin;
  }
  f[38] = vlq / 100.f; f[39] = vllo / 100.f;
  f[40] = h8Sinks;
  f[41] = (delta == 0) * f[34];
}

// Same features for every move of a pinned-move list at once (rows of PF floats in out; h8 rows are 0).
// Moves that share a sink share the post-sink altitude histogram, so this is much cheaper than calling
// move_features per move; gives exactly the same numbers (checked by `gen --check`).
static inline void all_move_features(const S& s, const vector<pair<int, int>>& mv, float* out, bool cheap = false) {
  int n = (int)mv.size(); memset(out, 0, sizeof(float) * PF * n);
  int p = s.pos, a = s.alt[p], O = 63 - p;
  int sumvis0 = 0; for (int i = 0; i < 64; i++) sumvis0 += s.vis[i];
  int nsinkAll = group_n(s, p);
  bool done[64] = {false};
  for (int i0 = 0; i0 < n; i0++) {
    if (mv[i0].second == H8 || done[i0]) continue;
    int delta = mv[i0].first;
    S c = s;  // the state after the sink (knight still at p)
    int nsink = 0;
    if (delta > 0) { bool rise = s.alt[O] != a; for (int i = 0; i < 64; i++) if (s.alt[i] == a && i != O) c.alt[i] -= delta; if (rise) c.alt[O] += delta; nsink = nsinkAll; }
    int amin = c.alt[0], amax = c.alt[0];
    for (int i = 1; i < 64; i++) { amin = min<int>(amin, c.alt[i]); amax = max<int>(amax, c.alt[i]); }
    if (amax - amin >= 1020) { for (int i = i0; i < n; i++) if (!done[i] && mv[i].second != H8 && mv[i].first == delta) { move_features(s, delta, mv[i].second, out + PF * i, cheap); done[i] = true; } continue; }
    int R = amax - amin + 1, off = 2 - amin;  // index = alt + off, with 2 slots of padding on each side
    uint8_t cnt[1028]; int16_t VL[1028]; memset(cnt, 0, R + 4); memset(VL, 0, sizeof(int16_t) * (R + 4));
    for (int i = 0; i < 64; i++) { int k = c.alt[i] + off; cnt[k]++; if (i != H8) VL[k] += 3 - min<int>(c.vis[i], 3); }
    int b1 = -1, b1n = 0, b2 = -1, b2n = 0;  // largest and second-largest groups (ties: lowest altitude)
    for (int k = 2; k < R + 2; k++) { if (cnt[k] > b1n) { b2 = b1; b2n = b1n; b1 = k; b1n = cnt[k]; } else if (cnt[k] > b2n) { b2 = k; b2n = cnt[k]; } }
    int wait = nsink * delta;
    for (int i = i0; i < n; i++) {
      if (done[i] || mv[i].second == H8 || mv[i].first != delta) continue;
      done[i] = true; int q = mv[i].second; float* f = out + PF * i;
      int aq = c.alt[q], dz = aq - c.alt[p], kq = aq + off, Oq = 63 - q;
      f[0] = wait / 100.f; f[1] = log1pf((float)wait) / 5.f; f[2] = delta > 0; f[3] = min(delta, 8) / 4.f; f[4] = delta == 4; f[5] = nsink / 64.f;
      f[6 + clampi(dz, -2, 2) + 2] = 1;
      int ngq = cnt[kq] - (c.alt[Oq] == aq ? 1 : 0);
      f[11] = ngq / 64.f; f[12] = cnt[kq - 2] / 64.f; f[13] = cnt[kq + 2] / 64.f;
      int bestN = 0, bestA = aq;
      if (b1 >= 0 && b1 != kq) { bestN = b1n; bestA = b1 - off; } else if (b2 >= 0) { bestN = b2n; bestA = b2 - off; }
      int gap = clampi(bestA - aq, -6, 6);
      f[14] = gap / 6.f; f[15] = gap == -2; f[16] = gap == 2; f[17] = bestN / 64.f;
      int vlQ = 3 - min<int>(s.vis[q] + 1, 3); f[18 + clampi(vlQ, 0, 2)] = 1;
      int vlO = Oq == H8 ? 3 : 3 - min<int>(s.vis[Oq], 3); f[21 + clampi(vlO, 0, 3)] = 1;
      f[25] = c.alt[Oq] == aq;
      bool oppLeaves = delta > 0 && s.alt[O] == a;
      f[26] = oppLeaves; f[27] = oppLeaves ? (O == H8 ? 0.f : (3 - min<int>(s.vis[O], 3)) / 3.f) : 0.f;
      double dx = 7 - q % 8, dy = 7 - q / 8, dd = (double)c.alt[H8] - aq;
      f[30] = (float)(sqrt(dx * dx + dy * dy + dd * dd) / PRIOR_D0);
      f[31] = clampi(c.alt[H8] - aq, -10, 10) / 10.f;
      f[32] = c.alt[H8] == aq; f[33] = c.alt[H8] == aq - 2;
      f[34] = (sumvis0 + 1) / 130.f;
      f[38] = (VL[kq] - (s.vis[q] < 3 ? 1 : 0)) / 100.f; f[39] = VL[kq - 2] / 100.f;
      f[40] = delta > 0 && s.alt[H8] == a;
      f[41] = (delta == 0) * f[34];
      if (!cheap) {
        S d = c; d.vis[q]++; d.pos = q;
        bool h8r; int mx; uint64_t reach = static_reach(d, 3, h8r, mx);
        f[28] = h8r; f[29] = __builtin_popcountll(reach) / 64.f;
        static thread_local vector<pair<int, int>> mv2;
        pinned_moves(d, 3, true, mv2);
        int mob = 0; bool fin = false; for (auto& m : mv2) { if (m.second == H8) fin = true; else mob++; }
        f[35] = mob / 10.f; f[36] = mob == 0 && !fin; f[37] = fin;
      }
    }
  }
}

// The scorer: x' = (x - mean) / sd; out = w2 . tanh(W1 x' + b1)  (H > 0)  or  w . x'  (H == 0)
struct PriorModel {
  int F = 0, H = 0, flags = 0; vector<float> mean, sd, W1, b1, w2;
  bool cheap() const { return flags & 1; }
  bool load(const std::string& fn) {
    FILE* fp = fopen(fn.c_str(), "r"); if (!fp) return false;
    char hdr[256]; if (!fgets(hdr, sizeof hdr, fp)) { fclose(fp); return false; }
    int nh = sscanf(hdr, "%d %d %d", &F, &H, &flags);
    if (nh < 2 || F != PF) { fprintf(stderr, "prior file %s: bad header (F must be %d)\n", fn.c_str(), PF); fclose(fp); return false; }
    auto rd = [&](vector<float>& v, size_t n) { v.resize(n); for (auto& x : v) if (fscanf(fp, "%f", &x) != 1) return false; return true; };
    bool ok = rd(mean, F) && rd(sd, F);
    if (H > 0) ok = ok && rd(W1, (size_t)H * F) && rd(b1, H) && rd(w2, H); else ok = ok && rd(w2, F);
    fclose(fp); if (!ok) fprintf(stderr, "prior file %s: truncated\n", fn.c_str());
    return ok;
  }
  float score(const float* f) const {
    float x[PF]; const float* mu = mean.data(); const float* sg = sd.data();
    for (int i = 0; i < PF; i++) x[i] = (f[i] - mu[i]) / sg[i];
    if (H == 0) { float o = 0; for (int i = 0; i < PF; i++) o += w2[i] * x[i]; return o; }
    float o = 0; const float* W = W1.data();
    for (int h = 0; h < H; h++) { const float* w = W + (size_t)h * PF; float z = b1[h]; for (int i = 0; i < PF; i++) z += w[i] * x[i]; o += w2[h] * tanhf(z); }
    return o;
  }
};
