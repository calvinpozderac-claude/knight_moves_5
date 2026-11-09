#!/bin/bash
# usage: pol.sh TAG INFILE TIME [WMAX]   polish (lns84 binary, free sinks kept); logs in pol/
cd /home/user/knight_moves_5/subagents/f5push
tag=$1; in=$2; tm=$3; w=${4:-12}
echo "$(date +%T) polish $tag start on $in ($(head -1 $in)) wmax $w" >> events.log
timeout $((tm+120)) nice -n 5 bin/polish --in $in --time $tm --threads 1 --wmax $w --ext 3 --ext0 4 \
   --node-limit 3000000 --nrpa-level 2 --nrpa-tails 45,60,90 --pool-min 8000 \
   --out pol/${tag}_best.txt --pool pol/${tag}_pool.txt --polished pol/${tag}_final.txt > pol/${tag}.log 2>&1
echo "$(date +%T) polish $tag end: $(head -1 pol/${tag}_best.txt) $(grep '^done' pol/${tag}.log)" >> events.log
