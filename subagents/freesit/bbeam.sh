#!/bin/bash
# bbeam.sh TAG TOUR K W [BBT]: block-macro beam from state before move K+1 -> runs/sBB_TAG_best.txt, log ins/bb_TAG.log
TAG=$1; T=$2; K=$3; W=$4; X=$5
if [ -n "$X" ]; then export BBT=1; fi
BLOCKBEAM=$W BBK=$K timeout 5000 nice -n 5 ./sfl_bb --tours $T --start $T --xover 0 --free 0 --out runs/sBB_${TAG}_best.txt > ins/bb_$TAG.log 2>&1
