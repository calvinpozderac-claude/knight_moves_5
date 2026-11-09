import sys, re
sys.path.insert(0, '/home/user/knight_moves_5/tools')
from sim import MAIN_ROWS
line = [l for l in open(sys.argv[1]) if '(' in l][0]
mv = [(int(a), b) for a, b in re.findall(r'\((\d+),\s*([a-h][1-8])\)', line)]
alt = {}
for i, row in enumerate(MAIN_ROWS):
    for x, v in enumerate(row): alt[(x, 7 - i)] = v
P = lambda s: (ord(s[0]) - 97, int(s[1]) - 1)
N = lambda p: chr(97 + p[0]) + str(p[1] + 1)
pos = (0, 0); vis = {p: 0 for p in alt}; vis[pos] = 1; T = 0
start = int(sys.argv[2]) if len(sys.argv) > 2 else 0
for i, (w, q) in enumerate(mv):
    A = alt[pos]; O = (7 - pos[0], 7 - pos[1])
    G = [p for p in alt if alt[p] == A and p != O]
    n = len(G)
    if w:
        d = w // n
        for p in G: alt[p] -= d
        if alt[O] != A: alt[O] += d
    T += w
    q = P(q); vis[q] += 1
    if i + 1 >= start:
        # level histogram near knight
        lv = {}
        for p in alt: lv[alt[p]] = lv.get(alt[p], 0) + 1
        top = sorted(lv.items(), key=lambda x: -x[1])[:3]
        print(f'{i+1:3d} {N(pos)}->{N(q)} w={w:3d} n={n:2d} alt={alt[q]:4d} vis={vis[q]} T={T} groups={top} h8={alt[(7,7)]}')
    pos = q
# final visits left in big groups
lv = {}
for p in alt: lv.setdefault(alt[p], []).append(p)
for a, ps in sorted(lv.items(), key=lambda x: -len(x[1]))[:3]:
    print(a, ' '.join(f'{N(p)}:{vis[p]}' for p in sorted(ps)))
