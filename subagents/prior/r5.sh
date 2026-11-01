#!/bin/bash
# Round 4b: level-3 guided skeleton search with strong diversity (many distinct strict skeletons for the free-wait pass)
# usage: bash r5.sh BASE AMAX RNG NOISE S | bash r5.sh warm8162 RNG S
if [ $1 = warm8162 ]; then r=$2; S=$3
  # local (warm) polish of the outside-basin 8,162 tour: seed-adapt 10, starts from the seed as best
  nice -n 5 timeout $((S + 600)) ./pnrpa --level 3 --time $S --rng $r --seed-tours tmp/r4/c8162.txt --anchor-min 20 --anchor-max 140 \
    --seed-adapt 10 --prior prior_r3_unk.txt --beta-set 0,0.5,1 --pool tmp/r5/pool_climb.txt --pool-min 8000 --out tmp/r5/best_w8162.txt > tmp/r5/w8162.log 2>&1
else b=$1; amax=$2; r=$3; nz=$4; S=$5
  nice -n 5 timeout $((S + 400)) ./pnrpa --level 3 --time $S --rng $r --seed-tours tmp/r4/b$b.txt --anchor-min 55 --anchor-max $amax \
    --seed-adapt 0 --no-warm --prior prior_r3.txt --beta-set 0.5,1,1.5,2,2.5 --init-noise $nz \
    --pool tmp/r5/pool_dance.txt --pool-min 8000 --out tmp/r5/best_${b}_r$r.txt > tmp/r5/d${b}_r$r.log 2>&1
fi
