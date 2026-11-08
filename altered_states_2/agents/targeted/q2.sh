#!/bin/bash
# after q1: forced exclusion, then pairs
cd /home/user/knight_moves_5/altered_states_2
while pgrep -f q1.sh > /dev/null; do sleep 5; done
B=agents/targeted/as2t
for S in NJ MO CO OK ID AL HI SC; do
  $B sa --forbid $S --bonus 40e6 --iters 5000000 --t0 3e6 --t1 1e6 --seconds 100 --seed 11 --polish --log agents/targeted/logs/forb_$S.csv > agents/targeted/out/forb_$S.txt
done
for P in WA,MD PA,MD TN,MD WA,TN PA,WA; do
  $B sa --require $P --bonus 40e6 --iters 5000000 --t0 3e6 --t1 1e6 --seconds 90 --seed 13 --polish --log agents/targeted/logs/req_${P/,/_}.csv > agents/targeted/out/req_${P/,/_}.txt
done
