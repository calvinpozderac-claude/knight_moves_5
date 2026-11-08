#!/bin/bash
# Phase C: L3 pnrpa with prior_g2 (retrained, 8,484 positions boosted x30) anchored across 8,484 (44-150):
# arm "fresh" (seed-adapt 0) vs arm "sa1" (one adapt step toward 8,484's continuation, then free search; --no-warm).
# usage: bash phaseC.sh SECONDS RNG
S=$1; R=$2
for arm in fresh sa1; do
  sa=0; [ $arm = sa1 ] && sa=1
  nice -n 5 timeout $((S + 300)) ./pnrpa --level 3 --threads 1 --time $S --rng $R --seed-tours tmp/b8484.txt --anchor-min 44 --anchor-max 150 \
    --seed-adapt $sa --no-warm --prior prior_g2.txt --beta-set 0.5,1,1.5,2,2.5 --init-noise 0.5 \
    --pool tmp/C/pool_$arm.txt --pool-min 8000 --out tmp/C/best_$arm.txt > tmp/C/$arm.log 2>&1 &
done
wait
