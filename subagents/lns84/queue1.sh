#!/bin/bash
# phase 1 (sequential, 1 thread): wait for exact table, then polish 8484 and 8337 with wide windows
cd /home/user/knight_moves_5/subagents/lns84
while pgrep -f exact_table.sh >/dev/null; do sleep 5; done
TIMEFORMAT='cpu_user=%U wall=%R'
{ time timeout 1100 nice -n 5 ./polish --in tmp/record_8484.txt --time 1000 --threads 1 --wmax 12 --ext 3 --ext0 4 \
   --node-limit 2000000 --nrpa-level 2 --nrpa-tails 45,60,90 --pool-min 8000 \
   --out tmp/pol84_best.txt --pool tmp/pol84_pool.txt --polished tmp/pol84_final.txt > tmp/pol84.log 2>&1 ; } 2>> tmp/pol84.log
{ time timeout 800 nice -n 5 ./polish --in tmp/record_8337.txt --time 700 --threads 1 --wmax 12 --ext 3 --ext0 4 \
   --node-limit 2000000 --nrpa-level 2 --nrpa-tails 45,60,90 --pool-min 8000 \
   --out tmp/pol37_best.txt --pool tmp/pol37_pool.txt --polished tmp/pol37_final.txt > tmp/pol37.log 2>&1 ; } 2>> tmp/pol37.log
echo QUEUE1_DONE >> tmp/pol37.log
