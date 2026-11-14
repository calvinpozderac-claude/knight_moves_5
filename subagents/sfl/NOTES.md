# sfl: sound free-wait LNS (start 13:55, 2026-11-14; 4 heavy threads, nice -n 5)

Goal: a tour above 9,286 with free waits inside the search, every free sink exact-necessity checked.

## Code (`sfl.cpp`, build: `g++ -O3 -march=native -std=c++17 -o sfl sfl.cpp`)
Copy of `subagents/lns84/lns.cpp` (round 6: `--start`, `add_free`, `--free F`) plus:
- `nec_ok(X)`: exact necessity check of every free sink (move with fd > 0 = in-group sink before a dz-0 knight
  move to a member of the knight's group). Replays the tour from the state before the sink with the probes of
  `tools/sim.py` necessity_check (t' = 0, t+1/997, t-1/997, t+1, 2t; every later wait kept in MINUTES, as the
  checker does) on exact rational altitudes (__int128 integers over a growing common denominator D; overflow
  (D > 2^90) = "unknown" = not proven, so the test can only err toward rejecting). A probe that makes any later
  jump illegal proves the sink necessary; a probe stops early once the perturbed altitudes rejoin the
  unperturbed trajectory. Pinned waits (delta > 0 to reach a legal dz) break at once under t +- 1/997 and are not
  re-tested.
- `Search::record` (the single choke point every finish passes: window DFS, replay/patch, skeleton reroute,
  crossovers, beam) accepts a finish only if `nec_ok`; failing tours are rejected (the search keeps the next best
  finish). `save()` re-checks before writing.
- `--fsp 1`: greedy free-sink pass (fw93's insertion sweep, inside the search) on every candidate within
  `--fspgate` (60) of the current tour: at each in-group zero-wait hop try a free sink of 0..FREEMAX units (or
  change an existing one), later moves keep their dz, keep the best exact-necessary gain, 3 rounds. This is the
  bias toward block orders with big -> small in-group transitions: a reorder (op 4 segment move, op 7 crossover,
  window DFS) that creates such a hop is scored with its free sink already in place.
- `--dump FILE`: every accepted tour with free sinks (validation sample). Test modes: `CHECKONLY=file` (verdict per
  tour line; `VERB=1` per free sink), `GENTEST2=p` (systematic 1-2 sink mutants), `NONEC=1` (+`RECDUMP=p`) disables
  the check in record() and samples raw candidates.
- Scripts: `run.sh` (one LNS job), `worker.sh` + `queue.txt` (4 workers), `sfl_saver.sh` + `newrec_sfl.py`
  (every 60 s, checker-verifies every distinct tour > 9,286 in runs/ pol/ -> NEW_RECORD_<m>.txt), `val.py`
  (sfl verdict vs check_tour on dump files).

## Validation of the necessity check (val.py: sfl verdict vs check_tour ok=True, maxvisits<=3, unnecessary=[])
- families' invalid tours (lns `--free 2`): X3 8,649 sfl flags exactly moves 137-146, 149-160 (= checker's list),
  X2 8,478 exactly 152-156 (= checker's list); the other free sinks in them pass in both.
- 260 distinct sink-subset mutants of X2/X3 (all BAD) + 23 mutants of strict/9,286 tours (OK): 283/283 agree.
- NONEC runs (check off), 831 accepted tours + 309 sampled record() candidates with free sinks: 1,140/1,140 agree
  (1 BAD).
- Sound runs (check on, fsp on, 90 s each from 9,286 and from 9,282): **581/581 dumped outputs pass check_tour**.
  (fsp on the strict 9,282 reproduces fw93's 9,286 at iteration 0.)
- Cost: ~70-80 it/s per thread with the check and fsp on (vs ~85 without).

## Runs
Common flags (`run.sh`): `--xover 0 --budget 400000 --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05
--fsp 1 --iters 40000`, `--free 4` (6 for D jobs), `--t0 40` (unless noted), focus window p = 0.85. ~10 min per job.
Batch A (from record_9286, donors 3 ties): A1 (s101, focus 95-177) **9,288** at it 2.5k (op 7 crossover); A2-A4
(s102 merge 35-100, s103 dance, s104 merge): 9,286.
- 9,288 (`NEW_RECORD_9288.txt`, checker ok, unnecessary=[]): 181 moves, 4 free sinks: f1->h2 (moved from h2's sit,
  gain 0), h2->f1 25 (n 25) and g1->e2 8 (n 8) in the MERGE (moves 64-92 rerouted vs 9,286), plus b7->c5 68.
  The new gain is in the merge, which the after-search fw pass could never reach (it needs a reroute + sinks).
- E1/E2 (s401/402 from 9,288, focus 95-177 / 35-177): 9,288. 123 distinct checker-ok 9,288s in runs/.
- B1-B2 (spread ties 6a5881e0 / 7f23be50): 9,286. **B3 (s203, tie 0bfba284, focus 95-177, donor 9,286): 9,290** at
  it 17k (op 7 a 94 c 93 d 118: crossover in the merge). `NEW_RECORD_9290.txt`: 178 moves, free sinks g1->e2 11
  (n 11), c1->d3 36 (n 36, move 92) and b7->c5 68. Checker ok=True, maxvisits=3, unnecessary=[].
- fw93's sweep93.py on 9,290: no legal single variant (its repin profile does not handle the merge free sinks); the
  in-search fsp pass is the polisher instead.
- Queued F1-F4 from 9,290 (s501 35-177, s502 60-130, s503 95-177, s504 1-177 p 0.5 t0 60).
