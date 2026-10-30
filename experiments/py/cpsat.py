"""CP-SAT model of the swamp-knight tour (integer sinks).
usage: python3 cpsat.py T Xmin [time_limit] [lo hi] [dmax]
Finds a tour of exactly T jumps ending at h8 with alt(h8) >= Xmin (maximizing it)."""
import sys
from ortools.sat.python import cp_model
from core import ALT0, NEIGH, nm

T = int(sys.argv[1]); XMIN = int(sys.argv[2])
TL = float(sys.argv[3]) if len(sys.argv) > 3 else 300
LO = int(sys.argv[4]) if len(sys.argv) > 4 else -25
HI = int(sys.argv[5]) if len(sys.argv) > 5 else 40
DMAX = int(sys.argv[6]) if len(sys.argv) > 6 else 20
H8, A1 = 63, 0

m = cp_model.CpModel()
alt = [[m.NewIntVar(LO, HI, f'a{t}_{i}') for i in range(64)] for t in range(T + 1)]
for i in range(64):
    m.Add(alt[0][i] == ALT0[i])
x = [[m.NewBoolVar(f'x{t}_{i}') for i in range(64)] for t in range(T + 1)]
pos = [m.NewIntVar(0, 63, f'p{t}') for t in range(T + 1)]
for t in range(T + 1):
    m.AddExactlyOne(x[t])
    m.Add(pos[t] == sum(i * x[t][i] for i in range(64)))
m.Add(x[0][A1] == 1)
m.Add(x[T][H8] == 1)
for t in range(T):
    m.Add(x[t][H8] == 0)
for i in range(64):
    if i != H8:
        m.Add(sum(x[t][i] for t in range(T + 1)) <= 3)

d = [m.NewIntVar(0, DMAX, f'd{t}') for t in range(T)]
ka = [m.NewIntVar(LO, HI, f'ka{t}') for t in range(T)]
for t in range(T):
    m.AddElement(pos[t], alt[t], ka[t])
    eq = [m.NewBoolVar(f'eq{t}_{i}') for i in range(64)]
    for i in range(64):
        m.Add(alt[t][i] == ka[t]).OnlyEnforceIf(eq[i])
        m.Add(alt[t][i] != ka[t]).OnlyEnforceIf(eq[i].Not())
    for i in range(64):
        o = x[t][63 - i]            # i is opposite of knight
        g = m.NewBoolVar(f'g{t}_{i}')  # i sinks
        r = m.NewBoolVar(f'r{t}_{i}')  # i rises
        # g = eq and not o ; r = o and not eq
        m.AddBoolAnd([eq[i], o.Not()]).OnlyEnforceIf(g)
        m.AddBoolOr([eq[i].Not(), o]).OnlyEnforceIf(g.Not())
        m.AddBoolAnd([o, eq[i].Not()]).OnlyEnforceIf(r)
        m.AddBoolOr([o.Not(), eq[i]]).OnlyEnforceIf(r.Not())
        # alt[t+1][i] = alt[t][i] - d*g + d*r
        m.Add(alt[t + 1][i] == alt[t][i] - d[t]).OnlyEnforceIf([g, r.Not()])
        m.Add(alt[t + 1][i] == alt[t][i] + d[t]).OnlyEnforceIf([r, g.Not()])
        m.Add(alt[t + 1][i] == alt[t][i]).OnlyEnforceIf([g.Not(), r.Not()])
    # jump legality
    for i in range(64):
        if i == H8:
            continue
        outs = []
        for j, dz in NEIGH[i]:
            b = m.NewBoolVar(f'm{t}_{i}_{j}')
            outs.append((j, b))
            m.AddImplication(b, x[t + 1][j])
            allowed = sorted(dz)
            if len(allowed) == 1:
                m.Add(alt[t + 1][j] - alt[t + 1][i] == allowed[0]).OnlyEnforceIf(b)
            else:
                bb = m.NewBoolVar('')
                m.Add(alt[t + 1][j] - alt[t + 1][i] == allowed[0]).OnlyEnforceIf([b, bb])
                m.Add(alt[t + 1][j] - alt[t + 1][i] == allowed[1]).OnlyEnforceIf([b, bb.Not()])
        m.Add(sum(b for _, b in outs) == 1).OnlyEnforceIf(x[t][i])
        m.Add(sum(b for _, b in outs) == 0).OnlyEnforceIf(x[t][i].Not())

m.Add(alt[T][H8] >= XMIN)
m.Maximize(alt[T][H8] * 1000 - sum(d))

solver = cp_model.CpSolver()
solver.parameters.max_time_in_seconds = TL
solver.parameters.num_workers = 2
class CB(cp_model.CpSolverSolutionCallback):
    def __init__(s):
        super().__init__()
    def on_solution_callback(s):
        X = s.Value(alt[T][H8])
        moves = []
        for t in range(T):
            p = s.Value(pos[t])
            n = sum(1 for i in range(64) if s.Value(alt[t][i]) == s.Value(alt[t][p]) and not (i == 63 - p))
            moves.append((s.Value(d[t]) * n, nm(s.Value(pos[t + 1]))))
        print(f'SOLUTION X={X} t={s.WallTime():.1f}s:', ', '.join(f'({a}, {b})' for a, b in moves), flush=True)
st = solver.Solve(m, CB())
print('status', solver.StatusName(st), 'obj', solver.ObjectiveValue() if st in (cp_model.OPTIMAL, cp_model.FEASIBLE) else None)
