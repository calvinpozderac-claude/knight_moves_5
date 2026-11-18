#!/usr/bin/env python3
# usage (from subagents/freesit): python3 newstrict.py FILE...  tours > 9282 in 2-line records that pass BOTH
# tools/check_tour.py (ok, maxvisits<=3, unnecessary=[]) AND strict_check (all positive waits pinned) -> STRICT_RECORD_<m>.txt
import sys, re, os, subprocess
sys.path.insert(0, '../../tools')
from sim import *
from fractions import Fraction as F
BEST = 9282
seen_f = 'tmp/checked_strict.txt'
seen = set(open(seen_f).read().split('\n')) if os.path.exists(seen_f) else set()
def strict(t):
    mv = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', t)]
    alt = init_alt(MAIN_ROWS); pos = parse('a1')
    for tt, q in mv:
        Q = parse(q)
        if tt.denominator != 1: return False
        if tt > 0:
            a2 = dict(alt); apply_wait(pos, tt + F(1, 997), a2, 8)
            if legal_jump(pos, Q, a2): return False
        apply_wait(pos, tt, alt, 8); pos = Q
    return True
cands = {}
for fn in sys.argv[1:]:
    try: L = open(fn).read().split('\n')
    except OSError: continue
    for i in range(len(L) - 1):
        if '(' not in L[i] and '(' in L[i + 1]:
            try: m = int(L[i].strip())
            except Exception: continue
            if m > BEST: cands[L[i + 1].strip()] = m
for t, m in sorted(cands.items(), key=lambda x: -x[1]):
    if t in seen: continue
    seen.add(t); open(seen_f, 'a').write(t + '\n')
    if not strict(t): print(m, 'not strict', flush=True); continue
    out = subprocess.run(['python3', '../../tools/check_tour.py', t], capture_output=True, text=True).stdout
    mv = re.search(r'maxvisits=(\d+)', out); Tm = re.search(r' T=(\S+)', out)
    ok = 'ok=True' in out and 'unnecessary=[]' in out and mv and int(mv.group(1)) <= 3 and Tm and int(Tm.group(1)) == m
    print(m, 'checker ok+strict' if ok else 'checker reject', flush=True)
    if ok:
        name = f'STRICT_RECORD_{m}.txt'
        if not os.path.exists(name): open(name, 'w').write(f'{m}\n{t}\n')
        print('STRICT RECORD', m, name, flush=True)
