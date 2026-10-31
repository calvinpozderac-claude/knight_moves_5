#!/bin/bash
# Full astar_cpu pipeline: gen (NRPA-finished cuts of known tours) -> feat -> train (numpy) -> search (with online refit).
# Run from anywhere; everything goes to subagents/astar_cpu/tmp/run (or $RUN). Environment knobs (defaults):
#   THREADS=2 GEN_MIN=25 TRAIN_MIN=15 SEARCH_MIN=60 HIDDEN=64 FEAT_MAX=600000 PREFIX=30:40 SEED_TOUR=data/tours/record_8123.txt
#   LABEL_THREADS=1 REFIT=300 (seconds between weight updates; 0 = no online refit) NICE=5
# Extra arguments are passed to `astar_cpu search` (e.g. --noise 40 --depth-bonus 5).
# A gen.bin / model.txt already in the run folder is reused (delete it to regenerate); GEN_MIN=0 skips gen.
set -e
cd "$(dirname "$0")/../.."
D=subagents/astar_cpu; R=${RUN:-$D/tmp/run}; mkdir -p "$R"
TH=${THREADS:-2}; GEN=${GEN_MIN:-25}; TRN=${TRAIN_MIN:-15}; SRCH=${SEARCH_MIN:-60}; NI=${NICE:-5}
sec() { awk "BEGIN{print int($1*60)}"; }
[ -x $D/astar_cpu ] && [ $D/astar_cpu -nt $D/astar_cpu.cpp ] || g++ -O3 -march=native -std=c++17 -pthread -o $D/astar_cpu $D/astar_cpu.cpp
T=data/tours
TOURS="--tours $T/record_8123.txt:100 --tours $T/record_8002.txt:100 --tours $T/record_7911.txt:100 --tours $T/record_7200.txt:50
       --tours $T/record_7066.txt:50 --tours $T/tour_6303.txt:30 --tours $T/curriculum.txt:1 --tours $T/openings.txt:0.2"
for p in ${EXTRA_TOURS:-}; do TOURS="$TOURS --tours $p"; done   # e.g. EXTRA_TOURS="path/pool.txt:20"
if [ "$GEN" != "0" ] && [ ! -s "$R/gen.bin" ]; then
  echo "== gen: $GEN min on $TH threads"
  nice -n $NI $D/astar_cpu gen --threads $TH --time $(sec $GEN) --out "$R/gen.bin" $TOURS | tee -a "$R/gen.log"
fi
if [ ! -s "$R/model.txt" ]; then
  echo "== feat + train ($TRN min)"
  nice -n $NI $D/astar_cpu feat --in "$R/gen.bin" --out "$R/feat.bin" --max ${FEAT_MAX:-600000} --threads $TH | tee -a "$R/train.log"
  nice -n $NI python3 $D/train.py --feat "$R/feat.bin" --out "$R/model.txt" --hidden ${HIDDEN:-64} --minutes $TRN | tee -a "$R/train.log"
  rm -f "$R/feat.bin"
fi
echo "== search: $SEARCH_MIN min"
LT=${LABEL_THREADS:-1}; RF=${REFIT:-300}
LEARN=""; [ "$RF" != "0" ] && LEARN="--refit $RF --replay $R/gen.bin --save-model $R/model_refit.txt"
nice -n $NI $D/astar_cpu search --model "$R/model.txt" --threads $TH --label-threads $LT $LEARN --time $(sec $SRCH) \
  --seed-tour ${SEED_TOUR:-$T/record_8123.txt} --prefix ${PREFIX:-30:40} --restart-stale 1200 \
  --out "$R/best.txt" --pool "$R/pool.txt" --record-dir $D "$@" 2>&1 | tee -a "$R/search.log"
