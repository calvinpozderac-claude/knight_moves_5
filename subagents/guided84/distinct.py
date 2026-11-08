# usage: python3 distinct.py MIN POOL...   distinct tours >= MIN (minutes<TAB>tour or 2-line), count per minutes value
import sys, re
mn = int(sys.argv[1]); seen = {}
for f in sys.argv[2:]:
    L = open(f).read().split('\n'); m = None
    for l in L:
        if '(' in l:
            if '\t' in l: a, t = l.split('\t', 1); m2 = int(float(a))
            else: t = l; m2 = m
            if m2 is not None and m2 >= mn: seen.setdefault(t.strip(), m2)
        else:
            try: m = int(float(l))
            except: m = None
from collections import Counter
c = Counter(seen.values()); print('distinct', len(seen), dict(sorted(c.items(), reverse=True)))
