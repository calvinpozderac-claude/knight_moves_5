#!/bin/bash
# after q2: longer LNS from the 9,271 / 9,268 polished tours (their proofs: k = 151 -> focus 1..150), same donors
cd /home/user/knight_moves_5/subagents/polish92
for spec in "925 don/P9271.txt" "926 don/P9268.txt"; do
  set -- $spec; s=$1; st=$2
  ./lns.sh L $s 150000 $st 1 150 0.85 don/*.txt
  r=lns/L_s${s}_best.txt
  if [ -f $r ]; then python3 ../../tools/check_tour.py "$(sed -n 2p $r)" >> events.log
    b1=$(head -1 $r); cp $r don/L_s${s}_$b1.txt
    if [ "$b1" -ge 9275 ]; then ./pol.sh PL_s$s $r 600 12; fi
  fi
done
echo "$(date +%T) q3 done" >> events.log
