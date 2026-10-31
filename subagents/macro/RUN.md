# macro: macro-move search (sit + min-cost zero-wait hops)

## Method
- **Macro move:** one sit (a pinned move with a positive wait), then the zero-wait hops to the next sitting
  square r.
  - **Hop paths:** a minimum-cost path from the landing square q to r (Dijkstra over zero-wait jumps).
    Entering v costs 1, plus `--use-w` (2) when v is "useful" (within 2 units of the knight's altitude).
  - **Path rules:** paths never enter h8, never revisit a square with 3 visits, and have at most `--max-hops` (4)
    hops.
  - **Root:** a pure-hop (no-sit) macro is also offered at the root.
  - **Finishes:** every pinned jump into h8 is recorded, from the knight's square and from every square the
    hops can reach. Rollouts keep going and return the best finish they passed.
  - **Pruning:** a sit after which h8 can't be reached any more is dropped (a cheap check; `--no-h8-prune`
    turns it off).
- **`--algo nrpa`** (default): NRPA over macros, picking each macro in two stages.
  - **Stage 1, the sit:** code (from, to, delta, visits of to); prior `--sit-bias`.
  - **Stage 2, the endpoint r:** code (from, to, delta, r, visits of r); prior `-hop-pen × path cost`.
  - **Restarts:** each one keeps k raw moves of a seed (`--anchor-min/max`), or starts at a1.
  - **Warm start:** `--seed-adapt N` / `--warm` start from the seed's own continuation, decomposed into macros.
  - **Tail polish:** after every finished restart, the DFS below re-searches the last `--tail` macros
    (`--polish-nodes` budget).
- **`--algo dfs`:** branch and bound over the last K macros of the best tour (seed). K sweeps from
  `--tail-min` to `--tail-max`, spread over the threads.
  - **Bound:** T + idealised dance (the future part of nrpa.cpp's `dance_pot`, plus 4 × group for the final
    jump), plus `--slack`.
  - **Search:** transposition table; `--dfs-nodes` budget per K. "complete" in the log means the tail is
    optimal over the macro space.
  - **Admissibility:** along the record's last 30 macros the bound is 290–530 minutes above the truth.
    Outside a two-group dance there is no pruning. A negative `--slack` is a heuristic: tighter, but no
    longer exact.
- **`--algo beam`:** beam over macros, ranked by T + idealised dance, deduplicated, with `--width` and `--noise`.
- **Outputs:**
  - `--out` (line 1 minutes, line 2 tour) is rewritten on every new best.
  - `--pool` gets every distinct finished tour ≥ `--pool-min` (7500) as `minutes<TAB>tour`.
  - A best above `--record` (8123) is run through `tools/check_tour.py`. If the checker accepts it, it also goes
    to `NEW_RECORD_<m>.txt` next to `--out`.
  - Every tour is replayed by an internal validator before it is written.
- **Stopping:** `--time`, SIGINT or SIGTERM stop the run cleanly.

## Build
```
g++ -O3 -march=native -std=c++17 -pthread -o subagents/macro/macro subagents/macro/macro.cpp
```

## Self-check (replaying the record gives 8,123)
```
subagents/macro/macro --selfcheck --seed-tours data/tours/record_8123.txt --anchor-min 38
```
- **Replay:** the engine gives 8123.
- **Coverage:** from move 38, the record is 83 macros (95 sits in the whole tour). The generator offers all
  83, each with the same hop count.
- **Rebuild:** rebuilding the record from its macros with our min-cost paths gives exactly 8123. The same
  holds from moves 35, 44 and 60.
- **From a1 it fails:** the opening's hop paths differ (seed 39 hops, ours 37), so the rebuild is illegal
  from move 0.
- **Estimate check:** the self-check also prints T + estimate minus final along the tail, to check
  the bound.

## Smoke tests
All runs used 1 thread at `nice -n 15` on a fully loaded machine (load 9 on 4 cores).

| Run | Result |
|---|---|
| NRPA level 2, fresh, after move 44 | 12–18 s per restart (10k rollouts, about 600–700 rollouts/s/thread). Restarts finished 6425, 6295, 6579, 6467, 5947→6088 (+141 from the tail DFS), 5031, 5470 |
| Raw `build/nrpa` level 2, same anchor | 4,200–5,525 at about 1 s per restart (10k rollouts/s) |
| DFS on the record, max-hops 4, slack 0 | Last 12 / 15 / 16 / 17 macros explored completely in 38k / 274k / 388k / 1.35M nodes (0.2 / 0.9 / 1.6 / 5.7 s). No improvement, so the record's last 17 macros are optimal over the macro space. About 3.5× more nodes per extra macro |
| DFS with max-hops 6 | K = 15, 16 complete, same result |
| DFS with `--slack -200` (heuristic) | K = 18–21 complete (≤ 1.3M nodes), no improvement |
| Beam, width 100–300, after move 44 | 6077–6597. The estimate stays flat (8642) along a dance, so it can't rank states until late. Weak |
| From a1, NRPA level 2, 30 s | No finish (as with raw NRPA) |
| Warm (`--seed-adapt 10 --warm`), level 1, anchors 40–70, 2 threads pinned to 1 core | Keeps 8123. The pool is deduplicated |

Every tour written was accepted by `tools/check_tour.py` (8123, 6579, 6597 checked). `best.txt` holds 8123,
the record rebuilt through the macro engine.

## Full runs (when cores are free)
Memory:
- NRPA: about 4 MB × (level + 1) per thread.
- DFS: about 50 bytes per node of budget per thread (20M nodes is about 1 GB).
- Beam: small.

```
B=subagents/macro; mkdir -p $B/runs
# 1) fresh macro-NRPA level 3 after the climb (each restart about 1M rollouts: about 10-25 min on one core),
#    with an exact tail polish of every result
nice -n 5 $B/macro --algo nrpa --level 3 --threads 4 --time 3600 --seed-tours data/tours/record_8123.txt \
   --anchor-min 36 --anchor-max 48 --tail 16 --polish-nodes 1000000 \
   --out $B/runs/l3_best.txt --pool $B/runs/pool.txt > $B/runs/l3.log
# 2) local search around the record (warm), anchors spread over the dance
nice -n 5 $B/macro --algo nrpa --level 2 --threads 4 --time 3600 --seed-tours data/tours/record_8123.txt \
   --anchor-min 40 --anchor-max 100 --seed-adapt 10 --warm --seed-best 0.5 --tail 16 \
   --out $B/runs/warm_best.txt --pool $B/runs/pool.txt > $B/runs/warm.log
# 3) exact / near-exact tail DFS of any tour (for example the best of a pool); slack < 0 is heuristic
nice -n 5 $B/macro --algo dfs --threads 4 --time 3600 --seed-tours $B/runs/pool.txt \
   --tail-min 15 --tail-max 40 --dfs-nodes 20000000 --slack -150 --out $B/runs/dfs_best.txt --pool $B/runs/pool.txt
```
- **`--seed-tours`:** accepts the data/tours format and the pool format (one tour per line, any prefix).
  In dfs mode it works on the best tour (the first seed, then each improvement).
- **Knobs to try:** `--hop-pen` (3; lower means more hops tried), `--max-hops` (4), `--use-w` (2),
  `--sit-bias` (0), `--iters`.
