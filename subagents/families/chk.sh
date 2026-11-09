#!/bin/bash
# check_tour every given file; print T, ok/unnecessary summary, signature
cd /home/user/knight_moves_5/subagents/families
for f in "$@"; do [ -f "$f" ] || continue; out=$(python3 ../../tools/check_tour.py "$(grep '(' $f | head -1)"); echo "$f $(head -1 $f) :: $out"; done
