// Training data for the learned move prior. Replays every tour of the given files, maps every state
// (alt + visits + pos) to the best final score of any known tour through it, and for every position on a
// tour writes one row per legal non-h8 pinned move: label = best score of a known tour through the child
// state (0 = no known tour), plus the move's features (prior.h).
//
// Split: --val-frac P puts a random share P of the tours (by --rng) in a validation set, --val-file FILE
// puts all tours of FILE there. Train positions = reached by some train tour, labelled with train tours
// only; val positions = reached only by val tours, labelled with all tours.
//
// usage: gen --out FILE [--val-frac P] [--val-file FILE]... [--rng S] TOURFILE...
// output (binary): int32 F, int32 nrows, int32 ngroups; ngroups x {int32 start, int32 count, int32 split,
//                  int32 moveidx, float Lmax}; nrows x {float label, float feats[F]}
#include "../../src/eng.h"
#include "prior.h"
#include <unordered_map>

struct Mv { int delta, q; };
struct Tour { vector<Mv> m; long long T; bool val; string src; };

static vector<Tour> read_tours(const string& fn, bool val) {
  vector<Tour> out; FILE* f = fopen(fn.c_str(), "r"); if (!f) { fprintf(stderr, "cannot open %s\n", fn.c_str()); exit(1); }
  static char line[400000];
  while (fgets(line, sizeof line, f)) {
    Tour t; t.val = val; t.src = fn; S s = start_state(); char* p = line; int w; char sq[8];
    bool bad = false;
    while ((p = strchr(p, '(')) && sscanf(p, "(%d, %2s)", &w, sq) == 2) {
      int q = (sq[0] - 'a') + 8 * (sq[1] - '1'); int n = group_n(s, s.pos); int d = w / max(1, n);
      if (d * max(1, n) != w && w) bad = true;
      t.m.push_back({d, q}); apply_move(s, d, q); p++;
    }
    if (t.m.size() > 1 && t.m.back().q == H8 && !bad) { t.T = s.T; out.push_back(t); }
  }
  fclose(f); return out;
}

int main(int argc, char** argv) {
  init_board(); prior_init();
  string out; bool check = false; double valFrac = 0; unsigned rng = 1; vector<string> files, valFiles;
  for (int i = 1; i < argc; i++) {
    string a = argv[i];
    if (a == "--out") out = argv[++i]; else if (a == "--check") check = true; else if (a == "--val-frac") valFrac = atof(argv[++i]);
    else if (a == "--val-file") valFiles.push_back(argv[++i]); else if (a == "--rng") rng = atoi(argv[++i]);
    else files.push_back(a);
  }
  if (check) {  // all_move_features == move_features on every position of every tour
    long long nm_ = 0, bad = 0; vector<pair<int, int>> mv; vector<float> A(PF * 64); float f[PF];
    for (auto& fn : files) for (auto& t : read_tours(fn, false)) {
      S s = start_state();
      for (auto& m : t.m) {
        pinned_moves(s, 3, true, mv);
        for (int ch = 0; ch < 2; ch++) {
          all_move_features(s, mv, A.data(), ch);
          for (size_t i = 0; i < mv.size(); i++) { if (mv[i].second == H8) continue; move_features(s, mv[i].first, mv[i].second, f, ch); nm_++;
            for (int k = 0; k < PF; k++) if (fabsf(f[k] - A[PF * i + k]) > 1e-6f) { if (bad++ < 10) fprintf(stderr, "mismatch feature %d: %g vs %g\n", k, f[k], A[PF * i + k]); } }
        }
        apply_move(s, m.delta, m.q);
      }
    }
    printf("check: %lld move feature vectors, %lld mismatches\n", nm_, bad); return bad != 0;
  }
  if (out.empty()) { fprintf(stderr, "usage: gen --out FILE [--val-frac P] [--val-file F] TOURFILES...\n"); return 1; }
  vector<Tour> tours; std::mt19937 g(rng);
  for (auto& f : files) { auto v = read_tours(f, false); for (auto& t : v) { t.val = std::uniform_real_distribution<double>(0, 1)(g) < valFrac; tours.push_back(t); } }
  for (auto& f : valFiles) { auto v = read_tours(f, true); for (auto& t : v) tours.push_back(t); }
  // drop duplicate tours (keep the first; a val copy of a train tour stays val only if listed with --val-file)
  {
    unordered_map<uint64_t, int> seen; vector<Tour> u;
    for (auto& t : tours) {
      uint64_t h = 1469598103934665603ULL; for (auto& m : t.m) h = (h ^ (m.q * 64 + m.delta + 1)) * 1099511628211ULL;
      auto it = seen.find(h);
      if (it == seen.end()) { seen[h] = (int)u.size(); u.push_back(t); }
      else if (t.val) u[it->second].val = true;  // explicit val file wins
    }
    tours.swap(u);
  }
  int nval = 0; long long best = 0; for (auto& t : tours) { nval += t.val; best = max(best, t.T); }
  fprintf(stderr, "%zu distinct tours (%d val), best %lld\n", tours.size(), nval, best);
  // best score through each state: train tours only, and all tours
  unordered_map<uint64_t, long long> Ltr, Lall; unordered_map<uint64_t, int> trainSeen;
  for (auto& t : tours) {
    S s = start_state();
    for (size_t i = 0; i <= t.m.size(); i++) {
      uint64_t h = hashS(s);
      auto& a = Lall[h]; a = max(a, t.T);
      if (!t.val) { auto& b = Ltr[h]; b = max(b, t.T); trainSeen[h] = 1; }
      if (i < t.m.size()) apply_move(s, t.m[i].delta, t.m[i].q);
    }
  }
  // positions
  FILE* fo = fopen(out.c_str(), "wb"); if (!fo) { fprintf(stderr, "cannot write %s\n", out.c_str()); return 1; }
  vector<int> gstart, gcount, gsplit, gidx; vector<float> gL; vector<float> rows;
  unordered_map<uint64_t, int> done; vector<pair<int, int>> mv; float f[PF];
  for (auto& t : tours) {
    S s = start_state();
    for (size_t i = 0; i + 1 < t.m.size(); i++) {  // the last move enters h8; no decision after it
      uint64_t h = hashS(s);
      if (!done.count(h)) {
        done[h] = 1;
        bool isTrain = trainSeen.count(h) > 0;
        auto& L = isTrain ? Ltr : Lall;
        pinned_moves(s, 3, true, mv);
        int cnt = 0; float Lmax = 0; int start = (int)rows.size() / (PF + 1);
        vector<float> tmp;
        for (auto& [d, q] : mv) {
          if (q == H8) continue;
          S c = s; apply_move(c, d, q); auto it = L.find(hashS(c)); float lab = it == L.end() ? 0.f : (float)it->second;
          move_features(s, d, q, f);
          tmp.push_back(lab); for (int k = 0; k < PF; k++) tmp.push_back(f[k]);
          Lmax = max(Lmax, lab); cnt++;
        }
        if (cnt >= 2 && Lmax > 0) {
          rows.insert(rows.end(), tmp.begin(), tmp.end());
          gstart.push_back(start); gcount.push_back(cnt); gsplit.push_back(isTrain ? 0 : 1); gidx.push_back((int)i); gL.push_back(Lmax);
        }
      }
      apply_move(s, t.m[i].delta, t.m[i].q);
    }
  }
  int F = PF, nr = (int)(rows.size() / (PF + 1)), ng = (int)gstart.size();
  fwrite(&F, 4, 1, fo); fwrite(&nr, 4, 1, fo); fwrite(&ng, 4, 1, fo);
  for (int i = 0; i < ng; i++) { fwrite(&gstart[i], 4, 1, fo); fwrite(&gcount[i], 4, 1, fo); fwrite(&gsplit[i], 4, 1, fo); fwrite(&gidx[i], 4, 1, fo); fwrite(&gL[i], 4, 1, fo); }
  fwrite(rows.data(), 4, rows.size(), fo); fclose(fo);
  int ntr = 0; for (int x : gsplit) ntr += x == 0;
  fprintf(stderr, "%d positions (%d train, %d val), %d rows -> %s\n", ng, ntr, ng - ntr, nr, out.c_str());
}
