#!/usr/bin/env python3
"""Validate sfl's exact necessity check against tools/check_tour.py.
usage: python3 val.py DUMPFILE...  (2-line records: minutes, tour). For each distinct tour: sfl's verdict
(CHECKONLY mode) vs check_tour (ok=True, maxvisits<=3, unnecessary=[]). Prints agreement counts and mismatches."""
import sys, os, re, subprocess, tempfile
from multiprocessing import Pool
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../tools'))
from check_tour import check
HERE = os.path.dirname(os.path.abspath(__file__))

def pyv(t):
    ok, T, mv, bad = check(t, show=False)
    return ok and mv <= 3 and bad == [], bad

tours = []
seen = set()
for fn in sys.argv[1:]:
    L = open(fn).read().split('\n')
    for i in range(len(L) - 1):
        if '(' not in L[i] and '(' in L[i + 1] and L[i + 1] not in seen:
            seen.add(L[i + 1]); tours.append(L[i + 1].strip())
with tempfile.NamedTemporaryFile('w', delete=False, suffix='.txt') as f:
    for t in tours: f.write(t + '\n')
    tmp = f.name
out = subprocess.run([os.path.join(HERE, 'sfl')], env=dict(os.environ, CHECKONLY=tmp), capture_output=True, text=True).stdout.split('\n')
mine = [l.split()[3] == 'OK' for l in out if re.match(r'^\d+ ', l)]
os.unlink(tmp)
assert len(mine) == len(tours), (len(mine), len(tours))
with Pool(int(os.environ.get('NP', '4'))) as p: res = p.map(pyv, tours)
agree = {}
mism = []
for t, m, (pv, bad) in zip(tours, mine, res):
    agree[(m, pv)] = agree.get((m, pv), 0) + 1
    if m != pv: mism.append((m, pv, bad, t))
print('tours', len(tours), 'verdicts (sfl, checker): count', agree)
for m, pv, bad, t in mism[:10]: print('MISMATCH sfl', m, 'checker', pv, bad, t[:80])
