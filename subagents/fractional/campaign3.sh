#!/bin/bash
# Round 2 follow-up (starts after campaign2): seed = subagents/fractional/best.txt (best strict tour so far, 8,235),
# strict (--free 0) and free-sink (--free 1) warm level-2 runs, 3 seeds x 240 s each, alternating.
D=subagents/fractional/tmp; B=$D/fnrpa; S=$D/seed3.txt
until [ -f $D/campaign2.done ]; do sleep 10; done
cp subagents/fractional/best.txt $S
run() { tag=$1; shift; timeout 400 nice -n 5 $B "$@" --seed $S --warm 10 --level 2 --time 240 --amin 35 --amax 999 --out $D/e_$tag.txt --pool $D/pool3.txt --pool-min 8236 > $D/e_$tag.log 2>&1; }
for r in 11 12 13; do
  run strict_r$r --sc 1 --free 0 --rng $r
  run free_r$r --sc 1 --free 1 --fstep 1 --fmax 4 --rng $r
done
echo done > $D/campaign3.done
