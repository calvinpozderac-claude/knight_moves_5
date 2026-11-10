# A wait is "free" if the following jump stays legal when the wait is perturbed slightly
# (target sinks with the knight's group). Pinned waits make the jump legal at an isolated value.
import sys,re,copy
sys.path.insert(0,'/home/user/knight_moves_5/tools')
from sim import *
from fractions import Fraction as F
for fn in sys.argv[1:]:
    txt=open(fn).read()
    mv=[(F(a),b) for a,b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)',txt)]
    alt=init_alt(MAIN_ROWS);pos=parse('a1');free=[]
    for i,(t,q) in enumerate(mv):
        Q=parse(q)
        if t>0:
            a2=dict(alt); apply_wait(pos,t+F(1,997),a2,8)
            if legal_jump(pos,Q,a2): free.append((i+1,int(t) if t.denominator==1 else str(t),q))
        if t.denominator!=1: free.append((i+1,str(t),q,'frac'))
        apply_wait(pos,t,alt,8);pos=Q
    print(fn, int(sum(t for t,_ in mv)), 'STRICT' if not free else f'free waits {free}')
