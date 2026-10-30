import sys, time, pickle
from st import *
from phaseA_state import phaseA_state
from beam2 import key
from beam5 import ok, finish_search

def run(s0, depth, W, topk, out):
    beam = [s0]
    best = 0
    t0 = time.time()
    def save(c):
        nonlocal best
        if c.T > best:
            best = c.T
            pickle.dump(c.moves, open(out, 'wb'))
    for d in range(depth):
        cand = {}
        for s in beam:
            for delta, q in moves_pinned(s.p, s.alt, s.vis, allow_h8=True):
                c = s.step(delta, q, check=False)
                if q == H8:
                    save(c); continue
                if d >= 1 and not ok(c): continue
                k = key(c)
                if k not in cand or cand[k].T < c.T:
                    cand[k] = c
        if not cand: break
        beam = sorted(cand.values(), key=lambda c: -c.T)[:W]
        for s in beam[:topk]:
            fT, fs = finish_search(s)
            if fs is not None: save(fs)
        print(d+1, 'beamtop', beam[0].T, 'best', best, f'{time.time()-t0:.0f}s', flush=True)
    print('BEST', best)

if __name__ == '__main__':
    run(phaseA_state(), 120, int(sys.argv[1]), int(sys.argv[2]), sys.argv[3])
