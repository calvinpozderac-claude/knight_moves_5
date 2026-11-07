// Shared helpers for the Eldrow search programs: word list, Wordle feedback, chain checking.
#pragma once
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eldrow {

constexpr int kAllGreen = 242;  // 2 + 2*3 + 2*9 + 2*27 + 2*81

// Feedback of guess g against target t, coded base 3 (0 gray, 1 yellow, 2 green), position 0
// is the least significant digit. Greens first, then yellows left to right, counting duplicates.
inline int feedback(const std::string& g, const std::string& t) {
  int res[5] = {0, 0, 0, 0, 0};
  int cnt[26] = {0};
  for (int i = 0; i < 5; i++) {
    if (g[i] == t[i]) res[i] = 2;
    else cnt[t[i] - 'A']++;
  }
  for (int i = 0; i < 5; i++) {
    if (res[i] != 2 && cnt[g[i] - 'A'] > 0) {
      res[i] = 1;
      cnt[g[i] - 'A']--;
    }
  }
  int code = 0;
  for (int i = 4; i >= 0; i--) code = code * 3 + res[i];
  return code;
}

inline std::string feedbackString(int code) {
  std::string s;
  for (int i = 0; i < 5; i++) {
    s += "-YG"[code % 3];
    code /= 3;
  }
  return s;
}

struct Words {
  std::vector<std::string> w;
  std::vector<uint8_t> fb;  // fb[g * n + t]
  int n = 0;

  explicit Words(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::string s;
    while (in >> s) {
      for (char& c : s) c = (char)toupper((unsigned char)c);
      if (s.size() != 5) throw std::runtime_error("bad word " + s);
      w.push_back(s);
    }
    std::sort(w.begin(), w.end());
    w.erase(std::unique(w.begin(), w.end()), w.end());
    n = (int)w.size();
    fb.resize((size_t)n * n);
    for (int g = 0; g < n; g++)
      for (int t = 0; t < n; t++) fb[(size_t)g * n + t] = (uint8_t)feedback(w[g], w[t]);
  }

  int at(int g, int t) const { return fb[(size_t)g * n + t]; }

  int index(const std::string& s) const {
    auto it = std::lower_bound(w.begin(), w.end(), s);
    if (it == w.end() || *it != s) return -1;
    return (int)(it - w.begin());
  }
};

// Checks a chain under the puzzle's hard-mode rule: every guess must be a word that could
// still be the target given all earlier feedback, and the last word is the target. Returns ""
// if valid, else a message naming the first problem.
inline std::string checkChain(const Words& W, const std::vector<std::string>& chain) {
  if (chain.empty()) return "empty chain";
  std::vector<int> ix;
  for (const auto& s : chain) {
    int i = W.index(s);
    if (i < 0) return s + " is not in the word list";
    ix.push_back(i);
  }
  int T = ix.back();
  for (size_t i = 0; i + 1 < ix.size(); i++)
    if (ix[i] == T) return "target " + chain.back() + " guessed early at position " +
                           std::to_string(i + 1);
  for (size_t i = 1; i < ix.size(); i++)
    for (size_t j = 0; j < i; j++)
      if (W.at(ix[j], ix[i]) != W.at(ix[j], T))
        return chain[i] + " (guess " + std::to_string(i + 1) + ") contradicts the feedback " +
               feedbackString(W.at(ix[j], T)) + " on " + chain[j] + " (guess " +
               std::to_string(j + 1) + ")";
  return "";
}

inline std::vector<std::string> splitChain(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == ',' || c == ' ' || c == '\n') {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur += (char)toupper((unsigned char)c);
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

}  // namespace eldrow
