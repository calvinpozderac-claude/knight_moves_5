#!/bin/bash
# usage: l2cmp.sh LEVEL TIME ANCHOR "cfg1" "cfg2" ... ; prints restarts, mean, max of finished minutes per cfg
L=$1; T=$2; A=$3; shift 3
cd /home/user/knight_moves_5
for cfg in "$@"; do
  nice -n 5 subagents/nrpa/gnrpa --level $L --threads 1 --time $T --seed-tours data/tours/record_8123.txt --anchor-min $A --anchor-max $A \
    --seed-adapt 0 --no-warm --out subagents/nrpa/tmp/cmp_out.txt $cfg > subagents/nrpa/tmp/cmp.log
  echo "L$L A$A [$cfg] $(grep -o 'finished [0-9]* minutes\|unfinished [0-9]* minutes' subagents/nrpa/tmp/cmp.log | awk '{n++; if($1=="finished"){f++; s+=$2; if($2>m)m=$2}} END{printf "n=%d fin=%d mean=%.0f max=%d", n, f, f?s/f:0, m}')"
done
