#!/bin/bash
# LNS: 3 seeds per lineage, 30k iters, free waits on, --start = lineage's polished best
cd /home/user/knight_moves_5/subagents/polish63
until grep -q "q1 done" run/events.log; do sleep 10; done
D="tmp/record_8559.txt tmp/record_8563.txt tmp/record_8609.txt tmp/strict_8607.txt run/P8559_best.txt run/P8563_best.txt run/P8559_pool.txt run/P8563_pool.txt"
for s in 301 302 303; do
  ./lns.sh L8559 $s 30000 run/P8559_best.txt run/focus_P8559.txt $D
  ./lns.sh L8563 $((s+10)) 30000 run/P8563_best.txt run/focus_P8563.txt $D
done
echo "$(date +%T) q2 done" >> run/events.log
