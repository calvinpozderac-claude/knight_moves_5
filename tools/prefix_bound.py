"""Upper bounds for partial tours, and a branch-and-bound exploration from the start.

The bound: replay Theorem 3's invariant (lean/KnightMoves5/StrictBound3.lean, XS.step) along the moves
played so far, then bound every strict completion with what is left:
  - the visits still unused (at most 1 + sum of (3 - visits) more moves),
  - the rescue budget already spent (B + B_nK + B_cov + Psi so far),
  - the jumps so far (Z),
  - the climb to h8 still owed: if h8 has not sunk, the knight must still stand at h8's altitude,
    and that stretch can only rescue knock-outs made with the floor at or below
    max(current floor, open records' floors), so alt(h8) - 2 - that much budget is wasted.
At the start it equals Theorem 3's 15,606 5/6. It is never below a tour's final length at any prefix
(checked on all 1,300 strict stored tours: `check`).

A dead-state test prunes pits: if the altitudes above the knight have a gap of more than 2 below h8,
and no reachable point's opposite is also reachable, h8 can never be reached.

usage:
  python3 tools/prefix_bound.py trace TOURFILE         bound after every prefix of a tour
  python3 tools/prefix_bound.py bfs DEPTH              all strict prefixes of DEPTH moves, ranked
  python3 tools/prefix_bound.py best "(t,sq) ..." N    best-first search below a prefix, N expansions
  python3 tools/prefix_bound.py check                  bound >= final time on every stored strict tour
"""
import sys, re, heapq, itertools, glob
from fractions import Fraction as F

ROOT = __file__.rsplit('/', 2)[0]
sys.path.insert(0, ROOT + '/tools')
from sim import MAIN_ROWS, init_alt, parse, name

SQ = [(x, y) for x in range(8) for y in range(8)]
H8 = (7, 7)
def opp(p): return (7 - p[0], 7 - p[1])

class XS:
    __slots__ = ('alt', 'pos', 'm', 'mu', 'rho', 'recs', 'Hm', 'T', 'R', 'D', 'Z', 'B', 'BnK', 'Bcov',
                 'M', 'vis', 'h8s', 'h8alt', 'moves', '_ub')
    def copy(s):
        n = XS.__new__(XS)
        for k in XS.__slots__:
            if not hasattr(s, k): continue
            v = getattr(s, k)
            setattr(n, k, dict(v) if isinstance(v, dict) else (list(v) if isinstance(v, list) else v))
        return n

def start():
    s = XS.__new__(XS)
    s.alt = init_alt(MAIN_ROWS); s.pos = (0, 0); s.m = F(0)
    s.mu = {p: F(0) for p in SQ}; s.rho = {p: F(0) for p in SQ}; s.recs = []
    s.Hm = F(0); s.T = s.R = s.D = s.Z = s.B = s.BnK = s.Bcov = F(0); s.M = 0
    s.vis = {p: 0 for p in SQ}; s.vis[(0, 0)] = 1; s.h8s = 0; s.h8alt = None; s.moves = []
    return s

def step(s, t, Q):
    """Apply move (wait t, jump to Q) exactly as XS.step does. Returns a new state."""
    t = F(t); A = s.alt; P = s.pos; k = A[P]; O = opp(P)
    G = [x for x in SQ if A[x] == k and x != O]
    d = t / len(G)
    rises = (A[O] != k)
    A2 = dict(A)
    for x in G: A2[x] = k - d
    if rises: A2[O] = A[O] + d
    rise = d if rises else F(0)
    low = k - d
    inG = set(G)
    hit = [r for r in s.recs if not r[4] and r[0] in inG]
    C = sum((r[1] for r in hit), F(0))
    marked = [(r[0], r[1], r[2], r[3], True, r[5], r[6]) if (not r[4] and r[0] in inG) else r for r in s.recs]
    n = s.copy(); n.moves = s.moves + [(t, Q)]
    fl = low < s.m
    h8in = H8 in inG
    if s.h8s == 0 and h8in: n.h8alt = k
    if fl:
        n.m = low
        n.recs = ([(O, s.m - low, k - s.m, s.m, False, rise, d - rise)] if d > 2 else []) + [r for r in marked if not r[4]]
        n.Hm = A2[Q] - low
        n.B = s.B + (s.m - low); n.BnK = s.BnK + (0 if d > 2 else s.m - low)
        n.h8s = 2 if s.h8s == 2 else (2 if (s.h8s == 1 or h8in) else 0)
    else:
        n.recs = marked
        n.Hm = max(s.Hm, A2[Q] - s.m)
        n.h8s = 2 if s.h8s == 2 else (1 if (s.h8s == 1 or h8in) else 0)
    for x in G:
        n.mu[x] = n.m; n.rho[x] = F(0)
    if rises: n.rho[O] = s.rho[O] + d
    n.Bcov = s.Bcov + C
    n.alt = A2; n.pos = Q
    n.T = s.T + t; n.R = s.R + rise; n.D = s.D + d; n.Z = s.Z + (A2[Q] - A2[P]); n.M = s.M + 1
    n.vis[Q] += 1
    return n

def psi(s):
    cov = sum((r[1] for r in s.recs if r[4]), F(0))
    return max(F(0), s.Hm - 2) - cov + max(F(0), 2 - (s.alt[s.pos] - s.m)) + sum((max(F(0), 2 - r[2]) for r in s.recs), F(0))

def h8_waste(s):
    """Budget the climb to h8 will still waste (0 once paid)."""
    if s.h8s == 2: return F(0)
    mus = max([s.m] + [r[3] for r in s.recs])
    a8 = s.h8alt if s.h8s == 1 else s.alt[H8]
    return a8 - 2 - mus

def ub(s):
    """Upper bound on the total time of every strict tour that starts with s.moves (Theorem 3 from s)."""
    dM = 1 + sum(3 - s.vis[x] for x in SQ if x != H8)
    used = s.B + s.BnK + s.Bcov + psi(s)
    K = used + 2 * dM - h8_waste(s)
    Bmax = s.B + 2 * dM
    def f(B):
        u = max(F(0), 2 * B - K)
        return 65 * B - F(7, 6) * u - u * u / 12
    Bstar = (K + 188) / 2
    cands = [min(max(Bstar, s.B), Bmax), s.B, min(max(K / 2, s.B), Bmax)]
    return 421 + s.Z + 2 * dM + max(f(B) for B in cands)

def load(path):
    lines = open(path).read().splitlines()
    txt = lines[1] if len(lines) > 1 and lines[0].strip().isdigit() else lines[0]
    return [(F(a), parse(b)) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', txt)]



def alive(s):
    """False if the knight can provably never reach h8.
    A jump climbs at most 2; points only sink at a sit at their own altitude (so at or below the
    knight's reach), and the only point that rises is the opposite of the sitter. Collect the points
    the knight can reach by stepping stones at most 2 apart in altitude (ignoring geometry). If h8 is
    not among them and no reachable point's opposite is also reachable (the one way to lift a
    stepping stone into the gap), the knight is trapped."""
    A = s.alt; k = A[s.pos]
    alts = sorted(set(A.values()))
    ceil = k
    for a in alts:
        if a <= ceil + 2: ceil = max(ceil, a)
        else: break
    if A[H8] <= ceil + 2: return True
    reach = [x for x in SQ if A[x] <= ceil]
    rs = set(reach)
    return any(opp(x) in rs for x in reach)


SHAPES = []
for dx in (-2, -1, 0, 1, 2):
    for dy in (-2, -1, 0, 1, 2):
        s = sorted([abs(dx), abs(dy)])
        if s == [0, 1]: SHAPES.append((dx, dy, 2))
        elif s == [0, 2]: SHAPES.append((dx, dy, 1))
        elif s == [1, 2]: SHAPES.append((dx, dy, 0))

def moves(s):
    """All strict moves: zero-wait jumps and sits whose wait is pinned by their own jump (no h8 yet)."""
    A = s.alt; P = s.pos; k = A[P]; O = opp(P)
    n = sum(1 for x in SQ if A[x] == k and x != O)
    out = []
    for dx, dy, adz in SHAPES:
        Q = (P[0] + dx, P[1] + dy)
        if not (0 <= Q[0] < 8 and 0 <= Q[1] < 8) or Q == H8 or s.vis[Q] >= 3: continue
        for dz in {adz, -adz}:
            if A[Q] - k == dz: out.append((F(0), Q))
            if A[Q] == k and Q != O: continue          # target in the sinking group: not strict
            d = dz - A[Q] + k
            if d > 0: out.append((n * d, Q))
    return out


def key(s):
    return (s.pos, tuple(s.alt[x] for x in SQ), tuple(s.vis[x] for x in SQ))

def fmt(mv):
    return ' '.join(f"({t},{name(q)})" for t, q in mv)

def cmd_trace(path):
    s = start(); print(f"after   0 moves: bound {float(ub(s)):9.1f}")
    for i, (t, q) in enumerate(load(path)):
        s = step(s, t, q); print(f"after {i+1:3d} moves: bound {float(ub(s)):9.1f}   time so far {float(s.T):7.0f}")

def cmd_bfs(depth):
    layer = {key(start()): start()}
    for dep in range(1, depth + 1):
        nxt = {}; dead = 0
        for s in layer.values():
            for t, Q in moves(s):
                c = step(s, t, Q)
                if not alive(c): dead += 1; continue
                kk = key(c); c._ub = ub(c)
                if kk not in nxt: nxt[kk] = c
                else:
                    o = nxt[kk]; fut = min(o._ub - o.T, c._ub - c.T)
                    best = o if o.T >= c.T else c; best._ub = best.T + fut; nxt[kk] = best
        layer = nxt; us = sorted(float(s._ub) for s in layer.values())
        print(f"depth {dep}: {len(layer)} live states, {dead} dead; bound max {us[-1]:.1f} median {us[len(us)//2]:.1f} min {us[0]:.1f}", flush=True)
    for s in sorted(layer.values(), key=lambda s: (-s._ub, -s.T))[:12]:
        print(f"  {float(s._ub):9.1f}  T={float(s.T):5.0f}  {fmt(s.moves)}")

def cmd_best(prefix, budget):
    s = start()
    for t, q in prefix: s = step(s, F(t), parse(q))
    s._ub = ub(s); cnt = itertools.count()
    heap = [(-s._ub, -s.T, -s.M, next(cnt), s)]; seen = {key(s)}; n = 0; deep = s
    while heap and n < budget:
        _, _, _, _, s = heapq.heappop(heap); n += 1
        if s.M > deep.M: deep = s
        for t, Q in moves(s):
            c = step(s, t, Q); k = key(c)
            if k in seen or not alive(c): continue
            seen.add(k); c._ub = ub(c); heapq.heappush(heap, (-c._ub, -c.T, -c.M, next(cnt), c))
        if n % 10000 == 0:
            print(f"{n} expansions: frontier max bound {float(-heap[0][0]) if heap else 0:.1f}, deepest {deep.M} moves", flush=True)
    print('frontier empty: no strict tour starts with this prefix' if not heap else
          f'after {n} expansions: frontier max bound {float(-heap[0][0]):.1f}')

def cmd_check():
    n = bad = 0
    for f in [ROOT + '/data/tours/curriculum.txt'] + sorted(glob.glob(ROOT + '/data/tours/record_*.txt')):
        for line in open(f):
            mv = [(F(a), parse(b)) for a, b in re.findall(r'\(([\d/]+),\s*([a-h][1-8])\)', line)]
            if not mv: continue
            s = start(); bs = [ub(s)]; ok = True
            for t, q in mv:
                k = s.alt[s.pos]
                if t > 0 and s.alt[q] == k and q != opp(s.pos): ok = False; break
                s = step(s, t, q); bs.append(ub(s))
            if ok:
                n += 1; bad += any(b < s.T for b in bs)
    print(f"{n} strict tours, {bad} with a prefix bound below the final time")

if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'trace': cmd_trace(sys.argv[2])
    elif c == 'bfs': cmd_bfs(int(sys.argv[2]))
    elif c == 'best': cmd_best([(int(a), b) for a, b in (x.strip('()').split(',') for x in sys.argv[2].split())], int(sys.argv[3]))
    elif c == 'check': cmd_check()
