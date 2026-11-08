#!/bin/bash
# after q2: release-and-polish constrained results; depth-4 polish of elite boards; ruin & recreate
cd /home/user/knight_moves_5/altered_states_2
while pgrep -f q2.sh > /dev/null; do sleep 5; done
B=agents/targeted/as2t
L=acdefghiklmnoprstuvwy
# 1) unconstrained depth-3 (full board) polish of the top 3 boards (by true score) of every constrained log
for f in agents/targeted/logs/req_*.csv agents/targeted/logs/forb_*.csv; do
  for g in $(sort -t, -k2,2nr $f | cut -d, -f1 | awk '!s[$0]++' | head -3); do
    echo "== $f $g"; $B polishw $g --depth 3 --win 5 --letters $L | tail -1
  done
done > agents/targeted/out/release_polish.txt
# 2) full depth-4 polish (21 letters) of the best board, 4x4-window depth-4 on the next elite boards
$B polishw hmlfsoaioutsnrcregahoywin --depth 4 --win 5 --letters $L > agents/targeted/out/p4_best.txt
for g in cidovlanihtorscyesaigwnmw kdanurhigocorstlaneamiwdy anwypvioeghlsrntcaniemidv dasilwinoayergcostairuonm kodanrylirmasogcineewhtnp hcmdugiaotsrnshteoiawyflw chtrnoaeoylrsewginsmdaoci; do
  $B polishw $g --depth 4 --win 4 --letters $L | tail -2
done > agents/targeted/out/p4_elite.txt
# 3) depth-5 inside 3x3 windows on the best
$B polishw hmlfsoaioutsnrcregahoywin --depth 5 --win 3 --letters $L > agents/targeted/out/p5w3_best.txt
