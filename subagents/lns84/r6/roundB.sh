#!/bin/bash
# round B worker: usage roundB.sh WAITPID STARTFILE SEED... ; waits for WAITPID, then LNS from STARTFILE with all r6 bests as donors
cd /home/user/knight_moves_5/subagents/lns84
w=$1; st=$2; shift 2
while kill -0 $w 2>/dev/null; do sleep 10; done
for s in "$@"; do
  mkdir -p r6/donB_$s; rm -f r6/donB_$s/*
  for f in r6/L*_best.txt r6/Q*_best.txt r6/R*_best.txt r6/P*_best.txt r6/record_8605.txt; do [ -f $f ] && cp $f r6/donB_$s/; done
  ls -t r6/*_best.txt.elite | head -3 | xargs -I{} cp {} r6/donB_$s/
  ./r6/lns.sh B $s 30000 $st r6/focus_8553.txt r6/donB_$s/*
done
