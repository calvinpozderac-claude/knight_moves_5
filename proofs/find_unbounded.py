"""Search tour prefixes for a place where an arbitrarily long wait can be inserted (literal rules).

Construction looked for, starting with the knight on a point P at the same altitude as h8:
  (W, Q1)    W arbitrary. The knight's level set G (h8 is in it) sinks W/n. Q1 is a knight move
             inside G, so this jump is legal whatever W is.
  zero-wait knight moves inside G to a point P2 whose opposite P2' is also in G
  (t2, ...)  sit on P2 with t2 = d2 * (|G| - 1), d2 in {1, 2}: G minus P2' sinks d2 and P2' stays
             (an opposite at the same altitude is ignored)
  zero-wait knight moves inside G minus P2', then a jump up d2 onto P2'
  down d2 again (or straight onto h8), then knight moves to h8.
The t2 wait is pinned by the jump onto P2'. The W wait is necessary in the literal sense when some
point y lies below G: with t' = n * (alt(P) - alt(y)) the group lands on y's level, the P2 sit then
has a bigger group and sinks a different amount, and the jump onto P2' becomes illegal.

Run:  python3 proofs/find_unbounded.py data/tours/record_9329.txt [more tour files]
"""
import re
import sys
from fractions import Fraction as F

ROOT = __file__.rsplit('/', 2)[0]
sys.path.insert(0, ROOT + '/tools')
from sim import MAIN_ROWS, init_alt, parse, name, apply_wait  # noqa: E402

H8 = (7, 7)
KNIGHT = [(1, 2), (2, 1), (-1, 2), (-2, 1), (1, -2), (2, -1), (-1, -2), (-2, -1)]
STEP = {2: [(1, 0), (-1, 0), (0, 1), (0, -1)],   # altitude change 2: one orthogonal step
        1: [(2, 0), (-2, 0), (0, 2), (0, -2)]}   # altitude change 1: two steps in a line


def opp(p):
    return (7 - p[0], 7 - p[1])


def nbrs(p, deltas):
    for dx, dy in deltas:
        q = (p[0] + dx, p[1] + dy)
        if 0 <= q[0] < 8 and 0 <= q[1] < 8:
            yield q


def find_suffix(alt, pos, visits, limit):
    """Return the suffix (list of (wait, square)) of at most `limit` moves, or None."""
    ell = alt[pos]
    if alt[H8] != ell:
        return None
    G = {p for p in alt if alt[p] == ell} - {opp(pos)}
    if not any(alt[y] < ell and y != opp(pos) for y in alt):
        return None
    vis = dict(visits)

    def ok(q):
        return q != H8 and vis.get(q, 0) < 3

    def step(q):
        vis[q] = vis.get(q, 0) + 1

    def unstep(q):
        vis[q] -= 1

    def walk_to_h8(cur, path, G2):            # knight moves inside G2, last one onto h8
        if len(path) > limit:
            return None
        if H8 in nbrs(cur, KNIGHT):
            return path + [(0, H8)]
        for q in nbrs(cur, KNIGHT):
            if q in G2 and ok(q):
                step(q)
                r = walk_to_h8(q, path + [(0, q)], G2)
                unstep(q)
                if r:
                    return r
        return None

    def after_top(top, path, G2, d2):         # on P2' (d2 above G2): step down, then to h8
        if H8 in nbrs(top, STEP[d2]):
            return path + [(0, H8)]
        for q in nbrs(top, STEP[d2]):
            if q in G2 and ok(q):
                step(q)
                r = walk_to_h8(q, path + [(0, q)], G2)
                unstep(q)
                if r:
                    return r
        return None

    def to_top(cur, path, G2, top, d2, wait):  # knight moves inside G2, then jump up onto top
        if len(path) > limit:
            return None
        if wait == 0 and top in nbrs(cur, STEP[d2]):
            if top == H8:
                return path + [(0, H8)]
            if ok(top):
                step(top)
                r = after_top(top, path + [(0, top)], G2, d2)
                unstep(top)
                if r:
                    return r
        for q in nbrs(cur, KNIGHT):
            if q in G2 and ok(q):
                step(q)
                r = to_top(q, path + [(wait, q)], G2, top, d2, 0)
                unstep(q)
                if r:
                    return r
        return None

    def to_p2(cur, path):                      # knight moves inside G (first carries W), sit on P2
        if len(path) > limit:
            return None
        if path and opp(cur) in G:
            G2 = G - {opp(cur)}
            for d2 in (2, 1):
                r = to_top(cur, path, G2, opp(cur), d2, d2 * len(G2))
                if r:
                    return r
        for q in nbrs(cur, KNIGHT):
            if q in G and ok(q):
                step(q)
                r = to_p2(q, path + [('W' if not path else 0, q)])
                unstep(q)
                if r:
                    return r
        return None

    return to_p2(pos, [])


def scan(moves, limit=12):
    alt = init_alt(MAIN_ROWS)
    pos = parse('a1')
    visits = {pos: 1}
    T = F(0)
    for k, (t, q) in enumerate(moves):
        for lim in range(3, limit + 1):        # shortest suffix first
            r = find_suffix(alt, pos, visits, lim)
            if r:
                return k, T, name(pos), r
        Q = parse(q)
        apply_wait(pos, t, alt, 8)
        T += t
        visits[Q] = visits.get(Q, 0) + 1
        pos = Q
    return None


if __name__ == '__main__':
    for path in sys.argv[1:]:
        for line in open(path):
            mv = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', line)]
            if len(mv) < 4:
                continue
            hit = scan(mv)
            if hit:
                k, T, at, suffix = hit
                print(f'{path}: after move {k} ({T} min, knight on {at}): '
                      + ', '.join(f'({t}, {name(q)})' for t, q in suffix))
