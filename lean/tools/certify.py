"""Write a Lean certificate that a tour obeys every rule of the puzzle (`KM5.Valid`).

usage: python3 lean/tools/certify.py TOURFILE NAME > lean/KnightMoves5/Certs/NAME.lean
       (TOURFILE: line 2 "(t, sq), ..." or a single line of moves)

Strict waits (target outside the sinking group) need no witness. For every other positive wait the
exact Python simulator finds a breaking t' (small changes, 0, t+1, 2t and every "merge" value that
lands the group on another point's altitude), and the Lean kernel re-checks it.
"""
import re
import sys
from fractions import Fraction as F
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from sim import MAIN_ROWS, init_alt, parse, apply_wait, simulate  # noqa: E402
from gen_tour import lean_tour  # noqa: E402


def lean_q(t):
    t = F(t)
    return str(t.numerator) if t.denominator == 1 else f'({t.numerator}/{t.denominator} : ℚ)'


def load(path):
    lines = Path(path).read_text().splitlines()
    txt = lines[1] if len(lines) > 1 and lines[0].strip().isdigit() else lines[0]
    return txt, [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', txt)]


def witnesses(moves):
    ok, T, msg = simulate(MAIN_ROWS, moves)
    if not ok:
        sys.exit(f'tour is not legal: {msg}')
    alt = init_alt(MAIN_ROWS)
    pos = parse('a1')
    out = []
    for i, (t, q) in enumerate(moves):
        Q = parse(q)
        if t > 0:
            A = alt[pos]
            O = (7 - pos[0], 7 - pos[1])
            G = [p for p in alt if alt[p] == A and p != O]
            if Q in G:  # not strict: find a breaking t'
                n = len(G)
                cands = [t + F(1, 997), t - F(1, 997), F(0), t + 1, 2 * t]
                cands += sorted({n * (A - alt[y]) for y in alt if alt[y] < A})
                cands += sorted({n * (alt[y] - alt[O]) / 2 for y in alt if alt[y] > alt[O]} if alt[O] != A else [])
                for tp in cands:
                    if tp < 0 or tp == t:
                        continue
                    m2 = list(moves)
                    m2[i] = (tp, q)
                    if not simulate(MAIN_ROWS, m2)[0]:
                        out.append((i, tp))
                        break
                else:
                    sys.exit(f'move {i + 1}: no breaking value found among the candidates')
        apply_wait(pos, t, alt, 8)
        pos = Q
    return T, out


def main(path, name):
    txt, moves = load(path)
    T, W = witnesses(moves)
    fname = Path(path).name  # a full path may contain "/-", which opens a nested Lean comment
    tour = lean_tour('tour', txt, fname).strip()
    S = ', '.join(f'({i}, {lean_q(tp)})' for i, tp in W)
    idx = ', '.join(str(i) for i, _ in W)
    print('import KnightMoves5.Certify\n')
    print(f'namespace KM5.Cert.{name}\n\nopen KM5\n')
    print(tour + '\n')
    print('theorem legal : Legal tour :=')
    print('  ⟨by decide +kernel, by decide +kernel, by decide +kernel, by decide +kernel⟩\n')
    print(f'theorem strictExcept : StrictExcept start tour [{idx}] 0 := by decide +kernel\n')
    for k, (i, tp) in enumerate(W):
        print(f'theorem breaks{k} : ¬ MovesOK start (setWait tour {i} {lean_q(tp)}) := by decide +kernel\n')
    print(f'theorem time_eq : time tour = {lean_q(T)} := by decide +kernel\n')
    print(f'/-- `{fname}`: {T} minutes, obeys every rule; {len(W)} non-strict wait(s), each with a breaking t\'. -/')
    print(f'theorem valid : Valid tour := valid_of_certificate tour [{S}] legal strictExcept (by')
    if not W:
        print('  intro p hp; simp at hp)')
    else:
        pats = ' | '.join(['rfl'] * len(W))
        prfs = ', '.join(f'⟨by norm_num, breaks{k}⟩' for k in range(len(W)))
        print('  intro p hp')
        print('  simp only [List.mem_cons, List.not_mem_nil, or_false] at hp')
        print(f'  rcases hp with {pats}')
        print(f'  exacts [{prfs}])')
    print(f'\nend KM5.Cert.{name}')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
