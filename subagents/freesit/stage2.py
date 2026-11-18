#!/usr/bin/env python3
# stage2.py OUTTAG N FILES... : score every distinct tour in FILES at its dance start (DSSCORE, DSMIN 61), keep distinct
# dance-start states, pick the top N by T(ds) and the top N by pot, run T-ranked BLOCKBEAM W 20000 from ds of each.
import sys, subprocess, os
tag, N = sys.argv[1], int(sys.argv[2]); files = sys.argv[3:]
tours = {}
for fn in files:
    try: L = open(fn).read().split('\n')
    except OSError: continue
    for l in L:
        if '(' in l: tours[l.strip()] = 1
os.makedirs('st2', exist_ok=True)
allf = f'st2/{tag}_all.txt'; T = list(tours)
open(allf, 'w').write('\n'.join(T) + '\n')
env = dict(os.environ, DSMIN='61', DSSCORE=allf)
out = subprocess.run(['./sfl_bb2', '--tours', 'in/s9329.txt', '--start', 'in/s9329.txt', '--free', '0'], capture_output=True, text=True, env=env).stdout
rows = []
for l in out.split('\n'):
    p = l.split()
    if len(p) == 8 and p[1] != '-1': rows.append((int(p[0]), int(p[1]), int(p[2]), float(p[3]), int(p[4]), int(p[5]), int(p[6]), p[7]))
best = {}
for r in rows:
    if r[7] not in best or best[r[7]][6] < r[6]: best[r[7]] = r
R = list(best.values())
print(f'{len(T)} tours, {len(R)} distinct dance-start states', flush=True)
sel = {}
for r in sorted(R, key=lambda r: -r[2])[:N]: sel[r[7]] = r
for r in sorted(R, key=lambda r: -r[3])[:N]: sel[r[7]] = r
for r in sorted(R, key=lambda r: -(r[2] + 40 * r[5]))[:N]: sel[r[7]] = r
print('selected', len(sel), 'top T(ds)', sorted((r[2] for r in R), reverse=True)[:8], 'max final', max(r[6] for r in R), flush=True)
for h, r in sel.items():
    fn = f'st2/{tag}_{h}.txt'; open(fn, 'w').write(f'{r[6]}\n{T[r[0]]}\n')
    env2 = dict(os.environ, BBT='1', BLOCKBEAM='20000', BBK=str(r[1]))
    o = subprocess.run(['nice', '-n', '5', './sfl_bb2', '--tours', fn, '--start', fn, '--xover', '0', '--free', '0', '--out', f'runs/s2_{tag}_{h}_best.txt'], capture_output=True, text=True, env=env2).stdout
    res = [l for l in o.split('\n') if 'BLOCKBEAM from' in l]
    print(h, 'ds', r[1], 'Tds', r[2], 'pot', int(r[3]), 'spare', r[4], 'pairs', r[5], 'final', r[6], '->', res[0].split('best ')[1] if res else '?', flush=True)
