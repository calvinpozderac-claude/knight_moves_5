#!/usr/bin/env python3
"""Table: for each constrained log, restarts, how many satisfied the constraint, best true score among those."""
import glob, os
for f in sorted(glob.glob(os.path.join(os.path.dirname(__file__) or '.', 'logs', '*.csv'))):
    name = os.path.basename(f)[:-4]
    kind, codes = name.split('_', 1)
    codes = codes.split('_')
    rows = [l.strip().split(',') for l in open(f) if l.strip()]
    ok = []
    for g, s, n, o, lst in rows:
        have = set(lst.split())
        sat = all(c in have for c in codes) if kind == 'req' else all(c not in have for c in codes)
        if sat: ok.append((int(s), g, n))
    ok.sort(reverse=True)
    b = ok[0] if ok else (0, '-', 0)
    print(f"{name:12s} restarts {len(rows):4d} satisfied {len(ok):4d}  best {b[0]:>12,} ({b[2]} states) {b[1]}")
