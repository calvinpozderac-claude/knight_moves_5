# Review: nmcs (rev_code_a, 2026-10-31)

Reviewed `nmcs.cpp` as of ~21:06–21:13 (it changed during the review: `--fd/--fo` features were added; the
latest md5 is 54e96725…). There was no RUN.md yet. I rebuilt it in my scratchpad. Runs: 1 thread, nice 15,
seed = record_8123.

## 1. Status and claims checked
| Item | Verdict | How |
|---|---|---|
| Engine fidelity | **verified** | The seed self-check prints "8123 minutes, histogram ok". My own legality replay (pinned-move membership) of 8,123/8,002/7,911 agrees. The histogram stays in range: altitudes −158..17. Note: `read_tours` doesn't check legality, only T and wait divisibility. |
| Lazy NMCS L2, anchors 35–48, 45 s | **verified** | 36 restarts (~1.5 s each), best **6,875**, checker ok=True, maxvisits=3, unnecessary=[]. 50k playouts/s. |
| SP-MCTS, anchors 35–48, 35 s | ran | best **6,239**, mean 5,558 over 7 restarts |
| Playout policy, bench from move 35 | measured | 36k playouts/s. 35% finish, mean **512** minutes, max 1,385. The policy is very weak in the dance. |
| Equal-budget reference | measured | From move 44 of 8,123: fresh NRPA level 3 (`build/nrpa --no-warm --seed-adapt 0`, 52 s) gives 7,378. NRPA with its default seed-adapt gives 8,123. |
| CLI/time/pool/Ctrl-C | OK | `--threads`, `--time`, `--out`, `--pool` (≥ 7,500), SIGINT/SIGTERM handled |

## 2. Issues
**MAJOR: no evidence that NMCS/SP-MCTS beat NRPA here, and the literature expects the opposite.** At about
45 s on one thread, LNMCS L2 is ~500 minutes below one fresh NRPA L3 run. That comparison is unequal:
different anchors, and one NRPA sample, so it is an anecdote. The experiment that would settle it:
10 anchors (35..48) × 2 rng seeds, NRPA L3 vs LNMCS L3 vs NMCS L2 with `--base-n` tuned, same wall time
per run. Report the median and spread. If NMCS is not within ~100 minutes of NRPA's median, it does not
deserve full-run cores.

**MAJOR: the playout policy is the bottleneck.** A mean of 512 minutes from move 35 means level 1–2
estimates are mostly noise. NRPA learns its policy; NMCS doesn't. Options: (a) fit FW/FR/FV/FG/FD/FO
offline to the moves of 7,911/8,002/8,123 (log-likelihood; a tiny conditional logit, in seconds). (b)
Use the NRPA-adapted weights of a level-2 run as the NMCS playout policy (a hybrid). Measure the bench
mean after each change.

**MINOR**
- SP-MCTS expansion sampling (`acc += prior / 1.2` with u in [0,1)): when no finishing child exists, the
  priors sum to 1/1.2, so in about 1/6 of expansions the loop falls through to the **last** child. That
  biases the search. Sample u in [0, Σprior).
- `report()` runs the Python checker while holding the global mutex, which stalls every thread for each
  new best ≥ 8,000. Copy the text and check outside the lock.
- `--checker` defaults to the relative `tools/check_tour.py`, so the program must run from the repo root.
  Otherwise "(checker not found)" means no NEW_RECORD file. Fail loudly instead.
- `read_tours`: check each parsed move against `pinned_moves`, as beam/astar do.

## 3. Recommendation: PIVOT (small)
Keep the code. It is clean, correct and has a nice Lazy-NMCS. Spend no full-run CPU on it until the
head-to-head shows it competitive. The best pivot is a learned or fitted playout policy (2a), which also
benefits nrpa. Expected value of a 4-core NMCS run as configured: low (likely 7,0xx–7,6xx from anchors
35–48).

## 4. Next round
1. Fit the playout weights to the record tours, then bench from moves 35/44. Success: mean finished
   minutes from move 44 ≥ 3,000 (now ~500 from move 35).
2. Head-to-head as in issue 1 (≤ 60 s per job, so use level 2 vs NRPA level 3 truncated at 50 s, 10
   anchors). Success: median within 100 minutes of NRPA.
3. Fix the MCTS sampling bias and rerun the 35 s MCTS smoke on 3 seeds. Report the mean and spread, not
   just the max.
