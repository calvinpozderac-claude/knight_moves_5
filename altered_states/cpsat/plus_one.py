#!/usr/bin/env python3
"""Subset feasibility: for each state t absent from GRID, is there any 5x5 grid containing
all of GRID's states plus t?  python3 plus_one.py --grid G --time 20"""
import argparse, time, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model import *
sys.path.insert(0, HERE)
from score import score as pyscore
ap = argparse.ArgumentParser()
ap.add_argument("--grid", default="CHDORWATAKIRUNSISOIASMCLL")
ap.add_argument("--time", type=float, default=20)
ap.add_argument("--workers", type=int, default=1)
a = ap.parse_args()
rows = [a.grid[i:i+5] for i in range(0, 25, 5)]
base, found = pyscore(rows)
base_set = sorted(found)
print("base", base, base_set, flush=True)
res = {}
for t in sorted(set(STATES) - set(base_set), key=len, reverse=True):
    sub = base_set + [t]
    m, x, present = build(states=sub, force=sub, objective=False, symmetry=True)
    s = cp_model.CpSolver()
    s.parameters.max_time_in_seconds = a.time
    s.parameters.num_workers = a.workers
    st = s.Solve(m)
    name = s.StatusName(st)
    extra = ""
    if st in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        g = grid_of(s, x)
        extra = f"{g} full score {pyscore([g[i:i+5] for i in range(0,25,5)])[0]}"
    res[t] = name
    print(f"{t:14s} +{len(t):2d} {name:10s} {s.WallTime():5.1f}s {extra}", flush=True)
print("SUMMARY", {k: sum(1 for v in res.values() if v == k) for k in set(res.values())})
