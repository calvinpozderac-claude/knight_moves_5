#!/bin/bash
# Round 3: free-sink NRPA from lns's 7,200-family tour (tmp/r9_s81_best.txt, 7,897), warm level 2, 3 seeds x 240 s,
# anchors 6..end (its climb leaves 8,123's at move 6).
D=subagents/fractional/tmp; B=$D/fnrpa; S=subagents/lns/tmp/r9_s81_best.txt
cp $S $D/seed10.txt
for r in 71 72 73; do timeout 400 nice -n 5 $B --seed $D/seed10.txt --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 240 --amin 6 --amax 999 --rng $r --out $D/n_r$r.txt --pool $D/pool10.txt --pool-min 7898 > $D/n_r$r.log 2>&1; done
echo done > $D/campaign10.done
