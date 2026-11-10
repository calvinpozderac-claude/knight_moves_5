"""Classify every wait of a tour: pinned (the jump is legal only at exactly this wait) or free (the jump
is a dz=0 knight move inside the knight's own group, legal for any wait), and fractional or not.
usage: python3 subagents/fractional/classify.py FILE   (line 2 = tour)"""
import sys, re
from fractions import Fraction as F
sys.path.insert(0, 'tools')
from sim import MAIN_ROWS, init_alt, parse, name, apply_wait, legal_jump

def moves_of(text):
    return [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', text)]

def classify(moves):
    alt = init_alt(MAIN_ROWS); pos = parse('a1'); out = []
    for i, (t, qs) in enumerate(moves):
        q = parse(qs); a0 = alt[pos]; O = (7 - pos[0], 7 - pos[1])
        ingroup = alt[q] == a0 and q != O
        n = apply_wait(pos, t, alt, 8)
        kind = 'zero' if t == 0 else ('free' if ingroup else 'pinned')
        frac = (t / n).denominator != 1
        out.append((i + 1, t, qs, n, kind, frac))
        pos = q
    return out, alt

if __name__ == '__main__':
    txt = open(sys.argv[1]).read().splitlines()[1]
    rows, alt = classify(moves_of(txt))
    from collections import Counter
    print(Counter((k, f) for _, _, _, _, k, f in rows))
    for r in rows:
        if r[4] == 'free' or r[5]:
            print(r)
    print('final fractional points:', [name(p) for p in alt if alt[p].denominator != 1])
