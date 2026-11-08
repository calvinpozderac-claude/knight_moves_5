#!/bin/bash
# run from altered_states_2/. Exact neighbourhood searches (one core, sequential).
T=265326530
B=hmlfsoaioutsnrcregahoywin
O=agents/exact/out
./agents/exact/bb allsub $B 5 --span 2 > $O/allsub5_span2_best.txt
tail -1 $O/allsub5_span2_best.txt
for g in kdanurhigocorstlaneamiwdy anwypvioeghlsrntcaniemidv dasilwinoayergcostairuonm kodanrylirmasogcineewhtnp hcmdugiaotsrnshteoiawyflw dasilwinoayergckstairuonm coladyericwsnghsaotamilen dacomginiylrsewoatsrmchok facgilorhaminntvaseylyrow jotaswenghyurimioladnschn cgrmhhaioatsncloerifwyouw; do
  ./agents/exact/bb allsub $g 4 --target $T > $O/allsub4_$g.txt
  echo $g; tail -1 $O/allsub4_$g.txt
done
./agents/exact/bb allsub $B 6 --span 2 > $O/allsub6_span2_best.txt
tail -1 $O/allsub6_span2_best.txt
