#!/bin/bash
cd /home/user/knight_moves_5
while kill -0 15785 2>/dev/null; do sleep 5; done
timeout 1150 nice -n 5 subagents/nrpa/gnrpa --level 4 --threads 1 --time 1080 --rng 101 --seed-tours subagents/lns/NEW_RECORD_8130.txt --anchor-min 60 --anchor-max 60 --seed-adapt 0 --no-warm --bsink4 3 --pool subagents/nrpa/tmp/pool8130.txt --pool-min 8000 --out subagents/nrpa/tmp/l4_best.txt > subagents/nrpa/tmp/l4.log 2>&1
