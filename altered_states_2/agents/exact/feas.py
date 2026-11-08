#!/usr/bin/env python3
"""feas.py [--limit SEC] [--solver NAME] CODES...
Is there a 5x5 board containing ALL listed states (each with <=1 alteration)? SAT/UNSAT/UNKNOWN."""
import sys, time, threading
from pysat.solvers import Solver
from satmodel import Enc

def feasible(codes, limit=60, sname='cd19'):
    e = Enc()
    for c in codes:
        e.add_state(c)
    t = time.time()
    with Solver(name=sname, bootstrap_with=e.cls) as s:
        timer = threading.Timer(limit, lambda: s.interrupt())
        timer.start()
        r = s.solve_limited(assumptions=[e.y[c] for c in codes], expect_interrupt=True)
        timer.cancel()
        g = e.grid(s.get_model()) if r else ''
    return {True: 'SAT', False: 'UNSAT', None: 'UNKNOWN'}[r], g, time.time() - t

if __name__ == '__main__':
    a = sys.argv[1:]
    limit, sname = 60, 'cd19'
    if '--limit' in a:
        i = a.index('--limit'); limit = float(a[i + 1]); del a[i:i + 2]
    if '--solver' in a:
        i = a.index('--solver'); sname = a[i + 1]; del a[i:i + 2]
    r, g, t = feasible(a, limit, sname)
    print(r, g, f'{t:.1f}s', ' '.join(a), flush=True)
