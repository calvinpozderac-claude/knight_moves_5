import re,statistics as st,sys
rows=[l.split() for l in open(sys.argv[1]) if 'complete' in l and 'truncated' not in l]
d={}
for r in rows:
    arm=r[0][4:]; A=r[1][2:]; m=int(r[r.index('minutes')+1]); s=float(r[r.index('secs')+1])
    d.setdefault((arm,A),[]).append((m,s))
    d.setdefault((arm,'all'),[]).append((m,s))
for k in sorted(d):
    v=sorted(x for x,_ in d[k]); n=len(v)
    q=lambda p: v[min(n-1,int(p*(n-1)+0.5))]
    print(f"{k[0]:9s} A={k[1]:3s} n={n:2d} median={st.median(v):6.0f} IQR={q(.25)}-{q(.75)} min={v[0]} max={v[-1]} secs/run={sum(s for _,s in d[k])/n:.0f}")
# paired wins
pa={}
for r in rows:
    pa.setdefault((r[1],r[2]),{})[r[0][4:]]=int(r[r.index('minutes')+1])
w=sum(1 for v in pa.values() if len(v)==2 and v['bsink4_3']>v['none']); n=sum(1 for v in pa.values() if len(v)==2)
print(f"paired: bsink4_3 beats none in {w}/{n} matched (anchor, rng) pairs")
