# graft92: crossovers and merge engineering (Round 9)

Goal: a tour above 9,275. 1 heavy thread, nice -n 5, `--free 0`. Start 23:16 (2026-11-13).

## Tools (this folder)
- `bin/lns_r6`, `bin/dlns`, `bin/polish`, `bin/bb`: copies of `subagents/families/bin`.
- `bin/graft` (`bin/graft.cpp` = dlns.cpp +): `--gw W --gref G --grange LO HI` rewards W per point of the largest
  sit group (any wait > 0, move index in [LO,HI)) above G; "feasible" (`*.feas_best.txt`) then means gmax > G and
  no rule hit. `--sitmin` applies to `--rule`. `POTDUMP=1` prints per move: group sizes and lns's idealised
  dance potential `dance_pot2`.
- `lns.sh` (plain lns_r6, `--free 0`), `ylns.sh` (bin/graft with penalties/rewards), `queue1.sh`,
  `graft92_saver.sh` (lns/, y/, x/ above 9,275 that check_tour accepts -> `NEW_RECORD_<m>.txt`; heartbeat
  `saver.alive`). Event log `events.log`.

## Analysis
- Direct crossovers (lns_r6 `--xover 1`: prefix of A + DFS + resync anywhere in B, all 9 pairs of 8,830..9,275):
  nothing above the parents (best combos just reproduce the B tour; 8,830 x 8,946 -> 8,952 only).
- 9,275's merge spends f3 (3 visits), g3 (3), e3 (3), e4 (2) in moves 55-95 (small sits of 25-60 min). Those
  are exactly 9,080's dance sitters, so a 9,275-prefix + 9,080-dance graft is infeasible as is.
- Group sizes at dance sits (n = minutes/4): 9,275 starts U/L = 37/25 (a2/b2) and decays to 19/16; L hits 16 by
  c3 and stays there. 9,080 starts 31/31 and decays slowly to 17/17. 9,268 starts 36/27.
- dance_pot2 (idealised dance value) max along tour vs score: 9,275 9,661; 9,271 9,661; 9,268 9,722;
  9,080 9,471; 8,946 9,414; 8,900 9,393; 8,830 9,370. Gap score-pot: 386-540 (correlates with score).

## Runs
- G1 s901 (23:18, 40 min): plain lns_r6 from 9,275, donors 9,080, 8,946, 8,900, 9,271, 9,268 (cross-family),
  focus 40-100 p 0.5.
- queue1 (after G1): YA = graft penalty from 9,275 (no sits >= 20 min on f3,g3,e3,e4 in moves 60-95, i.e. keep
  them for the dance; no b6,c6,d5,d6 sits from move 96, i.e. 9,080's sitter swap), pen 150, donors 9,080 + 9,275
  family; then plain LNS on its feasible best. YG = group reward gw 100 over gmax 37; then plain LNS.
- G1 stopped after 6 min (24k it, no gain over 9,275) to free the core for the splices below.
- **Splice scan** (bin/graft `XODETAIL`: per prefix length k, best of prefix + DFS(2) + resync anywhere in the
  donor, prefixes 9,275 / 9,271 / 9,268 x every tour in data/tours): with k in 50-115 only two donors give
  > 8,900, both at k = 93 (end of the new merge, groups 37/27):
  - **H1 = 9,275 merge + 8,946 dance: 9,266** (strict, checker ok) `in/H1_9266.txt`; sig f3/f7 g6/h3 d3/d4 c3/c4
    b2/b3 a2/a3 a4/b4 a5/b5 a6/b6 c5/c6 c5/d5 c6/c7 c7/c8 c8/d8 a8/b8 a7/b7 d6/d7 e7/e8 f8/g8 (close to 9,268's).
  - **H2 = 9,275 merge + 8,900 dance: 9,212** (strict) `in/H2_9212.txt`; sig f3/f7 g6/h3 d3/d4 c5/d5 c6/c7 c4/c5
    c3/c4 b2/b3 a2/a3 a4/b4 b5/b6 a5/a6 a7/b7 a8/b8 c7/c8 c8/d8 d6/d7 e7/e8 f8/g8 (new: 8,900's order on the new
    merge; dance_pot2 9,662 = 9,275's).
  - 9,275 merge + 9,080 dance: 7,354 (k = 93; the dance runs out of f3/g3/e3 visits). F5/F6/8,842/8,887 dances:
    < 5,800.
- LH2 s931 (plain LNS from H2, donors H2 + 8,900, 25 min, 143k it): **no gain at all, 9,212**.
- LH1 s941 (plain LNS from H1, donors H1 + 8,946, 20 min): 9,266 -> 9,268, and the signature drifts back to
  9,268's (a7/b7 a8/b8 c8/d6): H1 is just a variant of the 9,268 family.
- Splices of 9,275's merge with penalty92's 9,1xx-9,267 tours (same merge already): nothing new.
- **LH3 s951 (00:11, plain LNS repair of the 7,354 splice = 9,275 merge + 9,080 dance, donors splice + 9,080 +
  9,275): 7,354 -> 8,812 in 7 s -> 9,105 (50 s) -> 9,230 (96 s) -> 9,253 (6 min).** It does NOT keep 9,080's
  e/f sitters; it rebuilds an a-d dance in a new order: f3/f7 g6/h3 d3/d4 c3/c4 a3/b3 b2/b3 a2/a3 a2/b2 a3/a4
  a4/b4 a5/b5 a6/b6 **b7/b8 a7/b7 a8/b8 a7/c7 c6/c7 c5/d5 c6/c7 c8/d8** d6/d7 e7/e8 f8/g8 ("H3 family").
- 00:19 queue4 (YG) cancelled before it started; queue5 = two more plain-LNS seeds on the H3 family (donors: LH3
  best + LH3 elite).

## Restart (00:30, 2026-11-14, after container restart)
- LH3 s951 had reached **9,264** before the restart (checker ok; `keep/LH3_9264.txt`, `in/LH3best.txt`); sig now
  f3/f7 g6/h3 d3/d4 c3/c4 b2/b3 a2/a3 a4/b4 b5/b6 a5/a6 a7/b7 a8/b8 c6/c7 c5/d5 c6/c7 c8/d8 d6/d7 e7/e8 f8/g8.
  No elite file survived; H3 donors = in/LH3best.txt (9,264) + in/LH3_9242.txt.
- Saver restarted. queue5b = H3 seed 952 (20 min, from 9,264). queue6 = repaired splices (7 min each) then H3 seeds
  953, 954 (20 min each, chained).
- Splice scan redone (`sp/`, XODETAIL=1, all k): 9,275 prefix + donor dance at k ~ 93: 8,946 k93 9,266 (H1),
  9,080 k93 7,354 (H3), 8,830 k93 3,947, 8,842 k94 3,008; 8,887 and 8,813 (F6) have no resync for k 79-106, so
  use 8,887 k107 (4,084) and 8,813 k101 (4,083); also 8,946 k102 (8,373) as a second 8,946 splice.
  Splice inputs `in/S_<donor>_k<k>.txt`.
