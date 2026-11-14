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
