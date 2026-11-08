import sys,re,difflib
def ld(f):
    l=[x for x in open(f) if '(' in x][0]
    return [(int(a),b) for a,b in re.findall(r'\((\d+),\s*([a-h][1-8])\)',l)]
A=ld(sys.argv[1]);B=ld(sys.argv[2])
sm=difflib.SequenceMatcher(None,A,B,autojunk=False)
for op,i1,i2,j1,j2 in sm.get_opcodes():
    if op!='equal':
        print(f"{op} A[{i1+1}-{i2}] -> B[{j1+1}-{j2}]: {' '.join(f'{w}{q}' for w,q in A[i1:i2])}  =>  {' '.join(f'{w}{q}' for w,q in B[j1:j2])}")
