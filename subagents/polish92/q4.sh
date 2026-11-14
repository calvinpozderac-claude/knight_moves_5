#!/bin/bash
# after the 00:29 container restart: rerun s924 (killed at 20k), then penalty92's 9,275 start, then q3's long runs
cd /home/user/knight_moves_5/subagents/polish92
echo "$(date +%T) q4 start (restart after container reboot; donors + penalty92 9,275s b8a997d6, 23904b34)" >> events.log
run() { # seed iters start lo hi
  s=$1; it=$2; st=$3; lo=$4; hi=$5
  ./lns.sh L $s $it $st $lo $hi 0.85 don/*.txt
  r=lns/L_s${s}_best.txt
  if [ -f $r ]; then python3 ../../tools/check_tour.py "$(sed -n 2p $r)" >> events.log
    b1=$(head -1 $r); cp $r don/L_s${s}_$b1.txt
    if [ "$b1" -gt 9275 ]; then ./pol.sh PL_s$s $r 600 12; cp pol/PL_s${s}_best.txt don/PL_s${s}.txt; fi
  fi
}
run 924 60000 don/P9275.txt 1 152
run 927 60000 don/pen9275_b8a997d6.txt 1 154
run 925  80000 don/P9271.txt 1 150
run 926  60000 don/P9268.txt 1 150
echo "$(date +%T) q4 done" >> events.log
