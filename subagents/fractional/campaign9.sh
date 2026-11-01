#!/bin/bash
# Round 3 variants from 8,329 (after campaign8): half-unit free sinks (sc 2) x 2 seeds, then one level-3 warm run.
D=subagents/fractional/tmp; B=$D/fnrpa; S=data/tours/record_8329.txt
until [ -f $D/campaign8.done ]; do sleep 10; done
for r in 61 62; do timeout 400 nice -n 5 $B --seed $S --sc 2 --free 1 --fstep 1 --fmax 8 --warm 10 --level 2 --time 240 --amin 35 --amax 999 --rng $r --out $D/m_sc2_r$r.txt --pool $D/pool9.txt --pool-min 8330 > $D/m_sc2_r$r.log 2>&1; done
timeout 800 nice -n 5 $B --seed $S --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 3 --time 600 --amin 35 --amax 100 --rng 63 --out $D/m_l3_r63.txt --pool $D/pool9.txt --pool-min 8330 > $D/m_l3_r63.log 2>&1
echo done > $D/campaign9.done
