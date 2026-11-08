# lns84: LNS + polisher + exact tail proof on the 8,484 basin

Status: finished 07:00. **Best: 8,605** (`NEW_RECORD_8605.txt`, = data/tours/record_8605.txt), checker ok=True maxvisits=3 unnecessary=[], one free wait (48, 25, g6). Earlier best 8,600 had free waits (42, 36, g6) and (49, 25, f4).

## Build (all in subagents/lns84/, against current src/eng.h)
```
g++ -O3 -march=native -std=c++17 -o lns lns.cpp              # copy of subagents/lns/lns.cpp == lns_v3.cpp (free sinks: --free F)
g++ -O3 -march=native -std=c++17 -pthread -o polish polish.cpp  # copy of subagents/polish/polish.cpp
g++ -O2 -march=native -o bb bb.cpp                           # copy of subagents/exact/bb.cpp + common.h
```
Replay check: `lns` reads record_8484 as 8484 (replay 8484) and record_8337 as 8337; `polish` replays 8484
to 8484; `bb tmp/record_8484.txt 158 --nobound` returns 8484. So the eng.h change did not alter scoring.

## Structure of 8,484 (dump.py)
- Climb: moves 1-43 (a1 ... up the h-file to h7/g7/g8/f8), T=214 at move 43.
- Merge: moves 44-66 (f6/e4/f4/h5/h6/f5/f7/f6/f8/d7/d6/f5/f4, down to g4 via h4 (124 at n=31), then
  h2/f1/g3/f5/d6 -> d8 -> b8), T=657 after move 66.
- Dance: moves 67-161, starting b7/b8 with two groups of 31/31; sweeps b7/b8, a7/a8, a6/b6, c6/c7, c8/d8,
  c5/c4, b4/a4, a3/a2, b2/b3, b5/a5, c5/c4, d4..d3, e2/e3, c3/e4, e5/e6, e8/e7, f7/f6 -> h8. Final groups 18/16.
- 8,337 has the same climb shape (differs in moves 1-5 and a few shuffles), its merge is 45-71, dance from 72.

## Exact tail proof on 8,484 (`exact_table.sh 240`, log `tmp/exact_8484.log`)
`bb tmp/record_8484.txt k --nobound --alpha 0 --tt 25 --time 240` (sound exhaustive enumeration over pinned
moves, i.e. strict continuations; free waits are not enumerated):
| k | prefix T | optimum | time | nodes |
|---|---|---|---|---|
| 156, 152, 148, 146, 144, 142, 140 | | 8484 | <= 6.4 s | <= 10M |
| **138** | 7134 | **8484** | 92 s | 154M |
| 136 | | timeout at 240 s (403M nodes), nothing better | | |
So the last 24 moves of 8,484 (from move 138, the e2/e3 block on) are optimal for its prefix. LNS focus was
set to moves 1-137 (`--focus 1 137 0.85`). Note: a change *earlier* in the tour changes the tail's optimum,
which is exactly what happened in the polisher run below.

## Polisher on 8,484 (`queue1.sh`, log `tmp/pol84.log`)
```
nice -n 5 ./polish --in tmp/record_8484.txt --time 1000 --threads 1 --wmax 12 --ext 3 --ext0 4 \
  --node-limit 2000000 --nrpa-level 2 --nrpa-tails 45,60,90 --pool-min 8000 \
  --out tmp/pol84_best.txt --pool tmp/pol84_pool.txt --polished tmp/pol84_final.txt
```
Within the first second (all w=1 passes are fast):
- w=1 at move 60: same 8484, 161 -> 159 jumps; w=1 at move 28: 159 -> 157 jumps (freed visits);
- w=1 at move 49 (merge): 8484 -> **8486**;
- w=1 at move 12: same minutes, 161 -> 159 jumps;
- w=3 at move 140 (dance tail, now re-optimisable because the merge changed): 8486 -> **8526**.
8,526 is strict (`strict_check.py`: STRICT), 160 moves, `check_tour.py`: ok=True T=8526 maxvisits=3 unnecessary=[].
- The polisher then converged (no further gain) at 254 s; its NRPA tails (45/60/90) found nothing.

## Polisher on 8,337 (same flags, 700 s, log `tmp/pol37.log`)
8337 -> 8338 (w=0 sit insertion at move 26, after a reroute 164 -> 154 jumps) -> **8378** (w=1 at move 144,
the same kind of tail rework as in 8,526) -> 8382 (w=8 at 137) -> 8386 (w=10 at 98) -> **8390** (w=10 at 104),
still improving when time ran out (700 s). 8,390 checker-ok, 158 moves. Large windows (w=8-10) inside the
dance paid here, unlike the old basin.

## LNS round 1 (`queue2.sh` -> `run_lns.sh L2 <seed> 25000 1400 <inputs>`) : 8,527 ... 8,600
Command per seed (`run_lns.sh`):
```
nice -n 5 ./lns --tours tmp/start_p84.txt tmp/pol84_pool.txt tmp/start_p37.txt tmp/pol37_pool.txt \
  tmp/record_8484.txt tmp/record_8337.txt --xover 0 --seed S --t0 40 --budget 400000 --focus 1 137 0.85 \
  --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 25000 --time 1400 --out tmp/L2_sS_best.txt
```
start_p84 = polished 8,526; pol84_pool = 8484 (159 and 157 jumps), 8486; start_p37 = polished 8,390;
pol37_pool = 8337 (154 jumps), 8338 x2; plus the two original records. Start tour = 8,526. ~58 it/s.
Neither seed reached its 25k iterations: seed 1 was killed at about it 17.5k (an interruption), seed 2 at
about it 18k (container restart). So there are no `.elite` dumps and no "done" lines. Trajectories
(`tmp/L2_s1.log`, `tmp/L2_s2.log`; every listed tour checker-verified by `saver.sh`, `tmp/saver.log`):

| seed | it | score | operator / window | region |
|---|---|---|---|---|
| 1 | 3,814 | 8,527 | op 2 (detour), i=20, D=2 | climb |
| 1 | 5,053 | 8,529 | op 6 (duplicate block), a=130, len 3 | late dance |
| 1 | 7,814 | 8,530 | op 7 crossover, a=16, donor [16,40), b=35 | climb (donor = 8,337 family) |
| 1 | 16,269 | **8,595** | op 7 crossover, a=142, donor [143,149), b=146 | dance tail (e-file block) |
| 1 | 17,270 | **8,596** | op 2, i=115, D=6 | dance (b5/a5/c5 area) |
| 2 | 960 | 8,555 | op 6, a=140, len 1 | dance tail |
| 2 | 974 | **8,599** | op 0 (window), i=134, D=7 | dance tail |
| 2 | 3,890 | **8,600** | op 2, i=18, D=4 | climb |

Free waits: 8,555/8,595/8,596/8,599 have one, on f4 (the f4->h3/h5 hop after a 25-minute sit); 8,600 adds
(42, 36, g6). All were created by `--free 2` (8,526 is strict).

What changed, 8,526 -> 8,600 (`cmp.py`, difflib over moves):
- climb: moves 17-24 rewritten (g2 sit replaced by f2/h2/f2 shuffles, f1 sit 4, g5 sit 18 instead of f2 24),
  move 4 b1 -> e1;
- merge: moves 40-68 rewritten. The f6/e4/h5/h6/f5/f7 sequence becomes g6 (sit 36, free) -> f6 -> g6 11 ->
  h4 -> h6 -> h5 12 -> f4 25 (free) -> g3 78 -> g2 -> f4, then g5 88 and f2/f3/g3/f5 shuffles;
- dance start and middle unchanged; dance tail: e4/e5 instead of e6/e5 at 154-155, and an inserted
  e6 38 -> e8 36 pair at 160-161, then f7 68.
So the +74 = merge/climb rework (+4 to +5 directly) that changes group sizes so that the tail
(e-file block, 140-160) can take two more sits. The pattern is the same as polish's 8,484 -> 8,526.

## Round 2 (04:52-, 4 threads, after a container restart killed everything)
Code changes (both rebuilt, flags as above):
- `common.h` (bb): the prefix replay accepts free sinks (target in the knight's group, wait multiple of n).
  The enumeration itself is still over pinned moves only, so the proof is for strict continuations.
- `polish.cpp`: free sinks in the input are kept. `validate` accepts a wait on a same-altitude knight move,
  `Seg.fd` stores the sink, and `replay`/`key_minutes_alt` re-apply it. Window DFS still only generates
  pinned moves; necessity is left to `tools/check_tour.py` (saver). Before this, polish skipped every tour with a
  free wait ("wait does not change the jump"), and bb failed to replay 8,600 at move 41.
Scripts: `threadA.sh` (polish 8600 1800 s, then `pol_loop.sh`: polish every new best once),
`threadB.sh` (polish 8596 1200 s, continue 8390 1500 s, then LNS seeds 21-27), `threadC.sh` (exact table on
8600, sets `tmp/focus.txt` to 1..k-1, then LNS seeds 11-19), `threadD.sh` (LNS seeds 12-20).
`lns_thread.sh TAG ITERS SEEDS...`: per seed, copies its inputs to `tmp/in_<TAG>_s<seed>/` (top 6
NEW_RECORDs, polish bests, 8390, the 3 newest `.elite` dumps), so each run is reproducible from that folder
with its seed; 30k iterations, otherwise the round-1 flags. `pol_one.sh`: `--wmax 12 --ext 3 --ext0 4
--node-limit 3000000 --nrpa-level 2 --nrpa-tails 45,60,90`. Event log: `tmp/threads.log`.

### Round 2 results so far (all checker-verified by saver.sh, `tmp/saver.log`)
- exact on 8,600 (`tmp/exact_8600.log`, 300 s per k): optimum 8,600 for k = 164 ... **146** (k=146: 124 s,
  223M nodes); k=144 timed out (536M nodes, nothing better). Focus reset to 1-145.
- polish 8,600 (PA_8600): w=7 at move 16 -> 8,601, w=6 at move 52 -> **8,602**; converged at 437 s.
- polish 8,596 (PB_8596): w=1 at 22 -> 8,598, w=6 at 55 -> 8,599, **w=10 at move 72 (dance start) -> 8,603**.
- polish 8,603 (PA_8603): nothing in 711 s.
- LNS LD s12, LC s11 (30k each): nothing. LD s14: ends at 8,603 (= input). **LD s16: 8,604** at it 7,192
  (op 0, i=15, D=4: climb).
- polish 8,604 (PA_8604): **8,605** (reroute variant, then w=1 at move 25: climb).
- 05:15-05:45: LD s18/s20, LC s13/s15/s17/s19, LB s21 (30k each, focus 1-145): all end at 8,605 or below
  (LD s16 also found a second, differently routed 8,605 at it 29,506, op 0 i=13 D=6: `tmp/keep_8605_LDs16.txt`).
  Polish 8,605 (wmax 12): converged, nothing. `pol_big.sh` (wmax 16, ext 4, ext0 5, node limit 6M) on the
  LDs16 8,605 for 1200 s: nothing.
- Independent lineage LE (`threadB2.sh`, inputs only polished 8,390 + pol37_pool + 8,337, no 8,484 family):
  s31 8,390 -> 8,537, s33 -> 8,548 (30k each). Still 57 below the main line. (The saver wrote
  NEW_RECORD_8520/8524/8532/8536/8537/8548 for these before I raised its threshold to "above the best
  NEW_RECORD so far"; they are not records.)
- LF (focus 1-70 p 0.7, climb/merge emphasis) s41: nothing.
- 05:47-06:08: LF s43/s45 (climb focus), LH s51/s53/s55 (`T0=80`, hotter SA; 19-27 restarts each),
  LB s25/s27: all 8,605, nothing new. Polish of the LE lineage best (8,548): nothing.
- exact on 8,605 (`tmp/exact_8605.log`): optimum 8,605 for every k from 160 down to **138** (k=138: 75 s,
  131M nodes). The last 23 moves of 8,605 are optimal for its prefix.
- fw84's free-wait pass (`fw_thread.sh`: fnrpa copied from fw84, warm 10, level 2, free sinks 1-4, 300 s,
  anchors 35+) on 8,605: rngs 1, 2 -> 8,605.
- 06:10: two more independent lineages from the original records only (`lineage.sh LI 61|62 40000
  record_8484 record_8337`), each followed by a 600 s polish, to get other 86xx families for crossover.
- exact on 8,605 continued (1500 s per k): **k=136 proven** (809 s, 1.41G nodes, optimum 8,605); k=134 timed
  out at 1500 s (2.65G nodes, nothing better). The last 25 moves of 8,605 are optimal for its prefix.
- fw pass: 5 runs (8,605 rngs 1-3, LDs16 8,605 rngs 1-2), every one 8,605 -> 8,605; stopped to free the core.
- Lineages from the original records only (`lineage.sh LI`, 40k): s61 stayed at 8,484 (no improvement at all);
  **s62 reached 8,602 on its own** (it 35,883-37,142: op 7 a=136 -> 8,595, op 2 i=13 -> 8,596, op 2 i=15 ->
  8,599, op 0 i=155 D=7 -> 8,601, op 6 a=154 -> 8,602), polished to 8,603 (PLI_s62). Its climb differs from
  8,605's (moves 20-33: no g2/g4/g5/h5/h4/h2 loop, f3/f4/h5 shuffles and g4/f4/h6 sits instead), the merge is
  the same pattern. So the +100 over 8,484 is reproducible from scratch in ~36k iterations (1 of 2 seeds).
  Continuing that lineage alone (LL s65, 30k): 8,603, nothing.
- LJ s71/s73, LK s72/s74 (30k, inputs incl. the LI s62 elites as crossover donors): all 8,605.
- Lineages LI s63 / s64 (40k, original records only): **8,601 and 8,603** (polished: s64 stays 8,603).
  So 3 of 4 from-scratch seeds land at 8,601-8,603 within 40k iterations, and 1 stays at 8,484.
- LM s81, LN s82 (climb focus), LO s91-s93 (30k, donors incl. all lineage bests): all 8,605.
- Polish of lineage LI s63 (8,601) -> **8,605** (`tmp/keep_8605_PLIs63.txt`, checker ok, 159 moves): a third
  8,605, reached from the original records with no 86xx input. It differs from NEW_RECORD_8605 only in climb
  routing (moves 20-27, 44), so it is the same skeleton, reached independently.
- All jobs stopped at 07:00 (saver included).

## Summary (round 2)
- Records found here: 8,526 (polish, strict) -> 8,595/8,596/8,599/8,600 (LNS round 1, free waits) ->
  8,601/8,602 (polish 8,600) -> 8,603 (polish 8,596, w=10 at move 72) -> 8,604 (LNS LD s16, op 0 i=15) ->
  **8,605** (polish of 8,604, w=1 at move 25; found independently by LD s16 at it 29,506 with other routing).
  Best: `NEW_RECORD_8605.txt` (= data/tours/record_8605.txt), checker ok=True maxvisits=3 unnecessary=[],
  one free wait (48, 25, g6).
- Since 05:15, about 25 LNS runs of 30-40k iterations (seeds 13-93, three focus settings, T0 40 and 80,
  four independent lineages as donors), wide polishing (wmax 12 and 16) and the free-wait pass all ended at
  8,605 or below. This basin looks saturated for these methods at about 8,600-8,605.
- Exact: the last 25 moves of 8,605 (from move 136) are optimal for its prefix (strict continuations).
- Where the gains were: every big step (+42, +70) came from a small merge/climb change (moves 13-60) that
  shifted the dance group sizes, followed by a re-optimised dance tail (moves 130-160, the e-file block).
  Single windows found these only when they spanned both effects in sequence (polish: w=1 in the merge, then
  w=3 in the tail; LNS: crossover/op 0 in the tail after merge drift).
- Lessons: (1) apply the polisher before anything else on a new basin: +42 in under a second;
  (2) a from-scratch LNS lineage reproduces about +100 over 8,484 in ~36k iterations, so these numbers are a
  local ceiling of this basin rather than luck; (3) polish and bb did not support free waits; patched here.

# Round 6: AlphaZero tours (8,537 / 8,545 / 8,549 / 8,553), started 17:33
Folder: `r6/` (all logs; event log `r6/events.log`; saver log `r6/saver.log`). Records go to
`subagents/lns84/NEW_RECORD_<m>.txt` (and a copy in `r6/`). The saver only writes tours above the best
NEW_RECORD so far (8,605) that the checker accepts.
- Rebuilt lns/polish/bb after the merge (src/eng.h unchanged since 01:44). All four tours replay exactly
  (lns: 8537/8545/8549/8553; bb at k=n-2 gives the same totals).
- New `lns_r6` = lns.cpp + `--start FILE`: start the SA from that tour even when a donor (8,605) scores
  higher, so each lineage keeps its own start and the other tours are only crossover donors/elite seeds.
- Scripts: `r6/pol.sh TAG IN TIME [WMAX]`, `r6/lns.sh TAG SEED ITERS START FOCUSFILE DONORS...`,
  `r6/exact.sh TOUR TAG TIME` (writes `r6/focus_TAG.txt`), `r6/saver.sh`.

## Step 1: polish the four tours (4 in parallel, `r6/pol.sh P<t> r6/record_<t>.txt 900 12`)
| tour | polished (900 s) | steps that gained (polish log) |
|---|---|---|
| 8,537 | 8,603 (time up, still improving) | w=0 at 19 (+1); **w=1 at 136 (+42)**; w=6/8 at 100-103 (+4 x3); w=8 at 20 (+3); w=10 at 128, w=8 at 132 (+4 each) |
| 8,545 | 8,604 (time up) | w=0 at 22 (+1); **w=1 at 134 (+42)**; w=4 at 103, w=10 at 127, w=8 at 131, w=10 at 99 (+4 each) |
| 8,549 | **8,607** (time up, still improving) | w=0 at 19 (+1); **w=1 at 132 (+42)**; w=8 at 20 (+3); w=10 at 128, w=8 at 132, **w=10 at 60** (+4 each) |
| 8,553 | 8,604 (converged, 674 s) | w=0 at 20 (+1); **w=1 at 138 (+42)**; w=10 at 127, w=8 at 131 (+4 each) |

**8,607 (polished 8,549) is a new record, strict** (`NEW_RECORD_8607.txt`; checker ok=True maxvisits=3
unnecessary=[]; strict_check: STRICT). Same pattern on all four as on 8,484 -> 8,526: one sit inserted into
the late dance tail is worth +42, then large windows (w=8-10) in the dance (moves 99-132) give +4 each.

## Step 2: exact tail + LNS per lineage (`r6/worker1.sh` for 8553/8545, `r6/worker2.sh` = 600 s more polish
first, for 8549/8537 which were still improving). Seeds: 8553: 101-103, 8549: 111-113, 8537: 121-123,
8545: 131-133. 30k iterations, free sinks on, start = the lineage's polished best (`--start`), donors = the
4 originals, the 4 polished bests, 8,605 and its two variants. Focus 1..(first proven move - 1), p 0.85.
- Q polish (600 s more): 8,549 line stays 8,607 (converged 347 s); **8,537 line 8,603 -> 8,607** (converged
  550 s), a different tour from the 8,549 one (different opening), so 8,607 was reached twice independently.
- Exact tail proofs (`r6/exact.log`, 150 s per k): polished 8553 and 8545 (both 8,604) proven from move 132
  of 156, and their last ~24 moves are identical (same prefix T and node counts at every k: the four
  lineages converge on one dance tail); 8,549 line from move 133/157; 8,537 line from 135/157.
- LNS results so far (all checker-verified, NEW_RECORD files written by `r6/saver.sh`):
  - L8553 s101: 8,604 -> 8,608 (it 13,419, op 2 i=22 D=5) -> **8,609** (it 27,600, op 7 crossover a=15, donor
    [15,45), b=48: a climb transplant). Free wait (48, 25, g6), carried in from the 8,605 donor family.
  - L8553 s102, s103: 8,608 (two more distinct 8,608s). L8545 s131: 8,607 (op 5 a=163) -> 8,608 (op 7 a=81);
    L8545 s132: 8,608 (another distinct one).
  - L8549 s111: 8,607 -> 8,608 (op 7 a=100) -> 8,609 (op 7 a=13 donor [13,40)), identical to the polished
    8,609 (R8609), so it counts once.
  - Polish of 8,609 (R8609, 600 s): same minutes, re-routed (converged 400 s).
- Round B (`r6/roundB.sh`, queued behind each worker): starts 8,609 (two variants) and the 8,537-line 8,607,
  donors = every r6 best + 3 newest elite dumps; seeds 201-203, 211-213, 221-223; plus `r6/bigB.sh`: wmax 16
  polish of the best record (1200 s), then seeds 231-232 from it.
