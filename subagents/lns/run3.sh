#!/bin/bash
# independent lineage from 8123 variants (opening agent's r_0..r_11) + records, without 8130
cd /home/user/knight_moves_5/subagents/lns
until ! pgrep -f run2.sh > /dev/null; do sleep 5; done
timeout 720 nice -n 5 ./lns_v2 --tours tmp/div_8123.txt ../../data/tours/record_8123.txt ../../data/tours/record_8002.txt \
  ../../data/tours/record_7911.txt --xover 0 --seed 21 --t0 40 --budget 400000 \
  --focus 1 60 0.7 --iters 55000 --time 660 --out tmp/run3_s21_best.txt > tmp/run3_s21.log 2>&1
