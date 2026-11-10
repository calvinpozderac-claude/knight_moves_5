"""Turn free-wait tours into strict ones: for every free wait, set its sink to 0 (and to each integer
0..4), re-pin all later waits (same squares, same dz), then keep variants that pass check_tour and have
no free waits left. All subsets of up to 3 free waits are zeroed together.
usage (repo root): python3 subagents/fractional/postproc.py POOLFILE... -> prints the best strict and best free variants"""
import sys, re, itertools
from fractions import Fraction as F
sys.path.insert(0, 'tools'); sys.path.insert(0, 'subagents/fractional')
from sim import MAIN_ROWS, init_alt, parse, apply_wait, legal_jump, simulate, necessity_check
from repin import profile, rebuild, fmt
from collections import Counter


def free_idx(moves):
    return [i for i, (qs, dz, free, sink) in enumerate(profile(moves)) if free and sink > 0]


def valid(m):
    ok, T, _ = simulate(MAIN_ROWS, m)
    if not ok: return None
    c = Counter(q for _, q in m); c['a1'] += 1
    if max(v for k, v in c.items() if k != 'h8') > 3: return None
    return T


tours = set()
for fn in sys.argv[1:]:
    for line in open(fn):
        if '(' in line: tours.add(line.strip())
best_strict = (0, None); best_free = (0, None); seen = set()
for txt in tours:
    mv = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', txt)]
    T0 = valid(mv)
    if T0 is None: continue
    fi = free_idx(mv)
    if not fi:
        if T0 > best_strict[0] and not necessity_check(MAIN_ROWS, mv): best_strict = (T0, fmt(mv))
        continue
    if T0 > best_free[0] and not necessity_check(MAIN_ROWS, mv): best_free = (T0, fmt(mv))
    prof = profile(mv)
    for r in range(1, min(3, len(fi)) + 1):
        for sub in itertools.combinations(fi, r):
            m2 = rebuild(prof, {i: F(0) for i in sub})
            if m2 is None: continue
            T = valid(m2)
            if T is None or T <= best_strict[0]: continue
            key = fmt(m2)
            if key in seen: continue
            seen.add(key)
            if free_idx(m2): continue
            if necessity_check(MAIN_ROWS, m2): continue
            best_strict = (T, key)
print('best strict', best_strict[0]); print(best_strict[1])
print('best free', best_free[0]); print(best_free[1])
