// Memetic / genetic algorithm over whole Knight Moves 5 tours (island model).
//
// Genome: the sequence of moves (target square q, dz), dz = alt[q] - alt[p] right after the jump. The wait
// before each jump is implied (pinned, as in src/eng.h): delta = dz - (alt[q] - alt[p]) must be >= 0, so a
// gene keeps its meaning ("sit and hop up 2 onto q") when it is replayed in a different context.
// Fitness: replay from a1; the tour ends at the first arrival at h8. Every state passed is checked for a
// finishing jump into h8, so a child that breaks down late still finishes at its best passed chance.
// Finished tours score their minutes; unfinished ones score -1e7 + T * exp(-dist to h8).
//
// Repair: when a gene is illegal, the candidates are (a) the same target with the other dz, (b) skipping
// up to --skip genes, (c) a bridge of up to --bridge free moves (DFS over pinned moves) after which gene
// i..i+skip is legal again. Each candidate is scored by how many of the following --look genes then
// replay legally (minus small penalties per bridge move / skipped gene); the best is applied. After
// --max-repairs repairs, or when no candidate exists, the rest is completed by NRPA level 1
// (--comp-iters iterations, rollouts biased toward the (from, to) pairs of the abandoned tail).
//
// Operators:
//  - crossover: one-point A[0..i) + B[j..) or two-point A[0..i1) + B[j1..j2) + A[i2..), cut where the knight
//    stands on the same square in both parents, chosen with probability ~ exp(-dist / --xtau), dist = number of
//    points whose altitude relative to the knight differs (+0.5 per differing visit count). Partner B comes
//    from the island population or (--parch) from the seed archive.
//  - mutation: delete / insert / replace a window of 1-3 moves, flip a dz, or regenerate the tail by NRPA-1.
//  - memetic step: first-improvement local search (delete 1-3, replace or insert every legal move at every
//    cut, followed by repair) on the island best and a tournament-chosen elite every --ls-every generations.
// Island model: --threads islands (one thread each), steady-state replacement (a child replaces its parent if
// better, else the most similar of --crowd random members if better; duplicates and more than --max-same members
// with the same minutes rejected), ring migration of the best every --mig-every generations, restart after --stall
// generations without an island improvement.
//
// Outputs: --out FILE (line 1 minutes, line 2 tour; rewritten on every new best), --pool FILE (finished tours
// >= --pool-min and within --pool-slack of the best, "minutes<TAB>tour", deduplicated), log on stdout.
// Stops on --time or SIGINT/SIGTERM.
//
// build: g++ -O3 -march=native -std=c++17 -pthread -o genetic genetic.cpp
#include "../../src/eng.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <dirent.h>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <sys/stat.h>

struct G { uint8_t q; int8_t dz; };
static inline bool operator==(const G& a, const G& b) { return a.q == b.q && a.dz == b.dz; }
static int TYP[64][64], KIDX[64][64];
static bool H8ADJ[64];

// ---------------------------------------------------------------- parameters
static int POP = 64, MINCUT = 1, BRIDGE = 2, SKIP = 3, LOOK = 6, MAXREP = 6, COMPIT = 30, LSEVERY = 5, LSEVALS = 3000;
static int LS2 = 20, MAXSAME = 2, CROWD = 6, MIGEVERY = 10, STALL = 3000, SEEDTOP = 300;
static double PX = 0.5, PARCH = 0.2, XTAU = 2.0, TAILBIAS = 2.0, CALPHA = 1.0;
static long long POOLMIN = 7500, POOLSLACK = 150;

static std::atomic<bool> STOP{false};
static std::atomic<long long> NCHILD{0}, NCOMP{0}, NREP{0}, NLS{0}, NROLL{0};
// operator statistics: produced, accepted into a population, better than the (first) parent, finished
static const char* OPN[8] = {"xover1", "xover2", "delete", "insert", "replace", "flipdz", "tail", "local"};
static std::atomic<long long> OPST[8][4];
static thread_local int LASTOP = 0;

// ---------------------------------------------------------------- move semantics
static inline int gene_delta(const S& s, int q, int dz) {
  int p = s.pos, t = TYP[p][q];
  if (t < 0) return -1;
  if (q != H8 && s.vis[q] >= 3) return -1;
  if (!legal_dz(t, dz)) return -1;
  int a = s.alt[p];
  if (s.alt[q] == a && q != 63 - p) return dz == 0 ? 0 : -1;  // same group: only a level knight move
  int d = dz - (s.alt[q] - a);
  return d >= 0 ? d : -1;
}
static inline int gdelta(const S& s, G m) { return gene_delta(s, m.q, m.dz); }
static inline G gene_of(const S& s, int d, int q) {  // gene of the pinned move (d, q) from s
  int p = s.pos, a = s.alt[p];
  if (s.alt[q] == a && q != 63 - p) return {(uint8_t)q, 0};
  return {(uint8_t)q, (int8_t)(s.alt[q] - (a - d))};
}
static inline int dzopts(int t, int* o) {
  if (t == 0) { o[0] = 0; return 1; }
  if (t == 1) { o[0] = 2; o[1] = -2; return 2; }
  o[0] = 1; o[1] = -1; return 2;
}
static double D0 = 1;
static double h8dist(const S& s) {
  int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  return sqrt(dx * dx + dy * dy + dz * dz) / D0;
}
static inline uint64_t hash_genes(const vector<G>& g) {
  uint64_t h = 1469598103934665603ULL;
  for (const G& m : g) { h ^= (uint64_t)m.q * 31 + (uint8_t)m.dz; h *= 1099511628211ULL; }
  return h ^ g.size();
}

// ---------------------------------------------------------------- individuals
struct Ind { vector<G> g; long long T = 0; bool fin = false; double score = -1e18; uint64_t h = 0; };

static void eval_ind(Ind& x) {  // replay from a1 (genes must be legal)
  S s = start_state();
  for (const G& m : x.g) { int d = gdelta(s, m); if (d < 0) { x.score = -1e18; x.fin = false; return; } apply_move(s, d, m.q); }
  x.T = s.T; x.fin = !x.g.empty() && x.g.back().q == H8;
  x.score = x.fin ? (double)s.T : -1e7 + s.T * exp(-h8dist(s));
  x.h = hash_genes(x.g);
}
static string tour_text(const vector<G>& g) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < g.size(); i++) {
    int d = gdelta(s, g[i]); int w = apply_move(s, d, g[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(g[i].q).c_str()); out += buf;
  }
  return out;
}

// parse every line of a file that holds "(wait, sq)" pairs; keep the finished, legal ones
static int parse_file(const string& fn, vector<Ind>& out) {
  std::ifstream f(fn); if (!f) { fprintf(stderr, "cannot open %s\n", fn.c_str()); return 0; }
  string line; int n = 0; vector<pair<int, int>> mv;
  while (std::getline(f, line)) {
    S s = start_state(); Ind x; size_t k = 0; bool ok = true, any = false;
    while ((k = line.find('(', k)) != string::npos) {
      int w; char sq[3];
      if (sscanf(line.c_str() + k, "(%d, %2s)", &w, sq) != 2) { k++; continue; }
      k++; any = true;
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1');
      if (q < 0 || q > 63) { ok = false; break; }
      int nG = group_n(s, s.pos), d = w / max(1, nG);
      if (d * max(1, nG) != w) { ok = false; break; }
      pinned_moves(s, 3, true, mv);
      bool found = false; for (auto& [dd, qq] : mv) if (dd == d && qq == q) found = true;
      if (!found) { ok = false; break; }
      G m = gene_of(s, d, q);
      if (gdelta(s, m) != d) { ok = false; break; }
      x.g.push_back(m); apply_move(s, d, q);
      if (q == H8) break;
    }
    if (!any || !ok || x.g.empty() || x.g.back().q != H8) continue;
    eval_ind(x); out.push_back(std::move(x)); n++;
  }
  return n;
}

// ---------------------------------------------------------------- per-thread context
static const int NCODE = 64 * 16 * 32 * 4;
static inline uint32_t code_of(const S& s, int delta, int q) {
  return (((uint32_t)s.pos * 16 + KIDX[s.pos][q]) * 32 + (uint32_t)min(delta, 31)) * 4 + (uint32_t)min<int>(s.vis[q], 3);
}
struct Ctx {
  std::mt19937_64 rng;
  vector<float> W; vector<uint32_t> touched; uint8_t tb[64 * 64];
  vector<pair<int, int>> mv; vector<double> wv;
  Ctx(uint64_t seed) : rng(seed), W(NCODE, 0.f) { memset(tb, 0, sizeof tb); }
  double u() { return std::uniform_real_distribution<double>(0, 1)(rng); }
  int ri(int lo, int hi) { return hi <= lo ? lo : std::uniform_int_distribution<int>(lo, hi)(rng); }  // inclusive
};

// ---------------------------------------------------------------- running a plan with repair
struct Run {
  S s; vector<G> mv; long long finT = -1; size_t finLen = 0; G finG{H8, 0}; bool done = false;
  Run() { s = start_state(); fin_check(); }
  void fin_check() {
    int p = s.pos; if (!H8ADJ[p]) return;
    int o[2], n = dzopts(TYP[p][H8], o);
    for (int k = 0; k < n; k++) {
      int d = gene_delta(s, H8, o[k]); if (d < 0) continue;
      long long T = s.T + (d ? (long long)d * group_n(s, p) : 0);
      if (T > finT) { finT = T; finLen = mv.size(); finG = {H8, (int8_t)o[k]}; }
    }
  }
  bool step(G m) {
    int d = gdelta(s, m); if (d < 0) return false;
    apply_move(s, d, m.q); mv.push_back(m);
    if (m.q == H8) done = true; else fin_check();
    return true;
  }
};

// how many of plan[j..j+LOOK) replay legally from s (a finish counts as all of them)
static int lookahead(const S& s0, const vector<G>& plan, size_t j) {
  S s = s0; int c = 0;
  for (size_t k = j; k < plan.size() && c < LOOK; k++) {
    int d = gdelta(s, plan[k]); if (d < 0) break;
    if (plan[k].q == H8) return LOOK;
    apply_move(s, d, plan[k].q); c++;
  }
  return c;
}

struct Repair { G br[5]; int nbr = 0; size_t next = 0; double score = -1e18; };

static void bridge_dfs(const S& s, const vector<G>& plan, size_t i, int depth, G* path, Repair& best, Ctx& c) {
  if (depth > 0) {
    for (int k = 0; k <= SKIP && i + k < plan.size(); k++) {
      int la = lookahead(s, plan, i + k); if (la < 1) continue;
      double sc = la - 0.7 * depth - 0.5 * k + 1e-6 * s.T + 1e-3 * c.u();
      if (sc > best.score) { best.score = sc; best.nbr = depth; for (int t = 0; t < depth; t++) best.br[t] = path[t]; best.next = i + k; }
    }
  }
  if (depth >= BRIDGE || depth >= 4) return;
  vector<pair<int, int>> mv; pinned_moves(s, 3, false, mv);
  for (auto& [d, q] : mv) {
    S n = s; G m = gene_of(s, d, q); apply_move(n, d, q); path[depth] = m;
    bridge_dfs(n, plan, i, depth + 1, path, best, c);
  }
}

static bool find_repair(const S& s, const vector<G>& plan, size_t i, Ctx& c, Repair& best) {
  best = Repair();
  G a = plan[i]; int t = TYP[s.pos][a.q];
  if (t >= 0) {  // same target, the other dz
    int o[2], n = dzopts(t, o);
    for (int k = 0; k < n; k++) {
      if (o[k] == a.dz) continue;
      G a2{a.q, (int8_t)o[k]}; int d = gdelta(s, a2); if (d < 0) continue;
      S s2 = s; apply_move(s2, d, a2.q);
      int la = a2.q == H8 ? LOOK : lookahead(s2, plan, i + 1);
      double sc = la + 0.7 - 0.3 + 1e-6 * s2.T + 1e-3 * c.u();  // counts the gene itself as one legal step
      if (sc > best.score) { best.score = sc; best.nbr = 1; best.br[0] = a2; best.next = i + 1; }
    }
  }
  for (int k = 1; k <= SKIP && i + k < plan.size(); k++) {  // skip k genes
    int la = lookahead(s, plan, i + k); if (la < 1) continue;
    double sc = la - 0.5 * k + 1e-6 * s.T + 1e-3 * c.u();
    if (sc > best.score) { best.score = sc; best.nbr = 0; best.next = i + k; }
  }
  G path[5]; bridge_dfs(s, plan, i, 0, path, best, c);
  return best.score > -1e17;
}

// run plan[i..] from r (with repairs); i is left at the first gene not executed
static void run_plan(Run& r, const vector<G>& plan, size_t& i, Ctx& c) {
  int rep = 0;
  while (i < plan.size() && !r.done) {
    if (r.step(plan[i])) { i++; continue; }
    if (rep >= MAXREP) return;
    rep++; NREP++;
    Repair rp; if (!find_repair(r.s, plan, i, c, rp)) return;
    for (int k = 0; k < rp.nbr; k++) if (!r.step(rp.br[k])) return;
    i = rp.next;
  }
}

// ---------------------------------------------------------------- NRPA level-1 completion
struct Roll { vector<G> seq; long long T = 0; bool fin = false; double score = -1e18; };
static inline double mw(Ctx& c, const S& s, int d, int q) {
  return exp((double)c.W[code_of(s, d, q)] + (c.tb[s.pos * 64 + q] ? TAILBIAS : 0.0));
}
static Roll rollout(const S& root, Ctx& c) {
  S s = root; Roll r; long long finT = -1; size_t finLen = 0; G finG{H8, 0};
  auto& mv = c.mv; auto& w = c.wv;
  for (int steps = 0; steps < 400; steps++) {
    pinned_moves(s, 3, true, mv);
    w.clear(); double z = 0; int nonfin = 0;
    for (auto& [d, q] : mv) {
      if (q == H8) {
        long long T = s.T + (d ? (long long)d * group_n(s, s.pos) : 0);
        if (T >= finT) { finT = T; finLen = r.seq.size(); finG = gene_of(s, d, q); }
        w.push_back(0); continue;
      }
      double e = mw(c, s, d, q); w.push_back(e); z += e; nonfin++;
    }
    if (!nonfin) break;
    double x = c.u() * z, acc = 0; int pick = -1;
    for (size_t k = 0; k < mv.size(); k++) { if (mv[k].second == H8) continue; acc += w[k]; pick = (int)k; if (acc >= x) break; }
    r.seq.push_back(gene_of(s, mv[pick].first, mv[pick].second));
    apply_move(s, mv[pick].first, mv[pick].second);
  }
  NROLL++;
  if (finT >= 0) { r.seq.resize(finLen); r.seq.push_back(finG); r.fin = true; r.T = finT; r.score = (double)finT; }
  else { r.T = s.T; r.score = -1e7 + s.T * exp(-h8dist(s)); }
  return r;
}
static void cadapt(Ctx& c, const S& root, const vector<G>& seq) {
  static thread_local vector<pair<uint32_t, float>> upd; upd.clear();
  static thread_local vector<pair<int, int>> mv; static thread_local vector<double> e;
  S s = root;
  for (const G& m : seq) {
    if (m.q == H8) break;
    int dm = gdelta(s, m); if (dm < 0) break;
    pinned_moves(s, 3, true, mv);
    e.clear(); double z = 0;
    for (auto& [d, q] : mv) { double x = q == H8 ? 0 : mw(c, s, d, q); e.push_back(x); z += x; }
    for (size_t k = 0; k < mv.size(); k++) if (mv[k].second != H8) upd.push_back({code_of(s, mv[k].first, mv[k].second), (float)(-CALPHA * e[k] / z)});
    upd.push_back({code_of(s, dm, m.q), (float)CALPHA});
    apply_move(s, dm, m.q);
  }
  for (auto& [k, d] : upd) { c.W[k] += d; c.touched.push_back(k); }
}
static Roll complete(const S& root, const vector<G>& plan, size_t from, Ctx& c) {
  NCOMP++;
  S s = root;  // tail bias: (from, to) pairs of the abandoned tail
  vector<int> setPairs;
  { int p = -1;  // positions of the abandoned tail are unknown after repair; use the parent's (q_k, q_{k+1}) pairs
    for (size_t k = from; k < plan.size(); k++) { int q = plan[k].q; if (p >= 0) { c.tb[p * 64 + q] = 1; setPairs.push_back(p * 64 + q); } p = q; }
    if (from < plan.size()) { c.tb[s.pos * 64 + plan[from].q] = 1; setPairs.push_back(s.pos * 64 + plan[from].q); } }
  Roll best;
  for (int it = 0; it < COMPIT && !STOP; it++) {
    Roll r = rollout(root, c);
    if (r.score >= best.score) best = std::move(r);
    cadapt(c, root, best.seq);
  }
  if (COMPIT <= 0) best = rollout(root, c);
  for (uint32_t k : c.touched) c.W[k] = 0.f;
  c.touched.clear();
  for (int k : setPairs) c.tb[k] = 0;
  return best;
}

// deeper, warm-started NRPA (level >= 1) on sparse policies, used by the "tail" mutation with probability --p-deep:
// the policy is first shifted toward the parent's own tail, which is also the starting best (so it can only improve)
typedef std::unordered_map<uint32_t, float> SP;
static void loadW(Ctx& c, const SP& P) { for (auto& [k, v] : P) { c.W[k] = v; c.touched.push_back(k); } }
static void clearW(Ctx& c) { for (uint32_t k : c.touched) c.W[k] = 0.f; c.touched.clear(); }
static void adaptP(Ctx& c, SP& P, const S& root, const vector<G>& seq) {
  loadW(c, P); size_t t0 = c.touched.size(); cadapt(c, root, seq);
  for (size_t k = t0; k < c.touched.size(); k++) P[c.touched[k]] = c.W[c.touched[k]];
  clearW(c);
}
static Roll nested(int level, const S& root, SP P, int iters, Ctx& c, const Roll* init) {
  Roll best; if (init) best = *init;
  for (int it = 0; it < iters && !STOP; it++) {
    Roll r;
    if (level <= 1) { loadW(c, P); r = rollout(root, c); clearW(c); }
    else r = nested(level - 1, root, P, iters, c, nullptr);
    if (r.score >= best.score) best = std::move(r);
    adaptP(c, P, root, best.seq);
  }
  return best;
}
static int DEEPLEVEL = 2, DEEPITERS = 30, DEEPADAPT = 3; static double PDEEP = 0.05;
static Roll deep_tail(const S& root, const vector<G>& g, size_t from, Ctx& c) {
  NCOMP++;
  Roll init; init.seq.assign(g.begin() + from, g.end());
  { S s = root; bool ok = true; for (auto& m : init.seq) { int d = gdelta(s, m); if (d < 0) { ok = false; break; } apply_move(s, d, m.q); }
    init.fin = ok && !init.seq.empty() && init.seq.back().q == H8; init.T = s.T; init.score = init.fin ? (double)s.T : -1e18; }
  SP P; if (init.fin) for (int k = 0; k < DEEPADAPT; k++) adaptP(c, P, root, init.seq);
  return nested(DEEPLEVEL, root, P, DEEPITERS, c, init.fin ? &init : nullptr);
}

// turn a run into an individual: the run itself, its best passed finish, or an NRPA-1 completion
static Ind finalize(Run& r, const vector<G>& plan, size_t i, bool doComplete, Ctx& c) {
  Ind x; NCHILD++;
  if (r.done) { x.g = r.mv; x.T = r.s.T; x.fin = true; x.score = (double)x.T; }
  if (r.finT > (x.fin ? x.T : -1)) {
    x.g.assign(r.mv.begin(), r.mv.begin() + r.finLen); x.g.push_back(r.finG); x.T = r.finT; x.fin = true; x.score = (double)x.T;
  }
  if (!r.done && doComplete) {
    Roll k = complete(r.s, plan, i, c);
    if (k.fin && k.T > (x.fin ? x.T : -1)) { x.g = r.mv; x.g.insert(x.g.end(), k.seq.begin(), k.seq.end()); x.T = k.T; x.fin = true; x.score = (double)x.T; }
  }
  if (!x.fin) { x.g = r.mv; x.T = r.s.T; x.score = -1e7 + r.s.T * exp(-h8dist(r.s)); }
  x.h = hash_genes(x.g);
  return x;
}
static Ind build(const vector<G>& plan, bool doComplete, Ctx& c) {
  Run r; size_t i = 0; run_plan(r, plan, i, c); return finalize(r, plan, i, doComplete, c);
}

// ---------------------------------------------------------------- crossover
struct Traj { vector<uint8_t> pos; vector<int8_t> rel; vector<uint8_t> vis; };
static void traj_of(const vector<G>& g, Traj& t) {
  size_t n = g.size(); t.pos.resize(n + 1); t.rel.resize((n + 1) * 64); t.vis.resize((n + 1) * 64);
  S s = start_state();
  for (size_t i = 0; i <= n; i++) {
    t.pos[i] = s.pos; int a = s.alt[s.pos];
    for (int k = 0; k < 64; k++) { int d = s.alt[k] - a; t.rel[i * 64 + k] = (int8_t)max(-127, min(127, d)); t.vis[i * 64 + k] = s.vis[k]; }
    if (i < n) apply_move(s, gdelta(s, g[i]), g[i].q);
  }
}
static inline double tdist(const Traj& A, size_t i, const Traj& B, size_t j) {
  const int8_t* ra = &A.rel[i * 64]; const int8_t* rb = &B.rel[j * 64];
  const uint8_t* va = &A.vis[i * 64]; const uint8_t* vb = &B.vis[j * 64];
  int d = 0, dv = 0;
  for (int k = 0; k < 64; k++) { d += ra[k] != rb[k]; dv += va[k] != vb[k]; }
  return d + 0.5 * dv;
}
struct Cut { uint16_t i, j; float w; };
static bool pick_cut(const vector<Cut>& cs, Ctx& c, Cut& out) {
  if (cs.empty()) return false;
  double z = 0; for (auto& x : cs) z += x.w;
  double u = c.u() * z, acc = 0;
  for (auto& x : cs) { acc += x.w; if (acc >= u) { out = x; return true; } }
  out = cs.back(); return true;
}
static Ind crossover(const Ind& A, const Ind& B, Ctx& c) {
  LASTOP = 0;
  static thread_local Traj TA, TB; static thread_local vector<Cut> cs;
  traj_of(A.g, TA); traj_of(B.g, TB);
  size_t la = A.g.size(), lb = B.g.size();
  static thread_local vector<int> bucket[64];
  for (int k = 0; k < 64; k++) bucket[k].clear();
  for (size_t j = 1; j < lb; j++) bucket[TB.pos[j]].push_back((int)j);
  cs.clear();
  for (size_t i = max<size_t>(MINCUT, 1); i < la; i++)
    for (int j : bucket[TA.pos[i]]) {
      double d = tdist(TA, i, TB, j);
      cs.push_back({(uint16_t)i, (uint16_t)j, (float)exp(-d / XTAU)});
    }
  Cut c1; if (!pick_cut(cs, c, c1)) return A;
  vector<G> plan(A.g.begin(), A.g.begin() + c1.i);
  if (c.u() < 0.4) {  // two-point: B[j1..j2) replaces A[i1..i2)
    static thread_local vector<Cut> c2s; c2s.clear();
    for (auto& x : cs) if (x.i > c1.i && x.j > c1.j && x.j - c1.j <= 40 && x.i - c1.i <= 40) c2s.push_back(x);
    Cut c2;
    if (pick_cut(c2s, c, c2)) {
      plan.insert(plan.end(), B.g.begin() + c1.j, B.g.begin() + c2.j);
      plan.insert(plan.end(), A.g.begin() + c2.i, A.g.end());
      LASTOP = 1; return build(plan, COMPIT > 0, c);
    }
  }
  plan.insert(plan.end(), B.g.begin() + c1.j, B.g.end());
  LASTOP = 0; return build(plan, COMPIT > 0, c);
}

// ---------------------------------------------------------------- mutation
static bool random_move(Run& r, Ctx& c) {
  pinned_moves(r.s, 3, false, c.mv);
  if (c.mv.empty()) return false;
  auto [d, q] = c.mv[c.ri(0, (int)c.mv.size() - 1)];
  return r.step(gene_of(r.s, d, q));
}
static Ind mutate(const Ind& P, Ctx& c) {
  int n = (int)P.g.size(); if (n < 2) return P;
  int i = c.ri(min(MINCUT, n - 1), n - 1);
  Run r; for (int k = 0; k < i; k++) r.step(P.g[k]);
  double u = c.u(); size_t nx = i;
  if (u < 0.25) { LASTOP = 2; nx = i + c.ri(1, 3); }                                           // delete a window
  else if (u < 0.45) { LASTOP = 3; int L = c.ri(1, 3); for (int k = 0; k < L; k++) random_move(r, c); }   // insert
  else if (u < 0.70) { LASTOP = 4; nx = i + c.ri(1, 3); int L = c.ri(1, 3); for (int k = 0; k < L; k++) random_move(r, c); }  // replace
  else if (u < 0.80) {                                                          // flip dz
    LASTOP = 5;
    G m = P.g[i]; int t = TYP[r.s.pos][m.q];
    if (t > 0) { m.dz = -m.dz; if (r.step(m)) nx = i + 1; }
  } else {                                                                      // regenerate the tail
    LASTOP = 6;
    if (PDEEP > 0 && c.u() < PDEEP) {
      Roll k = deep_tail(r.s, P.g, i, c);
      Ind x; x.g = r.mv; x.g.insert(x.g.end(), k.seq.begin(), k.seq.end()); eval_ind(x); NCHILD++; return x;
    }
    if (COMPIT > 0) return finalize(r, P.g, i, true, c);
    nx = i + 1; random_move(r, c);
  }
  nx = min<size_t>(nx, P.g.size());
  run_plan(r, P.g, nx, c);
  return finalize(r, P.g, nx, COMPIT > 0, c);
}

// ---------------------------------------------------------------- memetic local search (first improvement)
static Ind local_search(Ind cur, int budget, Ctx& c) {
  NLS++;
  vector<pair<int, int>> mv;
  bool improved = true;
  while (improved && budget > 0 && !STOP) {
    improved = false;
    int n = (int)cur.g.size();
    vector<int> order; for (int i = min(MINCUT, n - 1); i < n; i++) order.push_back(i);
    std::shuffle(order.begin(), order.end(), c.rng);
    for (int i : order) {
      if (budget <= 0 || STOP) break;
      Run base; for (int k = 0; k < i; k++) base.step(cur.g[k]);
      auto tryit = [&](const vector<G>& pre, size_t nx) {
        budget--;
        Run r = base; for (const G& m : pre) if (!r.step(m)) return false;
        run_plan(r, cur.g, nx, c);
        Ind x = finalize(r, cur.g, nx, false, c);
        if (x.score > cur.score + 1e-9) { cur = std::move(x); return true; }
        return false;
      };
      for (int L = 1; L <= 3 && !improved && i + L <= n; L++) improved = tryit({}, i + L);
      if (improved) break;
      pinned_moves(base.s, 3, false, mv);
      auto mvs = mv;
      for (auto& [d, q] : mvs) {
        G m = gene_of(base.s, d, q);
        if (m == cur.g[i]) continue;
        if (tryit({m}, i + 1) || tryit({m}, i) || (i + 2 <= n && tryit({m}, i + 2))) { improved = true; break; }
        if (budget <= 0) break;
      }
      if (improved) break;
      for (int t = 0; t < LS2 && !mvs.empty() && budget > 0; t++) {  // random two-move replacements
        auto [d1, q1] = mvs[c.ri(0, (int)mvs.size() - 1)];
        G m1 = gene_of(base.s, d1, q1); S s1 = base.s; apply_move(s1, d1, q1);
        pinned_moves(s1, 3, false, mv); if (mv.empty()) continue;
        auto [d2, q2] = mv[c.ri(0, (int)mv.size() - 1)];
        G m2 = gene_of(s1, d2, q2);
        int L = c.ri(1, 3); if (i + L > n) continue;
        if (tryit({m1, m2}, i + L)) { improved = true; break; }
      }
      if (improved) break;
    }
  }
  return cur;
}

// ---------------------------------------------------------------- global state
static std::mutex GM;
static Ind GBEST; static string OUTFILE = "ga_best.txt", POOLFILE;
static std::unordered_set<uint64_t> POOLH; static long long NPOOL = 0;
static vector<Ind> ARCH;  // seed archive (read-only after start)
static auto T0 = std::chrono::steady_clock::now();
static double elapsed() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count(); }

static void report(const Ind& x, int island) {
  if (!x.fin) return;
  std::lock_guard<std::mutex> lk(GM);
  if (!POOLFILE.empty() && x.T >= POOLMIN && x.T >= GBEST.T - POOLSLACK && POOLH.insert(x.h).second) {
    FILE* f = fopen(POOLFILE.c_str(), "a"); if (f) { fprintf(f, "%lld\t%s\n", x.T, tour_text(x.g).c_str()); fclose(f); NPOOL++; }
  }
  if (!GBEST.fin || x.T > GBEST.T) {
    GBEST = x;
    string tmp = OUTFILE + ".tmp"; FILE* f = fopen(tmp.c_str(), "w");
    if (f) { fprintf(f, "%lld\n%s\n", x.T, tour_text(x.g).c_str()); fclose(f); rename(tmp.c_str(), OUTFILE.c_str()); }
    printf("[%7.1fs] NEW BEST %lld minutes (%zu moves, island %d) -> %s\n", elapsed(), x.T, x.g.size(), island, OUTFILE.c_str());
    fflush(stdout);
  }
}

// ---------------------------------------------------------------- islands
struct Island {
  int id; vector<Ind> pop; std::unordered_set<uint64_t> hs; std::mutex inm; vector<Ind> inbox;
  long long bestT = -1; std::atomic<long long> medT{0}, ndist{0}; int stall = 0; long long gens = 0;
  std::unordered_set<uint64_t> lsDone;
  int best_idx() const { int b = 0; for (int k = 1; k < (int)pop.size(); k++) if (pop[k].score > pop[b].score) b = k; return b; }
  int tour(Ctx& c, int k = 3) const { int b = c.ri(0, (int)pop.size() - 1); for (int t = 1; t < k; t++) { int x = c.ri(0, (int)pop.size() - 1); if (pop[x].score > pop[b].score) b = x; } return b; }
  // replacement: a child never enters if its genome is already present, or if MAXSAME members already have
  // its exact minutes (neutral clones of one tour would otherwise take over). Otherwise it replaces, among
  // --crowd random members (never the best), the one sharing the longest common prefix with it (crowding;
  // ties -> the worse one), if the child scores higher.
  // par >= 0: deterministic crowding first (the child replaces its parent if it beats it)
  bool insert(Ind&& x, Ctx& c, int par = -1) {
    if (x.g.empty() || hs.count(x.h)) return false;
    int same = 0; for (auto& y : pop) same += y.fin == x.fin && y.T == x.T;
    if (par >= 0 && par < (int)pop.size() && x.score > pop[par].score) {
      if (same >= MAXSAME) return false;
      hs.erase(pop[par].h); hs.insert(x.h); report(x, id); pop[par] = std::move(x); return true;
    }
    if ((int)pop.size() < POP) { if (same >= MAXSAME) return false; hs.insert(x.h); report(x, id); pop.push_back(std::move(x)); return true; }
    int b = best_idx(), v = -1, vl = -1;
    for (int t = 0; t < CROWD; t++) {
      int k = c.ri(0, (int)pop.size() - 1); if (k == b) continue;
      const auto& g = pop[k].g; size_t l = 0, n = min(g.size(), x.g.size()); while (l < n && g[l] == x.g[l]) l++;
      if ((int)l > vl || ((int)l == vl && pop[k].score < pop[v].score)) { v = k; vl = (int)l; }
    }
    if (v < 0 || !(x.score > pop[v].score)) return false;
    if (same >= MAXSAME && !(pop[v].fin == x.fin && pop[v].T == x.T)) return false;
    hs.erase(pop[v].h); hs.insert(x.h); report(x, id); pop[v] = std::move(x); return true;
  }
  void seed(Ctx& c) {
    pop.clear(); hs.clear(); lsDone.clear();
    int na = (int)ARCH.size();
    { Ind g; { std::lock_guard<std::mutex> lk(GM); g = GBEST; } if (g.fin) insert(std::move(g), c); }
    for (int k = 0; k < min(3, na); k++) { Ind x = ARCH[k]; insert(std::move(x), c); }
    for (int t = 0; t < 4 * POP && (int)pop.size() < POP / 2 && na; t++) {  // rank-biased picks from the archive
      int k = (int)(na * pow(c.u(), 2.0)); Ind x = ARCH[min(k, na - 1)]; insert(std::move(x), c);
    }
    for (int t = 0; t < 8 * POP && (int)pop.size() < POP && !STOP && !pop.empty(); t++) {
      Ind x = mutate(pop[c.ri(0, (int)pop.size() - 1)], c); insert(std::move(x), c);
    }
    bestT = -1; stall = 0;
    for (auto& x : pop) if (x.fin) bestT = max(bestT, x.T);
  }
};
static vector<Island*> ISL;

static void island_loop(int id, uint64_t seed) {
  Island& I = *ISL[id]; Ctx c(seed);
  I.seed(c);
  while (!STOP) {
    for (int k = 0; k < POP / 2 && !STOP; k++) {
      Ind child; double ps = 0; int par = -1;
      if (c.u() < PX && I.pop.size() >= 2) {
        int a = I.tour(c);
        const Ind* B;
        if (c.u() < PARCH && !ARCH.empty()) B = &ARCH[c.ri(0, (int)ARCH.size() - 1)];
        else { int b = I.tour(c); for (int t = 0; t < 4 && b == a; t++) b = I.tour(c); B = &I.pop[b]; }
        child = crossover(I.pop[a], *B, c); ps = I.pop[a].score; par = a;
        if (c.u() < 0.2) { int op = LASTOP; child = mutate(child, c); LASTOP = op; }
      } else { int a = I.tour(c); child = mutate(I.pop[a], c); ps = I.pop[a].score; par = a; }
      int op = LASTOP; bool better = child.score > ps, fin = child.fin;
      bool acc = I.insert(std::move(child), c, par);
      OPST[op][0]++; OPST[op][1] += acc; OPST[op][2] += better; OPST[op][3] += fin;
    }
    I.gens++;
    { std::lock_guard<std::mutex> lk(I.inm); for (auto& x : I.inbox) I.insert(std::move(x), c); I.inbox.clear(); }
    if (LSEVERY > 0 && I.gens % LSEVERY == 0) {  // memetic step on the best and one elite
      for (int rep = 0; rep < 2 && !STOP; rep++) {
        int k = rep == 0 ? I.best_idx() : I.tour(c, 4);
        if (I.lsDone.count(I.pop[k].h)) continue;
        I.lsDone.insert(I.pop[k].h);
        Ind y = local_search(I.pop[k], LSEVALS, c);
        OPST[7][0]++;
        if (y.h != I.pop[k].h) { OPST[7][2]++; I.lsDone.insert(y.h); OPST[7][1] += I.insert(std::move(y), c, k); }
      }
    }
    if (MIGEVERY > 0 && I.gens % MIGEVERY == 0 && ISL.size() > 1) {
      Island& J = *ISL[(id + 1) % ISL.size()];
      Ind x = I.pop[I.best_idx()], y = I.pop[I.tour(c)];
      std::lock_guard<std::mutex> lk(J.inm); J.inbox.push_back(std::move(x)); J.inbox.push_back(std::move(y));
    }
    long long bt = -1; for (auto& x : I.pop) if (x.fin) bt = max(bt, x.T);
    { vector<long long> v; for (auto& x : I.pop) v.push_back(x.fin ? x.T : 0); nth_element(v.begin(), v.begin() + v.size() / 2, v.end()); I.medT = v[v.size() / 2];
      sort(v.begin(), v.end()); I.ndist = unique(v.begin(), v.end()) - v.begin(); }
    if (bt > I.bestT) { I.bestT = bt; I.stall = 0; } else if (++I.stall >= STALL) {
      printf("[%7.1fs] island %d restarts (best %lld, %lld generations)\n", elapsed(), id, I.bestT, I.gens); fflush(stdout);
      I.seed(c);
    }
  }
}

static void on_signal(int) { STOP = true; }

int main(int argc, char** argv) {
  init_board(); D0 = 1; D0 = h8dist(start_state());
  for (int p = 0; p < 64; p++) { for (int q = 0; q < 64; q++) { TYP[p][q] = -1; KIDX[p][q] = 0; } for (int k = 0; k < NBN[p]; k++) { TYP[p][NBQ[p][k]] = NBT[p][k]; KIDX[p][NBQ[p][k]] = k; } }
  for (int p = 0; p < 64; p++) H8ADJ[p] = TYP[p][H8] >= 0;
  int THREADS = 1; double TIME = 60; uint64_t RNG = 1; vector<string> seedFiles; bool selfcheck = false; string checker = "tools/check_tour.py";
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--threads") THREADS = stoi(nx()); else if (a == "--time") TIME = stod(nx()); else if (a == "--rng") RNG = stoull(nx());
    else if (a == "--seeds") { while (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0) seedFiles.push_back(argv[++i]); }
    else if (a == "--seed-top") SEEDTOP = stoi(nx());
    else if (a == "--pop") POP = stoi(nx()); else if (a == "--min-cut") MINCUT = stoi(nx());
    else if (a == "--px") PX = stod(nx()); else if (a == "--parch") PARCH = stod(nx()); else if (a == "--xtau") XTAU = stod(nx());
    else if (a == "--bridge") BRIDGE = min(4, stoi(nx())); else if (a == "--skip") SKIP = stoi(nx()); else if (a == "--look") LOOK = stoi(nx());
    else if (a == "--max-repairs") MAXREP = stoi(nx());
    else if (a == "--comp-iters") COMPIT = stoi(nx()); else if (a == "--tail-bias") TAILBIAS = stod(nx()); else if (a == "--comp-alpha") CALPHA = stod(nx());
    else if (a == "--ls-every") LSEVERY = stoi(nx()); else if (a == "--ls-evals") LSEVALS = stoi(nx()); else if (a == "--ls2") LS2 = stoi(nx());
    else if (a == "--p-deep") PDEEP = stod(nx()); else if (a == "--deep-level") DEEPLEVEL = stoi(nx());
    else if (a == "--deep-iters") DEEPITERS = stoi(nx()); else if (a == "--deep-adapt") DEEPADAPT = stoi(nx());
    else if (a == "--max-same") MAXSAME = stoi(nx()); else if (a == "--crowd") CROWD = stoi(nx());
    else if (a == "--mig-every") MIGEVERY = stoi(nx()); else if (a == "--stall") STALL = stoi(nx());
    else if (a == "--out") OUTFILE = nx(); else if (a == "--pool") POOLFILE = nx();
    else if (a == "--pool-min") POOLMIN = stoll(nx()); else if (a == "--pool-slack") POOLSLACK = stoll(nx());
    else if (a == "--checker") checker = nx();
    else if (a == "--selfcheck") selfcheck = true;
    else { fprintf(stderr, "unknown option %s (see the comment at the top of genetic.cpp)\n", a.c_str()); return 1; }
  }
  if (seedFiles.empty()) {  // default: every .txt file in data/tours/
    DIR* d = opendir("data/tours");
    if (d) { for (dirent* e; (e = readdir(d));) { string n = e->d_name; if (n.size() > 4 && n.substr(n.size() - 4) == ".txt") seedFiles.push_back("data/tours/" + n); } closedir(d); }
    sort(seedFiles.begin(), seedFiles.end());
  }
  vector<Ind> all;
  for (auto& f : seedFiles) {
    size_t before = all.size(); int n = parse_file(f, all);
    long long b = 0; for (size_t k = before; k < all.size(); k++) b = max(b, all[k].T);
    printf("seeds: %s: %d finished tours, best %lld\n", f.c_str(), n, b);
  }
  sort(all.begin(), all.end(), [](const Ind& x, const Ind& y) { return x.score > y.score; });
  { std::unordered_set<uint64_t> seen; for (auto& x : all) if (seen.insert(x.h).second && (int)ARCH.size() < SEEDTOP) ARCH.push_back(x); }
  printf("archive: %zu distinct seed tours (best %lld, worst %lld)\n", ARCH.size(), ARCH.empty() ? 0 : ARCH[0].T, ARCH.empty() ? 0 : ARCH.back().T);
  if (selfcheck) {  // replay check: re-parse our own output text and compare
    for (size_t k = 0; k < min<size_t>(ARCH.size(), 5); k++) {
      Ind x = ARCH[k]; string t = tour_text(x.g); S s = start_state(); long long T = 0;
      for (auto& m : x.g) T += apply_move(s, gdelta(s, m), m.q);
      printf("selfcheck: seed %zu: %lld minutes, %zu moves, replay %lld\n", k, x.T, x.g.size(), T);
    }
    Ctx c(RNG); auto t0 = std::chrono::steady_clock::now(); long long tot = 0; int N = 2000;
    for (int k = 0; k < N; k++) { Ind y = build(ARCH[0].g, false, c); tot += y.T; }
    double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("selfcheck: %d plain replays+finalize of the best seed in %.3fs (%.0f/s), mean %lld\n", N, dt, N / dt, tot / N);
    return 0;
  }
  if (ARCH.empty()) { fprintf(stderr, "no finished seed tours\n"); return 1; }
  std::signal(SIGINT, on_signal); std::signal(SIGTERM, on_signal);
  printf("GA: %d islands x pop %d, px %.2f, parch %.2f, xtau %.1f, bridge %d, skip %d, look %d, max-repairs %d, comp-iters %d, tail-bias %.1f, ls every %d (%d evals), mig every %d, stall %d, min-cut %d, time %.0fs\n",
         THREADS, POP, PX, PARCH, XTAU, BRIDGE, SKIP, LOOK, MAXREP, COMPIT, TAILBIAS, LSEVERY, LSEVALS, MIGEVERY, STALL, MINCUT, TIME);
  fflush(stdout);
  T0 = std::chrono::steady_clock::now();
  for (int t = 0; t < THREADS; t++) { ISL.push_back(new Island()); ISL.back()->id = t; }
  vector<std::thread> th; for (int t = 0; t < THREADS; t++) th.emplace_back(island_loop, t, RNG * 1000003ULL + t * 7919ULL + 17);
  double lastLog = 0;
  while (!STOP && (TIME <= 0 || elapsed() < TIME)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (elapsed() - lastLog >= 10) {
      lastLog = elapsed();
      std::lock_guard<std::mutex> lk(GM);
      printf("[%7.1fs] best %lld | children %lld (%.0f/s), completions %lld, repairs %lld, rollouts %lld, local searches %lld, pool %lld | islands:",
             elapsed(), GBEST.fin ? GBEST.T : 0LL, (long long)NCHILD, NCHILD / elapsed(), (long long)NCOMP, (long long)NREP, (long long)NROLL, (long long)NLS, NPOOL);
      for (auto* I : ISL) printf(" %lld(med %lld, %lld distinct)", I->bestT, (long long)I->medT, (long long)I->ndist);
      printf("\n          ops (made/accepted/better/finished):");
      for (int k = 0; k < 8; k++) printf(" %s %lld/%lld/%lld/%lld", OPN[k], (long long)OPST[k][0], (long long)OPST[k][1], (long long)OPST[k][2], (long long)OPST[k][3]);
      printf("\n"); fflush(stdout);
    }
  }
  STOP = true;
  for (auto& t : th) t.join();
  printf("done: best %lld minutes after %.0fs, %lld children\n", GBEST.fin ? GBEST.T : 0LL, elapsed(), (long long)NCHILD);
  struct stat st;
  if (GBEST.fin && stat(checker.c_str(), &st) == 0) {
    string cmd = "python3 " + checker + " \"$(sed -n 2p " + OUTFILE + ")\"";
    printf("checking: %s\n", cmd.c_str()); fflush(stdout);
    int rc = system(cmd.c_str()); (void)rc;
  }
  for (auto* I : ISL) delete I;
}
