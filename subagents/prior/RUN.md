# Learned move prior for NRPA (GNRPA-style bias)

`pnrpa` is a copy of `src/nrpa.cpp` in which every move's logit is `weight[code] + beta * prior(s, move)`,
used in both the rollouts and the adapt step (as in GNRPA, Cazenave 2020). `prior` is a small MLP
(42 features -> 16 tanh -> 1) trained on every known tour. With no `--prior` (or `--beta 0`) it is stock NRPA.
All other options are the same as `build/nrpa` (see the comment at the top of `pnrpa.cpp`).

## Files
- `prior.h`: the 42 per-move features (`move_features`, and `all_move_features` that does every move of a
  position at once, exactly the same numbers) and the model reader/scorer.
- `gen.cpp`: training data. Replays every tour, maps each state (altitudes, visits, position) to the best
  final score of any known tour through it, and writes one row per legal non-h8 pinned move of every
  position on a tour: label = best known score through the child state (0 = none), plus features.
  `--val-frac`/`--val-file` give a held-out split; `--check` checks `all_move_features == move_features`.
- `train_prior.py`: numpy listwise training (softmax over each position's moves; target softmax((L-Lmax)/50)
  over the moves with a known tour; position weight Lmax/8123 + 0.02). Reports top-1 accuracy against
  uniform, max-wait and zero-wait baselines; writes the model file.
- `pnrpa.cpp`: the search. New options `--prior FILE --beta B [--prior-cache-log2 17]`. The pool is written
  `minutes<TAB>tour`, one per line. Ctrl-C/SIGTERM stops cleanly. `NRPA_BENCH=N` times N rollouts.
- Models: `prior_h16c.txt` (**recommended**: cheap features, trained on all tours), `prior_h16c_holdout.txt`
  (same, with 7911/8002/8123 and 15% of the other tours held out), `prior_h16.txt` (all features, 4-6x
  slower, no better), `prior_lin.txt` (linear, all features).
- `smoke.sh ANCHOR SECONDS "label|args" ...`: level-2 A/B from an anchor of the 8,123 tour.

## Build and regenerate (from `subagents/prior/`)
```
g++ -O3 -march=native -std=c++17 -o gen gen.cpp
g++ -O3 -march=native -std=c++17 -pthread -o pnrpa pnrpa.cpp
./gen --out tmp/all.bin ../../data/tours/*.txt                      # 2 s, 45k positions, 372k rows, 65 MB
OMP_NUM_THREADS=1 python3 train_prior.py tmp/all.bin --hidden 16 --epochs 20 --l2 1e-3 --wpow 1 \
    --drop 28,29,35,36,37 --out prior_h16c.txt                       # ~20 s
# held-out version: records held out + 15% of the other tours
./gen --out tmp/split.bin --val-frac 0.15 --rng 7 ../../data/tours/{curriculum,openings,record_7066,record_7200,tour_6303,from_scratch_2952}.txt \
    --val-file ../../data/tours/record_7911.txt --val-file ../../data/tours/record_8002.txt --val-file ../../data/tours/record_8123.txt
```
Self-check: `gen` replays every tour with the engine and reports best 8123 for the record.

## Smoke run (done, 1 thread, nice 15, machine fully loaded so absolute speeds are ~2-4x low)
```
bash smoke.sh 35 20 "stock|" "prior|--prior prior_h16c.txt --beta 2"
```
Level 2, fresh weights (`--seed-adapt 0 --no-warm`), equal wall time per config:

| anchor | config | time | restarts | mean | best | rollouts/s |
|---|---|---|---|---|---|---|
| 35 | stock | 20 s | 25 | 3,504 | 4,924 | 12,200 |
| 35 | prior_h16c beta 1 | 20 s | 6 | 6,493 | 7,184 | 2,750 |
| 35 | prior_h16c beta 2 | 20 s | 9 | 6,769 | 6,951 | 4,150 |
| 35 | holdout model beta 1 | 20 s | 9 | 6,540 | 6,820 | 4,250 |
| 35 | holdout model beta 2 | 20 s | 12 | 6,697 | 7,315 | 5,550 |
| 35 | holdout model beta 3 | 20 s | 15 | - | 7,054 | 7,000 |
| 35 | prior_h16 (all features) beta 1 | 20 s | 2 | 6,563 | 6,768 | 740 |
| 60 | stock | 15 s | 34 | 5,635 | 6,408 | 21,900 |
| 60 | prior_h16c beta 1 | 15 s | 8 | 7,464 | 7,830 | 4,700 |
| 60 | prior_h16c beta 2 | 15 s | 8 | 7,715 | 7,911 | 5,100 |
| 60 | holdout model beta 2 | 15 s | 12 | 7,681 | 7,813 | 7,600 |

A 4-second level-3 run (2 threads, anchor 60, prior_h16c beta 2, interrupted with Ctrl-C) found 7,930
(`best.txt`, checker ok). The prior costs 4-5x in rollouts/s (cache hit rate 80-92%; per-state cost is
the feature pass plus 16x42 MLP per move), but even at equal time it wins by a lot at level 2.
Ranking accuracy (top-1, held-out positions of 7911/8002/8123's dances): 0.78 vs 0.61 for "most wait",
0.19 uniform.

## Full runs (suggested; not run)
Memory: ~25 MB per thread (prior cache 2^17 entries) + stock NRPA's ~10 MB.
```
mkdir -p tmp
# 1) fresh level-3 searches early in the dance (each restart ~1M rollouts: ~3 min/thread at ~6k/s)
nice -n 5 ./pnrpa --level 3 --threads 4 --time 3600 --seed-tours ../../data/tours/record_8123.txt \
    --anchor-min 35 --anchor-max 46 --seed-adapt 0 --no-warm --prior prior_h16c.txt --beta 2 \
    --pool tmp/pool.txt --pool-min 7500 --out tmp/l3.txt
# 2) fresh level-4-ish searches after move 35: level 4 with 100 iters is ~100M rollouts (4-5 h with the
#    prior), so use --iters 50 at level 4 (12.5M rollouts, ~35-45 min/thread)
nice -n 5 ./pnrpa --level 4 --iters 50 --threads 4 --time 4000 --seed-tours ../../data/tours/record_8123.txt \
    --anchor-min 35 --anchor-max 35 --seed-adapt 0 --no-warm --prior prior_h16c.txt --beta 2 \
    --pool tmp/pool4.txt --pool-min 7500 --out tmp/l4.txt
# A/B: the same command without --prior (stock) for the same time; and the holdout model for diversity
```
Outputs: `--out` (line 1 minutes, line 2 tour, rewritten on every new best), `--pool` (`minutes<TAB>tour`
per finished result >= `--pool-min`), progress on stdout. Check with
`python3 ../../tools/check_tour.py "$(sed -n 2p tmp/l3.txt)"`.
