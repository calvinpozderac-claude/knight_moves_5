#!/bin/bash
# usage: lns_thread.sh TAG ITERS SEED...   each seed: inputs = current best NEW_RECORDs + polish bests + 8390 + recent elites
cd /home/user/knight_moves_5/subagents/lns84
tag=$1; it=$2; shift 2
for s in "$@"; do
  d=tmp/in_${tag}_s$s; mkdir -p $d
  i=0; for f in $(for g in NEW_RECORD_*.txt; do echo "$(head -1 $g) $g"; done | sort -rn | head -6 | awk '{print $2}'); do cp $f $d/r$i.txt; i=$((i+1)); done
  for f in tmp/P*_best.txt tmp/pol37_best.txt; do [ -f $f ] && cp $f $d/$(basename $f); done
  for f in $(ls -t tmp/*_best.txt.elite 2>/dev/null | head -3); do cp $f $d/$(basename $f); done
  read LO HI P < tmp/focus.txt
  echo "$(date +%T) $tag s$s focus $LO $HI $P inputs: $(ls $d | tr '\n' ' ')" >> tmp/threads.log
  TIMEFORMAT='cpu_user=%U wall=%R'
  { time timeout 3000 nice -n 5 ./lns --tours $d/* --xover 0 --seed $s --t0 40 --budget 400000 --focus $LO $HI $P \
     --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters $it --time 2800 \
     --out tmp/${tag}_s${s}_best.txt > tmp/${tag}_s${s}.log 2>&1 ; } 2>> tmp/${tag}_s${s}.log
  echo "$(date +%T) $tag s$s end: $(grep -E '^done' tmp/${tag}_s${s}.log)" >> tmp/threads.log
done
