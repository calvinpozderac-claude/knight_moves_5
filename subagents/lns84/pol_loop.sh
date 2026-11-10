#!/bin/bash
# polish every new best NEW_RECORD (>= current max) once; lock-free pick via mkdir
cd /home/user/knight_moves_5/subagents/lns84
tag=$1; tm=$2; n=0
while true; do
  best=$(for g in NEW_RECORD_*.txt; do echo "$(head -1 $g) $g"; done | sort -rn | head -1 | awk '{print $2}')
  T=$(head -1 $best)
  if mkdir tmp/polished_$T 2>/dev/null; then n=$((n+1)); ./pol_one.sh P${tag}_$T $best $tm; else sleep 30; fi
done
