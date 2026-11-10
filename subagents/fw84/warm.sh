#!/bin/bash
# Warm fnrpa runs from a config list. Each line: ID SEED T LEVEL WARM SC FMAX AMIN AMAX RNG. Run from subagents/fw84.
# usage: bash warm.sh CONFIGFILE TAG UNTIL
C=$1; TAG=$2; UNTIL=$3; mkdir -p tmp/$TAG
while read id seed T lev warm sc fmax amin amax rng; do
  [ -z "$id" ] && continue; [ "${id:0:1}" = "#" ] && continue
  [ $(date +%s) -ge $UNTIL ] && exit 0
  timeout $((T + 200)) nice -n 5 ./tmp/fnrpa --sc $sc --free 1 --fstep 1 --fmax $fmax --warm $warm --level $lev --time $T --amin $amin --amax $amax --rng $rng \
    --seed $seed --out tmp/$TAG/$id.txt --pool tmp/$TAG/pool.txt --pool-min 8400 > tmp/$TAG/$id.log 2>&1
  echo "$id $(basename $seed) T=$T L$lev warm$warm sc$sc fmax$fmax a$amin-$amax r$rng -> $(head -1 tmp/$TAG/$id.txt) | $(grep -o ' fin [0-9./]* min' tmp/$TAG/$id.log | sort | uniq -c | sort -nr | head -6 | awk '{printf "%sx%s ", $1, $3}') unfin=$(grep -c ' unfin ' tmp/$TAG/$id.log)" >> tmp/$TAG/summary.txt
  python3 newrec.py tmp/$TAG/pool.txt >> tmp/$TAG/records.txt
done < $C
