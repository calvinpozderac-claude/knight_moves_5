#!/bin/bash
# chain.sh TAG START RULE PEN:SECS [PEN:SECS ...]   decaying keep penalty: dlns stages, each from the previous stage's best
cd /home/user/knight_moves_5/subagents/penalty92
tag=$1; st=$2; rule=$3; shift 3; k=0; seed=$(( $(echo -n $tag | cksum | cut -d' ' -f1) % 50000 + 11 ))
for ps in "$@"; do
  pen=${ps%%:*}; t=${ps##*:}; k=$((k+1)); out=y/${tag}_c${k}_best.txt
  echo "$(date +%T) [chain $tag] stage $k pen $pen ${t}s from $st ($(head -1 $st))" >> events.log
  timeout $((t+60)) nice -n 5 bin/dlns --tours $st --start $st --seed $((seed+k)) --xover 0 --t0 40 --budget 400000 --dmax 8 --pdmax 4 \
    --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 0 --time $t --pen $pen --rule $rule --out $out > y/${tag}_c${k}.log 2>&1
  [ -f $out ] || cp $st $out
  fs=$(head -1 $out.feas_best.txt 2>/dev/null)
  echo "$(date +%T) [chain $tag] stage $k -> best $(head -1 $out) (banned sits $(grep 'forbidden sits' y/${tag}_c${k}.log | tail -1 | awk '{print $NF}')) feasible $fs; $(python3 fam.py in/P_9282.txt $out | cut -d' ' -f3-)" >> events.log
  st=$out
done
echo "$(date +%T) [chain $tag] done" >> events.log
