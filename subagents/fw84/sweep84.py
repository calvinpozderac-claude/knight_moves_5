"""Exhaustive free-wait insertion sweep (extends fractional/insert.py).
For every zero-wait hop i of the tour:
  - in-group hop (dz=0 knight move to a member of the knight's group): free sink s = k/6 units, k=1..48;
  - out-of-group zero-wait hop: every other pinned sink (the other dz sign) that is > 0.
Every later wait is recomputed (same squares; 'plain' keeps each later dz, 'flex' also tries the other dz
when the original would need a negative sink). Keep tours that pass simulate + maxvisits<=3 +
necessity_check == []. Then pairs among the top singles, then iterate greedily from the best accepted tour.
usage (from subagents/fw84): python3 sweep84.py FILE OUTPREFIX [ROUNDS]"""
import sys, re, itertools
from fractions import Fraction as F
from collections import Counter
sys.path.insert(0, '../../tools'); sys.path.insert(0, '../fractional')
from sim import MAIN_ROWS, init_alt, parse, apply_wait, legal_jump, simulate, necessity_check
from repin import profile, rebuild as rebuild_plain, fmt


def rebuild_flex(prof, override):
    alt = init_alt(MAIN_ROWS); pos = parse('a1'); moves = []
    for i, (qs, dz, free, sink) in enumerate(prof):
        q = parse(qs); O = (7 - pos[0], 7 - pos[1]); A = alt[pos]
        n = sum(1 for p in alt if alt[p] == A and p != O); ingroup = alt[q] == A and q != O
        if i in override: s = override[i]
        elif ingroup: s = sink
        else:
            opts = [dz] + [d for d in ([2, -2] if abs(dz) == 2 else [1, -1] if abs(dz) == 1 else [0]) if d != dz]
            s = None
            for d in opts:
                x = d - (alt[q] - A)
                if x >= 0: s = x; break
        if s is None or s < 0: return None
        apply_wait(pos, s * n, alt, 8)
        if not legal_jump(pos, q, alt): return None
        moves.append((s * n, qs)); pos = q
    return moves


def good(m):
    ok, T, _ = simulate(MAIN_ROWS, m)
    if not ok: return None
    c = Counter(q for _, q in m); c['a1'] += 1
    if max(v for k, v in c.items() if k != 'h8') > 3: return None
    return T


def options(mv):
    """(index, sink) candidates at every zero-wait hop"""
    alt = init_alt(MAIN_ROWS); pos = parse('a1'); out = []
    for i, (t, qs) in enumerate(mv):
        q = parse(qs); O = (7 - pos[0], 7 - pos[1]); A = alt[pos]
        if t == 0 and i < len(mv) - 1:  # a wait before the final jump is never necessary
            if alt[q] == A and q != O:
                out += [(i, F(k, 6), 'free') for k in range(1, 49)]
            else:
                dx, dy = abs(q[0] - pos[0]), abs(q[1] - pos[1]); st = sorted([dx, dy])
                dzs = [0] if st == [1, 2] else [2, -2] if st == [0, 1] else [1, -1]
                for d in dzs:
                    x = d - (alt[q] - A)
                    if x > 0: out.append((i, x, 'pin'))
        apply_wait(pos, t, alt, 8); pos = q
    return out


def sweep(mv, tag):
    prof = profile(mv); T0 = good(mv)
    opts = options(mv)
    print(f'[{tag}] base {T0}; zero-wait hops {sum(1 for t,_ in mv if t == 0)}; options {len(opts)}', flush=True)
    raw = {}
    for i, s, kind in opts:
        for rb_name, rb in (('plain', rebuild_plain), ('flex', rebuild_flex)):
            m2 = rb(prof, {i: s})
            if m2 is None: continue
            T = good(m2)
            if T is None: continue
            key = fmt(m2)
            if key not in raw: raw[key] = (T, i, s, kind, rb_name, m2)
    vals = sorted(raw.values(), key=lambda x: -x[0])
    print(f'[{tag}] legal single variants {len(vals)}; spread', Counter(int(T - T0) for T, *_ in vals).most_common(12), flush=True)
    acc = []
    for T, i, s, kind, rbn, m2 in vals:
        if T <= T0: break
        bad = necessity_check(MAIN_ROWS, m2)
        print(f'  single move {i+1} {kind} sink {s} ({rbn}): T={T} unnecessary={bad}', flush=True)
        if not bad: acc.append((T, m2))
    # pairs among the top 40 improving singles (by spot)
    top = [(i, s, kind) for T, i, s, kind, rbn, m2 in vals[:40] if T > T0]
    for (i, si, _), (j, sj, _) in itertools.combinations(top, 2):
        if i == j: continue
        for rb in (rebuild_plain, rebuild_flex):
            m2 = rb(prof, {i: si, j: sj})
            if m2 is None: continue
            T = good(m2)
            if T is None or T <= T0: continue
            if (acc and T <= max(a[0] for a in acc)): continue
            bad = necessity_check(MAIN_ROWS, m2)
            print(f'  pair moves {i+1},{j+1} sinks {si},{sj}: T={T} unnecessary={bad}', flush=True)
            if not bad: acc.append((T, m2))
    acc.sort(key=lambda x: -x[0])
    return T0, acc


if __name__ == '__main__':
    mv = [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', open(sys.argv[1]).read().splitlines()[1])]
    pre = sys.argv[2]; R = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    for r in range(R):
        T0, acc = sweep(mv, f'round {r+1}')
        if not acc: print('no accepted improvement; stop'); break
        T, mv = acc[0]
        open(f'{pre}_r{r+1}.txt', 'w').write(f'{T}\n{fmt(mv)}\n')
        print(f'round {r+1}: {T0} -> {T} written {pre}_r{r+1}.txt', flush=True)
