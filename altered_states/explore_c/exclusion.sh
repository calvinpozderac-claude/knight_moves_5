#!/usr/bin/env bash
# Forbid each state of the 127 set in turn (weight 0) and anneal; record the TRUE score of the best grid.
cd "$(dirname "$0")/.." || exit 1
OUT=results/explore_c/exclusion; mkdir -p $OUT
for s in ALASKA ARKANSAS COLORADO HAWAII ILLINOIS KANSAS LOUISIANA MISSOURI MONTANA NORTHCAROLINA NORTHDAKOTA SOUTHCAROLINA SOUTHDAKOTA UTAH WISCONSIN; do
  ./explore_c/build/xc --mode sa --forbid $s --time ${1:-30} --iters 20000000 --seed 3 > $OUT/$s.log 2>&1
  printf "%-14s " $s; grep FINAL $OUT/$s.log | sed 's/.*FINAL //'
done
