#!/usr/bin/env bash
# Equal-budget comparison of the build/as search modes.
# usage: bash bench.sh SECONDS THREADS "SEEDS" [OUTDIR]
set -u
T=${1:-120}; TH=${2:-1}; SEEDS=${3:-"1 2"}; OUT=${4:-results/bench}
mkdir -p "$OUT"
declare -A CFG=(
  [hc]="--mode hc --lambda 0"
  [hc_smooth]="--mode hc --lambda 0.5"
  [sa]="--mode sa --lambda 0 --iters 20000000 --T0 3 --T1 0.3"
  [sa_smooth]="--mode sa --lambda 0.5 --iters 6000000 --T0 3 --T1 0.3"
  [pt]="--mode pt --lambda 0 --replicas 16 --Tmin 0.3 --Tmax 6"
  [pt_smooth]="--mode pt --lambda 0.5 --replicas 16 --Tmin 0.3 --Tmax 6"
  [ils]="--mode ils --lambda 0 --kick 5"
)
for name in hc hc_smooth sa sa_smooth pt pt_smooth ils; do
  for s in $SEEDS; do
    nice -n 19 ./build/as ${CFG[$name]} --time "$T" --threads "$TH" --seed "$s" > "$OUT/${name}_s$s.log"
    printf "%-10s seed %s  " "$name" "$s"; grep -h RESULT "$OUT/${name}_s$s.log" | sed 's/.*best=/best=/'
  done
done
