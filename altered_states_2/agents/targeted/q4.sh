#!/bin/bash
cd /home/user/knight_moves_5/altered_states_2
while pgrep -f q3.sh > /dev/null; do sleep 5; done
agents/targeted/as2t rr hmlfsoaioutsnrcregahoywin --seconds ${1:-600} --seed 21 --inner 20000 --slack 1.5e6 --log agents/targeted/logs/rr.txt > agents/targeted/out/rr.txt
