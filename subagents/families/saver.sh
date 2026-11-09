#!/bin/bash
# any *_best.txt / pool tour above 8812 that check_tour accepts -> NEW_RECORD_<T>.txt
cd /home/user/knight_moves_5
D=subagents/families
while true; do
  for f in $D/pol/*_best.txt $D/lns/*_best.txt $D/nr/*_best.txt $D/x/*_best.txt $D/x/*feas_best.txt $D/y/*_best.txt $D/y/*feas_best.txt; do
    [ -f "$f" ] || continue
    T=$(head -1 "$f"); [[ "$T" =~ ^[0-9]+$ ]] || continue
    if [ "$T" -gt 8812 ] && [ ! -f $D/NEW_RECORD_$T.txt ]; then
      cp "$f" $D/cand.txt; T=$(head -1 $D/cand.txt)
      out=$(python3 tools/check_tour.py "$(sed -n 2p $D/cand.txt)")
      echo "$(date +%T) $f $T $out" >> $D/saver.log
      if echo "$out" | grep -q "ok=True" && echo "$out" | grep -q "unnecessary=\[\]" && echo "$out" | grep -Eq "maxvisits=[123]( |$)"; then
        cp $D/cand.txt $D/NEW_RECORD_$T.txt
      fi
    fi
  done
  sleep 30
done
