"""Run insert.py (plain and --flex) on every tour in a two-line-per-tour file; print the best per tour.
usage (repo root): python3 subagents/fractional/sweep_many.py FILE"""
import sys, subprocess, os
L = open(sys.argv[1]).read().splitlines(); D = sys.argv[2] if len(sys.argv) > 2 else "subagents/fractional/tmp/sweep"
os.makedirs(D, exist_ok=True); best = (0, None)
for k in range(0, len(L) - 1, 2):
    fn = f'{D}/in_{k//2}.txt'; open(fn, 'w').write(L[k] + '\n' + L[k + 1] + '\n')
    for flex in ([], ['--flex']):
        out = f'{D}/out_{k//2}{"f" if flex else ""}.txt'
        r = subprocess.run(['python3', 'subagents/fractional/insert.py', fn, out] + flex, capture_output=True, text=True).stdout
        b = r.strip().splitlines()[-1]; v = int(b.split()[1].split('/')[0]) if '/' not in b.split()[1] else float(eval(b.split()[1]))
        print(L[k], 'flex' if flex else 'plain', b, flush=True)
        if v > best[0] and os.path.exists(out): best = (v, out)
print('BEST', best)
