#!/bin/bash
# usage: dlns.sh TAG SEED ITERS START FORBID PEN FFROM DONOR...   diversity-forced LNS (bin/dlns), logs in x/
cd /home/user/knight_moves_5/subagents/families
tag=$1; s=$2; it=$3; st=$4; fb=$5; pen=$6; ff=$7; shift 7
echo "$(date +%T) dlns $tag s$s start $(head -1 $st) forbid $fb pen $pen from $ff" >> events.log
timeout ${TMAX:-3000} nice -n 5 bin/dlns --tours "$@" --start $st --xover 0 --seed $s --t0 ${T0:-40} --budget 400000 \
   --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters $it --time ${LT:-2700} \
   --forbid $fb --pen $pen --ffrom $ff --out x/${tag}_s${s}_best.txt > x/${tag}_s${s}.log 2>&1
echo "$(date +%T) dlns $tag s$s end: feas $(head -1 x/${tag}_s${s}_best.txt.feas_best.txt 2>/dev/null) $(grep -E '^done' x/${tag}_s${s}.log)" >> events.log
