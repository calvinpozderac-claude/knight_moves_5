#!/bin/bash
# Free-wait pass over every not-yet-processed distinct pool tour >= 8,200 (best first), T seconds each,
# until UNTIL (epoch seconds).   usage: bash r3_fw.sh T UNTIL POOL...
T=$1; UNTIL=$2; shift 2; mkdir -p tmp/fw3; touch tmp/fw3/done.txt
# tmp/fw3/extra.txt (minutes<TAB>tour, any minutes) goes first, e.g. the best outside-basin tour
(cat tmp/fw3/extra.txt 2>/dev/null; cat "$@" 2>/dev/null | awk -F'\t' 'NF==2 && $1>=8200' | sort -t$'\t' -k1,1nr) | awk -F'\t' '!s[$2]++' > tmp/fw3/cands_$$.txt
i=0; P=$$
while IFS=$'\t' read m t; do
  [ $(date +%s) -ge $UNTIL ] && break
  grep -qxF "$t" tmp/fw3/done.txt && continue
  echo "$t" >> tmp/fw3/done.txt  # claim it (two passes may run at once)
  i=$((i+1)); id=${P}_$i; printf '%s\n%s\n' "$m" "$t" > tmp/fw3/seed_$id.txt
  timeout $((T + 150)) nice -n 5 ./tmp/fnrpa --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time $T --amin 35 --amax 999 --rng $i \
    --seed tmp/fw3/seed_$id.txt --out tmp/fw3/out_$id.txt --pool tmp/fw3/pool.txt --pool-min 8200 > tmp/fw3/$id.log 2>&1
  echo "$id: $m -> $(head -1 tmp/fw3/out_$id.txt)" >> tmp/fw3/summary.txt
done < tmp/fw3/cands_$$.txt
