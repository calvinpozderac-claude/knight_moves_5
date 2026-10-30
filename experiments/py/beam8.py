"""Compact beam search (altitudes stored as bytes with offset) + optional potential term."""
import sys, time, pickle, array
from core import *
from phaseA_state import phaseA_state
from st import State
from beam5 import finish_search

OFF = 160
KN = (ix('f7'), ix('g6'))
OR = (ix('g8'), ix('h7'))
D2 = (ix('f8'), ix('h6'))


def enc(alt):
    return bytes(a + OFF for a in alt)


def dec(b):
    return [a - OFF for a in b]


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


def potential(alt, vis, p):
    """unused visits on points within 2 of knight level, weighted by group size."""
    L = alt[p]
    cnt = {}
    for i in range(64):
        a = alt[i]
        if L - 2 <= a <= L + 2:
            cnt[a] = cnt.get(a, 0) + 1
    pot = 0
    for i in range(64):
        a = alt[i]
        if a in cnt and vis[i] < 3:
            pot += (3 - vis[i]) * cnt[a]
    return pot


def run(s0, W, lam=0.0, maxdepth=160, out='best8.pkl', log=print, topk=8, cap=6000):
    hist = []
    beam = [(s0.T, s0.p, enc(s0.alt), bytes(s0.vis))]
    prefix = list(s0.moves)
    best = (0, None)
    t0 = time.time()

    def moves_of(depth, idx):
        ms = []
        while depth >= 0 and idx >= 0:
            par, tt, qq = hist[depth]
            ms.append((tt[idx], nm(qq[idx])))
            idx = par[idx]
            depth -= 1
        return prefix + ms[::-1]

    for d in range(maxdepth):
        cand = {}
        for bi, (T, p, ab, vis) in enumerate(beam):
            alt = dec(ab)
            A = alt[p]
            O = 63 - p
            g = [i for i in range(64) if alt[i] == A and i != O]
            rise = alt[O] != A
            n = len(g)
            for delta, q in moves_pinned(p, alt, vis, allow_h8=True):
                t = n * delta
                T2 = T + t
                if q == H8:
                    if T2 > best[0]:
                        ms = moves_of(d - 1, bi) + [(t, 'h8')]
                        best = (T2, ms)
                        pickle.dump(ms, open(out, 'wb'))
                    continue
                if delta:
                    a2 = list(alt)
                    for i in g:
                        a2[i] -= delta
                    if rise:
                        a2[O] += delta
                else:
                    a2 = alt
                v2 = bytearray(vis)
                v2[q] += 1
                if d >= 1 and not fin_ok(a2, v2, q):
                    continue
                k = (q, enc(a2), bytes(v2))
                old = cand.get(k)
                if old is None or old[0] < T2:
                    sc = T2 + (lam * potential(a2, v2, q) if lam else 0)
                    cand[k] = (T2, bi, t, sc)
        if not cand:
            break
        items = sorted(cand.items(), key=lambda kv: -kv[1][3])[:W]
        par = array.array('i')
        tt = array.array('i')
        qq = array.array('b')
        newbeam = []
        for i, ((q, ab, v2), (T2, bi, t, sc)) in enumerate(items):
            par.append(bi)
            tt.append(t)
            qq.append(q)
            newbeam.append((T2, q, ab, v2))
        hist.append((par, tt, qq))
        beam = newbeam
        del cand, items
        order = sorted(range(len(beam)), key=lambda i: -beam[i][0])[:topk]
        for i in order:
            T2, q, ab, v2 = beam[i]
            st = State(q, dec(ab), list(v2), T2, moves_of(d, i))
            fT, fs = finish_search(st, cap=cap)
            if fs is not None and fT > best[0]:
                best = (fT, fs.moves)
                pickle.dump(fs.moves, open(out, 'wb'))
        log(f'{d+1} beamtop {max(b[0] for b in beam)} best {best[0]} {time.time()-t0:.0f}s')
    return best


if __name__ == '__main__':
    W = int(sys.argv[1])
    lam = float(sys.argv[2])
    out = sys.argv[3]
    best = run(phaseA_state(), W, lam=lam, out=out, log=lambda m: print(m, flush=True))
    print('BEST', best[0])
