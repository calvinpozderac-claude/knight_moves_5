// Validates Eldrow chains: check <words.txt> "W1,W2,...,TARGET" ...
// Prints the feedback grid for each chain and whether it obeys the hard-mode rule.
#include "eldrow.hpp"

using namespace eldrow;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: check <words.txt> CHAIN [CHAIN...]\n";
    return 2;
  }
  Words W(argv[1]);
  int bad = 0;
  for (int a = 2; a < argc; a++) {
    auto chain = splitChain(argv[a]);
    std::string msg = checkChain(W, chain);
    std::cout << chain.size() << " words: " << argv[a] << "\n";
    if (msg.empty()) {
      int T = W.index(chain.back());
      for (auto& s : chain) std::cout << "  " << s << "  " << feedbackString(W.at(W.index(s), T)) << "\n";
      std::cout << "  VALID\n";
    } else {
      std::cout << "  INVALID: " << msg << "\n";
      bad++;
    }
  }
  return bad ? 1 : 0;
}
