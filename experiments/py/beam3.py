import sys, time
from st import *
from phaseA_state import phaseA_state
from beam2 import key

KN = [ix('f7'), ix('g6')]
OR = [ix('g8'), ix('h7')]
D2 = [ix('f8'), ix('h6')]
def finish_ok(s):
    h = s.alt[H8]
    for q in KN:
        if s.alt[q] == h and s.vis[q] < 3: return True
    for q in OR:
        if abs(s.alt[q] - h) == 2 and s.vis[q] < 3: return True
    for q in D2:
        if abs(s.alt[q] - h) == 1 and s.vis[q] < 3: return True
    return False

def beam(s0, depth, W, grace=3):
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
                if d >= grace and not finish_ok(c):
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
    bestT, b = beam(s, int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))
    print('BEST', bestT)
    if b:
        print(fmt(b.moves))
        import pickle; pickle.dump(b.moves, open('best_beam3.pkl','wb'))
