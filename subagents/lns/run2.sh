#!/bin/bash
# 3 seeds, sequential (one thread), deterministic iteration schedule, focus on moves 1-60
cd /home/user/knight_moves_5/subagents/lns
for s in 11 12 13; do
  timeout 720 nice -n 5 ./lns_v2 --tours tmp/seed_8130.txt ../../data/tours/record_8123.txt ../../data/tours/record_8002.txt \
    ../../data/tours/record_7911.txt tmp/run1_best.txt.elite --xover 0 --seed $s --t0 40 --budget 400000 \
    --focus 1 60 0.7 --iters 55000 --time 660 --out tmp/run2_s${s}_best.txt > tmp/run2_s${s}.log 2>&1
done
