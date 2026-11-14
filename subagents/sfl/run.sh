#!/bin/bash
# usage: run.sh TAG SEED START ITERS FLO FHI FOCP T0 FREE [DONORS...]   (from subagents/sfl)
TAG=$1; SEED=$2; START=$3; ITERS=$4; FLO=$5; FHI=$6; FOCP=$7; T0=$8; FREE=$9; shift 9
COMMON="--xover 0 --budget 400000 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05"
timeout 1800 nice -n 5 ./sfl --tours $START "$@" --start $START $COMMON --t0 $T0 --free $FREE --fsp 1 --seed $SEED \
  --iters $ITERS --time 1600 --focus $FLO $FHI $FOCP --out runs/${TAG}_best.txt --dump runs/${TAG}_dump.txt --dumpmax 400 > runs/${TAG}.log 2>&1
echo "$TAG $(grep -c 'NEW BEST' runs/${TAG}.log) $(head -1 runs/${TAG}_best.txt 2>/dev/null) $(tail -n 2 runs/${TAG}.log | head -1)" >> runs/summary.txt
