#!/bin/bash
# polish every distinct LNS best (>= 8605) that is not already a known tour (8609 / strict 8607 / polished start)
cd /home/user/knight_moves_5/subagents/polish63
until grep -q "q2 done" run/events.log; do sleep 10; done
known="$(for f in tmp/record_8609.txt tmp/strict_8607.txt run/P8559_best.txt; do sed -n 2p $f | md5sum | cut -c1-8; done)"
mkdir -p run/C
for f in run/L*_best.txt; do
  T=$(head -1 $f); [ "$T" -ge 8605 ] || continue
  h=$(sed -n 2p $f | md5sum | cut -c1-8)
  echo "$known" | grep -q $h && { echo "$(date +%T) skip $f ($T, $h known)" >> run/events.log; continue; }
  [ -f run/C/in_${T}_$h.txt ] && continue
  cp $f run/C/in_${T}_$h.txt; known="$known
$h"
done
for f in $(ls run/C/in_*.txt 2>/dev/null | sort -r); do
  b=$(basename $f .txt); ./pol.sh C_${b#in_} $f 420 12
done
echo "$(date +%T) q3 done" >> run/events.log
