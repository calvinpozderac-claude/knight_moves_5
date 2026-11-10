#!/bin/bash
# fw84's free-wait pass on the two strict 8,607 skeletons (8,549 line and 8,537 line), rngs 1-2, 300 s each
cd /home/user/knight_moves_5/subagents/lns84
mkdir -p r6/fw; sed 's/^BEST = Fraction(8605)/BEST = Fraction(8609)/; s#tmp/fw_checked.txt#r6/fw/checked.txt#' newrec.py > r6/newrec.py
for f in r6/S8549.txt r6/Q8537_best.txt; do for r in 1 2; do
  id=$(basename $f .txt)_r$r
  timeout 450 nice -n 5 ./fnrpa --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 300 --amin 35 --amax 999 --rng $r \
    --seed $f --out r6/fw/fw_$id.txt --pool r6/fw/fw_pool.txt --pool-min 8600 > r6/fw/fw_$id.log 2>&1
  echo "$(date +%T) fw $id: $(head -1 $f) -> $(head -1 r6/fw/fw_$id.txt) end" >> r6/events.log
  python3 r6/newrec.py r6/fw/fw_pool.txt r6/fw/fw_$id.txt >> r6/fw/records.txt
done; done
