#!/bin/bash
# Variation runs on the 9,286 (record_9282 + b7->c5 sink): opening anchors, weaker warm, wider free sinks. Run from subagents/fw93.
UNTIL=$1; S=in2/$(ls in2 | head -1); S=NEW_RECORD_9286.txt; mkdir -p tmp/p4
run() { tag=$1; shift; [ $(date +%s) -ge $UNTIL ] && exit 0
  timeout 260 nice -n 5 ./tmp/fnrpa --sc 1 --free 1 --fstep 1 --seed $S --out tmp/p4/fw_$tag.txt --pool tmp/p4/fw_pool_v.txt --pool-min 9200 "$@" > tmp/p4/fw_$tag.log 2>&1
  echo "$tag: 9286 -> $(head -1 tmp/p4/fw_$tag.txt)" >> tmp/p4/fw_summary.txt; }
run open_r201 --fmax 4 --warm 10 --level 2 --time 120 --amin 1 --amax 40 --rng 201
run warm3_r202 --fmax 4 --warm 3 --level 2 --time 120 --amin 35 --amax 999 --rng 202
run fmax8_r203 --fmax 8 --warm 10 --level 2 --time 120 --amin 35 --amax 999 --rng 203
run open_r204 --fmax 4 --warm 3 --level 2 --time 120 --amin 1 --amax 40 --rng 204
run dance_r205 --fmax 4 --warm 10 --level 3 --time 240 --amin 120 --amax 999 --rng 205
run merge_r206 --fmax 4 --warm 3 --level 2 --time 120 --amin 40 --amax 100 --rng 206
