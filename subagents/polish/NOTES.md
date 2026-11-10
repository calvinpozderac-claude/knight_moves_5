# polish: notes

**Result: 8,127 (new record, +4 over 8,123). Verified by `tools/check_tour.py`: ok=True, maxvisits=3,
unnecessary=[]. File: `NEW_RECORD_8127.txt` (= `best.txt`, 139 jumps).**

It was found by polishing `data/tours/record_8123.txt` for 60 s on one loaded core. 8,123 became
8,126 (a w=6 window at move 34), then 8,127 (a w=6 window at move 33). Two equal-minute windows then
cut the tour from 143 to 139 jumps. The change is in the merge (moves 32-46): the knight now sits on
c7, c8 and d8 in the upper-left before it heads to h5. That costs one dance sit (g2/g3) but raises the
waits around h5/h6 and in the merge.

## Method
See RUN.md for the details and commands. In short:
- The tour is a skeleton of key moves (the waits plus the final jump). Each key keeps its square and
  post-jump dz, and its wait is re-derived. Zero-wait stretches are re-routed by Dijkstra when needed.
- Minutes depend only on the key moves, not the routes. So a window candidate is scored first by a
  cheap altitude-only replay (cached by altitude hash), and only candidates that beat the tour get the
  full replay with routing and visit checks. That made the window DFS about 3x faster, at about
  1 M nodes/s on one core.
- The steps are: re-route variants, sit insertion (w=0, up to 4 new moves), exhaustive window DFS
  (w=1..8, up to w+2 new moves, node limit 200k), and NRPA L2/L3 on the last 20/30/45/60 moves.

## Self-test (60 s each, 1 thread, nice 15, loaded machine; every output verified with the checker)
| input | output | notes |
|---|---|---|
| record_8123 | **8,127** | window w=6 at moves 33-34 |
| record_8002 | 8,002 | no gain; pass reached 1085/1178 windows |
| record_7911 | 7,911 | re-route 142 -> 136 jumps (same as the python tool), no minute gain |
| record_7200 | 7,458 | many window gains |
| record_7066 | 7,413 | |
| tour_6303 | 7,124 | last gain 7,116 -> 7,124 from NRPA L2 on the tail |
| from_scratch_2952 | 5,838 | |
| curriculum top 30 (2 s each) | best 7,365 (from 6,814); most go 6,7xx-7,0xx -> 6,9xx-7,1xx | |

A second 60 s run from 8,127 (139 jumps) found nothing more at node limit 200k. A full pass over
w=0..8 takes about 60-70 s on one loaded core.

## What did not help / caveats
- The 8,123 and 8,002 tours are already route-optimal: all four re-route cost variants reproduce them.
  So step 1 helps only on older tours, for example 7,911.
- NRPA tail re-search found almost nothing on good tours: the window search covers short tails
  exhaustively. Level 3 on a 12-move tail took 20 s, which is wasted, so the default tails are now
  20 or more moves.
- Fractional-wait tours are skipped (integer engine).
- The search is first-improvement and restarts the pass after each gain. Equal-minute changes are
  accepted only when the tour gets shorter (fewer jumps), so the search cannot cycle.

## Recommendation for the next round
1. Pipe every pool (all agents' tours >= 7,500) through polish: `--top 50 --per-tour 300`. On the
   record it gained in 26 s, and the weaker tours gained 200-800 minutes.
2. Run polish on 8,127 with bigger windows and limits: `--wmax 10-12 --ext 3 --node-limit 5e6`,
   4 threads, 1 h. The gains so far are all in the merge (moves 30-50), so a window search that only
   covers moves 25-60 at w up to 14 is worth adding (an `--imin/--imax` option is easy).
3. Chain it with the other searches. Run polish on every NRPA or nnastar result, since NRPA tours that
   look close to the best usually have local slack. Also feed polished tours back to NRPA as anchors
   (`--anchor-min 45`) for fresh dance searches after the new merge.
