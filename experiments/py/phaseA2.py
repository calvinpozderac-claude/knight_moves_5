import sys
from collections import deque
from core import *
from phaseA import static_reach, path_to

RAISES = {ix('c2'): (ix('f7'), 13), ix('b2'): (ix('g7'), 15), ix('b3'): (ix('g6'), 17)}

def succ(p, alt, vis, maxdelta):
    out = set()
    for delta, q in moves_pinned(p, alt, vis, allow_h8=False, maxdelta=maxdelta):
        out.add((delta, q))
    if p in RAISES:
        O, tgt = RAISES[p]
        r = tgt - alt[O]
        if r > 0 and alt[O] != alt[p]:
            a2, n, t = apply(p, r, alt)
            for q, dz in NEIGH[p]:
                if q == H8 or vis[q] >= 3: continue
                if (a2[q] - a2[p]) in dz:
                    out.add((r, q))
    return out

def search(maxdepth, maxdelta):
    start = (A1, tuple(ALT0), tuple(1 if i == A1 else 0 for i in range(64)))
    seen = {(A1, tuple(ALT0))}
    frontier = [(start, [])]
    for depth in range(maxdepth):
        nxt = []
        for (p, alt, vis), path in frontier:
            prev = static_reach(p, alt, vis)
            hits = [q for q in prev if alt[q] == alt[H8] and q != H8]
            if hits:
                return path, path_to(prev, hits[0])
            for delta, q in succ(p, alt, vis, maxdelta):
                a2, n, t = apply(p, delta, list(alt))
                key = (q, tuple(a2))
                if key in seen: continue
                seen.add(key)
                v2 = list(vis); v2[q] += 1
                nxt.append(((q, tuple(a2), tuple(v2)), path + [(t, nm(q))]))
        frontier = nxt
        print('depth', depth+1, len(frontier), flush=True)
    return None

print(search(int(sys.argv[1]), int(sys.argv[2])))
