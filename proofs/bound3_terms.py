"""Check every step of Theorem 3 (strict tours last at most 16,094 1/3 minutes) on real tours.

Exact arithmetic, following proofs/UPPER_BOUND.md and lean/KnightMoves5/StrictBound3.lean.
Floor m = running minimum, B = its total drop. A *knock-out* is a floor-lowering sit with sink d > 2;
it is *covered* if its opposite point sinks again later, *open* otherwise. W = drop of open ones.
For each strict tour it checks
  depth      T <= 421 + R_b + 64 B - L      (L = sum over points of the floor drop since they last sank,
                                             R_b = rises that happened before the point's last sink)
  unpaid     R_b <= 2M + B - W
  loss       L >= (W^2 + 2W) / 12           (at most 3 knock-outs per sitter, tail condition)
  rescue     B + B_nK + B_cov <= 2M + 2     (B_nK: floor-lowering sits with d <= 2)
  windows    every covered knock-out's opposite sinks again at a sit where the knight's height is at
             least d + (floor drop since), and that sit follows a move that is not floor-lowering
and prints the tour's own bound 421 + 2M + 65B - 7W/6 - W^2/12 next to its length.

Run:  python3 proofs/bound3_terms.py data/tours/record_9329.txt data/tours/curriculum.txt
"""
import re
import sys
from fractions import Fraction as F

ROOT = __file__.rsplit('/', 2)[0]
sys.path.insert(0, ROOT + '/tools')
from sim import MAIN_ROWS, init_alt, parse  # noqa: E402


def load_all(path):
    out = []
    for line in open(path):
        mv = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', line)]
        if mv:
            out.append(mv)
    return out


def analyse(moves):
    alt = init_alt(MAIN_ROWS)
    pos = parse('a1')
    m = F(0)
    recs, sinks, T = [], {p: [] for p in alt}, F(0)
    for i, (t, q) in enumerate(moves):
        Q = parse(q)
        k = alt[pos]
        O = (7 - pos[0], 7 - pos[1])
        G = [p for p in alt if alt[p] == k and p != O]
        d = t / len(G) if t > 0 else F(0)
        rise = t > 0 and alt[O] != k
        strict = not (t > 0 and Q in G)
        if t > 0:
            for p in G:
                alt[p] -= d
                sinks[p].append(i)
            if rise:
                alt[O] += d
        nm = min(m, min(alt.values()))
        recs.append(dict(i=i, d=d, h=k - m, delta=m - nm, P=pos, O=O, rise=rise, strict=strict))
        m = nm
        T += t
        pos = Q
    return recs, sinks, T, m


def check(moves):
    recs, sinks, T, m_end = analyse(moves)
    if not all(r['strict'] for r in recs):
        return None
    M, B = len(recs), -m_end
    fl = lambda i: recs[i]['delta'] > 0  # noqa: E731
    nxt = lambda p, i: next((j for j in sinks[p] if j > i), None)  # noqa: E731
    K = [r for r in recs if r['delta'] > 0 and r['d'] > 2]
    BnK = sum((r['delta'] for r in recs if r['delta'] > 0 and r['d'] <= 2), F(0))
    cov = [(r, nxt(r['O'], r['i'])) for r in K if nxt(r['O'], r['i']) is not None]
    opn = [r for r in K if nxt(r['O'], r['i']) is None]
    Bcov = sum((r['delta'] for r, _ in cov), F(0))
    W = sum((r['delta'] for r in opn), F(0))
    tail = lambda i: sum((s['delta'] for s in recs[i:]), F(0))  # noqa: E731
    L = sum((tail(lst[-1] + 1) if lst else B for lst in sinks.values()), F(0))
    Rb = sum((r['d'] for r in recs if r['rise'] and sinks[r['O']] and sinks[r['O']][-1] > r['i']), F(0))
    windows = all(not fl(j - 1) and recs[j]['h'] >= r['d'] + sum((recs[l]['delta'] for l in range(r['i'] + 1, j)), F(0))
                  for r, j in cov)
    by = {}
    for r in opn:
        by.setdefault(r['P'], []).append(r)
    ok = dict(depth=T <= 421 + Rb + 64 * B - L, unpaid=Rb <= 2 * M + B - W,
              loss=L >= (W * W + 2 * W) / 12, rescue=B + BnK + Bcov <= 2 * M + 2,
              windows=windows, visits=all(len(g) <= 3 for g in by.values()))
    bound = 421 + 2 * M + 65 * B - F(7, 6) * W - W * W / 12
    return dict(T=T, M=M, B=B, W=W, Bcov=Bcov, BnK=BnK, L=L, ok=ok, bound=bound)


def main(paths):
    for p in paths:
        tours = load_all(p)
        n = bad = 0
        for mv in tours:
            r = check(mv)
            if r is None:
                continue
            n += 1
            if not all(r['ok'].values()):
                bad += 1
                print('VIOLATION', p, r['ok'])
            if len(tours) == 1:
                print(f"{p.split('/')[-1]}: T={int(r['T'])} M={r['M']} B={float(r['B']):.0f} "
                      f"open knock-outs W={float(r['W']):.0f} covered={float(r['Bcov']):.0f} "
                      f"B_nK={float(r['BnK']):.0f} L={float(r['L']):.0f}  own bound {float(r['bound']):.0f}")
        print(f"{p.split('/')[-1]}: {n} strict tours, {bad} violations")


if __name__ == '__main__':
    main(sys.argv[1:])
