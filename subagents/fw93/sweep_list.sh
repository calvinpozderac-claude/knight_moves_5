#!/bin/bash
# sweep93.py (3 rounds) over the inputs listed in LIST, worker ID of N. usage: bash sweep_list.sh LIST ID N
k=0
for f in $(cat $1); do k=$((k+1)); [ $(( (k-1) % $3 )) -ne $2 ] && continue
  h=$(basename $f .txt)
  timeout 900 nice -n 5 python3 sweep93.py $f tmp/sw/$h 3 > tmp/sw/$h.log 2>&1
  echo "$h: $(grep -h '^round\|no accepted' tmp/sw/$h.log | tr '\n' ' ')" >> tmp/sw/summary.txt
done
