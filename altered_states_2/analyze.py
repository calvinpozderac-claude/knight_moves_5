#!/usr/bin/env python3
"""Pool every restart in results/logs/*.csv and list the best distinct boards.

Boards that are rotations or reflections of each other score the same (king moves are
symmetric under the 8 symmetries of the square), so they are merged into one canonical form.

    python3 analyze.py [--top N] [--logs LABEL ...] [--require CITATION ...]
"""
import argparse
import collections
import glob
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def symmetries(g):
    rows = [g[i * 5:(i + 1) * 5] for i in range(5)]
    out = []
    for _ in range(4):
        rows = ["".join(rows[4 - c][r] for c in range(5)) for r in range(5)]  # rotate 90
        out.append("".join(rows))
        out.append("".join(r[::-1] for r in rows))  # mirror
    return out


def canon(g):
    return min(symmetries(g))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--logs", nargs="*")
    ap.add_argument("--require", nargs="*", default=[])
    ap.add_argument("--by-score", action="store_true",
                    help="merge boards with equal scores (variants differing only in letters no state uses)")
    a = ap.parse_args()
    files = sorted(glob.glob(os.path.join(HERE, "results", "logs", "*.csv")))
    if a.logs:
        files = [f for f in files if os.path.basename(f)[:-4] in a.logs]
    seen = {}
    hits = collections.Counter()
    where = collections.defaultdict(set)
    total = 0
    for f in files:
        for line in open(f):
            p = line.rstrip("\n").split(",")
            cites = p[4].split()
            if any(r not in cites for r in a.require):
                continue
            total += 1
            c = p[1] if a.by_score else canon(p[0])
            if c not in seen or not a.by_score:
                seen[c] = (int(p[1]), int(p[2]), p[4], canon(p[0]))
            hits[c] += 1
            where[c].add(os.path.basename(f)[:-4])
    print(f"{total} restarts, {len(seen)} distinct {'scores' if a.by_score else 'boards (up to symmetry)'}")
    for c, (s, n, cites, g) in sorted(seen.items(), key=lambda kv: -kv[1][0])[:a.top]:
        print(f"{s:>12,}  {n:>2} states  found {hits[c]:>3}x  {g}  [{cites}]  in {len(where[c])} runs")


if __name__ == "__main__":
    main()
