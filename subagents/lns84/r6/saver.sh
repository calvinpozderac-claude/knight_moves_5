#!/bin/bash
# watch r6/*_best.txt; any tour above the best NEW_RECORD so far (>= 8605) that the checker accepts -> NEW_RECORD_<T>.txt
cd /home/user/knight_moves_5
D=subagents/lns84
while true; do
  for f in $D/r6/*_best.txt; do
    [ -f "$f" ] || continue
    T=$(head -1 "$f"); [[ "$T" =~ ^[0-9]+$ ]] || continue
    MAXT=$(cat $D/NEW_RECORD_*.txt 2>/dev/null | awk 'NR%2==1' | sort -n | tail -1)
    if [ "$T" -gt "$MAXT" ] && [ ! -f $D/NEW_RECORD_$T.txt ]; then
      cp "$f" $D/r6/cand.txt; T=$(head -1 $D/r6/cand.txt)
      out=$(python3 tools/check_tour.py "$(sed -n 2p $D/r6/cand.txt)")
      echo "$(date +%T) $f $T $out" >> $D/r6/saver.log
      if echo "$out" | grep -q "ok=True" && echo "$out" | grep -q "unnecessary=\[\]" && echo "$out" | grep -Eq "maxvisits=[123] "; then
        cp $D/r6/cand.txt $D/NEW_RECORD_$T.txt; cp $D/r6/cand.txt $D/r6/NEW_RECORD_$T.txt
      fi
    fi
  done
  sleep 20
done
