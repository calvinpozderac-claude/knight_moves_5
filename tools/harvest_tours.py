"""Collect every complete tour (a1 -> h8) found in text files, logs and pickles, and verify each exactly.

    python3 tools/harvest_tours.py OUT.txt PATH [PATH ...]

PATH can be a file or a folder (searched recursively). A tour is any run of "(t, sq)" pairs
(also "(t, 'sq')" and Fraction(a, b) waits) that ends at h8. Each candidate is checked with the
exact Fraction simulator: legal jumps, h8 reached only at the end, at most 3 visits per point,
and no unnecessary sinks. OUT.txt gets one verified tour per line:
    <minutes> <jumps> <int|frac> (t, sq), (t, sq), ...
"int" means every sink is a whole number of units, so build/az can replay it.
"""
import os, pickle, re, sys
from collections import Counter
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sim import simulate, necessity_check, MAIN_ROWS  # noqa: E402

PAIR = r"\((?:\d+(?:/\d+)?|Fraction\(\d+,\s*\d+\)),\s*'?[a-h][1-8]'?\)"
SEQ = re.compile(r'(?:%s(?:,\s*)?){2,}' % PAIR)
ONE = re.compile(r"\((\d+(?:/\d+)?|Fraction\((\d+),\s*(\d+)\)),\s*'?([a-h][1-8])'?\)")
TEXT_EXT = {'.txt', '.log', '.py', '.json', '.jsonl', '.md', '.csv', '.cpp', '.h', ''}


def parse(seq):
    out = []
    for m in ONE.finditer(seq):
        t = Fraction(int(m.group(2)), int(m.group(3))) if m.group(2) else Fraction(m.group(1))
        out.append((t, m.group(4)))
    return out


def from_text(txt):
    for m in SEQ.finditer(txt):
        mv = parse(m.group(0))
        # a run may hold several tours back to back; split after every h8
        cur = []
        for t, q in mv:
            cur.append((t, q))
            if q == 'h8':
                yield cur
                cur = []


def from_pickle(fn):
    try:
        obj = pickle.load(open(fn, 'rb'))
    except Exception:
        return
    stack = [obj]
    while stack:
        o = stack.pop()
        if isinstance(o, (list, tuple)) and o and all(isinstance(x, (list, tuple)) and len(x) == 2 for x in o) \
                and all(isinstance(x[1], str) and re.fullmatch(r'[a-h][1-8]', x[1]) for x in o):
            mv = [(Fraction(x[0]), x[1]) for x in o]
            if mv[-1][1] == 'h8':
                yield mv
        elif isinstance(o, dict):
            stack.extend(o.values())
        elif isinstance(o, (list, tuple)):
            stack.extend(o)
        elif hasattr(o, '__dict__'):
            stack.extend(vars(o).values())


def files(paths):
    for p in paths:
        if os.path.isfile(p):
            yield p
        else:
            for root, dirs, fs in os.walk(p):
                dirs[:] = [d for d in dirs if d not in ('.git', '.venv', 'node_modules', '__pycache__')]
                for f in fs:
                    yield os.path.join(root, f)


def verify(mv):
    """Return (minutes, is_integer_sinks) if mv is a valid complete tour, else None."""
    if not mv or mv[-1][1] != 'h8' or any(q == 'h8' for _, q in mv[:-1]):
        return None
    try:
        ok, T, msg = simulate(MAIN_ROWS, mv)
    except Exception:
        return None
    if not ok:
        return None
    c = Counter(q for _, q in mv); c['a1'] += 1
    if max(v for k, v in c.items() if k != 'h8') > 3:
        return None
    if necessity_check(MAIN_ROWS, mv):
        return None
    return T, integer_sinks(mv)


def integer_sinks(mv):
    """True if every wait is a whole number of units for the group that sinks (replayable by build/az)."""
    rows = [[9, 8, 10, 12, 11, 8, 10, 17], [7, 9, 11, 9, 10, 12, 14, 12], [4, 7, 5, 8, 8, 6, 13, 10],
            [4, 10, 7, 9, 6, 8, 7, 9], [2, 6, 4, 2, 5, 9, 8, 11], [0, 3, 1, 4, 2, 7, 10, 7],
            [1, 2, 0, 1, 2, 5, 7, 6], [0, 2, 4, 3, 5, 6, 2, 4]]
    alt = [rows[7 - i // 8][i % 8] for i in range(64)]
    p = 0
    for t, sq in mv:
        q = (ord(sq[0]) - 97) + 8 * (int(sq[1]) - 1)
        if t:
            A = alt[p]; O = 63 - p
            n = sum(1 for i in range(64) if alt[i] == A and i != O)
            if t.denominator != 1 or int(t) % n:
                return False
            d = int(t) // n
            rise = alt[O] != A
            for i in range(64):
                if alt[i] == A and i != O: alt[i] -= d
            if rise: alt[O] += d
        p = q
    return True


def fmt(mv):
    return ', '.join(f'({t}, {q})' for t, q in mv)


def main():
    out_fn, paths = sys.argv[1], sys.argv[2:]
    seen, kept, n_cand = set(), [], 0
    for fn in files(paths):
        ext = os.path.splitext(fn)[1]
        if ext == '.pkl':
            cands = list(from_pickle(fn))
        elif ext in TEXT_EXT and os.path.getsize(fn) < 200_000_000:
            try:
                txt = open(fn, errors='ignore').read()
            except Exception:
                continue
            if 'h8' not in txt:
                continue
            cands = list(from_text(txt))
        else:
            continue
        for mv in cands:
            key = fmt(mv)
            if key in seen:
                continue
            seen.add(key); n_cand += 1
            r = verify(mv)
            if r:
                kept.append((r[0], len(mv), r[1], key))
    kept.sort()
    with open(out_fn, 'w') as f:
        for T, n, isint, key in kept:
            f.write(f'{T} {n} {"int" if isint else "frac"} {key}\n')
    ni = sum(1 for k in kept if k[2])
    print(f'{n_cand} distinct candidates, {len(kept)} valid complete tours ({ni} with whole-unit sinks) -> {out_fn}')


if __name__ == '__main__':
    main()
