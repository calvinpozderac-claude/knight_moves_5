#!/bin/bash
# usage: lineage.sh TAG SEED ITERS INPUTS...  independent lineage (inputs fixed, copied to tmp/in_TAG_sSEED), then polish its best
cd /home/user/knight_moves_5/subagents/lns84
tag=$1; s=$2; it=$3; shift 3
d=tmp/in_${tag}_s$s; mkdir -p $d; cp "$@" $d/
read LO HI P < tmp/focus.txt
echo "$(date +%T) $tag s$s (lineage) focus $LO $HI $P inputs: $(ls $d | tr '\n' ' ')" >> tmp/threads.log
TIMEFORMAT='cpu_user=%U wall=%R'
{ time timeout 3000 nice -n 5 ./lns --tours $d/* --xover 0 --seed $s --t0 40 --budget 400000 --focus $LO $HI $P \
   --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters $it --time 2800 \
   --out tmp/${tag}_s${s}_best.txt > tmp/${tag}_s${s}.log 2>&1 ; } 2>> tmp/${tag}_s${s}.log
echo "$(date +%T) $tag s$s end: $(grep -E '^done' tmp/${tag}_s${s}.log)" >> tmp/threads.log
./pol_one.sh P${tag}_s$s tmp/${tag}_s${s}_best.txt 600
