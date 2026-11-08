#!/bin/bash
# sequential single-core experiment batch; run from altered_states_2/
M=./agents/meta/meta; L=agents/meta/logs
$M lns hmlfsoaioutsnrcregahoywin --k 4 --letters 18 --allclusters 1 > $L/lns4_best.out
$M sa     --seconds 300 --seed 11 --iters 5e6 --t0 3e6 --t1 1e6 --rich 0    --log $L/sa_plain.csv  > $L/sa_plain.out
$M sa     --seconds 420 --seed 12 --iters 5e6 --t0 3e6 --t1 1e6 --rich 0.15 --log $L/sa_rich.csv   > $L/sa_rich.out
$M lnsrun --seconds 420 --seed 13 --iters 1e6 --t0 3e6 --t1 1e6 --k 3 --patience 60 --log $L/lnsrun.csv > $L/lnsrun.out
$M rw     --seconds 360 --seed 14 --iters 5e6 --t0 3e6 --t1 1e6 --sigma 0.5 --frac 0.6 --t2 1.5e6 --log $L/rw.csv > $L/rw.out
$M lahc   --seconds 240 --seed 15 --iters 5e6 --hist 5000 --log $L/lahc.csv > $L/lahc.out
$M ga     --seconds 600 --seed 16 --pop 24 --pelite 0.5 --childiters 1e6 --ct0 1.5e6 --ct1 5e5 --seediters 2e6 --log $L/ga.csv > $L/ga.out
echo ALLDONE > $L/done
