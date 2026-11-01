#!/usr/bin/env python3
"""Summarise ab.sh logs: completed restarts per arm and anchor (PARTIAL ones dropped), median and spread,
plus an equal-time view: the stock arm's restarts grouped into blocks of k consecutive restarts, where k is
the ratio of the prior arm's to the stock arm's mean restart time; best of each block vs the prior's restarts."""
import re, sys, glob, statistics as st
logs = sys.argv[1:] or sorted(glob.glob('tmp/ab/*_a*.log'))
data = {}
for fn in logs:
    m = re.search(r'([^/]+)_a(\d+)\.log$', fn); arm, A = m.group(1), int(m.group(2))
    sc, ts = [], []
    for line in open(fn):
        r = re.match(r'\[\s*(\d+)s\] thread \d+ restart \d+ \(anchor \d+[^)]*\): (finished|unfinished) (\d+) minutes', line)
        if not r or 'PARTIAL' in line: continue
        ts.append(int(r.group(1))); sc.append(int(r.group(3)) if r.group(2) == 'finished' else 0)
    data[(arm, A)] = (sc, ts)
def q(v, p):
    v = sorted(v); k = (len(v) - 1) * p; i = int(k); return v[i] + (v[min(i + 1, len(v) - 1)] - v[i]) * (k - i)
def desc(v):
    if not v: return 'n=0'
    return f'n={len(v):3d} median {st.median(v):6.0f}  IQR {q(v,.25):5.0f}-{q(v,.75):5.0f}  min {min(v):5d} max {max(v):5d}'
for A in sorted({a for _, a in data}):
    print(f'anchor {A}')
    per = {}
    for (arm, a), (sc, ts) in sorted(data.items()):
        if a != A: continue
        dt = (ts[-1] / len(ts)) if ts else 0; per[arm] = (sc, dt)
        print(f'  {arm:12s} {desc(sc)}  ({dt:.0f} s/restart)')
    for arm in per:
        if arm == 'stock' or 'stock' not in per: continue
        ssc, sdt = per['stock']; psc, pdt = per[arm]
        if not sdt or not pdt: continue
        k = max(1, round(pdt / sdt)); blocks = [max(ssc[i:i + k]) for i in range(0, len(ssc) - k + 1, k)]
        print(f'  stock best-of-{k} blocks (equal time to one {arm} restart): {desc(blocks)}')
