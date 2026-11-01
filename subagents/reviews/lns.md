# Review: lns (rev_search), 2026-10-31 21:15 UTC

## 1. Status and claims checked
| Claim | Verdict | How checked |
|---|---|---|
| `NEW_RECORD_8130.txt` / `best.txt` = 8,130 | **VERIFIED** | `python3 tools/check_tour.py "$(sed -n 2p subagents/lns/NEW_RECORD_8130.txt)"` gives `moves=156 ok=True T=8130 maxvisits=3 unnecessary=[]` |
| 8,130 uses only standard moves | **VERIFIED** | `python3 subagents/fractional/classify.py subagents/lns/NEW_RECORD_8130.txt` finds 97 pinned waits, 59 zero waits, **0 in-group ("free") sinks**, no fractions. It holds under the strict pinned reading of "every wait must be necessary", so it is a clean record. |
| tmp/keep_8126, run1_best, cand | verified (8126, 8130, 8130) | checker |
| Engine self-check | verified | `tmp/run1.log` replays 8123, 8002 and 7911 exactly |

How it differs from 8,123: the change is in the climb and merge, moves 15-60. There are extra zero hops (h5-h4-h6-h4-h6, a7/a8 shuffles), the climb sits are reorganised (52 c7, 27 c8, 6 d8, 30 d8, 24 c8), and the dance is entered through h5/h6 at move 59 (80, h5). The tour is 156 jumps against 135. From about move 60 on, the dance matches 8,123. This is the first evidence in this round that the **opening/merge region still has headroom**.

## 2. Issues
- **MAJOR: the record cannot be reproduced from source.** The 8,130 came from `lns_v1`, a copy of the binary made at 21:00. `lns.cpp` was edited at 21:06, and the source of `lns_v1` is not kept. Fix: whenever a binary produces a reported result, save its source next to it (`lns_v1.cpp`). Put the exact command in NOTES.md:
  `./lns_v1 --tours record_8123 record_8002 record_7911 --time 1500 --xover 0 --seed 7 --t0 40 --budget 400000 --out tmp/run1_best.txt`.
- **MAJOR (evidence): one seed, one run.** "SA finds +7" is one trajectory: 8126 at 250 s, 8127 at 367 s, 8130 at 575 s. Before claiming that some operator or setting "works", run at least 3 seeds at equal time and report the best and the time-to-first-improvement for each.
- **MINOR: the run is not deterministic even with a fixed seed.** The temperature is a function of wall-clock time (`el()/TIME`), so the same `--seed` follows a different path on a loaded machine. Add a `--iters` schedule (temperature as a function of the iteration count) for reproducible ablations.
- **MINOR: hash collisions in `replay()`.** The `memo` key is `hashS^j^T^pd`, so a collision silently prunes a branch. That costs search, not validity, because `score_full` and the checker catch everything. It is fine as is; just don't describe this search as exhaustive.
- **MINOR: op 5 (reverse) is mostly invalid by construction.** It accounts for about 1-2% of accepted moves (`acc` column 5). Consider giving its share of iterations to ops 1/2, which produced every improvement ("op 2 i 41", "op 1 i 41").

## 3. Recommendation: CONTINUE (highest expected value of the five right now)
This agent holds the only clean record. It is a cheap local method, and the improvement sat in the climb/merge window (i of 32 to 41), which nobody else is searching with this operator set. Don't over-read it, though: 8,130 is +7, a local move. Treat it as a new anchor, not as proof that SA beats structural search.

## 4. Next round
1. **Restart from 8,130.** Use `--tours NEW_RECORD_8130 record_8123 record_8002`, 3 seeds x 20 min, one thread, `nice -n 5`. Success means > 8,130, or a documented plateau (best per seed).
2. **Focus ablation at equal time.** Compare `--focus 15 65 0.7` with uniform `i`, 2 seeds each x 15 min from 8,130. Success means a clear difference in the best and in time-to-first-improvement. If focus wins, the merge region is where to put compute.
3. **Diversity seeds.** `subagents/opening/tmp/full/r_*.txt` holds 300 checker-legal variants of 8,123 (I spot-checked r_0, r_1, r_150 and r_299). They have the same altitudes and minutes but different visit patterns, i.e. freed visits in different places. Feed the 20 with the fewest moves as `--tours` crossover and elite sources (op 7), 15 min. Success means any tour > 8,130.

---
# Round 2 review: lns (rev_portfolio), 2026-11-01, pass 1 (02:20 UTC)

## 1. Claims checked
| Claim | Verdict | Command |
|---|---|---|
| `data/tours/record_8211.txt` = `NEW_RECORD_8211.txt` = 8,211 | **VERIFIED** | `cmp` gives identical files; `tools/check_tour.py` prints `moves=159 ok=True T=8211 maxvisits=3 maxwait=140 unnecessary=[]` |
| 8,211 is strict | **VERIFIED** | `python3 subagents/strict_check.py data/tours/record_8211.txt` prints `STRICT`; `portfolio_analysis.py` finds 0 free waits |
| How it was found | from the log | `tmp/run3_s21.log`, seed 21, `--iters 55000`, seeds = `tmp/div_8123.txt` (12 re-routed 8,123 variants from opening/reroute) plus records, *without* 8,130. Gains: 8126/8127 (op 0) → **8155, 8166 (op 7 crossover)** → **8193 (op 2 at i=113, in the dance)** → 8205 (op 7) → 8206-8210 (ops 0/1/2 at i=30-45) → **8211 (op 7)** at 136 s |
| Reproducibility | in progress | `tmp/repro_s21.log` re-runs seed 21 with the same `--iters`. Its first NEW BEST (8126 at it 7913, op 0 i 42 D 5) matches the original exactly, so the run looks deterministic. Confirm in pass 2 |

## 2. What changed structurally, 8,211 vs 8,123 (`subagents/reviews/portfolio_analysis.py`)
- **Same lineage.** Moves 1-14 (the bottom climb) are identical. The dance ladder order is the same, apart from one inserted block.
- **Climb/merge (moves 15-61) rewritten.** There are 15 pre-dance sits instead of 13: f7 ×3, b8 ×2 and e8 ×2 are new. The tour reaches the dance at move 62 with T=218 (211 before).
  - The first dance sit is **76 at h6 (4 units × 19)**, against 8,123's 19 (1 unit). That is +57.
  - The first ladder groups are bigger and more lopsided: 128/96/140/108 against 120/104/136/112. That is +52 more by dance sit 6.
- **One more sitter in the dance.** The pair d8/e1 was never sat in 8,123's dance; now e1 sits there (block `e1 d1 e1 d1` after the e3 ladder, gain op 2 at i=113). The result is 32 sitting points and 84 dance sits, against 31 and 83.
  - Cost: the middle of the dance loses about 90 min against 8,123 (sits 18-54), because the inserted block shifts the parity of the group sizes.
  - Gain: one extra 68-minute sit at the end.
- **Visit ledger.** 32 sitters × 3 = 96 possible sits; 84 are made. The other 12:
  - 9 are visits spent before the dance (h6 ×2, e1, d1, b4, c4, c2, e2, d2; 8,123 had 7);
  - 1 is a hop on a sitter (f6, the same as 8,123);
  - 2 are unused (b2, c2).
  - Every opposite pair now supplies a sitter.
- **Net +88:** about +116 from the merge and dance entry, about −90 in the middle of the dance, +48 for the extra final sit, and roughly +14 from the small re-phasing around it.

## 3. Issues
- **MAJOR (portfolio): this is one lineage being polished.** 8,130, 8,166, 8,206 and 8,211 all keep 8,123's moves 1-14 and its dance ladder. The jump to 8,211 came from *diversity in the seed set* (the re-routed variants in `div_8123`) combined with op 7 crossover, not from more iterations: the three run2 seeds from 8,130 all plateaued at 8,130. Fix: make "number of distinct starting lineages" the main knob, not the iteration count. Feed op 7 from other families too: 7,200/7,066 (different climb timing), the opening agent's other `r_*` files, and NRPA pool tours from different anchors.
- **MAJOR (evidence): one seed again.** The only success was seed 21 with div seeds; the comparison against run2 (8,130 seeds) is confounded, because the seed set, the focus and the starting tour all changed together. Fix: a 2×2 at equal `--iters`, {div seeds, no div seeds} × {start 8,123, start 8,211}, with 3 seeds each. That is 12 runs × about 4 min at the observed ~230 it/s. Report the best per seed.
- **MINOR: the "gains only in moves 15-60" claim in ROUND2.md is not quite right.** The 8,193 step was op 2 at i=113 (inside the dance) and added the e1 sitter. Keep `--focus` at 0.7, not 1.0. The dance still has 2 unused visits and one possible sitter swap.
- **MINOR:** NOTES.md doesn't describe 8,211 yet. Add the run3 command, the seed set, and the trajectory above.

## 4. Recommendation: CONTINUE (1 core), but diversify the input, not the iterations
Next steps, in order:
1. Confirm `repro_s21` reproduces 8,211 (success: identical trajectory).
2. Run the 2×2 seed-set ablation above.
3. Build a new diversity set from 8,211: `tools/reroute_zero_wait.py`, or ask for opening's reroute on 8,211's first 64 moves. Run 3 seeds × 55k iterations with op 7 drawing from it. Success: > 8,211 in at least 1/3 seeds, or a documented plateau with the best per seed.

**Pass 1 addendum (02:40 UTC):** `tmp/repro_s21.log` reached NEW BEST 8211 at it 32570 (op 7 a 63 c 64 d 89 b 84). That matches the original run's trajectory, so **8,211 reproduces deterministically from `--seed 21 --iters 55000`**. The reproducibility issue is closed. The evidence issue (one seed, confounded comparison) is still open.

---
# Round 2 review: lns, pass 2 (rev_portfolio), 2026-11-01 ~02:55 UTC

## 1. Claims checked
| Claim | Verdict | Command |
|---|---|---|
| `NEW_RECORD_8288.txt` = 8,288 | **VERIFIED, legal under the user's rule** | `check_tour.py` prints `moves=153 ok=True T=8288 maxvisits=3 unnecessary=[]`; `strict_check.py` reports 1 free wait `(111, 24, b3)` |
| How it was found | from the log | `tmp/r5A_s42.log`, v3 `--free 2`, seed 42, 12k iters: NEW BEST 8288 at it 7101 (op 5, a=117, len 5) |
| 8,211 reproduces | **VERIFIED** (pass 1) | `repro_s21` matches the original trajectory |

## 2. Did the 2×2 starting-set test happen? No
- What ran instead:
  - `run4.sh` (v3 vs v2, 5 seeds each) has no logs in tmp/. Either it was not run or its logs were removed.
  - `run5.sh` is a different A/B: free sinks (A) vs strict (B), both v3, the same 9 input tours, 12k iters, seeds 41-45.
- Status at this pass: A s41-44 done; B s41-43 done, s44 running, s45 pending.
  - A: 8241, **8288**, 8241, 8241 (1/4 seeds improve).
  - B: 8241, 8241, 8241 (0/3).
- So the free-sink operator helped in 1 of 4 seeds. That is a single success, not yet a rate. Every other run returned the best input, 8,241.
- The question of whether the *starting set* matters, the one the 8,211 jump raised, is still untested.

## 3. Lineage and convergence
- lns's 8,288 shares only 8,123's first 13 moves with fractional's 8,288. Its merge is the 8,241 (smoke2) merge: it enters the dance at move 59, against move 62 for fractional's tour.
- But its **last 94 squares are identical to fractional's 8,288**, including the same free wait (24 min at a1 → b3). lns's inputs did not contain fractional's 8,288 (it appeared 2 minutes earlier and is not in `run5.sh`).
- So two different searches **converged independently on the same dance-tail improvement**. That is good evidence the improvement is easy to find. It is also evidence that everyone sits in one basin: the 8,123 dance ladder.
- Both tours give exactly 8,288 on top of different merges. So lns's 8,241 merge is worth no more than the 8,211/8,235 merge.

## 4. Issues
- **MAJOR:** NOTES.md promises a "Round 2 section below" that is not there. Add `run4`/`run5`: commands, per-seed bests, CPU time (`cpu_user` is already logged).
- **MAJOR (evidence):** the free vs strict A/B is one hit out of 4. Finish s44/s45, then report n, the median and the max per arm.
- **MAJOR (portfolio):** the 2×2 starting-set test is still the most informative experiment for this agent, because diversity was what produced 8,211. Run it next, at 12k iters × 3 seeds per cell (about 12 runs × 4 min).
- **MINOR:** stack test. Fractional's 8,292 adds a second free wait at move 129 (84 min at d3). Feed `data/tours/record_8292.txt` into lns's inputs; op 7 can transplant that tail onto other merges.

## 5. Recommendation: CONTINUE (1 core)
1. Finish run5 and report it.
2. Run the 2×2 starting-set test with free sinks on.
3. Seed from 8,292 plus a deliberately *different* starting set: re-routed variants of 8,292's first 62 moves, and the 7,200/7,066 families.
