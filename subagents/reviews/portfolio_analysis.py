"""Portfolio analysis of finished tours (rev_portfolio). Cheap: ~1 s per tour.
usage: python3 subagents/reviews/portfolio_analysis.py TOURFILE...   (line 2 = tour; run from repo root)
Prints: phase minutes, dance sits, zero-wait hops, live-partner breaks, group-size profile M = n_up + n_low,
unused sit capacity per opposite pair, free (checker-only) waits, and an adjacency-free relaxation
bound on the dance from a few states along the tour (see PORTFOLIO.md for what it does and does not bound)."""
import sys, re, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
from sim import MAIN_ROWS, init_alt, parse, name, apply_wait
from fractions import Fraction as F
from collections import Counter

def load(fn):
    txt = open(fn).read().split('\n')[1]
    return [(F(a), b) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', txt)]

opp = lambda p: (7 - p[0], 7 - p[1])

def trace(moves):
    alt = init_alt(MAIN_ROWS); pos = (0, 0); T = 0; out = []
    for i, (t, qs) in enumerate(moves):
        a = alt[pos]; O = opp(pos); q = parse(qs)
        up = [p for p in alt if alt[p] == a and p != O]; low = [p for p in alt if alt[p] == a - 2]
        free = t > 0 and alt[q] == a and q != O          # dz=0 in-group jump after a wait: legal at t=0 too
        oalt = alt[O] - a                                  # 0: partner in the sitting group, -2: in the other group
        apply_wait(pos, t, alt, 8); T += t
        out.append(dict(i=i + 1, frm=pos, to=q, t=t, nup=len(up), nlow=len(low), oalt=oalt, T=T, free=free))
        pos = q
    return out

def state_after(moves, k):
    alt = init_alt(MAIN_ROWS); pos = (0, 0); vis = Counter({(0, 0): 1}); T = 0
    for (t, qs) in moves[:k]:
        apply_wait(pos, t, alt, 8); T += t; q = parse(qs); vis[q] += 1; pos = q
    return alt, pos, vis, T

def relax_bound(alt, pos, vis, T):
    """Two-group dance with adjacency ignored: every sit earns 4*n_up, consecutive sits alternate groups so
    n_t + n_{t+1} <= M(t); M = points at or below the knight's altitude (all lower points join at once),
    minus one per broken pair; one sitting member per pair (cap = 3 - visits, +1 on the knight's point),
    open pairs first, then closed pairs by descending cap (optimal break order). Sound only once no pair
    has a member below the two groups (otherwise a pair can supply both members, as a3/h6 and a4/h5 do)."""
    a = alt[pos]; H8, A1 = (7, 7), (0, 0)
    alive = {p: alt[p] <= a for p in alt}
    cap = {p: max(0, 3 - vis[p] + (p == pos)) if alive[p] else 0 for p in alt}; cap[H8] = cap[A1] = 0
    M = sum(alive.values()); seen = set(); openc = []; closed = []; below = 0
    for p in alt:
        key = tuple(sorted([p, opp(p)]))
        if key in seen: continue
        seen.add(key); o = opp(p)
        if alive[p] and alive[o]:
            closed.append(max(cap[p], cap[o])); below += min(alt[p], alt[o]) < a - 2
        elif alive[p] or alive[o]: openc.append(cap[p] + cap[o])
    seq = []
    for c in openc: seq += [M] * c
    for c in sorted(closed, reverse=True):
        if c > 0: M -= 1; seq += [M] * c
    return int(T + 4 * sum(seq[0::2])), len(seq), below

def report(fn):
    m = load(fn); out = trace(m); T = out[-1]['T']
    sits = [o for o in out if o['t'] > 0]
    k0 = next(o['i'] for o in sits if o['nup'] + o['nlow'] >= 40)        # first sit of the big two-group dance
    dance = [o for o in sits if o['i'] >= k0]
    hops = [o['i'] for o in out if o['i'] >= k0 and o['t'] == 0]
    breaks = sum(1 for o in dance if o['oalt'] in (0, -2))
    Ms = [o['nup'] + o['nlow'] for o in dance]
    vis = Counter({'a1': 1}); dsat = Counter()
    for o in out:
        vis[name(o['to'])] += 1
        if o['t'] > 0 and o['i'] >= k0: dsat[name(o['frm'])] += 1
    both = []; left_on_sitters = []; never = []
    for x in 'abcdefgh':
        for y in range(1, 9):
            s_ = f'{x}{y}'; o_ = name(opp(parse(s_)))
            if s_ > o_ or 'h8' in (s_, o_): continue
            if dsat[s_] and dsat[o_]: both.append(f'{s_}/{o_}')
            sitters = [z for z in (s_, o_) if dsat[z]]
            if not sitters: never.append(f'{s_}/{o_}(v{vis[s_]},{vis[o_]})')
            for z in sitters:
                if vis[z] < 3: left_on_sitters.append(f'{z}:{3-vis[z]}')
    unused = left_on_sitters
    print(f'== {fn}: {T} min, {len(m)} moves, free(checker-only) waits={sum(o["free"] for o in out)}')
    print(f'  T after move 35 = {out[34]["T"]}; dance (big two-group) from move {k0} at T={out[k0-2]["T"]}')
    print(f'  sits: total {len(sits)}, in dance {len(dance)}, zero-wait moves in dance {len(hops)} {hops}')
    print(f'  live-partner breaks {breaks}, sits/break {len(dance)/max(1,breaks):.2f}, mean min/sit {float(sum(o["t"] for o in dance))/len(dance):.1f}')
    print(f'  M=n_up+n_low every 8 dance sits: {Ms[::8]} ... last {Ms[-1]}; final move t={out[-1]["t"]} n={out[-1]["nup"]}')
    print(f'  pairs with both members sat in the dance: {both}; visits left on dance sitters: {unused}; pairs never sat in dance: {never}')
    for k in sorted(set([35, k0 - 1, k0 + 6, k0 + 20, len(m) - 30])):
        alt, pos, v, Tk = state_after(m, k); b, S, below = relax_bound(alt, pos, v, Tk)
        print(f'  relax bound after move {k:3d} (T={Tk}): {b}  sits<= {S}  pairs-with-member-below={below}{"  (NOT sound)" if below else ""}')

if __name__ == '__main__':
    for fn in sys.argv[1:]: report(fn)
