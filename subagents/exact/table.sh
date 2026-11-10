#!/bin/bash
# sound exhaustive suffix optimum: table.sh TOUR TIME k1 k2 ...
f=$1; t=$2; shift 2
for k in "$@"; do nice -n 5 timeout $((t+60)) ./bb $f $k --nobound --alpha 0 --tt 25 --time $t | grep --line-buffered -E "DONE|TIMEOUT"; done
