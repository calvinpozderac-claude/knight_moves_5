#!/usr/bin/env python3
"""CP-SAT model for Altered States 2 (5x5).

Letter variables x[c][l]; for each state s a one-directional "reachability" encoding:
  r0[i][c] : prefix w[0..i] can end on square c with 0 alterations
  r1[i][c] : prefix w[0..i] can end on square c with <=1 alteration
  r0[i][c] -> x[c][w_i] & OR_{c' in N(c)} r0[i-1][c']
  r1[i][c] -> OR_{N(c)} r0[i-1]  OR  z[i][c];  z -> x[c][w_i];  z -> OR_{N(c)} r1[i-1]
  r1[0][c] == true (first letter altered),  y[s] -> OR_c r1[L-1][c]
All implications only point "downwards", so y[s]=1 is possible iff the state really fits.
"""
import csv
import os

from ortools.sat.python import cp_model

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
N = 5
CELLS = range(N * N)


def nbrs(c):
    r, k = divmod(c, N)
    return [rr * N + kk for rr in range(r - 1, r + 2) for kk in range(k - 1, k + 2)
            if 0 <= rr < N and 0 <= kk < N and (rr, kk) != (r, k)]


NB = [nbrs(c) for c in CELLS]


def load_states():
    with open(os.path.join(ROOT, "data", "states.csv")) as f:
        rows = [r for r in csv.reader(f) if r and not r[0].startswith("#")][1:]
    return [dict(name=r[0], pop=int(r[1]), code=r[2],
                 spell="".join(ch for ch in r[0].upper() if ch.isalpha())) for r in rows]


STATES = load_states()
BY_CODE = {s["code"]: s for s in STATES}
LETTERS = sorted(set("".join(s["spell"] for s in STATES)))


def build(codes=None, fixed=None, free=None, require=(), forbid_letters=None, hint=None,
          objective=True, min_score=None, symmetry=False):
    """codes: states in the model (default all). fixed: grid string; squares not in `free`
    are fixed to fixed's letter. require: codes that must be present. Returns (model, x, y)."""
    m = cp_model.CpModel()
    codes = codes or [s["code"] for s in STATES]
    x = {}
    for c in CELLS:
        if fixed is not None and (free is None or c not in free):
            L = fixed[c].upper()
            x[c] = {L: m.NewConstant(1)}
            continue
        x[c] = {L: m.NewBoolVar(f"x{c}{L}") for L in LETTERS}
        m.AddExactlyOne(x[c].values())

    def X(c, L):
        return x[c].get(L)

    y = {}
    for code in codes:
        w = BY_CODE[code]["spell"]
        ys = m.NewBoolVar(f"y{code}")
        y[code] = ys
        Lw = len(w)
        r0 = [[None] * 25 for _ in range(Lw)]
        r1 = [[None] * 25 for _ in range(Lw)]
        for i in range(Lw):
            for c in CELLS:
                xl = X(c, w[i])
                # r0
                if xl is not None and (i == 0 or any(r0[i - 1][d] is not None for d in NB[c])):
                    v = m.NewBoolVar("")
                    m.AddImplication(v, xl)
                    if i > 0:
                        m.AddBoolOr([r0[i - 1][d] for d in NB[c] if r0[i - 1][d] is not None]).OnlyEnforceIf(v)
                    r0[i][c] = v
                # r1
                if i == 0:
                    r1[i][c] = True
                    continue
                prev0 = [r0[i - 1][d] for d in NB[c] if r0[i - 1][d] is not None]
                prev1 = [r1[i - 1][d] for d in NB[c] if r1[i - 1][d] is not None]
                if not prev0 and (xl is None or not prev1):
                    continue
                v = m.NewBoolVar("")
                opts = list(prev0)
                if xl is not None and prev1:
                    if any(p is True for p in prev1):
                        z = xl
                    else:
                        z = m.NewBoolVar("")
                        m.AddImplication(z, xl)
                        m.AddBoolOr(prev1).OnlyEnforceIf(z)
                    opts.append(z)
                m.AddBoolOr(opts).OnlyEnforceIf(v)
                r1[i][c] = v
        last = [r1[Lw - 1][c] for c in CELLS if r1[Lw - 1][c] is not None]
        if not last:
            m.Add(ys == 0)
        else:
            m.AddBoolOr(last).OnlyEnforceIf(ys)
        if code in require:
            m.Add(ys == 1)
    pop = {s["code"]: s["pop"] for s in STATES}
    expr = sum(pop[k] * y[k] for k in y)
    if objective:
        m.Maximize(expr)
    if min_score is not None:
        m.Add(expr >= min_score)
    if hint:
        for c in CELLS:
            if len(x[c]) > 1:
                for L, v in x[c].items():
                    m.AddHint(v, int(hint[c].upper() == L))
    return m, x, y


def solution_grid(solver, x):
    out = []
    for c in CELLS:
        for L, v in x[c].items():
            if solver.Value(v):
                out.append(L.lower())
                break
    return "".join(out)
