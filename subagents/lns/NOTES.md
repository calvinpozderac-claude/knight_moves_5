# lns: large-neighbourhood search / simulated annealing over whole tours

**Round 1 results: 8,130 (run1), then 8,211 (run3). Round 2: 8,241 (one free wait, which the checker accepts). See the Round 2 section below.** Verified with
`python3 tools/check_tour.py "$(sed -n 2p subagents/lns/NEW_RECORD_8130.txt)"`, which prints
`moves=156 ok=True T=8130 maxvisits=3 unnecessary=[]`. The same tour is in `best.txt`.
The review also found 0 in-group ("free") sinks and no fractions, so it uses only standard pinned moves.

## Method (`lns.cpp`, built on `../../src/eng.h`)
- **Representation.** A tour is a list of `(target square, dz)`, where dz = alt[q] - alt[p] right after the jump.
  The wait is implied: delta = dz - (alt[q] - alt[p]) must be >= 0. When the sitting square and the target are
  in the same group, only dz = 0 knight moves are allowed. Any edited sequence can therefore be replayed and
  scored exactly; the waits are derived again, so changes earlier in the tour carry through.
- **Repair (`Search`).**
  - `win_dfs`: from the state before move i, a DFS over up to D pinned moves.
  - At every DFS node on the same square as the square before move j of a source tour Y (j in a window),
    `replay(Y[j..])` runs, deriving the waits again ("re-sync").
  - If that replay breaks at move k, `patch` runs a DFS of depth PD (1-3) that tries to re-sync at
    Y[k..k+R]. This is recursive up to 2-4 levels, with a node budget (`--budget`).
  - Every state the search passes is checked for a finishing jump into h8. The best candidate that differs
    from the current tour is returned. The replay memo is a 64-bit hash, so the search is not exhaustive:
    a collision only prunes.
- **Operators** (op ids as in the log):
  - 0: replace a window. DFS depth 1-5, re-sync at j in [i, i+D+2]. Covers deletions and insertions too.
  - 1: skip ahead. DFS depth 1-3, re-sync at j in [i+1, i+12].
  - 2: detour or repeat. DFS depth 2-6, re-sync at j in [i-4, i+4].
  - 3: jump anywhere in the tour itself. DFS depth 1-2, any j.
  - 4: move a segment (or-opt, length up to 24).
  - 5: reverse a segment. This is mostly invalid; in v2, 70% of its share goes to ops 1 and 2.
  - 6: duplicate a dance block.
  - 7 (v2): two-point crossover, cur[0..a) + Z[c..d) + cur[b..], where Z is another record or elite tour.
  - 8 (v2): a random walk of 1-6 moves, then DFS 1-3 and re-sync.
  - Ops 4-7 build a modified Y and replay it with patching at the seams.
- **Acceptance.** Simulated annealing on minutes: accept if dT >= 0, otherwise with probability exp(dT/temp).
  There are 4 geometric cycles from `--t0 40` to `--t1 1`. An elite pool keeps 40 tours, and the search
  restarts from it when the current tour is more than 400 below the best, or when it stalls.
- Single thread, `nice -n 5`, about 100-140 iterations/s on the shared machine.

## Runs and numbers
| run | binary | seeds / start | schedule | best | first improvement |
|---|---|---|---|---|---|
| run1 | `lns_v1` | 8123, 8002, 7911 (start 8123), seed 7 | wall clock, 1500 s | **8,130** | 8126 at 250 s (op 2, i=41, D=5); 8127 at 367 s (op 2, i=32); 8130 at 575 s (op 1, i=41, D=3) |
| run2 s11 | `lns_v2` | 8130 + 8123, 8002, 7911 + 12 elite 8130 variants, focus 1-60 p=0.7 | 55,000 iters (deterministic) | 8,130 | none |
| run2 s12 | same | same | same | 8,130 | none |
| run2 s13 | same | same | same | 8,130 | none |
| run3 s21 | `lns_v2` | 12 variants of 8123 from `subagents/opening/tmp/full/r_0..11` + records, no 8130 | 55,000 iters | **8,211** | 8126 at it 7,913 (see Round 2 section) |
| test | lns | 8123, 30-60 s | wall clock | 8,123 | none |

Other results:
- One-point crossovers between all pairs of 8123, 8002, 7911 and 7200 (prefix of A, DFS depth 2, re-sync
  anywhere in B) found nothing above the better parent.
- A beam search over the tail with an idealised-dance heuristic (`dance_pot2`, still in `lns.cpp`, run with
  `BEAMTEST=200`) was useless. It reached 3.7k-7.5k from mid-dance states, because the heuristic ignores
  adjacency and the beam cannot reach h8.
- Op 2 and op 1 produced every gain. Ops 0 and 3 make most of the plateau moves (equal-minute rewrites).
  Ops 4-7 rarely get accepted.

**How 8,130 differs from 8,123.** From about move 60 on, the dance is the same as 8,123's (it starts at
g2/g1 with groups 35/25-26). The climb/merge in moves 15-60 is rewritten:
- extra zero-wait shuffles (h4/h6, a7/a8/c8),
- the sits around d7/c7/c8/d8/b8 are reorganised (52 c7, 27 c8, 6 d8, 30 d8, 24 c8),
- the dance is entered through h6 -> h5 (80).

The tour is 156 jumps long (8,123 has 135). So the headroom sits in the climb/merge, not in the dance.

## Reproduce
```
cd subagents/lns
g++ -O3 -march=native -std=c++17 -o lns_v1 lns_v1.cpp   # exact source of the binary that found 8,130
nice -n 5 ./lns_v1 --tours ../../data/tours/record_8123.txt ../../data/tours/record_8002.txt \
   ../../data/tours/record_7911.txt --time 1500 --xover 0 --seed 7 --t0 40 --budget 400000 --out tmp/run1_best.txt
```
- `lns_v1.cpp` was reconstructed afterwards by undoing the later edits. Compiling it gives a binary whose
  `.text` is byte-identical to `lns_v1` (`objdump -d`). The only other difference is the embedded source
  file name.
- v1's annealing temperature depends on wall-clock time, so run1's exact trajectory cannot be repeated on a
  loaded machine.
- `lns_v2.cpp` (identical to the current `lns.cpp`) adds `--iters N`, which makes the schedule depend only
  on the iteration count and the seed. It also adds `--focus LO HI P` (with probability P, the window
  start i is drawn from [LO, HI]), ops 7 and 8, and multi-tour input files (up to 12 tours per file).
- Deterministic 3-seed run: `./run2.sh`. Lineage from 8123 variants: `./run3.sh`.
- `saver.sh` checks every `tmp/run*_best.txt` with the exact checker and updates `best.txt` and
  `NEW_RECORD_*.txt`.
- `dump.py FILE [from]` prints group sizes and visits for each move.

## Recommendation for next round
1. **Put the compute on moves 1-60, with bigger neighbourhoods.** The +7 came from op 1/2 windows at i = 32-41.
   - Allow DFS depth 6-8 there, with a wider re-sync range ([i-6, i+20]).
   - Use a larger patch depth (PD 3-4), so that one change in the merge can re-sync with the dance 10-20 moves
     later.
   - Then, from 8,130, take a *visit-freeing* pass (`tools/reroute_zero_wait.py`), followed by SA. 8,130 has
     many zero-wait shuffles that may hold visits back.
2. **Secondary objective for the plateau.** Most accepted moves have equal minutes. Rank ties by free visits
   on points that are still in the final groups, so the SA drifts toward tours with slack that a later
   insertion can turn into sits.
3. **Run more seeds and lineages from different 8123 variants** (run3 style). The deterministic seeds from
   8130 plateaued, so diversity of the starting tour matters more than more iterations.
4. **Combine with structural search.** Give NRPA or nnastar the 8,130 prefix (the first 61 moves) as an
   anchor, or start from LNS-improved merges and re-search the dance.

---
# Round 2 (2026-11-01)

## How 8,211 was found (round 1, run3)
- **Binary and source:** `lns_v2` / `lns_v2.cpp`, the same code as the end of round 1.
- **Command** (`run3.sh`):
  ```
  ./lns_v2 --tours tmp/div_8123.txt ../../data/tours/record_8123.txt ../../data/tours/record_8002.txt \
    ../../data/tours/record_7911.txt --xover 0 --seed 21 --t0 40 --budget 400000 --focus 1 60 0.7 \
    --iters 55000 --time 660 --out tmp/run3_s21_best.txt
  ```
- **Inputs:** `tmp/div_8123.txt` holds 12 checker-legal variants of 8,123 (`subagents/opening/tmp/full/r_0..r_11`).
  They have the same minutes but different visit patterns. 8,130 was *not* an input, so 8,211 is an
  independent lineage from 8,123.
- **Deterministic.** A re-run (`tmp/repro_s21.log`) followed the identical trajectory and output
  (`cmp` identical).

Trajectory (iteration: score, operator and window):

| iteration | score | operator, window |
|---|---|---|
| 7,913 | 8,126 | op 0, i=42, D=5 |
| 11,358 | 8,127 | op 0, i=35 |
| 15,293 | **8,155** | op 7 crossover, a=76, partner [83, 94), b=81 |
| 15,463 | 8,166 | op 7, a=94, partner [90, 92), b=94 |
| 17,278 | **8,193** | op 2, i=113, D=6 (in the dance) |
| 17,327 | **8,205** | op 7, a=51, partner [43, 70), b=76 (merge plus early dance from a partner) |
| 18,096 | 8,206 | op 0, i=34 |
| 24,595 | 8,207 | op 2, i=41 |
| 26,038 | 8,209 | op 2, i=45 |
| 26,577 | 8,210 | op 1, i=30 |
| 32,570 | 8,211 | op 7, a=63, partner [64, 89), b=84 |

- By operator, the +88 over 8,123 splits into op 7 (crossovers with the 8,123 variants and the older records)
  ≈ +57, op 2 ≈ +30 (+27 of it in the dance at i=113), and op 0/1 windows in moves 30-45 ≈ +5.
- The diversity of the starting set (op 7 partners that free visits in different places) made the
  difference. With 8,130 as the only lineage (run2, 3 seeds), the search stayed at 8,130.

**What changed structurally, 8,123 -> 8,211** (`compare.py`, `pervisit.py`):
- Sits went from 96 to 99, and dance sits from 81 to 83.
  - The dance starts at move 65 (8,123: move 45), with T = 294 at its start (8,123: 290).
  - The dance is worth 7,917 minutes (8,123: 7,833).
  - The first dance groups are 35/27 (8,123: 34/28).
- Dance: the two visits left free at the end of 8,123 (d1 and e1) became sits. Sits per point: e1 1->3,
  d1 2->3, c2 2->1. The block is now e1-d1-c1, plus f1->f3 78 and f3->e3 87.
- Climb and merge:
  - sits per point: f7 1->3, b8 0->2, e8 1->2; h5, h6, e7 and g7 each lose one sit;
  - more zero-wait landings: f5, d6, b6, a6, d8 and others.
- Landings went from 136 to 160 in total.

## Round 2 code: `lns_v3` / `lns_v3.cpp` (identical to the current `lns.cpp` except for a debug print)
- **Bigger neighbourhoods:**
  - `--dmax 8`: op 0 uses DFS depth 1-8, op 2 depth 2-9;
  - `--pdmax 4`: patch depth 1-4;
  - `--rmax 8`: patch re-sync range;
  - `--wide 8`: widens the re-sync windows of ops 0-3 and 8 by 8 moves;
  - DFS move order is shuffled with the seeded RNG, so the node budget no longer always truncates the same
    branches.
- **Operators borrowed from polish** (`subagents/polish/polish.cpp`, ported into `Search::route` and
  `Search::skel_replay`):
  - The repair keeps the tour's *key* moves (every move that waits, plus the final one): their square,
    their from-square and their dz (either sign).
  - It re-routes the zero-wait stretches between key moves with min-cost Dijkstra. Landings on squares that
    later key moves need are penalised, and visits are reserved for later key landings.
  - `--skel P`: when a suffix replay breaks, try this skeleton repair with probability P.
  - `--reroute P`: op 9, re-route every stretch after move i. It is accepted if the minutes do not drop
    and the jumps do not grow.
- **Free sinks** (`--free F`), following the user's ruling that free waits are legal when a later move needs
  them:
  - A move can carry `fd` > 0: the knight sinks fd units while the target is in its own group, then makes a
    knight move with dz = 0.
  - The DFS adds these options (fd = 1..F) for every same-group knight target.
  - The parser reads free sinks, so it accepts 8,239, 8,288 and 8,292.
  - Necessity is not checked inside the search. `saver.sh` only promotes tours that `tools/check_tour.py`
    accepts with `unnecessary=[]`.
  - Moves also carry their from-square. This fixed a skeleton bug at segment seams, where a key move was
    tried from the wrong square.

## Round 2 results
Every number is checker-verified (`ok=True maxvisits=3 unnecessary=[]`).

**8,241: smoke test.** Command: `lns_v3 --tours 8239 8235 8211 --seed 1 --free 2 --iters 3000`, about 70 s.
- One free wait: (142, 18, g8).
- The merge is changed: it passes through g5 and drops the f4/d5/a6 shuffle. The dance starts at move 56
  (T = 299), with 84 dance sits and 148 jumps.

**run5: free sinks (A) vs strict (B).**
- Settings: `run5.sh`, 5 seeds each (41-45), 12,000 iterations each, the same 9 inputs (8241, 8239, 8235,
  8211, the 8,211 elite, the 12 8,123 variants, 8123, 8002, 7911).
- "Strict" means no *new* free sinks; the inputs' free waits can still be copied.

| seed | A (--free 2) | B (--free 0) |
|---|---|---|
| 41 | 8,241 | 8,241 |
| 42 | **8,288** (it 6,212: 8,261 by op 2, i=141, D=9; it 7,101: 8,288 by op 5 reverse, a=117, len 5) | 8,241 |
| 43 | 8,241 | 8,241 |
| 44 | 8,241 | 8,255 (op 3, i=93; then op 2, i=43, D=6) |
| 45 | 8,241 | 8,241 |
| CPU-seconds per run | 190-218 | 140-155 |

- At equal iterations, 1/5 seeds improve in each arm. A costs about 40% more CPU per iteration.
- 8,288 has one free wait, (111, 24, b3). Its gains came in the **dance** (i = 117-141), not the merge.
- The reviewer notes that fractional's 8,288 has the same last 94 squares, so the two agents converged on
  this tail.

**How 8,288 was found.**
- Run: run5 arm A, seed 42. Command (from `run5.sh`):
  ```
  ./lns_v3 --tours <the 9 inputs> --xover 0 --seed 42 --t0 40 --budget 400000 --focus 1 60 0.7
    --dmax 8 --pdmax 4 --rmax 8 --wide 8 --skel 0.5 --reroute 0.05 --free 2 --iters 12000
  ```
- Start: the 8,241, whose merge goes through g5 (T = 299 at the dance start).
- Two dance moves:
  - it 6,212: op 2 at i=141 with DFS depth 9, giving 8,261;
  - it 7,101: op 5 (segment reversal) at a=117, length 5, giving 8,288.
- The result has a new free sink on a1 (the a1->b3 hop waits 24, and a1 sits 0->1).
- Against 8,239: g8 sits 2->3, g6 sits 1->2, h6 and f7 lose a sit each, and there are 6 fewer landings in the
  merge (f4, d5, a6 and c3 are not visited).

**How 8,297 was found.**
- Run: run7 (`run7.sh`), a 2×2 at 6,000 iterations: {start 8,123, start 8,292} × {with div seeds (the 12
  8,123 variants + 7,200 + 7,066), without}, seeds 61 and 62, free sinks on.
- Two cells reached 8,297 independently:
  - start 8,292 + div, seed 61: op 2 at i=37, D=3, at it 2,577;
  - start 8,292 without div, seed 62: op 6 at a=31, len 2, at it 193.

  The two tours differ in their zero-wait routing, but have the same minutes.
- What changed: 8,297 = 8,292's dance (the same 7,998 dance minutes, 85 dance sits, both of fractional's
  free waits: (108, 24, b3) and (122, 84, d3)) + the 8,241-style merge.
  - That merge goes through g5/a7/c8, and skips the f4/d5/a6 shuffle and the h6 sit.
  - The dance now starts at move 58 instead of 65, with T = 299 instead of 294.
  - That is +5 minutes, with 7 fewer landings.

**run7: full table** (best per run; CPU about 91-100 s each).

| start | div | seed 61 | seed 62 |
|---|---|---|---|
| 8,123 | yes | 8,131 | 8,127 |
| 8,123 | no | 8,130 | 8,130 |
| 8,292 | yes | **8,297** | 8,292 |
| 8,292 | no | 8,293 | **8,297** |

At 6k iterations, diversity of the starting set made no visible difference. From 8,123, every run finds
+4 to +8 within about 4k iterations. From 8,292, the merge transplant (+5) is found in 2 of 4 runs.

**run6: continuation from 8,288.** Stopped after a few minutes, without a result, to make room for the 2×2
test. The partial log is `tmp/r6_s51_f2_killed.log`.

**run4: v3 vs v2.** Started, then stopped within a minute when the free-wait ruling arrived, and its logs
were deleted. While it ran, it produced a strict 8,233 (v3, seed 31), which `saver.sh` verified
(`tmp/saver2.log`). Not used as evidence.

---
# Round 3 (2026-11-01, 07:13-)

## 2×2 starting-set test (`run8.sh`)
- Settings: free sinks on (`--free 2`), `lns_v3`, 9,000 iterations per run, seeds 71-73,
  deterministic, one thread. About 140-168 CPU-seconds per run.
- Start A = 8,329 (prior's record, in the 8,211 family). Start B = 7,200: its bottom climb leaves 8,123's at
  move 6, so it is outside the 8,211 family.
- Diverse sets:
  - for A: the outside tours (7,066, 6,303, and 10 curriculum tours whose openings leave 8,123's within the
    first 9 moves: `tmp/curr_outside.txt`, 283-4,841 minutes), 12 8,123 variants with re-routed climbs
    (`tmp/outside_climbs.txt`, built from opening's `m1s*` prefixes plus 8,123's dance; all checker-legal),
    8,002 and 7,911;
  - for B: only the outside tours (all below 7,200, so the start stays outside the family).
  - "no div" = the start tour alone (op 7 then crosses over with its own elite pool).
- Note on the outside sources: opening's `full/r_*` variants all share 8,123's first 13 moves, and its
  `rr44_*` re-routed climbs could not be completed with 8,123's dance (every one breaks a visit cap). That
  is why curriculum tours and 7,200/7,066/6,303 were used.

| cell | s71 | s72 | s73 | spread |
|---|---|---|---|---|
| A (8,329) + div | 8,329 | 8,329 | 8,329 | +0 / +0 / +0 |
| A (8,329), no div | 8,329 | 8,329 | **8,330** | +0 / +0 / +1 |
| B (7,200) + div | 7,577 | 7,392 | 7,611 | +377 / +192 / +411 |
| B (7,200), no div | 7,430 | 7,504 | 7,501 | +230 / +304 / +301 |

Reading:
- From 8,329, the method is at a plateau: 1/6 runs improve, by +1, and the diverse set does not help.
- From 7,200, every run climbs by 190-410 in 9k iterations; with div the mean is +327, without it +278
  (n=3, a large overlap, so this is not significant).
- No outside-family run got near 8,000 at this budget, so the outside basin would need far more compute
  (see run9).

## 8,330 (new record, +1)
- Run: `run8.sh` cell A_nodiv, seed 73, at it 2,389, op 7 (crossover with an elite tour, a=137, partner
  [138, 150), b=147).
- The check gives `moves=151 ok=True T=8330 maxvisits=3 unnecessary=[]`. Free waits (the same two as 8,329):
  (132, 80, d4) and (145, 72, g8). Saved as `NEW_RECORD_8330.txt` and `best.txt`.
- **What changed against 8,329:**
  - The dance is identical: from h3 onwards it is the same sequence of squares, and the dance is worth
    8,035 minutes in both.
  - The climb and merge are rewritten from move 23 on: e7 waits 2 (after f7), d7 52, f7 18, d8 7, c8 48, then
    g6 -> h4 -> h5 -> h6 and 80 at h5, entering h3 at move 56 instead of 65.
  - T at the dance start is 295 (8,329: 294), and the first dance groups are 30/26+ instead of 32/24.
  - Landings drop by 9 (152 vs 161). Sits per point: d7 2->3, d8 1->2, f7 3->2. a6 and b7 are no longer
    visited.
- The SA had drifted to a different merge, and the crossover copied 8,329's dance tail back onto it.

## run9: the 7,200-family basin, 30k iterations, 3 seeds (`run9.sh` seed 81, `run9b.sh` seeds 82-83)
- Start: run8's B_div s73 best (7,611; its bottom climb comes from 7,200). Donors: 7,200, 7,066, 6,303 and
  the 10 outside curriculum tours. No tour from the 8,123 family was an input.
- Settings: `lns_v3`, `--free 2`, the same flags as run8, 30,000 iterations, about 515-560 CPU-seconds each.

| seed | best | dance start | T at dance start | dance sits | dance minutes | first moves shared with 8,123 |
|---|---|---|---|---|---|---|
| 81 | 7,998 | move 59 | 303 | 85 | 7,695 | 12 |
| 82 | **8,103** | move 49 | 303 | 87 | 7,800 | 14 |
| 83 | 8,060 | move 57 | 291 | 85 | 7,769 | 14 |

All three are checker-legal (`ok=True`, `unnecessary=[]`). They are
3 different finishes.

- Every seed climbs +390 to +490 over 30k iterations, and is still improving near the end: seed 81 gained
  +4 in its last 1,200 iterations.
- **All three seeds drifted back onto 8,123's bottom climb** (the first 12-14 moves) without any 8,123-family
  input. The b1-d1-d2-c2-a1-b1-c1-e1-e2-d4-c4-b4 start looks like a strong attractor, not an accident of
  seeding.
- The upper climb, merge and dance stay different. Seed 82 has the most dance sits seen so far (87; 8,330 has
  83) and the highest T at the dance start (303). Its dance order is worse, though: 7,800 dance minutes
  against 8,035 for 8,330.

## run11: 8,330 + run9's tours as op-7 donors (2 seeds × 6k)
- Both seeds stay at 8,330. Crossover did not transplant anything useful from the outside basin in 6k
  iterations.
