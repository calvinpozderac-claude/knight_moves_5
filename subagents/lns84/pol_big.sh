#!/bin/bash
# usage: pol_big.sh TAG INFILE TIME  (wider windows than pol_one.sh)
cd /home/user/knight_moves_5/subagents/lns84
tag=$1; in=$2; tm=$3
echo "$(date +%T) polish-big $tag start on $in ($(head -1 $in))" >> tmp/threads.log
TIMEFORMAT='cpu_user=%U wall=%R'
{ time timeout $((tm+120)) nice -n 5 ./polish --in $in --time $tm --threads 1 --wmax 16 --ext 4 --ext0 5 \
   --node-limit 6000000 --no-nrpa --pool-min 8000 --rng 7 \
   --out tmp/${tag}_best.txt --pool tmp/${tag}_pool.txt --polished tmp/${tag}_final.txt > tmp/${tag}.log 2>&1 ; } 2>> tmp/${tag}.log
echo "$(date +%T) polish-big $tag end: $(grep '^done' tmp/${tag}.log)" >> tmp/threads.log
