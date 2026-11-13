#!/bin/bash
# usage: exact.sh TOUR TAG TIME [STEP] : sound tail proof (bb, strict continuations) from the end backwards,
# stops at first timeout; writes focus_TAG.txt = "1 k-1" for the earliest proven k. log: exact.log
cd /home/user/knight_moves_5/subagents/polish92
f=$1; tag=$2; T=$3; st=${4:-2}; n=$(sed -n 2p $f | grep -o '(' | wc -l); k=$((n-4)); last=$n
while [ $k -gt 60 ]; do
  out=$(nice -n 5 timeout $((T+60)) bin/bb $f $k --nobound --alpha 0 --tt 25 --time $T | grep -E "DONE|TIMEOUT|replay fail|FOUND")
  echo "$(date +%T) $tag k=$k/$n :: $(echo $out | tr '\n' ' ' | cut -c1-200)" >> exact.log
  echo "$out" | grep -q DONE || break
  last=$k; k=$((k-st))
done
echo "1 $((last-1))" > focus_$tag.txt
echo "$(date +%T) exact $tag: proven from move $last of $n" >> events.log
