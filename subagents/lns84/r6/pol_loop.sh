#!/bin/bash
# polish every new best NEW_RECORD (>= 8606) once (600 s), output r6/R<T>_best.txt
cd /home/user/knight_moves_5/subagents/lns84
while true; do
  best=$(for g in NEW_RECORD_*.txt; do echo "$(head -1 $g) $g"; done | sort -rn | head -1 | awk '{print $2}')
  T=$(head -1 $best)
  if [ "$T" -gt 8605 ] && mkdir r6/polished_$T 2>/dev/null; then ./r6/pol.sh R$T $best 600 12; else sleep 30; fi
done
