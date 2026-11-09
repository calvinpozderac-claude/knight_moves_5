#!/bin/bash
# usage: seedq.sh ITERS SEED...  F5 focused LNS seeds in sequence; start = best checked tour (NEW_RECORD_* or 8812);
# donors = don/* (+ every NEW_RECORD); focus from focus_E8812.txt (p 0.85); any best > current gets polished (600 s, wmax 12) and added to don/
cd /home/user/knight_moves_5/subagents/f5push
it=$1; shift
for s in "$@"; do
  st=$(ls NEW_RECORD_*.txt 2>/dev/null | sort -t_ -k3 -n | tail -1); [ -z "$st" ] && st=in/r8812.txt
  read LO HI < focus_E8812.txt
  cur=$(head -1 $st)
  ./lns.sh F5 $s $it $st $LO $HI 0.85 don/* $(ls NEW_RECORD_*.txt 2>/dev/null)
  b=lns/F5_s${s}_best.txt; bt=$(head -1 $b)
  if [ "$bt" -gt "$cur" ] 2>/dev/null; then
    sleep 40   # let the saver check it
    if [ -f NEW_RECORD_$bt.txt ]; then
      cp NEW_RECORD_$bt.txt don/F5_$bt.txt
      ./pol.sh PF5_s$s NEW_RECORD_$bt.txt 600 12
    fi
  fi
done
