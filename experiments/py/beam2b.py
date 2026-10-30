import sys
from st import *
from phaseA_state import phaseA_state
from beam2 import key
def beam_top(s0, depth, W):
    beam = [s0]
    for d in range(depth):
        cand = {}
        for s in beam:
            for delta, q in moves_pinned(s.p, s.alt, s.vis, allow_h8=False):
                c = s.step(delta, q, check=False)
                k = key(c)
                if k not in cand or cand[k].T < c.T:
                    cand[k] = c
        beam = sorted(cand.values(), key=lambda c: -c.T)[:W]
    return beam[0]
s = beam_top(phaseA_state(), int(sys.argv[1]), 200)
print(fmt(s.moves[23:]))
s.show()
