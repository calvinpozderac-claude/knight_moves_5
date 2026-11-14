#!/bin/bash
# after queue7: junction-focused plain LNS on X9282 (focus 40-110 p 0.5), donors both 9,282s + 9,279 + 9,275
cd /home/user/knight_moves_5/subagents/graft92
while pgrep -f queue7.sh > /dev/null; do sleep 10; done
LT=1200 TMAX=1300 ./lns.sh L82f 981 900000 in/X9282.txt 40 110 0.5 in/X9282.txt in/P9282.txt in/record_9279.txt in/record_9275.txt
echo "$(date +%T) queue8 done" >> events.log
