from st import *
PHASE_A = "(0, b1), (0, d1), (8, d2), (9, c2), (0, a1), (0, b1), (0, c1), (0, e1), (20, e2), (0, d4), (0, c4), (0, b4), (0, b6), (0, d6), (0, f5), (0, h5), (0, h4), (0, h6), (0, h7), (0, g7), (0, e7), (0, e8), (0, d8)"
def phaseA_state():
    s = State()
    for t, q in parse_moves(PHASE_A):
        g, rise = s.group()
        n = len(g)
        assert t % n == 0
        s = s.step(t // n, ix(q))
    return s
