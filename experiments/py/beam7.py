"""Memory-lean beam search for the leapfrog phase, with h8 kept finishable."""
import sys, time, pickle, array
from core import *
from phaseA_state import phaseA_state
from st import State
from beam5 import finish_search

KN = (ix('f7'), ix('g6'))
OR = (ix('g8'), ix('h7'))
D2 = (ix('f8'), ix('h6'))


def fin_ok(alt, vis, p):
    h = alt[H8]
    if abs(h - alt[p]) > 2:
        return False
    for q in KN:
        if alt[q] == h and vis[q] < 3:
            return True
    for q in OR:
        if abs(alt[q] - h) == 2 and vis[q] < 3:
            return True
    for q in D2:
        if abs(alt[q] - h) == 1 and vis[q] < 3:
            return True
    return False


def expand(p, alt, vis):
    """yield (t, q, newalt) for pinned moves (h8 included)."""
    A = alt[p]
    O = 63 - p
    g = [i for i in range(64) if alt[i] == A and i != O]
    rise = alt[O] != A
    n = len(g)
    for delta, q in moves_pinned(p, alt, vis, allow_h8=True):
        if delta == 0:
            yield 0, q, alt
        else:
            a = list(alt)
            for i in g:
                a[i] -= delta
            if rise:
                a[O] += delta
            yield n * delta, q, tuple(a)


def run(s0, W, maxdepth=150, out='best7.pkl', log=print, topk=8, cap=6000):
    # beam entries: (T, p, alt tuple, vis bytes, idx)
    hist_parent = []
    hist_t = []
    hist_q = []
    beam = [(s0.T, s0.p, tuple(s0.alt), bytes(s0.vis), -1)]
    prefix = list(s0.moves)
    best = (0, None)
    t0 = time.time()

    def moves_of(depth, idx):
        ms = []
        while depth >= 0 and idx >= 0:
            ms.append((hist_t[depth][idx], nm(hist_q[depth][idx])))
            idx = hist_parent[depth][idx]
            depth -= 1
        return prefix + ms[::-1]

    for d in range(maxdepth):
        cand = {}
        for bi, (T, p, alt, vis, _) in enumerate(beam):
            for t, q, a2 in expand(p, alt, vis):
                T2 = T + t
                if q == H8:
                    if T2 > best[0]:
                        ms = moves_of(d - 1, bi) + [(t, 'h8')]
                        best = (T2, ms)
                        pickle.dump(ms, open(out, 'wb'))
                    continue
                v2 = bytearray(vis)
                v2[q] += 1
                if d >= 1 and not fin_ok(a2, v2, q):
                    continue
                k = (q, a2, bytes(v2))
                old = cand.get(k)
                if old is None or old[0] < T2:
                    cand[k] = (T2, bi, t)
        if not cand:
            break
        items = sorted(cand.items(), key=lambda kv: -kv[1][0])[:W]
        par = array.array('i')
        tt = array.array('i')
        qq = array.array('b')
        newbeam = []
        for i, ((q, a2, v2), (T2, bi, t)) in enumerate(items):
            par.append(bi)
            tt.append(t)
            qq.append(q)
            newbeam.append((T2, q, a2, v2, i))
        hist_parent.append(par)
        hist_t.append(tt)
        hist_q.append(qq)
        beam = newbeam
        del cand, items
        for (T2, q, a2, v2, i) in beam[:topk]:
            st = State(q, list(a2), list(v2), T2, moves_of(d, i))
            fT, fs = finish_search(st, cap=cap)
            if fs is not None and fT > best[0]:
                best = (fT, fs.moves)
                pickle.dump(fs.moves, open(out, 'wb'))
        log(f'{d+1} beamtop {beam[0][0]} best {best[0]} {time.time()-t0:.0f}s')
    return best


if __name__ == '__main__':
    W = int(sys.argv[1])
    out = sys.argv[2]
    best = run(phaseA_state(), W, out=out, log=lambda m: print(m, flush=True))
    print('BEST', best[0])
