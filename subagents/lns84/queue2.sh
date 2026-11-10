#!/bin/bash
# phase 2: LNS from the best polished tour (8526), 3 deterministic seeds, donors = polish pools + 8337 + 8484
cd /home/user/knight_moves_5/subagents/lns84
while ! grep -q QUEUE1_DONE tmp/pol37.log 2>/dev/null; do sleep 5; done
cp tmp/pol84_best.txt tmp/start_p84.txt; cp tmp/pol37_best.txt tmp/start_p37.txt
IN="tmp/start_p84.txt tmp/pol84_pool.txt tmp/start_p37.txt tmp/pol37_pool.txt tmp/record_8484.txt tmp/record_8337.txt"
for s in 1 2 3; do ./run_lns.sh L2 $s 25000 1400 $IN; done
echo QUEUE2_DONE > tmp/queue2.done
