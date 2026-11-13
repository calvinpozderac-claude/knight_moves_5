#!/bin/bash
# after queue2: LNS repair of the 9,275-merge + 9,080-dance splice (7,354)
cd /home/user/knight_moves_5/subagents/graft92
while kill -0 6381 2>/dev/null; do sleep 15; done
LT=1500 ./lns.sh LH3 951 600000 in/H3_7354.txt 1 180 0.0 in/H3_7354.txt in/record_9080.txt in/record_9275.txt
echo "$(date +%T) queue3 done" >> events.log
