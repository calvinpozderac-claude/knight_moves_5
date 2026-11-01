#!/bin/bash
# Round 3, core A: guided level-3 pnrpa, anchors 30-45 and 60-75 of 8,329 and 8,297, random beta per restart,
# 3 rngs per configuration, S seconds per run.   usage: bash r3_guided.sh S   (from subagents/prior)
S=$1
for r in 1 2 3; do for seed in ../../data/tours/record_8329.txt ../lns/NEW_RECORD_8297.txt; do for rg in "30 45" "60 75"; do
  set -- $rg; tag=$(basename $seed .txt)_a$1-$2_r$r
  nice -n 5 timeout $((S + 200)) ./pnrpa --level 3 --threads 1 --time $S --rng $((100 + r)) --seed-tours $seed \
    --anchor-min $1 --anchor-max $2 --seed-adapt 0 --no-warm --prior prior_r3.txt --beta-set 1,1.5,2,2.5 \
    --pool tmp/r3/pool_guided.txt --pool-min 7500 --out tmp/r3/best_$tag.txt > tmp/r3/g_$tag.log 2>&1
done; done; done
