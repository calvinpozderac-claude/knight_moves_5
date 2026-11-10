#!/bin/bash
# run9 seeds 82, 83: same start (run8 B_div s73 best, 7,611, 7200 family) and same donors as seed 81, 30000 iters
cd /home/user/knight_moves_5/subagents/lns
D=../../data/tours
TIMEFORMAT='cpu_user=%U wall=%R'
for s in 82 83; do
{ time timeout 900 nice -n 5 ./lns_v3 --tours tmp/r8_B_div_s73_best.txt $D/record_7200.txt $D/record_7066.txt $D/tour_6303.txt tmp/curr_outside.txt --xover 0 --seed $s --t0 40 \
   --budget 400000 --focus 1 60 0.7 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 30000 --time 870 \
   --out tmp/r9_s${s}_best.txt > tmp/r9_s$s.log 2>&1 ; } 2>> tmp/r9_s$s.log
done
