#!/bin/bash
# Phase B: L3 fresh searches anchored across 8,484 (moves 44-130): prior_g1 (pnrpa) vs stock nrpa, 1 thread each, same time.
# usage: bash phaseB.sh SECONDS RNG
S=$1; R=$2
nice -n 5 timeout $((S + 300)) ./pnrpa --level 3 --threads 1 --time $S --rng $R --seed-tours tmp/b8484.txt --anchor-min 44 --anchor-max 130 \
  --seed-adapt 0 --no-warm --prior prior_g1.txt --beta-set 0.5,1,1.5,2,2.5 --init-noise 0.5 \
  --pool tmp/B/pool_prior.txt --pool-min 8000 --out tmp/B/best_prior.txt > tmp/B/prior.log 2>&1 &
nice -n 5 timeout $((S + 300)) ./nrpa --level 3 --threads 1 --time $S --rng $R --seed-tours tmp/b8484.txt --anchor-min 44 --anchor-max 130 \
  --seed-adapt 0 --no-warm --pool tmp/B/pool_stock.txt --pool-min 8000 --out tmp/B/best_stock.txt > tmp/B/stock.log 2>&1 &
wait
