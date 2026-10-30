import itertools, sys
from collections import deque
from core import *

def reach(alt, src, avoid=(H8,)):
    seen = {src}
    dq = deque([src])
    while dq:
        u = dq.popleft()
        for q, dz in NEIGH[u]:
            if q in seen or q in avoid: continue
            if (alt[q] - alt[u]) in dz:
                seen.add(q); dq.append(q)
    return seen

def sink(alt, s, k):
    a, n, t = apply(s, k, alt)
    return a, t

def exits(alt, s):
    """points reachable from s by one zero-wait jump (after the wait)."""
    return [q for q, dz in NEIGH[s] if q != H8 and (alt[q]-alt[s]) in dz]

results = []
start_reach = reach(ALT0, A1)
cands = [(s, k) for s in range(64) if s not in (A1, H8) for k in range(1, 9)]
# depth-2 plans
for (s1, k1) in cands:
    if s1 not in start_reach: continue
    a1_, t1 = sink(ALT0, s1, k1)
    ex1 = exits(a1_, s1)
    if not ex1: continue
    r1 = set()
    for e in ex1: r1 |= reach(a1_, e)
    # check direct success
    tgt = alt17 = a1_[H8]
    hits = [q for q in r1 if a1_[q] == a1_[H8] and q != H8]
    if hits:
        results.append((t1, [(nm(s1), k1)], nm(hits[0])))
        continue
    for (s2, k2) in cands:
        if s2 not in r1 or s2 == s1: continue
        a2_, t2 = sink(a1_, s2, k2)
        ex2 = exits(a2_, s2)
        r2 = set()
        for e in ex2: r2 |= reach(a2_, e)
        hits = [q for q in r2 if a2_[q] == a2_[H8] and q != H8]
        if hits:
            results.append((t1+t2, [(nm(s1), k1), (nm(s2), k2)], nm(hits[0])))
results.sort()
print(len(results))
for r in results[:40]: print(r)
