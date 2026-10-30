from core import *
def comps(alt, exclude=(H8,)):
    seen = {}
    cid = 0
    out = []
    for s in range(64):
        if s in seen or s in exclude: continue
        stack=[s]; seen[s]=cid; members=[s]
        while stack:
            u=stack.pop()
            for q,dz in NEIGH[u]:
                if q in seen or q in exclude: continue
                if alt[q]-alt[u] in dz:
                    seen[q]=cid; stack.append(q); members.append(q)
        out.append(members); cid+=1
    return out
def show(alt):
    for y in range(7,-1,-1):
        print(y+1, ' '.join(f'{alt[x+8*y]:4d}' for x in range(8)))
    print('   ' + ' '.join(f'{c:>4s}' for c in 'abcdefgh'))
if __name__=='__main__':
    show(ALT0)
    for c in sorted(comps(ALT0), key=len, reverse=True):
        print(len(c), sorted(nm(i)+':'+str(ALT0[i]) for i in c))
