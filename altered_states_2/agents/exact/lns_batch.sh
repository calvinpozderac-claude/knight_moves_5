#!/bin/bash
# exact-region LNS (k=6 connected regions) on the top boards; run from altered_states_2/
for g in hmlfsoaioutsnrcregahoywin cidovlanihtorscyesaigwnmw kdanurhigocorstlaneamiwdy anwypvioeghlsrntcaniemidv dasilwinoayergcostairuonm kodanrylirmasogcineewhtnp hcmdugiaotsrnshteoiawyflw chtrnoaeoylrsewginsmdaoci; do
  ./agents/exact/bb lns $g --k 6 --secs ${SECS:-150} --seed 7 > agents/exact/out/lns6_$g.txt
  tail -1 agents/exact/out/lns6_$g.txt
done
