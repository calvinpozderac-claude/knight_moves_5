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



TOURS = {
    "Longest tour (7066 minutes)": "(0, b1), (0, d1), (8, d2), (9, c2), (0, a1), (0, b1), (0, c1), (0, e1), (20, e2), (0, d4), (0, c4), (0, b4), (0, b6), (0, d6), (0, f5), (0, h5), (0, h4), (0, h6), (0, h7), (0, g7), (0, e7), (0, e8), (0, d8), (8, e8), (1, g7), (0, e7), (0, g6), (4, g7), (8, f7), (12, d7), (0, c7), (8, d7), (6, d8), (52, d7), (13, c5), (63, b5), (76, c5), (112, b5), (100, a5), (132, b5), (112, a5), (132, a4), (0, b2), (108, b3), (132, b2), (104, b3), (132, b2), (104, b3), (132, c3), (100, d3), (128, c3), (100, d3), (128, c3), (100, d3), (128, d2), (96, e2), (124, d2), (96, e2), (124, e3), (96, f3), (116, e3), (96, f3), (116, g3), (96, f3), (112, g3), (96, g2), (108, g3), (96, g2), (108, g1), (96, g2), (104, g1), (96, h1), (100, g1), (96, h1), (100, h2), (96, h1), (96, h2), (96, h3), (0, f2), (69, h2), (72, h3), (0, f2), (92, f1), (92, e1), (92, f1), (88, e1), (92, f1), (88, f2), (0, e4), (88, e5), (84, e4), (88, e5), (84, e4), (88, e3), (0, c2), (0, b4), (0, c4), (0, b4), (84, c4), (0, a5), (84, a4), (84, a3), (80, a2), (76, a3), (80, a2), (76, a3), (80, a4), (57, a6), (0, c5), (40, e5), (57, f5), (57, f7), (36, f5), (38, f7), (54, f8), (72, g8), (68, f8), (72, g8), (68, f8), (54, h8)",
    "Q1 high finish (h8 at 19)": "(3, a2), (0, c3), (0, b3), (12, a3), (0, a2), (0, a4), (0, a5), (0, c4), (0, b4), (0, b6), (0, b7), (0, c7), (0, d7), (6, b8), (0, b6), (0, b4), (0, c4), (0, d4), (0, d3), (0, c1), (0, b1), (36, b3), (0, a3), (0, c2), (0, d2), (0, e2), (0, e1), (0, c1), (60, b1), (0, b3), (0, a3), (0, c2), (0, d2), (0, e2), (0, f2), (0, f3), (0, g5), (0, h3), (0, h5), (0, h6), (0, h4), (6, h3), (0, g5), (0, e5), (0, e6), (44, c6), (0, c5), (0, e5), (20, e6), (0, e5), (0, d5), (33, c3), (0, a2), (0, a1), (3, b1), (0, d1), (0, d3), (0, d4), (0, d2), (0, e2), (0, f2), (0, f3), (0, g5), (0, h3), (0, h4), (0, h6), (0, h7), (0, g7), (0, g8), (0, f8), (0, h8)",
    "Q2 no repeated point": "(0, c2), (0, a2), (9, a3), (0, a4), (0, a5), (0, c4), (0, b4), (0, b6), (0, b7), (0, c7), (0, e7), (0, f7), (0, g7), (1, h7), (0, h8)",
    "Q3 all 64 points equal at the finish": "(0, b1), (0, d1), (8, d2), (9, c2), (0, a1), (0, b1), (0, c1), (0, e1), (20, e2), (0, d4), (0, c4), (0, b4), (0, b6), (0, d6), (0, f5), (0, h5), (0, h4), (0, h6), (0, h7), (0, g7), (0, e7), (0, e8), (0, d8), (8, e8), (1, g7), (4, e7), (8, g7), (12, g6), (12, g8), (10, e8), (32, g8), (22, g6), (72, h6), (68, h7), (66, f6), (112, e4), (0, g5), (0, f7), (0, h8)",
    "Q4 longest wait 3 minutes": "(0, c2), (0, a2), (3, b2), (0, a4), (0, a5), (0, c4), (0, b4), (0, b6), (0, b7), (0, c7), (0, e7), (0, f7), (0, h7), (2, g7), (0, f7), (0, d8), (0, c8), (0, b8), (0, b6), (0, b4), (0, c4), (0, a5), (0, a4), (0, a2), (3, a1), (0, a2), (0, a3), (0, a4), (0, a5), (0, c4), (0, b4), (0, b6), (0, b7), (0, c7), (0, e7), (0, f7), (0, g7), (1, h7), (0, h8)",
}

# also check every tour file in data/tours (line 1 = claimed minutes, line 2 = the tour)
import glob as _glob, os as _os
for _fn in sorted(_glob.glob(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'data', 'tours', '*.txt'))):
    _lines = open(_fn).read().split('\n')
    TOURS[f'data/tours/{_os.path.basename(_fn)} (claimed {_lines[0].strip()} minutes)'] = _lines[1]


def parse_tour(text):
    import re
    from fractions import Fraction
    return [(Fraction(a), b) for a, b in re.findall(r'\((\d+(?:/\d+)?),\s*([a-h][1-8])\)', text)]

def final_altitudes(moves):
    alt = init_alt(MAIN_ROWS); pos = parse('a1')
    for t, q in moves:
        apply_wait(pos, F(t), alt, 8); pos = parse(q)
    return alt

if __name__ == '__main__':
    from collections import Counter
    for label, text in TOURS.items():
        moves = parse_tour(text)
        ok, T, msg = simulate(MAIN_ROWS, moves)
        c = Counter(q for _, q in moves); c['a1'] += 1
        alt = final_altitudes(moves)
        classes = Counter(alt.values())
        print(label)
        print('  jumps:', len(moves), '| legal:', ok, '|', msg, '| total minutes:', T)
        print('  most visits to any point other than h8:', max(v for k, v in c.items() if k != 'h8'))
        print('  longest single wait:', max(t for t, _ in moves), 'minutes')
        print('  h8 altitude at finish:', alt[(7, 7)])
        print('  largest same-altitude group at finish:', max(classes.values()), 'points')
        print('  unnecessary sinks:', necessity_check(MAIN_ROWS, moves) or 'none')
    ex = [(1, 'b3'), (0, 'a3'), (0, 'a2'), (0, 'b4'), (5, 'a2'), (3, 'c1'), (2, 'a2'),
          (0, 'c1'), (15, 'd1'), (1, 'd2'), (0, 'c2'), (1, 'b2'), (0, 'b1'), (0, 'b3'),
          (0, 'c3'), (0, 'c4'), (0, 'a4'), (0, 'b4'), (0, 'd4')]
    print('sanity check on the puzzle example (should be 28):', simulate(EX_ROWS, ex)[1])
