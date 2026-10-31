// Nested Rollout Policy Adaptation (Rosin 2011) for the swamp-knight puzzle.
//
// A rollout plays a tour to the end, choosing among the legal (pinned) moves with probability
// proportional to exp(weight[code] + bias); codes identify a move by (from, to, sink, visits of the
// target). Level L runs `iters` searches at level L-1, keeping the best sequence and shifting the
// weights toward it after each one. Puzzle-specific details:
//  - a rollout keeps going past chances to finish, but remembers them: the tour it returns ends at the
//    last point where h8 was reachable (so a passed finish is never wasted)
//  - finished tours score their minutes; unfinished ones score below every finished tour, ranked by
//    minutes x exp(-d) (d = 3D distance to h8, 1 at the start), which pulls early searches toward h8
//  - every restart can seed from a known tour (--seed-tours) or the best so far (--seed-best): the weights
//    are shifted toward it (--seed-adapt times) and the top-level search starts with it as its best, so
//    it can only improve on it
//  - --anchor-min/--anchor-max: each restart keeps the first k moves of its seed fixed (k drawn from that
//    range) and searches only the rest of the tour. This focuses the search on one stretch of the tour
//    (large-neighbourhood search); 0 (the default) searches whole tours from a1
//  - --pool FILE [--pool-min M]: append every finished result of at least M minutes to FILE
//  - --seed-alpha A: learning rate for the --seed-adapt steps (default: --alpha)
//  - --seed-top K: use only the K best tours of --seed-tours
//  - --no-warm: don't start the search from the seed's own continuation (only the anchor is kept)
//  - --watch: re-read --seed-tours whenever the file changes and adopt a better tour from it as the best
//    so far (lets several processes share improvements through one file)
//  - --objective opening [--dmin 56] [--max-steps 60]: optimise openings instead of tours: a rollout
//    scores the best dance start it passes (see dance_pot) and the output is that unfinished prefix
//  - --prefix-file FILE: every restart keeps FILE's (possibly unfinished) moves and searches the rest
//    from fresh weights (for finishing the openings found with --objective opening)
//  - --bias B adds B x (minutes the move waits) / 100 to every move's weight (Gibbs-NRPA prior)
// Independent searches run on --threads threads; improvements go to --out (line 1 minutes, line 2 tour).
//
// usage: nrpa [--level 3] [--iters 100] [--alpha 1] [--threads N] [--time SECONDS] [--rng SEED]
//             [--seed-tours FILE] [--seed-best P] [--seed-adapt K] [--anchor-min A] [--anchor-max B]
//             [--bias B] [--watch] [--no-warm] [--pool FILE] [--pool-min M] [--seed-top K] [--seed-alpha A]
//             [--objective tour|opening] [--dmin D] [--max-steps N] [--prefix-file FILE] [--out FILE]
#include "eng.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <sys/stat.h>

struct Mv { int delta, q; };

// dense policy: (from, neighbour index, sink capped at 31, visits of target capped at 3)
static int KIDX[64][64];
static const int NCODE = 64 * 16 * 32 * 4;
struct Policy {
  vector<float> w, e;  // weights and their exponentials (kept in sync by adapt)
  Policy() : w(NCODE, 0.f), e(NCODE, 1.f) {}
};
static inline uint32_t code_of(const S& s, int delta, int q) {
  return (((uint32_t)s.pos * 16 + KIDX[s.pos][q]) * 32 + (uint32_t)min(delta, 31)) * 4 + (uint32_t)min<int>(s.vis[q], 3);
}
static double D0 = 1, BIAS = 0;
static double h8dist(const S& s) {
  int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  return sqrt(dx * dx + dy * dy + dz * dz) / D0;
}
// waited minutes for a move (the group size times the sink)
static inline int move_minutes(const S& s, int delta) { return delta ? delta * group_n(s, s.pos) : 0; }
// exp(weight + bias) of a move
static inline double move_e(const Policy& pol, const S& s, int d, int q) {
  double x = pol.e[code_of(s, d, q)];
  if (BIAS != 0 && d) x *= exp(BIAS * move_minutes(s, d) / 100.0);
  return x;
}

struct Result { double score = -1e18; long long minutes = 0; bool finished = false; vector<Mv> seq; };

// --objective opening: score a rollout by the best "dance start" it passes through instead of by finishing.
// A dance start has the knight in the upper of two groups two apart (together >= DMIN points, h8 among
// them). Its potential is T plus the minutes of an idealised dance from there (path constraints ignored).
static int OBJ = 0, DMIN = 56, MAXSTEPS = 60;
static double dance_pot(const S& s) {
  int a = s.alt[s.pos], nG[2] = {0, 0};
  for (int i = 0; i < 64; i++) { if (s.alt[i] == a) nG[0]++; else if (s.alt[i] == a - 2) nG[1]++; }
  if (nG[1] == 0) return -1e18;
  int ah = s.alt[H8]; if (ah != a && ah != a - 2) return -1e18;
  if (nG[0] + nG[1] < DMIN) return -1e18;
  // play an idealised dance: sits alternate between the groups (upper first, at the knight's point),
  // squares whose partner is gone sit first, and a new pair is opened (its partner leaves) only when needed
  int grp[64], cap[64]; bool alive[64], open_[64];
  for (int i = 0; i < 64; i++) {
    alive[i] = s.alt[i] == a || s.alt[i] == a - 2; grp[i] = s.alt[i] == a ? 0 : 1;
    cap[i] = alive[i] ? 3 - s.vis[i] + (i == s.pos) : 0;
  }
  cap[H8] = cap[A1] = 0;  // h8 is the goal; sitting on a1 would push h8 out
  for (int i = 0; i < 64; i++) open_[i] = alive[i] && !alive[63 - i];
  double fut = 0; int g = 0;
  for (int step = 0; step < 200; step++) {
    int best = -1, bc = 0;
    if (step == 0) best = s.pos;
    else {
      for (int i = 0; i < 64; i++) if (alive[i] && grp[i] == g && open_[i] && cap[i] > bc) { best = i; bc = cap[i]; }
      if (best < 0) for (int i = 0; i < 64; i++) if (alive[i] && grp[i] == g && cap[i] > bc) { best = i; bc = cap[i]; }
    }
    if (best < 0 || cap[best] <= 0) break;
    int n = nG[g], p = 63 - best;
    if (!open_[best]) {
      if (alive[p]) { if (grp[p] == g) n--; nG[grp[p]]--; alive[p] = false; cap[p] = 0; }
      open_[best] = true;
    }
    fut += 4.0 * n; cap[best]--; g ^= 1;
  }
  return s.T + fut;
}

static std::atomic<bool> STOP{false};
static std::atomic<long long> PLAYOUTS{0};

// one rollout from the root state
static Result playout(const S& root, const Policy& pol, std::mt19937& rng) {
  S s = root; Result r; vector<pair<int, int>> mv; vector<double> w;
  long long finT = -1; size_t finLen = 0; Mv finMove{0, H8};
  double bestPot = -1e18; size_t potLen = 0; long long potT = 0; int qual = 0;
  if (OBJ == 1) { double p = dance_pot(s); if (p > bestPot) { bestPot = p; potLen = 0; potT = s.T; } }
  while (true) {
    if (OBJ == 1 && (int)r.seq.size() >= MAXSTEPS) break;
    pinned_moves(s, 3, OBJ == 0, mv);
    w.clear(); double z = 0; int nonfin = 0;
    for (auto& [d, q] : mv) {
      if (q == H8) {  // a chance to finish: remember the best one
        S c = s; apply_move(c, d, q);
        if (c.T >= finT) { finT = c.T; finLen = r.seq.size(); finMove = {d, q}; }
        w.push_back(0); continue;
      }
      double e = move_e(pol, s, d, q); w.push_back(e); z += e; nonfin++;
    }
    if (nonfin == 0) break;
    double u = std::uniform_real_distribution<double>(0, z)(rng), acc = 0; int pick = -1;
    for (size_t i = 0; i < mv.size(); i++) { if (mv[i].second == H8) continue; acc += w[i]; pick = (int)i; if (acc >= u) break; }
    r.seq.push_back({mv[pick].first, mv[pick].second});
    apply_move(s, mv[pick].first, mv[pick].second);
    if (OBJ == 1) {  // the dance has begun once states qualify; a few of them are enough
      double p = dance_pot(s);
      if (p > -1e17) { if (p > bestPot) { bestPot = p; potLen = r.seq.size(); potT = s.T; } if (++qual >= 6) break; }
    }
  }
  PLAYOUTS++;
  if (OBJ == 1) {
    r.seq.resize(potLen); r.minutes = potT; r.score = bestPot > -1e17 ? bestPot : -1e7 + s.T; r.finished = false;
    return r;
  }
  if (finT >= 0) {
    r.seq.resize(finLen); r.seq.push_back(finMove);
    r.finished = true; r.minutes = finT; r.score = (double)finT;
  } else {
    r.minutes = s.T; r.score = -1e7 + s.T * exp(-h8dist(s));
  }
  return r;
}

// shift the weights toward seq (steps before the final move into h8); all probabilities use the
// weights from before the update, as in the original algorithm
static void adapt(Policy& pol, const S& root, const vector<Mv>& seq, double alpha) {
  static thread_local vector<pair<uint32_t, float>> upd; upd.clear();
  static thread_local vector<pair<int, int>> mv;
  static thread_local vector<double> e;
  S s = root;
  for (const Mv& m : seq) {
    if (m.q == H8) break;
    pinned_moves(s, 3, true, mv);
    e.clear(); double z = 0;
    for (auto& [d, q] : mv) { double x = q == H8 ? 0 : move_e(pol, s, d, q); e.push_back(x); z += x; }
    for (size_t i = 0; i < mv.size(); i++) if (mv[i].second != H8) upd.push_back({code_of(s, mv[i].first, mv[i].second), (float)(-alpha * e[i] / z)});
    upd.push_back({code_of(s, m.delta, m.q), (float)alpha});
    apply_move(s, m.delta, m.q);
  }
  for (auto& [c, d] : upd) pol.w[c] += d;
  for (auto& [c, d] : upd) pol.e[c] = expf(pol.w[c]);
}

static int ITERS = 100; static double ALPHA = 1.0;
// warm start: `init` (if given) is the best sequence to begin from, so the search can only improve on it
static Result nrpa(int level, const S& root, Policy pol, std::mt19937& rng, const Result* init = nullptr) {
  if (level == 0) return playout(root, pol, rng);
  Result best; if (init) best = *init;
  for (int i = 0; i < ITERS && !STOP; i++) {
    Result r = level == 1 ? playout(root, pol, rng) : nrpa(level - 1, root, pol, rng);
    if (r.score >= best.score) best = r;
    adapt(pol, root, best.seq, ALPHA);
  }
  return best;
}

static vector<vector<Mv>> read_tours(const char* fn) {
  vector<vector<Mv>> out; FILE* f = fopen(fn, "r"); if (!f) { fprintf(stderr, "cannot open %s\n", fn); exit(1); }
  static char line[200000];
  while (fgets(line, sizeof line, f)) {
    vector<Mv> t; S s = start_state(); char* p = line; int w; char sq[8];
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
      t.push_back({d, q}); apply_move(s, d, q); p++;
    }
    if (t.size() > 1 && t.back().q == H8) out.push_back(t);
  }
  fclose(f); return out;
}

static vector<Mv> read_partial(const char* fn) {  // the first line with moves, finished or not
  FILE* f = fopen(fn, "r"); if (!f) { fprintf(stderr, "cannot open %s\n", fn); exit(1); }
  static char line[200000]; vector<Mv> t;
  while (fgets(line, sizeof line, f)) {
    S s = start_state(); char* p = line; int w; char sq[8];
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
      t.push_back({d, q}); apply_move(s, d, q); p++;
    }
    if (!t.empty()) break;
  }
  fclose(f); return t;
}

static Result as_result(const S& root, const vector<Mv>& seq) {
  if (OBJ == 1) {  // the best dance start along seq
    Result r; S s = root; double best = dance_pot(s); size_t len = 0; long long T = s.T;
    for (size_t i = 0; i < seq.size() && seq[i].q != H8 && (int)i < MAXSTEPS; i++) {
      apply_move(s, seq[i].delta, seq[i].q); double p = dance_pot(s); if (p > best) { best = p; len = i + 1; T = s.T; }
    }
    r.seq.assign(seq.begin(), seq.begin() + len); r.minutes = T; r.score = best > -1e17 ? best : -1e7; return r;
  }
  Result r; r.seq = seq; S s = root; for (const Mv& m : seq) apply_move(s, m.delta, m.q);
  r.minutes = s.T; r.finished = !seq.empty() && seq.back().q == H8; r.score = r.finished ? (double)s.T : -1e7;
  return r;
}

static string tour_text(const vector<Mv>& seq) {
  string out; S s = start_state(); char buf[64];
  for (size_t i = 0; i < seq.size(); i++) {
    int w = apply_move(s, seq[i].delta, seq[i].q);
    snprintf(buf, sizeof buf, "%s(%d, %s)", i ? ", " : "", w, nm(seq[i].q).c_str()); out += buf;
  }
  return out;
}

int main(int argc, char** argv) {
  init_board(); D0 = 1; D0 = h8dist(start_state());
  for (int p = 0; p < 64; p++) { for (int q = 0; q < 64; q++) KIDX[p][q] = 0; for (int k = 0; k < NBN[p]; k++) KIDX[p][NBQ[p][k]] = k; }
  int LEVEL = 3, THREADS = max(1u, std::thread::hardware_concurrency() / 2), SEEDADAPT = 10, AMIN = 0, AMAX = 0;
  double TIME = 0, SEEDBEST = 0; bool WATCH = false, WARM = true; string poolFile; long long POOLMIN = 0; int SEEDTOP = 0; double SEEDALPHA = -1;
  unsigned RNG = 1; string seedFile, outFile = "nrpa_best.txt", prefixFile;
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--level") LEVEL = stoi(nx()); else if (a == "--iters") ITERS = stoi(nx()); else if (a == "--alpha") ALPHA = stod(nx());
    else if (a == "--threads") THREADS = stoi(nx()); else if (a == "--time") TIME = stod(nx()); else if (a == "--rng") RNG = (unsigned)stoul(nx());
    else if (a == "--seed-tours") seedFile = nx(); else if (a == "--seed-best") SEEDBEST = stod(nx()); else if (a == "--seed-adapt") SEEDADAPT = stoi(nx());
    else if (a == "--anchor-min") AMIN = stoi(nx()); else if (a == "--anchor-max") AMAX = stoi(nx());
    else if (a == "--bias") BIAS = stod(nx()); else if (a == "--watch") WATCH = true;
    else if (a == "--no-warm") WARM = false;
    else if (a == "--pool") poolFile = nx(); else if (a == "--pool-min") POOLMIN = stoll(nx());
    else if (a == "--seed-top") SEEDTOP = stoi(nx()); else if (a == "--seed-alpha") SEEDALPHA = stod(nx());
    else if (a == "--objective") { string o = nx(); OBJ = o == "opening" ? 1 : 0; if (o != "opening" && o != "tour") { fprintf(stderr, "--objective tour|opening\n"); return 1; } }
    else if (a == "--dmin") DMIN = stoi(nx()); else if (a == "--max-steps") MAXSTEPS = stoi(nx());
    else if (a == "--prefix-file") prefixFile = nx();
    else if (a == "--out") outFile = nx();
    else { fprintf(stderr, "unknown option %s (see the comment at the top of src/nrpa.cpp)\n", a.c_str()); return 1; }
  }
  if (AMAX < AMIN) AMAX = AMIN;
  if (getenv("NRPA_BENCH")) {  // time rollouts from the anchor of the first seed tour with a zero policy
    auto sd = read_tours(seedFile.c_str()); S root = start_state(); for (int i = 0; i < AMIN; i++) apply_move(root, sd[0][i].delta, sd[0][i].q);
    Policy pol; std::mt19937 rng(1); int N = atoi(getenv("NRPA_BENCH")); double tot = 0; long long len = 0;
    auto b0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; i++) { Result r = playout(root, pol, rng); tot += r.minutes; len += r.seq.size(); }
    double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - b0).count();
    printf("%d rollouts in %.2fs (%.0f/s), mean minutes %.1f, mean length %.1f\n", N, dt, N / dt, tot / N, (double)len / N);
    return 0;
  }
  auto top_seeds = [&](vector<vector<Mv>> v) {
    if (SEEDTOP > 0 && (int)v.size() > SEEDTOP) {
      sort(v.begin(), v.end(), [](const vector<Mv>& x, const vector<Mv>& y) { return as_result(start_state(), x).minutes > as_result(start_state(), y).minutes; });
      v.resize(SEEDTOP);
    }
    return v;
  };
  vector<vector<Mv>> seeds; if (!seedFile.empty()) seeds = top_seeds(read_tours(seedFile.c_str()));
  if (!prefixFile.empty()) {  // every restart starts after this (possibly unfinished) sequence, from fresh weights
    seeds = {read_partial(prefixFile.c_str())}; AMIN = AMAX = (int)seeds[0].size(); WARM = false; SEEDADAPT = 0; SEEDBEST = 0;
    printf("prefix of %zu moves from %s\n", seeds[0].size(), prefixFile.c_str());
  }
  printf("NRPA level %d, %d iterations per level, alpha %.2f, %d threads, %zu seed tours, seed-best %.2f, anchor %d-%d, bias %.2f\n",
         LEVEL, ITERS, ALPHA, THREADS, seeds.size(), SEEDBEST, AMIN, AMAX, BIAS); fflush(stdout);
  if (AMAX > 0 && seeds.empty()) { fprintf(stderr, "--anchor-min/--anchor-max need --seed-tours\n"); return 1; }

  std::mutex mtx; Result gbest; auto t0 = std::chrono::steady_clock::now();
  auto mtime = [&]() { struct stat st; return stat(seedFile.c_str(), &st) == 0 ? (long long)st.st_mtime : 0LL; };
  long long seedMtime = seedFile.empty() ? 0 : mtime();
  auto elapsed = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); };
  auto worker = [&](int tid) {
    std::mt19937 rng(RNG * 1000003u + tid);
    for (int restart = 0; !STOP; restart++) {
      Policy pol; vector<Mv> seed;
      {
        std::lock_guard<std::mutex> lk(mtx);
        if (WATCH && !seedFile.empty() && mtime() != seedMtime) {  // another process improved the shared file
          seedMtime = mtime(); auto ns = read_tours(seedFile.c_str());
          if (!ns.empty()) seeds = top_seeds(ns);
          for (auto& t : ns) { Result x = as_result(start_state(), t);
            if (x.finished && x.score > gbest.score) { gbest = x; printf("adopted %lld minutes from %s\n", x.minutes, seedFile.c_str()); } }
        }
        bool useBest = gbest.finished && std::uniform_real_distribution<double>(0, 1)(rng) < SEEDBEST;
        if (useBest) seed = gbest.seq;
        else if (!seeds.empty()) seed = seeds[std::uniform_int_distribution<size_t>(0, seeds.size() - 1)(rng)];
      }
      // anchor: keep the seed's first k moves
      int k = 0;
      if (AMAX > 0 && !seed.empty()) {
        int hi = min<int>(AMAX, (int)seed.size() - (prefixFile.empty() ? 1 : 0)), lo = min(AMIN, hi);
        k = std::uniform_int_distribution<int>(lo, hi)(rng);
      }
      S root = start_state(); vector<Mv> prefix(seed.begin(), seed.begin() + k), rest(seed.begin() + k, seed.end());
      for (const Mv& m : prefix) apply_move(root, m.delta, m.q);
      for (int a = 0; a < SEEDADAPT && !rest.empty(); a++) adapt(pol, root, rest, SEEDALPHA >= 0 ? SEEDALPHA : ALPHA);
      Result init; if (!rest.empty()) init = as_result(root, rest);
      Result r = nrpa(LEVEL, root, pol, rng, (rest.empty() || !WARM) ? nullptr : &init);
      r.seq.insert(r.seq.begin(), prefix.begin(), prefix.end());
      std::lock_guard<std::mutex> lk(mtx);
      printf("[%7.0fs] thread %d restart %d (anchor %d): %s %lld minutes, %zu jumps (best %lld, %lld rollouts)\n", elapsed(), tid, restart, k,
             r.finished ? "finished" : "unfinished", r.minutes, r.seq.size(), gbest.finished ? gbest.minutes : 0LL, (long long)PLAYOUTS);
      if (OBJ == 1) {
        printf("  opening: potential %.0f at move %zu (T %lld)\n", r.score, r.seq.size(), r.minutes);
        if (!poolFile.empty() && r.score >= POOLMIN) { FILE* f = fopen(poolFile.c_str(), "a"); if (f) { fprintf(f, "%.0f\n%s\n", r.score, tour_text(r.seq).c_str()); fclose(f); } }
        if (r.score > gbest.score) {
          gbest = r; FILE* f = fopen(outFile.c_str(), "w");
          if (f) { fprintf(f, "%.0f\n%s\n", r.score, tour_text(r.seq).c_str()); fclose(f); }
          printf("NEW BEST opening potential %.0f (%zu moves) -> %s\n", r.score, r.seq.size(), outFile.c_str());
        }
        fflush(stdout); continue;
      }
      if (!poolFile.empty() && r.finished && r.minutes >= POOLMIN) {  // keep every good result for later polishing
        FILE* f = fopen(poolFile.c_str(), "a"); if (f) { fprintf(f, "%lld\n%s\n", r.minutes, tour_text(r.seq).c_str()); fclose(f); }
      }
      if (r.score > gbest.score) {
        gbest = r;
        if (r.finished) {
          FILE* f = fopen(outFile.c_str(), "w");
          if (!f) { fprintf(stderr, "cannot write %s (does its folder exist?)\n", outFile.c_str()); }
          else { fprintf(f, "%lld\n%s\n", r.minutes, tour_text(r.seq).c_str()); fclose(f); }
          printf("NEW BEST %lld minutes (%zu jumps) -> %s\n", r.minutes, r.seq.size(), outFile.c_str());
        }
      }
      fflush(stdout);
    }
  };
  vector<std::thread> th; for (int t = 0; t < THREADS; t++) th.emplace_back(worker, t);
  while (TIME <= 0 || elapsed() < TIME) std::this_thread::sleep_for(std::chrono::milliseconds(200));
  STOP = true;
  for (auto& t : th) t.join();
  printf("done: best %lld minutes after %.0fs, %lld rollouts\n", gbest.finished ? gbest.minutes : 0LL, elapsed(), (long long)PLAYOUTS);
}
