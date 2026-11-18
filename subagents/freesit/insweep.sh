#!/bin/bash
# insweep.sh TAG TOUR LO HI D : strict insertion sweep (raw objective), log ins/TAG.log, best runs/sINS_TAG_best.txt
TAG=$1; T=$2; LO=$3; HI=$4; D=$5; shift 5
INSWEEP=$D INLO=$LO INHI=$HI INB=${INB:-3000000} INW=${INW:-20} timeout 7000 nice -n 5 ./sfl_ins --tours $T --start $T --xover 0 --free 0 --skel 0.5 "$@" --out runs/sINS_${TAG}_best.txt > ins/$TAG.log 2>&1
