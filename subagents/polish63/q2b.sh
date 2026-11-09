#!/bin/bash
# LNS: lineage 8559 = polished best (identical to strict 8607) ; lineage 8563 = original unpolished 8563 (own start)
cd /home/user/knight_moves_5/subagents/polish63
until [ -f run/focus_P8559.txt ]; do sleep 10; done
D="tmp/record_8559.txt tmp/record_8563.txt tmp/record_8609.txt tmp/strict_8607.txt run/P8559_pool.txt run/P8563_pool.txt"
for s in 301 302 303; do
  ./lns.sh L8559 $s 30000 run/P8559_best.txt run/focus_P8559.txt $D
  ./lns.sh L8563 $((s+10)) 30000 tmp/record_8563.txt run/focus_O8563.txt $D
done
echo "$(date +%T) q2 done" >> run/events.log
