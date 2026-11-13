#!/bin/bash
# usage: ylns.sh TAG SEED ITERS START PEN "RULE RULE ..." DONOR...   penalised LNS (bin/graft, --free 0); RULE="sq,sq:FROM:TO"; EXTRA env for --gw etc; logs in y/
cd /home/user/knight_moves_5/subagents/graft92
tag=$1; s=$2; it=$3; st=$4; pen=$5; rules=$6; shift 6
ra=""; for r in $rules; do ra="$ra --rule $r"; done
echo "$(date +%T) ylns $tag s$s start $st ($(head -1 $st)) pen $pen rules $rules extra $EXTRA donors $*" >> events.log
timeout ${TMAX:-3300} nice -n 5 bin/graft --tours "$@" --start $st --xover 0 --seed $s --t0 ${T0:-40} --budget 400000 \
   --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 0 --iters $it --time ${LT:-1200} \
   --pen $pen $ra $EXTRA --out y/${tag}_s${s}_best.txt > y/${tag}_s${s}.log 2>&1
echo "$(date +%T) ylns $tag s$s end: feas $(head -1 y/${tag}_s${s}_best.txt.feas_best.txt 2>/dev/null) $(grep -E '^done' y/${tag}_s${s}.log)" >> events.log
