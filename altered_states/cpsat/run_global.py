#!/usr/bin/env python3
"""Global CP-SAT solve: python3 run_global.py --time 480 [--hint GRID] [--symmetry]"""
import argparse, time, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model import *
ap = argparse.ArgumentParser()
ap.add_argument("--time", type=float, default=300)
ap.add_argument("--hint")
ap.add_argument("--symmetry", action="store_true")
ap.add_argument("--workers", type=int, default=2)
ap.add_argument("--lp", action="store_true", help="solve LP relaxation only (GLOP)")
a = ap.parse_args()
t0 = time.time()
m, x, present = build(hint=a.hint, symmetry=a.symmetry)
print(f"model built in {time.time()-t0:.1f}s:", m.Proto().variables.__len__(), "vars,",
      len(m.Proto().constraints), "constraints", flush=True)
s = cp_model.CpSolver()
s.parameters.max_time_in_seconds = a.time
s.parameters.num_workers = a.workers
st = s.Solve(m, Logger(x, t0, None))
print("status", s.StatusName(st), "obj", s.ObjectiveValue(), "bound", s.BestObjectiveBound(),
      "wall", round(s.WallTime(), 1))
if st in (cp_model.OPTIMAL, cp_model.FEASIBLE):
    g = grid_of(s, x)
    print("BEST", int(s.ObjectiveValue()), g)
    print("states", [k for k, v in present.items() if s.Value(v)])
