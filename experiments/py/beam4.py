import sys, time, pickle
from st import *
from phaseA_state import phaseA_state
from beam2 import key
from beam3 import finish_ok

def finish_search(s0, maxdepth=6, cap=20000):
    """BFS for completions; returns best (T, state)."""
    best = (0, None)
    layer = [s0]
    seen = set()
    nodes = 0
    for d in range(maxdepth):
        nxt = []
        for s in layer:
            for delta, q in moves_pinned(s.p, s.alt, s.vis, allow_h8=True):
                c = s.step(delta, q, check=False)
                nodes += 1
                if q == H8:
                    if c.T > best[0]: best = (c.T, c)
                    continue
                k = key(c)
                if k in seen: continue
                seen.add(k)
                nxt.append(c)
        layer = nxt
        if nodes > cap: break
    return best

def beam(s0, depth, W, topk=6, grace=2):
    beam = [s0]
    best = (0, None)
    for d in range(depth):
        cand = {}
        for s in beam:
            for delta, q in moves_pinned(s.p, s.alt, s.vis, allow_h8=True):
                c = s.step(delta, q, check=False)
                if q == H8:
                    if c.T > best[0]: best = (c.T, c)
                    continue
                if d >= grace and not finish_ok(c):
                    continue
                k = key(c)
                if k not in cand or cand[k].T < c.T:
                    cand[k] = c
        if not cand: break
        beam = sorted(cand.values(), key=lambda c: -c.T)[:W]
        for s in beam[:topk]:
            fT, fs = finish_search(s)
            if fT > best[0]:
                best = (fT, fs)
                pickle.dump(fs.moves, open('best_beam4.pkl', 'wb'))
        print(d+1, 'beamtop', beam[0].T, 'best finished', best[0], flush=True)
    return best

if __name__ == '__main__':
    s = phaseA_state()
    bestT, b = beam(s, int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))
    print('BEST', bestT)
    if b: print(fmt(b.moves))
