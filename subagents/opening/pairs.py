import sys
from analyse import run, nm
text=open(sys.argv[1]).read().split('\n')[1]; D=int(sys.argv[2])  # D = dance start move index (moves done)
mv,rows,sits,zero,vis=run(text)
# visits before dance start
import re
v0=[0]*64; v0[0]=1
for k,(w,q) in enumerate(mv[:D]): v0[q]+=1
sitsD=[0]*64; zeroD=[0]*64; pos=mv[D-1][1]
for k,(w,q) in enumerate(mv[D:]):
    if w: sitsD[pos]+=1
    else: zeroD[pos]+=1
    pos=q
print('moves',len(mv),'T',rows[-1][3],'dance start after move',D,'T',rows[D-1][3])
print('total sits in dance',sum(sitsD),'zero hops in dance',sum(zeroD))
tot_loss=0
for p in range(32):
    o=63-p
    a,b=p,o
    print('%s(v0=%d sit=%d z=%d vis=%d)  %s(v0=%d sit=%d z=%d vis=%d)'%(nm(a),v0[a],sitsD[a],zeroD[a],vis[a],nm(b),v0[b],sitsD[b],zeroD[b],vis[b]))
