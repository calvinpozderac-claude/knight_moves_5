// Independent re-check of the Eldrow optimum (shares no code with search.cpp / eldrow.hpp).
//
// usage: exhaust <words.txt> <feedback.bin> <K> <bound: none|letters> [threads]
//
// feedback.bin is the N*N byte matrix written by fbmatrix.py from verify.py's mark()
// (row = guess, column = target, words in sorted order); only equality of bytes is used.
//
// Decides whether a chain of K words exists. A state is the set S of words consistent with
// every clue so far (exactly the legal next guesses, and the possible targets). Guess w in S
// with the target in class P = {x in S, x != w : fb(w,x) = fb(w,target)} moves to state P.
// "chain of length >= need from S" is decided by plain recursion over every w in S and every
// class P. Pruning:
//   always:          a chain from S has at most |S| words (each guess leaves the state).
//   bound=letters:   at most max over positions p of the number of distinct letters at p in S.
// The memo is keyed by the exact member list of S (no hashing shortcuts) and remembers the
// smallest need proven impossible for S. It is only a cache: it is cleared when it grows too
// big, which costs time but never correctness.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

static int N;
static std::vector<std::string> names;
static std::vector<unsigned char> FB;
static bool useLetters = false;
static std::atomic<bool> found{false};
static std::mutex outMu;

static int letterBound(const std::vector<uint16_t>& S) {
  int best = 0;
  for (int p = 0; p < 5; p++) {
    bool seen[26] = {false};
    int c = 0;
    for (uint16_t x : S) {
      int l = names[x][p] - 'A';
      if (!seen[l]) { seen[l] = true; c++; }
    }
    best = std::max(best, c);
  }
  return best;
}

struct Solver {
  std::unordered_map<std::string, int> failMemo;  // exact key -> smallest need proven impossible
  size_t memoCap = 6000000;
  uint64_t expansions = 0;
  std::vector<uint16_t> path;  // guesses on the current branch (for printing a success)

  static std::string key(const std::vector<uint16_t>& S) {
    return std::string((const char*)S.data(), S.size() * sizeof(uint16_t));
  }

  bool reach(const std::vector<uint16_t>& S, int need) {
    if (found.load(std::memory_order_relaxed)) return true;
    if (need <= 1) {
      if (!S.empty()) {
        std::lock_guard<std::mutex> g(outMu);
        std::cout << "CHAIN FOUND:";
        for (uint16_t w : path) std::cout << " " << names[w];
        std::cout << " " << names[S[0]] << std::endl;
        found = true;
      }
      return !S.empty();
    }
    if ((int)S.size() < need) return false;
    if (useLetters && letterBound(S) < need) return false;
    std::string k = key(S);
    auto it = failMemo.find(k);
    if (it != failMemo.end() && it->second <= need) return false;
    expansions++;
    // Every legal guess w, every feedback class of the remaining words.
    std::vector<std::vector<uint16_t>> cls(243);
    for (uint16_t w : S) {
      for (auto& c : cls) c.clear();
      const unsigned char* row = &FB[(size_t)w * N];
      for (uint16_t x : S)
        if (x != w) cls[row[x]].push_back(x);
      path.push_back(w);
      for (int c = 0; c < 243; c++) {
        if ((int)cls[c].size() < need - 1) continue;
        // cls belongs to this call only, so deeper calls cannot disturb it.
        if (reach(cls[c], need - 1)) { path.pop_back(); return true; }
      }
      path.pop_back();
    }
    if (failMemo.size() >= memoCap) failMemo.clear();
    auto& slot = failMemo[k];
    if (slot == 0 || need < slot) slot = need;
    return false;
  }
};

int main(int argc, char** argv) {
  if (argc < 5) {
    std::cerr << "usage: exhaust <words.txt> <feedback.bin> <K> <none|letters> [threads]\n";
    return 2;
  }
  {
    std::ifstream in(argv[1]);
    std::string s;
    while (in >> s) names.push_back(s);
    std::sort(names.begin(), names.end());
  }
  N = (int)names.size();
  {
    std::ifstream in(argv[2], std::ios::binary);
    FB.assign((size_t)N * N, 0);
    in.read((char*)FB.data(), FB.size());
    if ((size_t)in.gcount() != FB.size()) { std::cerr << "feedback.bin has wrong size\n"; return 2; }
  }
  int K = atoi(argv[3]);
  useLetters = std::string(argv[4]) == "letters";
  int threads = argc > 5 ? atoi(argv[5]) : 4;
  std::cout << N << " words, K=" << K << ", bound=" << (useLetters ? "letters" : "none (only |S|)")
            << ", threads=" << threads << std::endl;

  // Root: every word is a legal first guess. Hand first guesses to threads one at a time.
  std::atomic<int> next{0}, done{0};
  std::atomic<uint64_t> totalExp{0};
  auto t0 = std::chrono::steady_clock::now();
  auto worker = [&]() {
    Solver sv;
    std::vector<std::vector<uint16_t>> cls(243);
    for (;;) {
      int w = next++;
      if (w >= N || found) break;
      for (auto& c : cls) c.clear();
      for (int x = 0; x < N; x++)
        if (x != w) cls[FB[(size_t)w * N + x]].push_back((uint16_t)x);
      sv.path.assign(1, (uint16_t)w);
      for (int c = 0; c < 243 && !found; c++)
        if ((int)cls[c].size() >= K - 1) sv.reach(cls[c], K - 1);
      int d = ++done;
      if (d % 100 == 0 || d == N) {
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::lock_guard<std::mutex> g(outMu);
        std::cout << "first guesses done " << d << "/" << N << "  t=" << sec << "s" << std::endl;
      }
    }
    totalExp += sv.expansions;
  };
  std::vector<std::thread> ts;
  for (int i = 0; i < threads; i++) ts.emplace_back(worker);
  for (auto& t : ts) t.join();
  double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::cout << (found ? "RESULT: a chain of " : "RESULT: NO chain of ") << K
            << " words exists. expansions=" << totalExp << " t=" << sec << "s" << std::endl;
  return found ? 0 : 1;
}
