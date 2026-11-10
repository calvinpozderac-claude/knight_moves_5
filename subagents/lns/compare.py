import sys, re
from collections import Counter
for fn in sys.argv[1:]:
    line = [l for l in open(fn) if '(' in l][0]
    mv = [(int(a), b) for a, b in re.findall(r'\((\d+),\s*([a-h][1-8])\)', line)]
    T = 0; ds = None
    for i, (w, q) in enumerate(mv):
        if ds is None and w >= 96: ds = i; Tds = T
        T += w
    sits = [i for i, (w, q) in enumerate(mv) if w > 0]
    dsits = [i for i in sits if i >= ds]
    c = Counter(q for _, q in mv); c['a1'] += 1
    zw_d = sum(1 for i, (w, q) in enumerate(mv) if i >= ds and w == 0)
    print(f'{fn.split("/")[-1]:22s} T={T} moves={len(mv)} dance_start=move{ds+1} T_at_start={Tds} sits={len(sits)} dance_sits={len(dsits)} dance_zero_hops={zw_d} dance_minutes={T-Tds} visits_used={sum(v for k,v in c.items() if k!="h8")}')
