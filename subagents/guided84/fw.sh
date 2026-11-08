#!/bin/bash
# Free-wait pass (fnrpa warm L2, T s per rng, 8 rngs) over every distinct skeleton >= MIN in POOL files that is not
# 8,484 itself and was never passed (tmp/fw/passed.txt). Worker ID of N takes every N-th candidate. Stops at UNTIL (epoch s).
# usage: bash fw.sh ID N MIN T UNTIL POOL...
ID=$1; N=$2; MIN=$3; T=$4; UNTIL=$5; shift 5
R84=$(sed -n 2p ../../data/tours/record_8484.txt)
cat "$@" | awk -F'\t' -v m=$MIN 'NF==2 && $1>=m' | sort -t$'\t' -k1,1nr | awk -F'\t' '!s[$2]++' > tmp/fw/cand_$ID.txt
k=0
touch tmp/fw/passed.txt tmp/fw/summary.txt
while IFS=$'\t' read m t; do
  [ "$t" = "$R84" ] && continue
  k=$((k+1)); [ $(( (k-1) % N )) -ne $ID ] && continue
  grep -qxF "$t" tmp/fw/passed.txt 2>/dev/null && continue
  h=$(echo "$t" | md5sum | cut -c1-8); f=tmp/fw/skel_${m}_$h.txt; printf '%s\n%s\n' "$m" "$t" > $f
  for j in 1 2 3 4 5 6 7 8; do
    [ $(date +%s) -ge $UNTIL ] && exit 0
    r=$((5000 + (16#${h:0:3} % 400) * 10 + j)); [ $h = 6bcdc742 ] && r=$((5010 + j)); id=$(basename $f .txt)_r$r
    grep -q "^$id:" tmp/fw/summary.txt 2>/dev/null && continue
    timeout $((T + 120)) nice -n 5 ./fnrpa --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time $T --amin 35 --amax 999 --rng $r \
      --seed $f --out tmp/fw/fw_$id.txt --pool tmp/fw/fw_pool.txt --pool-min 8400 > tmp/fw/fw_$id.log 2>&1
    echo "$id: $m -> $(head -1 tmp/fw/fw_$id.txt)" >> tmp/fw/summary.txt
    bash newrec.sh tmp/fw/fw_pool.txt tmp/fw/fw_$id.txt >> tmp/fw/records.txt
  done
  echo "$t" >> tmp/fw/passed.txt
done < tmp/fw/cand_$ID.txt
