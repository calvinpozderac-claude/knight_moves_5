#!/bin/bash
# watch candidate files; verify with the exact checker; keep best.txt / NEW_RECORD_<T>.txt up to date
cd /home/user/knight_moves_5
while true; do
  for f in subagents/lns/tmp/run*_best.txt; do
    [ -f "$f" ] || continue
    T=$(head -1 "$f"); B=$(head -1 subagents/lns/best.txt)
    if [ "$T" -gt "$B" ]; then
      cp "$f" subagents/lns/tmp/cand.txt
      T=$(head -1 subagents/lns/tmp/cand.txt)
      out=$(python3 tools/check_tour.py "$(sed -n 2p subagents/lns/tmp/cand.txt)")
      echo "$(date +%T) $T $out"
      if echo "$out" | grep -q "ok=True" && echo "$out" | grep -q "unnecessary=\[\]" && echo "$out" | grep -Eq "maxvisits=[123] "; then
        cp subagents/lns/tmp/cand.txt subagents/lns/best.txt
        rm -f subagents/lns/NEW_RECORD_*.txt; cp subagents/lns/tmp/cand.txt subagents/lns/NEW_RECORD_$T.txt
      fi
    fi
  done
  sleep 20
done
