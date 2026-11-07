#!/usr/bin/env python3
"""Fix-and-optimise LNS: free a window of cells, fix the rest, solve exactly with CP-SAT.
python3 lns.py --grid G --time 600 [--seed 1]"""
import argparse, time, random, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model import *
sys.path.insert(0, HERE)
from score import score as pyscore, find_path

ap = argparse.ArgumentParser()
ap.add_argument("--grid", default="CHDORWATAKIRUNSISOIASMCLL")
ap.add_argument("--time", type=float, default=600)
ap.add_argument("--seed", type=int, default=1)
ap.add_argument("--win_time", type=float, default=60)
ap.add_argument("--workers", type=int, default=2)
a = ap.parse_args()
rng = random.Random(a.seed)

def sc(g):
    return pyscore([g[i:i+5] for i in range(0, 25, 5)])[0]

def blocks(h, w):
    return [[r * 5 + c for r in range(r0, r0 + h) for c in range(c0, c0 + w)]
            for r0 in range(6 - h) for c0 in range(6 - w)]

def window_gen(g):
    """yields (kind, cells) forever"""
    while True:
        wins = [("3x3", b) for b in blocks(3, 3)] + [("3x4", b) for b in blocks(3, 4)] \
             + [("4x3", b) for b in blocks(4, 3)]
        for _ in range(10):
            wins.append(("rand10", rng.sample(range(25), 10)))
        for _ in range(10):
            wins.append(("rand12", rng.sample(range(25), 12)))
        # cells on witness paths of 2 random present states + 3 random cells
        rows = [g[i:i+5] for i in range(0, 25, 5)]
        _, found = pyscore(rows)
        names = list(found)
        for _ in range(10):
            cells = set()
            for s in rng.sample(names, min(2, len(names))):
                cells |= {r * 5 + c for r, c in found[s]}
            cells |= set(rng.sample(range(25), 3))
            wins.append(("paths2", sorted(cells)))
        rng.shuffle(wins)
        for w in wins:
            yield w

t0 = time.time()
g = a.grid.upper()
best = sc(g)
print(f"start {best} {g}", flush=True)
stats = {}
seen_opt = set()
gen = window_gen(g)
while time.time() - t0 < a.time:
    kind, cells = next(gen)
    key = (g, tuple(sorted(cells)))
    if key in seen_opt:
        continue
    fixed = {c: g[c] for c in range(25) if c not in cells}
    m, x, present = build(fixed=fixed, hint=g)
    s = cp_model.CpSolver()
    s.parameters.max_time_in_seconds = min(a.win_time, a.time - (time.time() - t0) + 1)
    s.parameters.num_workers = a.workers
    st = s.Solve(m)
    ng = grid_of(s, x)
    obj = int(s.ObjectiveValue())
    k = stats.setdefault(kind, [0, 0, 0, 0.0])  # tried, proved-optimal, improved, time
    k[0] += 1; k[3] += s.WallTime()
    if st == cp_model.OPTIMAL:
        k[1] += 1
        seen_opt.add(key)
    if obj > best:
        k[2] += 1
        assert sc(ng) == obj, (ng, obj, sc(ng))
        best, g = obj, ng
        seen_opt = set()
        gen = window_gen(g)
        print(f"[{time.time()-t0:6.1f}s] IMPROVED {best} {g} via {kind} {sorted(cells)}", flush=True)
    else:
        print(f"[{time.time()-t0:6.1f}s] {kind:6s} n={len(cells):2d} {s.StatusName(st):8s} "
              f"obj {obj} bound {s.BestObjectiveBound():.0f} {s.WallTime():.1f}s", flush=True)
print("FINAL", best, g)
for kind, (n, opt, imp, t) in stats.items():
    print(f"  {kind:6s} tried {n:3d} proved-optimal {opt:3d} improved {imp} avg {t/max(n,1):.1f}s")
