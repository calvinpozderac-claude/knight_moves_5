#!/bin/bash
# polish92_saver.sh: any pol/*_best.txt, lns/*_best.txt, keep/*.txt above THR accepted by check_tour -> NEW_RECORD_<m>.txt
D=/home/user/knight_moves_5/subagents/polish92; THR=${THR:-9275}
cd $D
while true; do
  for f in pol/*_best.txt lns/*_best.txt lns/*_final.txt pol/*_final.txt keep/*.txt; do
    [ -f "$f" ] || continue
    m=$(head -1 "$f" | tr -dc '0-9'); [ -n "$m" ] || continue
    [ "$m" -gt "$THR" ] || continue
    [ -f NEW_RECORD_$m.txt ] && continue
    t=$(sed -n 2p "$f")
    r=$(cd /home/user/knight_moves_5/tools && python3 check_tour.py "$t")
    if echo "$r" | grep -q "ok=True" && echo "$r" | grep -q "unnecessary=\[\]" && echo "$r" | grep -qE "maxvisits=[123] "; then
      T=$(echo "$r" | grep -o ' T=[0-9]*' | tr -dc '0-9')
      if [ "$T" -gt "$THR" ] && [ ! -f NEW_RECORD_$T.txt ]; then
        printf "%s\n%s\n" "$T" "$t" > NEW_RECORD_$T.txt
        echo "$(date +%T) SAVER wrote NEW_RECORD_$T.txt from $f :: $r" >> events.log
      fi
    fi
  done
  date +%T > saver.alive
  sleep 30
done
