#!/bin/bash
# usage: pol_one.sh TAG INFILE TIME   (wide windows; free sinks of the input are kept)
cd /home/user/knight_moves_5/subagents/lns84
tag=$1; in=$2; tm=$3
echo "$(date +%T) polish $tag start on $in ($(head -1 $in))" >> tmp/threads.log
TIMEFORMAT='cpu_user=%U wall=%R'
{ time timeout $((tm+120)) nice -n 5 ./polish --in $in --time $tm --threads 1 --wmax 12 --ext 3 --ext0 4 \
   --node-limit 3000000 --nrpa-level 2 --nrpa-tails 45,60,90 --pool-min 8000 \
   --out tmp/${tag}_best.txt --pool tmp/${tag}_pool.txt --polished tmp/${tag}_final.txt > tmp/${tag}.log 2>&1 ; } 2>> tmp/${tag}.log
echo "$(date +%T) polish $tag end: $(grep '^done' tmp/${tag}.log)" >> tmp/threads.log
