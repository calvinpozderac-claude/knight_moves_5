"""Exact check of an unbounded tour family under the literal necessity rule.

Tour(W) = first 53 moves of data/tours/record_9329.txt, then
    (W, b4), (50, d5), (0, f4), (0, g6), (0, g5), (0, g6), (0, h8)

For every W > 286 the tour is legal, every point other than h8 is visited at most 3 times, and every wait t > 0 has a t' that makes some move illegal.
Its length is 288 + W minutes, so the literal rules allow arbitrarily long tours.

Run:  python3 proofs/verify_unbounded.py
"""
import re
import sys
from fractions import Fraction as F

ROOT = __file__.rsplit('/', 2)[0]
sys.path.insert(0, ROOT + '/tools')
from sim import MAIN_ROWS, init_alt, parse, apply_wait, simulate  # noqa: E402

PREFIX_LEN = 53
SUFFIX = [('W', 'b4'), (50, 'd5'), (0, 'f4'), (0, 'g6'), (0, 'g5'), (0, 'g6'), (0, 'h8')]


def record_prefix():
    txt = open(ROOT + '/data/tours/record_9329.txt').read().splitlines()[1]
    mv = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', txt)]
    return mv[:PREFIX_LEN]


def tour(W):
    return record_prefix() + [(F(W) if t == 'W' else F(t), q) for t, q in SUFFIX]


def state_before(moves, i):
    """Altitudes and knight square just before move i's wait."""
    alt = init_alt(MAIN_ROWS)
    pos = parse('a1')
    for t, q in moves[:i]:
        apply_wait(pos, t, alt, 8)
        pos = parse(q)
    return alt, pos


def breaking_tprime(moves, i):
    """Return some t' != t that makes the tour illegal, or None.

    Candidates: small perturbations, 0, and every 'merge' value t' = n*(alt(P)-alt(y)) that brings
    the knight's group exactly level with another point y below it."""
    t, q = moves[i]
    alt, pos = state_before(moves, i)
    A = alt[pos]
    lvl = [p for p in alt if alt[p] == A]
    o = (7 - pos[0], 7 - pos[1])
    n = len(lvl) - (1 if alt[o] == A else 0)
    cands = [t + F(1, 997), t - F(1, 997), F(0), t + 1, 2 * t]
    cands += sorted({n * (A - alt[y]) for y in alt if alt[y] < A})
    for tp in cands:
        if tp < 0 or tp == t:
            continue
        m2 = list(moves)
        m2[i] = (tp, q)
        ok, _, msg = simulate(MAIN_ROWS, m2)
        if not ok:
            return tp, msg
    return None


def main():
    pre = record_prefix()
    print(f'prefix: {PREFIX_LEN} moves, {sum(t for t, _ in pre)} minutes')
    alt, pos = state_before(tour(10**6), PREFIX_LEN)
    A = alt[pos]
    G = sorted(p for p in alt if alt[p] == A)
    print(f'knight on {chr(97+pos[0])}{pos[1]+1}, altitude {A}; level set ({len(G)} points) contains h8: '
          f'{(7, 7) in G}; points below: {sum(1 for p in alt if alt[p] < A)}')

    for W in [287, 1250, 10**4, 10**6, 10**12, 10**30, 7 * 10**40 + 3]:
        mv = tour(W)
        ok, T, msg = simulate(MAIN_ROWS, mv)
        vis = {}
        for _, q in mv:
            vis[q] = vis.get(q, 0) + 1
        vis['a1'] = vis.get('a1', 0) + 1
        maxv = max(v for k, v in vis.items() if k != 'h8')
        line = f'W={W}: legal={ok} ({msg}) T={T} max visits={maxv}'
        if W in (287, 10**6, 10**30):
            missing = []
            for i, (t, q) in enumerate(mv):
                if t > 0 and breaking_tprime(mv, i) is None:
                    missing.append(i + 1)
            line += f'  waits with no breaking t\': {missing or "none"}'
            if W == 10**6:
                tp, why = breaking_tprime(mv, PREFIX_LEN)
                line += f"\n    the W wait (move {PREFIX_LEN+1}) breaks at t'={tp}: {why}"
        print(line)


if __name__ == '__main__':
    main()
