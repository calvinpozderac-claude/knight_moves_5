"""Macro search with altitudes scaled by SC (so sinks of 1/SC units are allowed).
usage: macro_half.py SC Bnum Bden levels   -> max wait <= Bnum/Bden minutes"""
import sys
from fractions import Fraction as F
from collections import deque
from core import ALT0, NEIGH as N0, nm
SC = int(sys.argv[1]); BMAX = F(int(sys.argv[2]), int(sys.argv[3])); LEV = int(sys.argv[4])
H8, A1 = 63, 0
NEIGH = [[(q, tuple(d * SC for d in dz)) for q, dz in N0[i]] for i in range(64)]
A0 = tuple(a * SC for a in ALT0)
def reach(alt, src):
    prev = {src: None}; dq = deque([src])
    while dq:
        u = dq.popleft()
        for q, dz in NEIGH[u]:
            if q in prev or q == H8: continue
            if alt[q] - alt[u] in dz: prev[q] = u; dq.append(q)
    return prev
def path_to(prev, q):
    out = []
    while q is not None: out.append(q); q = prev[q]
    return out[::-1]
seen = set(); frontier = [(A1, A0, ())]
for lev in range(LEV):
    nxt = []
    for pos, alt, moves in frontier:
        prev = reach(alt, pos)
        for u in prev:
            for q, dz in NEIGH[u]:
                if q == H8 and alt[H8] - alt[u] in dz:
                    ms = list(moves) + [(F(0), nm(x)) for x in path_to(prev, u)[1:]] + [(F(0), 'h8')]
                    print('FOUND', ', '.join(f'({t}, {s})' for t, s in ms)); sys.exit()
        for P in prev:
            A = alt[P]; O = 63 - P
            g = [i for i in range(64) if alt[i] == A and i != O]; rise = alt[O] != A; n = len(g)
            pth = path_to(prev, P)
            k = 1
            while F(n * k, SC) <= BMAX:
                a2 = list(alt)
                for i in g: a2[i] -= k
                if rise: a2[O] += k
                for q, dz in NEIGH[P]:
                    if q == H8 or a2[q] - a2[P] not in dz: continue
                    key = (q, tuple(a2))
                    if key in seen: continue
                    seen.add(key)
                    ms = list(moves) + [(F(0), nm(x)) for x in pth[1:]] + [(F(n * k, SC), nm(q))]
                    nxt.append((q, tuple(a2), tuple(ms)))
                k += 1
    frontier = nxt
    if not nxt: break
print('none (levels explored:', lev + 1, ')')
