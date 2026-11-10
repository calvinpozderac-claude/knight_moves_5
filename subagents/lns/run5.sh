#!/bin/bash
# Round 2 A/B: A = v3 with free sinks (--free 2), B = v3 strict (--free 0). 5 seeds each, equal iterations,
# deterministic (iteration schedule), sequential on one thread.
cd /home/user/knight_moves_5/subagents/lns
IN="tmp/smoke2_8241.txt ../../data/tours/record_8239.txt ../../data/tours/record_8235.txt ../../data/tours/record_8211.txt tmp/run3_s21_best.txt.elite tmp/div_8123.txt ../../data/tours/record_8123.txt ../../data/tours/record_8002.txt ../../data/tours/record_7911.txt"
TIMEFORMAT='cpu_user=%U wall=%R'
for s in 41 42 43 44 45; do for arm in A B; do
  F=2; [ $arm = B ] && F=0
  { time timeout 600 nice -n 5 ./lns_v3 --tours $IN --xover 0 --seed $s --t0 40 --budget 400000 --focus 1 60 0.7 \
     --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free $F --iters 12000 --time 580 \
     --out tmp/r5${arm}_s${s}_best.txt > tmp/r5${arm}_s${s}.log 2>&1 ; } 2>> tmp/r5${arm}_s${s}.log
done; done
