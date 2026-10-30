from fractions import Fraction as F

MAIN_ROWS = [  # rank 8 down to rank 1
    [9, 8, 10, 12, 11, 8, 10, 17],
    [7, 9, 11, 9, 10, 12, 14, 12],
    [4, 7, 5, 8, 8, 6, 13, 10],
    [4, 10, 7, 9, 6, 8, 7, 9],
    [2, 6, 4, 2, 5, 9, 8, 11],
    [0, 3, 1, 4, 2, 7, 10, 7],
    [1, 2, 0, 1, 2, 5, 7, 6],
    [0, 2, 4, 3, 5, 6, 2, 4],
]
EX_ROWS = [
    [11, 10, 11, 14],
    [8, 6, 9, 9],
    [10, 4, 3, 1],
    [7, 6, 5, 0],
]


def parse(s):
    return (ord(s[0]) - 97, int(s[1:]) - 1)


def name(p):
    return chr(97 + p[0]) + str(p[1] + 1)


def init_alt(rows):
    N = len(rows)
    alt = {}
    for i, row in enumerate(rows):
        for x, v in enumerate(row):
            alt[(x, N - 1 - i)] = F(v)
    return alt


def legal_jump(p, q, alt):
    dx, dy, dz = q[0] - p[0], q[1] - p[1], alt[q] - alt[p]
    return sorted([abs(dx), abs(dy), abs(dz)]) == [0, 1, 2]


def apply_wait(pos, t, alt, N):
    """Apply a wait of t minutes at pos. Returns n."""
    A = alt[pos]
    O = (N - 1 - pos[0], N - 1 - pos[1])
    S = [p for p in alt if alt[p] == A]
    rise = True
    if alt[O] == A:
        S.remove(O)
        rise = False
    n = len(S)
    if t > 0:
        d = F(t) / n
        for p in S:
            alt[p] -= d
        if rise:
            alt[O] += d
    return n


def simulate(rows, moves, start='a1', target=None, verbose=False):
    """moves: list of (t, 'sq'). Returns (ok, total_time, msg)."""
    N = len(rows)
    alt = init_alt(rows)
    target = target or (chr(96 + N) + str(N))
    tgt = parse(target)
    pos = parse(start)
    visits = {p: 0 for p in alt}
    visits[pos] = 1
    T = F(0)
    for i, (t, qs) in enumerate(moves):
        t = F(t)
        if t < 0:
            return False, T, f'move {i+1}: negative wait'
        q = parse(qs)
        if q not in alt:
            return False, T, f'move {i+1}: off board'
        n = apply_wait(pos, t, alt, N)
        T += t
        if not legal_jump(pos, q, alt):
            dz = alt[q] - alt[pos]
            return False, T, f'move {i+1} ({t},{qs}): illegal jump {name(pos)}->{qs} dz={dz}'
        visits[q] += 1
        if q == tgt:
            if i != len(moves) - 1:
                return False, T, f'move {i+1}: reached target early'
        elif visits[q] > 3:
            return False, T, f'move {i+1}: {qs} visited 4 times'
        if verbose:
            print(f'{i+1:3d} ({t},{qs}) n={n} T={T}')
        pos = q
    if pos != tgt:
        return False, T, 'did not reach target'
    return True, T, 'ok'


def necessity_check(rows, moves, start='a1'):
    """For each move with t>0, check some perturbed t' breaks the tour."""
    bad = []
    for i, (t, q) in enumerate(moves):
        t = F(t)
        if t == 0:
            continue
        broken = False
        for tp in [t + F(1, 997), t - F(1, 997), F(0), t + 1, t * 2]:
            if tp < 0 or tp == t:
                continue
            m2 = list(moves)
            m2[i] = (tp, q)
            ok, _, _ = simulate(rows, m2, start)
            if not ok:
                broken = True
                break
        if not broken:
            bad.append(i + 1)
    return bad



TOUR = "(0, b1), (0, d1), (8, d2), (9, c2), (0, a1), (0, b1), (0, c1), (0, e1), (20, e2), (0, d4), (0, c4), (0, b4), (0, b6), (0, d6), (0, f5), (0, h5), (0, h4), (0, h6), (0, h7), (0, g7), (0, e7), (0, e8), (0, d8), (10, f7), (8, d7), (6, d8), (36, d7), (12, c5), (54, b5), (56, a5), (96, b5), (84, c5), (29, b3), (93, b2), (96, b3), (124, b2), (96, b3), (124, c3), (92, d3), (120, c3), (92, d3), (120, c3), (92, d3), (120, d2), (92, d1), (120, d2), (92, e2), (116, e3), (92, e2), (112, e3), (92, e4), (108, e5), (88, e4), (108, e5), (88, e4), (108, e3), (88, f3), (104, g3), (88, f3), (100, g3), (88, f3), (100, g3), (88, g2), (96, g1), (88, g2), (92, g1), (88, g2), (92, g1), (88, h1), (88, h2), (88, h1), (84, h2), (88, h1), (84, h2), (88, h3), (0, f2), (84, f1), (84, f2), (84, f1), (84, f2), (84, f1), (63, d1), (42, b1), (60, c1), (80, c2), (0, b4), (80, c4), (72, b4), (80, c4), (54, a4), (60, a5), (68, a4), (80, a3), (68, a2), (72, a3), (68, a2), (72, a3), (68, a4), (72, a5), (68, b5), (72, c5), (51, e5), (0, f7), (0, h8)"

if __name__ == '__main__':
    import re
    moves = [(int(a), b) for a, b in re.findall(r'\((\d+),\s*([a-h][1-8])\)', TOUR)]
    ok, T, msg = simulate(MAIN_ROWS, moves)
    print('moves:', len(moves), '| legal:', ok, '| total minutes:', T, '|', msg)
    from collections import Counter
    c = Counter(q for _, q in moves); c['a1'] += 1
    print('max visits to any point other than h8:', max(v for k, v in c.items() if k != 'h8'))
    print('unnecessary sinks:', necessity_check(MAIN_ROWS, moves) or 'none')
    ex = [(1, 'b3'), (0, 'a3'), (0, 'a2'), (0, 'b4'), (5, 'a2'), (3, 'c1'), (2, 'a2'),
          (0, 'c1'), (15, 'd1'), (1, 'd2'), (0, 'c2'), (1, 'b2'), (0, 'b1'), (0, 'b3'),
          (0, 'c3'), (0, 'c4'), (0, 'a4'), (0, 'b4'), (0, 'd4')]
    print('sanity check on the puzzle example (should be 28):', simulate(EX_ROWS, ex)[1])
