# How long can a tour be?

**Terminology.** Following the puzzle's own rule, a **free wait** is a wait t > 0 for which *no*
other value t′ makes that move or a later move illegal. Free waits are forbidden, and every other wait
is allowed. (The older notes in `subagents/` use "free wait" for something else: a wait that leaves its
*own* jump legal for every t. Under the puzzle's rule, such a wait is allowed whenever some t′ breaks a
later move.) A **strict** tour is a narrower class: every wait is pinned by its own jump, meaning any
small change of t breaks the very next jump. Every record tour in this repo is strict.

| Tours considered | Result | Status |
|---|---|---|
| **All tours that obey the rules** (no free waits) | **There is no longest tour.** For every W > 286 there is a legal tour of 288 + W minutes, none of whose waits is free. Every length above 574 minutes is achieved exactly. Example: a 100,288-minute tour | Proved (Theorem 1). Machine-checked in Lean: `KM5.no_longest_tour`, `KM5.unbounded`, `KM5.every_length_above_574`, `KM5.longTour_100288` |
| **Strict tours** (each wait pinned by its own jump) | **T ≤ 24,329 minutes.** The best known is 9,329 | Proved (Theorem 2). Machine-checked in Lean: `KM5.strict_bound`. The record is checked as `KM5.record9329_valid` |

The exact optimum over strict tours is not known; Theorem 2 puts it between 9,329 and 24,329.
Attempts to narrow that gap are described in [Attempts to narrow the strict gap](#attempts-to-narrow-the-strict-gap).

**Lean formalization** (`lean/`, Lean 4 + Mathlib). It covers:
- the rules themselves (`KnightMoves5/Rules.lean`);
- both theorems;
- the record's validity;
- the lemma that a strict wait is never free (`KM5.strict_allNecessary`).

Every result depends only on Lean's standard axioms (`propext`, `Classical.choice`, `Quot.sound`):
there is no `sorry`, and no `native_decide`. Concrete tours are evaluated by the kernel itself, in
exact rational arithmetic.

The formal rules are checked against the puzzle in three ways:
- the puzzle's own 4 × 4 worked example (28 minutes) is valid under them, three waits pinned only by
  later moves included (`KM5.Example4.valid`);
- the 8 × 8 rules are exactly the general n × n rules at n = 8 (`KM5.toGen_valid`);
- on 200 real and mutated tours, Lean agrees with `tools/sim.py` (`lean/test/DiffTest.lean`).

Any tour file can be certified with `lean/tools/certify.py`. See `lean/README.md`.

Python cross-checks (exact rational arithmetic, a few minutes):
```
python3 proofs/verify_unbounded.py            # Theorem 1: the tour family, its legality and necessity
python3 proofs/bound_terms.py data/tours/*.txt # Theorem 2: every lemma, checked on real tours
python3 proofs/find_unbounded.py data/tours/record_9329.txt data/tours/record_8830.txt
```

## Rules as used

- There are 64 points, and every jump changes (x, y, altitude) by a permutation of (0, ±1, ±2).
  Every jump therefore changes the knight's altitude by at most 2.
- A move (t, Q) means: wait t ≥ 0 on the current point P, then jump to Q. A wait with t > 0 is a
  **sit**.
- At a sit on P at altitude k:
  - let O be P's opposite point;
  - the group G is every point at altitude k except O, with n = |G|;
  - each point of G sinks d = t/n;
  - O rises d, unless it is also at altitude k, in which case it stays put.
- Points other than h8 take at most 3 visits, and the start on a1 counts as one. The tour ends on its
  first arrival at h8.
- The initial altitudes are all ≥ 0, with a1 = 0 and h8 = 17. They sum to 421.

`tools/sim.py` implements exactly these rules, and every check here uses it.

---

## Theorem 1: tours that obey the rules can be arbitrarily long

**Tour(W).** Take the first 53 moves of the 9,329 record. They take 238 minutes and end with the
knight on d5 at altitude 6. Then play:

| Move | Play | Effect |
|---|---|---|
| 54 | (W, b4) | The 26 points at altitude 6 (a8 b3 b4 b5 b6 b7 c5 c7 d5 d7 d8 e8 f3 f4 f7 f8 g2 g5 g6 g7 g8 h3 h4 h5 h7 h8) sink W/26. d5's opposite e4 (altitude 3) rises W/26. d5→b4 is a knight move inside the group, so the jump is legal for every W |
| 55 | (50, d5) | b4's opposite g5 is in the group at the same altitude, so it stays put. The other 25 sink 50/25 = 2 |
| 56, 57 | (0, f4), (0, g6) | Knight moves inside the sunk group |
| 58 | (0, g5) | g6→g5 is one step, up exactly 2 (g5 stayed behind) |
| 59 | (0, g6) | One step back down 2 |
| 60 | (0, h8) | Knight move: h8 sank with the group |

The tour takes **288 + W minutes**. For example, W = 100,000 gives this tour of 100,288 minutes:

```
(0, c2), (3, d2), (0, d1), (0, b1), (35, a1), (0, c2), (0, d2), (0, d1), (0, e1), (0, c1), (54, b1), (0, c1), (0, b1), (0, a1), (0, c2), (0, d2), (0, d1), (0, e1), (4, f1), (0, h2), (5, f2), (4, f1), (0, h2), (0, h3), (0, h2), (0, h3), (0, g5), (6, g4), (6, f4), (0, h5), (0, h4), (9, f4), (0, h5), (6, h6), (0, h7), (0, g7), (1, g8), (0, f8), (4, f7), (0, h7), (2, g7), (0, g8), (12, g7), (16, h7), (25, g5), (0, h5), (0, h6), (12, h4), (18, h6), (0, g6), (12, e6), (0, e5), (4, d5), (100000, b4), (50, d5), (0, f4), (0, g6), (0, g5), (0, g6), (0, h8)
```

**It is legal for every W > 286.** The lowest point outside the group is at −5. Once W/26 > 11, the
sunk group lies strictly below every other point, and e4 lies above it. From then on:

- no outside point shares an altitude with the group during moves 55–60;
- the b4 sit therefore always has the same 25-point group and sinks exactly 2;
- every jump in moves 55–60 is between group points (or g5, left behind at the group's old level),
  and their altitude differences do not depend on W.

So legality does not depend on W. Each point is visited at most 3 times (d5 twice, b4 once, f4, g5
and g6 three times each). The exact simulator confirms all of this for W = 287, 1,250, 10⁴, 10⁶,
10¹², 10³⁰ and 7·10⁴⁰ + 3.

**Every wait is necessary in the literal sense:**
- **Moves 1–53:** the record is strict, so each wait is pinned by its own jump.
- **Move 55:** pinned by move 58, which needs a rise of exactly 2.
- **Move 54 (the wait W):** take t′ = 52. The group then sinks exactly 2, onto altitude 4, where other
  points already lie. The b4 sit at move 55 then has 29 points, so it sinks 50/29 instead of 2, and
  the jump g6→g5 at move 58 has an altitude change of 50/29: illegal. So *some* t′ breaks a later
  move, which is all the literal rule asks.

Hence for every N there is a legal tour longer than N minutes. Under the literal rule no longest
route exists.

**Notes.**
- `tools/check_tour.py` flags move 54 as unnecessary. Its `necessity_check` only tries
  t′ ∈ {t ± 1/997, 0, t + 1, 2t}, and none of those is a merge value. The repo already documents that
  this checker can reject legal tours. `verify_unbounded.py` adds every merge value t′ = n·(k − a(y))
  for points y below the group, and finds a breaking t′ for every wait.
- The construction is not a fluke of one tour. Any time the knight is level with h8, some point lies
  below, and both points of an opposite pair in the group are reachable, it works.
  `find_unbounded.py` finds one in the 8,830 record as well (after move 48).
- The wait W is not *free* (t′ = 52 breaks a later move), but it is not *strict* either: a small
  change of W breaks nothing. That is why Theorem 2, which does give a finite bound, is about strict
  tours.
- Lean: `KM5.longTour_legal`, `KM5.longTour_allNecessary`, `KM5.longTour_time` and `KM5.unbounded`
  (`lean/KnightMoves5/Unbounded.lean`). They prove the claims above for a symbolic W > 286, not
  just for sampled values.

---

## Theorem 2: every strict tour lasts at most 24,329 minutes

**Strict** means that for every sit (t, Q) on P, the target Q is not in the sinking group G.

- If Q ∈ G, the jump's altitude change is 0 whatever t is, so t could change without breaking its
  own jump.
- If Q ∉ G, every other value of t changes that jump's altitude difference and makes it illegal.

`subagents/strict_check.py` tests exactly this.

Notation:
- M is the number of moves.
- k is the knight's altitude.
- m is the **floor**: the lowest altitude the board has had so far (a running minimum). It starts at
  0, never rises, and only changes during sits. Every altitude is always ≥ m.
- For a sit, d is its sink.

**Fact 1: M ≤ 189.** Before each move the knight stands on a point other than h8. These are visits,
the start included, and 63 points × 3 visits = 189.

**Lemma 1: every strict sit has d ≤ k − m + 2.**
The target Q is outside G, so it does not sink during the sit, and afterwards it is still at least
m. The knight ends the sit at k − d, and its jump needs a(Q) − (k − d) ≤ 2. So m − k + d ≤ 2.
This also covers Q = O: O either rose or stayed put.

**Lemma 2: a sit lowers the floor by at most min(d, 2).**
During a sit only G moves down, and it ends at k − d. Every other point stays put or rises, so it
stays ≥ m. The new floor is therefore min(m, k − d).
- By Lemma 1, k − d ≥ m − 2.
- Since k ≥ m, also k − d ≥ m − d.

Call a sit **floor-lowering** when k − d < m. Then

  m_end ≥ −B_fl,  where B_fl = Σ over floor-lowering sits of min(d, 2) ≤ 2·S_fl,

and S_fl is the number of floor-lowering sits.

**Lemma 3: S_fl ≤ M − 8.**
Let h = k − m ≥ 0 be the knight's height above the floor. It changes as follows:
- it is 0 at the start, since a1 is a lowest point;
- a jump raises it by at most 2, and leaves the floor unchanged;
- a sit that is not floor-lowering only lowers it;
- a floor-lowering sit sets it to 0, because the knight's group becomes the new floor.

Now consider h8:
- h8 sinks only when it is level with the knight at a sit. Before the first moment the knight is
  level with h8, it has not sunk, so a(h8) ≥ 17 ≥ 17 + m. At that moment h = a(h8) − m ≥ 17.
- Reaching h ≥ 17 takes at least 9 jumps since the last floor-lowering sit, or since the start.
- Those 9 jumps belong to 9 consecutive moves. At most the first of them (the floor-lowering sit's
  own move) contains a floor-lowering sit.

So at least 8 moves contain no floor-lowering sit.

(The floor is a running minimum rather than the current lowest altitude because the lowest point can
be the opposite point and rise. With the running minimum, m ≤ 0 holds throughout, which Lemma 3
needs. The Lean proof does the same.)

**Identity.**
- Each sit's t = n·d gives a sink of d to each of its n points, so T = Σ_x σ(x), where σ(x) is the
  total sinking of point x.
- With ρ(x) the total rising of x, a_end(x) = a_0(x) − σ(x) + ρ(x).
- Summing over the 64 points:

  T = 421 − Σ_x a_end(x) + R,  where R = Σ_x ρ(x).

**Proof of the bound.**
1. Every final altitude is at least m_end ≥ −B_fl, so −Σ_x a_end(x) ≤ 64·B_fl.
2. Each sit raises at most one point, by its own d, so R ≤ D, the total sink of all sits.
3. The knight's altitude is 0 + ΣΔ − D. At the end it is a_end(h8) ≥ −B_fl. Each Δ ≤ 2, so
   D ≤ 2M + B_fl.
4. Therefore

  T ≤ 421 + 65·B_fl + 2M ≤ 421 + 130·(M − 8) + 2M = 132·M − 619 ≤ 132·189 − 619 = **24,329**. ∎

Lean: `KM5.strict_bound` (`lean/KnightMoves5/StrictBound.lean`). It carries all of the above as a
single invariant `Inv` along the tour and proves that each move preserves it (`inv_step`).

The middle line, 421 + 65·B_fl + 2M, is also a bound for each tour on its own. Every lemma was
checked exactly on the records and on all 1,272 tours in `data/tours/curriculum.txt` (1,268 of
them strict), with no violations:

| Tour | T | M | sits | floor-lowering sits | B_fl | final floor | bound from its own M, B_fl |
|---|---|---|---|---|---|---|---|
| record_9329 | 9,329 | 185 | 128 | 93 | 184 | −179 | 12,751 |
| record_9328 | 9,328 | 176 | 126 | 93 | 184 | −179 | 12,733 |
| record_8830 | 8,830 | 173 | 122 | 90 | 177 | −171 | 12,272 |
| record_7911 | 7,911 | 142 | 95 | 79 | 158 | −153 | 10,975 |
| tour_6303 | 6,303 | 104 | 80 | 69 | 138 | −132 | 9,599 |

### Why 24,329 is far above 9,329

The proof allows three things the real tours cannot do:

1. **181 floor-lowering sits.** The record has 93. Its other 92 moves are zero-wait repositioning
   and the climb-and-merge sits that bring h8 down to the dance.
2. **All 64 points finishing on the floor.** Every sit raises its opposite out of the dance. In the
   record, the average final altitude is −133 against a floor of −179.
3. **Free re-entry of raised points (the R term).** This costs at most about 3%.

A tighter proof would have to show that a raised opposite stays out unless the knight pays moves to
climb back to it, and that visits run out. That is the structure the record's dance follows. Applied to the
record's own move counts, the bound is already down to 12,751, so the remaining slack in the
worst case comes mostly from tours with far more floor-lowering sits than any known tour has.

## Attempts to narrow the strict gap

**A longer strict tour.** The repo's diversity search ran in the background: a penalty trial (`dlns`)
forbids 2–4 of the record's sitting squares, then plain LNS (`lns`, `--free 0`, strict only) starts from
the result. Every candidate was re-checked with `tools/check_tour.py` and `subagents/strict_check.py`.
It found no strict tour above 9,329. Run counts are in the session report.

**A tighter bound.** The slack in Theorem 2 is the final height of points above the floor: its proof
assumes all 64 points end on the floor. In the record, the average point ends 46 units above it.
Most of that comes from **knock-outs**: every sit raises its opposite by d, and a sit with d > 2 lifts
that point more than 2 above the floor. From there it sinks again only at a sit where the knight
stands more than 2 above the floor, and getting the knight that high takes moves that are not
floor-lowering sits. This also gives a clean trade-off:
- in a run of consecutive floor-lowering sits, the next sink is at least the sum of the two floor
  drops;
- so either the floor drops by 1 or less per sit on average, or each sit knocks out its opposite.

The record follows this pattern exactly. Moves 1–98 bring all 64 points within 2 of the floor (64
"alive" at move 75). The dance, moves 99–185, then has knight height 2, floor drop 2 and sink 4 at
every sit, and its alive count falls from 64 to 33. That is about one knock-out per new sitter,
since each point sits 3 times. Counting this way (each sitter knocks out one opposite, at most 3 sits per point, and a long opening
first) suggests that the strict optimum is around 12,000–13,000.

**Why this is not yet a proof.** The natural way to make the knock-out argument rigorous is a
potential that charges a point's height above the floor at full rate up to 2 and at a reduced rate
w < 1 above that. With w = 1 it is exactly the Theorem 2 identity. With w < 1, every sit is still
paid for except one term: sits where the knight stands more than 2 above the floor. Such a sit brings
a whole same-height class of stranded points back down, and earns n·(h − 2)·(1 − w) beyond what the
potential pays.
- Bounding that term needs a bound on **how many stranded points can share one height**.
- The cost of reaching such a height is easy to bound: one move that is not a floor-lowering sit per
  2 units climbed.
- Without board geometry, the batch size can only be bounded by tracking when each point was
  stranded. A point stranded during a run of full floor drops is at a height no other point shares
  for long, so batches are small. With small floor drops they can be large, but then the floor barely
  moves.
- A back-of-the-envelope estimate of that trade-off suggests that no argument ignoring where points
  are on the board can go much below roughly 15,000. The estimate assumes rescues of about two
  stranded points for two extra moves; it is not a computed optimum. Real rescues are
  far more expensive: a stranded point is the mirror image of the sitter, usually on the far side of
  the board, and the record has only 4 spare visits.

So the proven bound stays at 24,329.
