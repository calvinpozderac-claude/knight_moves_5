#!/bin/bash
# after queue3: group-size reward YG (gmax > 37 earns 100/point) from 9,275, then plain LNS on its best gmax>=38 tour
cd /home/user/knight_moves_5/subagents/graft92
while kill -0 6491 2>/dev/null; do sleep 15; done
EXTRA="--gw 100 --gref 37 --grange 40 140" LT=900 ./ylns.sh YG 921 400000 in/record_9275.txt 0 "" in/record_9275.txt in/record_9271.txt in/record_9268.txt
f=y/YG_s921_best.txt.feas_best.txt
if [ -f $f ]; then cp $f in/YG.txt; python3 sig.py in/YG.txt >> events.log; LT=1500 ./lns.sh LYG 922 600000 in/YG.txt 1 180 0.0 in/YG.txt; fi
echo "$(date +%T) queue4 done" >> events.log
