#!/bin/bash
# usage: r6/pol.sh TAG INFILE TIME [WMAX]   polish (free sinks kept), logs in r6/
cd /home/user/knight_moves_5/subagents/lns84
tag=$1; in=$2; tm=$3; w=${4:-12}
echo "$(date +%T) polish $tag start on $in ($(head -1 $in)) wmax $w" >> r6/events.log
TIMEFORMAT='cpu_user=%U wall=%R'
{ time timeout $((tm+120)) nice -n 5 ./polish --in $in --time $tm --threads 1 --wmax $w --ext 3 --ext0 4 \
   --node-limit 3000000 --nrpa-level 2 --nrpa-tails 45,60,90 --pool-min 8000 \
   --out r6/${tag}_best.txt --pool r6/${tag}_pool.txt --polished r6/${tag}_final.txt > r6/${tag}.log 2>&1 ; } 2>> r6/${tag}.log
echo "$(date +%T) polish $tag end: $(grep '^done' r6/${tag}.log)" >> r6/events.log
