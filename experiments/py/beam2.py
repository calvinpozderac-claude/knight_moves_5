import sys, time
from st import *
from phaseA_state import phaseA_state

def key(s):
    return (s.p, tuple(s.alt), tuple(s.vis))

def beam(s0, depth, W):
    beam = [s0]
    best = (0, None)
    for d in range(depth):
        cand = {}
        for s in beam:
            for delta, q in moves_pinned(s.p, s.alt, s.vis, allow_h8=True):
                c = s.step(delta, q, check=False)
                if q == H8:
                    if c.T > best[0]:
                        best = (c.T, c)
                    continue
                k = key(c)
                if k not in cand or cand[k].T < c.T:
                    cand[k] = c
        if not cand: break
        beam = sorted(cand.values(), key=lambda c: -c.T)[:W]
        if d % 5 == 4:
            print(d+1, 'beamtop', beam[0].T, 'best finished', best[0], flush=True)
    return best

if __name__ == '__main__':
    s = phaseA_state()
    bestT, b = beam(s, int(sys.argv[1]), int(sys.argv[2]))
    print('BEST', bestT)
    if b:
        print(fmt(b.moves))
