#!/bin/bash
# graft.sh TAG A B: strict crossovers both ways (prefix of one tour + DFS<=2 + re-sync anywhere in the other, every cut point),
# no novelty penalty, then 1200 s plain LNS; logs the best graft per direction; elites (grafts included) in runs/TAG_best.txt.elite
TAG=$1; A=$2; B=$3
timeout 3600 nice -n 5 ./nov --tours $A $B --xover 1 --budget 400000 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 \
  --t0 40 --free 0 --seed $RANDOM --iters 30000 --time 1200 --focus 1 185 0.5 --out runs/${TAG}_best.txt > runs/${TAG}.log 2>&1
echo "$TAG $(grep '^xover' runs/${TAG}.log | tr '\n' ' ') | $(tail -2 runs/${TAG}.log | head -1 | cut -c1-50)" >> runs/graft_summary.txt
