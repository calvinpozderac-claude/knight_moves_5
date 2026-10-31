# exact: exhaustive suffix DP (sound) + heuristic branch-and-bound (unproven)

Best: **8,123** (no improvement). `best.txt` is a copy of `data/tours/record_8123.txt`, re-verified with
`ok=True maxvisits=3 unnecessary=[]`.

## Method
`bb.cpp` (+ `common.h`, includes `../../src/eng.h`) runs a depth-first search over **all** pinned moves of
the integer engine (`pinned_moves(s, 3, allowH8)`), starting from the state after move k of a tour.
- **Exhaustive mode (`--nobound`) is sound.** A memoised full enumeration. The transposition table (2^25
  entries) is keyed by two independent 64-bit hashes of (altitudes relative to the knight, visits,
  position). That key is valid because the future of a state does not depend on T or on the path, and the
  rules only use altitude differences. Each entry stores an `exact` flag: a subtree that was fully
  enumerated stores its exact optimum, and a hit returns that value whatever alpha is (DP over distinct
  states). Engine = checker: in an integer-altitude tour, every necessary wait is a pinned move.
  The only remaining risk is a double 128-bit hash collision.
- Move ordering: the known tour's move comes first, then the others by wait.
- **Heuristic bound `ub_future` (UNSOUND, label every result that uses it "unproven").** It is a "dance
  relaxation": two levels, sitters in the knight's component, one sitter per opposite pair, U/L
  alternation, 4·n per sit. `--check` (with `--nobound --alpha -100000000`) compares it with exact values
  on every enumerated state. It fails on about 1–2% of them, e.g. exact 241 > bound 140 at T=7676 on f6
  (off-dance and endgame states). I did not fix it this round. See the review `subagents/reviews/exact.md`
  for the likely causes.
- `--lambda L` prunes when T + L·bound <= alpha. This is a heuristic search on purpose.

## Sound results: exhaustive suffix optimum ("headroom table")
The optimum over **every legal continuation** of the tour's first k moves:

| tour | k | prefix T | exhaustive optimum | time | nodes |
|---|---|---|---|---|---|
| 8123 | 134, 130, 128, 126, 124, 122, 120 | | 8123 | <=1 s | <=0.6M |
| 8123 | 118 | 6992 | 8123 | 60–194 s | 78M |
| 8123 | **116** | 6916 | **8123** | 226 s | 130M |
| 8130 (`subagents/lns/NEW_RECORD_8130.txt`) | 148, 144, 142, 140, 138, 136, 134, 132 | | 8130 | <=16 s | <=6.3M |
| 8130 | **130** | 6763 | **8130** | 213 s | 118M |
| 8130 | 128 | 6599 | timeout (300 s, 203M nodes), nothing better found | | |
| 8002 | 120, 118 | | 8002 | 4 s / 35 s | |
| 7911 | 128, 126 | | 7911 | <=1 s | |

**Proven-optimal suffixes:** 8,123 from move **116** on (its last 19 moves), and 8,130 from move **130** on
(its last 26 moves). No tour that keeps those prefixes beats them. The 8,130 state after move 130 (knight
on d1, altitude -119) matches the 8,123 state after move 113: both have exactly 1,367 minutes left.
Running the exhaustive search from earlier points costs about 10–20 times more per 2 moves, so with
5 minutes per run the frontier is about k=114 (8123) and k=128 (8130).

## Unproven results (they rely on the heuristic bound)
- 8123, k=112 and k=108: no improvement found (0.2 s, 1.1 s). k=104: timeout after 240 s. k=104 with
  lambda 0.9: no improvement found (36 s).
- On the 4 cases checked (8002 k=118/120, 7911 k=126/128), the bounded search returned the same optimum
  as the exhaustive one.

## Reproduce
```
cd subagents/exact && g++ -O2 -march=native -o bb bb.cpp && g++ -O2 -o stats stats.cpp
./bb ../../data/tours/record_8123.txt 116 --nobound --alpha 0 --tt 25        # sound, ~4 min
./table.sh ../lns/NEW_RECORD_8130.txt 300 134 132 130                       # sound rows for 8130
./bb ../../data/tours/record_8123.txt 122 --nobound --check --alpha -100000000   # bound violation stats
./bb ../../data/tours/record_8123.txt 108 --tt 24                            # heuristic bound (unproven)
./stats ../../data/tours/record_8123.txt     # per move: T, group sizes, remaining visits
```
Logs: `table8130.log`, `table8130b.log`, `table8123.log`, `sweep1.log`, `sweep2.log`.

## Next ideas
1. **Stop polishing tails.** The last 19–26 moves of both record tours are exactly optimal. Any gain has to
   come from the merge or the early or middle dance.
2. **A bound that is sound by construction.** One sound fact to start from: over the future waits,
   sum(delta) <= K0 + 2W + 2J. Here K0 is the knight's height above the global minimum, W the number of
   waits and J the number of jumps. It holds because the global minimum drops by at most 2 per wait and
   each jump climbs at most 2. So T_future <= N_max·(K0 + 2W + 2J). What is missing is a sound N_max (the
   largest future group) and a sound J (only points that can be reached). Both need a proof that the
   points far above the dance can never be reached: they only rise unless the knight stands at their
   level. Use the bound only where that proof holds, and fall back to enumeration elsewhere. Then check it
   with `--check` to 0 violations.
3. **Use exact search as a finisher.** Solve the last ~20 moves exactly at every NRPA/LNS leaf. A suffix
   from k≈130 of 8130 takes seconds to minutes, and a shorter suffix takes milliseconds.
4. **Macro moves** (sit + landing + up to 2 zero-wait hops) for the middle dance. The dance bound can be
   made admissible within that model, so the middle dance could be solved exactly in the model.
