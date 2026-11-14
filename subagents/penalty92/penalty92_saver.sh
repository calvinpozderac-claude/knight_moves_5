#!/bin/bash
# any tour file above 9282 that check_tour accepts -> NEW_RECORD_<T>.txt (penalty92)
cd /home/user/knight_moves_5
D=subagents/penalty92
end=$(( $(date +%s) + ${1:-3500} ))
while [ $(date +%s) -lt $end ]; do
  for f in $D/y/*_best.txt $D/y/*feas_best.txt $D/lns/*_best.txt $D/pol/*_best.txt $D/pol/*_final.txt; do
    [ -f "$f" ] || continue
    T=$(head -1 "$f"); [[ "$T" =~ ^[0-9]+$ ]] || continue
    if [ "$T" -gt 9282 ] && [ ! -f $D/NEW_RECORD_$T.txt ]; then
      cp "$f" $D/cand.txt; T=$(head -1 $D/cand.txt)
      out=$(python3 tools/check_tour.py "$(sed -n 2p $D/cand.txt)")
      echo "$(date +%T) $f $T $out" >> $D/saver.log
      if echo "$out" | grep -q "ok=True" && echo "$out" | grep -q "unnecessary=\[\]" && echo "$out" | grep -Eq "maxvisits=[123]( |$)"; then
        cp $D/cand.txt $D/NEW_RECORD_$T.txt; cp $D/cand.txt $D/keep/NEW_${T}_$(md5sum $D/cand.txt|cut -c1-8).txt
      fi
    fi
  done
  sleep 30
done
