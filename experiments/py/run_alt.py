import sys
from st import *
from beam8 import run
def state_from(text):
    s = State()
    for t, q in parse_moves(text):
        g, rise = s.group()
        assert t % len(g) == 0
        s = s.step(t // len(g), ix(q))
    return s
if __name__ == '__main__':
    text = open(sys.argv[1]).read()
    W = int(sys.argv[2]); out = sys.argv[3]
    best = run(state_from(text), W, out=out, log=lambda m: print(m, flush=True))
    print('BEST', best[0])
