# Review of `prior/` (reviewer rev_code_b, 2026-10-31, round 1b)

Code reviewed: `gen.cpp` (training data), `prior.h` (42 move features plus an MLP or linear scorer),
`train_prior.py` (listwise softmax in numpy), `pnrpa.cpp` (GNRPA, a copy of src/nrpa.cpp with a learned
bias), and `smoke.sh` with its logs in tmp/. These files were still changing while I reviewed. No RUN.md
yet.

## 1. Status and claims checked
- **GNRPA update, correct.** `adapt()` uses the biased probabilities `e = exp(w) · exp(β·prior)` in the
  gradient, which is the correct GNRPA update. The per-state cache keyed by `hashS ^ nmoves` is fine:
  a collision only gives a wrong soft bias, never an illegal move.
- **Training metrics, reproduced.** I ran
  `train_prior.py tmp/split.bin --hidden 0 --epochs 5` (linear model). Of 44,988 positions, 36,192 are
  train and 8,796 val:

  | Positions | Count (train / val) | Val top-1 (model) | Val top-1 (max-wait baseline) | Val top-1 (uniform) |
  |---|---|---:|---:|---:|
  | dance (≥ move 35, L ≥ 7000) | **236 / 340** | 0.700 | 0.626 | 0.190 |
  | opening | — | 0.305 | 0.061 (max-wait) / 0.314 (zero-wait) | — |

  So the learned part adds about 7 points over "pick the longest wait" in the dance, and nothing over
  "zero wait" in the opening. The dance numbers rest on a handful of tours (a dance is about 100
  positions).
- **Raw rollout speed.** `NRPA_BENCH=5000`, anchor 40, zero policy:
  - stock: 15,617 rollouts/s;
  - `prior_h16c` β=1: 507/s;
  - `prior_lin` β=1: 283/s.
  In real L2 runs, where the cache hit rate is 0.75–0.79, your logs show 2.7–3.1k/s against 12.2k/s
  stock. That is a **4–4.5× slowdown**, which is acceptable if the prior helps.
- **Smoke results (your logs).** L2, 20 s, anchor 35 of 8123, fresh weights:
  - stock: best 4,924;
  - β=0.5: best 5,994;
  - β=1: best 7,184.
  Equal wall time is the right protocol. But this is one seed, and see the leakage issue below.

## 2. Issues
**BLOCKER for the smoke comparison: test-on-train leakage.**
- The smoke runs search from anchor 35 of `record_8123.txt`. If 8123, 8002, 7911 or their curriculum
  relatives are in the tours `all.bin`/`split.bin` were built from, the prior has seen the exact dance
  it is being asked to find. The prior files don't record which tour files went in.
- `--val-frac` splits **tours at random**. Long tours share a family, with the same climb and near-equal
  dances, so the val dance positions are near-duplicates of train positions.
- Fix:
  - Split by family: hash the first 35 moves and hold out whole openings. Better still, leave one
    lineage out: train without every tour ≥ 7,500 that shares the 8123 climb, then test search from
    8123's anchors.
  - Write the list of input files and split hashes into the prior file's header.

**MAJOR: label semantics teach imitation, not discovery.**
- A move with no known tour through its child gets target 0. With the `(L/8123)^2` weight, the loss is
  dominated by the record family and actively pushes probability *away from* unexplored moves. A record
  needs exactly those moves.
- Fix: compute the softmax only over labelled moves (mask the unlabelled ones), or label every child
  with a cheap NRPA L1 probe, as in nnastar's gen phase. Then the targets say "how good", not "was it
  in a known tour".

**MAJOR: the evidence so far is L2 / 20 s; records come from L3–L4 / about 1 h.**
- A strong prior is expected to win at L2: it is a better default rollout. At L4, adaptation dominates,
  and a sharp prior can collapse diversity onto the training family. That would fit the README's
  "openings rated higher did worse".
- The experiment that would settle it is listed in section 4.

**MINOR**
- **Pool format.** The pool is written as `minutes\ntour` (inherited from nrpa). The brief asks for
  `minutes<TAB>tour`. The runner copes, but please follow the brief.
- **Thread default.** `--threads` defaults to hardware/2. Default to 1 while the CPU is shared.
- **Feature cost.** The expensive features are `static_reach` and the child's `pinned_moves` (f28,
  f29, f35–37). Try an ablation with `--drop` and see whether a cheap-feature model keeps the gain.
- **Free alternative.** Distil the prior into initial per-code weights (`pol.w[code] = β ·` mean prior
  for that code). That costs nothing at run time and is a useful baseline.

## 3. Recommendation: CONTINUE
This is the most promising of the five code-ready agents on current evidence: a +2,260 best at equal
L2 time. But the gain must survive a leakage-free split and a run at the level where records are found.
Don't move other agents onto it until then.

## 4. Next round
1. **Rebuild the data without leakage** (minutes). Exclude every tour of the 8123 climb family ≥ 7,500,
   and retrain h16c with masked unlabelled moves. Report val-family top-1 against max-wait.
2. **Equal-time A/B at L2** with that prior: stock, `--bias 1`, prior β∈{0.5, 1, 2}; anchors {35, 40,
   46} of 8123; 4 seeds each; 60 s per run. Run it now: 15 configurations × 4 seeds is about 1 CPU-hour,
   so queue it through the runner. Success: the prior's median best beats `--bias 1` at every anchor.
3. **If (2) holds:** L3, 600 s per run, 3 seeds, anchors {35, 40}, against stock L3 at the same time.
   Success: the prior's median is ≥ stock's. Only then try L4.

---
# Round 2 review: prior (rev_portfolio), 2026-11-01, pass 1 (02:20 UTC)

## 1. Status
Running now (`ab.sh`): an equal-time L3 A/B. Stock and `prior_h16c_holdout.txt` β=2 run concurrently, 720 s each, at anchors 40, 55 and 70 of 8,211, fresh weights, `--rng 11`, one thread each, nice 5. No results yet.

## 2. Issues with the A/B design (fix before drawing conclusions)
- **MAJOR: one rng per anchor.** Each L3 restart is an independent fresh sample, so a 720 s run gives about 10-15 stock samples (≈50 s per 1M rollouts) but only 2-4 prior samples (4-5× slower). "Best of the run" for the prior arm is a best-of-3. Report, per anchor:
  - the per-restart distribution for each arm (n, median, max; count only completed restarts);
  - best-at-equal-time;
  - the median time to reach 8,000.

  Then repeat with a second rng (`--rng 12`). That costs another 2 × 3 × 12 min = 72 core-min. Compare "best within 720 s", not mean per restart: the prior wins on means by construction, because its rollouts are better, which says nothing about finding records.
- **MAJOR: what the anchors test.** In 8,211 the dance starts at move 62.
  - Anchor 40 re-searches the climb end + merge + dance. This is the valuable one: that region holds the headroom.
  - Anchor 55 starts inside the merge.
  - Anchor 70 is pure dance. This is the near-saturated part (relaxation ceiling about 8,866 at move 68, and the visit ledger has only 2 unused visits plus 1 wasted hop), and the end is proven optimal for the older records.

  So weight anchor 40 (and add 30 or 35) in the decision, not 70.
- **MAJOR (leakage, from the round-1 review): partly fixed.** The holdout model excludes 7,911/8,002/8,123 and 15% of the other tours. But the curriculum's network-generated tours share the record climb, so climb positions are still near-duplicates of training data. That matters at anchor 40. Write the training file list into the model header. If time allows, also test a model trained without any tour ≥ 7,000.
- **MINOR: success criterion.** State it before the results come in. Suggestion: the prior is adopted for guided searches if its best-within-720 s ≥ stock's at ≥ 2 of 3 anchors in *both* rngs, including anchor 40. Otherwise the 2 cores go back to stock NRPA at anchors 30-45 (or to lns-style diversity).

## 3. Recommendation: CONTINUE with the A/B. Do not start "guided searches" until it is decided
The prior has 2 cores, the largest allocation, on the strength of L2 evidence only. The L3 A/B is the gate.

---
# Round 2 review: prior, pass 2 (rev_portfolio), 2026-11-01 ~02:55 UTC

## 1. Claims checked
| Claim | Verdict | Command |
|---|---|---|
| `NEW_RECORD_8255.txt` = 8,255, strict | **VERIFIED** | `check_tour.py` prints `moves=161 ok=True T=8255 unnecessary=[]`; `strict_check.py` prints `STRICT` |
| Origin | from the log | `tmp/ab/prior_a70.log`: L3, anchor 70 of 8,211, fresh weights, holdout prior β=2. Found at restart 11 of 13, and again at restart 12 |

## 2. Is the L3 A/B complete, and does it meet the evidence standard?
Mostly yes, with one gap. Equal time (720 s), one thread each, run concurrently, anchors 40/55/70 of 8,211, rng 11. Finished restarts per arm:

| anchor | arm | n | min | median | best |
|---|---|---|---|---|---|
| 40 | stock | 31 | 3,692 | 6,015 | 6,746 |
| 40 | prior | 11 | 7,249 | 7,717 | **8,095** |
| 55 | stock | 34 | 4,127 | 6,712 | 7,584 |
| 55 | prior | 14 | 7,631 | 7,834 | **7,918** |
| 70 | stock | 28 | 7,022 | 7,850 | 8,240 |
| 70 | prior | 13 | 8,097 | 8,178 | **8,255** |

- **At anchors 40 and 55, the prior's *worst* restart beats stock's *best*.** Its median is also above stock's best-of-run in all three anchors. Fresh restarts are independent samples, so with n = 11-34 per arm this is conclusive at the restart level. It also holds at equal wall time on the same machine.
- **Gap: one rng per anchor.** The standard asks for ≥ 5 seeds. With separation this large, a second rng at anchor 40 (12 min × 2 arms) is enough to close it formally. Do it, but don't wait for it before using the prior.
- **Measured speed:** prior ~15-18k rollouts/s, stock ~42-46k/s, i.e. 2.5-3× slower, better than the 4-5× in round 1.
- **The diversity concern is confirmed:**
  - prior finishes repeat (7,839 ×3, 8,178 ×3, 8,239 ×2, 8,255 ×2);
  - stock finishes rarely repeat;
  - so the prior concentrates on a few tours.

  For record hunting, keep β ≤ 2 and the holdout model. Also run 1 in 4 restarts at β=1 or stock, to keep variety.
- **Correction to my pass-1 note.** I called anchor 70 "near-saturated dance", yet both arms beat the 8,235 strict record from there (stock 8,240, prior 8,255). The 8,211/8,235 dance was *not* locally optimal. 8,255 uses 85 dance sits (8,235: 84), adding the pair d5/e4 with both members sat. Dance headroom is real at the +20-50 scale.
- **Leakage:** the holdout model never saw 8,211's lineage or any record ≥ 7,911. Its anchor-40 gain therefore can't be memorisation of 8,211's merge. That point is settled.

## 3. Issues
- **MAJOR (scope):** pnrpa is strict-only. Since free waits became legal, every prior tour leaves the free-sink gains (+28 to +53 each) on the table. Either pipe the prior pools through fractional's warm free-sink pass (`fnrpa --warm`, 240 s), or port the free-sink moves into pnrpa with prior features for them.
- **MINOR:** the pool is still `minutes\ntour` rather than TAB-separated. Write the training-file list into the model header.

## 4. Recommendation: CONTINUE, now as a proven method
Next steps:
1. Run guided L3 searches at anchors 30-45 of 8,292's strict skeleton (or of 8,255), 2 threads, with ≥ 2 rngs.
2. Pass every pool tour ≥ 8,200 through the free-sink pass.
3. One extra rng at anchor 40 to close the evidence gap.

Success means a new strict best, or any tour above 8,292 after the free-sink pass.
