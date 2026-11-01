#!/bin/bash
# Round 3 (resumed): level-3 warm free-sink NRPA on the 7,200 family from 7,983, 2 x 900 s.
D=subagents/fractional/tmp; B=$D/fnrpa; S=subagents/fractional/tour7200fam_7983.txt
timeout 1000 nice -n 5 $B --seed $S --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 3 --time 900 --amin 6 --amax 999 --rng 91 --out $D/p_r91.txt > $D/p_r91.log 2>&1
timeout 1000 nice -n 5 $B --seed $S --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 3 --time 900 --amin 6 --amax 60 --rng 92 --out $D/p_r92.txt > $D/p_r92.log 2>&1
echo done > $D/campaign12.done
