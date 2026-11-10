#!/bin/bash
# Round 3: warm level-2 free-sink NRPA from the 8,329 record, 5 seeds x 240 s, one thread.
D=subagents/fractional/tmp; B=$D/fnrpa; S=data/tours/record_8329.txt
for r in 51 52 53 54 55; do timeout 400 nice -n 5 $B --seed $S --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 240 --amin 35 --amax 999 --rng $r --out $D/k_r$r.txt --pool $D/pool8.txt --pool-min 8330 > $D/k_r$r.log 2>&1; done
echo done > $D/campaign8.done
