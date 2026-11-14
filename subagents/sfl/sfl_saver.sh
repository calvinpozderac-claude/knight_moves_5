#!/bin/bash
# sfl saver: every 60 s, checker-verify every distinct tour above 9,286 in runs/ and pol/ -> NEW_RECORD_<m>.txt.
# Run from subagents/sfl. Stop: touch tmp/saver.stop
while true; do
  python3 newrec_sfl.py runs/*_best.txt runs/*.elite runs/*_dump.txt pol/*.txt >> tmp/saver.log 2>&1
  date +%T > tmp/saver.alive
  [ -f tmp/saver.stop ] && exit 0
  sleep 60
done
