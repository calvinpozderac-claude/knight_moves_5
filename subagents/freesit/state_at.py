import sys,re
sys.path.insert(0,'/home/user/knight_moves_5/tools')
from sim import *
from fractions import Fraction as F
from collections import Counter
fn=sys.argv[1]; K=int(sys.argv[2])
mv=[(F(a),b) for a,b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)',open(fn).read())]
alt=init_alt(MAIN_ROWS);pos=parse('a1');vis=Counter();vis['a1']=1
for i,(t,q) in enumerate(mv[:K]):
    apply_wait(pos,t,alt,8);pos=parse(q);vis[q]+=1
A=alt[pos];print('pos',name(pos),'alt',A)
for y in range(7,-1,-1):
    print(' '.join('%5s'%(str(alt[(x,y)]-A)) +'/%d'%vis[chr(97+x)+str(y+1)] for x in range(8)))
