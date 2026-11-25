"""Check every step of the strict-tour upper bound (proofs/UPPER_BOUND.md) on real tours.

Exact arithmetic. For each tour it checks:
  Lemma 1  every sit whose jump target is outside the knight's group sinks d <= k - m + 2
           (k = knight's altitude, m = lowest altitude on the board, both just before the sit)
  Lemma 2  the floor m never drops below -B_fl, where B_fl = sum of min(d, 2) over the sits that
           lower the floor
  Lemma 3  at least 8 moves are not floor-lowering sits (S_fl <= M - 8)
  Identity T = 421 - sum(final altitudes) + R   (R = total rise of opposite points)
and prints the bound for that tour, 421 + 65*B_fl + 2*M, next to its length.
A tour is "strict" when every wait's own jump goes to a point outside the sinking group.

Run:  python3 proofs/bound_terms.py data/tours/record_9329.txt [more tour files]
"""
import re
import sys
from fractions import Fraction as F

ROOT = __file__.rsplit('/', 2)[0]
sys.path.insert(0, ROOT + '/tools')
from sim import MAIN_ROWS, init_alt, parse  # noqa: E402


def run(moves):
    alt = init_alt(MAIN_ROWS)
    pos = parse('a1')
    T = R = B_fl = F(0)
    S = S_fl = 0
    lemma1 = floor_ok = strict = True
    for t, q in moves:
        t = F(t)
        Q = parse(q)
        if t > 0:
            k = alt[pos]
            m = min(alt.values())
            O = (7 - pos[0], 7 - pos[1])
            G = [p for p in alt if alt[p] == k and p != O]
            rise = alt[O] != k
            d = t / len(G)
            if Q in G:
                strict = False
            elif d > k - m + 2:
                lemma1 = False
            for p in G:
                alt[p] -= d
            if rise:
                alt[O] += d
                R += d
            new_m = min(alt.values())
            if new_m < m:
                S_fl += 1
                B_fl += min(d, F(2))
                if m - new_m > min(d, F(2)):
                    floor_ok = False
            S += 1
            T += t
        pos = Q
        if min(alt.values()) < -B_fl:
            floor_ok = False
    M = len(moves)
    return dict(M=M, S=S, S_fl=S_fl, B_fl=B_fl, R=R, T=T, strict=strict, lemma1=lemma1,
                floor=floor_ok, lemma3=S_fl <= M - 8,
                identity=(T == 421 - sum(alt.values()) + R),
                m_end=min(alt.values()), bound=421 + 65 * B_fl + 2 * M)


def load(path):
    lines = open(path).read().splitlines()
    txt = lines[1] if len(lines) > 1 and lines[0].strip().isdigit() else lines[0]
    return [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', txt)]


def main(paths):
    print(f"{'tour':22s} {'T':>6s} {'M':>4s} {'S':>4s} {'S_fl':>4s} {'B_fl':>6s} {'R':>6s} {'floor':>6s}"
          f"  strict lemma1 lemma2 lemma3 identity  bound")
    for p in paths:
        r = run(load(p))
        print(f"{p.split('/')[-1]:22s} {int(r['T']):6d} {r['M']:4d} {r['S']:4d} {r['S_fl']:4d} "
              f"{float(r['B_fl']):6.1f} {float(r['R']):6.1f} {float(r['m_end']):6.1f}  "
              f"{str(r['strict']):6s} {str(r['lemma1']):6s} {str(r['floor']):6s} {str(r['lemma3']):6s} "
              f"{str(r['identity']):8s} {float(r['bound']):6.0f}")


if __name__ == '__main__':
    main(sys.argv[1:])
