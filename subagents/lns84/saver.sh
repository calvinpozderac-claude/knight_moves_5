#!/bin/bash
# watch candidate files; verify with the exact checker; write NEW_RECORD_<T>.txt for anything > 8484 that passes
cd /home/user/knight_moves_5
D=subagents/lns84
while true; do
  for f in $D/tmp/*_best.txt; do
    [ -f "$f" ] || continue
    T=$(head -1 "$f"); [ -n "$T" ] || continue
    MAXT=$(cat $D/NEW_RECORD_*.txt 2>/dev/null | awk 'NR%2==1' | sort -n | tail -1)
    if [ "$T" -gt "$MAXT" ] && [ ! -f $D/NEW_RECORD_$T.txt ]; then
      cp "$f" $D/tmp/cand.txt; T=$(head -1 $D/tmp/cand.txt)
      out=$(python3 tools/check_tour.py "$(sed -n 2p $D/tmp/cand.txt)")
      echo "$(date +%T) $f $T $out" >> $D/tmp/saver.log
      if echo "$out" | grep -q "ok=True" && echo "$out" | grep -q "unnecessary=\[\]" && echo "$out" | grep -Eq "maxvisits=[123] "; then
        cp $D/tmp/cand.txt $D/NEW_RECORD_$T.txt
      fi
    fi
  done
  sleep 20
done
