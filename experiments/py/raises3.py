import sys
from collections import deque
from core import *

NB = [[(q, set(dz)) for q, dz in NEIGH[i]] for i in range(64)]
def reach(alt, srcs):
    seen = set(srcs)
    dq = deque(srcs)
    while dq:
        u = dq.popleft()
        au = alt[u]
        for q, dz in NB[u]:
            if q in seen or q == H8: continue
            if (alt[q] - au) in dz:
                seen.add(q); dq.append(q)
    return seen

def do(alt, s, k):
    a, n, t = apply(s, k, alt)
    ex = [q for q, dz in NB[s] if q != H8 and (a[q]-a[s]) in dz]
    if not ex: return None
    return a, t, reach(a, ex)

SITS = [i for i in range(64) if i % 8 <= 4 and i // 8 <= 4 and i != A1]
def hits(a, R):
    return [q for q in R if a[q] == a[H8]]

if __name__ == "__main__":
    L = [(tuple(ALT0), 0, [], frozenset(reach(ALT0, [A1])))]
    found = []
    seen = set()
    for level in range(3):
        nxt = []
        for alt, T, plan, R in L:
            alt = list(alt)
            for s in SITS:
                if s not in R: continue
                ks = range(1, 9)
                if level == 2:
                    o = 63 - s
                    ks = [17 - alt[o]] if 0 < 17 - alt[o] <= 12 else []
                    has17 = any(alt[q] == alt[H8] for q in range(63))
                    if has17: ks = range(1, 9)
                for k in ks:
                    r = do(alt, s, k)
                    if r is None: continue
                    a, t, R2 = r
                    key = tuple(a)
                    if key in seen: continue
                    seen.add(key)
                    p2 = plan + [(nm(s), k)]
                    h = hits(a, R2)
                    if h:
                        found.append((T + t, p2, [nm(x) for x in h]))
                        continue
                    nxt.append((key, T + t, p2, frozenset(R2)))
        print('level', level + 1, len(nxt), 'found', len(found), flush=True)
        L = nxt
    found.sort()
    for f in found[:30]: print(f)
