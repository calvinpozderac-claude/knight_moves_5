#!/bin/bash
# Equal-time A/B arm: level-3 fresh NRPA from anchors of the 8,211 tour, one thread, T seconds per anchor.
# usage: bash ab.sh LABEL SECONDS "ANCHORS" [extra pnrpa args]     (run from subagents/prior)
lab=$1; T=$2; anchors=$3; shift 3
for A in $anchors; do
  nice -n 5 timeout $((T + 120)) ./pnrpa --level 3 --threads 1 --time $T --rng 11 --seed-tours ../../data/tours/record_8211.txt \
    --anchor-min $A --anchor-max $A --seed-adapt 0 --no-warm --pool tmp/ab/pool_${lab}.txt --pool-min 7500 \
    --out tmp/ab/best_${lab}_a${A}.txt "$@" > tmp/ab/${lab}_a${A}.log 2>&1
done
