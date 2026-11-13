#!/bin/bash
# queue: wait for P9275, polish 9271/9268 (600 s each), then exact tail proofs on the polished bests
cd /home/user/knight_moves_5/subagents/polish92
while pgrep -f "pol/P9275_best" >/dev/null; do sleep 10; done
./pol.sh P9271 in/record_9271.txt 600 12
./pol.sh P9268 in/record_9268.txt 600 12
./exact.sh pol/P9275_best.txt E9275 240
./exact.sh pol/P9271_best.txt E9271 150
./exact.sh pol/P9268_best.txt E9268 150
echo "$(date +%T) q1 done" >> events.log
