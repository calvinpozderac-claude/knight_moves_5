#!/bin/bash
# usage: r6/lns.sh TAG SEED ITERS START FOCUSFILE DONOR...   (lns_r6 --start; inputs copied to r6/in_TAG_sSEED/)
cd /home/user/knight_moves_5/subagents/lns84
tag=$1; s=$2; it=$3; st=$4; ff=$5; shift 5
d=r6/in_${tag}_s$s; mkdir -p $d; cp $st $d/start.txt; for f in "$@"; do cp $f $d/; done
read LO HI P < $ff
echo "$(date +%T) $tag s$s start $(head -1 $st) focus $LO $HI $P donors: $(ls $d | grep -v start.txt | tr '\n' ' ')" >> r6/events.log
TIMEFORMAT='cpu_user=%U wall=%R'
{ time timeout 3000 nice -n 5 ./lns_r6 --tours $d/* --start $d/start.txt --xover 0 --seed $s --t0 40 --budget 400000 --focus $LO $HI $P \
   --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters $it --time 2800 \
   --out r6/${tag}_s${s}_best.txt > r6/${tag}_s${s}.log 2>&1 ; } 2>> r6/${tag}_s${s}.log
echo "$(date +%T) $tag s$s end: $(grep -E '^done' r6/${tag}_s${s}.log)" >> r6/events.log
