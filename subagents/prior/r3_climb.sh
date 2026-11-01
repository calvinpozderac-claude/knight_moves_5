#!/bin/bash
# Round 3, core B: new bottom climbs: level-3 pnrpa from anchors 12-35 of 8,329, random beta per restart
# (0 = stock), softer unk-gap prior, 3 rngs x S seconds.   usage: bash r3_climb.sh S
S=$1
for r in 1 2 3; do
  nice -n 5 timeout $((S + 300)) ./pnrpa --level 3 --threads 1 --time $S --rng $((200 + r)) --seed-tours ../../data/tours/record_8329.txt \
    --anchor-min 12 --anchor-max 35 --seed-adapt 0 --no-warm --prior prior_r3_unk.txt --beta-set 0,0.5,1,1.5,2 \
    --pool tmp/r3/pool_climb.txt --pool-min 7000 --out tmp/r3/best_climb_r$r.txt > tmp/r3/c_r$r.log 2>&1
done
