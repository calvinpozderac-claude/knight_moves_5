# Move-by-move analysis of a tour: visits, sits per point, opposite pairs, dance start.
import re, sys
ROWS=[[9,8,10,12,11,8,10,17],[7,9,11,9,10,12,14,12],[4,7,5,8,8,6,13,10],[4,10,7,9,6,8,7,9],[2,6,4,2,5,9,8,11],[0,3,1,4,2,7,10,7],[1,2,0,1,2,5,7,6],[0,2,4,3,5,6,2,4]]
ALT=[0]*64
for r in range(8):
    for x in range(8): ALT[x+8*(7-r)]=ROWS[r][x]
nm=lambda i: 'abcdefgh'[i%8]+str(i//8+1)
idx=lambda s: ord(s[0])-97+8*(int(s[1])-1)
def run(text, show=True):
    mv=[(int(a),idx(b)) for a,b in re.findall(r'\((\d+),\s*([a-h][1-8])\)',text)]
    alt=ALT[:]; vis=[0]*64; vis[0]=1; pos=0; T=0
    sits=[0]*64; zero=[0]*64; rows=[]
    for k,(w,q) in enumerate(mv):
        a=alt[pos]; O=63-pos
        grp=[i for i in range(64) if alt[i]==a and i!=O]; n=len(grp)
        d=w//n if w else 0
        if w:
            sits[pos]+=1
            rise = alt[O]!=a
            for i in grp: alt[i]-=d
            if rise: alt[O]+=d
        else: zero[pos]+=1
        T+=w; vis[q]+=1; pos=q
        # groups two apart around knight
        a=alt[pos]; nU=sum(1 for i in range(64) if alt[i]==a); nL=sum(1 for i in range(64) if alt[i]==a-2)
        rows.append((k+1,w,nm(q),T,a,nU,nL,alt[63]))
    return mv,rows,sits,zero,vis
if __name__=='__main__':
    f=sys.argv[1]; text=open(f).read().split('\n')[1]
    mv,rows,sits,zero,vis=run(text)
    for r in rows[:70]: print('move %3d wait %4d -> %s T=%5d alt=%3d upper=%2d lower=%2d h8=%d'%r)
