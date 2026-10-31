#!/bin/bash
# One thread, sequential. Run from the repo root: bash subagents/fractional/campaign.sh
D=subagents/fractional/tmp; B=$D/fnrpa
run() { tag=$1; shift; timeout 400 nice -n 5 $B "$@" --out $D/c_$tag.txt --pool $D/pool.txt --pool-min 8124 > $D/c_$tag.log 2>&1; }
for r in 1 2 3; do run s23_sc1_r$r --seed data/tours/record_8123.txt --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 240 --amin 35 --amax 134 --rng $r; done
for r in 1 2 3; do run s30_sc1_r$r --seed subagents/lns/NEW_RECORD_8130.txt --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 240 --amin 35 --amax 155 --rng $r; done
for r in 1 2; do run s30_sc2_r$r --seed subagents/lns/NEW_RECORD_8130.txt --sc 2 --free 1 --fstep 1 --fmax 8 --warm 10 --level 2 --time 240 --amin 35 --amax 155 --rng $r; done
echo CAMPAIGN_DONE > $D/campaign.done
