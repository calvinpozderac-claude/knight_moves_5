#!/bin/bash
# usage: seedq.sh ITERS LO HI SEED...  : focused LNS (lns_r6 --free 0) from the current best, donors = don/*;
# every new best is polished (600 s, wmax 12) and both go into don/.
cd /home/user/knight_moves_5/subagents/polish92
it=$1; LO=$2; HI=$3; shift 3
best() { for f in don/*.txt NEW_RECORD_*.txt; do [ -f $f ] && echo "$(head -1 $f) $f"; done | sort -n | tail -1 | cut -d' ' -f2; }
for s in "$@"; do
  st=$(best); b0=$(head -1 $st)
  if [ -n "$START" ] && [ "$(head -1 $START)" -ge "$b0" ]; then st=$START; fi
  ./lns.sh L $s $it $st $LO $HI 0.85 don/*.txt
  r=lns/L_s${s}_best.txt; b1=$(head -1 $r)
  python3 ../../tools/check_tour.py "$(sed -n 2p $r)" >> events.log
  if [ "$b1" -gt "$b0" ]; then
    cp $r don/L_s${s}_$b1.txt
    ./pol.sh PL_s$s $r 600 12
    cp pol/PL_s${s}_best.txt don/PL_s${s}_$(head -1 pol/PL_s${s}_best.txt).txt
  fi
done
echo "$(date +%T) seedq $* done" >> events.log
