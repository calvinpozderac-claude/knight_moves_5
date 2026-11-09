#!/bin/bash
# usage: lns.sh TAG SEED ITERS START LO HI P DONOR...   (lns_r6 --start; inputs copied to lns/in_TAG_sSEED/)
cd /home/user/knight_moves_5/subagents/f5push
tag=$1; s=$2; it=$3; st=$4; LO=$5; HI=$6; P=$7; shift 7
d=lns/in_${tag}_s$s; mkdir -p $d; cp $st $d/start.txt; for f in "$@"; do cp $f $d/; done
echo "$(date +%T) lns $tag s$s start $(head -1 $st) focus $LO $HI $P" >> events.log
timeout ${TMAX:-3000} nice -n 5 bin/lns_r6 --tours $d/* --start $d/start.txt --xover 0 --seed $s --t0 ${T0:-40} --budget 400000 --focus $LO $HI $P \
   --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free ${FREE:-0} --iters $it --time ${LT:-2800} \
   --out lns/${tag}_s${s}_best.txt > lns/${tag}_s${s}.log 2>&1
echo "$(date +%T) lns $tag s$s end: $(head -1 lns/${tag}_s${s}_best.txt) $(grep -E '^done' lns/${tag}_s${s}.log)" >> events.log
