// Exact branch-and-bound search for the longest Eldrow chain.
//
// A state is the set S of words still consistent with every clue so far. Guessing w in S
// splits S \ {w} into feedback classes; the next state is one of those classes (the class
// the chosen target falls in). f(S) = longest chain playable from S, counting the final
// (target) guess: f(S) = max(1, 1 + max over w in S and classes P of f(P)).
//
// Upper bound used for pruning: in a chain w_1..w_m = T inside S, the last non-target guess
// w_{m-1} is non-green at some position p. A letter that was ever green at p stays there, and
// a non-green letter at p can never be played at p again, so w_1..w_{m-1} all have distinct
// letters at p, none of them T[p]. Hence m <= (number of distinct letters at p among S),
// and f(S) <= min(|S|, max_p L_p(S)). (With S = all words this gives the puzzle's 26.)
//
// usage: search <words.txt> [start_K] [max_K]
//   Proves, for K = start_K, start_K+1, ..., whether a chain of K words exists, printing one
//   when it does, until a K fails (that K-1 is then the optimum) or max_K is reached.
#include <chrono>
#include <unordered_map>

#include "eldrow.hpp"

using namespace eldrow;

static const Words* WP;
static int N;

typedef std::vector<uint16_t> Set;

struct Info {
  uint8_t lo = 1, hi = 255;  // lo <= f(S) <= hi
  uint16_t bestW = 0;        // a guess realising lo (when lo >= 2)
  uint8_t bestCode = 0;
};

struct Key {
  uint64_t a, b;
  bool operator==(const Key& o) const { return a == o.a && b == o.b; }
};
struct KeyHash {
  size_t operator()(const Key& k) const { return k.a ^ (k.b * 0x9E3779B97F4A7C15ULL); }
};

static std::unordered_map<Key, Info, KeyHash> memo;
static uint64_t nodes = 0, expansions = 0;

static inline uint64_t mix(uint64_t x) {
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27; x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return x;
}

static Key keyOf(const Set& S) {
  uint64_t a = 0x12345678abcdefULL ^ S.size(), b = 0xfedcba9876543ULL + S.size() * 7;
  for (uint16_t x : S) {
    a = mix(a + x + 1);
    b = mix(b ^ ((uint64_t)x * 0x9E3779B97F4A7C15ULL + 12345));
  }
  return {a, b};
}

static uint32_t letterMask[5][26];

static int upperBound(const Set& S) {
  uint32_t m[5] = {0, 0, 0, 0, 0};
  for (uint16_t x : S) {
    const std::string& s = WP->w[x];
    for (int p = 0; p < 5; p++) m[p] |= 1u << (s[p] - 'A');
  }
  int b = 0;
  for (int p = 0; p < 5; p++) b = std::max(b, __builtin_popcount(m[p]));
  return std::min<int>((int)S.size(), b);
}

// Splits S \ {w} by feedback of guess w. Returns the class codes in order of decreasing size.
static void partition(const Set& S, int w, std::vector<Set>& buckets, std::vector<int>& codes) {
  for (auto& b : buckets) b.clear();
  const uint8_t* row = &WP->fb[(size_t)w * N];
  for (uint16_t x : S)
    if (x != w) buckets[row[x]].push_back(x);
  codes.clear();
  for (int c = 0; c < 243; c++)
    if (!buckets[c].empty()) codes.push_back(c);
}

// Is f(S) >= need ?
static bool reach(const Set& S, int need) {
  nodes++;
  if (need <= 1) return !S.empty();
  if ((int)S.size() < need) return false;
  Key k = keyOf(S);
  auto it = memo.find(k);
  if (it != memo.end()) {
    if (it->second.lo >= need) return true;
    if (it->second.hi < need) return false;
  }
  if (upperBound(S) < need) return false;
  expansions++;
  std::vector<Set> buckets(243);
  std::vector<int> codes;
  // Gather candidate (w, class) children whose bound allows need-1, then try the most
  // promising (largest bound, then largest class) first.
  struct Child { int w, code, ub, size; Set set; };
  std::vector<Child> kids;
  for (uint16_t w : S) {
    partition(S, w, buckets, codes);
    for (int c : codes) {
      const Set& B = buckets[c];
      if ((int)B.size() < need - 1) continue;
      int ub = upperBound(B);
      if (ub < need - 1) continue;
      kids.push_back({w, c, ub, (int)B.size(), B});
    }
  }
  std::sort(kids.begin(), kids.end(), [](const Child& a, const Child& b) {
    if (a.ub != b.ub) return a.ub > b.ub;
    return a.size > b.size;
  });
  for (auto& ch : kids) {
    if (reach(ch.set, need - 1)) {
      Info& inf = memo[k];
      if (inf.lo < need) {
        inf.lo = (uint8_t)need;
        inf.bestW = (uint16_t)ch.w;
        inf.bestCode = (uint8_t)ch.code;
      }
      return true;
    }
  }
  Info& inf = memo[k];
  inf.hi = std::min<int>(inf.hi, need - 1);
  return false;
}

static std::vector<std::string> reconstruct(Set S, int need) {
  std::vector<std::string> chain;
  while (need > 1) {
    Info& inf = memo[keyOf(S)];
    int w = inf.bestW, c = inf.bestCode;
    chain.push_back(WP->w[w]);
    Set B;
    for (uint16_t x : S)
      if (x != w && WP->at(w, x) == c) B.push_back(x);
    S = B;
    need--;
  }
  chain.push_back(WP->w[S[0]]);
  return chain;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: search <words.txt> [start_K] [max_K]\n";
    return 2;
  }
  Words W(argv[1]);
  WP = &W;
  N = W.n;
  int K = argc > 2 ? atoi(argv[2]) : 2;
  int maxK = argc > 3 ? atoi(argv[3]) : 26;
  Set all(N);
  for (int i = 0; i < N; i++) all[i] = (uint16_t)i;
  std::cout << N << " words, upper bound " << upperBound(all) << "\n";
  auto t0 = std::chrono::steady_clock::now();
  for (; K <= maxK; K++) {
    bool ok = reach(all, K);
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "K=" << K << (ok ? " FOUND" : " IMPOSSIBLE") << "  nodes=" << nodes
              << " expansions=" << expansions << " memo=" << memo.size() << " t=" << sec << "s"
              << std::endl;
    if (!ok) {
      std::cout << "optimum = " << K - 1 << " (if start_K was itself feasible)\n";
      break;
    }
    auto chain = reconstruct(all, K);
    std::string s;
    for (auto& x : chain) s += (s.empty() ? "" : ",") + x;
    std::string msg = checkChain(W, chain);
    std::cout << "  " << s << "  " << (msg.empty() ? "valid" : "INVALID: " + msg) << std::endl;
  }
  return 0;
}
