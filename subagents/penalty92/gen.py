# print the sitting blocks of a tour with move ranges, and opposites
import sys, fam
def opp(s): return 'abcdefgh'[7 - 'abcdefgh'.index(s[0])] + str(9 - int(s[1]))
def blocks_idx(path, thr=60):
    T, line = fam.read_tour(path); mv = fam.moves_of_line(line)
    pos = 'a1'; bl = []
    for i, (w, q) in enumerate(mv):
        if w >= thr:
            if bl and (pos in bl[-1][0] or len(bl[-1][0]) < 2): bl[-1][0].add(pos); bl[-1][2] = i
            else: bl.append([{pos}, i, i])
        pos = q
    return [(sorted(b), lo, hi) for b, lo, hi in bl]
if __name__ == '__main__':
    for b, lo, hi in blocks_idx(sys.argv[1]): print(','.join(b), lo, hi, ','.join(opp(x) for x in b))
