"""Differential test: Lean's `MovesOK` vs the Python simulator (tools/sim.py).

Writes test/DiffTest.lean with N tours (real tours, and mutants with a changed wait or target). For each
one it states Python's verdict as a theorem, `MovesOK start c` or `¬ MovesOK start c`, proved by
`decide +kernel`. The file compiling without errors means Lean and Python agree on every case.
(Compiled `#eval` is exponential on these definitions; the kernel caches shared subterms.)
"""
import random
import re
import sys
from fractions import Fraction as F
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from sim import MAIN_ROWS, init_alt, parse, apply_wait, legal_jump  # noqa: E402


def moves_ok(moves):
    """Waits >= 0 and every jump legal (no visit or end conditions), like Lean's MovesOK."""
    alt = init_alt(MAIN_ROWS)
    pos = parse('a1')
    for t, q in moves:
        if t < 0:
            return False
        Q = parse(q)
        apply_wait(pos, t, alt, 8)
        if not legal_jump(pos, Q, alt):
            return False
        pos = Q
    return True


def lean_move(t, q):
    t = F(t)
    ts = str(t.numerator) if t.denominator == 1 else f'({t.numerator}/{t.denominator} : ℚ)'
    return f'⟨{ts}, ({ord(q[0]) - 97}, {int(q[1]) - 1})⟩'


def main(n_cases=200, seed=1):
    rng = random.Random(seed)
    tours = []
    for line in (ROOT / 'data/tours/curriculum.txt').read_text().splitlines():
        mv = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', line)]
        if mv:
            tours.append(mv)
    for p in sorted((ROOT / 'data/tours').glob('record_9*.txt')):
        tours.append([(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', p.read_text())])
    cases = []
    while len(cases) < n_cases:
        mv = list(rng.choice(tours))
        mv = mv[:rng.randint(1, min(len(mv), 45))]
        kind = rng.random()
        if kind < 0.35:
            pass
        elif kind < 0.7:
            i = rng.randrange(len(mv))
            t, q = mv[i]
            mv[i] = (max(F(0), t + rng.choice([F(-1), F(1), F(1, 2), F(3), F(1, 7)])), q)
        else:
            i = rng.randrange(len(mv))
            t, q = mv[i]
            q2 = chr(97 + rng.randrange(8)) + str(1 + rng.randrange(8))
            mv[i] = (t, q2)
        cases.append((mv, moves_ok(mv)))
    out = ['import KnightMoves5.Rules', '', 'open KM5', '', 'set_option maxRecDepth 100000', '']
    for k, (mv, v) in enumerate(cases):
        out.append(f'def c{k} : List Move := [' + ', '.join(lean_move(t, q) for t, q in mv) + ']')
        out.append(f'example : {"" if v else "¬ "}MovesOK start c{k} := by decide +kernel')
    (Path(__file__).parent / 'DiffTest.lean').write_text('\n'.join(out) + '\n')
    print(f'{len(cases)} cases, {sum(v for _, v in cases)} legal per Python')


if __name__ == '__main__':
    main()
