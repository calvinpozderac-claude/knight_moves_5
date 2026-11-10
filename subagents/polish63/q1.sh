#!/bin/bash
cd /home/user/knight_moves_5/subagents/polish63
./pol.sh P8559 tmp/record_8559.txt 900 12
./pol.sh P8563 tmp/record_8563.txt 900 12
./exact.sh run/P8559_best.txt P8559 90
./exact.sh run/P8563_best.txt P8563 90
echo "$(date +%T) q1 done" >> run/events.log
