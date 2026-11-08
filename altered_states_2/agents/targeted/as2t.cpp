// Targeted search variant of as2 (single thread).
// Weighted objective: each state i has weight w_i = pop_i (+ BONUS if required, -BONUS if forbidden).
// Evaluation with early abort (upper bound) against an acceptance threshold.
//
// Modes:
//   as2t score GRID
//   as2t need            (grids on stdin; prints per-state min substitutions 0/1/2/3)
//   as2t sa [--require A,B] [--forbid C] [--bonus B] [--iters N] [--restarts R] [--t0 --t1]
//           [--seed S] [--seconds S] [--start GRID --noise F] [--log FILE] [--swap P] [--polish]
//   as2t polishw GRID [--depth D] [--letters abc..] [--win 3]   exhaustive changes of D squares
//           all lying inside one WxW window, letters restricted
//   as2t rr GRID [--seconds S] [--seed S] [--log FILE]   ruin & recreate (window re-anneal)
// Run from altered_states_2/ (reads data/states.csv).
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
struct St { string name, code; ll pop; vector<int> s; };
static vector<St> ST;  // original order (as in csv, by population desc)
static int NS;
static ll TOTAL = 0;

static void load() {
    for (int i = 0; i < 25; i++) { if (i % 5) NOTC0 |= 1u << i; if (i % 5 != 4) NOTC4 |= 1u << i; }
    ifstream f("data/states.csv");
    if (!f) { fprintf(stderr, "run from altered_states_2/\n"); exit(1); }
    string line; bool hdr = true;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (hdr) { hdr = false; continue; }
        stringstream ss(line); string a, b, c;
        getline(ss, a, ','); getline(ss, b, ','); getline(ss, c, ',');
        St s; s.name = a; s.pop = atoll(b.c_str()); s.code = c;
        for (char ch : a) if (isalpha((unsigned char)ch)) s.s.push_back(toupper(ch) - 'A');
        ST.push_back(s); TOTAL += s.pop;
    }
    NS = ST.size();
}
static int idx_of(const string& c) { for (int i = 0; i < NS; i++) if (ST[i].code == c) return i; fprintf(stderr, "bad code %s\n", c.c_str()); exit(1); }

struct Grid {
    uint8_t g[25]; uint32_t M[26];
    void build() { memset(M, 0, sizeof M); for (int i = 0; i < 25; i++) M[g[i]] |= 1u << i; }
    void set(int i, int c) { M[g[i]] &= ~(1u << i); g[i] = c; M[c] |= 1u << i; }
    string str() const { string s(25, '?'); for (int i = 0; i < 25; i++) s[i] = 'a' + g[i]; return s; }
};
static Grid parse(const string& s) { Grid G; for (int i = 0; i < 25; i++) G.g[i] = tolower(s[i]) - 'a'; G.build(); return G; }

static inline int need(const uint32_t* M, const vector<int>& s, int maxk) {
    uint32_t d0 = M[s[0]], d1 = FULL, d2 = FULL; size_t L = s.size();
    if (maxk >= 2) {
        for (size_t i = 1; i < L; i++) { uint32_t m = M[s[i]]; uint32_t e0 = expand(d0), e1 = expand(d1);
            d2 = (expand(d2) & m) | e1; d1 = (e1 & m) | e0; d0 = e0 & m; if (!d2) return 3; }
        return d0 ? 0 : d1 ? 1 : 2;
    }
    for (size_t i = 1; i < L; i++) { uint32_t m = M[s[i]]; uint32_t e0 = expand(d0);
        d1 = (expand(d1) & m) | e0; d0 = e0 & m; if (!d1) return 3; }
    return d0 ? 0 : 1;
}

// weighted objective with early abort
static vector<int> ORD;          // evaluation order: by weight desc (positive first)
static vector<double> W;         // weights by state index
static vector<double> SUFPOS;    // suffix sum of positive weights in ORD order
static void setup_weights(const vector<int>& req, const vector<int>& forb, double bonus) {
    W.assign(NS, 0);
    for (int i = 0; i < NS; i++) W[i] = (double)ST[i].pop;
    for (int i : req) W[i] += bonus;
    for (int i : forb) W[i] = -bonus;
    ORD.resize(NS); iota(ORD.begin(), ORD.end(), 0);
    sort(ORD.begin(), ORD.end(), [](int a, int b) { return W[a] > W[b]; });
    SUFPOS.assign(NS + 1, 0);
    for (int k = NS - 1; k >= 0; k--) SUFPOS[k] = SUFPOS[k + 1] + max(0.0, W[ORD[k]]);
}
// returns objective, or -inf-ish (< thr) as soon as it cannot reach thr
static inline double obj(const Grid& G, double thr = -1e300) {
    double acc = 0;
    for (int k = 0; k < NS; k++) {
        if (acc + SUFPOS[k] < thr) return -1e300;
        int i = ORD[k];
        if (need(G.M, ST[i].s, 1) <= 1) acc += W[i];
    }
    return acc;
}
static ll true_score(const Grid& G, int* cnt = nullptr, string* list = nullptr) {
    ll s = 0; int c = 0;
    for (int i = 0; i < NS; i++) if (need(G.M, ST[i].s, 1) <= 1) { s += ST[i].pop; c++; if (list) { *list += ST[i].code; *list += ' '; } }
    if (cnt) *cnt = c;
    return s;
}

static vector<double> letter_weights() {
    vector<double> w(26, 0);
    for (auto& s : ST) for (int c : s.s) w[c] += (double)s.pop;
    double tot = 0; for (double x : w) tot += x;
    for (auto& x : w) x = 0.85 * x / tot + 0.15 / 26.0;
    return w;
}

static vector<int> parse_codes(const string& s) { vector<int> v; stringstream ss(s); string x; while (getline(ss, x, ',')) if (!x.empty()) v.push_back(idx_of(x)); return v; }

// exhaustive best-improvement polish at depth <= D over all squares (D<=2 full; D==3 full)
static Grid polish_full(Grid G, int depth) {
    double cur = obj(G);
    while (true) {
        double best = cur; vector<pair<int,int>> mv;
        for (int a = 0; a < 25; a++) { int oa = G.g[a];
            for (int ca = 0; ca < 26; ca++) { if (ca == oa) continue; G.set(a, ca);
                double o = obj(G, best + 0.5); if (o > best + 0.5) { best = o; mv = {{a, ca}}; }
                if (depth >= 2) for (int b = a + 1; b < 25; b++) { int ob = G.g[b];
                    for (int cb = 0; cb < 26; cb++) { if (cb == ob) continue; G.set(b, cb);
                        double o2 = obj(G, best + 0.5); if (o2 > best + 0.5) { best = o2; mv = {{a, ca}, {b, cb}}; } }
                    G.set(b, ob); }
            }
            G.set(a, oa);
        }
        if (mv.empty()) break;
        for (auto& m : mv) G.set(m.first, m.second);
        cur = obj(G);
    }
    return G;
}

struct SAO { ll iters = 5000000; int restarts = 1000000; double t0 = 3e6, t1 = 1e6, seconds = 0, noise = 0, pswap = 0.2; unsigned seed = 1; string start, log; bool pol = false; };

static void run_sa(const SAO& o) {
    mt19937_64 rng(o.seed * 1000003ULL + 12345);
    uniform_real_distribution<double> U(0, 1);
    auto lw = letter_weights(); discrete_distribution<int> letter(lw.begin(), lw.end());
    auto t_start = chrono::steady_clock::now();
    double gbest = -1e300; string gbs;
    for (int rs = 0; rs < o.restarts; rs++) {
        if (o.seconds > 0 && chrono::duration<double>(chrono::steady_clock::now() - t_start).count() > o.seconds) break;
        Grid G;
        if (!o.start.empty()) { G = parse(o.start); for (int i = 0; i < 25; i++) if (U(rng) < o.noise) G.g[i] = letter(rng); }
        else for (int i = 0; i < 25; i++) G.g[i] = letter(rng);
        G.build();
        double cur = obj(G); Grid bG = G; double bO = cur;
        double lr = log(o.t1 / o.t0);
        for (ll it = 0; it < o.iters; it++) {
            double T = o.t0 * exp(lr * (double)it / (double)o.iters);
            int a, b = -1, ca, cb = 0;
            if (U(rng) < o.pswap) { a = rng() % 25; b = rng() % 25; if (G.g[a] == G.g[b]) continue; ca = G.g[a]; cb = G.g[b]; G.set(a, cb); G.set(b, ca); }
            else { a = rng() % 25; int c = letter(rng); if (c == G.g[a]) continue; ca = G.g[a]; G.set(a, c); }
            double thr = cur + T * log(U(rng) + 1e-300);
            double no = obj(G, thr);
            if (no >= thr) { cur = no; if (cur > bO) { bO = cur; bG = G; } }
            else { if (b >= 0) { G.set(a, ca); G.set(b, cb); } else G.set(a, ca); }
        }
        if (o.pol) { bG = polish_full(bG, 2); bO = obj(bG); }
        int cnt; string lst; ll sc = true_score(bG, &cnt, &lst);
        bool imp = bO > gbest; if (imp) { gbest = bO; gbs = bG.str(); }
        printf("r%d %s score %lld states %d obj %.0f%s\n", rs, bG.str().c_str(), sc, cnt, bO, imp ? " *best*" : "");
        fflush(stdout);
        if (!o.log.empty()) { FILE* f = fopen(o.log.c_str(), "a"); if (f) { fprintf(f, "%s,%lld,%d,%.0f,%s\n", bG.str().c_str(), sc, cnt, bO, lst.c_str()); fclose(f); } }
    }
    printf("BEST %s obj %.0f score %lld\n", gbs.c_str(), gbest, gbs.empty() ? 0 : true_score(parse(gbs)));
}

// windowed exhaustive polish: all subsets of D squares inside some WxW window, letters from L
static Grid polishw(Grid G, int D, const vector<int>& L, int win, bool verbose) {
    // collect distinct subsets
    set<vector<int>> subs;
    for (int r0 = 0; r0 + win <= 5; r0++) for (int c0 = 0; c0 + win <= 5; c0++) {
        vector<int> sq; for (int r = r0; r < r0 + win; r++) for (int c = c0; c < c0 + win; c++) sq.push_back(r * 5 + c);
        int n = sq.size();
        vector<int> idx(D); function<void(int,int)> rec = [&](int k, int st) {
            if (k == D) { vector<int> v; for (int x : idx) v.push_back(sq[x]); sort(v.begin(), v.end()); subs.insert(v); return; }
            for (int j = st; j < n; j++) { idx[k] = j; rec(k + 1, j + 1); } };
        rec(0, 0);
    }
    if (verbose) { printf("polishw: %zu subsets of %d squares, %zu letters\n", subs.size(), D, L.size()); fflush(stdout); }
    double cur = obj(G);
    while (true) {
        double best = cur; vector<pair<int,int>> mv; ll evals = 0;
        for (auto& v : subs) {
            int orig[8]; for (int k = 0; k < D; k++) orig[k] = G.g[v[k]];
            // odometer over letters != original
            vector<int> pos(D, 0);
            function<void(int)> rec = [&](int k) {
                if (k == D) { evals++; double o = obj(G, best + 0.5); if (o > best + 0.5) { best = o; mv.clear(); for (int j = 0; j < D; j++) mv.push_back({v[j], G.g[v[j]]}); } return; }
                for (int c : L) { if (c == orig[k]) continue; G.set(v[k], c); rec(k + 1); }
                G.set(v[k], orig[k]);
            };
            rec(0);
        }
        if (verbose) { printf("  pass: %lld evals, best %.0f (cur %.0f)\n", evals, best, cur); fflush(stdout); }
        if (mv.empty()) break;
        for (auto& m : mv) G.set(m.first, m.second);
        cur = obj(G);
        G = polish_full(G, 2); cur = obj(G);
        if (verbose) { printf("  improved: %s obj %.0f score %lld\n", G.str().c_str(), cur, true_score(G)); fflush(stdout); }
    }
    return G;
}

// ruin & recreate: pick a WxW window (W=3) or a random set of k squares, randomise them,
// anneal only those squares (short), then polish depth-1/2 on the full board. Accept if >= current - slack.
static void run_rr(const string& start, double seconds, unsigned seed, const string& logf, ll inner, double slack) {
    mt19937_64 rng(seed * 7777ULL + 1);
    uniform_real_distribution<double> U(0, 1);
    auto lw = letter_weights(); discrete_distribution<int> letter(lw.begin(), lw.end());
    Grid G = parse(start); double cur = obj(G); Grid best = G; double bestO = cur;
    auto t0 = chrono::steady_clock::now(); ll it = 0;
    set<string> seenBest;
    while (chrono::duration<double>(chrono::steady_clock::now() - t0).count() < seconds) {
        it++;
        Grid H = G;
        vector<int> sq;
        int kind = rng() % 3;
        if (kind == 0) { int r0 = rng() % 3, c0 = rng() % 3; for (int r = r0; r < r0 + 3; r++) for (int c = c0; c < c0 + 3; c++) sq.push_back(r * 5 + c); }
        else if (kind == 1) { int r0 = rng() % 4, c0 = rng() % 2; for (int r = r0; r < r0 + 2; r++) for (int c = c0; c < c0 + 4; c++) sq.push_back(r * 5 + c); if (rng() & 1) for (auto& x : sq) x = (x % 5) * 5 + x / 5; }
        else { int k = 5 + rng() % 4; vector<int> all(25); iota(all.begin(), all.end(), 0); shuffle(all.begin(), all.end(), rng); sq.assign(all.begin(), all.begin() + k); }
        for (int s : sq) H.set(s, letter(rng));
        double c2 = obj(H); Grid bH = H; double bO = c2;
        double T0 = 3e6, T1 = 3e5, lr = log(T1 / T0);
        for (ll t = 0; t < inner; t++) {
            double T = T0 * exp(lr * t / (double)inner);
            int a = sq[rng() % sq.size()], ca = H.g[a];
            int b = -1, cb = 0;
            if (U(rng) < 0.2) { b = rng() % 25; if (H.g[b] == ca) continue; cb = H.g[b]; H.set(a, cb); H.set(b, ca); }
            else { int c = letter(rng); if (c == ca) continue; H.set(a, c); }
            double thr = c2 + T * log(U(rng) + 1e-300);
            double no = obj(H, thr);
            if (no >= thr) { c2 = no; if (c2 > bO) { bO = c2; bH = H; } }
            else { if (b >= 0) { H.set(a, ca); H.set(b, cb); } else H.set(a, ca); }
        }
        bH = polish_full(bH, 1); bO = obj(bH);
        if (bO >= cur - slack) { G = bH; cur = bO; }
        if (bO > bestO) {
            bestO = bO; best = bH;
            printf("[rr it %lld %.0fs] NEW BEST %s obj %.0f score %lld\n", it, chrono::duration<double>(chrono::steady_clock::now() - t0).count(), best.str().c_str(), bestO, true_score(best));
            fflush(stdout);
            if (!logf.empty()) { FILE* f = fopen(logf.c_str(), "a"); if (f) { fprintf(f, "%s,%lld,%.0f\n", best.str().c_str(), true_score(best), bestO); fclose(f); } }
        }
        if (it % 200 == 0) { printf("[rr it %lld] cur %.0f %s best %.0f\n", it, cur, G.str().c_str(), bestO); fflush(stdout); }
        // occasional reset to best
        if (it % 1000 == 0) { G = best; cur = bestO; }
    }
    printf("RR BEST %s obj %.0f score %lld\n", best.str().c_str(), bestO, true_score(best));
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "see source header\n"); return 1; }
    load();
    string mode = argv[1];
    SAO o; vector<int> req, forb; double bonus = 40e6; int depth = 4, win = 3; string letters = "acdeghiklmnorstuwy"; string grid;
    ll inner = 20000; double slack = 0;
    for (int i = 2; i < argc; i++) {
        string a = argv[i];
        auto nx = [&]() { return string(argv[++i]); };
        if (a == "--require") req = parse_codes(nx());
        else if (a == "--forbid") forb = parse_codes(nx());
        else if (a == "--bonus") bonus = stod(nx());
        else if (a == "--iters") o.iters = stoll(nx());
        else if (a == "--restarts") o.restarts = stoi(nx());
        else if (a == "--t0") o.t0 = stod(nx());
        else if (a == "--t1") o.t1 = stod(nx());
        else if (a == "--seed") o.seed = stoul(nx());
        else if (a == "--seconds") o.seconds = stod(nx());
        else if (a == "--start") o.start = nx();
        else if (a == "--noise") o.noise = stod(nx());
        else if (a == "--log") o.log = nx();
        else if (a == "--swap") o.pswap = stod(nx());
        else if (a == "--polish") o.pol = true;
        else if (a == "--depth") depth = stoi(nx());
        else if (a == "--win") win = stoi(nx());
        else if (a == "--letters") letters = nx();
        else if (a == "--inner") inner = stoll(nx());
        else if (a == "--slack") slack = stod(nx());
        else if (a[0] != '-') grid = a;
        else { fprintf(stderr, "bad opt %s\n", a.c_str()); return 1; }
    }
    setup_weights(req, forb, bonus);
    if (mode == "score") {
        Grid G = parse(grid); int c; string l; ll s = true_score(G, &c, &l);
        printf("%s score %lld states %d obj %.0f : %s\n", grid.c_str(), s, c, obj(G), l.c_str());
    } else if (mode == "need") {
        string g;
        while (cin >> g) { if (g.size() != 25) continue; Grid G = parse(g); printf("%s", g.c_str()); for (int i = 0; i < NS; i++) printf(" %d", need(G.M, ST[i].s, 2)); printf("\n"); }
    } else if (mode == "sa") run_sa(o);
    else if (mode == "polishw") {
        vector<int> L; for (char c : letters) L.push_back(c - 'a');
        Grid G = polishw(parse(grid), depth, L, win, true);
        printf("FINAL %s obj %.0f score %lld\n", G.str().c_str(), obj(G), true_score(G));
    } else if (mode == "rr") run_rr(grid, o.seconds, o.seed, o.log, inner, slack);
    return 0;
}
