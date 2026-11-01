#!/bin/bash
# from 8334 (prior, 3 free waits) with 8330 and 8297 as crossover donors; 3 seeds x 25000 iters, free sinks on
cd /home/user/knight_moves_5/subagents/lns
D=../../data/tours
TIMEFORMAT='cpu_user=%U wall=%R'
for s in 111 112 113; do
{ time timeout 900 nice -n 5 ./lns_v3 --tours $D/record_8334.txt $D/record_8330.txt $D/record_8297.txt --xover 0 --seed $s --t0 40 \
   --budget 400000 --focus 1 60 0.7 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 25000 --time 870 \
   --out tmp/r12_s${s}_best.txt > tmp/r12_s$s.log 2>&1 ; } 2>> tmp/r12_s$s.log
done
