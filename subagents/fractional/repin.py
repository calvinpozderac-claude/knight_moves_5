"""(--example: the 4x4 puzzle example)
Re-pin a tour after changing one wait: keep every jump's target square and its dz (and, for free
sinks, the sink amount in altitude units), recompute the other waits exactly (Fractions), and report
the new total and the checker verdict.
usage: python3 subagents/fractional/repin.py FILE MOVE  (tries sink amounts k/6 units, k=0..36, at MOVE)"""
import sys, re
from fractions import Fraction as F
sys.path.insert(0, 'tools'); sys.path.insert(0, 'subagents/fractional')
from sim import MAIN_ROWS, init_alt, parse, name, apply_wait, legal_jump, simulate, necessity_check
from classify import moves_of
from sim import EX_ROWS
ROWS, N = MAIN_ROWS, 8


def profile(moves):
    """per move: (target, dz after the wait, free?, sink in units)"""
    alt = init_alt(ROWS); pos = parse('a1'); out = []
    for t, qs in moves:
        q = parse(qs); O = (N - 1 - pos[0], N - 1 - pos[1]); free = alt[q] == alt[pos] and q != O
        n = apply_wait(pos, t, alt, N)
        out.append((qs, alt[q] - alt[pos], free, F(t) / n)); pos = q
    return out


def rebuild(prof, override):
    """override: {move index: sink units}. Returns moves or None."""
    alt = init_alt(ROWS); pos = parse('a1'); moves = []
    for i, (qs, dz, free, sink) in enumerate(prof):
        q = parse(qs); O = (N - 1 - pos[0], N - 1 - pos[1]); A = alt[pos]
        S = [p for p in alt if alt[p] == A and p != O]; n = len(S)
        ingroup = alt[q] == A and q != O
        if i in override: s = override[i]
        elif free or (ingroup and sink == 0): s = sink if ingroup else None
        else: s = dz - (alt[q] - A)  # sink so the jump keeps its dz
        if s is None or s < 0: return None
        t = s * n
        apply_wait(pos, t, alt, N)
        if not legal_jump(pos, q, alt): return None
        moves.append((t, qs)); pos = q
    return moves


def fmt(moves):
    return ', '.join(f'({t}, {q})' for t, q in moves)


if __name__ == '__main__':
    if sys.argv[1] == '--example':
        ROWS, N = EX_ROWS, 4
        mv = [(F(t), q) for t, q in [(1, 'b3'), (0, 'a3'), (0, 'a2'), (0, 'b4'), (5, 'a2'), (3, 'c1'), (2, 'a2'), (0, 'c1'), (15, 'd1'), (1, 'd2'), (0, 'c2'), (1, 'b2'), (0, 'b1'), (0, 'b3'), (0, 'c3'), (0, 'c4'), (0, 'a4'), (0, 'b4'), (0, 'd4')]]
    else:
        mv = moves_of(open(sys.argv[1]).read().splitlines()[1])
    i = int(sys.argv[2]) - 1
    prof = profile(mv)
    print('move', i + 1, prof[i])
    for k in range(0, 37):
        s = F(k, 6); m2 = rebuild(prof, {i: s})
        if m2 is None: continue
        ok, T, msg = simulate(ROWS, m2)
        if not ok: continue
        from collections import Counter
        c = Counter(q for _, q in m2); c['a1'] += 1
        mvx = max(v for kk, v in c.items() if kk not in ('h8', 'd4'))
        bad = necessity_check(ROWS, m2)
        print(f'sink {s}: T={T} ok={ok} maxvisits={mvx} unnecessary={bad}')
        if len(sys.argv) > 3 and str(s) == sys.argv[3]: print(fmt(m2))
