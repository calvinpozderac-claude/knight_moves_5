# astar_cpu: running it

CPU-only port of the NN-guided A* (`src/nnastar.cpp` + `nnastar/`). The GPU ResNet is replaced by a
1-hidden-layer MLP (578 hand-made features -> 64 ReLU -> 2 outputs: finish logit, future minutes/1000),
evaluated inline in C++ (sparse first layer, about 3-4 us per position with features on a free core).
It is trained offline with numpy and can be refitted online in C++. The search value is the same as nnastar:
`p_finish^gamma * (T + future) + noise*N(0,1) - novelty penalty`.

## Build
```
g++ -O3 -march=native -std=c++17 -pthread -o subagents/astar_cpu/astar_cpu subagents/astar_cpu/astar_cpu.cpp
subagents/astar_cpu/astar_cpu check data/tours/record_8123.txt   # self-check: "8123 minutes, 135 moves"
subagents/astar_cpu/astar_cpu selftest                            # bitboard reach == eng.h static_reach
```
Run everything from the repo root (the new-record check calls `python3 tools/check_tour.py`).

## Pipeline (one command)
`subagents/astar_cpu/run_all.sh` runs gen -> feat -> train -> search into `subagents/astar_cpu/tmp/run/`.
Options are environment variables (defaults in brackets): `THREADS` [2] expansion threads (and gen/feat threads),
`GEN_THREADS` [=THREADS], `GEN_MIN` [25], `TRAIN_MIN` [15], `SEARCH_MIN` [60], `HIDDEN` [64], `FEAT_MAX` [600000],
`PREFIX` [30:40] (moves of `SEED_TOUR` [record_8123] kept, drawn per restart; `0` = from a1), `LABEL_THREADS` [1],
`REFIT` [300] seconds between online weight updates (0 = off), `NICE` [5], `EXTRA_TOURS` (more seed tours for gen,
e.g. `"runs/pool.txt:20"`), `RUN` (output folder). Extra arguments go to `astar_cpu search`.
An existing `gen.bin` / `model.txt` in the run folder is reused, so a second search is just a re-run.

**Full run on a free 4-core box** (about 1h45; heavy threads: 4 during gen, 1 during train, 2+1+1 during search):
```
GEN_THREADS=4 THREADS=2 GEN_MIN=25 TRAIN_MIN=15 SEARCH_MIN=60 subagents/astar_cpu/run_all.sh
# then more searches with the same model, e.g. from a1 with a depth bonus, or other prefixes:
RUN=subagents/astar_cpu/tmp/run PREFIX=0 SEARCH_MIN=60 subagents/astar_cpu/run_all.sh --depth-bonus 5 --out subagents/astar_cpu/tmp/run/best_a1.txt
RUN=subagents/astar_cpu/tmp/run PREFIX=34:36 SEARCH_MIN=60 subagents/astar_cpu/run_all.sh --noise 40
```
On a 32-core machine: `GEN_THREADS=30 THREADS=24 LABEL_THREADS=6 ...` (gen 25 min gives ~10x the data).

**Memory:** gen.bin about 200 B per sample (~1M samples per 25 min x 4 threads). Training holds the feature
matrix: 2.3 KB per sample (600k samples = 1.4 GB). Search with defaults: ~1.7 GB (60M nodes x 12 B, 20M open
x 20 B, 2^25 dedup x 12 B, novelty counters 128 MB) + ~0.7 GB for refit (replay, online ring, best-future map).
Lower with `--max-nodes`, `--max-open`, `--dedup-log2`, `--replay-cap`.

## The modes, by hand
```
D=subagents/astar_cpu; T=data/tours
# 1. data: cut a known tour at a random move, finish with NRPA level 0/1/2/3 (p = 0.25/0.40/0.34/0.01) from fresh
#    weights, label each state with the best final score known reachable from it (suffix max + probe rollouts);
#    also every state of every loaded tour labelled with its score. Appends 204-byte Samples (nnastar layout).
$D/astar_cpu gen --threads 4 --time 1500 --out $D/tmp/run/gen.bin --tours $T/record_8123.txt:100 --tours $T/record_8002.txt:100 \
   --tours $T/record_7911.txt:100 --tours $T/record_7200.txt:50 --tours $T/record_7066.txt:50 --tours $T/tour_6303.txt:30 \
   --tours $T/curriculum.txt:1 --tours $T/openings.txt:0.2        # [--levels 0.25,0.40,0.34,0.01 --seed N]
# 2. features (labels raised to the best future known for the same position; exact duplicates dropped)
$D/astar_cpu feat --in $D/tmp/run/gen.bin --out $D/tmp/run/feat.bin --max 600000 --threads 4
# 3. train (numpy, Adam, early stopping; known-tour positions x5, labels >= 7500 x3)
python3 $D/train.py --feat $D/tmp/run/feat.bin --out $D/tmp/run/model.txt --hidden 64 --minutes 15
# 4. search
$D/astar_cpu search --model $D/tmp/run/model.txt --threads 2 --time 3600 --seed-tour $T/record_8123.txt --prefix 30:40 \
   --label-threads 1 --refit 300 --replay $D/tmp/run/gen.bin --save-model $D/tmp/run/model_refit.txt \
   --restart-stale 1200 --out $D/tmp/run/best.txt --pool $D/tmp/run/pool.txt --record-dir $D
$D/astar_cpu bench --model $D/tmp/run/model.txt     # us per evaluation, and the value along the 8,123 tour
```
Search options (defaults): `--noise 60` (std, minutes), `--novelty-w 10` (minutes per doubling of the expansions
under the ancestors `--novelty-k1 8` / `--novelty-k2 32` moves up), `--gamma 0.5`, `--depth-bonus 0`,
`--batch 1024` (pops per thread per round), `--max-nodes 60M --max-open 20M --dedup-log2 25`,
`--restart-stale S` (0 = never), `--p-label 0.02` (share of expanded states sent to label workers, NRPA
`--label-level 1`, `--label-level-shallow 2` before `--shallow-depth 36`), `--refit S` (0 = off), `--refit-lr 1e-4`,
`--refit-batch 256`, `--online-frac 0.3` (share of each batch from new samples vs `--replay` files),
`--p-tree 0.3` (share of the path states of an improving A* finish added as samples), `--rescore 200000` (open
entries re-scored after each refit; the rest are shifted by the median change), `--pool-min 7500`,
`--record 8123`, `--rng N`. Without `--model` it runs on a fallback heuristic (T + 60 min per optimistic sit).

**Outputs:** `--out` best tour so far (line 1 minutes, line 2 tour; rewritten on every improvement), `--pool`
appended `minutes<TAB>tour` lines for every distinct finished tour >= `--pool-min` (from A* or label workers),
`--save-model` the refitted weights, and for a tour > `--record` that `tools/check_tour.py` accepts,
`<record-dir>/NEW_RECORD_<minutes>.txt`. Every tour is replayed through the engine before it is written.
Ctrl-C / SIGTERM / `--time` stop cleanly (threads joined, files closed).

## Smoke-test results (machine fully loaded: load ~12 on 4 cores, everything at nice 15, 1-2 threads)
- `check`: 8,123 tour -> 8123 minutes; `selftest`: 0 mismatches on 5,398 states.
- `bench`: 11 us per evaluation (features + MLP 64) under that load (features alone 10-17 us before the
  bitboard reach; the MLP alone ~4.6 us); expect about 3-4 us on a free core.
- `gen`, 1 thread, 45 s: 79 NRPA paths (28 finished), 103k samples (most are the ~1,900 known tours' states).
  `feat`: 50k unique samples. `train.py` 0.6-0.8 min: val loss 0.19-0.20, finish accuracy 0.99, future MAE ~285 min.
- `search` with that tiny model, prefix 35 of 8,123, 1 thread, 40 s: 4,014 expansions/s (~10 children each),
  best 4,091. 2 threads with tiny node caps (300k nodes, forces gc + prune every few seconds), 30 s: 6,481 exp/s,
  best **5,063** (`best.txt`, checker ok=True maxvisits=3 unnecessary=[]).
- Full pipeline (`run_all.sh`, 1 thread + 1 label thread + refit every 15 s, 45 s search): 626-867 exp/s
  (3 threads sharing a fraction of one core), 238-432 label jobs, 2,005 refit steps, re-scoring 20k entries
  1-3 s, best 3,662 (checker ok). From a1, 20 s: no finish yet (it is still breadth-first in the opening).
None of these are meaningful scores: the model saw 45 s of data. The real test is the full run above.
