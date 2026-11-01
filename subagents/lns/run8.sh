#!/bin/bash
# Round 3 2x2 starting-set test, free sinks on (--free 2), 3 seeds per cell, 9000 iters, deterministic, 1 thread.
# start A = 8329 (8,211 family); start B = 7200 (outside: bottom climb diverges from 8,123 at move 6).
# div for A: outside tours (7066, 6303, 10 curriculum tours with other openings) + 12 8,123 variants with
# re-routed climbs (opening's m1s*) + 8002 + 7911. div for B: only the outside tours (all below 7,200).
cd /home/user/knight_moves_5/subagents/lns
D=../../data/tours
OUT="$D/record_7066.txt $D/tour_6303.txt tmp/curr_outside.txt"
TIMEFORMAT='cpu_user=%U wall=%R'
for s in 71 72 73; do
 for cell in A_div A_nodiv B_div B_nodiv; do
  case $cell in
   A_div) IN="$D/record_8329.txt $OUT tmp/outside_climbs.txt $D/record_8002.txt $D/record_7911.txt";;
   A_nodiv) IN="$D/record_8329.txt";;
   B_div) IN="$D/record_7200.txt $OUT";;
   B_nodiv) IN="$D/record_7200.txt";;
  esac
  { time timeout 600 nice -n 5 ./lns_v3 --tours $IN --xover 0 --seed $s --t0 40 --budget 400000 --focus 1 60 0.7 \
     --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 9000 --time 580 \
     --out tmp/r8_${cell}_s${s}_best.txt > tmp/r8_${cell}_s${s}.log 2>&1 ; } 2>> tmp/r8_${cell}_s${s}.log
 done
done
