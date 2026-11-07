#!/usr/bin/env python3
"""Generate many distinct minimal layouts of the NORTH/SOUTH x CAROLINA/DAKOTA core.
Each SA run (build/as on just the 4 core states) gives a grid holding all four; squares are
then blanked greedily in random order while all four stay spellable, which leaves a minimal
layout. Layouts are deduplicated up to the 8 rotations/reflections."""
import subprocess, random, sys, os
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, "..")
sys.path.insert(0, ROOT)
from score import find_path
CORE = ["NORTHCAROLINA", "SOUTHCAROLINA", "NORTHDAKOTA", "SOUTHDAKOTA"]
N = int(sys.argv[1]) if len(sys.argv) > 1 else 300
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "results/explore_d/cd_layouts.txt")
sf = os.path.join(ROOT, "results/explore_d/cd_states.txt")
open(sf, "w").write("\n".join(CORE) + "\n")
def ok(g): return all(find_path([g[i:i+5] for i in range(0, 25, 5)], s) for s in CORE)
def sym(g):
    res = []
    G = [list(g[i:i+5]) for i in range(0, 25, 5)]
    for t in range(8):
        H = G
        for _ in range(t % 4): H = [list(r) for r in zip(*H[::-1])]
        if t >= 4: H = [r[::-1] for r in H]
        res.append("".join("".join(r) for r in H))
    return min(res)
seen = set(); rng = random.Random(1)
for k in range(N):
    r = subprocess.run([os.path.join(ROOT, "build/as"), "--mode", "sa", "--states", sf, "--time", "0.3",
                        "--threads", "1", "--iters", "400000", "--seed", str(1000 + k), "--T0", "2", "--T1", "0.2"],
                       capture_output=True, text=True).stdout
    g = r.split("grid=")[1].split()[0]
    if not ok(g): continue
    g = list(g); order = list(range(25)); rng.shuffle(order)
    for c in order:
        old = g[c]; g[c] = "."
        if not ok("".join(g)): g[c] = old
    seen.add(sym("".join(g)))
lay = sorted(seen, key=lambda s: (-s.count("."), s))
open(out, "w").write("\n".join(lay) + "\n")
from collections import Counter
print(f"{N} runs -> {len(lay)} distinct minimal layouts; cells used:", sorted(Counter(25 - s.count('.') for s in lay).items()))
