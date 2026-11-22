#!/bin/bash
# cchain.sh TAG SEED START CP CJ OPP [args]: new-CLIMB search: strict LNS with climb novelty (penalty CP when the Jaccard of the
# pre-dance sitters to the 9330 climb > CJ; OPP when >= 12 of the first 15 squares equal 9330's) and shallow dance levels
# (lower level -8..2) penalised 3000; 2400 s, whole tour; then the dance block beam (W 20000) from ds-4/ds/ds+4.
TAG=$1; SEED=$2; START=$3; CP=$4; CJ=$5; OPP=$6; shift 6
COMMON="--xover 0 --budget 400000 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05"
NOV="--novclimb $CP $CJ $(cat arch/ref_climb.txt) --novop $OPP 12 $(cat arch/ref_op15.txt) --novlv 3000 -8,-7,-6,-5,-4,-3,-2,-1,0,1,2"
timeout 2700 nice -n 5 ./nov --tours $START "$@" --start $START $COMMON $NOV --t0 80 --free 0 --seed $SEED --iters 60000 --time 2400 --focus 1 100 0.8 --out runs/${TAG}_best.txt > runs/${TAG}.log 2>&1
BF=runs/${TAG}_best.txt.adj; [ -f $BF ] || BF=$START
DS=$(python3 desc.py $BF | awk '{for(i=1;i<=NF;i++) if($i=="ds") print $(i+1)}'); B=$(head -1 $BF)
for k in $((DS-4)) $DS $((DS+4)); do
  BBT=1 BLOCKBEAM=20000 BBK=$k timeout 300 nice -n 5 ./bb2 --tours $BF --start $BF --xover 0 --free 0 --out runs/${TAG}_b${k}_best.txt > /dev/null 2>&1
  [ -f runs/${TAG}_b${k}_best.txt ] && v=$(head -1 runs/${TAG}_b${k}_best.txt) && [ "$v" -gt "$B" ] && B=$v
done
echo "$TAG adj $(head -1 $BF) $(grep 'ADJ BEST' runs/${TAG}.log | tail -1 | cut -c1-60) beam $B | $(python3 desc.py $BF | cut -d' ' -f2-9)" >> runs/climb_summary.txt
