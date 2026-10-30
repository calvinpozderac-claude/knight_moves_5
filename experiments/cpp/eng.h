// Shared engine for the swamp-knight puzzle (integer altitudes).
#pragma once
#include <bits/stdc++.h>
using namespace std;
typedef int16_t AT;
static const int H8 = 63, A1 = 0;
static int ALT0[64];
static int NBN[64], NBQ[64][16], NBT[64][16];  // type 0 knight(dz 0) 1 adjacent(dz +-2) 2 two-away(dz +-1)

static const int ROWS[8][8] = {
    {9, 8, 10, 12, 11, 8, 10, 17}, {7, 9, 11, 9, 10, 12, 14, 12}, {4, 7, 5, 8, 8, 6, 13, 10},
    {4, 10, 7, 9, 6, 8, 7, 9},     {2, 6, 4, 2, 5, 9, 8, 11},     {0, 3, 1, 4, 2, 7, 10, 7},
    {1, 2, 0, 1, 2, 5, 7, 6},      {0, 2, 4, 3, 5, 6, 2, 4}};

static inline string nm(int i) { string s; s += char('a' + i % 8); s += char('1' + i / 8); return s; }

static void init_board() {
  for (int r = 0; r < 8; r++)
    for (int x = 0; x < 8; x++) ALT0[x + 8 * (7 - r)] = ROWS[r][x];
  for (int i = 0; i < 64; i++) {
    NBN[i] = 0;
    int x = i % 8, y = i / 8;
    for (int dx = -2; dx <= 2; dx++)
      for (int dy = -2; dy <= 2; dy++) {
        int a = abs(dx), b = abs(dy);
        int t;
        if ((a == 1 && b == 2) || (a == 2 && b == 1)) t = 0;
        else if (a + b == 1) t = 1;
        else if ((a == 2 && b == 0) || (a == 0 && b == 2)) t = 2;
        else continue;
        int X = x + dx, Y = y + dy;
        if (X < 0 || X > 7 || Y < 0 || Y > 7) continue;
        NBQ[i][NBN[i]] = X + 8 * Y; NBT[i][NBN[i]] = t; NBN[i]++;
      }
  }
}

static inline bool legal_dz(int t, int dz) {
  if (t == 0) return dz == 0;
  if (t == 1) return dz == 2 || dz == -2;
  return dz == 1 || dz == -1;
}

struct S {
  AT alt[64];
  uint8_t vis[64];
  uint8_t pos;
  int32_t T;
};

static inline uint64_t hashS(const S& s, bool withVis = true) {
  uint64_t h = 1469598103934665603ULL ^ s.pos;
  const uint64_t* p = (const uint64_t*)s.alt;
  for (int i = 0; i < 16; i++) { h ^= p[i] + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); }
  if (withVis) {
    const uint64_t* v = (const uint64_t*)s.vis;
    for (int i = 0; i < 8; i++) { h ^= v[i] + 0x7f4a7c159e3779b9ULL + (h << 7) + (h >> 3); }
  }
  return h;
}

// group size n at position p (excluding opposite if equal)
static inline int group_n(const S& s, int p) {
  AT a = s.alt[p]; int O = 63 - p, n = 0;
  for (int i = 0; i < 64; i++) if (s.alt[i] == a && i != O) n++;
  return n;
}

// Pinned moves: (delta, q). delta>=0 is the sink that makes the jump to q legal.
static inline void pinned_moves(const S& s, int cap, bool allowH8, vector<pair<int, int>>& out) {
  out.clear();
  int p = s.pos; AT a = s.alt[p]; int O = 63 - p;
  for (int k = 0; k < NBN[p]; k++) {
    int q = NBQ[p][k], t = NBT[p][k];
    if (q == H8) { if (!allowH8) continue; }
    else if (s.vis[q] >= cap) continue;
    if (s.alt[q] == a && q != O) { if (t == 0) out.push_back({0, q}); continue; }
    int diff = s.alt[q] - a;
    if (t == 0) { if (-diff >= 0) out.push_back({-diff, q}); }
    else if (t == 1) { for (int dz : {2, -2}) if (dz - diff >= 0) out.push_back({dz - diff, q}); }
    else { for (int dz : {1, -1}) if (dz - diff >= 0) out.push_back({dz - diff, q}); }
  }
}

// apply sink delta at s.pos then jump to q; returns minutes waited
static inline int apply_move(S& s, int delta, int q) {
  int waited = 0;
  if (delta > 0) {
    int p = s.pos; AT a = s.alt[p]; int O = 63 - p;
    bool rise = s.alt[O] != a;
    int n = 0;
    for (int i = 0; i < 64; i++) if (s.alt[i] == a && i != O) { s.alt[i] -= delta; n++; }
    if (rise) s.alt[O] += delta;
    waited = n * delta;
    s.T += waited;
  }
  s.vis[q]++; s.pos = q;
  return waited;
}

static inline bool jump_ok(const S& s, int p, int q, int t) { return legal_dz(t, s.alt[q] - s.alt[p]); }

// static BFS from s.pos; returns reachable mask (excluding H8), sets h8reach
static inline uint64_t static_reach(const S& s, int cap, bool& h8reach, int& maxAlt) {
  uint64_t seen = 1ULL << s.pos;
  int st[64], sp = 0; st[sp++] = s.pos;
  h8reach = false; maxAlt = s.alt[s.pos];
  while (sp) {
    int u = st[--sp];
    for (int k = 0; k < NBN[u]; k++) {
      int q = NBQ[u][k];
      if (!jump_ok(s, u, q, NBT[u][k])) continue;
      if (q == H8) { h8reach = true; continue; }
      if (seen >> q & 1) continue;
      if (s.vis[q] >= cap) continue;
      seen |= 1ULL << q; st[sp++] = q;
      if (s.alt[q] > maxAlt) maxAlt = s.alt[q];
    }
  }
  return seen;
}

static S start_state() {
  S s; for (int i = 0; i < 64; i++) { s.alt[i] = ALT0[i]; s.vis[i] = 0; }
  s.vis[A1] = 1; s.pos = A1; s.T = 0; return s;
}
