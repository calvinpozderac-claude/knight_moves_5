"""subagents/strict_check.py on any board: a positive wait is "free" if the next jump stays legal when
the wait is nudged by +1/997 (the target sinks with the knight). usage: python3 strict_check4.py --example | FILE"""
import sys, re
sys.path.insert(0, '/home/user/knight_moves_5/tools')
from sim import MAIN_ROWS, EX_ROWS, init_alt, parse, apply_wait, legal_jump, simulate
from fractions import Fraction as F
EX = [(1, 'b3'), (0, 'a3'), (0, 'a2'), (0, 'b4'), (5, 'a2'), (3, 'c1'), (2, 'a2'), (0, 'c1'), (15, 'd1'), (1, 'd2'),
      (0, 'c2'), (1, 'b2'), (0, 'b1'), (0, 'b3'), (0, 'c3'), (0, 'c4'), (0, 'a4'), (0, 'b4'), (0, 'd4')]
if sys.argv[1] == '--example': rows, mv = EX_ROWS, [(F(t), q) for t, q in EX]
else:
    rows = MAIN_ROWS
    mv = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', open(sys.argv[1]).read().splitlines()[1])]
N = len(rows); alt = init_alt(rows); pos = parse('a1'); free = []
for i, (t, q) in enumerate(mv):
    Q = parse(q)
    if t > 0:
        a2 = dict(alt); apply_wait(pos, t + F(1, 997), a2, N)
        a0 = dict(alt)  # also: is the jump legal with no wait at all?
        if legal_jump(pos, Q, a2): free.append((i + 1, str(t), q, 'legal-at-0' if legal_jump(pos, Q, a0) else ''))
    apply_wait(pos, t, alt, N); pos = Q
print('tour', simulate(rows, mv)[:2], 'STRICT' if not free else f'free waits {free}')
