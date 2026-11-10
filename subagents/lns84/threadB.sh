#!/bin/bash
cd /home/user/knight_moves_5/subagents/lns84
./pol_one.sh PB_8596 NEW_RECORD_8596.txt 1200
./pol_one.sh PB_8390 tmp/pol37_best.txt 1500
./lns_thread.sh LB 30000 21 23 25 27
