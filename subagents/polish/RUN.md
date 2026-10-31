# polish: post-processing tour polisher

## What it does
`polish` takes finished tours (a tour file, a pool, or several files) and improves each one in place by
local search. A tour is treated as a **sit skeleton**: the key moves are every move that waits, plus the
final move into h8. Stretches of zero-wait hops join them. Zero-wait hops never change an altitude, so a
stretch can be re-routed along any zero-wait path without changing a minute; only the visits change.
Every step re-plays the rest of the tour through one replay routine. That routine keeps each key move's
target square and post-jump dz and re-derives its wait, so changed group sizes give changed minutes.
It re-routes a stretch with a min-cost zero-wait path (Dijkstra) whenever the old stretch no longer
fits: the knight is on a different square, a hop is illegal, or it would spend a visit that a later key
landing needs.

Steps, iterated until nothing improves:
1. **Re-route** (C++ successor of `tools/reroute_zero_wait.py`). The whole tour is replayed with every
   stretch re-routed, so the minutes stay the same while visits are freed. Four cost variants penalise
   landings on squares that later key moves use, on dance squares, or neither. A variant with fewer
   jumps is adopted directly. The others are kept as starting points for steps 2 and 3.
2. **Sit insertion**: a window search with w = 0. At every move it tries inserting up to `--ext0` (4)
   pinned moves, for example extra sit/hop pairs, then replays the rest with all later waits re-derived.
3. **Window re-search.** For every window of w = 1..`--wmax` (8) moves, a DFS tries every replacement
   of up to w + `--ext` (2) pinned moves. After every sit in the replacement, the rest of the tour is
   replayed. Pruning:
   - visit caps that reserve the landings the rest of the tour still needs;
   - a transposition table;
   - a fast altitude-only replay of the remaining key moves, because the minutes depend only on the
     key moves and not on the routes. Its results are cached by altitude.

   Only candidates that beat the current tour get the full replay with routing. Finishing jumps into
   h8 from any DFS node are candidates too. There is a node limit per window (`--node-limit`).
4. **NRPA re-search of the last N moves**, run once steps 1-3 stop finding gains. It uses level 2, then
   level 3, on N in `--nrpa-tails` (20, 30, 45, 60), with both fresh and warm weights. The code is
   copied from `src/nrpa.cpp`.

A change is accepted if it gives more minutes, or the same minutes with fewer jumps (fewer jumps frees
visits). Every candidate is checked by a full rule validator before it is accepted.

## Build
```
g++ -O3 -march=native -std=c++17 -pthread -o subagents/polish/polish subagents/polish/polish.cpp
```

## Commands (run from `subagents/polish/`)
Smoke run (60 s, 1 thread; on a busy machine this reproduced 8,123 -> 8,127):
```
nice -n 15 ./polish --in ../../data/tours/record_8123.txt --time 60 --no-nrpa --out tmp/b.txt --pool tmp/pool.txt
```
Full run on a pool, for example everything >= 7,500 found by the other agents. It polishes the best 20
tours with about 10 minutes each:
```
nice -n 5 ./polish --in POOLFILE --top 20 --per-tour 600 --time 12000 --threads 4 \
    --wmax 10 --node-limit 2000000 --nrpa-level 3 \
    --out tmp/best.txt --pool tmp/polished_pool.txt --polished tmp/polished_all.txt
```
To push the current best further (single tour, 1 hour):
```
nice -n 5 ./polish --in best.txt --time 3600 --threads 4 --wmax 10 --ext 3 --node-limit 5000000 --out tmp/best.txt --pool tmp/pool.txt
```
You can pipe a tour through it by writing it to a file first. Any line that contains "(" is read as a
tour, so `minutes<TAB>tour`, nrpa's two-line `minutes\ntour`, and bare tours all work. Tours with
fractional waits are skipped (the engine uses integer altitudes).

Options: `--threads N` (windows run in parallel), `--time S` (total), `--per-tour S`, `--top K`,
`--wmax`, `--ext`, `--ext0`, `--node-limit`, `--nrpa-level` (2 or 3), `--nrpa-tails 20,30,...`,
`--no-nrpa`, `--no-window`, `--pen` (routing penalty in the tail replay), `--rng`, `--quiet`.
Ctrl-C/SIGTERM stops cleanly, and every result is already on disk by then.

**Memory**: about 30 MB per thread (transposition table of 2^20 entries plus a key cache).

## Output files
- `--out FILE` (default `best.txt`): the best tour so far (line 1 minutes, line 2 tour), rewritten on
  every improvement.
- `--pool FILE`: every accepted improvement of at least `--pool-min` (7,500) minutes, as
  `minutes<TAB>tour`, appended as it is found.
- `--polished FILE`: the final polished version of each input tour, as `minutes<TAB>tour`.

Check a result with `python3 ../../tools/check_tour.py "$(sed -n 2p tmp/best.txt)"`.

## Smoke-test results (1 thread, nice 15, machine shared with 5 searches, load 5-15)
- Replaying 8,123 through the engine gives 8,123. Re-routing on 7,911 gives 142 -> 136 jumps, which
  matches `tools/reroute_zero_wait.py`. The 8,123 and 8,002 tours are already route-optimal.
- **record_8123: 8,123 -> 8,126 after 26 s -> 8,127 after 51 s**, then same minutes with 143 -> 139
  jumps. It is verified `ok=True maxvisits=3 unnecessary=[]` (`NEW_RECORD_8127.txt`). Both gains came
  from w = 6 windows at moves 33-34, where the merge was reworked. A second 60 s run from the 139-jump
  8,127 found nothing more at node limit 200k.
- Speed: 0.9-1.4 M DFS nodes/s and about 0.5 M tail evaluations/s. A full pass (w = 0..8, about 1,200
  windows, node limit 200k) takes about 60-70 s on one loaded core.
- Self-test on the other tours in data/tours: see NOTES.md.
