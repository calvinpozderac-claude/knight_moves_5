#!/bin/bash
cd "$(dirname "$0")"
while true; do
  python3 newstrict.py runs/s*_best.txt runs/s*.elite runs/s*_best.txt.adj >> tmp/strict_saver.log 2>&1
  date +%T > tmp/strict_saver.alive
  [ -f tmp/saver.stop ] && exit 0
  sleep 60
done
