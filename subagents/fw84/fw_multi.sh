#!/bin/bash
# Free-wait pass, several rngs per seed (copy of prior/fw_multi.sh, paths for fw84). Run from subagents/fw84.
# usage: bash fw_multi.sh T "RNGS" UNTIL TAG AMIN AMAX SEED_FILE...
T=$1; RNGS=$2; UNTIL=$3; TAG=$4; AMIN=$5; AMAX=$6; shift 6
mkdir -p tmp/$TAG
for f in "$@"; do for r in $RNGS; do
  [ $(date +%s) -ge $UNTIL ] && exit 0
  id=$(basename $f .txt)_r$r
  timeout $((T + 150)) nice -n 5 ./tmp/fnrpa --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time $T --amin $AMIN --amax $AMAX --rng $r \
    --seed $f --out tmp/$TAG/fw_$id.txt --pool tmp/$TAG/fw_pool.txt --pool-min 8300 > tmp/$TAG/fw_$id.log 2>&1
  echo "$id: $(head -1 $f) -> $(head -1 tmp/$TAG/fw_$id.txt)" >> tmp/$TAG/fw_summary.txt
  python3 newrec.py tmp/$TAG/fw_pool.txt >> tmp/$TAG/records.txt
done; done
