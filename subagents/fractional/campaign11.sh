#!/bin/bash
# Iterated warm level-2 free-sink NRPA on the 7,200 family: each 240 s run is seeded with the best so far.
D=subagents/fractional/tmp; B=$D/fnrpa; cp $D/n_r71.txt $D/it_best.txt
for r in 81 82 83 84 85; do
  timeout 400 nice -n 5 $B --seed $D/it_best.txt --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 240 --amin 6 --amax 999 --rng $r --out $D/it_r$r.txt > $D/it_r$r.log 2>&1
  if [ -f $D/it_r$r.txt ] && [ "$(head -1 $D/it_r$r.txt)" -gt "$(head -1 $D/it_best.txt)" ]; then cp $D/it_r$r.txt $D/it_best.txt; fi
done
echo done > $D/campaign11.done
