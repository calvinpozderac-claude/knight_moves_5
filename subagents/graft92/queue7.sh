#!/bin/bash
# 9,279 round: plain LNS on X9282 (9,275 merge + 9,279 dance), then repaired splices 9,279 merge x other dances
cd /home/user/knight_moves_5/subagents/graft92
LT=1200 TMAX=1300 ./lns.sh L82 971 900000 in/X9282.txt 1 180 0.0 in/X9282.txt in/record_9279.txt in/record_9275.txt
python3 sig.py lns/L82_s971_best.txt | tail -1 >> events.log
for s in 9080_k89 9264_k80 8946_k90 9275_k70; do
  d=${s%_k*}; don=in/record_$d.txt; [ $d = 9264 ] && don=in/LH3best.txt
  LT=480 TMAX=540 ./lns.sh R79x$d 97${d: -1} 900000 in/S79_$s.txt 1 180 0.0 in/S79_$s.txt $don in/record_9279.txt
  python3 sig.py lns/R79x${d}_s97${d: -1}_best.txt | tail -1 >> events.log
done
echo "$(date +%T) queue7 done" >> events.log
