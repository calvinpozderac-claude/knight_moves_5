#!/bin/bash
# beamcells.sh: T-ranked dance block beam (W 20000) from the dance start (and ds+-4) of every arch/cells tour -> runs/beam_<cell>_best.txt
cd "$(dirname "$0")"
tail -n +2 arch/families.tsv | while IFS=$'\t' read T lv first cl ds jac nsit src cell; do
  b=$(basename $cell .txt); best=0
  for k in $((ds-4)) $ds $((ds+4)); do
    r=$(BBT=1 BLOCKBEAM=20000 BBK=$k timeout 300 nice -n 5 ./bb --tours $cell --start $cell --xover 0 --free 0 --out runs/beam_${b}_k${k}_best.txt 2>/dev/null | grep "^BLOCKBEAM" | grep -o "best [0-9-]*" | cut -d" " -f2)
    echo "$b T $T ds $ds k $k beam $r"
  done
done
