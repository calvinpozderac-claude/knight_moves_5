import sys, pickle, re
from fractions import Fraction as F
from collections import Counter
from sim import MAIN_ROWS, init_alt, apply_wait, parse, legal_jump
def final_alts(moves):
    alt = init_alt(MAIN_ROWS); pos = parse('a1')
    for t, q in moves:
        apply_wait(pos, F(t), alt, 8); pos = parse(q)
    return alt
def groups(alt):
    c = Counter(alt.values())
    return sorted(c.items(), key=lambda kv: -kv[1])
if __name__ == '__main__':
    for fn in sys.argv[1:]:
        moves = pickle.load(open(fn, 'rb'))
        g = groups(final_alts(moves))
        print(fn, 'largest groups:', g[:4], 'h8 final:', final_alts(moves)[(7,7)])
