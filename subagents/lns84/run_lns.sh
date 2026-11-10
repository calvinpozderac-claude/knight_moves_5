#!/bin/bash
# usage: run_lns.sh TAG SEED ITERS TIME INPUTS...   (deterministic schedule via --iters; free sinks on)
cd /home/user/knight_moves_5/subagents/lns84
tag=$1; s=$2; it=$3; tm=$4; shift 4
TIMEFORMAT='cpu_user=%U wall=%R'
{ time timeout $((tm+120)) nice -n 5 ./lns --tours "$@" --xover 0 --seed $s --t0 40 --budget 400000 --focus 1 137 0.85 \
   --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters $it --time $tm \
   --out tmp/${tag}_s${s}_best.txt > tmp/${tag}_s${s}.log 2>&1 ; } 2>> tmp/${tag}_s${s}.log
