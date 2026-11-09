#!/bin/bash
cd /home/user/knight_moves_5/subagents/f5push
while kill -0 4961 2>/dev/null; do sleep 10; done
./exact.sh NEW_RECORD_8830.txt E8830 240
cp focus_E8830.txt focus_E8812.txt
./seedq.sh 60000 506
