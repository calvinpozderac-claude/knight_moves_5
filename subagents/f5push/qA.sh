#!/bin/bash
cd /home/user/knight_moves_5/subagents/f5push
./exact.sh in/8810_e28e09e8.txt E8810 240
cp pol/P8595_best.txt in/p8595.txt
LT=900 TMAX=960 ./lns.sh S8595 601 70000 in/p8595.txt 1 135 0.85 in/r8595.txt pol/P8595_pool.txt pol/P8595_final.txt
./seedq.sh 60000 502 504 506
