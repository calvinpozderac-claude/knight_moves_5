#!/usr/bin/env python3
"""Whole-grid CP-SAT with the four Carolina/Dakota states forced present."""
import sys, os, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "cpsat"))
from ortools.sat.python import cp_model
import model
T = float(sys.argv[1]) if len(sys.argv) > 1 else 900
m, x, present = model.build(hint="CHDORWATAKIRUNSISOIASMCLL", symmetry=True,
                            force=["NORTHCAROLINA", "SOUTHCAROLINA", "NORTHDAKOTA", "SOUTHDAKOTA"])
s = cp_model.CpSolver(); s.parameters.max_time_in_seconds = T; s.parameters.num_workers = 2
s.parameters.log_search_progress = False
t0 = time.time()
st = s.Solve(m, model.Logger(x, t0, None))
print(f"status {s.StatusName(st)} obj {s.ObjectiveValue():.0f} bound {s.BestObjectiveBound():.0f} time {time.time()-t0:.0f}s grid {model.grid_of(s, x)}")
