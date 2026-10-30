import sys
from st import *
from phaseA_state import phaseA_state
from beam2 import key
from beam3 import finish_ok
def beam_top(s0, depth, W):
    beam = [s0]
    for d in range(depth):
        cand = {}
        for s in beam:
            for delta, q in moves_pinned(s.p, s.alt, s.vis, allow_h8=False):
                c = s.step(delta, q, check=False)
                if d >= 2 and not finish_ok(c): continue
                k = key(c)
                if k not in cand or cand[k].T < c.T:
                    cand[k] = c
        beam = sorted(cand.values(), key=lambda c: -c.T)[:W]
    return beam[0]
s = beam_top(phaseA_state(), int(sys.argv[1]), 300)
print(fmt(s.moves[23:]))
s.show()
h = s.alt[H8]
print('h8 group:', [nm(i) for i in range(64) if s.alt[i]==h])
for q in ['f7','g6','g8','h7','f8','h6']:
    print(q, s.alt[ix(q)], s.vis[ix(q)])
