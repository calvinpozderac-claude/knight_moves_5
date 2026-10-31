#!/bin/bash
# heuristic-bound B&B sweep over prefix lengths (1 thread)
for k in "$@"; do
  nice -n 5 timeout 400 ./bb ../../data/tours/record_8123.txt $k --time 240 --tt 25 --out tmp/found_k$k.txt | grep -v '^(' 
done
