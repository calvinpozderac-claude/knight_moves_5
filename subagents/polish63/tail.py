import sys,re
# usage: tail.py N file... : print last N (wait,square) moves of each tour and pairwise common tail length
def load(f):
    l=open(f).read().split('\n'); return int(l[0]), re.findall(r'\((\d+), ([a-h]\d)\)', l[1])
N=int(sys.argv[1]); T=[(f,)+load(f) for f in sys.argv[2:]]
for f,m,mv in T:
    print(f"{m} n={len(mv)} {f}\n  sq:  "+' '.join(s for w,s in mv[-N:])+"\n  w:   "+' '.join(w for w,s in mv[-N:]))
for i in range(len(T)):
    for j in range(i+1,len(T)):
        a=[s for w,s in T[i][2]]; b=[s for w,s in T[j][2]]; k=0
        while k<min(len(a),len(b)) and a[-1-k]==b[-1-k]: k+=1
        A=T[i][2]; B=T[j][2]; kw=0
        while kw<min(len(A),len(B)) and A[-1-kw]==B[-1-kw]: kw+=1
        print(f"common tail {T[i][1]} vs {T[j][1]}: squares {k}, moves(with waits) {kw}")
