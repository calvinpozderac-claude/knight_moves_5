"""Insert free sinks into a tour: at each zero-wait in-group knight move, try a sink of s units (s in
1/2..6 in half units), re-pin all later waits (same squares, same dz), and keep the tours that pass the
exact checker (ok, maxvisits<=3, unnecessary=[]). Then try pairs built from the best singles.
usage (repo root): python3 subagents/fractional/insert.py FILE [OUT]"""
import sys, re, itertools
from fractions import Fraction as F
sys.path.insert(0, 'tools'); sys.path.insert(0, 'subagents/fractional')
from sim import MAIN_ROWS, simulate, necessity_check
from repin import profile, rebuild, fmt
from collections import Counter


def good(m):
    ok, T, _ = simulate(MAIN_ROWS, m)
    if not ok: return None
    c = Counter(q for _, q in m); c['a1'] += 1
    if max(v for k, v in c.items() if k != 'h8') > 3: return None
    return T


mv = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', open(sys.argv[1]).read().splitlines()[1])]
prof = profile(mv); T0 = good(mv); print('base', T0)
spots = [i for i, (qs, dz, free, sink) in enumerate(prof) if free and sink == 0]
print('in-group zero-wait spots', [i + 1 for i in spots])
singles = []
for i in spots:
    for k in range(1, 13):
        s = F(k, 2); m2 = rebuild(prof, {i: s})
        if m2 is None: continue
        T = good(m2)
        if T is None or T <= T0: continue
        singles.append((T, i, s, m2))
singles.sort(key=lambda x: -x[0])
print('best raw singles', [(str(T), i + 1, str(s)) for T, i, s, _ in singles[:10]])
best = (T0, mv)
for T, i, s, m2 in singles[:30]:
    if T > best[0] and not necessity_check(MAIN_ROWS, m2): best = (T, m2); print('single ok', T, 'move', i + 1, 'sink', s)
cands = {}
for T, i, s, _ in singles: cands.setdefault(i, []).append(s)
for (i, si), (j, sj) in itertools.combinations([(i, s) for T, i, s, _ in singles[:40]], 2):
    if i == j: continue
    m2 = rebuild(prof, {i: si, j: sj})
    if m2 is None: continue
    T = good(m2)
    if T is None or T <= best[0]: continue
    if not necessity_check(MAIN_ROWS, m2): best = (T, m2); print('pair ok', T, 'moves', i + 1, j + 1, 'sinks', si, sj)
print('best', best[0])
if len(sys.argv) > 2 and best[0] > T0:
    open(sys.argv[2], 'w').write(f'{best[0]}\n{fmt(best[1])}\n')
