#!/bin/bash
# Interleaved, matched-seed L3 comparison on the 8,130 tour: for each rng, each anchor, each arm, one complete restart.
cd /home/user/knight_moves_5
OUT=subagents/nrpa/tmp/proto
mkdir -p $OUT
for r in 1 2 3 4 5 6 7 8 9 10 11 12; do
 for A in 44 52 60; do
  for arm in none bsink4_3; do
   cfg=""; [ $arm = bsink4_3 ] && cfg="--bsink4 3"
   nice -n 5 subagents/nrpa/gnrpa --level 3 --threads 1 --restarts 1 --rng $r --seed-tours subagents/lns/NEW_RECORD_8130.txt \
     --anchor-min $A --anchor-max $A --seed-adapt 0 --no-warm $cfg --pool subagents/nrpa/tmp/pool8130.txt --pool-min 8000 \
     --out $OUT/best_${arm}_A${A}_r${r}.txt > $OUT/log_${arm}_A${A}_r${r}.txt 2>&1
   echo "arm=$arm A=$A rng=$r $(grep RESTART $OUT/log_${arm}_A${A}_r${r}.txt)"
  done
 done
done
