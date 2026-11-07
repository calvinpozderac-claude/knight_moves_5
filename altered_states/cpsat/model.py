#!/usr/bin/env python3
"""CP-SAT model for Altered States.

x[c][l]  cell c holds letter l (exactly one of the 25 letters used by state names)
y[s][i][c]  position i of state s can sit on cell c via a valid King's walk
  y[s][i][c] => x[c][s_i];  y[s][i][c] => OR_{d adj c, d != c} y[s][i-1][d]
present[s] => OR_c y[s][L-1][c];  maximise sum L_s * present[s].
"""
import os, sys, time, random, argparse
from ortools.sat.python import cp_model

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATES = open(os.path.join(HERE, "states.txt")).read().split()
LETTERS = "ABCDEFGHIJKLMNOPRSTUVWXYZ"
NB = [[(r2 * 5 + c2) for r2 in range(max(0, r - 1), min(5, r + 2))
       for c2 in range(max(0, c - 1), min(5, c + 2)) if (r2, c2) != (r, c)]
      for r in range(5) for c in range(5)]


def d4(cell, t):
    r, c = divmod(cell, 5)
    for _ in range(t % 4):
        r, c = c, 4 - r
    if t >= 4:
        c = 4 - c
    return r * 5 + c


def build(fixed=None, hint=None, states=STATES, symmetry=False, force=None,
          objective=True, redundant=True):
    """fixed: dict cell->letter to fix. hint: 25-char grid. force: states that must be present."""
    m = cp_model.CpModel()
    x = [{l: m.NewBoolVar(f"x{c}{l}") for l in LETTERS} for c in range(25)]
    for c in range(25):
        m.AddExactlyOne(x[c].values())
    if fixed:
        for c, l in fixed.items():
            m.Add(x[c][l] == 1)
    present = {}
    for s in states:
        L = len(s)
        prev = None
        for i, ch in enumerate(s):
            cur = []
            for c in range(25):
                if fixed and c in fixed and fixed[c] != ch:
                    cur.append(None)
                    continue
                v = m.NewBoolVar(f"y{s}{i}_{c}")
                m.AddImplication(v, x[c][ch])
                if prev is not None:
                    ps = [prev[d] for d in NB[c] if prev[d] is not None]
                    if not ps:
                        m.Add(v == 0)
                    else:
                        m.AddBoolOr(ps).OnlyEnforceIf(v)
                cur.append(v)
            prev = cur
        p = m.NewBoolVar(f"p{s}")
        last = [v for v in prev if v is not None]
        if last:
            m.AddBoolOr(last).OnlyEnforceIf(p)
        else:
            m.Add(p == 0)
        if redundant:
            for ch in set(s):
                m.AddBoolOr([x[c][ch] for c in range(25)]).OnlyEnforceIf(p)
            # letter multiplicity: a letter repeated in s with an *adjacent* repeat
            # (e.g. SS) needs >= 2 cells holding it
            for a, b in zip(s, s[1:]):
                if a == b:
                    m.Add(sum(x[c][a] for c in range(25)) >= 2).OnlyEnforceIf(p)
        present[s] = p
    if force:
        for s in force:
            m.Add(present[s] == 1)
    if objective:
        m.Maximize(sum(len(s) * present[s] for s in states))
    if hint:
        for c in range(25):
            for l in LETTERS:
                m.AddHint(x[c][l], int(hint[c] == l))
    if symmetry:
        # lexicographic-ish symmetry breaking on corner letters: corner 0 letter index
        # <= every other corner's letter index (weak D4 breaking, sound)
        idx = [sum(LETTERS.index(l) * x[c][l] for l in LETTERS) for c in range(25)]
        for c in (4, 20, 24):
            m.Add(idx[0] <= idx[c])
    return m, x, present


def grid_of(solver, x):
    return "".join(next(l for l in LETTERS if solver.Value(x[c][l])) for c in range(25))


class Logger(cp_model.CpSolverSolutionCallback):
    def __init__(self, x, t0, log):
        super().__init__(); self.x = x; self.t0 = t0; self.log = log
    def on_solution_callback(self):
        g = "".join(next(l for l in LETTERS if self.Value(self.x[c][l])) for c in range(25))
        msg = f"[{time.time()-self.t0:7.1f}s] obj {self.ObjectiveValue():.0f} bound {self.BestObjectiveBound():.0f} {g}"
        print(msg, flush=True)
