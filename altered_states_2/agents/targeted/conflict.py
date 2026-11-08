#!/usr/bin/env python3
"""For boards in the logs scoring >= threshold: for each hard state X, best score with X, and
which other states are most under-represented when X is present (conflict)."""
import glob, subprocess, sys
sys.path.insert(0, '.')
from analyze import canon
thr = float(sys.argv[1]) if len(sys.argv) > 1 else 255e6
best = {}
for f in glob.glob('results/logs/*.csv') + glob.glob('agents/targeted/logs/*.csv'):
    for line in open(f):
        p = line.strip().split(',')
        if len(p) < 2 or len(p[0]) != 25: continue
        if int(p[1]) >= thr: best[canon(p[0])] = int(p[1])
rows = [l.split(',') for l in open('data/states.csv') if l[0] != '#' and not l.startswith('name')]
codes = [r[2] for r in rows]; pops = [int(r[1]) for r in rows]
gs = list(best)
out = subprocess.run(['agents/targeted/as2t', 'need'], input='\n'.join(gs), capture_output=True, text=True).stdout.split('\n')
H = {}
for line in out:
    if line: p = line.split(); H[p[0]] = [int(x) <= 1 for x in p[1:]]
print(f"{len(gs)} distinct boards >= {thr/1e6:.0f}M")
for X in ['PA','WA','MA','TN','MD','KY','CT','NV','MS','NM','NE','OK','WV','HI','MT','AK','DE','VT','WY','ID','MO','CO','SC','AL','NJ','UT','WI','MN']:
    xi = codes.index(X)
    w = [g for g in gs if H[g][xi]]; wo = [g for g in gs if not H[g][xi]]
    if not w: print(f"{X}: never present"); continue
    bw = max(w, key=lambda g: best[g])
    rate = lambda S, i: sum(H[g][i] for g in S) / len(S)
    conf = sorted(range(len(codes)), key=lambda i: (rate(w, i) - rate(wo, i)) * pops[i])[:5]
    print(f"{X}: {len(w):4d} boards, best {best[bw]:,} {bw}; conflicts: " +
          ' '.join(f"{codes[i]}({rate(w,i):.2f}vs{rate(wo,i):.2f})" for i in conf))
