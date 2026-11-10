#!/bin/bash
# watch run/*_best.txt; any tour above 8609 that the checker accepts -> NEW_RECORD_<T>.txt
cd /home/user/knight_moves_5
D=subagents/polish63
while true; do
  for f in $D/run/*_best.txt; do
    [ -f "$f" ] || continue
    T=$(head -1 "$f"); [[ "$T" =~ ^[0-9]+$ ]] || continue
    if [ "$T" -gt 8609 ] && [ ! -f $D/NEW_RECORD_$T.txt ]; then
      cp "$f" $D/run/cand.txt; T=$(head -1 $D/run/cand.txt)
      out=$(python3 tools/check_tour.py "$(sed -n 2p $D/run/cand.txt)")
      echo "$(date +%T) $f $T $out" >> $D/run/saver.log
      if echo "$out" | grep -q "ok=True" && echo "$out" | grep -q "unnecessary=\[\]" && echo "$out" | grep -Eq "maxvisits=[123] "; then
        cp $D/run/cand.txt $D/NEW_RECORD_$T.txt
      fi
    fi
  done
  sleep 20
done
