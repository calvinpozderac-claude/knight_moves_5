#!/bin/bash
# after queue5b seed1: repaired splices (9,275 merge prefix + other dances, plain LNS repair, 7 min each),
# then H3 seeds 2 and 3 (20 min each, chained from the best H3 tour)
cd /home/user/knight_moves_5/subagents/graft92
while kill -0 1681 2>/dev/null; do sleep 10; done
for s in 8830_k93 8813_k101 8887_k107 8842_k94 8946_k102; do
  d=${s%_k*}
  LT=420 TMAX=480 ./lns.sh R$d 96${d: -1} 900000 in/S_$s.txt 1 180 0.0 in/S_$s.txt in/record_$d.txt in/record_9275.txt
  python3 sig.py lns/R${d}_s96${d: -1}_best.txt | tail -1 >> events.log
done
best() { for f in in/LH3best.txt lns/LH3*_best.txt; do echo "$(head -1 $f) $f"; done | sort -nr | head -1 | cut -d' ' -f2; }
B=$(best); cp $B in/LH3best2.txt
LT=1200 TMAX=1300 ./lns.sh LH3c 953 900000 in/LH3best2.txt 1 180 0.0 in/LH3best2.txt in/LH3best.txt in/LH3_9242.txt
B=$(best); cp $B in/LH3best3.txt
LT=1200 TMAX=1300 ./lns.sh LH3d 954 900000 in/LH3best3.txt 1 180 0.0 in/LH3best3.txt in/LH3best.txt in/LH3_9242.txt
echo "$(date +%T) queue6 done" >> events.log
