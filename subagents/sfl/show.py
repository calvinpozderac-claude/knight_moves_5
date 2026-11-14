#!/usr/bin/env python3
# usage: python3 show.py TOURFILE [FROM]  -- moves with group size n, sink units, in-group hops (*) and free sinks (F)
import sys, re
sys.path.insert(0, '../../tools')
from sim import *
L = open(sys.argv[1]).read().split('\n'); t = L[1] if '(' in L[1] else L[0]
mv = [(int(a), b) for a, b in re.findall(r'\((\d+), ([a-h][1-8])\)', t)]
fr = int(sys.argv[2]) if len(sys.argv) > 2 else 1
alt = init_alt(MAIN_ROWS); pos = parse('a1'); T = 0
for i, (w, q) in enumerate(mv):
    A = alt[pos]; qq = parse(q); ing = alt[qq] == A and qq != (7 - pos[0], 7 - pos[1])
    n = apply_wait(pos, w, alt, 8); T += w
    tag = ('F' if w else '*') if ing else ''
    if i + 1 >= fr: print(f'{i+1:3d} {name(pos)}->{q} w{w:4d} n{n:3d} u{F(w, n) if w else 0} T{T} {tag}')
    pos = qq
