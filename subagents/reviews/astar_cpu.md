# Review: astar_cpu (rev_code_a, 2026-10-31)

Reviewed snapshot: `astar_cpu.cpp` md5 659681e5… (it has changed since; the newer md5 is c71b3a99…) and
`train.py` as of 21:15. There was no RUN.md yet. I rebuilt the C++ in my scratchpad. I used your
`tmp/model_smoke.txt` read-only.

## 1. Status and claims checked
| Item | Verdict | How |
|---|---|---|
| Engine fidelity | **verified** | `astar_cpu check data/tours/record_8123.txt` prints "8123 minutes, 135 moves". `seq_minutes` checks pinned-move membership, and `text_minutes` also requires the waits to round-trip. Every reported tour is re-replayed before it is written. |
| Dedup/dominance | **sound** | Key = 64-bit hash of (alt, vis, pos). It keeps max T and drops a child when the stored T ≥ its T, and skips stale pops. Same state means the same future, so this is exact up to hash collisions: ~1e-4 chance at 6e7 states, and a collision only loses a node. |
| Eval speed | measured | `bench`: 16.5 µs/eval (features + MLP H=64). Search ≈ 5k expansions/s on one thread, ~45k children/s. |
| Smoke search, prefix 44, smoke model, 40 s | ran | best **4,966**. Not meaningful with a smoke model, but nothing ≥ 7,500 yet. Your `tmp/smoke_best.txt` = 4,091. |

## 2. Issues
**MAJOR: the labels and weights favour the known tours by construction.**
(a) `gen` writes every state of every seed tour labelled with that tour's score (src 5). Off-tour states
are labelled by fresh-weight NRPA levels 0–2 (99%), which finish far lower (playouts from move 35
average ~500 minutes).
(b) `train.py` then multiplies src-5 weight by `--elite-w 5` and high labels by `--high-w 3`.
The model can learn "looks like the 8,123 path → 8,123, anything else → much less", and the search will
retrace the record. That is how nnastar went from 8,002 to 8,123, so it is not useless. But it caps how
different the result can be, and it makes the pool mostly record variants. Fixes: set elite-w to 1 by
default. Label off-tour states with *relative* targets: the label minus the same-level NRPA baseline
from the cut state. Add held-out diagnostics: the value on the record path vs one-move deviations that
are known to finish within 100 minutes of it.

**MAJOR: the validation split leaks.** `train.py` splits by random sample, and consecutive states of one
labelled path are near-duplicates, so early stopping measures memorisation. Split by path/cut: add a
path id to `Sample` (the src/depth bytes leave no room, so use a hash of the cut root state) and hold out
whole paths.

**MAJOR (budget): the pipeline cost on 4 shared cores.** The nnastar recipe is 25 min gen + 30 min train
+ search. The README's 25 min gen was on a 32-thread box, so on one core it is ~10× less data. Plan the
full run as: gen 2 threads × 30 min, train 1 core ≤ 15 min (numpy), search 1–2 threads × 30 min with
`--refit` + 1 label thread. Put that plan with expected memory (`search` prints ~0.6 GB at 2e7 nodes)
in RUN.md.

**MINOR**
- Label workers read `bestScore` outside `resMx` (`min(poolMin, bestScore + 1)`). That is a data race;
  use an atomic.
- The dedup table is cleared entirely at 70% load, which loses all transpositions at once. Use
  generation-tagged eviction, or simply grow it.
- `rescore` sorts the whole heap (up to 2e7 entries) every refit. That costs a few seconds per adoption
  at large open lists. Use `nth_element` on the top K.
- The fallback heuristic (no model) is T + 60·optimistic sits. Say in RUN.md that it is only for testing.

## 3. Recommendation: CONTINUE (highest expected value of the five)
This is the method that produced both records. A CPU port with an online-refit MLP is the most credible
code-ready path to > 8,123, provided the value function generalises off the record path. The two MAJOR
learning issues decide whether it can find something new or only re-polish 8,123.

## 4. Next round
1. **Leave-one-out test** (the key experiment, ~1 h of one core in total). Gen and train **without**
   8,123 and 8,002 in `--tours`, then search from a1 and from prefix 34 of 7,911. Success: ≥ 8,002, i.e.
   it rediscovers the 4-hop merge. That would be evidence of real generalisation and would justify 2–3
   cores for the full run.
2. Ablation, same budget: elite-w 5 vs 1, random vs path-held-out validation. Report the search best and
   the number of distinct move-48 prefixes among pool tours ≥ 7,900, 2 rng seeds each.
3. Full run (only after 1): gen 2 threads × 30 min, then `search --threads 2 --label-threads 1 --refit 120`
   for 60 min from a1 and from prefixes 30–44.
