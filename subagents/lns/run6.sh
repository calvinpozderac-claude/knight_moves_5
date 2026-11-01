#!/bin/bash
# continue from 8288 (+ 8241, records): 3 seeds with free sinks up to 2, 1 seed up to 4
cd /home/user/knight_moves_5/subagents/lns
while pgrep -x run5.sh > /dev/null || pgrep -f "bash ./run5.sh" > /dev/null; do sleep 5; done
IN="tmp/keep_8288.txt tmp/smoke2_8241.txt ../../data/tours/record_8239.txt ../../data/tours/record_8235.txt ../../data/tours/record_8211.txt tmp/div_8123.txt ../../data/tours/record_8002.txt ../../data/tours/record_7911.txt"
TIMEFORMAT='cpu_user=%U wall=%R'
for cfg in "51 2" "52 2" "53 2" "54 4"; do set -- $cfg
  { time timeout 600 nice -n 5 ./lns_v3 --tours $IN --xover 0 --seed $1 --t0 40 --budget 400000 --focus 1 60 0.7 \
     --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free $2 --iters 9000 --time 580 \
     --out tmp/r6_s$1_f$2_best.txt > tmp/r6_s$1_f$2.log 2>&1 ; } 2>> tmp/r6_s$1_f$2.log
done
