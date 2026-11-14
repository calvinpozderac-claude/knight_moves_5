#!/bin/bash
# after LH3: more plain-LNS seeds on the LH3 hybrid family (same-family donors only: LH3 best + LH3 elite), then YG
cd /home/user/knight_moves_5/subagents/graft92
while kill -0 6491 2>/dev/null; do sleep 15; done
cp lns/LH3_s951_best.txt in/LH3best.txt; python3 sig.py in/LH3best.txt >> events.log
awk 'NR<=6' lns/LH3_s951_best.txt.elite > in/LH3elite.txt
LT=1500 ./lns.sh LH3b 952 600000 in/LH3best.txt 1 180 0.0 in/LH3best.txt in/LH3elite.txt
B=lns/LH3b_s952_best.txt; [ -f $B ] && [ $(head -1 $B) -gt $(head -1 in/LH3best.txt) ] && cp $B in/LH3best2.txt || cp in/LH3best.txt in/LH3best2.txt
LT=1500 ./lns.sh LH3c 953 600000 in/LH3best2.txt 1 180 0.0 in/LH3best2.txt in/LH3elite.txt
echo "$(date +%T) queue5 done" >> events.log
