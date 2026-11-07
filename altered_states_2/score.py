#!/usr/bin/env python3
"""Independent checker for Altered States 2 boards (any n x n size).

Written separately from src/as2.cpp (plain recursive search, no bitmasks) so the two can
cross-check each other.

    python3 score.py thoainesl                      # the puzzle's 3x3 example
    python3 score.py GRID [GRID ...] [--paths]      # --paths prints one tracing per state

For every state it searches for a king's-move path (squares may be reused, each step goes to
a different adjacent square) that spells the name with at most one altered letter, and reports
the score, the states, the citations, and (with --paths) the path used for each state.
"""
import csv
import os
import sys
from functools import lru_cache

HERE = os.path.dirname(os.path.abspath(__file__))


def read_rows(name):
    with open(os.path.join(HERE, "data", name)) as f:
        rows = [r for r in csv.reader(f) if r and not r[0].startswith("#")]
    return rows[1:]


STATES = [
    dict(name=r[0], pop=int(r[1]), code=r[2], coast=r[3], spell="".join(c for c in r[0].upper() if c.isalpha()))
    for r in read_rows("states.csv")
]
BY_CODE = {s["code"]: s for s in STATES}
LAND = {}
for a, b, kind in read_rows("borders.csv"):
    if kind == "land":
        LAND.setdefault(a, set()).add(b)
        LAND.setdefault(b, set()).add(a)
TOTAL = sum(s["pop"] for s in STATES)


def trace(grid, word):
    """Return a list of squares spelling `word` with <= 1 substitution, or None."""
    n = int(round(len(grid) ** 0.5))
    assert n * n == len(grid), "grid length must be a perfect square"
    nbrs = []
    for i in range(n * n):
        r, c = divmod(i, n)
        nbrs.append([rr * n + cc for rr in range(r - 1, r + 2) for cc in range(c - 1, c + 2)
                     if 0 <= rr < n and 0 <= cc < n and (rr, cc) != (r, c)])

    @lru_cache(maxsize=None)
    def go(k, sq, alt):
        # word[k] is placed on square sq, `alt` substitutions used so far (including this one)
        if k == len(word) - 1:
            return (sq,)
        for nx in nbrs[sq]:
            a = alt + (grid[nx] != word[k + 1])
            if a <= 1:
                rest = go(k + 1, nx, a)
                if rest:
                    return (sq,) + rest
        return None

    for sq in range(n * n):
        a = int(grid[sq] != word[0])
        p = go(0, sq, a)
        if p:
            return list(p)
    return None


def c2c_chain(codes):
    """Shortest coast-to-coast chain (East coast -> West coast over land borders) inside `codes`."""
    from collections import deque
    starts = [c for c in codes if BY_CODE[c]["coast"] == "E"]
    prev = {c: None for c in starts}
    q = deque(starts)
    while q:
        u = q.popleft()
        if BY_CODE[u]["coast"] == "W":
            chain = []
            while u:
                chain.append(u)
                u = prev[u]
            return chain[::-1]
        for v in sorted(LAND.get(u, ())):
            if v in codes and v not in prev:
                prev[v] = u
                q.append(v)
    return None


def citations(found, score):
    codes = {s["code"] for s in found}
    m8 = [s["code"] for s in STATES if s["spell"].startswith("M")]
    out = []
    if len(found) >= 20:
        out.append("20S")
    if score >= 200_000_000:
        out.append("200M")
    if "PA" in codes:
        out.append("PA")
    if all(c in codes for c in m8):
        out.append("M8")
    if all(c in codes for c in ("AZ", "CO", "NM", "UT")):
        out.append("4C")
    if "CA" not in codes:
        out.append("NOCAL")
    chain = c2c_chain(codes)
    if chain:
        out.append("C2C")
    return out, chain


def score(grid, show_paths=False):
    grid = grid.strip().upper()
    n = int(round(len(grid) ** 0.5))
    found = []
    print(f"grid {grid.lower()}")
    for r in range(n):
        print("   " + " ".join(grid[r * n:(r + 1) * n]))
    for s in sorted(STATES, key=lambda s: -s["pop"]):
        p = trace(grid, s["spell"])
        if p is not None:
            found.append(s)
            if show_paths:
                spelled = "".join(grid[i] for i in p)
                cells = " ".join(f"r{i // n + 1}c{i % n + 1}" for i in p)
                print(f"   {s['name']:<15} {s['pop']:>11,}  as {spelled:<14} {cells}")
    total = sum(s["pop"] for s in found)
    cites, chain = citations(found, total)
    print(f"score {total:,} ({100 * total / TOTAL:.2f}% of {TOTAL:,}), {len(found)} states")
    print("states: " + " ".join(s["code"] for s in found))
    print("citations: " + " ".join(cites) + (f"   (C2C chain: {'-'.join(chain)})" if chain else ""))
    return total, found, cites


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    for g in args or ["thoainesl"]:
        score(g, "--paths" in sys.argv)
        print()
