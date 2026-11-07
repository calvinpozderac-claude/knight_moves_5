// Altered States 2 (Jane Street puzzle) solver.
//
// A 5x5 grid of letters. A state scores (its 2020 census population, once) if its name
// can be traced by king's moves (squares may be reused, but every step moves to a
// different, adjacent square) with at most one letter "altered" (substituted).
//
// Scoring is a bit-parallel DP: for each state and each prefix length i,
//   d0 = set of squares where the first i letters can end with 0 substitutions,
//   d1 = same with <= 1 substitution (d2 with <= 2 for the optional near-miss bonus),
// each a 25-bit mask, advanced with a king-neighbourhood "expand" made of shifts.
//
// Modes:
//   as2 score GRID            score a 25-letter grid, list states and citations
//   as2 sa [options]          simulated annealing, many restarts, several threads
//   as2 polish GRID [opts]    exhaustive 1-, 2- (and optionally 3-) square improvement
//
// Run from the altered_states_2/ folder (reads data/states.csv and data/borders.csv).

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
typedef long long ll;

static const uint32_t FULL = (1u << 25) - 1;
static uint32_t NOTC0, NOTC4;  // squares not in column 0 / not in column 4

static inline uint32_t expand(uint32_t m) {
    uint32_t h = ((m & NOTC4) << 1) | ((m & NOTC0) >> 1);
    uint32_t b = m | h;
    return (h | (b << 5) | (b >> 5)) & FULL;
}

struct StateInfo {
    string name, spell, code;
    char coast;  // 'E', 'W' or '-'
    ll pop;
    vector<int> s;  // letters 0..25
};

static vector<StateInfo> ST;
static int NS;
static vector<vector<int>> ADJ;  // land borders, by state index
static ll TOTAL = 0;

static string trim(const string& x) {
    size_t a = x.find_first_not_of(" \t\r\n"), b = x.find_last_not_of(" \t\r\n");
    return a == string::npos ? "" : x.substr(a, b - a + 1);
}

static vector<vector<string>> read_csv(const string& path) {
    ifstream f(path);
    if (!f) { fprintf(stderr, "cannot open %s (run from the altered_states_2 folder)\n", path.c_str()); exit(1); }
    vector<vector<string>> rows;
    string line;
    bool header = true;
    while (getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (header) { header = false; continue; }
        vector<string> r;
        stringstream ss(line);
        string c;
        while (getline(ss, c, ',')) r.push_back(trim(c));
        rows.push_back(r);
    }
    return rows;
}

static void load(const string& dir) {
    for (int i = 0; i < 25; i++) {
        if (i % 5 != 0) NOTC0 |= 1u << i;
        if (i % 5 != 4) NOTC4 |= 1u << i;
    }
    map<string, int> idx;
    for (auto& r : read_csv(dir + "/states.csv")) {
        StateInfo s;
        s.name = r[0];
        s.pop = atoll(r[1].c_str());
        s.code = r[2];
        s.coast = r[3][0];
        for (char c : s.name)
            if (isalpha((unsigned char)c)) { s.spell += (char)toupper(c); s.s.push_back(toupper(c) - 'A'); }
        idx[s.code] = (int)ST.size();
        ST.push_back(s);
        TOTAL += s.pop;
    }
    NS = (int)ST.size();
    ADJ.assign(NS, {});
    for (auto& r : read_csv(dir + "/borders.csv")) {
        if (r[2] != "land") continue;
        int a = idx.at(r[0]), b = idx.at(r[1]);
        ADJ[a].push_back(b);
        ADJ[b].push_back(a);
    }
}

static int idx_of(const string& code) {
    for (int i = 0; i < NS; i++) if (ST[i].code == code) return i;
    fprintf(stderr, "unknown state code %s\n", code.c_str());
    exit(1);
}

// ---------------------------------------------------------------- grid + scoring

struct Grid {
    uint8_t g[25];
    uint32_t M[26];
    void build() {
        memset(M, 0, sizeof M);
        for (int i = 0; i < 25; i++) M[g[i]] |= 1u << i;
    }
    void set(int i, int c) {
        M[g[i]] &= ~(1u << i);
        g[i] = (uint8_t)c;
        M[c] |= 1u << i;
    }
    string str() const {
        string s(25, '?');
        for (int i = 0; i < 25; i++) s[i] = (char)('a' + g[i]);
        return s;
    }
};

static Grid parse_grid(const string& s) {
    Grid G;
    if (s.size() != 25) { fprintf(stderr, "grid must have 25 letters\n"); exit(1); }
    for (int i = 0; i < 25; i++) G.g[i] = (uint8_t)(tolower(s[i]) - 'a');
    G.build();
    return G;
}

// minimum number of substitutions needed (0, 1, 2) or 3 meaning "more than 2"
static inline int need(const uint32_t* M, const vector<int>& s, int maxk) {
    uint32_t d0 = M[s[0]], d1 = FULL, d2 = FULL;
    size_t L = s.size();
    if (maxk >= 2) {
        for (size_t i = 1; i < L; i++) {
            uint32_t m = M[s[i]];
            uint32_t e0 = expand(d0), e1 = expand(d1);
            d2 = (expand(d2) & m) | e1;
            d1 = (e1 & m) | e0;
            d0 = e0 & m;
            if (!d2) return 3;
        }
        return d0 ? 0 : d1 ? 1 : 2;
    }
    for (size_t i = 1; i < L; i++) {
        uint32_t m = M[s[i]];
        uint32_t e0 = expand(d0);
        d1 = (expand(d1) & m) | e0;
        d0 = e0 & m;
        if (!d1) return 3;
    }
    return d0 ? 0 : 1;
}

// ---------------------------------------------------------------- citations

enum { A_20S, A_200M, A_PA, A_M8, A_4C, A_NOCAL, A_C2C, NA };
static const char* ANAME[NA] = {"20S", "200M", "PA", "M8", "4C", "NOCAL", "C2C"};
static vector<int> M8_IDX, C4_IDX;
static int PA_IDX, CA_IDX;

static void init_awards() {
    for (int i = 0; i < NS; i++) if (ST[i].spell[0] == 'M') M8_IDX.push_back(i);
    for (string c : {"AZ", "CO", "NM", "UT"}) C4_IDX.push_back(idx_of(c));
    PA_IDX = idx_of("PA");
    CA_IDX = idx_of("CA");
}

// fewest states that must be added to `have` to complete a coast-to-coast chain
// (0 = the board already has one). States in `banned` may not be used.
static int c2c_missing(const char* have, const char* banned) {
    const int INF = 1000;
    vector<int> dist(NS, INF);
    // Dijkstra with node weights in {0,1}: cost of using a state = 0 if on the board, else 1
    vector<vector<int>> buckets(NS + 2);
    for (int i = 0; i < NS; i++)
        if (ST[i].coast == 'E' && !banned[i]) {
            dist[i] = have[i] ? 0 : 1;
            buckets[dist[i]].push_back(i);
        }
    for (int d = 0; d <= NS; d++) {
        for (size_t k = 0; k < buckets[d].size(); k++) {
            int u = buckets[d][k];
            if (dist[u] != d) continue;
            if (ST[u].coast == 'W') return d;
            for (int v : ADJ[u]) {
                if (banned[v]) continue;
                int nd = d + (have[v] ? 0 : 1);
                if (nd < dist[v]) { dist[v] = nd; buckets[nd].push_back(v); }
            }
        }
    }
    return INF;
}

struct Eval {
    ll score = 0;
    int count = 0;
    char have[64] = {};
    int near = 0;       // states needing exactly 2 substitutions
    ll near_pop = 0;
    bool award[NA] = {};
    double obj = 0;
};

struct Config {
    double near_bonus = 0.0;   // weight on population of states needing 2 substitutions
    vector<int> targets;       // citations to enforce
    double penalty = 20e6;     // objective penalty per unit of citation deficiency
};

static void compute_awards(Eval& e) {
    e.award[A_20S] = e.count >= 20;
    e.award[A_200M] = e.score >= 200000000LL;
    e.award[A_PA] = e.have[PA_IDX];
    bool m8 = true; for (int i : M8_IDX) m8 &= (bool)e.have[i];
    e.award[A_M8] = m8;
    bool c4 = true; for (int i : C4_IDX) c4 &= (bool)e.have[i];
    e.award[A_4C] = c4;
    e.award[A_NOCAL] = !e.have[CA_IDX];
    char none[64] = {};
    e.award[A_C2C] = c2c_missing(e.have, none) == 0;
}

static Eval evaluate(const Grid& G, const Config& cfg, bool full) {
    Eval e;
    int maxk = (cfg.near_bonus > 0) ? 2 : 1;
    for (int i = 0; i < NS; i++) {
        int k = need(G.M, ST[i].s, maxk);
        if (k <= 1) { e.have[i] = 1; e.score += ST[i].pop; e.count++; }
        else if (k == 2) { e.near++; e.near_pop += ST[i].pop; }
    }
    double obj = (double)e.score + cfg.near_bonus * (double)e.near_pop;
    for (int a : cfg.targets) {
        double def = 0;
        switch (a) {
            case A_20S: def = max(0, 20 - e.count); break;
            case A_200M: def = max(0.0, (200e6 - (double)e.score) / 10e6); break;
            case A_PA: def = e.have[PA_IDX] ? 0 : 1; break;
            case A_M8: for (int i : M8_IDX) def += e.have[i] ? 0 : 1; break;
            case A_4C: for (int i : C4_IDX) def += e.have[i] ? 0 : 1; break;
            case A_NOCAL: def = e.have[CA_IDX] ? 3 : 0; break;
            case A_C2C: {
                char banned[64] = {};
                for (int t : cfg.targets) if (t == A_NOCAL) banned[CA_IDX] = 1;
                def = min(10, c2c_missing(e.have, banned));
                break;
            }
        }
        obj -= cfg.penalty * def;
    }
    e.obj = obj;
    if (full) compute_awards(e);
    return e;
}

static void print_eval(const Grid& G, const Eval& e) {
    string s = G.str();
    printf("grid %s\n", s.c_str());
    for (int r = 0; r < 5; r++) {
        printf("  ");
        for (int c = 0; c < 5; c++) printf(" %c", toupper(s[r * 5 + c]));
        printf("\n");
    }
    printf("score %lld (%.2f%% of %lld), %d states\n", e.score, 100.0 * e.score / TOTAL, TOTAL, e.count);
    printf("states:");
    for (int i = 0; i < NS; i++) if (e.have[i]) printf(" %s", ST[i].code.c_str());
    printf("\ncitations:");
    for (int a = 0; a < NA; a++) if (e.award[a]) printf(" %s", ANAME[a]);
    printf("\n");
}

// ---------------------------------------------------------------- annealing

static vector<double> letter_weights() {
    // letters weighted by how often they occur in state names, times population,
    // plus a uniform floor so every letter can be proposed
    vector<double> w(26, 0);
    for (auto& s : ST) for (int c : s.s) w[c] += (double)s.pop;
    double tot = 0;
    for (double x : w) tot += x;
    for (auto& x : w) x = 0.85 * x / tot + 0.15 / 26.0;
    return w;
}

struct SAOpts {
    int threads = 4;
    int restarts = 100;       // per thread
    ll iters = 2000000;       // per restart
    double t0 = 3e6, t1 = 2e4;
    unsigned seed = 1;
    double seconds = 0;       // wall-clock limit (0 = none)
    double p_swap = 0.2;
    string start;             // optional starting grid (otherwise random)
    double start_noise = 0;   // fraction of squares randomised when starting from `start`
    string log;               // file to append every restart's result to
};

static mutex out_mu;
static Grid best_grid;
static double best_obj = -1e300;

static void sa_thread(int tid, const SAOpts& o, const Config& cfg, atomic<int>& done_restarts,
                      chrono::steady_clock::time_point t_start, vector<pair<double, string>>* results) {
    mt19937_64 rng(o.seed * 1000003ULL + tid * 7919ULL + 17);
    uniform_real_distribution<double> U(0, 1);
    vector<double> w = letter_weights();
    discrete_distribution<int> letter(w.begin(), w.end());
    for (int rs = 0; rs < o.restarts; rs++) {
        if (o.seconds > 0) {
            double el = chrono::duration<double>(chrono::steady_clock::now() - t_start).count();
            if (el > o.seconds) break;
        }
        Grid G;
        if (!o.start.empty()) {
            G = parse_grid(o.start);
            for (int i = 0; i < 25; i++) if (U(rng) < o.start_noise) G.g[i] = (uint8_t)letter(rng);
            G.build();
        } else {
            for (int i = 0; i < 25; i++) G.g[i] = (uint8_t)letter(rng);
            G.build();
        }
        Eval cur = evaluate(G, cfg, false);
        Grid bestG = G;
        double bestO = cur.obj;
        double lr = log(o.t1 / o.t0);
        for (ll it = 0; it < o.iters; it++) {
            double T = o.t0 * exp(lr * (double)it / (double)o.iters);
            int a, b = -1, ca = 0, cb = 0;
            if (U(rng) < o.p_swap) {
                a = (int)(rng() % 25);
                b = (int)(rng() % 25);
                if (G.g[a] == G.g[b]) continue;
                ca = G.g[a]; cb = G.g[b];
                G.set(a, cb); G.set(b, ca);
            } else {
                a = (int)(rng() % 25);
                int c = letter(rng);
                if (c == G.g[a]) continue;
                ca = G.g[a];
                G.set(a, c);
            }
            Eval ne = evaluate(G, cfg, false);
            double d = ne.obj - cur.obj;
            if (d >= 0 || U(rng) < exp(d / T)) {
                cur = ne;
                if (cur.obj > bestO) { bestO = cur.obj; bestG = G; }
            } else {
                if (b >= 0) { G.set(a, ca); G.set(b, cb); }
                else G.set(a, ca);
            }
        }
        Eval be = evaluate(bestG, cfg, true);
        {
            lock_guard<mutex> lk(out_mu);
            results->push_back({bestO, bestG.str()});
            int n = ++done_restarts;
            bool improved = bestO > best_obj;
            if (improved) { best_obj = bestO; best_grid = bestG; }
            printf("[t%d r%d #%d] %s score %lld states %d obj %.0f%s\n", tid, rs, n, bestG.str().c_str(),
                   be.score, be.count, bestO, improved ? "  *best*" : "");
            fflush(stdout);
            if (!o.log.empty()) {
                FILE* f = fopen(o.log.c_str(), "a");
                if (f) {
                    fprintf(f, "%s,%lld,%d,%.0f,", bestG.str().c_str(), be.score, be.count, bestO);
                    bool first = true;
                    for (int a2 = 0; a2 < NA; a2++)
                        if (be.award[a2]) { fprintf(f, "%s%s", first ? "" : " ", ANAME[a2]); first = false; }
                    fprintf(f, "\n");
                    fclose(f);
                }
            }
        }
    }
}

// ---------------------------------------------------------------- parallel tempering

struct PTOpts {
    int replicas = 16;
    double tlo = 3e4, thi = 3e6;
    int steps = 200;          // Metropolis moves per replica between exchange attempts
};

static void pt_thread(int tid, const SAOpts& o, const PTOpts& p, const Config& cfg,
                      chrono::steady_clock::time_point t_start, vector<pair<double, string>>* results) {
    mt19937_64 rng(o.seed * 1000003ULL + tid * 7919ULL + 99991);
    uniform_real_distribution<double> U(0, 1);
    vector<double> w = letter_weights();
    discrete_distribution<int> letter(w.begin(), w.end());
    int K = p.replicas;
    vector<Grid> G(K);
    vector<double> O(K), T(K);
    for (int k = 0; k < K; k++) {
        T[k] = p.tlo * pow(p.thi / p.tlo, K == 1 ? 0.0 : (double)k / (K - 1));
        if (!o.start.empty()) {
            G[k] = parse_grid(o.start);
            for (int i = 0; i < 25; i++) if (U(rng) < o.start_noise) G[k].g[i] = (uint8_t)letter(rng);
        } else {
            for (int i = 0; i < 25; i++) G[k].g[i] = (uint8_t)letter(rng);
        }
        G[k].build();
        O[k] = evaluate(G[k], cfg, false).obj;
    }
    Grid bestG = G[0];
    double bestO = O[0];
    vector<ll> tries(K, 0), swaps(K, 0);
    ll round = 0;
    double next_report = 60;
    while (true) {
        double el = chrono::duration<double>(chrono::steady_clock::now() - t_start).count();
        if (el > o.seconds) break;
        for (int k = 0; k < K; k++) {
            Grid& g = G[k];
            for (int s = 0; s < p.steps; s++) {
                int a, b = -1, ca = 0, cb = 0;
                if (U(rng) < o.p_swap) {
                    a = (int)(rng() % 25);
                    b = (int)(rng() % 25);
                    if (g.g[a] == g.g[b]) continue;
                    ca = g.g[a]; cb = g.g[b];
                    g.set(a, cb); g.set(b, ca);
                } else {
                    a = (int)(rng() % 25);
                    int c = letter(rng);
                    if (c == g.g[a]) continue;
                    ca = g.g[a];
                    g.set(a, c);
                }
                double no = evaluate(g, cfg, false).obj;
                double d = no - O[k];
                if (d >= 0 || U(rng) < exp(d / T[k])) {
                    O[k] = no;
                    if (no > bestO) {
                        bestO = no; bestG = g;
                        lock_guard<mutex> lk(out_mu);
                        if (bestO > best_obj) {
                            best_obj = bestO; best_grid = bestG;
                            Eval be = evaluate(bestG, cfg, true);
                            printf("[t%d %.0fs] %s score %lld states %d obj %.0f\n", tid, el, bestG.str().c_str(),
                                   be.score, be.count, bestO);
                            fflush(stdout);
                        }
                    }
                } else {
                    if (b >= 0) { g.set(a, ca); g.set(b, cb); }
                    else g.set(a, ca);
                }
            }
        }
        for (int k = (int)(round & 1); k + 1 < K; k += 2) {
            double d = (1.0 / T[k] - 1.0 / T[k + 1]) * (O[k + 1] - O[k]);
            tries[k]++;
            if (d >= 0 || U(rng) < exp(d)) { swap(G[k], G[k + 1]); swap(O[k], O[k + 1]); swaps[k]++; }
        }
        round++;
        if (tid == 0 && el > next_report) {
            next_report += 60;
            lock_guard<mutex> lk(out_mu);
            printf("[t0 %.0fs] rounds %lld, swap rates:", el, round);
            for (int k = 0; k + 1 < K; k++) printf(" %.2f", tries[k] ? (double)swaps[k] / tries[k] : 0.0);
            printf("\n");
            fflush(stdout);
        }
    }
    lock_guard<mutex> lk(out_mu);
    results->push_back({bestO, bestG.str()});
    Eval be = evaluate(bestG, cfg, true);
    printf("[t%d done] %s score %lld states %d\n", tid, bestG.str().c_str(), be.score, be.count);
    if (!o.log.empty()) {
        FILE* f = fopen(o.log.c_str(), "a");
        if (f) {
            fprintf(f, "%s,%lld,%d,%.0f,", bestG.str().c_str(), be.score, be.count, bestO);
            bool first = true;
            for (int a2 = 0; a2 < NA; a2++)
                if (be.award[a2]) { fprintf(f, "%s%s", first ? "" : " ", ANAME[a2]); first = false; }
            fprintf(f, "\n");
            fclose(f);
        }
    }
}

// ---------------------------------------------------------------- polish

// exhaustive best-improvement search over all changes of up to `depth` squares
static Grid polish(Grid G, const Config& cfg, int depth, bool verbose) {
    Eval cur = evaluate(G, cfg, false);
    bool improved = true;
    while (improved) {
        improved = false;
        double bestO = cur.obj;
        vector<pair<int, int>> bestMove;
        // depth 1
        for (int a = 0; a < 25; a++) {
            int oa = G.g[a];
            for (int ca = 0; ca < 26; ca++) {
                if (ca == oa) continue;
                G.set(a, ca);
                double o = evaluate(G, cfg, false).obj;
                if (o > bestO + 0.5) { bestO = o; bestMove = {{a, ca}}; }
                if (depth >= 2) {
                    for (int b = a + 1; b < 25; b++) {
                        int ob = G.g[b];
                        for (int cb = 0; cb < 26; cb++) {
                            if (cb == ob) continue;
                            G.set(b, cb);
                            double o2 = evaluate(G, cfg, false).obj;
                            if (o2 > bestO + 0.5) { bestO = o2; bestMove = {{a, ca}, {b, cb}}; }
                            if (depth >= 3) {
                                for (int c = b + 1; c < 25; c++) {
                                    int oc = G.g[c];
                                    for (int cc = 0; cc < 26; cc++) {
                                        if (cc == oc) continue;
                                        G.set(c, cc);
                                        double o3 = evaluate(G, cfg, false).obj;
                                        if (o3 > bestO + 0.5) { bestO = o3; bestMove = {{a, ca}, {b, cb}, {c, cc}}; }
                                    }
                                    G.set(c, oc);
                                }
                            }
                        }
                        G.set(b, ob);
                    }
                }
            }
            G.set(a, oa);
        }
        if (!bestMove.empty()) {
            for (auto& m : bestMove) G.set(m.first, m.second);
            cur = evaluate(G, cfg, false);
            improved = true;
            if (verbose) { printf("  improved to obj %.0f: %s\n", cur.obj, G.str().c_str()); fflush(stdout); }
        }
    }
    return G;
}

// ---------------------------------------------------------------- main

static vector<int> parse_targets(const string& s) {
    vector<int> t;
    stringstream ss(s);
    string x;
    while (getline(ss, x, ',')) {
        bool ok = false;
        for (int a = 0; a < NA; a++) if (x == ANAME[a]) { t.push_back(a); ok = true; }
        if (!ok) { fprintf(stderr, "unknown citation %s\n", x.c_str()); exit(1); }
    }
    return t;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
                "usage: as2 score GRID | as2 sa [--threads N --restarts R --iters I --t0 T --t1 T --seed S\n"
                "       --seconds S --swap P --near W --award LIST --penalty P --start GRID --noise F --log FILE]\n"
                "       | as2 pt [--seconds S --replicas K --tlo T --thi T --steps N + sa options]\n"
                "       | as2 polish GRID [--depth D --near W --award LIST]\n");
        return 1;
    }
    load("data");
    init_awards();
    string mode = argv[1];
    SAOpts o;
    Config cfg;
    PTOpts pt;
    int depth = 2;
    string grid;
    for (int i = 2; i < argc; i++) {
        string a = argv[i];
        auto nxt = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return string(argv[++i]); };
        if (a == "--threads") o.threads = stoi(nxt());
        else if (a == "--restarts") o.restarts = stoi(nxt());
        else if (a == "--iters") o.iters = stoll(nxt());
        else if (a == "--t0") o.t0 = stod(nxt());
        else if (a == "--t1") o.t1 = stod(nxt());
        else if (a == "--seed") o.seed = (unsigned)stoul(nxt());
        else if (a == "--seconds") o.seconds = stod(nxt());
        else if (a == "--swap") o.p_swap = stod(nxt());
        else if (a == "--near") cfg.near_bonus = stod(nxt());
        else if (a == "--award") cfg.targets = parse_targets(nxt());
        else if (a == "--penalty") cfg.penalty = stod(nxt());
        else if (a == "--start") o.start = nxt();
        else if (a == "--noise") o.start_noise = stod(nxt());
        else if (a == "--log") o.log = nxt();
        else if (a == "--depth") depth = stoi(nxt());
        else if (a == "--replicas") pt.replicas = stoi(nxt());
        else if (a == "--tlo") pt.tlo = stod(nxt());
        else if (a == "--thi") pt.thi = stod(nxt());
        else if (a == "--steps") pt.steps = stoi(nxt());
        else if (a[0] != '-') grid = a;
        else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 1; }
    }
    if (mode == "score") {
        Grid G = parse_grid(grid);
        print_eval(G, evaluate(G, cfg, true));
    } else if (mode == "sa") {
        auto t_start = chrono::steady_clock::now();
        atomic<int> done{0};
        vector<vector<pair<double, string>>> res(o.threads);
        vector<thread> th;
        for (int t = 0; t < o.threads; t++)
            th.emplace_back(sa_thread, t, cref(o), cref(cfg), ref(done), t_start, &res[t]);
        for (auto& t : th) t.join();
        double el = chrono::duration<double>(chrono::steady_clock::now() - t_start).count();
        printf("\n%d restarts in %.1f s\nBEST:\n", done.load(), el);
        print_eval(best_grid, evaluate(best_grid, cfg, true));
    } else if (mode == "pt") {
        if (o.seconds <= 0) o.seconds = 60;
        auto t_start = chrono::steady_clock::now();
        vector<vector<pair<double, string>>> res(o.threads);
        vector<thread> th;
        for (int t = 0; t < o.threads; t++)
            th.emplace_back(pt_thread, t, cref(o), cref(pt), cref(cfg), t_start, &res[t]);
        for (auto& t : th) t.join();
        printf("\nBEST:\n");
        print_eval(best_grid, evaluate(best_grid, cfg, true));
    } else if (mode == "polish") {
        Grid G = parse_grid(grid);
        Grid P = polish(G, cfg, depth, true);
        print_eval(P, evaluate(P, cfg, true));
    } else {
        fprintf(stderr, "unknown mode %s\n", mode.c_str());
        return 1;
    }
    return 0;
}
