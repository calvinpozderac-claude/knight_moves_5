#!/bin/bash
# Free-wait pass (fractional's fnrpa, warm level 2, free sinks of 1-4 units) over pool tours.
# usage: bash fw_pass.sh CANDS(minutes<TAB>tour) SECONDS TAG    (run from subagents/prior)
C=$1; T=$2; tag=$3; i=0
while IFS=$'\t' read m t; do
  i=$((i+1)); printf '%s\n%s\n' "$m" "$t" > tmp/fw/seed_${tag}_$i.txt
  timeout $((T + 150)) nice -n 5 ./tmp/fnrpa --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time $T --amin 35 --amax 999 --rng $i \
    --seed tmp/fw/seed_${tag}_$i.txt --out tmp/fw/out_${tag}_$i.txt --pool tmp/fw/pool.txt --pool-min 8200 > tmp/fw/${tag}_$i.log 2>&1
  echo "$m -> $(head -1 tmp/fw/out_${tag}_$i.txt)"
done < $C
