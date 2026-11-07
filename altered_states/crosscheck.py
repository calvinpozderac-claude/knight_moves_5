#!/usr/bin/env python3
"""Cross-check build/as against score.py on random mutations of a good grid."""
import random, subprocess, sys
from score import score
base = sys.argv[1] if len(sys.argv) > 1 else "IWDNIOHAISDCTULNAROASOKSC"
letters = "ABCDEFGHIJKLMNOPRSTUVWXYZ"
rng = random.Random(0)
bad = 0
N = int(sys.argv[2]) if len(sys.argv) > 2 else 400
for t in range(N):
    g = list(base)
    for _ in range(rng.randint(0, 6)):
        g[rng.randrange(25)] = rng.choice(letters + "SSIINNAAOO")   # extra doubles -> SS, II, NN paths
    g = "".join(g)
    out = subprocess.run(["./build/as", "--mode", "score", "--init", g], capture_output=True, text=True).stdout
    c = int(out.split()[1])
    p, _ = score([g[i:i+5] for i in range(0, 25, 5)])
    if c != p:
        bad += 1; print("MISMATCH", g, c, p)
print(f"{N} grids checked, {bad} mismatches")
