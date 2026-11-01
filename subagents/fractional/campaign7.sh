#!/bin/bash
# Two more seeds of the campaign6 setting (5 seeds in total from 8,292), after campaign6.
D=subagents/fractional/tmp; B=$D/fnrpa; S=$D/seed6.txt
until [ -f $D/campaign6.done ]; do sleep 10; done
for r in 44 45; do timeout 400 nice -n 5 $B --seed $S --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 240 --amin 35 --amax 999 --rng $r --out $D/h_r$r.txt --pool $D/pool6.txt --pool-min 8293 > $D/h_r$r.log 2>&1; done
echo done > $D/campaign7.done
