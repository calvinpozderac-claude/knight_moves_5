# astar_cpu: notes

## What it is
A CPU-only version of the NN-guided A* that found 8,123 (`src/nnastar.cpp`, `nnastar/`). Everything that needed
the GPU is replaced:
- **Value model**: a 1-hidden-layer MLP (default 64 ReLU units) over 578 features. Two heads, the same as
  nnastar: a finish logit, and future minutes / 1000. Search value is `sigmoid(logit)^gamma * (T + 1000*max(fut,0))`.
  The feature normalisation is folded into the first layer, and zero inputs are skipped (most inputs are one-hot).
  Cost per position, features included: about 11 us on this loaded machine, roughly 3-4 us on a free core.
- **Features** (`features()` in `astar_cpu.cpp`):
  - 32 scalars: T, depth, total visits left, knight-h8 altitude gap, the knight's group size, counts at relative
    altitude +-1..4, h8's group size, static reachability (h8 reachable, reach size, visits left in reach, max
    altitude), opposite-pair counts (both / one member with visits left, sum of max and min visits), the
    optimistic dance bound, number of moves, number of zero-wait moves, and h8 neighbours reachable.
    The dance bound is the sits from groups L and L-2 among reachable points, where each opposite pair gives the
    visits of one member. It is computed for L = the knight's altitude, for L = the knight's altitude + 2, and as
    a maximum over L of sits x 2 x size, the minutes estimate.
  - 8 per-point planes of 64: visits left, knight one-hot, altitude relative to the knight (clipped to +-8),
    whether the point is in the knight's group, the group 2 below or the group 2 above, whether it is statically
    reachable, and altitude relative to h8.
  - 34 histogram entries: points and visits left at each relative altitude from -8 to +8.
- **Data** (`gen`): the same method as nnastar's seed jobs. It cuts a known tour at a random move and finishes it
  with NRPA level 0/1/2/3 (probabilities 0.25/0.40/0.34/0.01) from fresh weights. Each state is labelled with a
  suffix maximum plus probe rollouts, and the prefix states also get the seed tour's score. Every state of every
  known tour is added too (records, 6,303, curriculum, openings). `feat` raises each label to the best future
  known for the same position (nnastar's best-future table) and drops duplicates.
- **Training**: numpy (`train.py`) with Adam, the nnastar loss (BCE + smooth-L1 with beta 0.5), early stopping,
  and higher weights for elite positions.
- **Search**: the same as nnastar's engine:
  - 12-byte nodes; a state is rebuilt by replaying its path.
  - a max-heap open list, capped, that drops its worst entries.
  - dedup by (alt, vis, pos) that keeps the entry with more minutes.
  - novelty penalty: 10 min for each doubling of the expansions under the ancestors 8 and 32 moves up, applied lazily.
  - Gaussian noise of 60 min.
  - gc and prune when memory fills; restart after S seconds without a new best.

  Each round pops 1,024 nodes per thread. The threads expand them and score the children inline, then one serial
  merge pushes the children. Other parts:
  - label workers run NRPA from a share of the expanded states. Their finished tours go to the pool as well.
  - the online refit thread trains with Adam in C++ on a replay of gen samples, label-worker samples and the
    paths of A* finishes that raised a node's best. It publishes weights every `--refit` seconds, and the
    search re-scores the top 200k open entries then.
- **Output**: best tour, a pool of tours >= 7,500 (`minutes<TAB>tour`), and NEW_RECORD files that are checked with
  `tools/check_tour.py`. Every tour is replayed through the engine before it is written.

## Results
Smoke tests only. The CPU was fully used by the other agents, so the jobs ran for 60 s or less at nice 15.
The model had seen 45 s of generated data.
- Self-check: the 8,123 tour replays to 8,123. The bitboard reach matches `static_reach` on 5,398 states.
- Best from a 30 s smoke search after move 35 of 8,123 (2 threads): **5,063**, verified (`best.txt`). From a1 in
  20 s: no finish yet.
- Throughput under that load was 4,000-7,000 expansions/s per thread, with ~10 children each.
See RUN.md for the details.

## What to watch in the full run / next steps
- The model is the weak point. 25 min of 4-thread gen gives about 10k NRPA paths, a few hundred times the smoke
  data. If the search stalls, first add more gen time and seed gen with the pools of other agents
  (`EXTRA_TOURS="path/pool.txt:20"`): the 7,500+ tours matter most.
- The first refit moves values a lot (median shift -2,000 to -5,000 min in the smoke tests). The online labels come
  from weak NRPA level 1/2 runs, and those are lower bounds. The search orders by value, so a uniform shift is
  harmless. If the ordering degrades, lower `--online-frac` or `--refit-lr`, or set `REFIT=0`.
- Knobs to try: `--depth-bonus 2..10` (dives deeper, especially from a1); `--noise 30..90`; `PREFIX=34:36`
  (right after the climb) or `PREFIX=0` (from a1, where nnastar found 8,123); `HIDDEN=128`.
- Possible upgrades:
  - an incremental state cache, to avoid replaying ~100 moves per expansion.
  - a second hidden layer.
  - features for "visits left in the dance groups" restricted to the orthogonal-neighbour graph.
