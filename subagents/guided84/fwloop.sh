#!/bin/bash
# Keep running the free-wait pass (fw.sh) over new skeletons >= MIN in the given pool globs until UNTIL.
# usage: bash fwloop.sh ID N MIN T UNTIL "POOLGLOB..."
ID=$1; N=$2; MIN=$3; T=$4; UNTIL=$5; G=$6
while [ $(date +%s) -lt $UNTIL ]; do
  before=$(wc -l < tmp/fw/passed.txt 2>/dev/null || echo 0)
  bash fw.sh $ID $N $MIN $T $UNTIL $G
  after=$(wc -l < tmp/fw/passed.txt 2>/dev/null || echo 0)
  [ "$before" = "$after" ] && sleep 30
done
