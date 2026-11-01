#!/bin/bash
# longer outside-family lineage: start from run8's best B tour (7200 family) + outside div set, 30000 iters
cd /home/user/knight_moves_5/subagents/lns
while pgrep -f "run8.sh" > /dev/null; do sleep 5; done
D=../../data/tours
B=$(for f in tmp/r8_B_*_best.txt; do echo "$(head -1 $f) $f"; done | sort -n | tail -1 | cut -d' ' -f2)
echo "start from $B" > tmp/r9.log
TIMEFORMAT='cpu_user=%U wall=%R'
{ time timeout 900 nice -n 5 ./lns_v3 --tours $B $D/record_7200.txt $D/record_7066.txt $D/tour_6303.txt tmp/curr_outside.txt --xover 0 --seed 81 --t0 40 \
   --budget 400000 --focus 1 60 0.7 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 30000 --time 870 \
   --out tmp/r9_s81_best.txt >> tmp/r9.log 2>&1 ; } 2>> tmp/r9.log
