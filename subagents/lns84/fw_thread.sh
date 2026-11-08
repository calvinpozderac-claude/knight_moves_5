#!/bin/bash
# fw84's free-wait pass (fnrpa, warm 10, level 2, free sinks 1-4) on distinct 8,60x skeletons, several rngs
cd /home/user/knight_moves_5/subagents/lns84
mkdir -p tmp/fw
for f in NEW_RECORD_8605.txt tmp/keep_8605_LDs16.txt NEW_RECORD_8603.txt NEW_RECORD_8602.txt; do for r in 1 2 3; do
  id=$(basename $f .txt)_r$r
  echo "$(date +%T) fw $id start" >> tmp/threads.log
  timeout 450 nice -n 5 ./fnrpa --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 300 --amin 35 --amax 999 --rng $r \
    --seed $f --out tmp/fw/fw_$id.txt --pool tmp/fw/fw_pool.txt --pool-min 8590 > tmp/fw/fw_$id.log 2>&1
  echo "$(date +%T) fw $id: $(head -1 $f) -> $(head -1 tmp/fw/fw_$id.txt)" >> tmp/threads.log
  python3 newrec.py tmp/fw/fw_pool.txt tmp/fw/fw_$id.txt >> tmp/fw/records.txt
done; done
