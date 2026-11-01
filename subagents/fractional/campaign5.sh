#!/bin/bash
# After 8,288: free-sink NRPA from the 8,288 record (one thread, sequential). Run from the repo root.
D=subagents/fractional/tmp; B=$D/fnrpa; S=subagents/fractional/NEW_RECORD_8288.txt
run() { tag=$1; shift; timeout 900 nice -n 5 $B --seed $S --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 "$@" --out $D/g_$tag.txt --pool $D/pool5.txt --pool-min 8289 > $D/g_$tag.log 2>&1; }
for r in 31 32 33; do run all_r$r --level 2 --time 240 --amin 35 --amax 999 --rng $r; done
for r in 34 35; do run early_r$r --level 2 --time 240 --amin 35 --amax 80 --rng $r; done
run l3_r36 --level 3 --time 600 --amin 35 --amax 100 --rng 36
echo done > $D/campaign5.done
