import sys, re
from sim import simulate, necessity_check, MAIN_ROWS
from collections import Counter
def check(text, show=True):
    moves = [(int(a) if '/' not in a else __import__('fractions').Fraction(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', text)]
    ok, T, msg = simulate(MAIN_ROWS, moves)
    c = Counter(q for _, q in moves); c['a1'] += 1
    mv = max(v for k, v in c.items() if k != 'h8')
    bad = necessity_check(MAIN_ROWS, moves)
    if show:
        print(f'moves={len(moves)} ok={ok} T={T} {msg} maxvisits={mv} maxwait={max(t for t,_ in moves)} unnecessary={bad}')
    return ok, T, mv, bad
if __name__ == '__main__':
    check(sys.argv[1])
