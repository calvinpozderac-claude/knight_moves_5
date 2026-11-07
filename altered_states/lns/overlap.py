#!/usr/bin/env python3
"""Overlap graph between state names: longest shared substring (length >= 4) per pair."""
import itertools, os
S = open(os.path.join(os.path.dirname(__file__), "..", "states.txt")).read().split()
def lcs(a, b):
    best = ""
    for i in range(len(a)):
        for j in range(i + len(best) + 1, len(a) + 1):
            if a[i:j] in b: best = a[i:j]
            else: break
    return best
pairs = []
for a, b in itertools.combinations(S, 2):
    c = lcs(a, b)
    if len(c) >= 4: pairs.append((len(c), c, a, b))
for L, c, a, b in sorted(pairs, reverse=True): print(f"{L} {c:<10} {a} / {b}")
