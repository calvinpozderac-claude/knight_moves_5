// Exact branch-and-bound over a set of free squares ("region") of a 5x5 Altered States board.
// Free squares not yet assigned are WILDCARDS (match every letter); the score with wildcards is
// an upper bound on every completion, so subtrees whose bound <= incumbent are pruned.
//
//   bb region GRID CELLS...          optimise exactly the listed cells (0..24), rest fixed
//   bb lns GRID --k K [--seed S] [--secs T] [--mode win|rand|conn]
//                                     repeatedly optimise regions of K cells, accept improvements
//   bb full [--order ...]            whole-board B&B (for timing / bound experiments)
//
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
struct St { string name, spell; ll pop; vector<int> s; };
static vector<St> ST;
static vector<int> LET;  // letters that occur in names
static ll TOTAL = 0;

static void load() {
    ifstream f("data/states.csv");
    if (!f) { fprintf(stderr, "run from altered_states_2/\n"); exit(1); }
    string line; bool header = true;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (header) { header = false; continue; }
        stringstream ss(line); string name, pop;
        getline(ss, name, ','); getline(ss, pop, ',');
        St s; s.name = name; s.pop = atoll(pop.c_str());
        for (char c : name) if (isalpha(c)) { s.spell += toupper(c); s.s.push_back(toupper(c) - 'A'); }
        ST.push_back(s); TOTAL += s.pop;
    }
    sort(ST.begin(), ST.end(), [](const St& a, const St& b) { return a.pop > b.pop; });
    set<int> L; for (auto& s : ST) for (int c : s.s) L.insert(c);
    LET.assign(L.begin(), L.end());
    NOTC0 = NOTC4 = 0;
    for (int i = 0; i < 25; i++) { if (i % 5) NOTC0 |= 1u << i; if (i % 5 != 4) NOTC4 |= 1u << i; }
}

// board: cell letter 0..25 or -1 (wildcard)
struct Board {
    int b[25];
    uint32_t M[26];
    void build() {
        uint32_t wild = 0;
        for (int l = 0; l < 26; l++) M[l] = 0;
        for (int i = 0; i < 25; i++) { if (b[i] < 0) wild |= 1u << i; else M[b[i]] |= 1u << i; }
        for (int l = 0; l < 26; l++) M[l] |= wild;
    }
    void set(int i, int l) {
        int o = b[i];
        uint32_t bit = 1u << i;
        if (o < 0) { for (int k = 0; k < 26; k++) M[k] &= ~bit; } else M[o] &= ~bit;
        b[i] = l;
        if (l < 0) { for (int k = 0; k < 26; k++) M[k] |= bit; } else M[l] |= bit;
    }
};
static inline bool fits(const Board& B, const St& s) {
    uint32_t d0 = B.M[s.s[0]], d1 = FULL;
    for (size_t i = 1; i < s.s.size(); i++) {
        uint32_t m = B.M[s.s[i]];
        uint32_t e0 = expand(d0);
        d1 = (expand(d1) & m) | e0;
        d0 = e0 & m;
        if (!d1) return false;
    }
    return d1 != 0;
}
static ll score(const Board& B) {
    ll t = 0;
    for (auto& s : ST) if (fits(B, s)) t += s.pop;
    return t;
}
// score with early exit when it can no longer exceed `need` (returns value < need if pruned)
static ll score_bound(const Board& B, ll need) {
    ll t = 0, rest = TOTAL;
    for (auto& s : ST) {
        rest -= s.pop;
        if (fits(B, s)) t += s.pop;
        if (t + rest < need) return t + rest;
    }
    return t;
}
static string gstr(const Board& B) { string s; for (int i = 0; i < 25; i++) s += B.b[i] < 0 ? '?' : char('a' + B.b[i]); return s; }
static Board parse(const string& g) {
    Board B; for (int i = 0; i < 25; i++) B.b[i] = g[i] == '?' ? -1 : tolower(g[i]) - 'a'; B.build(); return B;
}

static ll nodes;
static ll incumbent;
static bool strict = true;
static Board bestB;
static vector<pair<ll,string>> ties;
static bool dynamic_order = true;
// live: indices of states that fit with wildcards (others are dead in the whole subtree)
// certain pop: states fitting with wildcards as blanks (fit in the whole subtree)
static ll eval_live(const Board& B, const vector<int>& live, ll base, ll need) {
    ll t = base, rest = 0;
    for (int i : live) rest += ST[i].pop;
    for (int i : live) {
        rest -= ST[i].pop;
        if (fits(B, ST[i])) t += ST[i].pop;
        if (t + rest < need) return t + rest;
    }
    return t;
}
// ---- stronger bound (B2): uncertain states need some free square, unaltered, with a usable letter.
// States whose usable free squares are a single square c compete for c's one letter.
static const int BLANK = 'Q' - 'A';
static bool use_b2 = false;
struct Info { ll bound, certain; vector<int> unc; uint32_t useful[25]; };
static void analyze(const Board& B, uint32_t fm, const vector<int>& live, ll certain0, Info& I, bool fast_need, ll need) {
    uint32_t Z[26];
    for (int l = 0; l < 26; l++) Z[l] = B.M[l] & ~fm;
    I.certain = certain0; I.unc.clear();
    for (int c = 0; c < 25; c++) I.useful[c] = 0;
    static ll group[25][26];
    for (int c = 0; c < 25; c++) if (fm >> c & 1) for (int l = 0; l < 26; l++) group[c][l] = 0;
    ll multi = 0;
    uint32_t f0[32], f1[32], g0[32], g1[32];
    ll rest = 0; for (int i : live) rest += ST[i].pop;
    for (int si : live) {
        const St& s = ST[si];
        rest -= s.pop;
        int L = s.s.size();
        f0[0] = B.M[s.s[0]]; f1[0] = FULL;
        bool ok = true;
        for (int i = 1; i < L; i++) {
            uint32_t m = B.M[s.s[i]], e0 = expand(f0[i - 1]);
            f1[i] = (expand(f1[i - 1]) & m) | e0; f0[i] = e0 & m;
            if (!f1[i]) { ok = false; break; }
        }
        if (!ok) goto next;
        {
            // blank fit
            uint32_t d0 = Z[s.s[0]], d1 = FULL;
            for (int i = 1; i < L; i++) { uint32_t m = Z[s.s[i]], e0 = expand(d0); d1 = (expand(d1) & m) | e0; d0 = e0 & m; }
            if (d1) { I.certain += s.pop; goto next; }
        }
        {
            g0[L - 1] = B.M[s.s[L - 1]]; g1[L - 1] = FULL;
            for (int i = L - 2; i >= 0; i--) {
                uint32_t m = B.M[s.s[i]], e0 = expand(g0[i + 1]);
                g1[i] = (expand(g1[i + 1]) & m) | e0; g0[i] = e0 & m;
            }
            uint32_t cellsUsed = 0; uint32_t letm[25];
            for (int i = 0; i < L; i++) {
                uint32_t P0 = i ? expand(f0[i - 1]) : FULL, P1 = i ? expand(f1[i - 1]) : FULL;
                uint32_t S0 = i < L - 1 ? expand(g0[i + 1]) : FULL, S1 = i < L - 1 ? expand(g1[i + 1]) : FULL;
                uint32_t U = B.M[s.s[i]] & fm & ((P0 & S1) | (P1 & S0));
                uint32_t nw = U & ~cellsUsed;
                while (nw) { int c = __builtin_ctz(nw); nw &= nw - 1; letm[c] = 0; }
                cellsUsed |= U;
                while (U) { int c = __builtin_ctz(U); U &= U - 1; letm[c] |= 1u << s.s[i]; }
            }
            if (!cellsUsed) goto next;  // cannot happen (would be certain)
            I.unc.push_back(si);
            uint32_t cu = cellsUsed;
            while (cu) { int c = __builtin_ctz(cu); cu &= cu - 1; I.useful[c] |= letm[c]; }
            if (__builtin_popcount(cellsUsed) == 1) {
                int c = __builtin_ctz(cellsUsed); uint32_t lm = letm[c];
                while (lm) { int l = __builtin_ctz(lm); lm &= lm - 1; group[c][l] += s.pop; }
            } else multi += s.pop;
        }
    next:;
    }
    ll g = 0;
    for (int c = 0; c < 25; c++) if (fm >> c & 1) { ll mx = 0; for (int l = 0; l < 26; l++) mx = max(mx, group[c][l]); g += mx; }
    I.bound = I.certain + multi + g;
}
static bool hybrid = false;
static void dfs(Board& B, vector<int>& freec, const vector<int>& live0, ll certain0) {
    nodes++;
    ll need = strict ? incumbent + 1 : incumbent;
    // recompute live / certain for this node
    vector<int> live; ll certain = certain0;
    {
        Board Z = B;  // wildcards -> blank
        // build masks with free squares as nothing
        for (int l = 0; l < 26; l++) Z.M[l] = B.M[l];
        uint32_t fm = 0; for (int c : freec) fm |= 1u << c;
        for (int l = 0; l < 26; l++) Z.M[l] &= ~fm;
        ll ub = certain0;
        for (int i : live0) {
            if (!fits(B, ST[i])) continue;
            ub += ST[i].pop;
            if (fits(Z, ST[i])) certain += ST[i].pop; else live.push_back(i);
        }
        if (ub < need) return;
    }
    static Info HI[32];
    uint32_t usefulm[25];
    for (int c = 0; c < 25; c++) usefulm[c] = ~0u;
    if (hybrid && !freec.empty()) {
        uint32_t fm = 0; for (int c : freec) fm |= 1u << c;
        Info& I = HI[freec.size()];
        analyze(B, fm, live, certain, I, false, need);
        if (I.bound < need) return;
        for (int c : freec) usefulm[c] = I.useful[c] | (1u << BLANK);
    }
    if (freec.empty()) {
        ll s = certain;
        if (s > incumbent) { incumbent = s; bestB = B; ties.clear(); }
        if (s == incumbent && !strict) ties.push_back({s, gstr(B)});
        return;
    }
    // choose cell
    int bestk = 0; vector<pair<ll,int>> bestch; ll bestkey = LLONG_MAX;
    int ncand = dynamic_order ? (int)freec.size() : 1;
    for (int k = 0; k < ncand; k++) {
        int c = freec[k];
        vector<pair<ll,int>> ch;
        ll key = 0;
        for (int l = 0; l < 26; l++) {
            if (!(usefulm[c] >> l & 1)) continue;
            if (!hybrid && find(LET.begin(), LET.end(), l) == LET.end()) continue;
            B.set(c, l);
            ll ub = eval_live(B, live, certain, need);
            if (ub >= need) { ch.push_back({ub, l}); key += 1; }
        }
        B.set(c, -1);
        if (key < bestkey) { bestkey = key; bestk = k; bestch = ch; if (key == 0) break; }
    }
    int c = freec[bestk];
    freec.erase(freec.begin() + bestk);
    sort(bestch.rbegin(), bestch.rend());
    for (auto& [ub, l] : bestch) {
        need = strict ? incumbent + 1 : incumbent;
        if (ub < need) break;
        B.set(c, l);
        dfs(B, freec, live, certain);
    }
    B.set(c, -1);
    freec.insert(freec.begin() + bestk, c);
}

static void dfs2(Board& B, vector<int>& freec, const vector<int>& live0, ll certain0) {
    nodes++;
    ll need = strict ? incumbent + 1 : incumbent;
    uint32_t fm = 0; for (int c : freec) fm |= 1u << c;
    Info I; analyze(B, fm, live0, certain0, I, false, need);
    if (I.bound < need) return;
    if (freec.empty()) {
        ll s = I.certain;
        if (s > incumbent) { incumbent = s; bestB = B; ties.clear(); }
        if (s == incumbent && !strict) ties.push_back({s, gstr(B)});
        return;
    }
    if (I.unc.empty()) {  // free squares irrelevant: fill blanks
        for (int c : freec) B.set(c, BLANK);
        ll s = I.certain;
        if (s > incumbent) { incumbent = s; bestB = B; ties.clear(); }
        if (s == incumbent && !strict) ties.push_back({s, gstr(B)});
        for (int c : freec) B.set(c, -1);
        return;
    }
    int bestk = -1; vector<pair<ll,int>> bestch; ll bestkey = LLONG_MAX;
    Info J;
    for (int k = 0; k < (int)freec.size(); k++) {
        int c = freec[k];
        uint32_t lm = I.useful[c] | (1u << BLANK);
        vector<pair<ll,int>> ch; ll key = 0;
        uint32_t fm2 = fm & ~(1u << c);
        while (lm) {
            int l = __builtin_ctz(lm); lm &= lm - 1;
            B.set(c, l);
            analyze(B, fm2, I.unc, I.certain, J, false, need);
            if (J.bound >= need) { ch.push_back({J.bound, l}); key++; }
        }
        B.set(c, -1);
        if (key < bestkey) { bestkey = key; bestk = k; bestch = ch; if (key == 0) break; }
        if (!dynamic_order) break;
    }
    int c = freec[bestk];
    freec.erase(freec.begin() + bestk);
    sort(bestch.rbegin(), bestch.rend());
    for (auto& [ub, l] : bestch) {
        need = strict ? incumbent + 1 : incumbent;
        if (ub < need) break;
        B.set(c, l);
        dfs2(B, freec, I.unc, I.certain);
    }
    B.set(c, -1);
    freec.insert(freec.begin() + bestk, c);
}

// optimise region exactly; returns best board (strictly better than `target` if found)
static ll solve_region(Board B, vector<int> cells, ll target, Board& out, ll& nn) {
    for (int c : cells) B.set(c, -1);
    // order cells: most constrained first (by bound reduction when set to its best letter) - keep given order
    incumbent = target; nodes = 0; bestB = B;
    ll ub = score(B);
    if (ub <= target && strict) { nn = 1; out = B; return target; }
    vector<int> live(ST.size()); iota(live.begin(), live.end(), 0);
    if (use_b2) dfs2(B, cells, live, 0); else dfs(B, cells, live, 0);
    nn = nodes; out = bestB;
    return incumbent;
}

int main(int argc, char** argv) {
    load();
    if (argc < 2) { fprintf(stderr, "usage\n"); return 1; }
    string mode = argv[1];
    if (mode == "score") {
        Board B = parse(argv[2]);
        printf("%lld\n", score(B));
        for (auto& s : ST) if (fits(B, s)) printf("%s ", s.spell.c_str());
        printf("\n");
        return 0;
    }
    if (mode == "region") {
        Board B = parse(argv[2]);
        vector<int> cells;
        ll target = -1;
        for (int i = 3; i < argc; i++) {
            string a = argv[i];
            if (a == "--ties") strict = false;
            else if (a == "--static") dynamic_order = false;
            else if (a == "--b2") use_b2 = true;
            else if (a == "--hybrid") hybrid = true;
            else if (a == "--target") target = atoll(argv[++i]);
            else cells.push_back(atoi(argv[i]));
        }
        ll base = score(B);
        if (target < 0) target = base;
        auto t0 = chrono::steady_clock::now();
        Board out; ll nn;
        ll r = solve_region(B, cells, target, out, nn);
        double dt = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        printf("base %lld best %lld %s nodes %lld time %.2f\n", base, r, r > target ? gstr(out).c_str() : "-", nn, dt);
        if (!strict) for (auto& t : ties) printf("tie %lld %s\n", t.first, t.second.c_str());
        return 0;
    }
    if (mode == "allsub") {
        // every k-subset of squares (optionally restricted to subsets of diameter <= D)
        Board B = parse(argv[2]);
        int K = atoi(argv[3]); ll target = -1; ll prog = 2000; int part = 0, nparts = 1; int maxspan = 99;
        for (int i = 4; i < argc; i++) {
            string a = argv[i];
            if (a == "--target") target = atoll(argv[++i]);
            else if (a == "--b2") use_b2 = true;
            else if (a == "--hybrid") hybrid = true;
            else if (a == "--part") { part = atoi(argv[++i]); nparts = atoi(argv[++i]); }
            else if (a == "--span") maxspan = atoi(argv[++i]);
            else if (a == "--prog") prog = atoll(argv[++i]);
        }
        ll base = score(B);
        if (target < 0) target = base;
        printf("allsub k=%d base %lld target %lld %s\n", K, base, target, gstr(B).c_str()); fflush(stdout);
        auto t0 = chrono::steady_clock::now();
        vector<int> idx(K); iota(idx.begin(), idx.end(), 0);
        ll cnt = 0, done = 0, totn = 0, found = 0;
        while (true) {
            int r0 = 9, r1 = -1, c0 = 9, c1 = -1;
            for (int c : idx) { r0 = min(r0, c / 5); r1 = max(r1, c / 5); c0 = min(c0, c % 5); c1 = max(c1, c % 5); }
            if (max(r1 - r0, c1 - c0) <= maxspan && (cnt++ % nparts) == part) {
                Board out; ll nn;
                ll r = solve_region(B, idx, target, out, nn);
                done++; totn += nn;
                if (r > target) { found++; printf("BETTER %lld %s cells", r, gstr(out).c_str()); for (int c : idx) printf(" %d", c); printf("\n"); fflush(stdout); }
                if (done % prog == 0) { printf("done %lld nodes %lld t %.0f\n", done, totn, chrono::duration<double>(chrono::steady_clock::now() - t0).count()); fflush(stdout); }
            }
            int i = K - 1;
            while (i >= 0 && idx[i] == 25 - K + i) i--;
            if (i < 0) break;
            idx[i]++;
            for (int j = i + 1; j < K; j++) idx[j] = idx[j - 1] + 1;
        }
        printf("finished subsets %lld nodes %lld found %lld time %.1f\n", done, totn, found, chrono::duration<double>(chrono::steady_clock::now() - t0).count());
        return 0;
    }
    if (mode == "lns") {
        Board B = parse(argv[2]);
        int K = 8; unsigned seed = 1; double secs = 60; string rmode = "conn";
        for (int i = 3; i < argc; i++) {
            string a = argv[i];
            if (a == "--k") K = atoi(argv[++i]);
            else if (a == "--seed") seed = atoi(argv[++i]);
            else if (a == "--secs") secs = atof(argv[++i]);
            else if (a == "--mode") rmode = argv[++i];
            else if (a == "--b2") use_b2 = true;
            else if (a == "--hybrid") hybrid = true;
        }
        mt19937 rng(seed);
        ll cur = score(B);
        printf("start %lld %s\n", cur, gstr(B).c_str()); fflush(stdout);
        auto t0 = chrono::steady_clock::now();
        ll it = 0, totn = 0;
        while (chrono::duration<double>(chrono::steady_clock::now() - t0).count() < secs) {
            vector<int> cells;
            if (rmode == "rand") {
                vector<int> p(25); iota(p.begin(), p.end(), 0); shuffle(p.begin(), p.end(), rng);
                cells.assign(p.begin(), p.begin() + K);
            } else {  // connected random region grown from a random square
                set<int> S; S.insert(rng() % 25);
                while ((int)S.size() < K) {
                    vector<int> fr;
                    for (int c : S) { int r = c / 5, q = c % 5;
                        for (int dr = -1; dr <= 1; dr++) for (int dq = -1; dq <= 1; dq++) {
                            int rr = r + dr, qq = q + dq;
                            if (rr < 0 || rr > 4 || qq < 0 || qq > 4) continue;
                            if (!S.count(rr * 5 + qq)) fr.push_back(rr * 5 + qq);
                        } }
                    S.insert(fr[rng() % fr.size()]);
                }
                cells.assign(S.begin(), S.end());
                shuffle(cells.begin(), cells.end(), rng);
            }
            Board out; ll nn;
            ll r = solve_region(B, cells, cur, out, nn);
            it++; totn += nn;
            if (r > cur) {
                cur = r; B = out;
                printf("IMPROVED %lld %s it %lld\n", cur, gstr(B).c_str(), it); fflush(stdout);
            }
            if (it % 50 == 0) { printf("it %lld nodes %lld t %.0f cur %lld\n", it, totn,
                chrono::duration<double>(chrono::steady_clock::now() - t0).count(), cur); fflush(stdout); }
        }
        printf("end %lld %s it %lld\n", cur, gstr(B).c_str(), it);
        return 0;
    }
    return 0;
}
