#!/bin/bash
# rchain.sh TAG SEED START [donors]: 2400 s novelty relax LNS, then T-ranked dance block beam (W 20000) from ds-4/ds/ds+4 of its best
TAG=$1; SEED=$2; START=$3; shift 3
TL=2400 ./run.sh ${TAG} $SEED $START 60000 1 185 0.5 60 "$@"
BF=runs/${TAG}_best.txt; [ -f $BF ] || BF=$START; B=$(head -1 $BF)
DS=$(python3 desc.py $BF | awk '{for(i=1;i<=NF;i++) if($i=="ds") print $(i+1)}')
for k in $((DS-4)) $DS $((DS+4)); do
  BBT=1 BLOCKBEAM=20000 BBK=$k timeout 300 nice -n 5 ./bb --tours $BF --start $BF --xover 0 --free 0 --out runs/${TAG}_b${k}_best.txt > /dev/null 2>&1
  [ -f runs/${TAG}_b${k}_best.txt ] && v=$(head -1 runs/${TAG}_b${k}_best.txt) && [ "$v" -gt "$B" ] && B=$v
done
echo "$TAG relax $(head -1 $BF) beam $B" >> runs/chain_beam.txt
