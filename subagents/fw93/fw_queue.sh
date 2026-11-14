#!/bin/bash
# Shared-queue fnrpa fw pass worker. usage: bash fw_queue.sh QUEUE UNTIL TAG W
# QUEUE lines: "SEEDFILE RNG T AMIN AMAX WARM LEVEL FMAX"; workers take the next line under flock (counter QUEUE.next).
Q=$1; UNTIL=$2; TAG=$3; W=$4; mkdir -p tmp/$TAG
while true; do
  [ $(date +%s) -ge $UNTIL ] && exit 0
  n=$( (flock 9; c=$(cat $Q.next 2>/dev/null || echo 0); c=$((c+1)); echo $c > $Q.next; echo $c) 9>$Q.lock )
  line=$(sed -n ${n}p $Q); [ -z "$line" ] && exit 0
  read f r T a b wm lv fm <<< "$line"
  id=$(basename $f .txt)_r${r}_a${a}
  timeout $((T + 150)) nice -n 5 ./tmp/fnrpa --sc 1 --free 1 --fstep 1 --fmax $fm --warm $wm --level $lv --time $T --amin $a --amax $b --rng $r \
    --seed $f --out tmp/$TAG/fw_$id.txt --pool tmp/$TAG/fw_pool_$W.txt --pool-min 9200 > tmp/$TAG/fw_$id.log 2>&1
  echo "$id: $(head -1 $f) -> $(head -1 tmp/$TAG/fw_$id.txt)" >> tmp/$TAG/fw_summary.txt
done
