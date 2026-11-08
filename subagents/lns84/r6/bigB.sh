#!/bin/bash
cd /home/user/knight_moves_5/subagents/lns84
while kill -0 $1 2>/dev/null; do sleep 10; done
best=$(for g in NEW_RECORD_*.txt; do echo "$(head -1 $g) $g"; done | sort -rn | head -1 | awk '{print $2}')
cp $best r6/bigB_in.txt
echo "$(date +%T) polish-big BIG on $best" >> r6/events.log
timeout 1400 nice -n 5 ./polish --in r6/bigB_in.txt --time 1200 --threads 1 --wmax 16 --ext 4 --ext0 5 --node-limit 6000000 --no-nrpa --pool-min 8000 --rng 7 \
   --out r6/BIG_best.txt --pool r6/BIG_pool.txt --polished r6/BIG_final.txt > r6/BIG.log 2>&1
echo "$(date +%T) polish-big BIG end: $(grep '^done' r6/BIG.log)" >> r6/events.log
./r6/roundB.sh 1 r6/BIG_best.txt 231 232
