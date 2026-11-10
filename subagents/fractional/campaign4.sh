#!/bin/bash
# Round 2 after the ruling (free waits legal): free-sink warm level-2 NRPA from 8,264 / 8,235 / 8,239, 240 s each.
D=subagents/fractional/tmp; B=$D/fnrpa
run() { tag=$1; S=$2; shift 2; timeout 400 nice -n 5 $B "$@" --seed $S --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 240 --amin 35 --amax 999 --out $D/f_$tag.txt --pool $D/pool4.txt --pool-min 8265 > $D/f_$tag.log 2>&1; }
for r in 21 22 23; do run s8264_r$r subagents/fractional/NEW_RECORD_8264.txt --rng $r; run s8235_r$r data/tours/record_8235.txt --rng $r; done
for r in 21 22; do run s8239_r$r data/tours/record_8239.txt --rng $r; done
echo done > $D/campaign4.done
