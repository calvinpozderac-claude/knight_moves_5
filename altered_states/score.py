#!/usr/bin/env python3
"""Exact, independent scorer for Jane Street's "Altered States" puzzle.

A state is present if its letters can be spelled by a walk of King's moves on the
5x5 grid. Consecutive letters must be on distinct, adjacent squares (staying put is
not a move); a square may be revisited later in the walk (the example grid's OHIO
uses its single O twice). A state scores its length, once.

Usage:
    python3 score.py ROW1 ROW2 ROW3 ROW4 ROW5      # e.g. python3 score.py ".IH.." ...
    python3 score.py --grid 25LETTERS
    python3 score.py --example                     # the 3x3 example from the puzzle (13)
Blank squares can be written as '.' or '_'.
Prints the score, the rows and every state found with one witness path.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
STATES = open(os.path.join(HERE, "states.txt")).read().split()


def neighbours(r, c, n_rows, n_cols):
    for dr in (-1, 0, 1):
        for dc in (-1, 0, 1):
            if (dr or dc) and 0 <= r + dr < n_rows and 0 <= c + dc < n_cols:
                yield r + dr, c + dc


def find_path(rows, word):
    """Return a list of (row, col) spelling `word` by King's moves, or None."""
    n_rows, n_cols = len(rows), len(rows[0])
    # layer[i] maps a cell where letter i can sit -> the cell it came from
    layer = [{(r, c): None for r in range(n_rows) for c in range(n_cols)
              if rows[r][c] == word[0]}]
    for ch in word[1:]:
        nxt = {}
        for (r, c) in layer[-1]:
            for (nr, nc) in neighbours(r, c, n_rows, n_cols):
                if rows[nr][nc] == ch and (nr, nc) not in nxt:
                    nxt[(nr, nc)] = (r, c)
        if not nxt:
            return None
        layer.append(nxt)
    cell = next(iter(layer[-1]))
    path = [cell]
    for i in range(len(word) - 1, 0, -1):
        cell = layer[i][cell]
        path.append(cell)
    return path[::-1]


def score(rows, states=STATES):
    found = {}
    for s in states:
        p = find_path(rows, s)
        if p is not None:
            found[s] = p
    return sum(len(s) for s in found), found


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return
    if argv[0] == "--example":
        rows = [".IH", "DAO", ".W."]
    elif argv[0] == "--grid":
        g = argv[1].upper()
        assert len(g) == 25, "need 25 letters"
        rows = [g[i:i + 5] for i in range(0, 25, 5)]
    else:
        rows = [a.upper() for a in argv]
    rows = [r.replace("_", ".") for r in rows]
    assert len({len(r) for r in rows}) == 1, "rows must have equal length"
    total, found = score(rows)
    print("score", total)
    print("rows ", " / ".join(rows))
    for r in rows:
        print("   ", " ".join(r))
    print(f"{len(found)} states:")
    for s in sorted(found, key=lambda s: (-len(s), s)):
        path = "-".join(f"{'abcde'[c]}{len(rows) - r}" for r, c in found[s])
        print(f"  {s:<14}{len(s):>3}  {path}")


if __name__ == "__main__":
    main(sys.argv[1:])
