import sys
from analyse import run, nm, ALT
import re
text=open(sys.argv[1]).read().split('\n')[1]; D=int(sys.argv[2])
mv=[(int(a),b) for a,b in re.findall(r'\((\d+),\s*([a-h][1-8])\)',text)]
from analyse import idx
alt=ALT[:]; vis=[0]*64; vis[0]=1; pos=0
for w,q in mv[:D]:
    q=idx(q); a=alt[pos]; O=63-pos; grp=[i for i in range(64) if alt[i]==a and i!=O]
    if w:
        d=w//len(grp)
        rise=alt[O]!=a
        for i in grp: alt[i]-=d
        if rise: alt[O]+=d
    vis[q]+=1; pos=q
a=alt[pos]
print('knight',nm(pos),'alt',a)
for y in range(7,-1,-1):
    print(' '.join('%4s'%( ('U' if alt[y*8+x]==a else 'L' if alt[y*8+x]==a-2 else str(alt[y*8+x]-a)) + str(vis[y*8+x]) ) for x in range(8)))
