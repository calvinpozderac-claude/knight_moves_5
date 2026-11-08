# lns84: LNS + polisher + exact tail proof on the 8,484 basin

Status: in progress. **Best so far: 8,526 (strict, checker ok=True maxvisits=3 unnecessary=[]), NEW_RECORD_8526.txt.**

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
