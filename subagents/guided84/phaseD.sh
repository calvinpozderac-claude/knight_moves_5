#!/bin/bash
# Phase D: L3 pnrpa2 (pnrpa + --sa-set: random number of seed-adapt steps per restart) with prior_g3, seeds 8,484 and 8,488
# (random per restart), dance anchors AMIN-AMAX, --no-warm, beta-set 0.5-2.5, init-noise 0.5.
# usage: bash phaseD.sh TAG SECONDS RNG AMIN AMAX SASET
G=$1; S=$2; R=$3; A=$4; B=$5; SA=$6
nice -n 5 timeout $((S + 300)) ./pnrpa2 --level 3 --threads 1 --time $S --rng $R --seed-tours tmp/seeds2.txt --anchor-min $A --anchor-max $B \
  --sa-set $SA --no-warm --prior prior_g3.txt --beta-set 0.5,1,1.5,2,2.5 --init-noise ${NZ:-0.5} \
  --pool tmp/D/pool_$G.txt --pool-min 8000 --out tmp/D/best_$G.txt > tmp/D/$G.log 2>&1
