#!/bin/bash
# graft92: any *_best.txt / feas_best / elite tour above THR (default 9282) that check_tour accepts -> NEW_RECORD_<T>.txt
cd /home/user/knight_moves_5
D=subagents/graft92
while true; do
  date +%T > $D/saver.alive
  for f in $D/lns/*_best.txt $D/lns/*feas_best.txt $D/y/*_best.txt $D/y/*feas_best.txt $D/x/*_best.txt; do
    [ -f "$f" ] || continue
    T=$(head -1 "$f"); [[ "$T" =~ ^[0-9]+$ ]] || continue
    if [ "$T" -gt ${THR:-9282} ] && [ ! -f $D/NEW_RECORD_$T.txt ] && ! grep -q "^$f $T " $D/saver.log 2>/dev/null; then
      cp "$f" $D/cand.txt; T=$(head -1 $D/cand.txt)
      out=$(python3 tools/check_tour.py "$(sed -n 2p $D/cand.txt)")
      echo "$f $T $(date +%T) $out" >> $D/saver.log
      if echo "$out" | grep -q "ok=True" && echo "$out" | grep -q "unnecessary=\[\]" && echo "$out" | grep -Eq "maxvisits=[123]( |$)"; then
        cp $D/cand.txt $D/NEW_RECORD_$T.txt
      fi
    fi
  done
  sleep 30
done
