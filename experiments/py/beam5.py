import sys, time, pickle, heapq, itertools
from st import *
from phaseA_state import phaseA_state
from beam2 import key

KN = [ix('f7'), ix('g6')]
OR = [ix('g8'), ix('h7')]
D2 = [ix('f8'), ix('h6')]
def fin_neighbors(s):
    h = s.alt[H8]
    out = []
    for q in KN:
        if s.alt[q] == h and s.vis[q] < 3: out.append(q)
    for q in OR:
        if abs(s.alt[q] - h) == 2 and s.vis[q] < 3: out.append(q)
    for q in D2:
        if abs(s.alt[q] - h) == 1 and s.vis[q] < 3: out.append(q)
    return out

def ok(s):
    if abs(s.alt[H8] - s.alt[s.p]) > 2: return False
    return bool(fin_neighbors(s))

def pdist(a, b):
    ax, ay, bx, by = a % 8, a // 8, b % 8, b // 8
    return max(abs(ax-bx), abs(ay-by))

def finish_search(s0, cap=6000):
    cnt = itertools.count()
    best = (0, None)
    def h(s):
        f = fin_neighbors(s) or [H8]
        return min(pdist(s.p, q) for q in f)
    pq = [(h(s0), next(cnt), s0)]
    seen = set()
    nodes = 0
    while pq and nodes < cap:
        _, _, s = heapq.heappop(pq)
        for delta, q in moves_pinned(s.p, s.alt, s.vis, allow_h8=True):
            c = s.step(delta, q, check=False)
            nodes += 1
            if q == H8:
                if c.T > best[0]: best = (c.T, c)
                continue
            k = key(c)
            if k in seen: continue
            seen.add(k)
            heapq.heappush(pq, (h(c) + 0.01 * len(c.moves) - 0.0001 * c.T, next(cnt), c))
        if best[1] is not None and nodes > cap // 3:
            break
    return best

def beam(s0, depth, W, topk=4, grace=1):
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
                if d >= grace and not ok(c):
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
                pickle.dump(fs.moves, open('best_beam5.pkl', 'wb'))
        print(d+1, 'beamtop', beam[0].T, 'best finished', best[0], flush=True)
    return best

if __name__ == '__main__':
    s = phaseA_state()
    bestT, b = beam(s, int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))
    print('BEST', bestT)
    if b: print(fmt(b.moves))
