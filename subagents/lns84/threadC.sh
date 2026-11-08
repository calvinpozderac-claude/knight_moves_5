#!/bin/bash
cd /home/user/knight_moves_5/subagents/lns84
./exact_table.sh NEW_RECORD_8600.txt tmp/exact_8600.log 300 164 160 156 154 152 150 148 146 144 142 140 138 136 134
k=$(grep DONE tmp/exact_8600.log | tail -1 | sed 's/.*:: DONE k=\([0-9]*\).*/\1/'); [ -n "$k" ] && echo "1 $((k-1)) 0.85" > tmp/focus.txt
./lns_thread.sh LC 30000 11 13 15 17 19
