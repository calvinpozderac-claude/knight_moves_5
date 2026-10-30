"""Cheapest final ramp for target X: static path from a1's escape neighbour to h8.
Cost = sum |h - alt0| over path points (raises weighted RW).  Pair constraints
(alt(P)+alt(opp P) <= sigma0) are enforced between points on the path."""
import sys, heapq
from core import ALT0, NEIGH, nm, ix
SIG = {i: ALT0[i] + ALT0[63 - i] for i in range(64)}

def design(X, RW=2.0, g=0, maxlen=40):
    a1f = 17 - X - g
    pq = []
    cnt = 0
    for N, dz in NEIGH[0]:
        for d in dz:
            h = a1f + d
            if N == 63:
                continue
            c = (RW if h > ALT0[N] else 1) * abs(h - ALT0[N])
            heapq.heappush(pq, (c, cnt, N, h, (N,), ((N, h),))); cnt += 1
    seen = {}
    while pq:
        c, _, P, h, used, path = heapq.heappop(pq)
        if P == 63:
            return c, path
        key = (P, h, frozenset(x for x in used if 63 - x in used or True) if False else None)
        k2 = (P, h)
        if k2 in seen and seen[k2] <= c - 1e-9:
            continue
        seen[k2] = c
        if len(path) >= maxlen:
            continue
        for Q, dz in NEIGH[P]:
            if Q in used or Q == 0:
                continue
            for d in dz:
                if d < 0:
                    continue
                h2 = h + d
                if Q == 63:
                    if h2 != X:
                        continue
                    heapq.heappush(pq, (c, cnt, Q, h2, used + (Q,), path + ((Q, h2),))); cnt += 1
                    continue
                if h2 > X:
                    continue
                bad = any(R == 63 - Q and hr + h2 > SIG[Q] for R, hr in path)
                if bad:
                    continue
                c2 = c + (RW if h2 > ALT0[Q] else 1) * abs(h2 - ALT0[Q])
                heapq.heappush(pq, (c2, cnt, Q, h2, used + (Q,), path + ((Q, h2),))); cnt += 1
    return None

if __name__ == '__main__':
    for X in range(int(sys.argv[1]), int(sys.argv[2]) + 1):
        out = []
        for g in (0, 1, 2, 3):
            r = design(X, g=g)
            if r:
                out.append((r[0], g, r[1]))
        out.sort()
        for c, g, path in out[:1]:
            ch = [(nm(P), h - ALT0[P]) for P, h in path if h != ALT0[P] and P != 63]
            print(f'X={X} g={g} cost={c:.0f} len={len(path)}: ' + ' '.join(f'{nm(P)}:{h}' for P, h in path))
            print('   changes:', ch, flush=True)
