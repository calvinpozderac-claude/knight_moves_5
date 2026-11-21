#!/usr/bin/env python3
# gather.py OUT MINT DIRS... : every distinct tour (lines with '(') of >= MINT minutes under DIRS; strict ones with descriptor
# -> OUT (tsv: T, ds, lv, first, nsit, sitters, op8, file, tour)
import sys, os, re, glob
from desc import desc, moves
out, mint = sys.argv[1], int(sys.argv[2]); seen = set(); rows = []
for d in sys.argv[3:]:
    for root, _, files in os.walk(d):
        for f in files:
            if not f.endswith('.txt'): continue
            p = os.path.join(root, f)
            try:
                if os.path.getsize(p) > 20_000_000: continue
                L = open(p, errors='ignore').read().split('\n')
            except OSError: continue
            for l in L:
                if '(' not in l or 'h8)' not in l: continue
                l = l.strip()
                if l in seen: continue
                seen.add(l)
                mv = moves(l)
                if not mv or mv[-1][1] != 'h8' or sum(w for w, _ in mv) < mint: continue
                try: dd = desc(l)
                except Exception: continue
                if not dd['strict']: continue
                rows.append((int(dd['T']), dd['ds'], dd['lv'], dd['first'], len(dd['sit']), ','.join(sorted(dd['sit'])), ' '.join(dd['op'][:8]), p, l))
rows.sort(key=lambda r: -r[0])
with open(out, 'w') as fo:
    for r in rows: fo.write('\t'.join(str(x) for x in r) + '\n')
print(len(seen), 'distinct lines,', len(rows), 'strict tours >=', mint)
