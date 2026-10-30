"""Fast integer-altitude core for the 8x8 swamp knight puzzle.
Index i = x + 8*y (x: file a..h = 0..7, y: rank 1..8 = 0..7). Opposite = 63 - i.
"""
from sim import MAIN_ROWS

H8 = 63
A1 = 0


def nm(i):
    return chr(97 + i % 8) + str(i // 8 + 1)


def ix(s):
    return (ord(s[0]) - 97) + 8 * (int(s[1:]) - 1)


ALT0 = [0] * 64
for r, row in enumerate(MAIN_ROWS):
    y = 7 - r
    for x, v in enumerate(row):
        ALT0[x + 8 * y] = v

# neighbours: list of (j, dzset)
NEIGH = [[] for _ in range(64)]
for i in range(64):
    x, y = i % 8, i // 8
    for dx in range(-2, 3):
        for dy in range(-2, 3):
            a = sorted([abs(dx), abs(dy)])
            if a == [1, 2]:
                dz = (0,)
            elif a == [0, 1]:
                dz = (2, -2)
            elif a == [0, 2]:
                dz = (1, -1)
            else:
                continue
            X, Y = x + dx, y + dy
            if 0 <= X < 8 and 0 <= Y < 8:
                NEIGH[i].append((X + 8 * Y, dz))


def group_of(p, alt):
    A = alt[p]
    O = 63 - p
    g = [i for i in range(64) if alt[i] == A and i != O]
    rise = alt[O] != A
    return g, rise


def apply(p, delta, alt):
    """Sink knight's group at p by delta (int >= 0). Returns (newalt, n, t)."""
    g, rise = group_of(p, alt)
    n = len(g)
    if delta == 0:
        return alt, n, 0
    a = list(alt)
    for i in g:
        a[i] -= delta
    if rise:
        a[63 - p] += delta
    return a, n, n * delta


def legal(p, q, alt):
    for j, dz in NEIGH[p]:
        if j == q:
            return (alt[q] - alt[p]) in dz
    return False


def moves_pinned(p, alt, visits, allow_h8=True, maxdelta=None):
    """Immediate-pinned moves: (delta, q). delta>0 only if jump to static q needs it."""
    A = alt[p]
    O = 63 - p
    res = []
    for q, dz in NEIGH[p]:
        if q == H8:
            if not allow_h8:
                continue
        elif visits[q] >= 3:
            continue
        if alt[q] == A and q != O:
            if 0 in dz:
                res.append((0, q))
            continue
        for d in dz:
            delta = d - (alt[q] - A)
            if delta >= 0 and (maxdelta is None or delta <= maxdelta):
                res.append((delta, q))
    return res


def replay(moves, start=A1):
    """moves: list of (t, qname) with integer t. Uses exact sim for verification."""
    from sim import simulate
    return simulate(MAIN_ROWS, [(t, q) for t, q in moves])
