import time, sys
from model import *
best="hmlfsoaioutsnrcregahoywin"
for free in [[], [0,1,2,5,6,7,10,11,12], [6,7,8,11,12,13,16,17,18]]:
    t=time.time()
    m,x,y=build(fixed=best, free=set(free), hint=best)
    s=cp_model.CpSolver(); s.parameters.num_workers=1; s.parameters.max_time_in_seconds=120
    st=s.Solve(m)
    print(free, s.StatusName(st), s.ObjectiveValue(), s.BestObjectiveBound(), solution_grid(s,x), round(time.time()-t,1), flush=True)
