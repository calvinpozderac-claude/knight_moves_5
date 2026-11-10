#!/bin/bash
# 8330 + outside-basin tours from run9 (8103, 8060, 7998) as op-7 donors; 2 seeds x 6000 iters
cd /home/user/knight_moves_5/subagents/lns
TIMEFORMAT='cpu_user=%U wall=%R'
for s in 101 102; do
{ time timeout 400 nice -n 5 ./lns_v3 --tours tmp/keep_8330.txt tmp/r9_s82_best.txt tmp/r9_s83_best.txt tmp/r9_s81_best.txt --xover 0 --seed $s --t0 40 \
   --budget 400000 --focus 1 60 0.7 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 6000 --time 380 \
   --out tmp/r11_s${s}_best.txt > tmp/r11_s$s.log 2>&1 ; } 2>> tmp/r11_s$s.log
done
