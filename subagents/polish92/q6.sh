#!/bin/bash
# after q5: two more seeds from the best 9,279-line tour: s934 with the wider 9,275-line donor set, s935 dance-only focus 40..154
cd /home/user/knight_moves_5/subagents/polish92
best() { for f in don2/*.txt; do echo "$(head -1 $f) $f"; done | sort -n | tail -1 | cut -d' ' -f2; }
for spec in "934 1 154 W" "935 40 154 N"; do
  set -- $spec; s=$1; lo=$2; hi=$3
  st=$(best); b0=$(head -1 $st); [ "$(head -1 don2/P9279.txt)" -ge "$b0" ] && st=don2/P9279.txt
  if [ $4 = W ]; then D="don2/*.txt don/r9271.txt don/r9268.txt don/pen9275_*.txt don/L_s926_9275.txt"; else D="don2/*.txt"; fi
  ./lns.sh M $s 60000 $st $lo $hi 0.85 $D
  r=lns/M_s${s}_best.txt
  if [ -f $r ]; then b1=$(head -1 $r); python3 ../../tools/check_tour.py "$(sed -n 2p $r)" >> events.log
    cp $r don2/M_s${s}_$b1.txt
    if [ "$b1" -gt "$b0" ]; then ./pol.sh PM_s$s $r 600 12; cp pol/PM_s${s}_best.txt don2/PM_s${s}.txt; fi
  fi
done
echo "$(date +%T) q6 done" >> events.log
