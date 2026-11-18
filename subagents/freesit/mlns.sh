#!/bin/bash
# mlns.sh TAG SEED START TIME T0 [extra]: merge LNS scored by the dance beam -> runs/sML_TAG_best.txt, log runs/sML_TAG.log
TAG=$1; SEED=$2; START=$3; TM=$4; T0=$5; shift 5
MLNS=${W1:-5000} MLW2=${W2:-20000} timeout $((TM+300)) nice -n 5 ./sfl_ml --tours $START --start $START --xover 0 --free 0 --budget 200000 --dmax 6 --pdmax 3 --rmax 6 --wide 6 --skel 0.3 --t0 $T0 --t1 1 --time $TM --focus 15 99 1 --seed $SEED --dump runs/sML_${TAG}_dump.txt --dumpmax 2000 "$@" --out runs/sML_${TAG}_best.txt > runs/sML_${TAG}.log 2>&1
echo "sML_$TAG $(grep -c 'NEW BEST' runs/sML_${TAG}.log) $(tail -1 runs/sML_${TAG}.log)" >> runs/summary.txt
