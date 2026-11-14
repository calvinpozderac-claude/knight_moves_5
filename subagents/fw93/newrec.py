#!/usr/bin/env python3
# usage: python3 newrec.py FILE... (2-line tours: minutes, tour). Every distinct tour above 9282 that
# tools/check_tour.py accepts (ok=True, maxvisits<=3, unnecessary=[]) -> NEW_RECORD_<m>.txt (run from subagents/fw93)
import sys, subprocess, re, os
from fractions import Fraction
BEST = Fraction(9282)
seen_f = 'tmp/checked.txt'
seen = set(open(seen_f).read().split('\n')) if os.path.exists(seen_f) else set()
cands = {}
for fn in sys.argv[1:]:
    try: L = open(fn).read().split('\n')
    except OSError: continue
    for i in range(len(L) - 1):
        if '(' not in L[i] and '(' in L[i + 1]:
            try: m = Fraction(L[i].strip())
            except Exception: continue
            if m > BEST: cands[L[i + 1].strip()] = m
for t, m in sorted(cands.items(), key=lambda x: -x[1]):
    if t in seen: continue
    out = subprocess.run(['python3', '../../tools/check_tour.py', t], capture_output=True, text=True).stdout
    seen.add(t); open(seen_f, 'a').write(t + '\n')
    mv = re.search(r'maxvisits=(\d+)', out)
    ok = 'ok=True' in out and 'unnecessary=[]' in out and mv and int(mv.group(1)) <= 3
    print(f'{float(m):.3f} checker: {out.strip().splitlines()[-1] if out.strip() else "?"} -> {"ACCEPT" if ok else "reject"}')
    if ok:
        name = f'NEW_RECORD_{str(m).replace("/", "_")}.txt'
        if not os.path.exists(name): open(name, 'w').write(f'{m}\n{t}\n')
        print('NEW RECORD', m, name)
