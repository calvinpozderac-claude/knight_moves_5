#!/bin/bash
# after round B: polish every distinct tour >= 8608 among r6 L*/B* bests (600 s, wmax 12), 3 in parallel
cd /home/user/knight_moves_5/subagents/lns84
# (no wait)
mkdir -p r6/C; declare -A seen; n=0
for f in r6/L*_best.txt r6/B*_best.txt; do
  T=$(head -1 $f); [ "$T" -ge 8608 ] || continue
  h=$(sed -n 2p $f | md5sum | cut -c1-8); [ -n "${seen[$h]}" ] && continue; seen[$h]=1
  cp $f r6/C/in_${T}_$h.txt; n=$((n+1))
done
echo "$(date +%T) sweepC: $n distinct tours >= 8608" >> r6/events.log
ls r6/C/in_*.txt | xargs -P 3 -I{} bash -c 'b=$(basename {} .txt); ./r6/pol.sh C_${b#in_} {} 600 12'
echo "$(date +%T) sweepC end" >> r6/events.log
