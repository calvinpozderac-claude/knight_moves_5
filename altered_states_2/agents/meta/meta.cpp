// Alternative metaheuristics for Altered States 2 (single thread).
// Scorer adapted from ../../src/as2.cpp, made incremental: changing squares only
// affects states whose names contain one of the old/new letters (alterations are
// letter-blind), so only those states are re-traced.
//
// Modes (run from altered_states_2/):
//   meta score GRID
//   meta sa     --seconds S --seed N [--iters --t0 --t1 --rich P]   (plain or rich-move SA)
//   meta lahc   --seconds S --seed N [--iters --hist L]
//   meta rw     --seconds S --seed N [--sigma s]                    (reweighted SA then true-score SA tail)
//   meta ga     --seconds S --seed N [--pop P --iters ...]          (memetic GA)
//   meta lns    GRID [--k K --letters N]                            (exhaustive k-cluster LNS)
//   meta lnsrun --seconds S --seed N                                (SA restart + LNS polish)
// Every restart / child is appended to --log FILE as grid,score,nstates,method

#include <bits/stdc++.h>
using namespace std;
typedef long long ll;

static const uint32_t FULL = (1u << 25) - 1;
static uint32_t NOTC0, NOTC4;
static inline uint32_t expand(uint32_t m) {
    uint32_t h = ((m & NOTC4) << 1) | ((m & NOTC0) >> 1);
    uint32_t b = m | h;
    return (h | (b << 5) | (b >> 5)) & FULL;
}

static int NS;
static vector<string> NAME, CODE;
static vector<ll> POP;
static vector<vector<int>> SP;
static uint64_t LMASK[26];  // states containing letter
static vector<double> LW;   // letter proposal weights
static uint32_t NB[25];

static void load() {
    for (int i = 0; i < 25; i++) {
        if (i % 5 != 0) NOTC0 |= 1u << i;
        if (i % 5 != 4) NOTC4 |= 1u << i;
    }
    for (int i = 0; i < 25; i++) NB[i] = expand(1u << i) & ~(1u << i);
    ifstream f("data/states.csv");
    if (!f) { fprintf(stderr, "run from altered_states_2/\n"); exit(1); }
    string line; bool hdr = true;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (hdr) { hdr = false; continue; }
        stringstream ss(line); string a, b, c;
        getline(ss, a, ','); getline(ss, b, ','); getline(ss, c, ',');
        vector<int> s;
        for (char ch : a) if (isalpha((unsigned char)ch)) s.push_back(toupper(ch) - 'A');
        NAME.push_back(a); POP.push_back(atoll(b.c_str())); CODE.push_back(c); SP.push_back(s);
    }
    NS = NAME.size();
    for (int i = 0; i < NS; i++) for (int c : SP[i]) LMASK[c] |= 1ULL << i;
    LW.assign(26, 0);
    for (int i = 0; i < NS; i++) for (int c : SP[i]) LW[c] += POP[i];
    double t = 0; for (double x : LW) t += x;
    for (auto& x : LW) x = 0.85 * x / t + 0.15 / 26;
}

static inline bool has(const uint32_t* M, const vector<int>& s) {
    uint32_t d0 = M[s[0]], d1 = FULL;
    size_t L = s.size();
    for (size_t i = 1; i < L; i++) {
        uint32_t m = M[s[i]];
        uint32_t e0 = expand(d0);
        d1 = (expand(d1) & m) | e0;
        d0 = e0 & m;
        if (!d1) return false;
    }
    return true;
}

struct Board {
    uint8_t g[25];
    uint32_t M[26];
    uint64_t have = 0;
    void build() { memset(M, 0, sizeof M); for (int i = 0; i < 25; i++) M[g[i]] |= 1u << i; }
    void set(int i, int c) { M[g[i]] &= ~(1u << i); g[i] = c; M[c] |= 1u << i; }
    void full() { have = 0; for (int i = 0; i < NS; i++) if (has(M, SP[i])) have |= 1ULL << i; }
    // recompute only states in `aff`
    uint64_t recompute(uint64_t aff) const {
        uint64_t h = have & ~aff;
        while (aff) { int i = __builtin_ctzll(aff); aff &= aff - 1; if (has(M, SP[i])) h |= 1ULL << i; }
        return h;
    }
    string str() const { string s(25, '?'); for (int i = 0; i < 25; i++) s[i] = 'a' + g[i]; return s; }
};
static Board parse(const string& s) { Board B; for (int i = 0; i < 25; i++) B.g[i] = tolower(s[i]) - 'a'; B.build(); B.full(); return B; }
static ll score(uint64_t h) { ll s = 0; while (h) { int i = __builtin_ctzll(h); h &= h - 1; s += POP[i]; } return s; }
static double wscore(uint64_t h, const vector<double>& w) { double s = 0; while (h) { int i = __builtin_ctzll(h); h &= h - 1; s += w[i]; } return s; }

static mt19937_64 rng(1);
static double U() { return (rng() >> 11) * (1.0 / 9007199254740992.0); }
static discrete_distribution<int>* LETTER;
static int rl() { return (*LETTER)(rng); }

// ---------------------------------------------------------------- symmetry
static int SYM[8][25];
static void init_sym() {
    for (int s = 0; s < 8; s++) for (int r = 0; r < 5; r++) for (int c = 0; c < 5; c++) {
        int rr = r, cc = c;
        if (s & 1) cc = 4 - cc;
        if (s & 2) rr = 4 - rr;
        if (s & 4) swap(rr, cc);
        SYM[s][r * 5 + c] = rr * 5 + cc;
    }
}
static string canon(const string& g) {
    string best = g;
    for (int s = 0; s < 8; s++) { string t(25, '?'); for (int i = 0; i < 25; i++) t[SYM[s][i]] = g[i]; best = min(best, t); }
    return best;
}

// ---------------------------------------------------------------- moves
struct Change { int sq; int old; };
static vector<Change> CH;
static uint64_t aff_acc;
static inline void put(Board& B, int sq, int c) {
    if (B.g[sq] == c) return;
    aff_acc |= LMASK[B.g[sq]] | LMASK[c];
    CH.push_back({sq, B.g[sq]});
    B.set(sq, c);
}
static void undo(Board& B) { for (int k = (int)CH.size() - 1; k >= 0; k--) B.set(CH[k].sq, CH[k].old); }

// rich-move statistics
static const char* MVN[] = {"single", "swap", "rowshift", "colshift", "rowswap", "colswap", "rot2", "sym3", "plant", "pair"};
static ll MV_TRY[10], MV_ACC[10], MV_IMP[10];

static int propose(Board& B, double prich, uint64_t have) {
    CH.clear(); aff_acc = 0;
    double u = U();
    int type;
    if (u < prich) {
        type = 2 + (int)(rng() % 8);
    } else type = (U() < 0.2) ? 1 : 0;
    uint8_t tmp[25];
    switch (type) {
        case 0: { int a = rng() % 25; put(B, a, rl()); break; }
        case 1: { int a = rng() % 25, b = rng() % 25; int ca = B.g[a], cb = B.g[b]; put(B, a, cb); put(B, b, ca); break; }
        case 2: { int r = rng() % 5, d = (rng() & 1) ? 1 : 4; memcpy(tmp, B.g, 25);
                  for (int c = 0; c < 5; c++) put(B, r * 5 + c, tmp[r * 5 + (c + d) % 5]); break; }
        case 3: { int c = rng() % 5, d = (rng() & 1) ? 1 : 4; memcpy(tmp, B.g, 25);
                  for (int r = 0; r < 5; r++) put(B, r * 5 + c, tmp[((r + d) % 5) * 5 + c]); break; }
        case 4: { int a = rng() % 5, b = rng() % 5; memcpy(tmp, B.g, 25);
                  for (int c = 0; c < 5; c++) { put(B, a * 5 + c, tmp[b * 5 + c]); put(B, b * 5 + c, tmp[a * 5 + c]); } break; }
        case 5: { int a = rng() % 5, b = rng() % 5; memcpy(tmp, B.g, 25);
                  for (int r = 0; r < 5; r++) { put(B, r * 5 + a, tmp[r * 5 + b]); put(B, r * 5 + b, tmp[r * 5 + a]); } break; }
        case 6: { int r = rng() % 4, c = rng() % 4; memcpy(tmp, B.g, 25);
                  int q[4] = {r * 5 + c, r * 5 + c + 1, r * 5 + c + 6, r * 5 + c + 5};
                  int d = 1 + rng() % 3;
                  for (int k = 0; k < 4; k++) put(B, q[k], tmp[q[(k + d) % 4]]); break; }
        case 7: { int r = rng() % 3, c = rng() % 3, s = 1 + rng() % 7; memcpy(tmp, B.g, 25);
                  for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
                      int ii = i, jj = j; if (s & 1) jj = 2 - jj; if (s & 2) ii = 2 - ii; if (s & 4) swap(ii, jj);
                      put(B, (r + ii) * 5 + c + jj, tmp[(r + i) * 5 + c + j]); } break; }
        case 8: { // plant a 3-letter segment of a missing state along a king walk
                  uint64_t miss = ~have & ((1ULL << NS) - 1);
                  if (!miss) break;
                  // choose missing state weighted by pop
                  double tot = 0; for (int i = 0; i < NS; i++) if (miss >> i & 1) tot += POP[i];
                  double x = U() * tot; int st = 0;
                  for (int i = 0; i < NS; i++) if (miss >> i & 1) { x -= POP[i]; if (x <= 0) { st = i; break; } }
                  const auto& s = SP[st]; int len = min<int>(3, s.size());
                  int off = rng() % (s.size() - len + 1);
                  int sq = rng() % 25;
                  for (int k = 0; k < len; k++) {
                      put(B, sq, s[off + k]);
                      uint32_t nb = NB[sq]; int cnt = __builtin_popcount(nb), p = rng() % cnt;
                      for (int t = 0; t < p; t++) nb &= nb - 1;
                      sq = __builtin_ctz(nb);
                  }
                  break; }
        case 9: { // set two adjacent squares at once
                  int a = rng() % 25; uint32_t nb = NB[a]; int cnt = __builtin_popcount(nb), p = rng() % cnt;
                  for (int t = 0; t < p; t++) nb &= nb - 1;
                  int b = __builtin_ctz(nb); put(B, a, rl()); put(B, b, rl()); break; }
    }
    return type;
}

// ---------------------------------------------------------------- annealer
struct Res { Board best; ll bestScore; };
static ll G_BEST = 0; static string G_BESTS;
static FILE* LOG = nullptr;
static int HIT = 0, NRUN = 0; static double SUM = 0; static ll MAXR = 0;
static vector<ll> ALLRES;
static const ll TOP = 265326530;
static void record(const Board& B, ll s, const char* tag) {
    NRUN++; SUM += s; MAXR = max(MAXR, s); ALLRES.push_back(s);
    if (s >= TOP) HIT++;
    if (s > G_BEST) { G_BEST = s; G_BESTS = B.str(); }
    if (LOG) { fprintf(LOG, "%s,%lld,%d,%s\n", B.str().c_str(), s, __builtin_popcountll(B.have), tag); fflush(LOG); }
}

// SA on weighted objective w (or true pop if w empty); returns best on objective, also tracks best true
static Board anneal(Board B, ll iters, double t0, double t1, double prich, const vector<double>& w, ll* bestTrue, Board* bestTrueB) {
    bool weighted = !w.empty();
    auto obj = [&](uint64_t h) { return weighted ? wscore(h, w) : (double)score(h); };
    double cur = obj(B.have);
    Board best = B; double bo = cur;
    ll bt = score(B.have); Board btB = B;
    double lr = log(t1 / t0);
    for (ll it = 0; it < iters; it++) {
        double T = t0 * exp(lr * (double)it / (double)iters);
        int type = propose(B, prich, B.have);
        if (CH.empty()) continue;
        MV_TRY[type]++;
        uint64_t nh = B.recompute(aff_acc);
        double no = obj(nh);
        double d = no - cur;
        if (d >= 0 || U() < exp(d / T)) {
            MV_ACC[type]++; if (d > 0) MV_IMP[type]++;
            B.have = nh; cur = no;
            if (cur > bo) { bo = cur; best = B; }
            if (weighted) { ll s = score(nh); if (s > bt) { bt = s; btB = B; } }
        } else undo(B);
    }
    if (bestTrue) { if (!weighted) { bt = score(best.have); btB = best; } *bestTrue = bt; *bestTrueB = btB; }
    return best;
}

static Board randboard() { Board B; for (int i = 0; i < 25; i++) B.g[i] = rl(); B.build(); B.full(); return B; }

// ---------------------------------------------------------------- LAHC
static Board lahc(Board B, ll iters, int L, double prich) {
    vector<ll> hist(L, score(B.have));
    ll cur = score(B.have); Board best = B; ll bs = cur;
    for (ll it = 0; it < iters; it++) {
        int type = propose(B, prich, B.have);
        if (CH.empty()) continue;
        uint64_t nh = B.recompute(aff_acc);
        ll ns = score(nh);
        int v = it % L;
        if (ns >= cur || ns >= hist[v]) {
            B.have = nh; cur = ns;
            if (cur > bs) { bs = cur; best = B; }
        } else undo(B);
        hist[v] = cur;
    }
    return best;
}

// ---------------------------------------------------------------- LNS (exhaustive k squares over letter subset)
static vector<int> LSET;
static ll lns_best; static uint8_t lns_bestg[25];
static void lns_rec(Board& B, const vector<int>& sq, int k, uint64_t have) {
    if (k == (int)sq.size()) {
        ll s = score(have);
        if (s > lns_best) { lns_best = s; memcpy(lns_bestg, B.g, 25); }
        return;
    }
    int p = sq[k], old = B.g[p];
    for (int c : LSET) {
        uint64_t h = have;
        if (c != old) {
            B.set(p, c);
            uint64_t aff = LMASK[old] | LMASK[c];
            Board tmp; (void)tmp;
            uint64_t hh = have & ~aff; uint64_t a = aff;
            while (a) { int i = __builtin_ctzll(a); a &= a - 1; if (has(B.M, SP[i])) hh |= 1ULL << i; }
            h = hh;
        }
        lns_rec(B, sq, k + 1, h);
        if (c != old) B.set(p, old);
    }
    // also keep original letter if not in LSET
    if (find(LSET.begin(), LSET.end(), old) == LSET.end()) lns_rec(B, sq, k + 1, have);
}
static vector<int> random_cluster(int k) {
    vector<int> s; int a = rng() % 25; s.push_back(a); uint32_t in = 1u << a;
    while ((int)s.size() < k) {
        uint32_t fr = 0; for (int x : s) fr |= NB[x]; fr &= ~in;
        int cnt = __builtin_popcount(fr), p = rng() % cnt;
        for (int t = 0; t < p; t++) fr &= fr - 1;
        int b = __builtin_ctz(fr); s.push_back(b); in |= 1u << b;
    }
    return s;
}
// returns improved board; tries `tries` random clusters (or until no improvement in `patience`)
static Board lns(Board B, int k, int patience, bool verbose) {
    ll cur = score(B.have); int fail = 0;
    while (fail < patience) {
        vector<int> sq = (rng() & 1) ? random_cluster(k) : [&] { vector<int> v(25); iota(v.begin(), v.end(), 0); shuffle(v.begin(), v.end(), rng); v.resize(k); return v; }();
        lns_best = cur; memcpy(lns_bestg, B.g, 25);
        lns_rec(B, sq, 0, B.have);
        if (lns_best > cur) {
            memcpy(B.g, lns_bestg, 25); B.build(); B.full(); cur = score(B.have); fail = 0;
            if (verbose) { printf("  lns -> %lld %s\n", cur, B.str().c_str()); fflush(stdout); }
        } else fail++;
    }
    return B;
}

// ---------------------------------------------------------------- args
static map<string, string> ARGS;
static double argd(const string& k, double d) { return ARGS.count(k) ? atof(ARGS[k].c_str()) : d; }
static string args(const string& k, const string& d) { return ARGS.count(k) ? ARGS[k] : d; }
static double now() { static auto t0 = chrono::steady_clock::now(); return chrono::duration<double>(chrono::steady_clock::now() - t0).count(); }

static void summary(const char* tag) {
    sort(ALLRES.begin(), ALLRES.end());
    double med = ALLRES.empty() ? 0 : ALLRES[ALLRES.size() / 2];
    printf("SUMMARY %s runs %d mean %.3fM median %.3fM max %lld hits(>=265326530) %d time %.0fs best %s\n",
           tag, NRUN, NRUN ? SUM / NRUN / 1e6 : 0, med / 1e6, MAXR, HIT, now(), G_BESTS.c_str());
    ll tot = 0; for (int t = 0; t < 10; t++) tot += MV_TRY[t];
    if (tot) for (int t = 0; t < 10; t++) if (MV_TRY[t])
        printf("  move %-8s tried %10lld acc %.4f improve %.5f\n", MVN[t], MV_TRY[t], (double)MV_ACC[t] / MV_TRY[t], (double)MV_IMP[t] / MV_TRY[t]);
}

static vector<string> load_elite(const string& path, int n) {
    // file with one grid per line (first field)
    vector<string> v; ifstream f(path); string line;
    while (getline(f, line) && (int)v.size() < n) { if (line.size() >= 25) v.push_back(line.substr(0, 25)); }
    return v;
}

int main(int argc, char** argv) {
    load(); init_sym();
    if (argc < 2) { fprintf(stderr, "usage: see header\n"); return 1; }
    string mode = argv[1]; string pos;
    for (int i = 2; i < argc; i++) {
        string a = argv[i];
        if (a.rfind("--", 0) == 0 && i + 1 < argc) { ARGS[a.substr(2)] = argv[++i]; }
        else pos = a;
    }
    rng.seed((uint64_t)argd("seed", 1) * 1000003ULL + 12345);
    discrete_distribution<int> letter(LW.begin(), LW.end()); LETTER = &letter;
    string lg = args("log", ""); if (!lg.empty()) LOG = fopen(lg.c_str(), "a");
    double secs = argd("seconds", 60);
    ll iters = (ll)argd("iters", 5e6);
    double t0 = argd("t0", 3e6), t1 = argd("t1", 1e6), prich = argd("rich", 0);
    { int nl = (int)argd("letters", 18); vector<int> idx(26); iota(idx.begin(), idx.end(), 0);
      sort(idx.begin(), idx.end(), [&](int a, int b) { return LW[a] > LW[b]; }); LSET.assign(idx.begin(), idx.begin() + nl); }

    if (mode == "score") {
        Board B = parse(pos);
        printf("%s %lld %d states:", B.str().c_str(), score(B.have), __builtin_popcountll(B.have));
        for (int i = 0; i < NS; i++) if (B.have >> i & 1) printf(" %s", CODE[i].c_str());
        printf("\n");
        return 0;
    }
    if (mode == "bench") {
        Board B = randboard(); ll n = 0; double t = now(); uint64_t sink = 0;
        while (now() - t < 2) { for (int k = 0; k < 10000; k++) { propose(B, 0, B.have); sink ^= B.recompute(aff_acc); undo(B); n++; } }
        printf("%.2fM evals/s incremental\n", n / (now() - t) / 1e6);
        // correctness: random walk with rich moves, compare incremental vs full
        int bad = 0;
        for (int k = 0; k < 200000; k++) { propose(B, 0.3, B.have); uint64_t h = B.recompute(aff_acc);
            Board C = B; C.full(); if (C.have != h) bad++; if (U() < 0.5) B.have = h; else undo(B); }
        printf("mismatches %d\n", bad);
        Board T = parse("hmlfsoaioutsnrcregahoywin"); t = now(); n = 0;
        while (now() - t < 2) { for (int k = 0; k < 10000; k++) { propose(T, 0, T.have); sink ^= T.recompute(aff_acc); undo(T); n++; } }
        printf("%.2fM evals/s incremental on best board (sink %llu)\n", n / (now() - t) / 1e6, (unsigned long long)sink);
        return 0;
    }
    if (mode == "sa" || mode == "rw" || mode == "lahc" || mode == "lnsrun") {
        double sigma = argd("sigma", 0.5);
        int hist = (int)argd("hist", 2000);
        while (now() < secs) {
            Board B = randboard(), R; ll s;
            if (mode == "sa") { R = anneal(B, iters, t0, t1, prich, {}, nullptr, nullptr); s = score(R.have); }
            else if (mode == "lahc") { R = lahc(B, iters, hist, prich); s = score(R.have); }
            else if (mode == "lnsrun") {
                R = anneal(B, iters, t0, t1, prich, {}, nullptr, nullptr);
                R = lns(R, (int)argd("k", 3), (int)argd("patience", 60), false); s = score(R.have);
            } else {
                vector<double> w(NS);
                for (int i = 0; i < NS; i++) { normal_distribution<double> N(0, sigma); w[i] = POP[i] * exp(N(rng)); }
                double frac = argd("frac", 0.6);
                ll bt; Board btB;
                R = anneal(B, (ll)(iters * frac), t0, t1, prich, w, &bt, &btB);
                R = anneal(R, (ll)(iters * (1 - frac)), argd("t2", 1.5e6), t1, prich, {}, nullptr, nullptr);
                s = score(R.have);
                if (bt > s) { s = bt; R = btB; }
            }
            record(R, s, mode.c_str());
            printf("[%d] %s %lld%s\n", NRUN, R.str().c_str(), s, s >= TOP ? (s > TOP ? "  ***NEW***" : "  =TOP") : ""); fflush(stdout);
        }
        summary(mode.c_str());
        return 0;
    }
    if (mode == "lns") {
        Board B = parse(pos);
        int k = (int)argd("k", 4);
        printf("start %lld\n", score(B.have));
        if (ARGS.count("allclusters")) {
            // exhaustive over every connected cluster of size k (enumerate by canonical sets)
            set<uint32_t> seen; vector<uint32_t> cur;
            for (int i = 0; i < 25; i++) { cur.push_back(1u << i); seen.insert(1u << i); }
            for (int sz = 1; sz < k; sz++) {
                vector<uint32_t> nx;
                for (uint32_t s : cur) { uint32_t fr = 0; for (int i = 0; i < 25; i++) if (s >> i & 1) fr |= NB[i]; fr &= ~s;
                    while (fr) { int b = __builtin_ctz(fr); fr &= fr - 1; uint32_t t = s | 1u << b; if (seen.insert(t).second) nx.push_back(t); } }
                cur = nx;
            }
            printf("%zu clusters of size %d, letters %zu\n", cur.size(), k, LSET.size()); fflush(stdout);
            ll base = score(B.have); int done = 0;
            for (uint32_t s : cur) {
                vector<int> sq; for (int i = 0; i < 25; i++) if (s >> i & 1) sq.push_back(i);
                lns_best = base; memcpy(lns_bestg, B.g, 25);
                lns_rec(B, sq, 0, B.have);
                if (lns_best > base) { Board X = B; memcpy(X.g, lns_bestg, 25); X.build(); X.full(); printf("IMPROVE %lld %s\n", score(X.have), X.str().c_str()); fflush(stdout); }
                if (++done % 200 == 0) { printf("  %d/%zu %.0fs\n", done, cur.size(), now()); fflush(stdout); }
            }
            printf("done %.0fs\n", now());
        } else {
            B = lns(B, k, (int)argd("patience", 200), true);
            printf("end %lld %s\n", score(B.have), B.str().c_str());
        }
        return 0;
    }
    if (mode == "ga") {
        int P = (int)argd("pop", 24);
        double pelite = argd("pelite", 0.5);
        vector<string> elite = load_elite(args("elite", "agents/meta/elite.txt"), (int)argd("nelite", 200));
        vector<pair<ll, Board>> pop; set<string> seen;
        auto add = [&](const Board& B) {
            string c = canon(B.str()); if (seen.count(c)) return false;
            seen.insert(c); pop.push_back({score(B.have), B}); return true;
        };
        // seed: some elite boards (excluding the top ones optionally), rest from short SA
        int nel = (int)(P * pelite);
        int skip = (int)argd("skipelite", 0);
        vector<string> pool(elite.begin() + min<size_t>(skip, elite.size()), elite.end());
        shuffle(pool.begin(), pool.end(), rng);
        for (int i = 0; i < nel && i < (int)pool.size(); i++) add(parse(pool[i]));
        ll seedIters = (ll)argd("seediters", 2e6);
        while ((int)pop.size() < P) { Board R = anneal(randboard(), seedIters, t0, t1, prich, {}, nullptr, nullptr); add(R); record(R, score(R.have), "ga-seed"); }
        ll childIters = (ll)argd("childiters", 1e6);
        double ct0 = argd("ct0", 1.5e6), ct1 = argd("ct1", 5e5);
        int gen = 0;
        while (now() < secs) {
            sort(pop.begin(), pop.end(), [](auto& a, auto& b) { return a.first > b.first; });
            // tournament selection
            auto pick = [&]() { int a = rng() % pop.size(), b = rng() % pop.size(); return min(a, b); };
            int ia = pick(), ib = pick(); if (ia == ib) ib = (ib + 1) % pop.size();
            Board A = pop[ia].second, Bb = pop[ib].second;
            // align B to A by best symmetry
            int bs = 0, bm = -1;
            for (int s = 0; s < 8; s++) { int m = 0; for (int i = 0; i < 25; i++) m += A.g[SYM[s][i]] == Bb.g[i]; if (m > bm) { bm = m; bs = s; } }
            uint8_t bg[25]; for (int i = 0; i < 25; i++) bg[SYM[bs][i]] = Bb.g[i];
            // mask: rows / cols / quadrant / random rectangle / random cluster
            uint32_t mask = 0; int mt = rng() % 4;
            if (mt == 0) { int r = 1 + rng() % 4; for (int i = 0; i < r * 5; i++) mask |= 1u << i; }
            else if (mt == 1) { int c = 1 + rng() % 4; for (int i = 0; i < 25; i++) if (i % 5 < c) mask |= 1u << i; }
            else if (mt == 2) { int r0 = rng() % 5, c0 = rng() % 5, h = 2 + rng() % 3, w = 2 + rng() % 3;
                for (int r = r0; r < min(5, r0 + h); r++) for (int c = c0; c < min(5, c0 + w); c++) mask |= 1u << (r * 5 + c); }
            else { auto cl = random_cluster(6 + rng() % 8); for (int x : cl) mask |= 1u << x; }
            Board C; for (int i = 0; i < 25; i++) C.g[i] = (mask >> i & 1) ? bg[i] : A.g[i];
            C.build(); C.full();
            Board R = anneal(C, childIters, ct0, ct1, prich, {}, nullptr, nullptr);
            ll s = score(R.have);
            record(R, s, "ga-child");
            gen++;
            string c = canon(R.str());
            if (ARGS.count("crowd")) {
                // deterministic crowding: replace the most similar member (best symmetry alignment) if child is better
                int bi = -1, bsim = -1;
                for (int j = 0; j < (int)pop.size(); j++) {
                    int mx = 0;
                    for (int sy = 0; sy < 8; sy++) { int m = 0; for (int i = 0; i < 25; i++) m += pop[j].second.g[SYM[sy][i]] == R.g[i]; mx = max(mx, m); }
                    if (mx > bsim) { bsim = mx; bi = j; }
                }
                if (!seen.count(c) && s > pop[bi].first) { seen.erase(canon(pop[bi].second.str())); pop[bi] = {s, R}; seen.insert(c); }
            } else if (!seen.count(c) && s > pop.back().first) {
                seen.erase(canon(pop.back().second.str()));
                pop.back() = {s, R}; seen.insert(c);
            }
            if (gen % 50 == 0) {
                sort(pop.begin(), pop.end(), [](auto& a, auto& b) { return a.first > b.first; });
                printf("gen %d t=%.0f popbest %lld popmed %lld worst %lld globalbest %lld hits %d\n", gen, now(), pop[0].first, pop[P / 2].first, pop.back().first, G_BEST, HIT);
                fflush(stdout);
            }
            if (s > TOP) { printf("***NEW*** %s %lld\n", R.str().c_str(), s); fflush(stdout); }
        }
        summary("ga");
        sort(pop.begin(), pop.end(), [](auto& a, auto& b) { return a.first > b.first; });
        for (int i = 0; i < min(P, 8); i++) printf("  pop %lld %s\n", pop[i].first, pop[i].second.str().c_str());
        return 0;
    }
    fprintf(stderr, "unknown mode\n");
    return 1;
}
