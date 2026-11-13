#!/bin/bash
# sequential queue (1 thread): graft penalty YA -> plain LNS on its feasible best; group reward YG -> plain LNS
cd /home/user/knight_moves_5/subagents/graft92
while pgrep -f "G1_s901_best" > /dev/null; do sleep 20; done
D="in/record_9080.txt in/record_9275.txt in/record_9271.txt in/record_9268.txt"
EXTRA="--sitmin 20" LT=1200 ./ylns.sh YA 911 400000 in/record_9275.txt 150 "f3,g3,e3,e4:60:96 b6,c6,d5,d6:96:400" $D
f=y/YA_s911_best.txt.feas_best.txt
if [ -f $f ]; then cp $f in/YA.txt; python3 sig.py in/YA.txt >> events.log; LT=1500 ./lns.sh LYA 912 400000 in/YA.txt 1 180 0.5 in/YA.txt; fi
EXTRA="--gw 100 --gref 37 --grange 40 140" LT=900 ./ylns.sh YG 921 400000 in/record_9275.txt 0 "" in/record_9275.txt in/record_9271.txt in/record_9268.txt
f=y/YG_s921_best.txt.feas_best.txt
if [ -f $f ]; then cp $f in/YG.txt; python3 sig.py in/YG.txt >> events.log; LT=1500 ./lns.sh LYG 922 400000 in/YG.txt 1 180 0.5 in/YG.txt; fi
echo "$(date +%T) queue1 done" >> events.log
