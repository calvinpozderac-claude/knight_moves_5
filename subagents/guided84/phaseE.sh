#!/bin/bash
# Phase E: pnrpa3 = pnrpa2 + --tail-from M --tail-adapt N: the policy remembers only the seed's tail (moves M..end,
# the d-file re-cross and the end game) and searches the dance from the anchor with fresh weights (sa from SASET).
# usage: bash phaseE.sh TAG SECONDS RNG AMIN AMAX SASET TAILFROM TAILADAPT [NOISE]
G=$1; S=$2; R=$3; A=$4; B=$5; SA=$6; TF=$7; TA=$8; NZ=${9:-0.5}
nice -n 5 timeout $((S + 300)) ./pnrpa3 --level 3 --threads 1 --time $S --rng $R --seed-tours tmp/seeds2.txt --anchor-min $A --anchor-max $B \
  --sa-set $SA --tail-from $TF --tail-adapt $TA --no-warm --prior prior_g3.txt --beta-set 0.5,1,1.5,2,2.5 --init-noise $NZ \
  --pool tmp/D/pool_$G.txt --pool-min 8000 --out tmp/D/best_$G.txt > tmp/D/$G.log 2>&1
