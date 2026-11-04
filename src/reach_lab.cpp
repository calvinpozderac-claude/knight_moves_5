// Reachability lab: compares "can h8 still be reached?" tests on positions whose answer is known.
//
// Ground truth: every position along a finished tour (files of "minutes\ntour" pairs) can reach h8, so any
// test that says "dead" there is wrong (a false dead). Pruning power: positions from random legal walks off
// those tours (most of which can no longer finish); the share each test calls dead.
//
// Tests:
//  orig  the notebook's reach_end (backward search from h8 with restricted points; known to be unsound)
//  L1    reach_end without restricted points (a point may sink any number of times)
//  L2    L1, and a point whose opposite can still be visited may also rise (any altitude above works too)
//  L3    L2, and the point being stepped onto may have sunk (only the visit cap is left: plain graph search)
//  ceil  altitude ceiling (provably sound): the knight's altitude only rises on a jump, by at most 2
//        (adjacent), 1 (two away) or 0 (knight); a point rises only if its opposite can still be stood on.
//        H[X] = most the knight's altitude can be on X; h8 is reachable only if a neighbour u has
//        H[u] + k >= alt(h8), or some point can get level with h8 (so h8 could be sunk)
//
// usage: reach_lab [--walks 20] [--walk-len 40] [--rng 1] FILE...
#define main heur_search_main
#include "heur_search.cpp"
#undef main

// allow: points that may still be stood on later in a finishing tour (a point stood on later must itself be
// able to reach h8, so the fixed point below shrinks it to the reach set)
static inline bool can_rise(const S& s, int X, uint64_t allow = ~0ULL) {
  int O = 63 - X; return X != H8 && (allow >> O & 1) && (s.vis[O] < 3 || s.pos == O);
}
// can u's altitude ever drop? only while the knight stands on a point level with it: one that can still be
// stood on and is (or can get) level with u. Points that can rise count as sinkable (they can get anywhere).
static bool sinkable(const S& s, int u, uint64_t allow = ~0ULL) {
  if (can_rise(s, u, allow)) return true;
  for (int z = 0; z < 64; z++) {
    if (z == 63 - u || z == H8 || !(allow >> z & 1)) continue;
    if (s.vis[z] >= 3 && z != s.pos) continue;
    if (s.alt[z] >= s.alt[u] || can_rise(s, z, allow)) return true;
  }
  return false;
}
static uint64_t reach_loose(const S& s, int level, uint64_t allow = ~0ULL) {  // backward search from h8
  uint64_t reach = 0, queued = 1ULL << H8; int q[64], qh = 0, qt = 0; q[qt++] = H8;
  while (qh < qt) {
    int u = q[qh++]; reach |= 1ULL << u;
    int ux = u % 8, uy = u / 8, A = s.alt[u];
    bool uLow = level == 3 || (level == 4 && sinkable(s, u, allow));  // u may have been sunk below A
    auto tryX = [&](int X, int exact) {
      if ((queued >> X & 1) || (s.vis[X] >= 3 && X != s.pos)) return;
      int Y = s.alt[X];
      bool canRise = level >= 2 && can_rise(s, X, allow);
      bool ok = Y >= A - exact || canRise || uLow;  // X sinks (or rises) to A +- exact; or u is lowered
      if (exact == 0 && !ok) return;
      if (!ok) return;
      queued |= 1ULL << X; q[qt++] = X;
    };
    for (auto& d : ORTH) {
      int x = ux + d[0], y = uy + d[1]; if (onb(x, y)) tryX(x + 8 * y, 2);
      x = ux + 2 * d[0]; y = uy + 2 * d[1]; if (onb(x, y)) tryX(x + 8 * y, 1);
    }
    for (auto& d : KN) { int x = ux + d[0], y = uy + d[1]; if (onb(x, y)) tryX(x + 8 * y, 0); }
  }
  return reach;
}

static bool alive_orig(const S& s) { vector<Mv> mv; return score_of(s, mv) > 0; }
static bool alive_loose(const S& s, int level) { return reach_loose(s, level) >> s.pos & 1; }
// L2b iterated: rises and sinks only through points that are themselves still in the reach set
static uint64_t reach_fix(const S& s, int* iters = nullptr) {
  uint64_t R = ~0ULL; int it = 0;
  for (;;) { uint64_t N = reach_loose(s, 4, R | (1ULL << H8)); it++; if (N == R || !(N >> s.pos & 1)) { R = N; break; } R = N; }
  if (iters) *iters = it; return R;
}
// sound bounded lookahead: depth-first over distinct positions (L2b-filtered); 0 = finish found, 1 = proven
// dead (search ran out of positions within the budget), 2 = unknown
static int lookahead(const S& s0, long long budget, long long* used = nullptr) {
  std::unordered_set<uint64_t> vis; vector<S> st{s0}; vector<Mv> m2; long long nodes = 0; int res = 1;
  while (!st.empty()) {
    S c = st.back(); st.pop_back();
    if (++nodes > budget) { res = 2; break; }
    gen_moves(c, m2); bool hit = false;
    for (auto& m : m2) {
      S d = c; if (!do_move(d, m)) continue;
      if (m.q == H8) { hit = true; break; }
      if (!alive_loose(d, 4)) continue;
      d.T = 0; if (vis.insert(hashS(d)).second) st.push_back(d);
    }
    if (hit) { res = 0; break; }
  }
  if (used) *used = nodes; return res;
}
static bool alive_fix(const S& s) { return reach_fix(s) >> s.pos & 1; }

static const int INF = 1 << 28;
static bool alive_ceil(const S& s) {
  int U[64], H[64];
  for (int i = 0; i < 64; i++) {
    U[i] = (s.vis[63 - i] < 3 || s.pos == 63 - i) && i != H8 ? INF : s.alt[i];
    H[i] = -INF;
  }
  H[s.pos] = s.alt[s.pos];
  int q[4096], qh = 0, qt = 0; bool inq[64] = {}; q[qt++] = s.pos; inq[s.pos] = true;
  int need = s.alt[H8];
  while (qh != qt) {
    int u = q[qh]; qh = (qh + 1) & 4095; inq[u] = false;
    for (int k = 0; k < NBN[u]; k++) {
      int X = NBQ[u][k], t = NBT[u][k], up = t == 1 ? 2 : (t == 2 ? 1 : 0);
      if (X == H8) { if (H[u] + up >= need) return true; continue; }
      if (s.vis[X] >= 3) continue;
      int h = min(U[X], H[u] + up);
      if (h > H[X]) { H[X] = h; if (h >= need) return true; if (!inq[X]) { inq[X] = true; q[qt] = X; qt = (qt + 1) & 4095; } }
    }
  }
  return false;
}

int main(int argc, char** argv) {
  init_board();
  int WALKS = 20, WLEN = 40, EXN = 0, MID = 0; long long EXB = 2000000; unsigned RNG = 1; vector<string> files;
  for (int i = 1; i < argc; i++) {
    string a = argv[i];
    if (a == "--walks") WALKS = atoi(argv[++i]); else if (a == "--walk-len") WLEN = atoi(argv[++i]);
    else if (a == "--mid") MID = atoi(argv[++i]);
    else if (a == "--exhaust") EXN = atoi(argv[++i]); else if (a == "--exhaust-budget") EXB = atoll(argv[++i]);
    else if (a == "--rng") RNG = (unsigned)atoi(argv[++i]); else files.push_back(a);
  }
  vector<vector<Mv>> tours;
  for (auto& fn : files) {
    std::ifstream f(fn); string line;
    while (std::getline(f, line)) {
      if (line.find('(') == string::npos) continue;
      vector<Mv> mv; const char* p = strchr(line.c_str(), '('); int w; char sq[8];
      while (p && sscanf(p, "(%d, %2s)", &w, sq) == 2) { mv.push_back({w, (sq[0] - 'a') + 8 * (sq[1] - '1')}); p = strchr(p + 1, '('); }
      if (!mv.empty() && mv.back().q == H8) tours.push_back(mv);
    }
  }
  const int NV = 8;
  const char* NAME[NV] = {"orig", "L1", "L2", "L3", "ceil", "L2b", "L3+ceil", "fix"};
  auto alive = [&](const S& s, int v) {
    switch (v) { case 0: return alive_orig(s); case 4: return alive_ceil(s); case 5: return alive_loose(s, 4);
      case 6: return alive_loose(s, 3) && alive_ceil(s); case 7: return alive_fix(s); default: return alive_loose(s, v); } };
  long long nTrue = 0, falseDead[NV] = {}, nWalk = 0, dead[NV] = {}, walkDeadEnd = 0; vector<S> walkPos;
  std::mt19937_64 rng(RNG); vector<Mv> mv;
  std::unordered_set<uint64_t> seen;
  for (auto& t : tours) {
    S s = start_state();
    for (size_t i = 0; i < t.size(); i++) {
      if (seen.insert(hashS(s)).second) {
        nTrue++;
        for (int v = 0; v < NV; v++) if (!alive(s, v)) falseDead[v]++;
      }
      // random legal walk off the tour from this position (never into h8)
      if ((int)(rng() % 100) < 100 * WALKS / (int)t.size() + 1) {
        S w = s;
        for (int st = 0; st < WLEN; st++) {
          gen_moves(w, mv); vector<Mv> ok;
          for (auto& m : mv) { if (m.q == H8) continue; S c = w; if (do_move(c, m)) ok.push_back(m); }
          if (ok.empty()) break;
          S prev = w; do_move(w, ok[rng() % ok.size()]);
          // stop at the first position with no moves left; score the one before it (it still has moves)
          gen_moves(w, mv); bool any = false; for (auto& m : mv) { S c = w; if (m.q != H8 && do_move(c, m)) { any = true; break; } }
          if (!any) { walkDeadEnd++; w = prev; break; }
        }
        nWalk++; walkPos.push_back(w);
        for (int v = 0; v < NV; v++) if (!alive(w, v)) dead[v]++;
      }
      if (!do_move(s, t[i])) { fprintf(stderr, "illegal tour move\n"); break; }
    }
  }
  printf("%zu tours, %lld distinct on-tour positions (all can finish), %lld random-walk positions that still have moves (%lld walks ran out of moves)\n",
         tours.size(), nTrue, nWalk, walkDeadEnd);
  printf("%-7s %14s %20s\n", "test", "false dead", "walk positions dead");
  for (int v = 0; v < NV; v++)
    printf("%-7s %8lld %5.2f%% %12lld %6.2f%%\n", NAME[v], falseDead[v], 100.0 * falseDead[v] / nTrue, dead[v], 100.0 * dead[v] / max(1LL, nWalk));
  // timing
  long long sink = 0; size_t nT = min<size_t>(walkPos.size(), 100000);
  for (int v = 0; v < NV; v++) {
    auto a = std::chrono::steady_clock::now(); for (size_t r = 0; r < nT; r++) sink += alive(walkPos[r], v);
    printf("%-7s %.2f us/call\n", NAME[v], std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count() / nT * 1e6);
  }
  printf("(%lld)\n", sink);
  // ground truth for a sample of walk positions the sound tests keep: depth-first search for any finish
  // (deduplicated), up to EXB nodes each: finishes / proven dead (search exhausted) / unknown
  if (EXN > 0) {
    long long fin = 0, deadP = 0, unk = 0, l1dead[3] = {}, l2dead[3] = {}, fxdead[3] = {};
    std::shuffle(walkPos.begin(), walkPos.end(), rng); int done = 0; vector<long long> finN, deadN;
    for (auto& w0 : walkPos) {
      if (done >= EXN) break;
      if (!alive_loose(w0, 4)) continue;
      done++;
      std::unordered_set<uint64_t> vis; vector<S> st{w0}; long long nodes = 0; int res = 1;  // 0 fin 1 dead 2 unknown
      while (!st.empty()) {
        S c = st.back(); st.pop_back();
        if (++nodes > EXB) { res = 2; break; }
        vector<Mv> m2; gen_moves(c, m2); bool hit = false;
        for (auto& m : m2) {
          S d = c; if (!do_move(d, m)) continue;
          if (m.q == H8) { hit = true; break; }
          if (!alive_loose(d, 4)) continue;
          d.T = 0; uint64_t h = hashS(d); if (vis.insert(h).second) st.push_back(d);
        }
        if (hit) { res = 0; break; }
      }
      (res == 0 ? fin : res == 1 ? deadP : unk)++;
      if (res == 0) finN.push_back(nodes); else if (res == 1) deadN.push_back(nodes);
      if (!alive_loose(w0, 1)) l1dead[res]++;
      if (!alive_loose(w0, 2)) l2dead[res]++;
      if (!alive_fix(w0)) fxdead[res]++;
    }
    printf("exhaustive check of %d walk positions kept by L2b (budget %lld nodes): can finish %lld, proven dead %lld, unknown %lld\n",
           done, EXB, fin, deadP, unk);
    printf("  of these, L1 calls dead: %lld / %lld / %lld;  L2 calls dead: %lld / %lld / %lld\n",
           l1dead[0], l1dead[1], l1dead[2], l2dead[0], l2dead[1], l2dead[2]);
    auto pct = [](vector<long long> v) { std::sort(v.begin(), v.end()); string r;
      for (double f : {0.25, 0.5, 0.75, 0.9, 1.0}) if (!v.empty()) r += " " + std::to_string(v[min(v.size() - 1, (size_t)(f * v.size()))]);
      return r; };
    printf("  nodes to prove dead (25/50/75/90/100%%):%s\n  nodes to find a finish:%s\n", pct(deadN).c_str(), pct(finN).c_str());
    printf("  fix calls dead: %lld / %lld / %lld\n", fxdead[0], fxdead[1], fxdead[2]);
  }
  if (MID > 0) {  // positions at random depths of random walks off the tours: share proven dead per budget
    vector<S> mid; std::uniform_int_distribution<int> L(1, WLEN);
    for (auto& t : tours) {
      if ((int)mid.size() >= MID) break;
      S s = start_state(); int cut = rng() % t.size();
      for (int i = 0; i < cut; i++) do_move(s, t[i]);
      int len = L(rng); bool stuck = false;
      for (int st2 = 0; st2 < len && !stuck; st2++) {
        gen_moves(s, mv); vector<Mv> ok;
        for (auto& m : mv) { if (m.q == H8) continue; S c = s; if (do_move(c, m)) ok.push_back(m); }
        if (ok.empty()) stuck = true; else do_move(s, ok[rng() % ok.size()]);
      }
      if (!stuck) mid.push_back(s);
    }
    long long l2b = 0; for (auto& s : mid) l2b += !alive_loose(s, 4);
    printf("%zu positions at random walk depths: L2b dead %.1f%%\n", mid.size(), 100.0 * l2b / mid.size());
    for (long long b : {10LL, 30LL, 100LL, 300LL, 1000LL, 3000LL}) {
      long long c[3] = {}, tot = 0; auto a = std::chrono::steady_clock::now();
      for (auto& s : mid) { long long u; c[lookahead(s, b, &u)]++; tot += u; }
      double us = std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count() / mid.size() * 1e6;
      printf("  budget %5lld: finish %5.1f%%  proven dead %5.1f%%  unknown %5.1f%%  | %.0f nodes, %.1f us per position\n",
             b, 100.0 * c[0] / mid.size(), 100.0 * c[1] / mid.size(), 100.0 * c[2] / mid.size(), (double)tot / mid.size(), us);
    }
  }
  return 0;
}
