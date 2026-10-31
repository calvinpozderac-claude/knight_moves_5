#include "common.h"
int main(int argc,char**argv){ init_board(); auto t=read_tour(argv[1]); int k=atoi(argv[2]); S s=start_state(); replay_moves(s,t,k);
 AT a=s.alt[s.pos]; printf("pos %s a=%d\n",nm(s.pos).c_str(),a);
 for(int y=7;y>=0;y--){ for(int x=0;x<8;x++){int i=x+8*y; int d=s.alt[i]-a; if(d==0||d==-2) printf(" %c%d/%d", d==0?'U':'L', s.vis[i], 0); else printf(" %+4d", d);} printf("\n"); }
}
