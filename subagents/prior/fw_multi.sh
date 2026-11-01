#!/bin/bash
# Free-wait pass with several rngs per strict skeleton (the pass is high-variance: the 8,295 skeleton gave
# 8,299 / 8,307 / 8,334 with different rngs).  usage: bash fw_multi.sh T "RNGS" UNTIL SKELETON_FILE...
T=$1; RNGS=$2; UNTIL=$3; shift 3
for f in "$@"; do for r in $RNGS; do
  [ $(date +%s) -ge $UNTIL ] && exit 0
  id=$(basename $f .txt)_r$r
  timeout $((T + 150)) nice -n 5 ./tmp/fnrpa --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time $T --amin 35 --amax 999 --rng $r \
    --seed $f --out tmp/r3c/fw_$id.txt --pool tmp/r3c/fw_pool.txt --pool-min 8250 > tmp/r3c/fw_$id.log 2>&1
  echo "$id: $(head -1 $f) -> $(head -1 tmp/r3c/fw_$id.txt)" >> tmp/r3c/fw_summary.txt
  bash newrec.sh tmp/r3c/fw_pool.txt >> tmp/r3c/records.txt
done; done
