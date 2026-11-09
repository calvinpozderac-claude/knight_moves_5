import sys, re
from collections import defaultdict
sys.path.insert(0,'/home/user/knight_moves_5/subagents/families')
from sig import moves, read
def N(x,y): return chr(97+x)+str(y+1)
def opp(s): return N(7-(ord(s[0])-97), 7-(int(s[1])-1))
for p in sys.argv[1:]:
    mv=moves(read(p)); pos='a1'; sits=defaultdict(int); vis=defaultdict(int); vis['a1']=1; val=defaultdict(int)
    for w,q in mv:
        if w>=40: sits[pos]+=1; val[pos]+=w
        vis[q]+=1; pos=q
    print(p)
    rows=[]
    for y in range(7,-1,-1):
        r=''
        for x in range(8):
            s=N(x,y); r+=f' {sits[s]}/{vis[s]}' + ('*' if sits[s] else ' ')
        rows.append(r)
    print('\n'.join(rows))
    pairs_both=[ (s,opp(s)) for s in sits if sits[opp(s)] and s<opp(s)]
    print('sitters',len(sits),'total sits',sum(sits.values()),'pairs with both sitting',pairs_both)
