#!/bin/bash
cd "$(dirname "$0")"
while true; do
  python3 newstrict.py runs/*_best.txt runs/*.elite runs/*.adj runs/*pool*.txt >> tmp/saver.log 2>&1
  date +%T > tmp/saver.alive
  [ -f tmp/saver.stop ] && exit 0
  sleep 120
done
