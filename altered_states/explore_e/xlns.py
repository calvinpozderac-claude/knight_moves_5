#!/usr/bin/env python3
"""State-core exact LNS for Altered States.

Each step keeps a random subset S' of the states present in the current grid, fixes only the
squares of a minimal layout of S' (squares are blanked greedily in random order while every
state of S' stays spellable), frees every other square and lets CP-SAT optimise the free squares
exactly against all 50 states. The current grid is always a feasible fill, so the result never
scores less. Equal-scoring different grids are accepted (plateau walk, with a tabu set of visited
grids); after --patience steps without a new grid, 2-4 random squares are kicked and the walk goes on.

usage: xlns.py --seed N --time SECONDS --start GRID [--maxfree 12] [--win 8]
"""
import sys, os, time, random, argparse
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "cpsat"))
sys.path.insert(0, os.path.join(HERE, ".."))
from ortools.sat.python import cp_model
import model
from score import score, find_path

ap = argparse.ArgumentParser()
ap.add_argument("--seed", type=int, default=1)
ap.add_argument("--time", type=float, default=1800)
ap.add_argument("--start", default="CHDORWATAKIRUNSISOIASMCLL")
ap.add_argument("--maxfree", type=int, default=12)
ap.add_argument("--minfree", type=int, default=7)
ap.add_argument("--win", type=float, default=8.0, help="CP-SAT seconds per step")
ap.add_argument("--patience", type=int, default=25)
ap.add_argument("--pkeep", type=float, default=0.6, help="probability each present state is kept")
a = ap.parse_args()
rng = random.Random(a.seed)
LET = "ABCDEFGHIJKLMNOPRSTUVWXYZ"
rows = lambda g: [g[i:i + 5] for i in range(0, 25, 5)]


def minimal_layout(g, states):
    g = list(g)
    order = list(range(25)); rng.shuffle(order)
    for c in order:
        o = g[c]; g[c] = "."
        if not all(find_path(rows("".join(g)), s) for s in states):
            g[c] = o
    return "".join(g)


def exact_fill(g, layout):
    fixed = {c: layout[c] for c in range(25) if layout[c] != "."}
    m, x, present = model.build(fixed=fixed, hint=g)
    s = cp_model.CpSolver(); s.parameters.max_time_in_seconds = a.win; s.parameters.num_workers = 1
    s.parameters.random_seed = rng.randrange(10**6)
    st = s.Solve(m)
    if st not in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        return None, None, st
    return model.grid_of(s, x), int(s.BestObjectiveBound()), st


t0 = time.time()
cur = a.start
cur_sc, found = score(rows(cur))
best, best_sc = cur, cur_sc
seen = {cur}
stale = 0
step = 0
print(f"start {cur_sc} {cur}", flush=True)
while time.time() - t0 < a.time:
    step += 1
    _, found = score(rows(cur))
    present = list(found)
    # choose a kept subset whose minimal layout leaves minfree..maxfree squares free
    layout = None
    for _ in range(30):
        keep = [s for s in present if rng.random() < a.pkeep]
        if not keep:
            continue
        L = minimal_layout(cur, keep)
        free = L.count(".")
        if a.minfree <= free <= a.maxfree:
            layout = L; break
    if layout is None:
        continue
    g, bound, st = exact_fill(cur, layout)
    if g is None:
        continue
    sc = score(rows(g))[0]
    dropped = sorted(set(present) - set(keep))
    tag = "OPT" if st == cp_model.OPTIMAL else "feas"
    if sc > cur_sc or (sc == cur_sc and g not in seen):
        if sc > cur_sc:
            print(f"[{time.time()-t0:7.1f}s] step {step} {cur_sc} -> {sc} {g} (freed {layout.count('.')}, {tag})", flush=True)
        cur, cur_sc = g, sc
        seen.add(g)
        stale = 0
        if sc > best_sc:
            best, best_sc = g, sc
            print(f"[{time.time()-t0:7.1f}s] NEW BEST {best_sc} {best}", flush=True)
    else:
        stale += 1
    if stale >= a.patience:
        g = list(cur)
        for _ in range(rng.randint(2, 4)):
            g[rng.randrange(25)] = rng.choice(LET)
        cur = "".join(g); cur_sc = score(rows(cur))[0]; stale = 0
        print(f"[{time.time()-t0:7.1f}s] kick -> {cur_sc} {cur}", flush=True)
    if step % 20 == 0:
        print(f"[{time.time()-t0:7.1f}s] step {step} cur {cur_sc} best {best_sc} seen {len(seen)}", flush=True)
print(f"RESULT best {best_sc} {best} steps {step} distinct grids {len(seen)}")
