"""Plan the dance (the long sinking phase) exactly with CP-SAT, from the state a seed tour reaches.

In the dance the board is two groups two apart. Each step the knight sits in the upper group (it sinks
by 4; the point opposite the knight leaves the dance) and jumps onto an adjacent point of the other group,
which is now the upper one; between steps it may make zero-wait knight moves inside the upper group.
Each point takes at most 3 visits. The model chooses the whole sequence of steps (with the finish into
h8) to maximise the minutes, and prints the resulting tour, which is then checked by replaying it.

usage: python3 tools/dance_cpsat.py TOURFILE K [--steps 90] [--time 600] [--workers 2] [--repo 2]
                                    [--hint] [--out FILE]
  K: keep the first K moves of the tour (the knight must then be in the upper of two groups 2 apart)
  --repo R: allow up to R zero-wait knight moves between two steps (0, 1 or 2)
  --hint: give the solver the seed tour's own dance as a starting solution (when it fits the model)
"""
import argparse, re, sys, time
from ortools.sat.python import cp_model

ROWS = [[9, 8, 10, 12, 11, 8, 10, 17], [7, 9, 11, 9, 10, 12, 14, 12], [4, 7, 5, 8, 8, 6, 13, 10],
        [4, 10, 7, 9, 6, 8, 7, 9], [2, 6, 4, 2, 5, 9, 8, 11], [0, 3, 1, 4, 2, 7, 10, 7],
        [1, 2, 0, 1, 2, 5, 7, 6], [0, 2, 4, 3, 5, 6, 2, 4]]
H8, A1 = 63, 0
def nm(i): return 'abcdefgh'[i % 8] + str(i // 8 + 1)
def sq(s): return (ord(s[0]) - 97) + 8 * (int(s[1]) - 1)
def xy(i): return i % 8, i // 8
def adj(i):  # orthogonal neighbours (jump type dz +-2)
    x, y = xy(i); return [X + 8 * Y for X, Y in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)) if 0 <= X < 8 and 0 <= Y < 8]
def two(i):  # two away in a line (dz +-1)
    x, y = xy(i); return [X + 8 * Y for X, Y in ((x + 2, y), (x - 2, y), (x, y + 2), (x, y - 2)) if 0 <= X < 8 and 0 <= Y < 8]
def kn(i):  # knight moves (dz 0)
    x, y = xy(i); out = []
    for dx, dy in ((1, 2), (2, 1), (-1, 2), (-2, 1), (1, -2), (2, -1), (-1, -2), (-2, -1)):
        X, Y = x + dx, y + dy
        if 0 <= X < 8 and 0 <= Y < 8: out.append(X + 8 * Y)
    return out

def replay(moves):
    """moves: list of (wait, square). Returns (alt, vis, pos, T) after them (integer engine, like eng.h)."""
    alt = [0] * 64
    for r in range(8):
        for x in range(8): alt[x + 8 * (7 - r)] = ROWS[r][x]
    pos, vis, T = 0, [0] * 64, 0; vis[0] = 1
    for w, q in moves:
        a, O = alt[pos], 63 - pos
        grp = [j for j in range(64) if alt[j] == a and j != O]
        if w:
            d = w // len(grp)
            for j in grp: alt[j] -= d
            if alt[O] != a: alt[O] += d
        vis[q] += 1; T += w; pos = q
    return alt, vis, pos, T

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('tour'); ap.add_argument('K', type=int)
    ap.add_argument('--steps', type=int, default=90); ap.add_argument('--time', type=float, default=600)
    ap.add_argument('--workers', type=int, default=2); ap.add_argument('--repo', type=int, default=2)
    ap.add_argument('--hint', action='store_true'); ap.add_argument('--out', default='')
    a = ap.parse_args()
    line = [l for l in open(a.tour).read().split('\n') if '(' in l][0]
    tour = [(int(w), sq(s)) for w, s in re.findall(r'\((\d+), ([a-h][1-8])\)', line)]
    prefix = tour[:a.K]
    alt, vis, pos, T0 = replay(prefix)
    top = alt[pos]
    G = [set(i for i in range(64) if alt[i] == top), set(i for i in range(64) if alt[i] == top - 2)]
    print(f'start after {a.K} moves: knight {nm(pos)} T {T0}; upper group {len(G[0])}, lower group {len(G[1])}, '
          f'out {64 - len(G[0]) - len(G[1])}; h8 in {"upper" if H8 in G[0] else "lower" if H8 in G[1] else "neither"}')
    if H8 not in G[0] and H8 not in G[1]: sys.exit('h8 is not in the dance')
    inD = G[0] | G[1]
    NT = a.steps
    grp = lambda t: G[t % 2]
    m = cp_model.CpModel()
    sit = [{s: m.NewBoolVar(f's{t}_{s}') for s in grp(t) if s not in (H8, A1)} for t in range(NT)]
    land = [{q: m.NewBoolVar(f'l{t}_{q}') for q in grp(t + 1) if q != H8} for t in range(NT)]
    active = [m.NewBoolVar(f'a{t}') for t in range(NT)]
    kicked = [{u: m.NewBoolVar(f'k{t}_{u}') for u in inD} for t in range(NT + 1)]
    m.Add(active[0] == 1)
    for t in range(NT - 1): m.Add(active[t + 1] <= active[t])
    for u in inD: m.Add(kicked[0][u] == 0)
    # kicks: the point opposite a sitting point leaves the dance
    for t in range(NT):
        for u in inD:
            m.Add(kicked[t + 1][u] >= kicked[t][u])
            p = 63 - u
            if p in sit[t]: m.Add(kicked[t + 1][u] >= sit[t][p])
    # one sit per active step, on a point still in the dance
    for t in range(NT):
        m.Add(sum(sit[t].values()) == active[t])
        for s, v in sit[t].items(): m.Add(v + kicked[t][s] <= 1)
    m.Add(sit[0][pos] == 1)
    # finishing: step t is last if active[t] and not active[t+1]
    fin = [m.NewBoolVar(f'f{t}') for t in range(NT)]
    for t in range(NT):
        nxt = active[t + 1] if t + 1 < NT else 0
        m.Add(fin[t] == active[t] - nxt) if t + 1 < NT else m.Add(fin[t] == active[t])
    # finish types: 4 = jump onto h8 from an orthogonal neighbour, 3 = from two away, 2 = knight move after
    # sinking 2 (merging the groups); all need h8 in the lower group
    fin4, fin3, fin2, fin0 = [], [], [], []
    for t in range(NT):
        ok = H8 in grp(t + 1)
        f4 = [sit[t][s] for s in adj(H8) if s in sit[t]] if ok else []
        f3 = [sit[t][s] for s in two(H8) if s in sit[t]] if ok else []
        f2 = [sit[t][s] for s in kn(H8) if s in sit[t]] if ok else []
        f0 = [sit[t][s] for s in kn(H8) if s in sit[t]] if H8 in grp(t) else []  # same level: hop in, no wait
        b4, b3, b2, b0 = m.NewBoolVar(''), m.NewBoolVar(''), m.NewBoolVar(''), m.NewBoolVar('')
        for b, f in ((b4, f4), (b3, f3), (b2, f2), (b0, f0)):
            m.Add(b <= sum(f)) if f else m.Add(b == 0)
        m.Add(b4 + b3 + b2 + b0 == fin[t])
        fin4.append(b4); fin3.append(b3); fin2.append(b2); fin0.append(b0)
    # landing after a non-final step: an orthogonal neighbour in the other group, still in the dance after the sit
    for t in range(NT):
        nxt = active[t + 1] if t + 1 < NT else 0
        m.Add(sum(land[t].values()) == nxt)
        for q, v in land[t].items():
            src = [sit[t][s] for s in adj(q) if s in sit[t]]
            m.Add(v <= sum(src)) if src else m.Add(v == 0)
            m.Add(v + kicked[t + 1][q] <= 1)
    # next sitting point: the landing point, or up to R zero-wait knight moves inside the (new) upper group
    repo = [[{} for _ in range(a.repo)] for t in range(NT)]
    for t in range(NT - 1):
        g = grp(t + 1)
        prev = land[t]
        for k in range(a.repo):
            step = {}
            for r in g:
                if r == H8: continue
                src = [prev[q] for q in kn(r) if q in prev]
                if not src: continue
                v = m.NewBoolVar(''); m.Add(v <= sum(src)); m.Add(v + kicked[t + 1][r] <= 1)
                step[r] = v
            m.Add(sum(step.values()) <= (sum(repo[t][k - 1].values()) if k else active[t + 1]))
            repo[t][k] = step
            prev = step
        # where the knight stops: after the landing (no knight move) or after the last knight move made
        stops = {}
        levels = [land[t]] + repo[t]
        for k, lev in enumerate(levels):
            deeper = sum(levels[k + 1].values()) if k + 1 < len(levels) else 0
            for r, v in lev.items():
                st = m.NewBoolVar('')
                m.Add(st <= v); m.Add(st >= v - deeper)
                if k + 1 < len(levels): m.Add(st <= 1 - deeper)
                stops.setdefault(r, []).append(st)
        for r, sts in stops.items():
            if r in sit[t + 1]: m.Add(sit[t + 1][r] == sum(sts))
            else: m.Add(sum(sts) == 0)  # cannot sit there (a1)
        for r in sit[t + 1]:
            if r not in stops: m.Add(sit[t + 1][r] == 0)
    # visits
    for s in inD:
        if s == H8: continue
        uses = [land[t][s] for t in range(NT) if s in land[t]] + [repo[t][k][s] for t in range(NT) for k in range(a.repo) if s in repo[t][k]]
        if uses: m.Add(vis[s] + sum(uses) <= 3)
    # minutes: 4 x (upper group size), the point opposite the sitter excluded if it is in the same group
    mins = []
    for t in range(NT):
        g = grp(t)
        alive_cnt = sum(1 - kicked[t][u] for u in g)
        excl = []
        for s in sit[t]:
            p = 63 - s
            if p in g:
                e = m.NewBoolVar(''); m.Add(e >= sit[t][s] - kicked[t][p]); excl.append(e)
        cnt = alive_cnt - sum(excl)
        mt = m.NewIntVar(0, 256, f'm{t}')
        m.Add(mt <= 4 * cnt); m.Add(mt <= 256 * active[t])
        m.Add(mt <= 3 * cnt + 256 * (1 - fin3[t])); m.Add(mt <= 2 * cnt + 256 * (1 - fin2[t]))
        m.Add(mt <= 256 * (1 - fin0[t]))
        mins.append(mt)
    m.Maximize(sum(mins))

    class CB(cp_model.CpSolverSolutionCallback):
        def __init__(s): super().__init__(); s.t0 = time.time()
        def on_solution_callback(s):
            print(f'  {time.time() - s.t0:6.0f}s  solution {T0 + int(s.ObjectiveValue())}  bound {T0 + int(s.BestObjectiveBound())}', flush=True)
    sol = cp_model.CpSolver(); sol.parameters.max_time_in_seconds = a.time; sol.parameters.num_workers = a.workers
    st = sol.Solve(m, CB())
    print('status', sol.StatusName(st), 'objective', T0 + sol.ObjectiveValue() if st in (cp_model.OPTIMAL, cp_model.FEASIBLE) else None,
          'bound', T0 + sol.BestObjectiveBound())
    if st not in (cp_model.OPTIMAL, cp_model.FEASIBLE): return
    # decode into moves
    moves = list(prefix)
    for t in range(NT):
        if not sol.Value(active[t]): break
        s = [s for s, v in sit[t].items() if sol.Value(v)][0]
        if sol.Value(fin[t]):
            moves.append(('FIN4' if sol.Value(fin4[t]) else 'FIN3' if sol.Value(fin3[t]) else 'FIN2' if sol.Value(fin2[t]) else 'FIN0', s)); break
        q = [q for q, v in land[t].items() if sol.Value(v)][0]
        moves.append(('SIT', q))
        for k in range(a.repo):
            r = [r for r, v in repo[t][k].items() if sol.Value(v)]
            if r: moves.append((0, r[0]))
    # turn symbolic moves into waits by replaying with the integer engine
    out = list(prefix)
    for mv in moves[len(prefix):]:
        alt, vis2, p, T = replay(out)
        a0, O = alt[p], 63 - p; n = sum(1 for j in range(64) if alt[j] == a0 and j != O)
        kind, q = mv
        if kind == 'SIT': out.append((4 * n, q))
        elif kind == 0: out.append((0, q))
        elif kind == 'FIN4': out.append((4 * n, H8))
        elif kind == 'FIN3': out.append((3 * n, H8))
        elif kind == 'FIN2': out.append((2 * n, H8))
        elif kind == 'FIN0': out.append((0, H8))
    txt = ', '.join(f'({w}, {nm(q)})' for w, q in out)
    _, _, _, T = replay(out)
    print('tour minutes', T, 'jumps', len(out))
    if a.out:
        open(a.out, 'w').write(f'{T}\n{txt}\n')
    print(txt)

if __name__ == '__main__':
    main()
