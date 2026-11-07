#!/usr/bin/env bash
# Exploration A experiment batch (single thread, sequential).
B=./explore_a/build/wsa; R=results/explore_a
run() { name=$1; shift; $B "$@" > $R/$name.log 2>&1; printf "%-14s " $name; grep RESULT $R/$name.log; }
run gls_a      --mode gls --time 300 --iters 3000000 --T0 2 --T1 0.3 --polish 1000000 --up 0.25 --seed 1
run gls_b      --mode gls --time 300 --iters 3000000 --T0 2 --T1 0.3 --polish 1000000 --up 0.5 --wmax 6 --restart 25 --seed 2
run no_carol   --mode wsa --time 150 --weights NORTHCAROLINA=0,SOUTHCAROLINA=0 --seed 3
run no_dak     --mode wsa --time 150 --weights NORTHDAKOTA=0,SOUTHDAKOTA=0 --seed 4
run pen_cd     --mode wsa --time 150 --weights NORTHCAROLINA=-0.5,SOUTHCAROLINA=-0.5,NORTHDAKOTA=-0.5,SOUTHDAKOTA=-0.5 --seed 5
run no_mid     --mode wsa --time 150 --weights LOUISIANA=0,WISCONSIN=0,ILLINOIS=0,MISSOURI=0 --seed 6
run rsub       --mode rsub --time 450 --iters 5000000 --polish 2000000 --seed 7
