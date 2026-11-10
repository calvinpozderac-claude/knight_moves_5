#!/bin/bash
# usage: bash r3c.sh dance S | climb S
mode=$1; S=$2
if [ $mode = dance ]; then
  for r in 1 2 3; do for b in 8334 8330 8329; do
    nice -n 5 timeout $((S + 200)) ./pnrpa --level 3 --time $S --rng $((500 + r)) --seed-tours ../../data/tours/record_$b.txt \
      --anchor-min 55 --anchor-max 80 --seed-adapt 0 --no-warm --prior prior_r3.txt --beta-set 0.5,1,1.5,2,2.5 --init-noise 0.5 \
      --pool tmp/r3c/pool_dance.txt --pool-min 7500 --out tmp/r3c/best_d${b}_r$r.txt > tmp/r3c/d${b}_r$r.log 2>&1
  done; done
else
  for r in 1 2 3; do
    nice -n 5 timeout $((S + 300)) ./pnrpa --level 3 --time $S --rng $((600 + r)) --seed-tours tmp/r3b/c8162.txt \
      --anchor-min 20 --anchor-max 60 --seed-adapt 0 --no-warm --prior prior_r3_unk.txt --beta-set 0,0.5,1,1.5,2 --init-noise 0.5 \
      --pool tmp/r3c/pool_climb.txt --pool-min 7000 --out tmp/r3c/best_c_r$r.txt > tmp/r3c/c_r$r.log 2>&1
  done
fi
