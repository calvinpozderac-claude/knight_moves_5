# fw93: free waits on the 9,282 record line

## Setup (run everything from subagents/fw93)
- `fnrpa.cpp` = copy of `subagents/fractional/fnrpa.cpp`; build `g++ -O2 -std=c++17 -o tmp/fnrpa fnrpa.cpp`.
  Replay: `./tmp/fnrpa --seed ../../data/tours/record_9282.txt --time 0 --free 1` -> "177 moves, 9282.000 minutes" (exact).
- Inputs `in/s<m>_<md5 of tour line, 8 hex>.txt`: all 32 distinct strict tours in the repo at 9,282 (13), 9,280 (1),
  9,279 (4), 9,275 (14), each checker ok (ok=True, maxvisits<=3, unnecessary=[]).
- `sweep93.py` = `fw84/sweep84.py` (exhaustive insertion sweep, 3 greedy rounds); `sweep_all.sh` runs it over all inputs.
- `fw_jobs.sh JOBFILE ID N UNTIL TAG AMIN AMAX`: fnrpa fw pass (warm 10, level 2, free sinks 1-4), pool >= 9,200.
- `fw93_saver.sh`: every 60 s `newrec.py` checker-verifies every distinct pool/sweep tour above 9,282 -> NEW_RECORD_<m>.txt.

## Results
- **9,286 (checker ok=True, maxvisits=3, unnecessary=[]) `NEW_RECORD_9286.txt`** from record_9282 (ff703337):
  sweep round 1, move 151 (b7 -> c5, in-group hop) free sink 4 units = wait 68 (n=17), which makes the c5 sit
  (64, c4) a 0 wait. The 4-unit sit moves from c5's group (16) to b7's group (17): +4. Sinks s = 1/6..4 at that
  hop all pass the checker (T = 9282 + s; floppy but legal under the ruling). Round 2: nothing more.
  The fnrpa pass (rng 1, 40 s) found the same 9,286.
- **Sweep over all 32 inputs (`tmp/sw/summary.txt`, ~16 s per tour): every input gains exactly +4, always by the same
  hop**: the dance's b7 -> c5 block transition (move 143-155 depending on the tour), (0, c5),(64, c4) ->
  (68, c5),(0, c4). 9,282 -> 9,286 (13 tours, 13 distinct 9,286s in `in2/`), 9,280 -> 9,284, 9,279 -> 9,283,
  9,275 -> 9,279. Rounds 2-3: nothing more on any tour (the b7->c5 hop is the dance's only in-group zero-wait hop;
  the merge's in-group hops give no legal improving insertion).
- **fnrpa pass p2 on the 32 original inputs** (`tmp/jobs_orig.txt`, 8 rngs each (1-8), 15 s, warm 10, level 2, free
  sinks 1-4, anchors 35..end, `tmp/p2/fw_summary.txt`): every run ends at exactly input + 4 (the same b7->c5 sink).
- **fnrpa pass p3 on the 13 distinct 9,286s** (`in2/`, rngs 101-108, 20 s each, same settings): **104/104 runs -> 9,286**,
  ~6,000 finishes, none above 9,286. Zero rng spread: 9,286 is a fixed point of the warm free-sink pass.
- Variation runs p4 on NEW_RECORD_9286 (`fw_var.sh`, 120 s each): opening anchors 1-40 (warm 10 and warm 3), warm 3
  anchors 35+, free sinks up to 8 units: every finish exactly 9,286.

## Round 2 (coordinator: 4 cores, ~75 more min, until 14:00)
- 41 more inputs (all distinct checker-ok strict tours 9,267-9,277 in data/, penalty92/, graft92/, polish92/, incl.
  penalty92's 9,277s, 9,273, 9,270, 9,267). Sweep (`sweep_list.sh`): gains +1..+4 only, always the b7->c5-type hop:
  9,277 -> 9,281 (2), 9,274 -> 9,278, 9,273 -> 9,277 (2), 9,272 -> 9,276 (2), 9,271 -> 9,275 (5) / 9,272 (8),
  9,270 -> 9,274, 9,268 -> 9,269 (13), 9,267 -> 9,271 (6), 9,269: nothing. No tour above 9,286.
- p5 (`fw_queue.sh tmp/q5.txt`, shared queue, up to 4 workers): the 59 improved non-9,282 tours (`in3/`) x 8 rngs
  (301-308, 15 s, anchors 35+), then 8 more rngs on each 9,286 (4 with anchors 120+ = dance only, 4 with 35-120 = merge).
- (Bug, my own: the watcher that was to add workers 2-3 matched its own command line in `pgrep -f`, so p5 ran on
  2 workers until 13:33, then 4.)
- **p5 results (576 runs, all done by 13:52):** the 13 distinct 9,286s x 8 more rngs (dance-only anchors 120+ and
  merge anchors 35-120): 104/104 -> 9,286, ~6,700 finishes at 9,286, none higher. The 59 improved lower tours x 8
  rngs: each stays at its sweep score, except 9,267-line tours lifted to 9,279 (i9271 from 9,267 / i9275 from 9,271:
  9,279 on most rngs, i9271_077a452c 4/8, c783cf9a 0/8): they rejoin the 9,275 + 4 line. Nothing above 9,286.
- Distinct checker-ok finishes in all pools: 74 x 9,286, 27 x 9,285, 5 x 9,284, 39 x 9,283 (each above 9,282 has a
  NEW_RECORD_<m>.txt for the first one found; the rest are in tmp/*/fw_pool_*.txt).

## Conclusion
- Best **9,286**, checker ok=True, maxvisits=3, unnecessary=[] (`NEW_RECORD_9286.txt` = data/tours/record_9286.txt).
- The free-wait gain on this line is a single local move: the sit after the b7 -> c5 block transition moved onto
  b7 (group 17 instead of 16), +4 on every tour with that transition. Everything else is rigid: 0 spread over
  ~470 fnrpa runs (rngs 1-8, 101-112, 119-122, 201-206, 301-308). More needs new dance block orders with in-group
  transitions (gain = sink x (n_prev - n_next)), i.e. a penalty/graft-style dance rebuild with free moves on.
