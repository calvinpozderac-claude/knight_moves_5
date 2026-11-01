#!/bin/bash
# Round 4: level 4 (--iters 50) guided searches.  usage: bash r4.sh dance BASE RNG S | climb RNG S
if [ $1 = dance ]; then b=$2; r=$3; S=$4
  nice -n 5 timeout $((S + 900)) ./pnrpa --level 4 --iters 50 --time $S --rng $r --seed-tours tmp/r4/b$b.txt \
    --anchor-min 55 --anchor-max 95 --seed-adapt 0 --no-warm --prior prior_r3.txt --beta-set 0.5,1,1.5,2,2.5 --init-noise 0.5 \
    --pool tmp/r4/pool_dance.txt --pool-min 7500 --out tmp/r4/best_d${b}_r$r.txt > tmp/r4/d${b}_r$r.log 2>&1
else r=$2; S=$3
  nice -n 5 timeout $((S + 900)) ./pnrpa --level 4 --iters 50 --time $S --rng $r --seed-tours tmp/r4/c8162.txt \
    --anchor-min 20 --anchor-max 60 --seed-adapt 0 --no-warm --prior prior_r3_unk.txt --beta-set 0,0.5,1,1.5,2 --init-noise 0.5 \
    --pool tmp/r4/pool_climb.txt --pool-min 7000 --out tmp/r4/best_c_r$r.txt > tmp/r4/c_r$r.log 2>&1
fi
