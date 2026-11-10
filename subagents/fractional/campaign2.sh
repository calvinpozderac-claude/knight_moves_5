#!/bin/bash
# Round 2, one thread, sequential. Run from the repo root: bash subagents/fractional/campaign2.sh
# Equal-time A/B from the 8,211 strict record: free sinks (--free 1) vs strict control (--free 0), 5 seeds x 240 s,
# level 2, warm start; then 2 seeds of half-unit free sinks (--sc 2).
D=subagents/fractional/tmp; B=$D/fnrpa; S=data/tours/record_8211.txt
run() { tag=$1; shift; timeout 400 nice -n 5 $B "$@" --seed $S --warm 10 --level 2 --time 240 --amin 35 --amax 999 --out $D/d_$tag.txt --pool $D/pool2.txt --pool-min 8212 > $D/d_$tag.log 2>&1; }
for r in 1 2 3 4 5; do
  run free_r$r --sc 1 --free 1 --fstep 1 --fmax 4 --rng $r
  run strict_r$r --sc 1 --free 0 --rng $r
done
for r in 1 2; do run sc2_r$r --sc 2 --free 1 --fstep 1 --fmax 8 --rng $r; done
echo done > $D/campaign2.done
