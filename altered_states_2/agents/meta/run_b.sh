#!/bin/bash
# second batch: memetic GA, then 4-square LNS climb from the top 10 runner-up boards
M=./agents/meta/meta; L=agents/meta/logs
$M ga --seconds 540 --seed 16 --pop 24 --pelite 0.5 --childiters 1e6 --ct0 1.5e6 --ct1 5e5 --seediters 2e6 --log $L/ga.csv > $L/ga.out
for g in $(head -10 agents/meta/elite.txt | cut -d, -f1); do
  $M lns $g --k 4 --patience 250 --seed 3 >> $L/lns4_elite.out
done
echo ALLDONE > $L/done_b
