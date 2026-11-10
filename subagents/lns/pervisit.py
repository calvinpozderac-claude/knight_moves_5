import sys, re
from collections import Counter
def load(fn):
    line = [l for l in open(fn) if '(' in l][0]
    mv = [(int(a), b) for a, b in re.findall(r'\((\d+),\s*([a-h][1-8])\)', line)]
    vis = Counter(q for _, q in mv); vis['a1'] += 1
    sits = Counter(); pos = 'a1'
    for w, q in mv:
        if w > 0: sits[pos] += 1
        pos = q
    return vis, sits
A = load(sys.argv[1]); B = load(sys.argv[2])
sq = [c + r for r in '12345678' for c in 'abcdefgh']
print('square: visits A->B, sits A->B (only changed)')
for s in sq:
    if A[0][s] != B[0][s] or A[1][s] != B[1][s]:
        print(f'{s}: vis {A[0][s]}->{B[0][s]} sits {A[1][s]}->{B[1][s]}')
print('total sits', sum(A[1].values()), '->', sum(B[1].values()), '; total landings', sum(A[0].values()), '->', sum(B[0].values()))
