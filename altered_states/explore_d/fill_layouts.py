#!/usr/bin/env python3
"""Exactly fill every free square ('.') of each layout with CP-SAT against all 50 states.
usage: fill_layouts.py LAYOUTS_FILE PART NPARTS SECONDS_PER_LAYOUT"""
import sys, os, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "cpsat"))
from ortools.sat.python import cp_model
import model
lay = [l.strip() for l in open(sys.argv[1]) if l.strip()]
part, nparts, T = int(sys.argv[2]), int(sys.argv[3]), float(sys.argv[4])
for i, L in enumerate(lay):
    if i % nparts != part: continue
    fixed = {c: L[c] for c in range(25) if L[c] != "."}
    m, x, present = model.build(fixed=fixed)
    s = cp_model.CpSolver(); s.parameters.max_time_in_seconds = T; s.parameters.num_workers = 1
    t0 = time.time(); st = s.Solve(m)
    g = model.grid_of(s, x) if st in (cp_model.OPTIMAL, cp_model.FEASIBLE) else "-"
    print(f"{i:4d} {L} cells={len(fixed)} {s.StatusName(st)} obj={s.ObjectiveValue():.0f} bound={s.BestObjectiveBound():.0f} t={time.time()-t0:.1f}s grid={g}", flush=True)
