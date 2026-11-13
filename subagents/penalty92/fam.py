# helpers: sit sequence, block signature, edit distance
import sys, re
def moves_of_line(line):
    return [(int(a), b) for a, b in re.findall(r'\((\d+),\s*([a-h][1-8])\)', line)]
def read_tour(path):
    L = open(path).read().split('\n')
    line = [l for l in L if '(' in l][0].strip()
    return int(L[0]) if L[0].strip().isdigit() else None, line
def sits(mv, thr=60):
    pos = 'a1'; out = []
    for w, q in mv:
        if w >= thr: out.append(pos)
        pos = q
    return out
def blocks(out):
    bl = []
    for s in out:
        if bl and (s in bl[-1] or len(bl[-1]) < 2): bl[-1].add(s)
        else: bl.append({s})
    return ['/'.join(sorted(b)) for b in bl]
def sig_of(path):
    T, line = read_tour(path); return ' '.join(blocks(sits(moves_of_line(line))))
def seq_of(path):
    T, line = read_tour(path); return sits(moves_of_line(line))
def lev(a, b):
    n, m = len(a), len(b); d = list(range(m + 1))
    for i in range(1, n + 1):
        p = d[0]; d[0] = i
        for j in range(1, m + 1):
            t = d[j]; d[j] = min(d[j] + 1, d[j - 1] + 1, p + (a[i - 1] != b[j - 1])); p = t
    return d[m]
if __name__ == '__main__':
    ref = seq_of(sys.argv[1])
    for p in sys.argv[2:]:
        print(p, read_tour(p)[0], lev(ref, seq_of(p)), sig_of(p))
