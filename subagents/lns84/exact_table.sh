#!/bin/bash
# usage: exact_table.sh TOUR LOG TIME k1 k2 ...  sound exhaustive suffix optimum (pinned continuations); stops at first timeout
cd /home/user/knight_moves_5/subagents/lns84
f=$1; log=$2; T=$3; shift 3
for k in "$@"; do
  out=$(nice -n 5 timeout $((T+60)) ./bb $f $k --nobound --alpha 0 --tt 25 --time $T | grep -E "DONE|TIMEOUT|replay fail")
  echo "$(date +%T) k=$k :: $out" >> $log
  echo "$out" | grep -q DONE || break
done
