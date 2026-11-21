#!/bin/bash
# run.sh TAG SEED START ITERS FLO FHI FOCP T0 [args...]: strict novelty LNS (./nov, --free 0) -> runs/TAG_best.txt, log runs/TAG.log
TAG=$1; SEED=$2; START=$3; ITERS=$4; FLO=$5; FHI=$6; FOCP=$7; T0=$8; shift 8
SIT=$(cat arch/ref_sit.txt)
NOV="--novp ${NOVP:-2000} --novsit $SIT --novj ${NOVJ:-0.8} --novlv ${NOVLP:-3000} -12,-11,-10,-9"
COMMON="--xover 0 --budget 400000 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05"
timeout $((${TL:-1800}+200)) nice -n 5 ./nov --tours $START "$@" --start $START $COMMON $NOV --t0 $T0 --free 0 --seed $SEED \
  --iters $ITERS --time ${TL:-1800} --focus $FLO $FHI $FOCP --out runs/${TAG}_best.txt > runs/${TAG}.log 2>&1
echo "$TAG $(grep -c 'NEW BEST' runs/${TAG}.log) $(grep 'ADJ BEST' runs/${TAG}.log | tail -1) | $(tail -n 2 runs/${TAG}.log | head -1 | cut -c1-60)" >> runs/summary.txt
