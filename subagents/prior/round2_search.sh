#!/bin/bash
# Round 2 task 2: retrain the prior with the new records and the A/B pools, then fresh guided level-3
# searches anchored on 8,235 (identical to 8,211 for its first 95 moves) across moves 30-70.
# usage: bash round2_search.sh SECONDS     (run from subagents/prior; uses 2 cores)
T=$1
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
cat tmp/ab/pool_*.txt > tmp/r2_abpool.txt 2>/dev/null
./gen --out tmp/r2.bin ../../data/tours/*.txt ../polish/NEW_RECORD_8127.txt tmp/r2_abpool.txt
nice -n 5 python3 train_prior.py tmp/r2.bin --hidden 16 --epochs 20 --l2 1e-3 --wpow 1 --drop 28,29,35,36,37 --out prior_r2.txt > tmp/r2_train.log 2>&1
nice -n 5 python3 train_prior.py tmp/r2.bin --hidden 16 --epochs 20 --l2 1e-3 --wpow 1 --drop 28,29,35,36,37 --unk-gap 100 --out prior_r2_unk.txt > tmp/r2_train_unk.log 2>&1
for m in r2 r2_unk; do
  nice -n 5 timeout $((T + 200)) ./pnrpa --level 3 --threads 1 --time $T --rng 21 --seed-tours ../../data/tours/record_8235.txt \
    --anchor-min 30 --anchor-max 70 --seed-adapt 0 --no-warm --prior prior_$m.txt --beta 2 \
    --pool tmp/r2_pool_$m.txt --pool-min 7500 --out tmp/r2_best_$m.txt > tmp/r2_search_$m.log 2>&1 &
done
wait
