#!/bin/bash
# Free-wait pass, 8 rngs each, over new distinct strict skeletons >= MIN from POOL that were never passed before
# (tmp/r4/passed.txt holds every skeleton already passed). Worker ID of N takes every N-th candidate.
# usage: bash fw4.sh ID N MIN UNTIL POOL...
ID=$1; N=$2; MIN=$3; UNTIL=$4; shift 4
cat "$@" | awk -F'\t' -v m=$MIN 'NF==2 && $1>=m' | sort -t$'\t' -k1,1nr | awk -F'\t' '!s[$2]++' > tmp/r4/cand_$ID.txt
k=0
while IFS=$'\t' read m t; do
  grep -qxF "$t" tmp/r4/passed.txt 2>/dev/null && continue
  k=$((k+1)); [ $(( (k-1) % N )) -ne $ID ] && continue
  f=tmp/r4/skel_b_${ID}_$k.txt; printf '%s\n%s\n' "$m" "$t" > $f
  bash fw_multi.sh 120 "$((1000+k*10+1)) $((1000+k*10+2)) $((1000+k*10+3)) $((1000+k*10+4)) $((1000+k*10+5)) $((1000+k*10+6)) $((1000+k*10+7)) $((1000+k*10+8))" $UNTIL $f
  [ $(date +%s) -ge $UNTIL ] && break
done < tmp/r4/cand_$ID.txt
