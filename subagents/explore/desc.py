#!/usr/bin/env python3
# Family descriptor of strict Knight Moves 5 tours.
#   desc(tour) -> dict: T, strict, ds (dance start: first state with >= 61 points on the knight's level + the denser
#   neighbouring level +-2; else the state with the most), lv (the two dance levels, absolute, sorted), sit (frozenset of
#   squares with a positive wait at or after ds), op (first 15 squares), first (first move square)
import sys, re
sys.path.insert(0, '/home/user/knight_moves_5/tools')
from sim import init_alt, MAIN_ROWS, parse, apply_wait, legal_jump, name
from fractions import Fraction as F
def moves(t): return [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', t)]
def desc(t):
    mv = moves(t); alt = init_alt(MAIN_ROWS); pos = parse('a1'); strict = True; best = (-1, 0, None); T = F(0); waits = []
    ds = None
    for i, (w, q) in enumerate(mv):
        a0 = alt[pos]; cnt = {}
        for v in alt.values(): cnt[v] = cnt.get(v, 0) + 1
        nm, np_ = cnt.get(a0 - 2, 0), cnt.get(a0 + 2, 0); n = cnt.get(a0, 0) + max(nm, np_)
        other = a0 - 2 if nm >= np_ else a0 + 2
        if n > best[0]: best = (n, i, (a0, other))
        if ds is None and n >= 61 and i >= 40: ds = (i, (a0, other))
        Q = parse(q)
        if w.denominator != 1: strict = False
        if w > 0:
            a2 = dict(alt); apply_wait(pos, w + F(1, 997), a2, 8)
            if legal_jump(pos, Q, a2): strict = False
        waits.append((name(pos), w)); apply_wait(pos, w, alt, 8); pos = Q; T += w
    if ds is None: ds = (best[1], best[2])
    k, lv = ds
    sit = frozenset(p for p, w in waits[k:] if w > 0)
    return dict(T=T, strict=strict, ds=k, lv=tuple(sorted(int(x) for x in lv)) if lv else None, sit=sit,
                op=tuple(q for _, q in mv[:15]), first=mv[0][1] if mv else None, n=len(mv))
def jac(a, b): return len(a & b) / max(1, len(a | b))
if __name__ == '__main__':
    for fn in sys.argv[1:]:
        t = [l for l in open(fn) if '(' in l][0]; d = desc(t)
        print(fn, int(d['T']), 'STRICT' if d['strict'] else 'free', 'ds', d['ds'], 'lv', d['lv'], 'nsit', len(d['sit']), 'first', d['first'], ' '.join(d['op'][:8]))
