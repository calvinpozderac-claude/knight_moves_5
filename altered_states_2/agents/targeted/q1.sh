#!/bin/bash
# forced inclusion runs, sequential, single core
cd /home/user/knight_moves_5/altered_states_2
B=agents/targeted/as2t
for S in WA TN MD MA NV KY CT MS; do
  $B sa --require $S --bonus 40e6 --iters 5000000 --t0 3e6 --t1 1e6 --seconds 170 --seed 7 --polish --log agents/targeted/logs/req_$S.csv > agents/targeted/out/req_$S.txt
done
