import sys,re
sys.path.insert(0,'/home/user/knight_moves_5/tools')
from sim import *
from fractions import Fraction as F
from collections import Counter
fn=sys.argv[1]
txt=open(fn).read()
mv=[(F(a),b) for a,b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)',txt)]
alt=init_alt(MAIN_ROWS);pos=parse('a1')
vis=Counter(); vis['a1']=1
T=0
rows=[]
for i,(t,q) in enumerate(mv):
    Q=parse(q)
    A=alt[pos]
    n=sum(1 for p in alt if alt[p]==A) 
    O=(7-pos[0],7-pos[1])
    if alt[O]==A: n-=1
    if t>0:
        # other group sizes: count altitude histogram
        hist=Counter(alt.values())
        top=sorted(hist.items(), key=lambda x:-x[1])[:3]
        rows.append((i+1,name(pos),q,int(t),n,float(t/n),[(float(a-A),c) for a,c in top], float(alt[parse('h8')]-A)))
    apply_wait(pos,t,alt,8);pos=Q;T+=t;vis[q]+=1
print(len(mv),'moves, T',T, 'sits',len(rows))
for r in rows: print(r)
# visits
unused={k:3-v for k,v in vis.items() if k!='h8'}
allsq=[chr(97+x)+str(y+1) for x in range(8) for y in range(8)]
print('visits:', ' '.join(f'{s}:{vis[s]}' for s in allsq))
sit=Counter(r[1] for r in rows)
print('sits per point:', dict(sit))
print('total sits', sum(sit.values()), 'sitters', len(sit))
print('final alts rel h8:'); 
h=alt[parse('h8')]
for y in range(7,-1,-1): print(' '.join(f'{float(alt[(x,y)]-h):6.0f}' for x in range(8)))
