import sys, itertools
from st import *

def goto(s, tgt):
    path = static_path(s, ix(tgt))
    if not path: return None
    return walk(s, path)

def sit(s, k):
    a2, n, t = apply(s.p, k, s.alt)
    outs = []
    for q, dz in NEIGH[s.p]:
        if q == H8 or s.vis[q] >= 3: continue
        if (a2[q]-a2[s.p]) in dz:
            outs.append(s.step(k, q))
    return outs

def build(s, plan):
    if not plan: return [s]
    sq, k = plan[0]
    s1 = goto(s, sq)
    if s1 is None: return []
    res = []
    for s2 in sit(s1, k):
        res += build(s2, plan[1:])
    return res

def best_phaseA(plan, target):
    fin = []
    for s in build(State(), plan):
        p = static_path(s, ix(target))
        if p:
            s3 = walk(s, p)
            fin.append((len(s3.moves), s3))
    fin.sort(key=lambda x: x[0])
    return fin[0][1] if fin else None

if __name__ == '__main__':
    plans = {
      'g6a': ([('b3', 4), ('c3', 3), ('b4', 8)], 'g6'),
      'g6b': ([('b3', 3), ('b4', 8), ('b3', 1)], 'g6'),
      'g6c': ([('b3', 4), ('a2', 3), ('b4', 8)], 'g6'),
      'g6d': ([('b3', 4), ('b4', 5), ('b4', 3)], 'g6'),
      'g6e': ([('b3', 4), ('d2', 3), ('b4', 8)], 'g6'),
      'g4':  ([('d2', 3), ('b3', 3), ('b5', 9)], 'g4'),
    }
    for name, (plan, tgt) in plans.items():
        s = best_phaseA(plan, tgt)
        if s is None:
            print(name, 'FAILED'); continue
        print(name, len(s.moves), s.T, fmt(s.moves))
