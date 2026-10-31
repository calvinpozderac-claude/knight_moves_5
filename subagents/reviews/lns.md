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
