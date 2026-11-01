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

---
# Round 2 update (rev_portfolio), 2026-11-01 02:40 UTC: pass 1 of 3 only (later passes not run)

## Verified boards
- **Strict:** 8,211 (`data/tours/record_8211.txt`). Checker ok, strict_check STRICT, and it reproduces deterministically (lns seed 21, `--iters 55000`).
- **Free-wait (checker-valid, one free sink each):**
  - 8,239 (fractional `tmp/d_free_r1.txt`, free wait at move 153);
  - 8,183 (8,130 lineage);
  - 8,176.

## Correction to §0/§2(d) above
The 4×4 worked example in `tools/sim.py` contains three free waits that are legal at 0: moves 5, 6 and 7, one of them a 5/2-unit sink (`strict_check4.py --example`). So the pinned reading is *not* clearly "the natural reading". If that example is the official one, free waits are what the puzzle intends. The open question is the example's provenance: it must be quoted with its source in fractional/RULES.md. Until then: two boards, and the strict one is the conservative record.

## What changed in 8,211 vs 8,123 (`portfolio_analysis.py`)
- **Same lineage.**
  - Moves 1-14 are identical, and so is the dance ladder order.
  - Moves 15-61 (climb top and merge) are rewritten: 15 pre-dance sits instead of 13 (new: f7 ×3, b8 ×2, e8 ×2).
  - The dance is entered with a 4-unit sit at h6 (76 against 19), and its first ladder groups are larger (35/27 against 34/28). Together that is about +116 by dance sit 6.
- **One more dance sitter.**
  - Pair d8/e1 was unused in 8,123; now e1 sits in a `e1 d1 e1 d1` block in the mid-dance, found by op 2 at i=113. So not all gains are in moves 15-60.
  - That gives 32 sitters and 84 dance sits (against 31/83), with about −90 in the middle and +48 for one more final sit.
- **Ledger.** Possible sits 96; the dance makes 84. Lost: 9 to pre-dance visits on sitters (against 7), 1 to a hop on a sitter, 2 unused. Every opposite pair now supplies a sitter.

## Revised headroom (strict)
- **(a) Dance.** The relaxation ceiling is unchanged, about 8,866 from 8,211's state after move 68. Realistic gain: +20-150 (1 hop, 2 unused visits, break parity).
- **(b)+(c) Climb top and merge.** Still the main lever: the 8,211 step was +88 here. Each re-routed pre-dance visit on a sitter is worth about 70-95. 9 such visits are now spent, and maybe 4-6 of them are avoidable routing. Realistic gain: +0-300.
- **Strict target for round 3:** 8,250-8,400.
- **Free waits.** Each well-placed free sink has added +28 to +53 so far. Whether several stack is untested.

## Portfolio risk
All three strict-board results this round (8,130, 8,211 and the free-wait stacks) are the **8,123 lineage**: the same bottom climb and the same dance ladder. The decisive ingredient of 8,211 was **seed diversity plus crossover** (re-routed variants in `lns/tmp/div_8123.txt` and op 7), not iterations: three seeds from 8,130 plateaued.
- No agent now explores a different bottom climb or a different ladder order.
- The prior A/B, the only non-LNS strict search, had finished 1 prior and 3 stock restarts at anchor 40 at 02:40 (best 7,689 vs 6,618). It is too early to judge, and the A/B has one rng per anchor.

## Recommended CPU split for round 3 (4 cores)
| Share | Cores | Track |
|---|---|---|
| 30% | 1.2 | **lns**: diversity-fed. Build a new re-routed variant set from 8,211 and other families; 3 seeds; the 2×2 seed-set ablation |
| 25% | 1.0 | **prior (or stock NRPA)**: anchors 30-45 of 8,211, fresh weights, ≥ 2 rngs. Only if the L3 A/B shows best-within-720 s ≥ stock at anchor 40 in both rngs; otherwise stock NRPA on the same anchors |
| 25% | 1.0 | **New: lineage diversity.** Re-route the bottom climb (moves 1-14, 7-9 sitter visits) and alternative ladder orders, e.g. reviving opening/reroute or goal for one round. Feed the outputs to lns op 7 |
| 15% | 0.6 | **fractional**: stack free sinks on 8,211 (5 seeds, equal-time strict control) + RULES.md |
| 5% | 0.2 | polish/exact certification of each new best |

---
# Round 2 update 2 (rev_portfolio), 2026-11-01: rules settled, boards merged

**Rule (from the user):** a wait (t, P) with t > 0 is legal iff some t' exists such that replacing it with (t', P), all other waits fixed, makes that move or a later move illegal. Any tour that `tools/check_tour.py` accepts counts.

**Verified:**
- `data/tours/record_8239.txt`: `ok=True T=8239 maxvisits=3 unnecessary=[]`, with one free wait (move 153, 72 min at f6 before the dz=0 hop to g8). **This is the overall record.**
- `data/tours/record_8235.txt`: `ok=True T=8235 unnecessary=[]`, STRICT. **This is the strict record.**
- Both have the same structure as 8,211: the same lineage, 84 dance sits and 32 sitters; the M profile matches 8,211 up to 2 counts at one sample (48 vs 46); 2 unused visits (b2, c2).

## Is `necessity_check` sound under this rule? Yes
- **How it works.** It marks a wait necessary only if one of its probes t' ∈ {t ± 1/997, 0, t+1, 2t} (t' ≥ 0, t' ≠ t) makes `simulate` fail. That probe is then an explicit witness.
- **Why every failure is an illegal move.** With only one wait changed, the squares are unchanged, so visit counts, early arrival at h8 and the final square are all unchanged. The only remaining failure is "illegal jump" at that move or a later one, which is exactly the rule's condition. Negative and equal probes are skipped.
- **Remaining gap is conservative.** A wait whose only witnesses lie outside the probe set is reported unnecessary, i.e. rejected. That is fine.
- **Example witnesses** for 8,239's free wait at move 153:
  - t ± 1/997 and t+1 break move 154 (g8→f8, fractional dz);
  - 2t breaks move 154 (g8→f8, dz=6);
  - 0 breaks move 155 (f8→g8, dz=6).
- **Caveat.** `check_tour.py` parses only `digits` and `digits/digits`, so tours must be written in that format. A decimal such as 8176.5 would be silently dropped by the regex. Writers must emit exact fractions.
- **Retire `strict_check.py` as a gate.** Keep it only as a classifier, to report how many free waits a tour uses.

## Headroom with free waits allowed
- **What a free wait does.** It shifts one group's altitude by any rational amount, at the cost of one visit (it must be followed by a dz=0 hop, which in the dance happens anyway). If it sits at a1, h8 rises too.
- **Observed gains:** +53 (8,123→8,176), +53 (8,130→8,183), +28 (8,211→8,239). Each so far is a **single** free sink re-phasing the endgame or the a1/h8 pair.
- **Ceiling.** The relaxation's ceiling of about 8,866 still bounds two-group dances, because free sinks don't add sits or points; the knight only gains by sitting during hops that already exist. The dance has about 14 zero-wait hops; each could become a free sink worth group-size × units.
  - The 8,239 free sink takes 72 min (4 units at n=18) on a hop in the endgame.
  - Free sinks in the merge/climb, where group sizes are small but parity and alignment matter, are untested.
- **Estimate.** Several free sinks stacked could be worth about +50 to +250 on top of the strict lineage (most hops sit next to small groups late, or would break alignment). The strict levers are unchanged: climb top and merge +0–300, dance +20–150.
- **Revised round-3 target: 8,300–8,500.** The upper end needs both stacked free sinks and a better merge.
- **The rule may admit more than free sinks.** Waits that are legal at 0 but are needed by a later move include ones that change group alignment so that later dances merge differently. Under the strict model these were invisible. The engine (`src/eng.h`) cannot generate them; only `fnrpa` can.

## How the CPU split should change (round 3, 4 cores)
| Share | Cores | Track | Change |
|---|---|---|---|
| 35% | 1.4 | **free-wait search** (fractional's fnrpa, with the free-sink move set): stack sinks on 8,235/8,239 and the lns elites; anchors over the whole tour, including the merge; ≥ 3 seeds | was 15% + rules work |
| 25% | 1.0 | **lns**, fed with varied starting sets, from 8,235 and 8,239 | was 30%. Its biggest missing piece is free-sink operators: add "insert a free sink of s units before an existing zero-wait hop" as an op, with waits re-derived (fractional's `repin.py` logic) |
| 20% | 0.8 | **lineage diversity**: re-route the bottom climb, alternative ladder orders. Free sinks loosen alignment constraints, so climbs that failed under pinned moves deserve another try with fnrpa | was 25% |
| 15% | 0.6 | **prior / stock NRPA** at anchors 30–45, ≥ 2 seeds | was 25%; it is still gated on the L3 A/B, and its engine is strict-only, so its value per core dropped once free sinks became legal |
| 5% | 0.2 | polish / exact certification | unchanged |

**Guards:**
- Don't send everyone into free sinks. The +28 to +53 gains are single-sink re-phasings with one seed each.
- Require ≥ 3 seeds, equal time, and a free vs strict control (campaign2 design) before claiming that stacking pays.

---
# Round 2, pass 2 (rev_portfolio), 2026-11-01 ~02:55 UTC

## Boards (all re-verified with `check_tour.py`; free waits counted with `strict_check.py`)
- **Overall record: 8,292** (`data/tours/record_8292.txt`, fractional), with 2 free waits (moves 115 and 129).
- 8,288 was found twice: by fractional (1 free wait, move 115) and by lns (1 free wait, move 111).
- 8,264 (fractional).
- **Strict best: 8,255** (prior, L3 anchor 70), ahead of 8,235.

## Convergence: one lineage, one basin
- **Every tour above 8,200 is the 8,123 lineage:** the same first 13-15 moves and the same dance-ladder order (h3/h2 → h1/g1 → g2/g3 → f2/f1 → f3/e3 → c5/b5 → a5/a4 → a3/a2 → b3/b2 → …).
  - The strict tours share 81-114 moves with 8,211.
  - lns's and fractional's 8,288s have different merges, but their **last 94 squares are identical**, including the same 24-minute free wait at a1→b3. They found it independently.
- **What this tells us:**
  - (i) The tail improvement is easy and robust.
  - (ii) The merge difference between them is worth exactly 0.
  - (iii) All three agents are exploiting the same basin. Nobody has run a different bottom climb or ladder order since round 1.

## Robustness of the gains
| Gain | Evidence | Robust? |
|---|---|---|
| prior vs stock at L3 | 3 anchors, 11-34 restarts per arm; at anchors 40/55 the prior's worst restart beats stock's best | **yes**, conclusive at the restart level (1 rng; add a second) |
| free vs strict (fractional, campaign2) | 2/2 paired seeds, +29 and +4 | likely; n is short of 5 |
| free vs strict (lns run5) | 1 of 4 seeds improves (8,288), 0/3 strict | weak so far |
| 8,288 tail | found independently by two agents; 59/472 finishes in one fractional run | **yes** |
| 8,292 | 1 of 5 seeds, 1 of 655 finishes | single event; legal, but no repeatable method yet |
| dance re-search (anchor 70 of 8,211) | both arms beat the 8,235 strict record (8,240, 8,255) | **yes**. It corrects my earlier "dance near-saturated" claim |

## Revised headroom (free waits legal)
- **Free sinks.** 8,292 has 10 zero-wait hops left in the dance and about 6 in the merge. Each hop is a candidate free sink. Gains so far are 24-84 min per sink before re-pinning losses, and +4 to +53 net per sink. An exhaustive insertion sweep (every hop × every sink k/6, later waits re-pinned) is cheap and has not been run. Estimate: **+20 to +150**.
- **Dance re-search** (shown above, at anchor 70): +20-60.
- **Merge/climb:** +0-300, unchanged and still the largest lever. It is untouched since 8,211, because every agent now seeds from the same merge.
- **Ceiling.** The relaxation still bounds the two-group dance at about 8,866. Free sinks don't add sitters or points.
- **Round-3 target: 8,350-8,500.**
- **Pumps (giant one-group free-sink loops, 11k+):** genuinely illegal under the user's rule, since no witness t' exists (see fractional.md). There is no hidden headroom there.

## Round-3 CPU split (4 cores)
| Cores | Agent / track | Why |
|---|---|---|
| 1.25 | **fractional**: exhaustive free-sink insertion sweep on 8,292 and on prior's strict pool, then warm L2, ≥ 5 seeds | biggest *cheap* expected gain now that free waits are legal; deterministic |
| 1.25 | **prior**: guided L3 at anchors 30-45 (merge) *and* 60-75 (dance) of 8,255/8,292's skeleton, ≥ 2 rngs; every pool tour ≥ 8,200 goes through the free-sink pass | the only method with conclusive equal-time evidence; it gave the strict record |
| 1.0 | **lns**: the 2×2 starting-set test with free sinks on; seed sets **not** derived from 8,211, i.e. re-routed 8,292 prefixes plus the 7,200/7,066 families | its 8,211 jump came from diversity. This is also the anti-convergence slot: at least half of it must start from tours outside the current lineage |
| 0.5 | **reserved: lineage diversity**. Prior-guided search from small anchors (12-35), i.e. new bottom climbs, plus lns op 7 crossover with those | guards against all four cores polishing one basin. Cut it after round 3 if it produces nothing ≥ 8,100 |

**Evidence rules for round 3:**
- **Seeds.** Report n seeds and per-seed bests for every claim.
- **"Method X works".** Requires ≥ 3 seeds that improve at equal CPU, not one hit.
- **New records.** A new record (a single hit) does not move cores.
- **Seeding.** No agent may seed *only* from the current record. Every run file must list at least one input tour from outside the record's prefix family.
