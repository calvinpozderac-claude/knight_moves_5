"""For every free wait of a tour (the next jump is a dz=0 move inside the knight's own group, legal for
any wait), report:
  - drop0:  does setting it to 0 (all other waits fixed) break the tour?
  - probes: which of necessity_check's probes (t+1/997, t-1/997, 0, t+1, 2t) break it
  - grid:   every sink s = k/12 units, 0 <= s <= 3*t/n + 2, that still gives a legal tour with the other waits
            fixed (if only s = t/n survives, the wait is the unique working value on that grid)
usage: python3 subagents/fractional/nec_detail.py FILE | --example"""
import sys, re
from fractions import Fraction as F
sys.path.insert(0, 'tools')
from sim import MAIN_ROWS, EX_ROWS, init_alt, parse, name, apply_wait, simulate


def free_waits(rows, moves):
    N = len(rows); alt = init_alt(rows); pos = parse('a1'); out = []
    for i, (t, qs) in enumerate(moves):
        q = parse(qs); O = (N - 1 - pos[0], N - 1 - pos[1])
        ingroup = alt[q] == alt[pos] and q != O
        n = apply_wait(pos, F(t), alt, N)
        if F(t) > 0 and ingroup: out.append((i, n))
        pos = q
    return out


def main():
    if sys.argv[1] == '--example':
        rows = EX_ROWS
        moves = [(1, 'b3'), (0, 'a3'), (0, 'a2'), (0, 'b4'), (5, 'a2'), (3, 'c1'), (2, 'a2'), (0, 'c1'), (15, 'd1'), (1, 'd2'),
                 (0, 'c2'), (1, 'b2'), (0, 'b1'), (0, 'b3'), (0, 'c3'), (0, 'c4'), (0, 'a4'), (0, 'b4'), (0, 'd4')]
        moves = [(F(t), q) for t, q in moves]
    else:
        rows = MAIN_ROWS
        txt = open(sys.argv[1]).read().splitlines()[1]
        moves = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', txt)]
    ok, T, msg = simulate(rows, moves, target=None)
    print('tour ok', ok, 'T', T)
    for i, n in free_waits(rows, moves):
        t = moves[i][0]
        def works(tp):
            m2 = list(moves); m2[i] = (tp, moves[i][1]); return simulate(rows, m2)[0]
        probes = {lbl: not works(tp) for lbl, tp in [('+e', t + F(1, 997)), ('-e', t - F(1, 997)), ('0', F(0)), ('+1', t + 1), ('2t', 2 * t)] if tp >= 0}
        s0 = t / n; grid = []
        k = 0
        while F(k, 12) <= 3 * s0 + 2:
            if works(F(k, 12) * n): grid.append(F(k, 12))
            k += 1
        print(f'move {i+1} ({t}, {moves[i][1]}) n={n} sink={s0}: drop0_breaks={probes["0"]} probes_breaking={[k for k, v in probes.items() if v]} '
              f'working_sinks_on_1/12_grid={[str(g) for g in grid]}')


main()
