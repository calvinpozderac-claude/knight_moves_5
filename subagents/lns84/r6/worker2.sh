#!/bin/bash
# lineage T whose polish was still improving: polish 600 s more (Q<T>), then exact + 3 LNS seeds from it
cd /home/user/knight_moves_5/subagents/lns84
t=$1; shift; seeds="$@"
./r6/pol.sh Q$t r6/P${t}_best.txt 600 12
cp r6/Q${t}_best.txt r6/S${t}.txt
./r6/exact.sh r6/S${t}.txt $t 150
D="r6/record_8537.txt r6/record_8545.txt r6/record_8549.txt r6/record_8553.txt r6/P8537_best.txt r6/P8545_best.txt r6/P8549_best.txt r6/P8553_best.txt r6/record_8605.txt tmp/keep_8605_LDs16.txt tmp/keep_8605_PLIs63.txt"
for s in $seeds; do ./r6/lns.sh L$t $s 30000 r6/S${t}.txt r6/focus_$t.txt $D; done
