#!/bin/bash
# continuation from 8330 (+8329, 8297), 3 seeds x 6000 iters, free sinks on
cd /home/user/knight_moves_5/subagents/lns
while pgrep -f "run9.sh" > /dev/null; do sleep 5; done
D=../../data/tours
TIMEFORMAT='cpu_user=%U wall=%R'
for s in 91 92 93; do
 { time timeout 400 nice -n 5 ./lns_v3 --tours tmp/keep_8330.txt $D/record_8329.txt tmp/keep_8297b.txt --xover 0 --seed $s --t0 40 \
   --budget 400000 --focus 1 60 0.7 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 6000 --time 380 \
   --out tmp/r10_s${s}_best.txt > tmp/r10_s${s}.log 2>&1 ; } 2>> tmp/r10_s${s}.log
done
