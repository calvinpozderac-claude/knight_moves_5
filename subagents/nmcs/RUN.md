# nmcs: Nested Monte Carlo Search, Lazy NMCS and SP-MCTS

One program, `nmcs.cpp`, with three search methods. All of them use the pinned moves of `src/eng.h`, so
each jump fixes the wait before it.

- **`--method nmcs`**: Cazenave's NMCS, which memorises the best sequence. At level L, every legal move
  is scored by a level L-1 search from the child. The search then plays the next move of the best
  sequence found so far. Level 1 scores each move with `--base-n` playouts (default 1).
- **`--method lnmcs`**: Lazy NMCS. Same as above, but at levels >= `--lazy-min-level` (default 2) each
  move is first rated by `--lazy-n` (4) cheap playouts, using their max (or `--lazy-stat mean`). Only the
  best `--lazy-k` (3) moves get the full level L-1 search, plus the move on the memorised best sequence.
  Finishing moves are always scored exactly.
- **`--method mcts`**: single-player MCTS with max-backup. Selection is PUCT, using the playout softmax
  as the prior; `--uct` switches to UCB1 with unvisited children first, and then `--c 0.3` is the
  better setting. Q is `(1-mix)*max + mix*mean` of the subtree's playout values, normalised to [0,1]
  over the `--span` minutes (1000) below the best value. `--mix 0` is pure max-backup.
  - After `--commit-iters` iterations (2000), the root moves one step along the best sequence and the
    subtree under the new root is kept. With `--commit-iters 0` the root never moves; that setting is
    weak here (3,899).
  - A finishing move is a terminal node. Terminals already visited, and subtrees fully explored, are
    skipped.
- **`--method playout`**: plain playouts, as a baseline.

**Playout policy.** The policy is a softmax over cheap move features, divided by `--tau` (default 0.8).
Each feature has a weight (defaults in brackets):

| Option | Default | Feature |
|---|---|---|
| `--fw` | 0.25 | minutes waited / 100 |
| `--fr` | 10 | h8 is within 2 of the knight's new altitude. `--reach-bfs` uses true zero-wait reachability instead, about 50× slower. |
| `--fv` | 3 | visits left at the target |
| `--fg` | 0.5 | size of the target's group after the move / 64 |
| `--fd` | 10 | "dance step": the sunk group ends 2 below the target |
| `--fo` | 0 | opposite point already out of the groups |

Notes on the policy:
- The fd feature turned out to be the important one. From move 35 of the record, the best of 20,000
  playouts was 1,582 minutes with a uniform policy and about 5,900 with this softmax.
- A playout keeps going past chances to finish but remembers the best one, as in `src/nrpa.cpp`.
- Unfinished playouts score below every finished one: `-1e7 + T*exp(-dist to h8)`. The MCTS value for
  an unfinished playout is `min(-1, -2000 + T*exp(-d))`.

**Start and parallelism.**
- The search starts from a1, or from the position after k moves of a seed tour: `--seed-tours FILE`
  with `--anchor K`, or `--anchor-min A --anchor-max B` to draw k for each restart.
- `--warm` makes the seed's own continuation the starting best, so the search can only improve on it.
- `--threads N` runs N independent searches (root parallelism). Each thread restarts until `--time S`,
  until `--restarts N` restarts in total, or on Ctrl-C/SIGTERM. All of these stop cleanly.

## Build (from the repo root)
```
g++ -O3 -march=native -std=c++17 -pthread -o subagents/nmcs/nmcs subagents/nmcs/nmcs.cpp
```

## Output files
- `--out FILE`: the best finished tour so far, rewritten atomically on each improvement (line 1
  minutes, line 2 tour).
- `--pool FILE`: appends `minutes<TAB>tour` for every distinct top-level improvement and every restart
  result of at least `--pool-min` (7500) minutes.
- Exact check: any new best of at least `--verify-min` (8000) minutes goes through
  `python3 tools/check_tour.py`; set the path with `--checker`, and **run from the repo root**. If it
  passes and beats `--record` (8123), it is written to `--record-dir/NEW_RECORD_<m>.txt`.
- stdout: a line for each new best and the checker's verdict, a line for each restart (skipped for
  restarts under 1 s), and a final summary: playouts per second, the number of finished restarts and
  their mean.

## Smoke run (about 1 minute, 1 thread)
```
mkdir -p subagents/nmcs/tmp
nice -n 15 subagents/nmcs/nmcs --seed-tours data/tours/record_8123.txt --anchor 35 --bench 20000   # playout speed
nice -n 15 subagents/nmcs/nmcs --method nmcs --level 2 --seed-tours data/tours/record_8123.txt --anchor 35 \
    --time 50 --out subagents/nmcs/tmp/smoke.txt --pool subagents/nmcs/tmp/smoke_pool.txt --record-dir subagents/nmcs
python3 tools/check_tour.py "$(sed -n 2p subagents/nmcs/tmp/smoke.txt)"
```

## Full runs (suggested; nothing here has been run for longer than 1 minute)
Memory use:
- NMCS and LNMCS need under 10 MB per thread.
- MCTS needs at most 40 bytes × `--max-nodes` per thread: 160 MB at the default of 4M nodes. A 15 s run
  used 29 MB.

Restart times from move 35, on one core shared with other jobs:

| Method | Time per restart |
|---|---|
| NMCS L2 | about 3 s |
| LNMCS L2 | about 1.5 s |
| LNMCS L3 | estimated 6–10 min |
| NMCS L3 | estimated 40–60 min |
| NMCS L4 | too slow (many hours) |

```
# A) fresh Lazy NMCS level 3 after move 34-41 of the record (like NRPA's fresh level-3/4 searches)
nice -n 5 subagents/nmcs/nmcs --method lnmcs --level 3 --threads 4 --time 3600 \
    --seed-tours data/tours/record_8123.txt --anchor-min 34 --anchor-max 41 \
    --out subagents/nmcs/tmp/lnmcs3.txt --pool subagents/nmcs/tmp/pool.txt --record-dir subagents/nmcs
# B) full NMCS level 3 (one restart ~ 40-60 min per thread)
nice -n 5 subagents/nmcs/nmcs --method nmcs --level 3 --threads 4 --time 4000 \
    --seed-tours data/tours/record_8123.txt --anchor-min 34 --anchor-max 38 \
    --out subagents/nmcs/tmp/nmcs3.txt --pool subagents/nmcs/tmp/pool.txt --record-dir subagents/nmcs
# C) warm polish of the record: re-search random suffixes, can only improve
nice -n 5 subagents/nmcs/nmcs --method nmcs --level 3 --tau 2 --warm --threads 4 --time 3600 \
    --seed-tours data/tours/record_8123.txt --anchor-min 60 --anchor-max 125 \
    --out subagents/nmcs/tmp/warm.txt --pool subagents/nmcs/tmp/pool.txt --record-dir subagents/nmcs
# D) SP-MCTS (PUCT, max-backup) with step commits
nice -n 5 subagents/nmcs/nmcs --method mcts --commit-iters 20000 --threads 4 --time 3600 \
    --seed-tours data/tours/record_8123.txt --anchor-min 34 --anchor-max 41 \
    --out subagents/nmcs/tmp/mcts.txt --pool subagents/nmcs/tmp/pool.txt --record-dir subagents/nmcs
```
Worth sweeping in a full run: `--tau` 0.8 / 1.4 / 2 (at high levels, 2 gave better maxima from move 100),
`--lazy-k` 2–4, and `--base-n` 1–3. Seed with a pool file (with `--seed-top K`) to re-search several tours.

## Smoke-test results (1 thread, nice 15, 4 cores shared with 5 other searches)
Self-check: replaying the 8,123 tour gives 8,123 (the histogram matches at every step). Warm runs keep
8,123, and the checker reports `ok=True maxvisits=3 unnecessary=[]`. Every pool and best tour tested
passed the checker.

Playout speed was 35–50k per second from move 35 and 260–350k per second from move 100 (shorter tours).

From move 35 (fresh search):

| Method | Result |
|---|---|
| playouts (20k) | 11–15% finish; best 5,900 |
| NMCS L1 | 0.015 s per restart, mean 4,908 |
| NMCS L2 | 3 s per restart, mean 6,210, best 6,623 in 50 s |
| LNMCS L2 | best 6,516 in 25 s |
| LNMCS L3 (k=2) | best 6,841 after 30 s (first restart not finished) |
| MCTS PUCT, commits of 1–4k | mean 5,900–6,090 per restart (3–20 s each) |
| MCTS UCT | mean 5,185 |
| MCTS without commits | 3,899 |

From move 100, where the record's own continuation gives 8,123:

| Method | Result |
|---|---|
| NMCS L2 | best 7,827 (tau 0.8) or 7,961 (tau 2) in 25 s |
| NMCS L3, tau 2 | 2.5 s per restart, mean 7,825, best **7,966** in 40 s (`best.txt`) |
| NRPA L3, baseline | 20 s per restart; 7,994 and 7,840 in 40 s |

From move 90, LNMCS L3 reached a best of 7,817 (mean 7,574) in 55 s.
