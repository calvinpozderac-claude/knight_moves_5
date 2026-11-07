"""Writes the feedback matrix of a word list using verify.py's mark(), for exhaust.cpp.

usage: python3 fbmatrix.py <words.txt> <out.bin>
Byte (g * N + t) is the colouring of guess g against target t, words in sorted order,
coded gray=0, yellow=1, green=2 with position 0 the least significant base-3 digit.
"""
import os
import sys
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from verify import mark  # noqa: E402

V = {"gray": 0, "yellow": 1, "green": 2}
words = sorted(open(sys.argv[1]).read().split())


def row(g):
    return bytes(sum(V[c] * 3 ** i for i, c in enumerate(mark(g, t))) for t in words)


if __name__ == "__main__":
    with Pool() as p:
        rows = p.map(row, words, chunksize=50)
    with open(sys.argv[2], "wb") as f:
        f.write(b"".join(rows))
    print(len(words), "words")
