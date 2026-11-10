#!/bin/bash
# Round 3 (after review pass 1). usage: bash r3b.sh dance S | climb S    (from subagents/prior)
mode=$1; S=$2
if [ $mode = dance ]; then
  # dance re-search, anchors 55-80 of 8,330 / 8,329 / 8,297, 3 rngs each, random beta + random initial weights per restart
  for r in 1 2 3; do for b in 8330 8329 8297; do
    nice -n 5 timeout $((S + 200)) ./pnrpa --level 3 --time $S --rng $((300 + r)) --seed-tours ../../data/tours/record_$b.txt \
      --anchor-min 55 --anchor-max 80 --seed-adapt 0 --no-warm --prior prior_r3.txt --beta-set 0.5,1,1.5,2,2.5 --init-noise 0.5 \
      --pool tmp/r3b/pool_dance.txt --pool-min 7500 --out tmp/r3b/best_d${b}_r$r.txt > tmp/r3b/d${b}_r$r.log 2>&1
  done; done
else
  # climb track: anchors 20-60 of lns's 7,200-family tour (snapshot of lns/tmp/r9_s81_best.txt, 7,879)
  for r in 1 2 3; do
    nice -n 5 timeout $((S + 300)) ./pnrpa --level 3 --time $S --rng $((400 + r)) --seed-tours tmp/r3/seed_7200fam.txt \
      --anchor-min 20 --anchor-max 60 --seed-adapt 0 --no-warm --prior prior_r3_unk.txt --beta-set 0,0.5,1,1.5,2 --init-noise 0.5 \
      --pool tmp/r3b/pool_climb.txt --pool-min 7000 --out tmp/r3b/best_c_r$r.txt > tmp/r3b/c_r$r.log 2>&1
  done
fi
