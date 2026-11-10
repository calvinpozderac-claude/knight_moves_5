#!/bin/bash
# heuristic (lambda-scaled bound) B&B: args pairs K LAMBDA
while [ $# -ge 2 ]; do k=$1; lam=$2; shift 2
  nice -n 5 timeout 200 ./bb3 ../../data/tours/record_8123.txt $k --lambda $lam --time 150 --tt 25 --out tmp/found_k${k}_l$lam.txt | grep --line-buffered -E "^k=|DONE|TIMEOUT|FOUND"
done
