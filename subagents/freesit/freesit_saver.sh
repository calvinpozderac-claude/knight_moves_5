#!/bin/bash
# every 60 s: checker-verify every distinct tour > 9,290 in runs/ -> NEW_RECORD_<m>.txt. Stop: touch tmp/saver.stop
cd "$(dirname "$0")"
while true; do
  python3 newrec_sfl.py runs/*_best.txt runs/*.elite runs/*_dump.txt >> tmp/saver.log 2>&1
  date +%T > tmp/saver.alive
  [ -f tmp/saver.stop ] && exit 0
  sleep 60
done
