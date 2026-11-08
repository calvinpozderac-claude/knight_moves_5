#!/usr/bin/env python3
"""Analyse elite boards from results/logs: state presence, near-misses (need 2), pattern counts."""
import glob, subprocess, collections, sys, os
sys.path.insert(0, '.')
from analyze import canon
N = int(sys.argv[1]) if len(sys.argv) > 1 else 50
best = {}
for f in glob.glob('results/logs/*.csv') + glob.glob('agents/targeted/logs/*.csv'):
    for line in open(f):
        p = line.strip().split(',')
        if len(p) < 2 or len(p[0]) != 25: continue
        c = canon(p[0]); best[c] = int(p[1])
top = sorted(best.items(), key=lambda kv: -kv[1])[:N]
codes = [l.split(',')[2] for l in open('data/states.csv') if l[0] != '#' and not l.startswith('name')]
pops = [int(l.split(',')[1]) for l in open('data/states.csv') if l[0] != '#' and not l.startswith('name')]
out = subprocess.run(['agents/targeted/as2t', 'need'], input='\n'.join(g for g, _ in top), capture_output=True, text=True).stdout.split('\n')
pres = collections.Counter(); near = collections.Counter(); far = collections.Counter()
rows = []
for line in out:
    if not line: continue
    p = line.split(); g = p[0]; n = list(map(int, p[1:]))
    rows.append((g, n))
    for i, k in enumerate(n):
        if k <= 1: pres[codes[i]] += 1
        elif k == 2: near[codes[i]] += 1
        else: far[codes[i]] += 1
print(f"top {len(rows)} distinct boards, scores {top[0][1]:,} .. {top[-1][1]:,}")
print("state  pop(M)  present  need2  need3+")
for i, c in enumerate(codes):
    print(f"{c:>4} {pops[i]/1e6:7.2f}  {pres[c]:5d}  {near[c]:5d}  {far[c]:5d}")
print()
for (g, s), (g2, n) in zip(top, rows):
    miss = [codes[i] for i, k in enumerate(n) if k == 2 and pops[i] > 2.5e6]
    have = [codes[i] for i, k in enumerate(n) if k <= 1 and pops[i] > 2.5e6 and codes[i] in ('PA','WA','MA','TN','MD','KY','CT','NV','MS')]
    print(f"{s:>12,} {g}  bigmissing-present:{','.join(have):20s} near2(big):{','.join(miss)}")
