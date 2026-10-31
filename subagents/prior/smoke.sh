#!/bin/bash
# Smoke comparison: stock vs prior NRPA level 2, fresh weights, from anchor A of the 8,123 tour.
# usage: bash smoke.sh ANCHOR SECONDS "LABEL|EXTRA ARGS" ...   (run from subagents/prior)
A=$1; T=$2; shift 2
for cfg in "$@"; do
  lab=${cfg%%|*}; args=${cfg#*|}
  log=tmp/smoke_a${A}_${lab}.log
  timeout $((T + 30)) nice -n 15 ./pnrpa --level 2 --threads 1 --time $T --rng 1 --seed-tours ../../data/tours/record_8123.txt \
    --anchor-min $A --anchor-max $A --seed-adapt 0 --no-warm --out tmp/smoke_best_a${A}_${lab}.txt $args > $log
  awk -v lab="$lab" -v A=$A '/restart/ && /finished/ {n++; s+=$(NF-8+0); } /restart/ {r++} /^done/ {d=$0}
       /restart/ { for(i=1;i<=NF;i++) if ($i=="minutes,") { m=$(i-1); if ($(i-2)=="finished") { f++; t+=m; if (m>b) b=m } } }
       END { printf "anchor %s %-10s restarts %d finished %d mean %.0f best %d | %s\n", A, lab, r, f, (f?t/f:0), b, d }' $log
done
