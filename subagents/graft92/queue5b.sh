#!/bin/bash
# graft92 restart: queue5 = plain-LNS seeds on the H3 family (donors: H3 tours only), chained from the best
cd /home/user/knight_moves_5/subagents/graft92
LT=1200 TMAX=1300 ./lns.sh LH3b 952 900000 in/LH3best.txt 1 180 0.0 in/LH3best.txt in/LH3_9242.txt
echo "$(date +%T) queue5b seed1 done" >> events.log
