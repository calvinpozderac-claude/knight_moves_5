import sys, random
from st import *

def key(s):
    return (s.p, tuple(s.alt), tuple(s.vis))

def beam(s0, depth, W, allow_h8=False):
    beam = [s0]
    best = s0
    for d in range(depth):
        cand = {}
        for s in beam:
            for delta, q in moves_pinned(s.p, s.alt, s.vis, allow_h8=allow_h8):
                c = s.step(delta, q, check=False)
                k = key(c)
                if k not in cand or cand[k].T < c.T:
                    cand[k] = c
        if not cand: break
        beam = sorted(cand.values(), key=lambda c: -c.T)[:W]
        if beam[0].T > best.T: best = beam[0]
        if d % 10 == 9:
            print(d+1, beam[0].T, len(cand), flush=True)
    return best

s = State()
s = walk(s, static_path(s, ix('g7')))
b = beam(s, int(sys.argv[1]), int(sys.argv[2]))
print(b.T)
print(fmt(b.moves))
b.show()
