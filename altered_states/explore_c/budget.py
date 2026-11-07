#!/usr/bin/env python3
"""Letter-budget lower bound on the number of cells a set of states needs.

A copy of letter X sits on a square with at most 8 neighbours, so if X is adjacent (in the
spelling) to d distinct letters across the chosen states, X needs at least ceil(d/8) copies;
a doubled letter (SS, LL, II, NN, EE, TT, PP) needs at least 2. The sum over letters must be
<= 25. Usage: budget.py STATE,STATE,...  (or no args: report for the 127 set + greedy probes)
"""
import sys, math, itertools, os
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALL = open(os.path.join(HERE, "states.txt")).read().split()
L = {s: len(s) for s in ALL}

def need(states):
    nb, dbl = {}, set()
    for s in states:
        for a, b in zip(s, s[1:]):
            if a == b: dbl.add(a); continue
            nb.setdefault(a, set()).add(b); nb.setdefault(b, set()).add(a)
    letters = set("".join(states))
    per = {x: max(math.ceil(len(nb.get(x, ())) / 8), 2 if x in dbl else 1) for x in letters}
    return sum(per.values()), per, {x: len(nb.get(x, ())) for x in letters}

if __name__ == "__main__":
    if len(sys.argv) > 1:
        S = sys.argv[1].split(",")
    else:
        S = "ALASKA ARKANSAS COLORADO HAWAII ILLINOIS KANSAS LOUISIANA MISSOURI MONTANA NORTHCAROLINA NORTHDAKOTA SOUTHCAROLINA SOUTHDAKOTA UTAH WISCONSIN".split()
    tot, per, deg = need(S)
    print("states", len(S), "points", sum(L[s] for s in S), "cell lower bound", tot)
    print(" ".join(f"{x}:{per[x]}(d{deg[x]})" for x in sorted(per)))
    # which single additions keep the bound <= 25?
    base = set(S)
    out = []
    for s in ALL:
        if s in base: continue
        t, _, _ = need(list(base | {s}))
        out.append((t, s))
    print("bound after adding one state:", " ".join(f"{s}:{t}" for t, s in sorted(out)))
