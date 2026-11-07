"""Independent check of an Eldrow chain, written directly from the puzzle statement.

usage: python3 verify.py words.txt "W1,W2,...,TARGET"
"""
import sys
from collections import Counter


def mark(guess, target):
    """Wordle colouring: greens first, then yellows left to right counting duplicates."""
    out = ["gray"] * 5
    unused = Counter()
    for i in range(5):
        if guess[i] == target[i]:
            out[i] = "green"
        else:
            unused[target[i]] += 1
    for i in range(5):
        if out[i] != "green" and unused[guess[i]] > 0:
            out[i] = "yellow"
            unused[guess[i]] -= 1
    return tuple(out)


def check(words, chain):
    target = chain[-1]
    for w in chain:
        assert w in words, f"{w} not in list"
    assert target not in chain[:-1], "target guessed before the end"
    for i, g in enumerate(chain):
        # g must be a possible mystery word given every earlier guess's colouring
        for h in chain[:i]:
            assert mark(h, g) == mark(h, target), f"{g} contradicts clue from {h}"
    return True


if __name__ == "__main__":
    words = set(open(sys.argv[1]).read().split())
    assert mark("ERROR", "ROVER") == ("yellow", "yellow", "gray", "yellow", "green")
    assert mark("TRACK", "TIGER") == ("green", "yellow", "gray", "gray", "gray")
    for bad in ["DIRTY", "TARDY", "TEPID"]:
        assert mark("TRACK", bad) != mark("TRACK", "TIGER")
    assert mark("TRACK", "TOWER") == mark("TRACK", "TIGER")
    chain = [w.strip().upper() for w in sys.argv[2].split(",")]
    check(words, chain)
    print(f"valid chain of {len(chain)} words ending in {chain[-1]}")
