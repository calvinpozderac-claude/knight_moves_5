import sys, re
def moves(line):
    return [(int(a), b) for a, b in re.findall(r'\((\d+),\s*([a-h][1-8])\)', line)]
def read(path):
    L=[l for l in open(path) if '(' in l]; return L[0].strip()
def dance_sig(mv, thr=60):
    # squares on which knight sits >= thr (sit happens at square before move)
    pos='a1'; out=[]
    for w,q in mv:
        if w>=thr: out.append(pos)
        pos=q
    # compress into blocks: unordered set of consecutive alternating pair
    blocks=[]
    for s in out:
        if blocks and (s in blocks[-1] or len(blocks[-1])<2): blocks[-1].add(s) if s in blocks[-1] or len(blocks[-1])<2 else None
        else: blocks.append({s})
    return out, ['/'.join(sorted(b)) for b in blocks]
def dance_start(mv, thr=60):
    for i,(w,q) in enumerate(mv):
        if w>=thr and i>40: return i
if __name__=='__main__':
    for p in sys.argv[1:]:
        mv=moves(read(p)); out,bl=dance_sig(mv)
        print(p, len(mv), 'start', dance_start(mv), 'nsits', len(out))
        print('  ', ' '.join(bl))
