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
