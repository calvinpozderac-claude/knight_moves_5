#!/bin/bash
# Round 2: A = v3 (bigger neighbourhoods + polish-style skeleton routing + reroute op), B = v2 settings; 5 seeds each,
# same inputs, same iteration count, deterministic. Sequential: one thread.
cd /home/user/knight_moves_5/subagents/lns
IN="../../data/tours/record_8211.txt tmp/run3_s21_best.txt.elite tmp/div_8123.txt ../../data/tours/record_8123.txt ../../data/tours/record_8002.txt ../../data/tours/record_7911.txt"
TIMEFORMAT='cpu_user=%U wall=%R'
for s in 31 32 33 34 35; do
  { time timeout 900 nice -n 5 ./lns_v3 --tours $IN --xover 0 --seed $s --t0 40 --budget 400000 --focus 1 60 0.7 \
     --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --iters 25000 --time 880 --out tmp/r4A_s${s}_best.txt > tmp/r4A_s${s}.log 2>&1 ; } 2>> tmp/r4A_s${s}.log
done
for s in 31 32 33 34 35; do
  { time timeout 900 nice -n 5 ./lns_v2 --tours $IN --xover 0 --seed $s --t0 40 --budget 400000 --focus 1 60 0.7 \
     --iters 25000 --time 880 --out tmp/r4B_s${s}_best.txt > tmp/r4B_s${s}.log 2>&1 ; } 2>> tmp/r4B_s${s}.log
done
