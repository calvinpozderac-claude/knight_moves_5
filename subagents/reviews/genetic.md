# Review: genetic (rev_code_a, 2026-10-31)

Reviewed snapshot: `genetic.cpp` md5 f7e7f293… (it has changed since; the newer md5 is 62b52379…).
There was no RUN.md yet. I rebuilt it in my scratchpad and ran 1 island for 40 s from the repo root,
with outputs in my scratchpad.

## 1. Status and claims checked
| Item | Verdict | How |
|---|---|---|
| Engine fidelity | **verified** | `parse_file` checks every move against `pinned_moves` and the gene round-trip. Seeds replay to their minutes. My independent legality replay of 8,123 / 8,002 / 7,911 agrees. The gene semantics (q, dz) match `pinned_moves`. The in-group and opposite-point cases are right: the opposite point 63−p is never a jump neighbour of p, since its offsets are odd, so that case never arises. |
| Smoke run | **verified** | best 8,123 (= the seed). The pool holds 160 rows ≥ 7,950. Top new tours: **another 8,123** and an 8,115, both pass `check_tour.py` (ok=True, maxvisits=3, unnecessary=[]). |
| Diversity of the results | **local** | The second 8,123 shares 32 moves at the start and 89 at the end with the record. The difference is (6,e7)→(6,d8) at move 33 and re-timed waits in the merge, the same structure. The 8,115 differs only in the first two moves (e1/d1 vs b1/d1), so it has the same climb. The population converged: median 8,043, 40 distinct scores after 30 s. |
| CLI / time / pool | mostly OK | `--threads` (= islands) and `--time` work, with a clean stop. The pool rule deviates from CODE_BRIEF (see MINOR). |

## 2. Issues
**MAJOR: crossover mostly re-creates a parent.** Cuts need the same square, weighted by
exp(−state distance/2). All good seeds share the first ~34 moves, so the weight sits on cut pairs
(i = j) inside the shared prefix (distance 0), and the child is exactly B. The op stats show it: xover1
made 11,332 children and only 145 were accepted. Fix: drop cut pairs where A[0..i) == B[0..j) and the
tails are equal near the cut. Reject children whose hash equals a parent's before calling `build()`.
Report "children identical to a parent" as an op stat.

**MAJOR: there is no mechanism to leave the record's basin.** Every island is seeded with GBEST plus the
top three archive tours, which includes 8,123. Steady-state replacement with crowding then converges on
the 8,123 family within seconds. To find something different, run at least one island seeded only
with non-record families: curriculum, 7,066/7,200 descendants, other agents' pools with different merges.
Keep migration from the record islands into it off. Use "distinct prefix up to move 48" as the island's
diversity measure instead of "distinct minutes".

**MINOR**
- The pool contains the seeds. The record itself is row 1, because `insert()` calls `report()` for seeded
  members. Exclude seeds from the pool (hash set of ARCH), as goexplore does.
- The pool rule `T ≥ GBEST − 150` (with GBEST = 8,123 from the seeds) means nothing below 7,973 is
  pooled. CODE_BRIEF asks for ≥ 7,500. Either follow it or say in RUN.md why (flood control), and
  make `--pool-slack` default large.
- "NEW BEST 8123" at t=0 is the seed. Print "seed best" and "new best" separately, like goexplore.
- The checker runs only at exit, on `--out`. Fine. Default seeds are found by relative path
  `data/tours`, so the program must run from the repo root (document it).

## 3. Recommendation: CONTINUE (cheap), refocused
As built, it is a competent local optimiser of the 8,123 family: it produced ties and −8 variants in 40 s.
That overlaps with polish/lns. The unique value a GA can add is recombining different families, which
needs the crossover fix and an island without the record. Expected value: low-moderate. CPU: 1 core at
most until the calibration shows it can climb.

## 4. Next round
1. **Leave-one-out** (1 island, 20 min). Seeds = everything except 8,123 and 8,002. Success: reaches
   ≥ 8,002. This shows it can rediscover the shorter-merge improvement by recombination.
2. Crossover A/B (2 × 10 min each, 2 rng seeds): current crossover vs "identical-prefix cuts excluded".
   Metric: accepted-and-better children per 1,000 children, and best at 10 min.
3. Write RUN.md: build line, smoke and full commands (suggest `--threads 1 --time 3600`), and memory.
