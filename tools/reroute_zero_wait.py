"""Re-route the zero-wait stretches of a tour.

Every sitting move is kept (so the altitudes and the minutes are unchanged), but consecutive sitting moves
are joined by the cheapest zero-wait path: landing on a point that sits somewhere in the tour costs 1, any
other landing 0.001. This frees visits for a later search of the dance to use (on the 7,911 tour it raised
the bound on dance sits from 79 to 82).

usage: python3 tools/reroute_zero_wait.py TOURFILE [UPTO] [OUT]   (UPTO: only stretches ending before move UPTO)
"""
import sys, re, heapq
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dance_cpsat import replay, nm, sq, adj, two, kn, H8, ROWS
line = [l for l in open(sys.argv[1]).read().split('\n') if '(' in l][0]
upto = int(sys.argv[2]) if len(sys.argv) > 2 else 10**9   # only re-route stretches that end before this move
tour = [(int(w), sq(s)) for w, s in re.findall(r'\((\d+), ([a-h][1-8])\)', line)]
def sitters_of(t, start):
    out = set(); pos = 0
    for k, (w, q) in enumerate(t):
        if w > 0 and k >= start: out.add(pos)
        pos = q
    return out
S = sitters_of(tour, 0)
def zw_moves(alt, p):
    for q in kn(p):
        if alt[q] == alt[p]: yield q
    for q in adj(p):
        if abs(alt[q] - alt[p]) == 2: yield q
    for q in two(p):
        if abs(alt[q] - alt[p]) == 1: yield q
def total_vis(t):
    v = [0] * 64; v[0] = 1
    for w, q in t: v[q] += 1
    return v
changed = True; it = 0
while changed and it < 20:
    changed = False; it += 1
    # find maximal zero-wait runs: indices i..j (moves with w == 0, not into h8), followed by a sitting move
    k = 0
    while k < len(tour):
        if tour[k][0] != 0 or tour[k][1] == H8: k += 1; continue
        i = k
        while k < len(tour) and tour[k][0] == 0 and tour[k][1] != H8: k += 1
        j = k - 1  # run i..j; tour[j][1] is where the next sit happens
        if j >= upto: break
        alt, vis, pos, T = replay(tour[:i])
        start, end = pos, tour[j][1]
        vis_all = total_vis(tour)
        for m in range(i, j + 1): vis_all[tour[m][1]] -= 1   # free the run's own landings
        old_cost = sum((1 if tour[m][1] in S else 0.001) for m in range(i, j + 1))
        # Dijkstra over (square, path length) with per-square caps from the rest of the tour
        best = {start: (0, [])}; pq = [(0, start, [])]; found = None
        while pq:
            c, p, path = heapq.heappop(pq)
            if p == end and path: found = (c, path); break
            if len(path) > 20: continue
            for q in zw_moves(alt, p):
                if q == H8: continue
                used = path.count(q)
                if vis_all[q] + used + 1 > 3: continue
                nc = c + (1 if q in S else 0.001)
                key = (q, len(path) + 1)
                if q in best and best[q][0] <= nc - 1e-9 and q != end: continue
                if q not in best or nc < best[q][0]: best[q] = (nc, path + [q])
                heapq.heappush(pq, (nc, q, path + [q]))
        if found and found[0] < old_cost - 1e-6:
            new = tour[:i] + [(0, q) for q in found[1]] + tour[j + 1:]
            # verify: replay must keep all waits identical
            ok = True
            try:
                alt2, vis2, pos2, T2 = replay(new)
                ok = T2 == replay(tour)[3] and max(total_vis(new)[:63]) <= 3
            except Exception: ok = False
            if ok:
                print(f'run {i+1}-{j+1}: {nm(start)}->{nm(end)} {" ".join(nm(tour[m][1]) for m in range(i, j+1))} (cost {old_cost:.3f}) => {" ".join(nm(q) for q in found[1])} (cost {found[0]:.3f})')
                tour = new; changed = True; break
print('sitter landings in zero-wait moves:', sum(1 for w, q in tour if w == 0 and q in S and q != H8))
txt = ', '.join(f'({w}, {nm(q)})' for w, q in tour)
T = replay(tour)[3]
if len(sys.argv) > 3: open(sys.argv[3], 'w').write(f'{T}\n{txt}\n')
print('minutes', T, 'jumps', len(tour))
