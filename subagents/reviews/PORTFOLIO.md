# Portfolio review (rev_portfolio), 2026-10-31

The question this review answers: where can the remaining minutes come from, and is the compute going there?
The numbers below come from cheap analysis of the finished tours. To reproduce them:
`nice -n 15 python3 subagents/reviews/portfolio_analysis.py data/tours/record_8123.txt ...` (about 1 s per tour).

## 0. Headline

1. **New best, verified:** `subagents/lns/tmp/run1_best.txt` = **8,130**. Checker output: `ok=True`,
   `maxvisits=3`, `unnecessary=[]`. Every wait is pinned (`subagents/fractional/classify.py` finds 0 free waits).
   It differs from 8,123 in moves 32–60 (the end of the climb and the merge). It is **+7**, a micro-gain. Its
   dance has the same structure as 8,123's (83 dance sits, same group-size profile). Orchestrator: copy it into
   `data/tours/` and the shared seed files.
2. **Checker-only 8,176** (`subagents/fractional/CHECKER_ONLY_8176.txt`). The checker accepts it, but it has one
   "free" wait: move 96 waits 23 min on a1, then makes a dz=0 knight move to b3, a jump that is legal with no
   wait at all. `tools/check_tour.py` accepts it only because its necessity test is global (perturbing the wait
   breaks the tour later). The rule reads "every wait must be necessary". The natural reading, and the one
   every record so far satisfies, is that the wait is needed for the jump that follows it. **Keep it on a
   separate board, and ask the human for a ruling.** Do not let it drive the portfolio.
3. **The 8,123 dance is already almost saturated** (section 2). Re-searching only the dance has small
   headroom. The biggest identified lever is visits that the climb and merge spend on points the dance later
   sits on: 7 sits, worth about 70–95 minutes each.
4. **The portfolio is lopsided.** About 11 of 15 agents search the same neighbourhood (8,123's tour after move
   about 30–45, or the whole tour warm-started from it). Nobody is dedicated to the climb. Nobody computes a
   sound bound, so nobody can tell "converged" from "under-powered". The machine is running at **load 14 on
   4 cores**, so every wall-clock budget is inflated about 3.5 times, and method comparisons made so far are
   not budget-matched.

## 1. Anatomy of 8,123 (independently computed)

| Phase | Moves | Minutes | Sits | Zero-wait moves | Notes |
|---|---|---|---|---|---|
| bottom climb | 1–12 | 0→37 | 3 (d1 8, d2 9, e1 20) | 9 | the sits at d1/d2/e1 raise their opposites e8/e7/d8 (the raise trick) |
| top climb | 13–35 | 37→148 | 9 | 14 | walk up to d8, sink it with h8; tiny groups, so 148 minutes in total |
| merge | 36–44 | 148→290 | 3 (h5 63, h6 19, h5 60) | 6 | h5/h6 are sat while their partners a4/a3 are still *below* the groups |
| dance | 45–135 | 290→8,123 | 81 | 10 | 4·n per sit, n from 34 down to 17; final move 51 (3 units at n=17, not 4) |

- **Group-size profile** (M = upper + lower group, every 8 dance sits): 49, 61, 58, 56, 53, 50, 48, 44, 41, 37, 34.
  Points from below join during the first ~8 sits. After that, M falls by one at every live-partner break:
  30 breaks in 83 dance sits, 2.77 sits per break. A pair supplies at most 3 sits, so the ideal is 3.0.
- **Visit ledger** for the dance's 31 sitting points: 31 × 3 = 93 possible sits. The dance makes 83. The other
  10 go like this:
  - **7** are visits spent before the dance: h6, b4, c4, c2, e2, d2, d1. Of these, d1 and d2 are the raise-trick
    sits; the rest are routing arrivals or hops.
  - **1** is a zero-wait hop on a sitter (f6, move 129).
  - **2** are left unused (b2, d1).
  - The other 12 hops in the dance land on non-sitters, so they are free.
  - For comparison, 7,911 lost 11 sits before the dance and 5 to hops. That is where 7,911 → 8,123 came from.
- **Exchange rates** (use these to judge any proposed change):
  - one extra sit appended late is worth about 2·M_end ≈ 68 min;
  - one live break avoided with R sits left is worth about 2R (≈ 80 at mid-dance);
  - turning the 3-unit final move into a full sit is worth ≈ +17.
- **Same picture in 8,002, 8,123, 8,130 and 8,176:** identical M profiles within ±2. 8,002 → 8,123 (+121) is
  pure dance routing on the same 43-move prefix. 8,123 → 8,130 (+7) changes the merge.

## 2. Headroom estimates (honest, with what each number does and does not bound)

The relaxation (`relax_bound` in the script) models a two-group dance with adjacency ignored:
- every sit earns 4·n_up;
- consecutive sits alternate between the groups, so n_t + n_{t+1} ≤ M(t);
- all lower points join at once;
- each pair has one sitting member, with cap 3 − visits;
- break order is optimal (open pairs first, then the pairs with the largest caps).

It is a valid bound only once no pair has a member below the two groups. Before that, a pair can supply both
members (a4/h5 and a3/h6 do), and the bound breaks.

| Lever | Ceiling | Realistic next-rounds gain | Confidence | Evidence |
|---|---|---|---|---|
| **(a) better dance, same first ~45 moves** | relaxation ≤ **8,862** from 8,123's state after move 46 (8,869 for the 8,130 tour) | **+30…+200**: recover the 1 wasted hop, the 2 unused visits, 1–2 breaks, a full-sit finish | high that it is small | ledger above shows 80 of the 83 dance sits are visit-optimal. The relaxation's +740 is mostly adjacency it ignores (5 sits ≈ 475) |
| **(b) better merge (moves 36–60)** | not boundable by this relaxation (14–24 pairs have a member below at move 35–39) | **+0…+400** | low (wide) | 7,911 → 8,002 (+91) and 8,123 → 8,130 (+7) came from here. Each extra "partner-below" pair (both members used) adds up to 3 sits with no break, ≈ +150–250 |
| **(c) different climb** | 7 sits lost before the dance ≈ +500–650 if all were avoidable. About 2–3 of them (d1, d2 sat to raise e8/e7) are inherent to the raise trick | **+0…+400**, mostly by re-routing the *bottom* climb (moves 1–12), not by a new climb | low, but the largest well-defined target | c2, e2, c4, b4 and h6 are routing arrivals and hops on future sitters. A brand-new climb from a1 has never been found (README); P(success in a few rounds) is under 10% |
| **(d) rule freedoms** | *fractional waits: exactly 0 under the pinned reading* (proof below). Free sinks: unknown, possibly large | +53 shown (8,176), but checker-only | the rules question dominates | 8,176 uses one free wait at a1. Because a1's opposite is h8, that wait lifts h8 one unit and re-phases the dance |

**Why fractional waits cannot help under the pinned reading.** Every altitude starts as an integer. A positive
wait at p must be needed for the next jump p→q. The target q is never p's own opposite: the jump would need
both coordinate differences 7 − 2x and 7 − 2y to be odd, and a legal jump needs one of them to be 0. So q is
either in p's group (dz = 0 for any wait: the wait is free) or static. If q is static, the jump pins the sink
to an integer, so t/n is an integer, and the opposite rises by the same integer. Altitudes therefore stay
integers forever, and "fractional" adds nothing unless free sinks are allowed. A fractional-waits track is a
free-sinks track with extra steps.

**Bottom line.** Under the strict rules, a realistic target for the next few rounds is about **8,150–8,350**.
It comes from (b) the merge and (c) bottom-climb routing that frees sitter visits, then (a) a dance
re-search that turns the freed visits into sits. Fine-tuning the dance alone is close to exhausted
(about +30…200).

## 3. Portfolio map

| Agent | What it attacks | Overlap | Portfolio verdict |
|---|---|---|---|
| nrpa (gnrpa, biases) | (a)/(b), anchors 34–44, fresh | the proven baseline | **keep as the reference baseline** every method is measured against |
| lns | whole tour, splice / SA | genetic, polish | **keep**: the only producer of a strict improvement this round (8,130). It is one seed, so it is not yet proof that LNS beats NRPA |
| opening (reroute / insert / splice_nrpa) | (b)/(c): visit-pair loss of the prefix | polish, tools/reroute_zero_wait.py | **raise priority**: it targets the 7-sit ledger directly. Point it at moves 1–12 and 36–60 |
| goal | (b)/(c): designed dance starts | opening | **keep, refocus**: make the objective the visit ledger (sitter visits used, partner-below pairs), not the idealised dance estimate. That estimate rated an opening at 8,725 which only reached 7,621 |
| goexplore | diversity across stages | goal | **keep 1 slot**: the only archive that can hold alternative climbs; descriptor must include sitter visits used |
| beam_dom | (a)/(b) from k=35 | nrpa | **bake-off only**, until it beats the baseline at equal budget |
| astar_cpu | nnastar without GPU | — | **bake-off, priority 1 among new methods**: its family found 8,002 and 8,123. But a small MLP is not the 5M-parameter ResNet; test that it reproduces ≥ 8,000 first |
| macro | (a): sit+hop macros | polish | **bake-off**: cuts branching in exactly the regime that is near-saturated |
| nmcs | (a): NMCS / LNMCS / SP-MCTS | nrpa | **redundant with nrpa**: smoke plus one budget-matched comparison, then stop unless it wins |
| genetic | whole tour, splice crossover | lns | **redundant with lns**: one bake-off; merge the best operators into lns if it loses |
| prior | GNRPA move prior | nrpa | **keep cheap**: an accelerator. Evaluate as nrpa+prior vs nrpa at equal rollouts |
| polish | post-processing (re-route, insert, window) | opening/insert, lns, tools/reroute_zero_wait.py | **merge into one post-processing pipeline** run on every pool tour ≥ 8,050 |
| exact (bb) | tail B&B, k=104 | — | **keep, re-purpose**: certify the optimality frontier (prove suffixes from k = 120, 110, 100… optimal). That turns "headroom (a)" from an estimate into a fact. Its bound must be shown sound |
| fractional | (d) | — | **keep at ≤ 5% CPU** until the rules ruling. Its pinned-only runs (`--free 0`) are just another NRPA, so drop them |
| runner | infrastructure | — | **critical**: enforce ≤ 4 heavy jobs and CPU-time accounting, add the strict pinned check, keep two boards |

**Redundancy.**
- Stochastic suffix search near 8,123: nrpa, nmcs, splice_nrpa, fnrpa, prior, beam_dom, astar_cpu, macro, and
  goexplore when seeded.
- Whole-tour edit: lns, genetic, polish.
- Post-processing: polish, opening/insert, opening/reroute, tools/reroute_zero_wait.py.

**Missing.**
1. A **strict-rules validator** in the pipeline: `check_tour.py` plus "0 free waits" (classify.py logic).
   Without it, a checker-permissive tour can quietly become "the record".
2. A **shared, documented bound and estimator**. Every agent has its own optimistic "dance estimate", and none
   is calibrated. Needed: (i) the relaxation above (sound once the groups are formed) for pruning and
   triage; (ii) a calibration of estimate vs achieved, by regression over the pool at equal L3 budget.
3. A **bottom-climb re-route study.** Find prefixes from a1 that reach a state from which 8,123's moves
   13–35 replay legally (same sits, or same altitude changes) while visiting fewer of the 31 dance sitters.
   It is small, nearly exact, and aims at up to 5 sits.
4. An explicit **partner-below exploitation**: count pairs whose both members sit (in the merge, the
   partner is still below and rising) as a design objective in goal/opening.
5. An **experiment harness**: runner runs N seeds × fixed CPU-seconds and reports the distribution
   (section 5).

## 4. CPU allocation (4 cores; per 60-min round = 4 core-hours)

Enforce **at most 4 heavy single-thread jobs at once** through runner. Today about 11 jobs share 4 cores.
That gives no speed-up, only noise, and it makes every wall-clock budget meaningless.

| Share | Cores | Track | Contents (rotate inside the track by evidence) |
|---|---|---|---|
| 35% | 1.4 | **(b)+(c) prefix redesign** (biggest headroom) | opening reroute/splice on moves 1–12 and 36–60 → finish with fresh NRPA L3 from the new prefix. goal with a ledger objective. Every candidate prefix is finished by the *same* L3 protocol, 5 seeds |
| 30% | 1.2 | **(a)+(b) local search near the best** | lns 0.5, fresh NRPA L3/L4 at anchors 34–60 on 8,123 *and* 8,130 0.5, prior-NRPA 0.2. Not more: this is near-saturated |
| 15% | 0.6 | **new-method bake-off** | astar_cpu first, then macro, beam_dom, nmcs, genetic. Each gets a fixed budget on the standard benchmarks (section 5); promote only on a win |
| 10% | 0.4 | **post-process + certify** | polish pipeline on every new pool tour ≥ 8,050; exact frontier proofs |
| 5% | 0.2 | **structural diversity** | goexplore archive with alternative climbs (low P, high payoff) |
| 5% | 0.2 | **(d) rules track** | fractional free-sinks, separate board, until the human rules |

**Guard against record-chasing.**
- The next best tour (LNS's 8,130 today) goes into the shared seed files. It does **not** change track
  shares. Shares change only through the bake-off evidence below.
- A warm start from the best tour is allowed for at most half of a track's time; the rest stays fresh or
  structural.
- A track is cut only after 2 budget-matched rounds below the baseline, *and* after its self-checks pass
  (section 5). That separates "the idea is bad" from "buggy or under-powered".
- Re-balance after every round; no track goes below 5% while its sound ceiling is above the best tour.

## 5. Evidence standard for "method A is better than B"

1. **Fixed benchmark tasks** (the same for everyone):
   - B1: fresh search after move 46 of 8,123 (pure dance);
   - B2: after move 35 (merge + dance);
   - B3: after move 12 (bottom-climb redesign);
   - B4: from a1.
   Report which tasks a method was run on.
2. **Budget in CPU-seconds, not wall-clock.** Measure user time (`/usr/bin/time -f %U`, or getrusage in the
   program), or count rollouts or node expansions, at the same nice level. Standard budget: 600 CPU-s per seed
   for B1/B2, 1,800 CPU-s for B3/B4.
3. **At least 5 independent seeds**, and for anchored methods at least 3 anchors. Report n, median, IQR and
   max of each seed's best, plus the number of distinct finished tours ≥ 8,000 and the median time to reach
   8,000.
4. **"Better"** means:
   - same task, same budget;
   - A's median > B's median, and A beats B in ≥ 4/5 seed-paired runs (or a one-sided Mann–Whitney test at
     p < 0.05 with n ≥ 8);
   - and the gap is larger than the baseline's IQR.
   One run is an anecdote, and so is a single new record.
5. **Warm-started runs** that return the seed tour are null results. Report the share of runs that improve,
   and by how much.
6. **Self-checks** before any negative conclusion:
   - replay 8,123 to exactly 8,123;
   - re-find 8,123's own suffix from move 120 (and from 110) within budget.
   A method that fails these is buggy, not refuted.
7. **A record counts** when it passes `check_tour.py`, has 0 free waits, and is reproducible from the logged
   command and seed. Checker-only tours go on the separate board.
