#!/bin/bash
# Free-wait pass worker (fnrpa warm 10, level 2, free sinks 1-4, anchors AMIN..AMAX). Run from subagents/fw93.
# usage: bash fw_jobs.sh JOBFILE ID N UNTIL TAG AMIN AMAX   (JOBFILE lines: "SEEDFILE RNG T"; worker ID takes every N-th line)
J=$1; ID=$2; N=$3; UNTIL=$4; TAG=$5; AMIN=$6; AMAX=$7
mkdir -p tmp/$TAG
k=0
while read f r T; do
  k=$((k+1)); [ $(( (k-1) % N )) -ne $ID ] && continue
  [ $(date +%s) -ge $UNTIL ] && exit 0
  id=$(basename $f .txt)_r$r
  timeout $((T + 120)) nice -n 5 ./tmp/fnrpa --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time $T --amin $AMIN --amax $AMAX --rng $r \
    --seed $f --out tmp/$TAG/fw_$id.txt --pool tmp/$TAG/fw_pool_$ID.txt --pool-min 9200 > tmp/$TAG/fw_$id.log 2>&1
  echo "$id: $(head -1 $f) -> $(head -1 tmp/$TAG/fw_$id.txt)" >> tmp/$TAG/fw_summary.txt
done < $J
