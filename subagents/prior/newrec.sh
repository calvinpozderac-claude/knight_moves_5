#!/bin/bash
# usage: bash newrec.sh FILE...  (files with "minutes<TAB>tour" lines or 2-line tours). Any tour above the best
# record (data/tours/record_*.txt and NEW_RECORD_*.txt here) that the checker accepts -> NEW_RECORD_<m>.txt
best=$( (for f in ../../data/tours/record_*.txt NEW_RECORD_*.txt; do head -1 $f; done) | sed 's/\..*//' | sort -n | tail -1)
cat "$@" 2>/dev/null | python3 -c "
import sys,re
lines=sys.stdin.read().split('\n'); m=None
for l in lines:
    if '(' in l:
        if '\t' in l: a,t=l.split('\t',1); print(int(float(a)),t,sep='\t')
        elif m is not None: print(m,l,sep='\t')
    else:
        try: m=int(float(l))
        except: m=None
" | sort -t$'\t' -k1,1nr | awk -F'\t' -v b=$best '$1>b' | awk -F'\t' '!s[$2]++' | while IFS=$'\t' read m t; do
  if python3 ../../tools/check_tour.py "$t" | grep -q "ok=True.*unnecessary=\[\]"; then
    printf '%s\n%s\n' "$m" "$t" > NEW_RECORD_$m.txt; cp NEW_RECORD_$m.txt best.txt; echo "NEW RECORD $m"; break
  fi
done
