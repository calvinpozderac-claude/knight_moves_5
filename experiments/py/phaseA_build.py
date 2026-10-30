from st import *
from raises3 import reach, NB
import itertools

def exits(s):
    return [q for q, dz in NEIGH[s.p] if q != H8 and s.vis[q] < 3 and (s.alt[q]-s.alt[s.p]) in dz]

def goto(s, tgt):
    path = static_path(s, ix(tgt))
    assert path, ('no path to', tgt)
    return walk(s, path)

def sit(s, k):
    """sink knight's group by k, then take any exit; return list of states (one per exit)"""
    a2, n, t = apply(s.p, k, s.alt)
    outs = []
    for q, dz in NEIGH[s.p]:
        if q == H8 or s.vis[q] >= 3: continue
        if (a2[q]-a2[s.p]) in dz:
            outs.append(s.step(k, q))
    return outs

plan = [('d1', 4), ('e1', 5), ('d2', 3)]
def build(s, plan):
    if not plan:
        return [s]
    sq, k = plan[0]
    try:
        s1 = goto(s, sq)
    except AssertionError:
        return []
    res = []
    for s2 in sit(s1, k):
        res += build(s2, plan[1:])
    return res

s0 = State()
fin = []
for perm in itertools.permutations(plan):
    for s in build(s0, list(perm)):
        p = static_path(s, ix('d8'))
        if p:
            s3 = walk(s, p)
            fin.append((len(s3.moves), s3))
fin.sort(key=lambda x: x[0])
for L, s in fin[:5]:
    print(L, s.T, fmt(s.moves))
s = fin[0][1]
s.show()
