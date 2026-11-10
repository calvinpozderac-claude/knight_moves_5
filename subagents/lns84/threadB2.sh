#!/bin/bash
# independent lineage from the 8,337 family (polished 8,390), no 8,484-family input; then polish its best
cd /home/user/knight_moves_5/subagents/lns84
while pgrep -f "seed 21 " >/dev/null; do sleep 10; done
for s in 31 33; do
  d=tmp/in_LE_s$s; mkdir -p $d; cp tmp/pol37_best.txt tmp/pol37_pool.txt tmp/record_8337.txt $d/
  [ -f tmp/LE_s31_best.txt.elite ] && cp tmp/LE_s31_best.txt.elite $d/
  read LO HI P < tmp/focus.txt
  echo "$(date +%T) LE s$s (8337 lineage) focus $LO $HI $P inputs: $(ls $d | tr '\n' ' ')" >> tmp/threads.log
  TIMEFORMAT='cpu_user=%U wall=%R'
  { time timeout 3000 nice -n 5 ./lns --tours $d/* --xover 0 --seed $s --t0 40 --budget 400000 --focus $LO $HI $P \
     --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 30000 --time 2800 \
     --out tmp/LE_s${s}_best.txt > tmp/LE_s${s}.log 2>&1 ; } 2>> tmp/LE_s${s}.log
  echo "$(date +%T) LE s$s end: $(grep -E '^done' tmp/LE_s${s}.log)" >> tmp/threads.log
done
./pol_one.sh PE_lineage tmp/LE_s33_best.txt 900
./lns_thread.sh LB 30000 25 27
