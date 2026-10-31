#include "common.h"
int main(int argc,char**argv){
  init_board(); auto t=read_tour(argv[1]); S s=start_state(); int total=0; for(auto&m:t) total+=m.first;
  vector<pair<int,int>> mv;
  for(size_t i=0;i<t.size();i++){
    // group sizes
    AT a=s.alt[s.pos]; int nUp=0,nDn=0,nUp2=0; for(int j=0;j<64;j++){ if(s.alt[j]==a) nUp++; if(s.alt[j]==a-2) nDn++; if(s.alt[j]==a+2) nUp2++; }
    int remv=0; for(int j=0;j<63;j++) if(s.alt[j]==a||s.alt[j]==a-2||s.alt[j]==a+2) remv+=3-s.vis[j];
    printf("%3zu T=%5d rem=%5d pos=%s alt=%d h8alt=%d grp(a)=%d grp(a-2)=%d grp(a+2)=%d remvis=%d  next=(%d,%s)\n",i,s.T,total-s.T,nm(s.pos).c_str(),a,s.alt[63],nUp,nDn,nUp2,remv,t[i].first,nm(t[i].second).c_str());
    vector<pair<int,int>> one={t[i]}; if(!replay_moves(s,one,1)) return 1;
  }
  printf("final T=%d\n",s.T);
}
