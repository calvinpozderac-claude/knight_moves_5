"""Per-method stats over agents/meta/logs/*.csv (grid,score,nstates,tag)."""
import csv, glob, statistics as st, os
TOP = 265326530
def canon(g):
    out = []
    for s in range(8):
        t = ['?'] * 25
        for r in range(5):
            for c in range(5):
                rr, cc = r, c
                if s & 1: cc = 4 - cc
                if s & 2: rr = 4 - rr
                if s & 4: rr, cc = cc, rr
                t[rr * 5 + cc] = g[r * 5 + c]
        out.append(''.join(t))
    return min(out)
allb = {}
for f in sorted(glob.glob(os.path.join(os.path.dirname(__file__), 'logs/*.csv'))):
    rows = [r for r in csv.reader(open(f))]
    for tag in sorted(set(r[3] for r in rows)):
        sc = [int(r[1]) for r in rows if r[3] == tag]
        print(f"{os.path.basename(f):14s} {tag:9s} n={len(sc):4d} mean={st.mean(sc)/1e6:7.2f}M median={st.median(sc)/1e6:7.2f}M "
              f"max={max(sc):,} hits={sum(s >= TOP for s in sc)} ({100*sum(s >= TOP for s in sc)/len(sc):.1f}%) >=264M: {sum(s>=264e6 for s in sc)}")
    for r in rows: allb[canon(r[0])] = int(r[1])
print("top distinct boards over all my runs:")
for g, s in sorted(allb.items(), key=lambda x: -x[1])[:8]: print(f"  {s:,} {g}")
