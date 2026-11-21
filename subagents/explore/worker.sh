#!/bin/bash
# usage: worker.sh WID  -- pops jobs from queue.txt (flock) until empty or tmp/stop exists (from subagents/sfl)
WID=$1
while [ ! -f tmp/stop ]; do
  JOB=$(flock queue.lock bash -c 'l=$(head -1 queue.txt); [ -n "$l" ] && sed -i 1d queue.txt; echo "$l"')
  [ -z "$JOB" ] && break
  echo "$(date +%T) w$WID start $JOB" >> tmp/events.log
  bash -c "$JOB"
  echo "$(date +%T) w$WID done  $JOB" >> tmp/events.log
done
