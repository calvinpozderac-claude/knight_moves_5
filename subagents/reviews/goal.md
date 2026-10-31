# Review of `goal/` (reviewer rev_code_b, 2026-10-31, round 1b)

Code reviewed: `subagents/goal/goal.cpp`, which has the subcommands design / search / score / selfcheck
and the scorers ideal, bound2, bound3, dbeam and dnrpa, plus `tmp/design/targets.txt`. I built my own
copy. No RUN.md yet.

## 1. Status and claims checked
- **Selfcheck, verified.** `goal selfcheck` replays 8123 exactly ("135 moves, 8123 minutes, ends on h8,
  legal yes") and the text round trip works.
- **Is the dance-designer bound calibrated? No.** Measured with `goal score`:

  **8123 tour** (`--tour data/tours/record_8123.txt`):

  | Move | ideal | bound2 | bound3 | dbeam(300) | dnrpa(L1×2) |
  |---:|---:|---:|---:|---:|---:|
  | 42 (first dance start, where the designer anchors `base_k42`) | 7474 | **6457** | **7212** | 4246 | 4900 |
  | 45 | 8366 | 8047 | 8209 | — | — |
  | 46 onward (plateau) | — | 8217 | 8217 | — | — |

  The real tour reached **8123** from move 42. So "bound2" and "bound3" are not bounds: they are
  1,666 and 911 minutes too low at the state the designer uses.

  **7911 tour:** move 46 gives bound3 7290; it plateaus at 8217 from move 50. That is the **same
  plateau as 8123**, so the scorer cannot tell a 7,911 opening from an 8,123 one. The difference we
  need to resolve is about 100 minutes.

  **lns 8130:** the first dance start, at move 51, is rated bound3 **6451**, against 8130 achieved.

  **README:** an opening rated 8,725 reached only 7,621, an error in the other direction.

  So the error is about ±1,000 in both directions.
- **targets.txt.** Variants are ranked by these scores, for example "single_move_g5_to_the_lower_group"
  rated 8225 against base 7212. A +1,013 difference from a one-square edit is within the scorer's
  error, so the ranking carries no usable signal yet.

## 2. Issues
**BLOCKER for the design → search pipeline: the target score has no demonstrated relation to the
achieved result.** Fix it in this order:
1. **Build a calibration set.**
   - The dance starts of every distinct verified tour ≥ 7,500 in data/ and the pools (hundreds of
     tours, grouped by opening), with their best achieved minutes.
   - The 8,725 / 7,621 opening.
   - The eight level-4 results from the README.
   - Report Spearman ρ and residuals for each scorer.
2. **Score the settled state.** Score at the state after the merge has finished, for example the max
   over k of the score along the prefix, or the first k after which it is constant. Not the first
   detected dance start, which comes before the last joins (moves 42 → 45 change everything above).
3. **Use a strong scorer.** Use an NRPA L3 dance solver from the dance start, about 30 s, as the scorer
   for the top 5–10 designs. dnrpa L1×2 (4,900) and dbeam(300) (4,246) are far below what the dance is
   worth, so they are not informative lower bounds.

**MAJOR: designed targets are hypothetical states.**
- Edited altitudes and visits may be unreachable from a1, and the search's distance weights (`--w-alt`,
  `--w-vis`, `--w-knight`) are uncalibrated.
- Fix: first run a **recall test**. Can `goal search` from a1, or from k=23 of 8123, reach the record's
  own `base_k42` state, or one scoring as well, within 60 s? If it can't reach a known-reachable target,
  it won't reach a designed one.

**MINOR**
- **Speed.** `score` takes 4–8 s per state with dbeam on. Make dbeam opt-in for tables.
- **Input parsing.** `parse_moves` accepts waits that aren't multiples of the group size (`d = w/n`
  truncates). Reject them, as polish does, or a fractional or garbled input becomes a different tour
  without warning.
- **Output.** The prefix output for `build/nrpa --prefix-file` is a good interface, but document it in
  RUN.md with an example. Every finished tour must also pass `tools/check_tour.py` before it goes to
  the pool.

## 3. Recommendation: CONTINUE, but the main job is calibration
A new opening or merge is the genuinely different direction the portfolio needs: README says the 7,911
dance was already sit-optimal. But "design a better dance start" only works with a scorer that ranks
real outcomes, and today none of the five does.

## 4. Next round
1. **Calibration table** (all scorers over ≥ 50 distinct verified tours ≥ 7,500, grouped by opening;
   cheap scorers only, < 60 s total). Success: one scorer has Spearman ρ ≥ 0.6 with achieved minutes,
   and an upper-bound-style scorer is ≥ achieved on every tour. If neither holds, fix the scorer before
   any design.
2. **Recall test:** `goal search` from k=23 of 8123 toward base_k42, 60 s. Success: it reaches a dance
   start whose settled bound3 is ≥ the record's own.
3. **Then design:** take only the top 3 designs by the calibrated scorer and finish each with
   `build/nrpa --prefix-file` L3. Run them when cores free up: 3 × 30 min, 1 thread. Success: any
   prefix whose finish beats the record's dance from the same T.
