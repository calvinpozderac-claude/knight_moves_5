#!/usr/bin/env python3
"""CNF encoding of Altered States 2 (5x5) for pysat.

Variables x[c][L] (exactly one letter per square). For every state, one-directional
reachability literals (see model.py):
  r0[i][c] -> x[c][w_i];  r0[i][c] -> OR_{N(c)} r0[i-1]
  r1[i][c] -> OR_{N(c)} r0[i-1]  OR z[i][c];  z -> x[c][w_i];  z -> OR_{N(c)} r1[i-1]
  y[s] -> OR_c r1[L-1][c]
Optionally (backward=True) the symmetric "suffix" encoding is not added; the forward
one is complete and sound.
"""
from pysat.formula import CNF, IDPool
from pysat.card import CardEnc, EncType

from model import STATES, BY_CODE, LETTERS, NB, CELLS


class Enc:
    def __init__(self, letters=LETTERS):
        self.pool = IDPool()
        self.cls = []
        self.letters = letters
        self.x = {c: {L: self.pool.id(("x", c, L)) for L in letters} for c in CELLS}
        for c in CELLS:
            lits = list(self.x[c].values())
            self.cls.append(lits)
            enc = CardEnc.atmost(lits, 1, vpool=self.pool, encoding=EncType.seqcounter)
            self.cls.extend(enc.clauses)
        self.y = {}

    def new(self):
        return self.pool.id()

    def add_state(self, code):
        w = BY_CODE[code]["spell"]
        Lw = len(w)
        TRUE = "T"
        r0 = [[None] * 25 for _ in range(Lw)]
        r1 = [[None] * 25 for _ in range(Lw)]
        cl = self.cls
        for i in range(Lw):
            for c in CELLS:
                xl = self.x[c].get(w[i])
                if xl is not None:
                    prev = [r0[i - 1][d] for d in NB[c] if r0[i - 1][d] is not None] if i else None
                    if i == 0 or prev:
                        v = self.new()
                        cl.append([-v, xl])
                        if i:
                            cl.append([-v] + prev)
                        r0[i][c] = v
                if i == 0:
                    r1[i][c] = TRUE
                    continue
                prev0 = [r0[i - 1][d] for d in NB[c] if r0[i - 1][d] is not None]
                prev1 = [r1[i - 1][d] for d in NB[c] if r1[i - 1][d] is not None]
                opts = list(prev0)
                if xl is not None and prev1:
                    if TRUE in prev1:
                        opts.append(xl)
                    else:
                        z = self.new()
                        cl.append([-z, xl])
                        cl.append([-z] + prev1)
                        opts.append(z)
                if not opts:
                    continue
                v = self.new()
                cl.append([-v] + opts)
                r1[i][c] = v
        last = [r1[Lw - 1][c] for c in CELLS if r1[Lw - 1][c] is not None]
        ys = self.pool.id(("y", code))
        cl.append([-ys] + last)
        self.y[code] = ys
        return ys

    def fix(self, grid, cells):
        for c in cells:
            cl = self.x[c].get(grid[c].upper())
            if cl is None:
                raise ValueError("letter not in alphabet")
            self.cls.append([cl])

    def grid(self, model):
        ms = set(l for l in model if l > 0)
        out = []
        for c in CELLS:
            ch = "?"
            for L, v in self.x[c].items():
                if v in ms:
                    ch = L.lower()
            out.append(ch)
        return "".join(out)
