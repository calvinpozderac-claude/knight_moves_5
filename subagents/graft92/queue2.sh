#!/bin/bash
# sequential queue (1 thread): plain LNS on the splice hybrids (9,275 merge + 8,900 / 8,946 dance)
cd /home/user/knight_moves_5/subagents/graft92
LT=1500 ./lns.sh LH2 931 600000 in/H2_9212.txt 1 180 0.0 in/H2_9212.txt in/record_8900.txt
LT=1200 ./lns.sh LH1 941 600000 in/H1_9266.txt 1 180 0.0 in/H1_9266.txt in/record_8946.txt
echo "$(date +%T) queue2 done" >> events.log
