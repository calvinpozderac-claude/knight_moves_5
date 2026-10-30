"""Phase A: get the knight onto a point (not h8) whose altitude equals h8's.
Moves: zero-wait jumps, immediate-pinned waits, and 'raise' waits that lift the
opposite point to a value that lines it up with one of its neighbours.
"""
import sys
from collections import deque
from core import *

TOPRIGHT = set(i for i in range(64) if i % 8 >= 4 and i // 8 >= 4)


def static_reach(p, alt, visits):
    """BFS over zero-wait jumps. Returns dict node->path(list of nodes)."""
    prev = {p: None}
    dq = deque([p])
    while dq:
        u = dq.popleft()
        for q, dz in NEIGH[u]:
            if q == H8 or q in prev or visits[q] >= 3:
                continue
            if (alt[q] - alt[u]) in dz:
                prev[q] = u
                dq.append(q)
    return prev


def path_to(prev, q):
    out = []
    while q is not None:
        out.append(q)
        q = prev[q]
    return out[::-1]


def raise_targets(p, alt):
    O = 63 - p
    if O not in TOPRIGHT:
        return []
    vals = set()
    for N, dz in NEIGH[O]:
        for d in dz:
            vals.add(alt[N] + d)
            vals.add(alt[N] - d)
    vals.add(alt[H8])
    return sorted(v - alt[O] for v in vals if v > alt[O] and v - alt[O] <= 8)


def succ(p, alt, visits, maxdelta=3):
    out = []
    for delta, q in moves_pinned(p, alt, visits, allow_h8=False, maxdelta=maxdelta):
        out.append((delta, q))
    if alt[63 - p] != alt[p]:
        for r in raise_targets(p, alt):
            a2, n, t = apply(p, r, alt)
            for q, dz in NEIGH[p]:
                if q == H8 or visits[q] >= 3:
                    continue
                if (a2[q] - a2[p]) in dz:
                    out.append((r, q))
    return list(set(out))


def search(maxdepth=14, maxstates=3_000_000):
    start = (A1, tuple(ALT0), tuple(1 if i == A1 else 0 for i in range(64)))
    seen = {(A1, tuple(ALT0))}
    frontier = [(start, [])]
    for depth in range(maxdepth):
        nxt = []
        for (p, alt, vis), path in frontier:
            # goal test
            prev = static_reach(p, alt, vis)
            hits = [q for q in prev if alt[q] == alt[H8] and q != H8]
            if hits:
                return path, (p, alt, vis), path_to(prev, hits[0])
            for delta, q in succ(p, alt, vis):
                a2, n, t = apply(p, delta, list(alt))
                key = (q, tuple(a2))
                if key in seen:
                    continue
                seen.add(key)
                v2 = list(vis)
                v2[q] += 1
                nxt.append(((q, tuple(a2), tuple(v2)), path + [(t, nm(q))]))
        frontier = nxt
        print('depth', depth + 1, 'frontier', len(frontier), 'seen', len(seen), flush=True)
        if len(seen) > maxstates:
            break
    return None


if __name__ == '__main__':
    r = search(int(sys.argv[1]) if len(sys.argv) > 1 else 10)
    print(r)
