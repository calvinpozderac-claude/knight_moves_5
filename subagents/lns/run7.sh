#!/bin/bash
# 2x2 starting-set test (free sinks on, --free 2): {div: 12 variants of 8123 + 7200/7066 families, none} x {start 8123, start 8292}
# 2 seeds per cell, 6000 iters each, deterministic, sequential on one thread.
cd /home/user/knight_moves_5/subagents/lns
D=../../data/tours
DIV="tmp/div_8123.txt $D/record_7200.txt $D/record_7066.txt"
TIMEFORMAT='cpu_user=%U wall=%R'
for s in 61 62; do
 for start in 8123 8292; do for div in div nodiv; do
  if [ $start = 8123 ]; then IN="$D/record_8123.txt $D/record_8002.txt $D/record_7911.txt"; else IN="$D/record_8292.txt $D/record_8123.txt $D/record_8002.txt $D/record_7911.txt"; fi
  [ $div = div ] && IN="$IN $DIV"
  { time timeout 400 nice -n 5 ./lns_v3 --tours $IN --xover 0 --seed $s --t0 40 --budget 400000 --focus 1 60 0.7 \
     --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 6000 --time 380 \
     --out tmp/r7_${start}_${div}_s${s}_best.txt > tmp/r7_${start}_${div}_s${s}.log 2>&1 ; } 2>> tmp/r7_${start}_${div}_s${s}.log
 done; done
done
