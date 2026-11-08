#!/bin/bash
# run from altered_states_2/
B=hmlfsoaioutsnrcregahoywin
O=agents/exact/out
./agents/exact/bb allsub $B 5 --span 3 --prog 2000 > $O/allsub5_span3_best.txt
# every 2x3 and 3x2 rectangle (6 squares) re-optimised exactly
for h in 2 3; do w=$((5-h)); w=$(( h==2 ? 3 : 2 ));
 for r in $(seq 0 $((5-h))); do for c in $(seq 0 $((5-w))); do
  cells=""; for i in $(seq 0 $((h-1))); do for j in $(seq 0 $((w-1))); do cells="$cells $(( (r+i)*5 + c+j ))"; done; done
  echo "rect ${h}x${w} at $r,$c: $(./agents/exact/bb region $B $cells)"
 done; done
done > $O/rect6_best.txt
