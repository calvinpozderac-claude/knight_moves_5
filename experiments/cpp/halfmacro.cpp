// Macro BFS with altitudes scaled by SC (allows 1/SC-unit sinks), exact visit tracking.
// usage: halfmacro SC Bnum Bden maxStatesPerLevel   (max wait <= Bnum/Bden minutes)
#include "eng.h"
int SC; long long BN, BD; size_t CAP;
struct St { S s; int par; vector<pair<string,string>> mv; };
int main(int argc, char** argv) {
  init_board();
  SC = atoi(argv[1]); BN = atoll(argv[2]); BD = atoll(argv[3]); CAP = atoll(argv[4]);
  S s0 = start_state(); for (int i = 0; i < 64; i++) s0.alt[i] *= SC;
  vector<vector<St>> L; L.push_back({{s0, -1, {}}});
  unordered_set<uint64_t> seen; seen.insert(hashS(s0));
  auto legal = [&](int t, int dz) { return legal_dz(t, dz % SC == 0 ? dz / SC : 99); };
  for (int lev = 0; lev < 60; lev++) {
    vector<St> nxt;
    for (int i = 0; i < (int)L.back().size(); i++) {
      const S& s = L.back()[i].s;
      // BFS static
      int prev[64]; for (int k = 0; k < 64; k++) prev[k] = -2; prev[s.pos] = -1; int qu[64], h = 0, t = 0; qu[t++] = s.pos;
      while (h < t) { int u = qu[h++]; for (int k = 0; k < NBN[u]; k++) { int q = NBQ[u][k];
        if (!legal(NBT[u][k], s.alt[q] - s.alt[u])) continue;
        if (q == H8) { // goal
          vector<pair<string,string>> out; int li = (int)L.size() - 1, ii = i;
          while (li >= 0 && ii >= 0) { auto& st = L[li][ii]; out.insert(out.begin(), st.mv.begin(), st.mv.end()); ii = st.par; li--; }
          vector<int> path; int x = u; while (x != s.pos) { path.push_back(x); x = prev[x]; } reverse(path.begin(), path.end());
          for (int p : path) out.push_back({"0", nm(p)}); out.push_back({"0", "h8"});
          printf("FOUND "); for (size_t j = 0; j < out.size(); j++) printf("%s(%s, %s)", j ? ", " : "", out[j].first.c_str(), out[j].second.c_str()); printf("\n");
          return 0; }
        if (prev[q] != -2 || s.vis[q] >= 3) continue; prev[q] = u; qu[t++] = q; } }
      for (int P = 0; P < 64; P++) {
        if (prev[P] == -2) continue;
        S w = s; vector<pair<string,string>> walk; vector<int> path; int x = P;
        while (x != s.pos) { path.push_back(x); x = prev[x]; } reverse(path.begin(), path.end());
        for (int p : path) { w.vis[p]++; w.pos = p; walk.push_back({"0", nm(p)}); }
        int n = group_n(w, P);
        for (int k = 1; (long long)n * k * BD <= BN * SC; k++) {
          S c0 = w; apply_move(c0, k, P); c0.vis[P]--; c0.pos = P;  // apply sink only
          for (int k2 = 0; k2 < NBN[P]; k2++) {
            int q = NBQ[P][k2]; if (q == H8 || c0.vis[q] >= 3) continue;
            if (!legal(NBT[P][k2], c0.alt[q] - c0.alt[P])) continue;
            S c = c0; c.vis[q]++; c.pos = q;
            uint64_t hh = hashS(c); if (seen.count(hh)) continue; seen.insert(hh);
            vector<pair<string,string>> mv = walk;
            long long num = (long long)n * k, den = SC; long long g = __gcd(num, den);
            string ts = (den / g == 1) ? to_string(num / g) : to_string(num / g) + "/" + to_string(den / g);
            mv.push_back({ts, nm(q)});
            nxt.push_back({c, i, mv});
            if (nxt.size() >= CAP) break;
          }
          if (nxt.size() >= CAP) break;
        }
        if (nxt.size() >= CAP) break;
      }
      if (nxt.size() >= CAP) break;
    }
    fprintf(stderr, "level %d states %zu\n", lev + 1, nxt.size());
    if (nxt.empty()) { printf("none\n"); return 0; }
    L.push_back(std::move(nxt));
  }
  printf("none (level limit)\n");
}
