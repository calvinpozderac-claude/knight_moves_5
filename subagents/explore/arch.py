#!/usr/bin/env python3
# arch.py: MAP-Elites-style family archive. Cells = (dance level pair, first move, sitter cluster); a sitter cluster is a
# representative sitter set, a tour joins it if Jaccard >= 0.8 (else it opens a new cluster). Sources: arch/all_strict.tsv
# (repo pools, strict, >= 8000) + every tour in runs/ (bests, elites, NRPA pools). Writes arch/families.tsv (best per cell,
# with Jaccard to the 9,330 record's sitters) and arch/cells/<cell>.txt (2-line tour files). Strict tours only.
import glob, os, sys
from desc import desc, jac
REF = set(open('arch/ref_sit.txt').read().strip().split(','))
cache_f = 'arch/desc_cache.tsv'; cache = {}
if os.path.exists(cache_f):
    for l in open(cache_f):
        p = l.rstrip('\n').split('\t')
        if len(p) == 7: cache[p[6]] = p[:6]
rows = []
for l in open('arch/all_strict.tsv'):
    r = l.rstrip('\n').split('\t'); rows.append((int(r[0]), r[2], r[3], set(r[5].split(',')), r[1], r[7], '\t'.join(r[8:])))
new = open(cache_f, 'a')
for fn in glob.glob('runs/*_best.txt') + glob.glob('runs/*.elite') + glob.glob('runs/*pool*.txt'):
    try: L = open(fn).read().split('\n')
    except OSError: continue
    for t in L:
        t = t.strip()
        if '(' not in t or not t.endswith('h8)'): continue
        if t not in cache:
            try: d = desc(t)
            except Exception: continue
            cache[t] = [str(int(d['T'])), str(d['ds']), str(d['lv']), str(d['first']), ','.join(sorted(d['sit'])), '1' if d['strict'] else '0']
            new.write('\t'.join(cache[t] + [t]) + '\n')
        c = cache[t]
        if c[5] != '1': continue
        rows.append((int(c[0]), c[2], c[3], set(c[4].split(',')), c[1], fn, t))
new.close()
rows.sort(key=lambda r: -r[0])
cells = {}; reps = {}
for r in rows:
    key = (r[1], r[2]); cl = None
    for i, rep in enumerate(reps.get(key, [])):
        if jac(rep, r[3]) >= 0.8: cl = i; break
    if cl is None: reps.setdefault(key, []).append(r[3]); cl = len(reps[key]) - 1
    ck = (r[1], r[2], cl)
    if ck not in cells: cells[ck] = r
os.makedirs('arch/cells', exist_ok=True)
with open('arch/families.tsv', 'w') as fo:
    fo.write('T\tlevels\tfirst\tcluster\tds\tjac_vs_9330\tnsit\tsource\tcell_file\n')
    for ck, r in sorted(cells.items(), key=lambda x: -x[1][0]):
        name = f"arch/cells/{ck[0].replace('(', '').replace(')', '').replace(', ', '_')}_{ck[1]}_{ck[2]}.txt"
        open(name, 'w').write(f'{r[0]}\n{r[6]}\n')
        fo.write(f'{r[0]}\t{ck[0]}\t{ck[1]}\t{ck[2]}\t{r[4]}\t{jac(r[3], REF):.2f}\t{len(r[3])}\t{r[5]}\t{name}\n')
print(len(rows), 'tours,', len(cells), 'cells')
