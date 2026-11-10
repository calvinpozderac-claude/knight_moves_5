#!/bin/bash
# usage: r6/exact.sh TOUR TAG TIME : sound tail proof from the end backwards (step 2), stops at first timeout;
# writes r6/focus_TAG.txt = "1 k-1 0.85" for the earliest proven k
cd /home/user/knight_moves_5/subagents/lns84
f=$1; tag=$2; T=$3; n=$(sed -n 2p $f | grep -o '(' | wc -l); k=$((n-4)); last=$n
while [ $k -gt 60 ]; do
  out=$(nice -n 5 timeout $((T+60)) ./bb $f $k --nobound --alpha 0 --tt 25 --time $T | grep -E "DONE|TIMEOUT|replay fail")
  echo "$(date +%T) $tag k=$k/$n :: $out" >> r6/exact.log
  echo "$out" | grep -q DONE || break
  last=$k; k=$((k-2))
done
echo "1 $((last-1)) 0.85" > r6/focus_$tag.txt
echo "$(date +%T) exact $tag: proven from move $last of $n" >> r6/events.log
