"""Macro search: static walks + single sits (wait <= B) until h8 is statically reachable.
Usage: python3 macro.py B cap levels
"""
import sys
from collections import deque
from core import *

def reach(alt, vis, src, cap):
    prev = {src: None}
    dq = deque([src])
    while dq:
        u = dq.popleft()
        au = alt[u]
        for q, dz in NEIGH[u]:
            if q in prev or q == H8 or vis[q] >= cap:
                continue
            if (alt[q] - au) in dz:
                prev[q] = u
                dq.append(q)
    return prev

def path_to(prev, q):
    out = []
    while q is not None:
        out.append(q)
        q = prev[q]
    return out[::-1]

def h8_entry(alt, prev):
    for u in prev:
        for q, dz in NEIGH[u]:
            if q == H8 and (alt[H8] - alt[u]) in dz:
                return u
    return None

def group(alt, p):
    A = alt[p]; O = 63 - p
    g = [i for i in range(64) if alt[i] == A and i != O]
    return g, alt[O] != A

def search(B, cap, levels, kmax=12, width=None):
    vis0 = [0] * 64; vis0[A1] = 1
    start = (A1, tuple(ALT0), tuple(vis0), ())
    frontier = [start]
    seen = set()
    for lev in range(levels + 1):
        nxt = []
        for pos, alt, vis, moves in frontier:
            prev = reach(alt, vis, pos, cap)
            e = h8_entry(alt, prev)
            if e is not None:
                ms = list(moves) + [(0, nm(x)) for x in path_to(prev, e)[1:]] + [(0, 'h8')]
                return ms
            if lev == levels:
                continue
            for P in prev:
                g, rise = group(alt, P)
                n = len(g)
                pth = path_to(prev, P)
                for k in range(1, kmax + 1):
                    if n * k > B:
                        break
                    a2 = list(alt)
                    for i in g:
                        a2[i] -= k
                    if rise:
                        a2[63 - P] += k
                    v2 = list(vis)
                    for x in pth[1:]:
                        v2[x] += 1
                    for q, dz in NEIGH[P]:
                        if q != H8 and v2[q] >= cap:
                            continue
                        if (a2[q] - a2[P]) not in dz:
                            continue
                        # exit must be pinned (q not in sinking group) unless it is a group move
                        v3 = list(v2)
                        v3[q] += 1
                        ms = list(moves) + [(0, nm(x)) for x in pth[1:]] + [(n * k, nm(q))]
                        if q == H8:
                            return ms
                        key = (q, tuple(a2)) if cap > 1 else (q, tuple(a2), tuple(v3))
                        if key in seen:
                            continue
                        seen.add(key)
                        nxt.append((q, tuple(a2), tuple(v3), tuple(ms)))
        print('level', lev + 1, 'states', len(nxt), flush=True)
        frontier = nxt
    return None

if __name__ == '__main__':
    B, cap, levels = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    r = search(B, cap, levels)
    if r:
        print('FOUND', ', '.join(f'({t}, {q})' for t, q in r))
    else:
        print('none')
