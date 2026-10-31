// NRPA over an extended move set: altitudes scaled by SC (so 1/SC-unit sinks are representable),
// pinned moves exactly as src/eng.h (wait implied by the jump), plus optional "free" sinks:
// sink the knight's own group by FSTEP..FMAX scaled units, then a dz=0 knight move to a member of
// that same group (the jump was legal before the wait and stays legal for any wait, so under the
// pinned reading of "every wait must be necessary" it is NOT necessary; tools/check_tour.py's global
// perturbation test can still accept it when the rest of the tour depends on the offset).
// usage: fnrpa --sc 2 --free 1 --fstep 1 --fmax 8 --level 3 --time 600 --seed FILE --amin 35 --amax 60
//              --rng 1 --out best.txt --pool pool.txt --pool-min 8000
#include "../../src/eng.h"
#include <chrono>
typedef long long ll;
static int SC = 1, FREE = 0, FSTEP = 1, FMAX = 4, ITERS = 100;
struct St { int alt[64]; uint8_t vis[64]; int pos; ll Tsc; };
struct Mv { int d, q; };
static St start() { St s; for (int i = 0; i < 64; i++) { s.alt[i] = ALT0[i] * SC; s.vis[i] = 0; } s.vis[A1] = 1; s.pos = A1; s.Tsc = 0; return s; }
static inline int gsize(const St& s, int p) { int a = s.alt[p], O = 63 - p, n = 0; for (int i = 0; i < 64; i++) if (s.alt[i] == a && i != O) n++; return n; }
static inline bool okdz(int t, int dz) { if (dz % SC) return false; return legal_dz(t, dz / SC); }
static void gen(const St& s, bool allowH8, vector<Mv>& out) {
  out.clear(); int p = s.pos, a = s.alt[p], O = 63 - p;
  for (int k = 0; k < NBN[p]; k++) {
    int q = NBQ[p][k], t = NBT[p][k];
    if (q == H8) { if (!allowH8) continue; } else if (s.vis[q] >= 3) continue;
    if (s.alt[q] == a && q != O) {
      if (t == 0) { out.push_back({0, q}); if (FREE && q != H8) for (int d = FSTEP; d <= FMAX; d += FSTEP) out.push_back({d, q}); }
      continue;
    }
    int diff = s.alt[q] - a;
    if (t == 0) { if (-diff >= 0) out.push_back({-diff, q}); }
    else if (t == 1) { for (int dz : {2, -2}) if (dz * SC - diff >= 0) out.push_back({dz * SC - diff, q}); }
    else { for (int dz : {1, -1}) if (dz * SC - diff >= 0) out.push_back({dz * SC - diff, q}); }
  }
}
static inline ll apply(St& s, int d, int q) {
  ll w = 0;
  if (d > 0) { int p = s.pos, a = s.alt[p], O = 63 - p; bool rise = s.alt[O] != a; int n = 0;
    for (int i = 0; i < 64; i++) if (s.alt[i] == a && i != O) { s.alt[i] -= d; n++; }
    if (rise) s.alt[O] += d; w = (ll)n * d; s.Tsc += w; }
  s.vis[q]++; s.pos = q; return w;
}
static int KIDX[64][64];
static const int NCODE = 64 * 16 * 64 * 4 * 2;
static inline uint32_t code_of(const St& s, int d, int q) {
  bool fr = d > 0 && s.alt[q] == s.alt[s.pos];
  return ((((uint32_t)s.pos * 16 + KIDX[s.pos][q]) * 64 + (uint32_t)min(d, 63)) * 4 + min<int>(s.vis[q], 3)) * 2 + fr;
}
struct Pol { vector<float> w, e; Pol() : w(NCODE, 0), e(NCODE, 1) {} };
struct Res { double score = -1e18; ll Tsc = 0; bool fin = false; vector<Mv> seq; };
static double D0 = 1;
static double hd(const St& s) { int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)(s.alt[H8] - s.alt[p]) / SC; return sqrt(dx*dx+dy*dy+dz*dz) / D0; }
static ll ROLL = 0;
static Res playout(const St& root, const Pol& pol, std::mt19937& rng) {
  St s = root; Res r; static vector<Mv> mv; static vector<double> w; ll finT = -1; size_t finLen = 0; Mv finM{0, H8};
  while (true) {
    gen(s, true, mv); w.clear(); double z = 0; int nf = 0;
    for (auto& m : mv) { if (m.q == H8) { St c = s; apply(c, m.d, m.q); if (c.Tsc >= finT) { finT = c.Tsc; finLen = r.seq.size(); finM = m; } w.push_back(0); continue; }
      double e = pol.e[code_of(s, m.d, m.q)]; w.push_back(e); z += e; nf++; }
    if (!nf) break;
    double u = std::uniform_real_distribution<double>(0, z)(rng), acc = 0; int pick = -1;
    for (size_t i = 0; i < mv.size(); i++) { if (mv[i].q == H8) continue; acc += w[i]; pick = i; if (acc >= u) break; }
    r.seq.push_back(mv[pick]); apply(s, mv[pick].d, mv[pick].q);
  }
  ROLL++;
  if (finT >= 0) { r.seq.resize(finLen); r.seq.push_back(finM); r.fin = true; r.Tsc = finT; r.score = (double)finT / SC; }
  else { r.Tsc = s.Tsc; r.score = -1e7 + (double)s.Tsc / SC * exp(-hd(s)); }
  return r;
}
static void adapt(Pol& pol, const St& root, const vector<Mv>& seq) {
  static vector<pair<uint32_t, float>> upd; upd.clear(); static vector<Mv> mv; static vector<double> e;
  St s = root;
  for (const Mv& m : seq) { if (m.q == H8) break; gen(s, true, mv); e.clear(); double z = 0;
    for (auto& x : mv) { double v = x.q == H8 ? 0 : pol.e[code_of(s, x.d, x.q)]; e.push_back(v); z += v; }
    for (size_t i = 0; i < mv.size(); i++) if (mv[i].q != H8) upd.push_back({code_of(s, mv[i].d, mv[i].q), (float)(-e[i] / z)});
    upd.push_back({code_of(s, m.d, m.q), 1.f}); apply(s, m.d, m.q); }
  for (auto& [c, d] : upd) pol.w[c] += d; for (auto& [c, d] : upd) pol.e[c] = expf(pol.w[c]);
}

// epsilon test for free sinks (mirrors tools/sim.py necessity_check's t+1/997 probe): returns true if
// every free sink in seq, perturbed by an infinitesimal amount, breaks some later jump.
static ll NCHK = 0, NBAD = 0;
static bool free_ok(const St& root, const vector<Mv>& seq) {
  NCHK++;
  vector<St> states; states.reserve(seq.size() + 1); St s = root; states.push_back(s);
  for (auto& m : seq) { apply(s, m.d, m.q); states.push_back(s); }
  for (size_t i = 0; i < seq.size(); i++) {
    const St& si = states[i]; const Mv& m = seq[i];
    if (!(m.d > 0 && si.alt[m.q] == si.alt[si.pos])) continue;  // pinned waits break at once
    double c[64] = {0}; int p = si.pos, a = si.alt[p], O = 63 - p, n = gsize(si, p);
    for (int k = 0; k < 64; k++) if (si.alt[k] == a && k != O) c[k] -= 1.0 / n;
    if (si.alt[O] != a) c[O] += 1.0 / n;
    bool broken = false;
    for (size_t j = i + 1; j < seq.size() && !broken; j++) {
      const St& sj = states[j]; const Mv& mj = seq[j]; int pj = sj.pos, aj = sj.alt[pj], Oj = 63 - pj;
      if (mj.d > 0) {
        bool oin = sj.alt[Oj] == aj && fabs(c[Oj] - c[pj]) < 1e-9;
        for (int k = 0; k < 64; k++) if (sj.alt[k] == aj && k != Oj && fabs(c[k] - c[pj]) > 1e-9) { broken = true; break; }
        if (sj.alt[Oj] == aj && !oin) broken = true;  // membership differs from the unperturbed run
        if (broken) break;
        if (sj.alt[Oj] != aj) { /* opposite rises by the same amount in both runs */ }
      }
      if (fabs(c[mj.q] - c[pj]) > 1e-9) broken = true;
    }
    if (!broken) { NBAD++; return false; }
  }
  return true;
}
static double TLIM = 0; static std::chrono::steady_clock::time_point T0;
static double el() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count(); }
static Res nrpa(int lev, const St& root, Pol pol, std::mt19937& rng, const Res* init = nullptr) {
  if (lev == 0) return playout(root, pol, rng);
  Res best; if (init) best = *init;
  for (int i = 0; i < ITERS && el() < TLIM; i++) { Res r = lev == 1 ? playout(root, pol, rng) : nrpa(lev - 1, root, pol, rng);
    if (FREE && r.fin && r.score >= best.score && !free_ok(root, r.seq)) { r.fin = false; r.score = -1e6 + r.score; }
    if (r.score >= best.score) best = r; adapt(pol, root, best.seq); }
  return best;
}
static vector<Mv> read_tour(const char* fn) {  // integer tour (line 2 of a data/tours file)
  FILE* f = fopen(fn, "r"); static char line[200000]; vector<Mv> t; St s = start();
  while (fgets(line, sizeof line, f)) { if (!strchr(line, '(')) continue; char* p = line; int w; char sq[8];
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) { int q = (sq[0]-'a') + 8*(sq[1]-'1'); int n = gsize(s, s.pos); int d = w * SC / max(1, n);
      t.push_back({d, q}); apply(s, d, q); p++; } break; }
  fclose(f); return t;
}
static string text(const vector<Mv>& seq) {
  string o; St s = start(); char b[64];
  for (size_t i = 0; i < seq.size(); i++) { ll w = apply(s, seq[i].d, seq[i].q); ll g = std::gcd(w, (ll)SC); if (!g) g = 1; ll nu = w / g, de = SC / g;
    if (w == 0 || de == 1) snprintf(b, sizeof b, "%s(%lld, %s)", i ? ", " : "", nu, nm(seq[i].q).c_str());
    else snprintf(b, sizeof b, "%s(%lld/%lld, %s)", i ? ", " : "", nu, de, nm(seq[i].q).c_str()); o += b; }
  return o;
}
int main(int argc, char** argv) {
  init_board(); int WARM = 0, LEV = 3, AMIN = 35, AMAX = 60; unsigned RNG = 1; string seedF = "data/tours/record_8123.txt", out = "best.txt", pool; double PMIN = 8000;
  for (int i = 1; i < argc; i++) { string a = argv[i]; auto nx = [&]() { return string(argv[++i]); };
    if (a == "--sc") SC = stoi(nx()); else if (a == "--free") FREE = stoi(nx()); else if (a == "--fstep") FSTEP = stoi(nx()); else if (a == "--fmax") FMAX = stoi(nx());
    else if (a == "--warm") WARM = stoi(nx()); else if (a == "--level") LEV = stoi(nx()); else if (a == "--iters") ITERS = stoi(nx()); else if (a == "--time") TLIM = stod(nx());
    else if (a == "--seed") seedF = nx(); else if (a == "--amin") AMIN = stoi(nx()); else if (a == "--amax") AMAX = stoi(nx());
    else if (a == "--rng") RNG = stoul(nx()); else if (a == "--out") out = nx(); else if (a == "--pool") pool = nx(); else if (a == "--pool-min") PMIN = stod(nx());
    else { fprintf(stderr, "bad option %s\n", a.c_str()); return 1; } }
  for (int p = 0; p < 64; p++) for (int k = 0; k < NBN[p]; k++) KIDX[p][NBQ[p][k]] = k;
  { St s = start(); D0 = 1; D0 = hd(s); }
  vector<Mv> seed = read_tour(seedF.c_str()); { St s = start(); for (auto& m : seed) apply(s, m.d, m.q); printf("seed %s: %zu moves, %.3f minutes\n", seedF.c_str(), seed.size(), (double)s.Tsc / SC); }
  std::mt19937 rng(RNG); T0 = std::chrono::steady_clock::now(); Res gb; gb.score = -1e18;
  for (int rs = 0; el() < TLIM; rs++) {
    int k = std::uniform_int_distribution<int>(AMIN, AMAX)(rng);
    St root = start(); for (int i = 0; i < k; i++) apply(root, seed[i].d, seed[i].q);
    Pol pol; Res init; vector<Mv> rest(seed.begin() + k, seed.end());
    if (WARM) { for (int a = 0; a < WARM; a++) adapt(pol, root, rest); St s = root; for (auto& m : rest) apply(s, m.d, m.q); init.seq = rest; init.fin = true; init.Tsc = s.Tsc; init.score = (double)s.Tsc / SC; }
    Res r = nrpa(LEV, root, pol, rng, WARM ? &init : nullptr); r.seq.insert(r.seq.begin(), seed.begin(), seed.begin() + k);
    int nfree = 0; { St s = start(); for (auto& m : r.seq) { if (m.d > 0 && s.alt[m.q] == s.alt[s.pos]) nfree++; apply(s, m.d, m.q); } }
    printf("[%6.0fs] restart %d anchor %d: %s %.3f min, %zu jumps, %d free sinks (best %.3f, %lld rollouts, chk %lld bad %lld)\n", el(), rs, k, r.fin ? "fin" : "unfin", (double)r.Tsc / SC, r.seq.size(), nfree, gb.fin ? (double)gb.Tsc / SC : 0.0, ROLL, NCHK, NBAD);
    if (r.fin && !pool.empty() && (double)r.Tsc / SC >= PMIN) { FILE* f = fopen(pool.c_str(), "a"); fprintf(f, "%.6g\n%s\n", (double)r.Tsc / SC, text(r.seq).c_str()); fclose(f); }
    if (r.fin && r.score > gb.score) { gb = r; FILE* f = fopen(out.c_str(), "w"); fprintf(f, "%.6g\n%s\n", (double)r.Tsc / SC, text(r.seq).c_str()); fclose(f); printf("NEW BEST %.3f\n", (double)r.Tsc / SC); }
    fflush(stdout);
  }
}
