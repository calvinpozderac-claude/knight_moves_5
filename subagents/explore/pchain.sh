#!/bin/bash
# pchain.sh TAG SEED START SQS LO P [donors...]: family-jump chain: (1) 1200 s LNS with a visit penalty P on SQS in moves LO..200
# (+ novelty penalty); (2) T-ranked dance block beam W 20000 from ds-4/ds/ds+4 of the adjusted tour (best kept);
# (3) 1200 s relax (novelty penalty only) from the best of adjusted/beam tours.
TAG=$1; SEED=$2; START=$3; SQS=$4; LO=$5; P=$6; shift 6
TL=1200 ./run.sh ${TAG}p $SEED $START 30000 1 185 0.5 80 "$@" --vpen $P --vsq $SQS --vrange $LO 200
[ -f runs/${TAG}p_best.txt.adj ] && cp runs/${TAG}p_best.txt.adj in/${TAG}adj.txt || cp $START in/${TAG}adj.txt
DS=$(python3 desc.py in/${TAG}adj.txt | awk '{for(i=1;i<=NF;i++) if($i=="ds") print $(i+1)}')
B=$(head -1 in/${TAG}adj.txt); BF=in/${TAG}adj.txt
for k in $((DS-4)) $DS $((DS+4)); do
  BBT=1 BLOCKBEAM=20000 BBK=$k timeout 300 nice -n 5 ./bb --tours $BF --start $BF --xover 0 --free 0 --out runs/${TAG}b${k}_best.txt > /dev/null 2>&1
  if [ -f runs/${TAG}b${k}_best.txt ]; then v=$(head -1 runs/${TAG}b${k}_best.txt); if [ "$v" -gt "$B" ]; then B=$v; cp runs/${TAG}b${k}_best.txt in/${TAG}beam.txt; fi; fi
done
[ -f in/${TAG}beam.txt ] && BF=in/${TAG}beam.txt
echo "$TAG adj $(head -1 in/${TAG}adj.txt) beam $B" >> runs/chain_beam.txt
TL=1200 ./run.sh ${TAG}r $((SEED+1)) $BF 30000 1 185 0.5 60 $START "$@"
