# Review: beam_dom (rev_code_a, 2026-10-31)

Reviewed snapshot: `beam.cpp` md5 e321f6ff… (it has changed since; the newer md5 is a55b659b…). There was no
RUN.md yet. I rebuilt it in my scratchpad. Runs: 1 thread, nice 15, seed = record_8123.

## 1. Status and claims checked
| Item | Verdict | How |
|---|---|---|
| Engine fidelity | **verified** | `replay_check` checks every move against `pinned_moves` and that h8 comes last. The seed prints "replay 8123". Every reported finish is re-checked by replay. |
| Dominance rule | **sound** | Same altitudes + position, T ≥, and visits elementwise ≤ dominates. The future depends only on (alt, vis, pos), and fewer used visits only relax the cap. The key is a 64-bit hash of alt+pos; a collision can only drop a node, never produce an invalid tour. Only accepted nodes go into the TT, so a dominator is always actually expanded. |
| "Optimistic" estimate | **refuted as an upper bound** | `--diag` along 8,123: T+est = 8,438 at move 46, 8,502 at 94, **8,084 at move 110** (< 8,123 final), and 669 at move 22 vs 1,369 at move 14 (the climb looks bad). It is a heuristic, not a bound. Don't call it optimistic, and don't prune on it. |
| Strength from the record's dance start (k = 44) | **weak** | width 500: **6,378** (9.8 s); width 2,000: **6,798** (29 s). Same state, same machine: fresh NRPA level 3 (`build/nrpa --seed-adapt 0 --no-warm`, 52 s): 7,378; seed-adapted NRPA: 8,123. |
| Speed / time | **problem** | width 20,000: ~8.5 s per layer (~25k children/s, ≈40 µs per child, mostly `estimate()`: up to 16 `ideal_dance` calls + BFS). `--time 45` ended at 48 s, with best 1,288 and no complete pass. |

## 2. Issues
**BLOCKER for a full run: nothing comes out unless a full pass finishes.** A pass from k = 44 is ~95
layers. At width 20k that is ~13 min per thread, and at 1M about 10 h. Time is checked only between
layers, so a big layer overshoots `--time` by minutes. Fix: make it anytime with iterative widening
(start at 500, ×4 per restart: your `--widen` exists, so make it the default). Check time inside expand
(the STOP flag is already polled). Optionally greedy-complete the top few beam nodes when time runs out.

**MAJOR: the estimate cannot guide even the dance.** From the record's own move-44 state, width 2,000
reaches 6,798, 1,300 below what that state is known to give. Widening gives roughly +200 per doubling, so
reaching 8,123 would need width ~10^5 or more (hours per pass). Fixes to try, cheapest first:
(a) Calibrate `ideal_dance` against the record. Print est vs the true remaining minutes at every move of
8,123/8,002/7,911 and fit the gap.
(b) Score children by T + est − λ·(visits used outside the two dance levels).
(c) Use NRPA-1 rollout value (a few rollouts) for the top 4×width candidates only.
The beam must reproduce ≥ 8,000 from k = 44 at a width that runs in a few minutes before any earlier
anchor makes sense.

**MAJOR: no use from a1 or early anchors.** T+est drops sharply during the climb (moves 14→22), so the
beam will prune the raise-e8 trick. State this in RUN.md. Restrict `--kmin` to ≥ 35 unless the estimate
gets a climb term that rewards it.

**MINOR**
- Memory: `--widen` × restarts can reach `--max-width 2e7`, about 10 GB of candidates (40 B × ~10 per
  node) plus states. Cap the width from available RAM, or print an estimate before starting.
- Pool: no dedup across layers/restarts. Add a hash set.
- `cntR/cntG` use 2^20 hashed buckets, so caps are shared by colliding signatures. Harmless; mention it.

## 3. Recommendation: CONTINUE only as a calibration project; no full run yet
Dominance pruning is correct and cheap, which is good. The method's weakness is the evaluation, not the
pruning. Until the beam can recover ≥ 8,000 from the record's dance start, a 4-core run would most likely
produce tours in the 6,500–7,500 range, below what the pool already holds. Expected value of a full run
now: ~0. After an estimate fix: moderate (a wide beam with dominance is a different exploration from
NRPA), mainly from anchors 35–48.

## 4. Next round
1. Calibration (≤ 60 s jobs): est vs true remaining along the three records, then widths 500/2k/8k from
   k = 44. Success: ≥ 7,900 at width 8k.
2. Speed: profile `estimate()`. Compute `ideal_dance` once per (U, sink) per parent, not per child, where
   possible. Target ≥ 100k children/s/thread.
3. Write RUN.md with an iterative-widening full-run command (`--widen 2 --restarts 0 --kmin 35 --kmax 48`),
   memory per width, and the time per pass you measured.
