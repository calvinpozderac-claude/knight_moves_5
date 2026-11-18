#!/bin/bash
# strict chain: schain.sh TAG SEED START SQ HI DONORS... : TL s hard exclusion (free 0) then relax from the adjusted tour
TAG=$1; SEED=$2; START=$3; SQ=$4; HI=$5; shift 5
TL=${TL:-2400} ./runl.sh ${TAG}p $SEED $START 100000 1 100 0.9 60 0 "$@" --vpen 1000 --vsq $SQ --vrange 1 $HI
[ -f runs/${TAG}p_best.txt.adj ] && cp runs/${TAG}p_best.txt.adj in/${TAG}adj.txt || cp $START in/${TAG}adj.txt
TL=${TL:-2400} ./runl.sh ${TAG}r $((SEED+100)) in/${TAG}adj.txt 100000 1 178 0.6 60 0 $START "$@"
