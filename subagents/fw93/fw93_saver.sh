#!/bin/bash
# fw93 saver: every 60 s, checker-verify every distinct pool/sweep tour above 9,282 -> NEW_RECORD_<m>.txt. Run from subagents/fw93.
while true; do
  python3 newrec.py tmp/*/fw_pool_*.txt tmp/*/fw_*.txt tmp/sw/*_r*.txt >> tmp/saver_records.txt 2>&1
  [ -f tmp/saver.stop ] && exit 0
  sleep 60
done
