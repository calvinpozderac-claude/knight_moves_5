// Depth-first branch-and-bound over pinned moves (eng.h) for the suffix of a tour.
// usage: bb TOURFILE K [--alpha A] [--time SEC] [--tt LOG2] [--nobound] [--check] [--out FILE]
#include "common.h"
#include <chrono>
static const int NEG = -1000000000;
static int ALPHA; static bool NOBOUND=false, CHECK=false; static double TLIM=1e18;
static long long nodes=0, viol=0, checked=0; static chrono::steady_clock::time_point t0;
static bool timed_out=false;
static vector<pair<int,int>> prefixW, tourW, stackW; static string OUTF;
static int KPRE;
struct TTE { uint64_t key; int32_t ub; };
static vector<TTE> TT; static uint64_t TTMASK;
static inline uint64_t keyS(const S& s){
  uint64_t h = 1469598103934665603ULL ^ (s.pos*0x9E3779B97F4A7C15ULL);
  AT b = s.alt[s.pos];
  for (int i=0;i<64;i++){ uint64_t v = (uint64_t)(uint16_t)(s.alt[i]-b) | ((uint64_t)s.vis[i]<<16) | ((uint64_t)i<<24);
    v *= 0xff51afd7ed558ccdULL; v ^= v>>33; h ^= v; h *= 0xc4ceb9fe1a85ec53ULL; h ^= h>>29; }
  return h | 1;
}
// heuristic ("dance relaxation") upper bound on future minutes from s
static int ub_future(const S& s){
  AT a = s.alt[s.pos];
  int c[5]; for (int d=-2; d<=2; d++){ c[d+2]=0; }
  for (int i=0;i<64;i++){ int d=s.alt[i]-a; if(d>=-2&&d<=2) c[d+2]++; }
  // dance levels: (a, a-2) or (a+2, a)
  int top = (c[4]+c[2] > c[2]+c[0]) ? a+2 : a;
  bool inA[64]; int U=0,L=0;
  for (int i=0;i<64;i++){ inA[i] = (s.alt[i]==top || s.alt[i]==top-2); if(s.alt[i]==top) U++; else if(s.alt[i]==top-2) L++; }
  if (!inA[H8]) {
    // h8 not in dance: fall back to loose bound
    return 1000000;
  }
  // sits available
  int F=0; int blocks[32]; int nb=0;
  for (int p=0;p<32;p++){
    int q=63-p;
    int rp = (p==H8||!inA[p]) ? 0 : (3-s.vis[p]) + (s.pos==p);
    int rq = (q==H8||!inA[q]) ? 0 : (3-s.vis[q]) + (s.pos==q);
    if (rp<0) rp=0; if (rq<0) rq=0;
    if (inA[p] && inA[q] && p!=0 ) { int r=max(rp,rq); if(r>0) blocks[nb++]=r; }
    else if (inA[p] && inA[q] && p==0) { F += rp; } // a1-h8 pair: h8 never sits; sitting a1 would kick h8 (ignored)
    else F += rp + rq;
  }
  sort(blocks, blocks+nb, greater<int>());
  int Ssum = F; for(int i=0;i<nb;i++) Ssum+=blocks[i];
  // alternating gains, starting group: whichever (take max of two starts)
  int best=0;
  for (int start=0; start<2; start++){
    int g[2] = {U, L}; int tot=0, k=0; int cur=start;
    // free sits
    for (int i=0;i<F;i++){ tot += 4*g[cur]; cur^=1; k++; }
    for (int b=0;b<nb;b++){
      // kill at first sit of block: remove from the group sat on less in the rest; approximated: larger group keeps
      // (optimistic) remove from group 'cur^1'?? choose the removal maximizing remaining sum: remove from group sat on fewer times
      int rem = 0; for(int bb=b; bb<nb; bb++) rem += blocks[bb];
      // in remaining 'rem' sits starting with cur: cur gets ceil(rem/2), other floor(rem/2)
      int other = cur^1;
      if (g[other] > 0 && (rem/2) <= (rem+1)/2) g[other]--; else g[cur]--;
      for (int i=0;i<blocks[b];i++){ tot += 4*g[cur]; cur^=1; }
    }
    best = max(best, tot);
  }
  (void)Ssum;
  return best;
}
static void save_solution(int T){
  vector<pair<int,int>> t = prefixW; for (auto& m: stackW) t.push_back(m);
  string s = tour_str(t);
  fprintf(stdout, "FOUND %d (%.1fs, %lld nodes)\n%s\n", T, chrono::duration<double>(chrono::steady_clock::now()-t0).count(), nodes, s.c_str()); fflush(stdout);
  if (!OUTF.empty()){ FILE* f=fopen(OUTF.c_str(),"w"); fprintf(f,"%d\n%s\n",T,s.c_str()); fclose(f); }
}
static int dfs(S& s, int depth, bool onTour){
  nodes++;
  if ((nodes & 0xFFFF)==0){ if (chrono::duration<double>(chrono::steady_clock::now()-t0).count() > TLIM) timed_out=true; }
  if (timed_out) return 1000000; // invalid; caller ignores
  uint64_t key = keyS(s); TTE& e = TT[key & TTMASK];
  if (e.key==key && s.T + e.ub <= ALPHA) return e.ub;
  int b = NOBOUND ? 1000000 : ub_future(s);
  if (s.T + b <= ALPHA) { return b; }
  vector<pair<int,int>> mv; pinned_moves(s, 3, true, mv);
  int n = group_n(s, s.pos);
  // order: tour move first, then by gain
  int tourIdx = -1;
  if (onTour) { int ti = KPRE + depth; if (ti < (int)tourW.size()) for (size_t i=0;i<mv.size();i++) if (mv[i].second==tourW[ti].second && n*mv[i].first==tourW[ti].first) tourIdx=i; }
  vector<int> ord(mv.size()); iota(ord.begin(), ord.end(), 0);
  stable_sort(ord.begin(), ord.end(), [&](int x,int y){ if (x==tourIdx) return y!=tourIdx; if (y==tourIdx) return false; return mv[x].first > mv[y].first; });
  int best = NEG;
  for (int oi: ord){
    auto m = mv[oi]; int g = n*m.first;
    if (m.second == H8){
      if (s.T + g > ALPHA && !CHECK){ ALPHA = s.T + g; stackW.push_back({g,m.second}); save_solution(ALPHA); stackW.pop_back(); }
      best = max(best, g); continue;
    }
    S s2 = s; apply_move(s2, m.first, m.second);
    stackW.push_back({g, m.second});
    int r = dfs(s2, depth+1, onTour && oi==tourIdx);
    stackW.pop_back();
    if (timed_out) return 1000000;
    if (r > NEG/2) best = max(best, g + r);
  }
  if (CHECK && !NOBOUND) {}
  if (CHECK) { checked++; int bb = ub_future(s); if (best > NEG/2 && best > bb) { viol++; if (viol<=10) fprintf(stderr,"bound violation: exact %d > ub %d at T=%d pos=%s depth %d\n", best, bb, s.T, nm(s.pos).c_str(), depth); } }
  if (e.key != key || best < e.ub) { e.key = key; e.ub = best; }
  return best;
}
int main(int argc, char** argv){
  init_board();
  const char* tf = argv[1]; KPRE = atoi(argv[2]);
  ALPHA = 8123; int ttlog = 24;
  for (int i=3;i<argc;i++){ string a=argv[i];
    if (a=="--alpha") ALPHA=atoi(argv[++i]); else if (a=="--time") TLIM=atof(argv[++i]);
    else if (a=="--tt") ttlog=atoi(argv[++i]); else if (a=="--nobound") NOBOUND=true; else if (a=="--check") CHECK=true;
    else if (a=="--out") OUTF=argv[++i]; }
  TT.assign(1ULL<<ttlog, TTE{0, 0}); TTMASK = (1ULL<<ttlog)-1;
  tourW = read_tour(tf); prefixW.assign(tourW.begin(), tourW.begin()+KPRE);
  S s = start_state(); if (!replay_moves(s, tourW, KPRE)) return 1;
  int ub0 = ub_future(s);
  printf("k=%d T=%d pos=%s root heuristic UB=%d (total %d) alpha=%d\n", KPRE, s.T, nm(s.pos).c_str(), ub0, s.T+ub0, ALPHA); fflush(stdout);
  t0 = chrono::steady_clock::now();
  int a0 = ALPHA;
  int r = dfs(s, 0, true);
  double el = chrono::duration<double>(chrono::steady_clock::now()-t0).count();
  if (timed_out) printf("TIMEOUT k=%d after %.1fs nodes=%lld best=%d\n", KPRE, el, nodes, ALPHA);
  else printf("DONE k=%d %.1fs nodes=%lld best=%d (start alpha %d) proven_ub_total=%d %s\n", KPRE, el, nodes, ALPHA, a0, s.T + r, (ALPHA==a0? "=> no tour > alpha with this prefix":"improved"));
  if (CHECK) printf("checked %lld violations %lld\n", checked, viol);
}
