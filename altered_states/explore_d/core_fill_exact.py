#!/usr/bin/env python3
"""Freeze the squares a set of 'core' states uses in a grid (their witness paths from
score.py) and let CP-SAT optimise every other square exactly against all 50 states."""
import sys, os, time, argparse
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "cpsat"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ortools.sat.python import cp_model
import model
from score import score
ap = argparse.ArgumentParser()
ap.add_argument("--grid", default="CHDORWATAKIRUNSISOIASMCLL")
ap.add_argument("--core", default="NORTHCAROLINA,SOUTHCAROLINA,NORTHDAKOTA,SOUTHDAKOTA")
ap.add_argument("--time", type=float, default=600)
ap.add_argument("--workers", type=int, default=2)
a = ap.parse_args()
g = a.grid
rows = [g[i:i+5] for i in range(0, 25, 5)]
tot, found = score(rows)
cells = set()
for s in a.core.split(","):
    cells |= {r * 5 + c for r, c in found[s]}
fixed = {c: g[c] for c in cells}
print(f"base {tot}; core {a.core}; {len(cells)} fixed cells, {25-len(cells)} free", flush=True)
print("fixed pattern:", "".join(g[c] if c in cells else "." for c in range(25)), flush=True)
m, x, present = model.build(fixed=fixed, hint=g)
solver = cp_model.CpSolver()
solver.parameters.max_time_in_seconds = a.time
solver.parameters.num_workers = a.workers
t0 = time.time()
st = solver.Solve(m, model.Logger(x, t0, None))
best = model.grid_of(solver, x)
print(f"status {solver.StatusName(st)} obj {solver.ObjectiveValue():.0f} bound {solver.BestObjectiveBound():.0f} "
      f"time {time.time()-t0:.1f}s grid {best}")
print("verified score.py:", score([best[i:i+5] for i in range(0, 25, 5)])[0])
