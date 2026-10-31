// Depth-first branch-and-bound over pinned moves (eng.h) for the suffix of a tour.
// usage: bb TOURFILE K [--alpha A] [--time SEC] [--tt LOG2] [--nobound] [--check] [--out FILE]
#include "common.h"
#include <chrono>
static const int NEG = -1000000000;
static int ALPHA; static bool NOBOUND=false, CHECK=false; static double TLIM=1e18; static double LAM=1.0;
static long long nodes=0, viol=0, checked=0; static chrono::steady_clock::time_point t0;
static bool timed_out=false;
static vector<pair<int,int>> prefixW, tourW, stackW; static string OUTF;
static int KPRE;
struct TTE { uint64_t key, key2; int32_t ub; int32_t exact; };
static long long tthits=0;
static vector<TTE> TT; static uint64_t TTMASK;
static inline uint64_t keyS(const S& s){
  uint64_t h = 1469598103934665603ULL ^ (s.pos*0x9E3779B97F4A7C15ULL);
  AT b = s.alt[s.pos];
  for (int i=0;i<64;i++){ uint64_t v = (uint64_t)(uint16_t)(s.alt[i]-b) | ((uint64_t)s.vis[i]<<16) | ((uint64_t)i<<24);
    v *= 0xff51afd7ed558ccdULL; v ^= v>>33; h ^= v; h *= 0xc4ceb9fe1a85ec53ULL; h ^= h>>29; }
  return h | 1;
}
static inline uint64_t keyS2(const S& s){
  uint64_t h = 0x84222325cbf29ce4ULL + s.pos; AT b = s.alt[s.pos];
  for (int i=0;i<64;i++){ uint64_t v = ((uint64_t)(uint16_t)(s.alt[i]-b) << 8) ^ s.vis[i] ^ ((uint64_t)i<<40);
    h = (h ^ v) * 0x100000001b3ULL; h ^= h >> 31; h *= 0x9E3779B97F4A7C15ULL; }
  return h;
}
// heuristic ("dance relaxation") upper bound on future minutes from s
static uint64_t ORTM[64], KNM[64], TWOM[64];
static void init_masks(){ for(int i=0;i<64;i++){ ORTM[i]=KNM[i]=TWOM[i]=0; for(int k=0;k<NBN[i];k++){ int q=NBQ[i][k]; uint64_t b=1ULL<<q; if(NBT[i][k]==0) KNM[i]|=b; else if(NBT[i][k]==1) ORTM[i]|=b; else TWOM[i]|=b; } } }
static int ub_future(const S& s){
  AT a = s.alt[s.pos];
  int c[5]; for (int d=-2; d<=2; d++){ c[d+2]=0; }
  for (int i=0;i<64;i++){ int d=s.alt[i]-a; if(d>=-2&&d<=2) c[d+2]++; }
  // dance levels: (a, a-2) or (a+2, a)
  int top = (c[4]+c[2] > c[2]+c[0]) ? a+2 : a;
  bool inA[64], inC[64]; int U=0,L=0;
  for (int i=0;i<64;i++){ inA[i] = (s.alt[i]==top || s.alt[i]==top-2); if(s.alt[i]==top) U++; else if(s.alt[i]==top-2) L++; }
  if (!inA[H8]) {
    // h8 not in dance: fall back to loose bound
    return 1000000;
  }
  // component of the knight over available dance points (ortho U<->L, knight within the same level)
  uint64_t Um=0, Lm=0, av=0;
  for (int i=0;i<64;i++){ if(s.alt[i]==top) Um|=1ULL<<i; else if(s.alt[i]==top-2) Lm|=1ULL<<i; if(inA[i] && i!=H8 && s.vis[i]<3) av|=1ULL<<i; }
  av |= 1ULL<<s.pos;
  uint64_t comp = 1ULL<<s.pos, fr = comp; bool h8ok=false;
  while (fr){ int u=__builtin_ctzll(fr); fr&=fr-1;
    bool uU = (Um>>u)&1;
    uint64_t nb = (ORTM[u] & (uU?Lm:Um)) | (KNM[u] & (uU?Um:Lm));
    if (((ORTM[u]|KNM[u]|TWOM[u])>>H8)&1) h8ok=true;
    nb &= av & ~comp; comp|=nb; fr|=nb; }
  for (int i=0;i<64;i++) inC[i] = h8ok ? (((comp>>i)&1)!=0) : inA[i];
  // sits available (alternating relaxation)
  int F=0; int blocks[32]; int nb=0; int SG[2]={0,0};
  auto grpOf=[&](int i){ return s.alt[i]==top ? 0 : 1; };
  for (int p=0;p<32;p++){
    int q=63-p;
    int rp = (p==H8||!inC[p]) ? 0 : (3-s.vis[p]) + (s.pos==p);
    int rq = (q==H8||!inC[q]) ? 0 : (3-s.vis[q]) + (s.pos==q);
    if (rp<0) rp=0; if (rq<0) rq=0;
    if (inA[p] && inA[q] && p!=0) {
      int r=max(rp,rq); if(r>0){ blocks[nb++]=r;
        if (grpOf(p)==grpOf(q)) SG[grpOf(p)]+=r; else { SG[0]+= (grpOf(p)==0?rp:rq); SG[1]+=(grpOf(p)==1?rp:rq); } }
    } else {
      F += rp + rq;
      if (rp && inA[p]) SG[grpOf(p)] += rp; if (rq && inA[q]) SG[grpOf(q)] += rq;
    }
  }
  sort(blocks, blocks+nb, greater<int>());
  int Stot = F; for(int i=0;i<nb;i++) Stot+=blocks[i];
  int best=0;
  for (int start=0; start<2; start++){
    int Smax = min(Stot, min(2*SG[start], 2*SG[start^1]+1));
    int g[2] = {U, L}; int tot=0, k=0; int cur=start;
    for (int i=0;i<F && k<Smax;i++){ tot += 4*g[cur]; cur^=1; k++; }
    for (int b=0;b<nb && k<Smax;b++){
      int other = cur^1;
      if (g[other] > 0) g[other]--; else g[cur]--;
      for (int i=0;i<blocks[b] && k<Smax;i++){ tot += 4*g[cur]; cur^=1; k++; }
    }
    best = max(best, tot);
  }
  return best;
}
static void save_solution(int T){
  vector<pair<int,int>> t = prefixW; for (auto& m: stackW) t.push_back(m);
  string s = tour_str(t);
  fprintf(stdout, "FOUND %d (%.1fs, %lld nodes)\n%s\n", T, chrono::duration<double>(chrono::steady_clock::now()-t0).count(), nodes, s.c_str()); fflush(stdout);
  if (!OUTF.empty()){ FILE* f=fopen(OUTF.c_str(),"w"); fprintf(f,"%d\n%s\n",T,s.c_str()); fclose(f); }
}
// returns an upper bound on the best future minutes; *ex = true iff the value is exact (subtree fully enumerated)
static int dfs(S& s, int depth, bool onTour, bool* ex){
  nodes++; *ex = false;
  if ((nodes & 0xFFFF)==0){ if (chrono::duration<double>(chrono::steady_clock::now()-t0).count() > TLIM) timed_out=true; }
  if (timed_out) return 1000000;
  uint64_t key = keyS(s), key2 = keyS2(s); TTE& e = TT[key & TTMASK];
  bool hit = (e.key==key && e.key2==key2);
  if (hit && e.exact) { tthits++; *ex = true; return e.ub; }
  if (hit && s.T + e.ub <= ALPHA) return e.ub;
  if (!NOBOUND) { int b = ub_future(s); if (s.T + (int)(LAM*b) <= ALPHA) return b; }
  vector<pair<int,int>> mv; pinned_moves(s, 3, true, mv);
  int n = group_n(s, s.pos);
  int tourIdx = -1;
  if (onTour) { int ti = KPRE + depth; if (ti < (int)tourW.size()) for (size_t i=0;i<mv.size();i++) if (mv[i].second==tourW[ti].second && n*mv[i].first==tourW[ti].first) tourIdx=i; }
  vector<int> ord(mv.size()); iota(ord.begin(), ord.end(), 0);
  stable_sort(ord.begin(), ord.end(), [&](int x,int y){ if (x==tourIdx) return y!=tourIdx; if (y==tourIdx) return false; return mv[x].first > mv[y].first; });
  int best = NEG; bool allex = true;
  for (int oi: ord){
    auto m = mv[oi]; int g = n*m.first;
    if (m.second == H8){
      if (s.T + g > ALPHA && !CHECK){ ALPHA = s.T + g; stackW.push_back({g,m.second}); save_solution(ALPHA); stackW.pop_back(); }
      best = max(best, g); continue;
    }
    S s2 = s; apply_move(s2, m.first, m.second);
    stackW.push_back({g, m.second});
    bool cex; int r = dfs(s2, depth+1, onTour && oi==tourIdx, &cex);
    stackW.pop_back();
    if (timed_out) return 1000000;
    if (!cex) allex = false;
    if (r > NEG/2) best = max(best, g + r);
  }
  if (CHECK) { checked++; int bb = ub_future(s); if (allex && best > NEG/2 && best > bb) { viol++; if (viol<=10) fprintf(stderr,"bound violation: exact %d > ub %d at T=%d pos=%s depth %d\n", best, bb, s.T, nm(s.pos).c_str(), depth); } }
  if (!hit || allex || best < e.ub) { e.key = key; e.key2 = key2; e.ub = (hit && !allex && !e.exact) ? min(e.ub, best) : best; e.exact = allex; }
  *ex = allex;
  return best;
}
int main(int argc, char** argv){
  init_board(); init_masks();
  const char* tf = argv[1]; KPRE = atoi(argv[2]);
  ALPHA = 8123; int ttlog = 24;
  for (int i=3;i<argc;i++){ string a=argv[i];
    if (a=="--alpha") ALPHA=atoi(argv[++i]); else if (a=="--time") TLIM=atof(argv[++i]);
    else if (a=="--tt") ttlog=atoi(argv[++i]); else if (a=="--nobound") NOBOUND=true; else if (a=="--check") CHECK=true;
    else if (a=="--out") OUTF=argv[++i]; else if (a=="--lambda") LAM=atof(argv[++i]); }
  TT.assign(1ULL<<ttlog, TTE{0, 0, 0, 0}); TTMASK = (1ULL<<ttlog)-1;
  tourW = read_tour(tf); prefixW.assign(tourW.begin(), tourW.begin()+KPRE);
  S s = start_state(); if (!replay_moves(s, tourW, KPRE)) return 1;
  int ub0 = ub_future(s);
  printf("k=%d T=%d pos=%s root heuristic UB=%d (total %d) alpha=%d\n", KPRE, s.T, nm(s.pos).c_str(), ub0, s.T+ub0, ALPHA); fflush(stdout);
  t0 = chrono::steady_clock::now();
  int a0 = ALPHA;
  bool rex; int r = dfs(s, 0, true, &rex);
  double el = chrono::duration<double>(chrono::steady_clock::now()-t0).count();
  if (timed_out) printf("TIMEOUT k=%d after %.1fs nodes=%lld best=%d\n", KPRE, el, nodes, ALPHA);
  else if (rex) printf("DONE k=%d %.1fs nodes=%lld tthits=%lld EXACT suffix optimum from k: %d (prefix T=%d)%s\n", KPRE, el, nodes, tthits, s.T + r, s.T, NOBOUND? " [sound: exhaustive]":" [bound used but no cut]");
  else printf("DONE k=%d %.1fs nodes=%lld best=%d (start alpha %d) ub_total=%d %s\n", KPRE, el, nodes, ALPHA, a0, s.T + r, NOBOUND ? "[sound]" : "[UNPROVEN: relies on heuristic bound]");
  if (CHECK) printf("checked %lld violations %lld\n", checked, viol);
}
