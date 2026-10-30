#!/usr/bin/env bash
# Generate more finished tours and merge them into the curriculum set.
# The trained networks branch off random points of the existing tours, and also play from the
# 23-move opening with different amounts of randomness. Every tour is verified exactly
# (tools/harvest_tours.py) before it is added.
# usage: bash scripts/generate_tours.sh [games_per_job=200] [parallel_jobs=half the CPUs] [set=data/tours/curriculum.txt]
set -euo pipefail
cd "$(dirname "$0")/.."
G=${1:-200}; P=${2:-$(( $(nproc) / 2 ))}; SET=${3:-data/tours/curriculum.txt}
[ -x build/az ] || make
OUT=runs/gen_$(date +%Y%m%d_%H%M%S); mkdir -p "$OUT"
SEEDED=weights/seeded_record_net.bin; SCRATCH=weights/from_scratch_net.bin
BR="OPENING=0 PREFIX_TOURS=$SET PREFIX_LEN=-1 PREFIX_BALANCE=0.5"
jobs=(
  "OPENING=1 TAU1=1 TAU2=1 TOURS_OUT=$OUT/t01.txt build/az selfplay $SEEDED $OUT/s01.bin $G 800 $RANDOM 25 $OUT/b01.txt"
  "OPENING=1 TAU1=1 TAU2=0 TOURS_OUT=$OUT/t02.txt build/az selfplay $SEEDED $OUT/s02.bin $G 800 $RANDOM 25 $OUT/b02.txt"
  "OPENING=1 TAU1=1 TAU2=1 TOURS_OUT=$OUT/t03.txt build/az selfplay $SCRATCH $OUT/s03.bin $G 400 $RANDOM 25 $OUT/b03.txt"
  "OPENING=1 TAU1=1 TAU2=1 TOURS_OUT=$OUT/t04.txt build/az selfplay $SCRATCH $OUT/s04.bin $G 100 $RANDOM 25 $OUT/b04.txt"
  "$BR TAU1=1 TAU2=0 TOURS_OUT=$OUT/t05.txt build/az selfplay $SEEDED $OUT/s05.bin $G 800 $RANDOM 25 $OUT/b05.txt"
  "$BR TAU1=1 TAU2=1 TOURS_OUT=$OUT/t06.txt build/az selfplay $SEEDED $OUT/s06.bin $G 800 $RANDOM 25 $OUT/b06.txt"
  "$BR TAU1=1 TAU2=0 TOURS_OUT=$OUT/t07.txt build/az selfplay $SCRATCH $OUT/s07.bin $G 800 $RANDOM 25 $OUT/b07.txt"
  "$BR TAU1=1 TAU2=1 TOURS_OUT=$OUT/t08.txt build/az selfplay $SCRATCH $OUT/s08.bin $G 400 $RANDOM 25 $OUT/b08.txt"
)
echo "== Running ${#jobs[@]} generators ($G games each, $P at a time) into $OUT"
printf '%s\n' "${jobs[@]}" | xargs -P "$P" -I{} bash -c '{} > /dev/null'
echo "== Generated $(cat "$OUT"/t*.txt | wc -l) finished tours; verifying and merging"
PY=python3; [ -x .venv/bin/python ] && PY=.venv/bin/python
$PY tools/harvest_tours.py "$OUT/merged.txt" "$SET" "$OUT"
cp "$OUT/merged.txt" "$SET"
echo "== $SET now holds $(wc -l < "$SET") verified tours"
