#!/bin/bash
# 9,279 line: polish 900 s -> bb tail proof -> focused LNS seeds 931-933 (60k, --free 0, donors don2/), new bests polished + fed back
cd /home/user/knight_moves_5/subagents/polish92
./pol.sh P9279 in/record_9279.txt 900 12
cp pol/P9279_best.txt don2/P9279.txt
python3 ../../tools/check_tour.py "$(sed -n 2p pol/P9279_best.txt)" >> events.log
./exact.sh pol/P9279_best.txt E9279 240
read LO HI < focus_E9279.txt
best() { for f in don2/*.txt; do echo "$(head -1 $f) $f"; done | sort -n | tail -1 | cut -d' ' -f2; }
for s in 931 932 933; do
  st=$(best); b0=$(head -1 $st)
  [ "$(head -1 don2/P9279.txt)" -ge "$b0" ] && st=don2/P9279.txt
  ./lns.sh M $s 60000 $st $LO $HI 0.85 don2/*.txt
  r=lns/M_s${s}_best.txt
  if [ -f $r ]; then
    b1=$(head -1 $r); python3 ../../tools/check_tour.py "$(sed -n 2p $r)" >> events.log
    cp $r don2/M_s${s}_$b1.txt
    if [ "$b1" -gt "$b0" ]; then ./pol.sh PM_s$s $r 600 12; cp pol/PM_s${s}_best.txt don2/PM_s${s}.txt; fi
  fi
done
echo "$(date +%T) q5 done" >> events.log
