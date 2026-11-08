#!/bin/bash
# sound exhaustive suffix optimum (strict/pinned moves only) for record_8484, from k down; stops at first timeout
cd /home/user/knight_moves_5/subagents/lns84
T=${1:-240}
for k in 156 152 148 146 144 142 140 138 136 134 132 130 128 126 124 122 120 118 116 114 112 110; do
  out=$(nice -n 5 timeout $((T+60)) ./bb tmp/record_8484.txt $k --nobound --alpha 0 --tt 25 --time $T | grep -E "DONE|TIMEOUT|FOUND [0-9]+ " | grep -v "^FOUND 8484")
  echo "$(date +%T) k=$k :: $out"
  echo "$out" | grep -q DONE || break
done
