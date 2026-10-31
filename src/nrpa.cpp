// Nested Rollout Policy Adaptation (Rosin 2011) for the swamp-knight puzzle.
//
// A rollout plays a whole tour from a1, choosing among the legal (pinned) moves with probability
// proportional to exp(weight[code]); codes identify a move by (from, to, sink, visits of the target).
// Level L runs `iters` searches at level L-1, keeping the best sequence and shifting the weights toward
// it after each one. Puzzle-specific details:
//  - a rollout keeps going past chances to finish, but remembers them: the tour it returns ends at the
//    last point where h8 was reachable (so a passed finish is never wasted)
//  - finished tours score their minutes; unfinished ones score below every finished tour, ranked by
//    minutes x exp(-d) (d = 3D distance to h8, 1 at the start), which pulls early searches toward h8
//  - every restart can seed from a known tour (--seed-tours) or the best so far (--seed-best): the weights
//    are shifted toward it and the top-level search starts with it as its best, so it can only improve on it
// Independent searches run on --threads threads; improvements go to --out (line 1 minutes, line 2 tour).
//
// usage: nrpa [--level 3] [--iters 100] [--alpha 1] [--threads N] [--time SECONDS] [--rng SEED]
//             [--seed-tours FILE] [--seed-best P] [--seed-adapt K] [--out FILE]
#include "eng.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>

struct Mv { int delta, q; };
typedef unordered_map<uint32_t, float> Policy;

static inline uint32_t code_of(const S& s, int delta, int q) {
  return (((uint32_t)s.pos * 64 + q) * 128 + (uint32_t)min(delta, 127)) * 4 + (uint32_t)min<int>(s.vis[q], 3);
}
static inline float wget(const Policy& p, uint32_t c) { auto it = p.find(c); return it == p.end() ? 0.f : it->second; }
static double D0 = 1;
static double h8dist(const S& s) {
  int p = s.pos; double dx = 7 - p % 8, dy = 7 - p / 8, dz = (double)s.alt[H8] - s.alt[p];
  return sqrt(dx * dx + dy * dy + dz * dz) / D0;
}

struct Result { double score = -1e18; long long minutes = 0; bool finished = false; vector<Mv> seq; };

static std::atomic<bool> STOP{false};
static std::atomic<long long> PLAYOUTS{0};

// one rollout from a1
static Result playout(const Policy& pol, std::mt19937& rng) {
  S s = start_state(); Result r; vector<pair<int, int>> mv; vector<double> w;
  long long finT = -1; size_t finLen = 0; Mv finMove{0, H8};
  while (true) {
    pinned_moves(s, 3, true, mv);
    w.clear(); double z = 0; int nonfin = 0;
    for (auto& [d, q] : mv) {
      if (q == H8) {  // a chance to finish: remember the best one
        S c = s; apply_move(c, d, q);
        if (c.T >= finT) { finT = c.T; finLen = r.seq.size(); finMove = {d, q}; }
        w.push_back(0); continue;
      }
      double e = exp((double)wget(pol, code_of(s, d, q))); w.push_back(e); z += e; nonfin++;
    }
    if (nonfin == 0) break;
    double u = std::uniform_real_distribution<double>(0, z)(rng), acc = 0; int pick = -1;
    for (size_t i = 0; i < mv.size(); i++) { if (mv[i].second == H8) continue; acc += w[i]; pick = (int)i; if (acc >= u) break; }
    r.seq.push_back({mv[pick].first, mv[pick].second});
    apply_move(s, mv[pick].first, mv[pick].second);
  }
  PLAYOUTS++;
  if (finT >= 0) {
    r.seq.resize(finLen); r.seq.push_back(finMove);
    r.finished = true; r.minutes = finT; r.score = (double)finT;
  } else {
    r.minutes = s.T; r.score = -1e7 + s.T * exp(-h8dist(s));
  }
  return r;
}

// shift the weights toward seq (steps before the final move into h8)
static void adapt(Policy& pol, const vector<Mv>& seq, double alpha) {
  Policy old = pol; S s = start_state(); vector<pair<int, int>> mv;
  for (const Mv& m : seq) {
    if (m.q == H8) break;
    pinned_moves(s, 3, true, mv);
    double z = 0; for (auto& [d, q] : mv) if (q != H8) z += exp((double)wget(old, code_of(s, d, q)));
    for (auto& [d, q] : mv) if (q != H8) { uint32_t c = code_of(s, d, q); pol[c] -= (float)(alpha * exp((double)wget(old, c)) / z); }
    pol[code_of(s, m.delta, m.q)] += (float)alpha;
    apply_move(s, m.delta, m.q);
  }
}

static int ITERS = 100; static double ALPHA = 1.0;
// warm start: `init` (if given) is the best sequence to begin from, so the search can only improve on it
static Result nrpa(int level, Policy pol, std::mt19937& rng, const Result* init = nullptr) {
  if (level == 0) return playout(pol, rng);
  Result best; if (init) best = *init;
  for (int i = 0; i < ITERS && !STOP; i++) {
    Result r = nrpa(level - 1, pol, rng);
    if (r.score >= best.score) best = r;
    adapt(pol, best.seq, ALPHA);
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

static Result as_result(const vector<Mv>& seq) {
  Result r; r.seq = seq; S s = start_state(); for (const Mv& m : seq) apply_move(s, m.delta, m.q);
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
  int LEVEL = 3, THREADS = max(1u, std::thread::hardware_concurrency() / 2), SEEDADAPT = 10; double TIME = 0, SEEDBEST = 0;
  unsigned RNG = 1; string seedFile, outFile = "nrpa_best.txt";
  for (int i = 1; i < argc; i++) {
    string a = argv[i]; auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
    if (a == "--level") LEVEL = stoi(nx()); else if (a == "--iters") ITERS = stoi(nx()); else if (a == "--alpha") ALPHA = stod(nx());
    else if (a == "--threads") THREADS = stoi(nx()); else if (a == "--time") TIME = stod(nx()); else if (a == "--rng") RNG = (unsigned)stoul(nx());
    else if (a == "--seed-tours") seedFile = nx(); else if (a == "--seed-best") SEEDBEST = stod(nx()); else if (a == "--seed-adapt") SEEDADAPT = stoi(nx());
    else if (a == "--out") outFile = nx();
    else { fprintf(stderr, "unknown option %s (see the comment at the top of src/nrpa.cpp)\n", a.c_str()); return 1; }
  }
  vector<vector<Mv>> seeds; if (!seedFile.empty()) seeds = read_tours(seedFile.c_str());
  printf("NRPA level %d, %d iterations per level, alpha %.2f, %d threads, %zu seed tours, seed-best %.2f\n",
         LEVEL, ITERS, ALPHA, THREADS, seeds.size(), SEEDBEST); fflush(stdout);

  std::mutex mtx; Result gbest; auto t0 = std::chrono::steady_clock::now();
  auto elapsed = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); };
  auto worker = [&](int tid) {
    std::mt19937 rng(RNG * 1000003u + tid);
    for (int restart = 0; !STOP; restart++) {
      Policy pol; vector<Mv> seed;
      {
        std::lock_guard<std::mutex> lk(mtx);
        bool useBest = gbest.finished && std::uniform_real_distribution<double>(0, 1)(rng) < SEEDBEST;
        if (useBest) seed = gbest.seq;
        else if (!seeds.empty()) seed = seeds[std::uniform_int_distribution<size_t>(0, seeds.size() - 1)(rng)];
      }
      for (int k = 0; k < SEEDADAPT && !seed.empty(); k++) adapt(pol, seed, ALPHA);
      Result init; if (!seed.empty()) init = as_result(seed);
      Result r = nrpa(LEVEL, pol, rng, seed.empty() ? nullptr : &init);
      std::lock_guard<std::mutex> lk(mtx);
      printf("[%7.0fs] thread %d restart %d: %s %lld minutes, %zu jumps (best %lld, %lld rollouts)\n", elapsed(), tid, restart,
             r.finished ? "finished" : "unfinished", r.minutes, r.seq.size(), gbest.finished ? gbest.minutes : 0LL, (long long)PLAYOUTS);
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
  while (TIME <= 0 || elapsed() < TIME) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    bool alive = false; (void)alive;
  }
  STOP = true;
  for (auto& t : th) t.join();
  printf("done: best %lld minutes after %.0fs, %lld rollouts\n", gbest.finished ? gbest.minutes : 0LL, elapsed(), (long long)PLAYOUTS);
}
