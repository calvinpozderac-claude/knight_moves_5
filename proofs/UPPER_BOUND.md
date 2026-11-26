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
| **Strict tours** (each wait pinned by its own jump) | **T ≤ 15,114 minutes** (Theorem 4), down from 15,606⅚ (Theorem 3) and 24,329 (Theorem 2). The best known is 9,330 | Theorems 2 and 3 are machine-checked in Lean: `KM5.strict_bound3`, `KM5.strict_bound`. Theorem 4 is computer-assisted: an exhaustive search (`tools/prove_bound.cpp`, 84 s) that prunes only with two Lean-proved lemmas, `KM5.strict_prefix_bound` and `KM5.pit_dead`. The record is checked as `KM5.record9329_valid` |

The exact optimum over strict tours is not known; Theorem 4 puts it between 9,330 and 15,114.
Attempts to narrow that gap are described in [Attempts to narrow the strict gap](#attempts-to-narrow-the-strict-gap).

**Lean formalization** (`lean/`, Lean 4 + Mathlib). It covers:
- the rules themselves (`KnightMoves5/Rules.lean`);
- Theorems 1–3, and the two lemmas that Theorem 4's search relies on;
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
python3 proofs/bound3_terms.py data/tours/*.txt # Theorem 3: every step, checked on real tours
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

Lean: `KM5.strict_bound` (`lean/KnightMoves5/StrictBound.lean`), and `KM5.strict_bound_length`
for the bound by length, T ≤ 132·M − 619. It carries all of the above as a
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

### Where Theorem 2 loses

The proof charges every floor-lowering sit for all 64 points following the floor down. Real tours
cannot do that: every sit raises its opposite point out of the dance, and in the record the average
point ends 46 above the floor. Theorem 3 makes this loss part of the proof.

## Theorem 3: every strict tour lasts at most 15,606⅚ minutes

Notation as in Theorem 2, with m the running floor and B = −m_end its total drop. A floor-lowering
sit with sink d > 2 is a **knock-out**. Its opposite point O ends the sit at least d above the new
floor:
- if O was level with the knight it stays put, d above the group;
- otherwise it rises by d.

A knock-out is **open** if O never sinks again in the rest of the tour, and **covered** otherwise.
- W is the total floor drop of the open knock-outs;
- B_cov is the total floor drop of the covered ones;
- B_nK is the total floor drop of floor-lowering sits with d ≤ 2.

So B = W + B_cov + B_nK.

**Step 1: depth.** Let depth(x) be the floor's total drop up to the last sit at which x sinks (0 if
it never sinks). Write σ(x) for x's total sinking, so that T = Σ_x σ(x).
- After its last sink, x is at or above the floor of that moment, which is −depth(x).
- Before it, x's altitude changed only by its sinks and its rises.

Hence σ(x) ≤ a_0(x) + depth(x) + (rises of x before its last sink), and

  T ≤ 421 + R_b + Σ_x depth(x),

where R_b is the total of the rises that happen before the risen point's last sink.

**Step 2: the open knock-outs cost depth.** Σ_x depth(x) = 64·B − L, where L sums over the points
the floor's drop after their last sink. Take an open knock-out k with opposite O:
- O does not sink at k (it is the excluded opposite), and never sinks after it;
- so O loses at least τ(k), the floor's drop from k (inclusive) to the end.

A point sits at most 3 times (visits), so at most 3 open knock-outs share an opposite. Their
drops are each at most 2, and τ(k) is at least the drop of the open knock-outs from k on. So

  L ≥ (1/3)·Σ_open τ(k) ≥ (W² + 2W)/12.

**Step 3: their rises are never paid back.** At an open knock-out, O rose by d or stayed level, and
never sank again. So that rise is not in R_b, and

  R_b ≤ D − Σ_open d ≤ 2M + B − W

(D ≤ 2M + B as in Theorem 2, and d > 2 ≥ the floor drop).

**Step 4: rescues cost climbing.** The claim is that **B + B_nK + B_cov ≤ 2M + 2**, so on average a
move adds at most 2 to the left side.
- A knock-out adds only its floor drop, which is at most 2.
- A floor-lowering sit with d ≤ 2 adds twice its drop δ. But d = h + δ, so the knight was at height
  h ≤ 2 − δ. Right after a floor-lowering sit, the knight's height is exactly its jump, and that
  jump is at least that sit's own drop. So if the previous move was a floor-lowering sit, it
  dropped at most 2 − δ and left at least δ of its 2 unused. If it was any other move, it added
  nothing.
- **Covering** knock-out k means that O sinks again. That only happens at a sit at O's altitude, so
  the knight must stand at height at least d_k + (floor drop since k).
  - Right after a floor-lowering sit the knight is at most 2 up, and each further jump climbs at
    most 2. Reaching that height takes moves that are not floor-lowering sits.
  - All knock-outs covered between two floor-lowering sits fit under the floor drop since the
    oldest of them, k₁. That drop is at most H − h_{k₁}, where H is the knight's best height in
    the stretch.
  - So a rescue is paid for in floor drop, not in rescued points: one climb can rescue many points
    only if they were knocked out within the last few units of floor drop.

Formally, the potential

  Ψ = max(0, H − 2) − (drop covered since the last floor-lowering sit) + max(0, 2 − h) + Σ_k max(0, 2 − h_k)

starts at 2 and is never negative. The last sum runs over the knock-outs that are still open, plus
those covered since the last floor-lowering sit, with h_k the knight's height at sit k. Every move
raises B + B_nK + B_cov + Ψ by at most 2, so B + B_nK + B_cov ≤ 2M + 2.

**Step 5: the climb to h8 is wasted.** h8 starts at altitude 17 and only rises until it first
sinks. So the knight stands at altitude ≥ 17 at some sit, the one where h8 first sinks. If h8
never sinks, the knight stands there at the end, on h8. Take the stretch between two
floor-lowering sits that contains that moment:
- its best height is H ≥ 17 − m;
- the knock-outs it rescues fit under the floor drop since the oldest of them, which is at most
  −m, because every knock-out happened with the floor at or below 0.

So at least 17 − 2 = 15 of the climbing stored in Ψ is never used for a rescue. Hence

  B + B_nK + B_cov ≤ 2M + 2 − 15 = 2M − 13, and W = B − B_nK − B_cov ≥ 2B − 2M + 13.

**Proof of the bound.** Steps 1–3 give

  T ≤ 421 + 2M + 65·B − (7/6)·W − W²/12.

Let u = 2B − 2M + 13.
- If u ≤ 0, then B ≤ M − 6.5 and T ≤ 421 + 2M + 65·(M − 6.5) = 67M − 1.5.
- Otherwise W ≥ u by Step 5, and T ≤ 67M − 1.5 + (94/3)·u − u²/12 ≤ 67M − 1.5 + 8,836/3, with the
  maximum at u = 188.

Either way T ≤ 67·M + 2,943⅚, and M ≤ 189 gives **T ≤ 93,641/6 = 15,606⅚**. ∎

Lean: `KM5.strict_bound3` and, by length, `KM5.strict_bound3_length`
(`lean/KnightMoves5/StrictBound3.lean`). As in Theorem 2, a single invariant (`XInv`) is carried
along the tour, and one lemma shows that every move preserves it (`xinv_step`). The invariant
holds:
- the floor at each point's last sink;
- a list of the knock-outs not yet covered, with their drops, heights and floors;
- the potential Ψ;
- whether h8 has sunk yet, and whether the stretch where it did has been closed.

Every step was checked exactly on the records and on all 1,268 strict tours in
`data/tours/curriculum.txt`, with no violations (`proofs/bound3_terms.py`). Each tour also satisfies
the middle inequality with its own M, B and W:

| Tour | T | M | B | W (open) | B_cov | bound from its own M, B, W |
|---|---|---|---|---|---|---|
| record_9329 | 9,329 | 185 | 179 | 170 | 6 | 9,819 |
| record_9328 | 9,328 | 176 | 179 | 170 | 6 | 9,801 |
| record_8830 | 8,830 | 173 | 171 | 164 | 4 | 9,449 |
| record_7911 | 7,911 | 142 | 153 | 151 | 2 | 8,574 |

**Why 15,607 is still above 9,329.** With the record's own numbers the bound is 9,819, only 5%
above the record. The worst case of the proof is a different kind of tour:
- a short opening (the 8-move climb to h8 and nothing else);
- about 44 one-move climbs, each rescuing one knocked-out opposite within one floor drop of its
  knock-out;
- so about 88 of the 277 units of floor drop are covered;
- and W ≈ 188 remains open.

The known tours cover at most 6. The proof allows a rescue whenever the knight's height reaches
the opposite's altitude. On the real board that point is the mirror image of the sitter, usually
on the far side of the board. After a dance step to a neighbour of the sitter, one more step
reaches the opposite only for sitters on d4, d5, e4 and e5. A tighter bound needs this geometry.

Three further ideas were measured and not proved, all against a model of the worst case built from
rescue cycles and plain knock-outs:
- **Rescues strand the layer the knight left.** In a worst-case rescue cycle, the rescue sit is a
  floor-lowering sit at height 4. It lifts every point at the knight's previous position's
  altitude more than 2 above the floor, which in a real dance is half the dance. Counting even one
  such point as lost makes rescues useless, and the model's optimum drops to about 13,000.
  - But in the abstract model the stranded point can double as the knocked-out opposite of a
    later sitter, which cancels most of the gain.
  - On the real board the stranded point and that sitter are mirror images far apart, so
    excluding this again needs geometry.
- **Open knock-outs from rescues come early.** Each rescue cycle leaves its rescue sit's own
  opposite open, early in the tour, while the loss bound pretends that all open knock-outs come
  last. In the model this is worth about 480 minutes.
- **The opening.** Every one of the 13 distinct starting altitudes above 2 needs a sit at that
  altitude. In a single climb-and-merge cascade, each sit after the top costs another 2 of the
  budget, about 24 in all, worth roughly 780 minutes. But rises can merge single points into
  other levels, and sits during the climb cost less, so this is not yet a proof.

## Theorem 4: every strict tour lasts at most 15,114 minutes

This one is computer-assisted. A search goes through every strict partial tour from the start, and
it stops extending a partial tour only for reasons proved in Lean.

**Theorem 4.** Every strict tour lasts at most 15,114⅓ minutes. Strict waits are whole multiples of
their group size on integer altitudes, so tour times are integers, and a strict tour lasts at most
15,114 minutes.

**Lemma A (Theorem 3 from a partial tour; Lean: `KM5.strict_prefix_bound`).** Let `pre` be the
start of a strict tour, and let e be the state of Theorem 3's invariant after `pre`. Then the whole
tour lasts at most ubF(e), where:
- dM = 1 + Σ_{x ≠ h8} (3 − visits(x)) bounds the moves left (h8 once, every other point up to 3
  visits);
- used = B + B_nK + B_cov + Ψ is the rescue budget spent so far;
- the climb to h8 still owed is w = a8 − 2 − μ. Here μ is the highest floor among the current floor
  and the open records. a8 is h8's altitude if h8 has not sunk; if it sank since the last
  floor-lowering sit, a8 is the floor plus the segment's best height. Once paid, w = 0;
- K = used + 2·dM − w;
- ubF(e) = 421 + Z + 2·dM + g(b), with g(b) = 65b − (7/6)u − u²/12, u = max(0, 2b − K), and
  b = (K + 188)/2 clamped to [B, B + 2·dM].

*Proof.* Theorem 3's invariant is proved for any constants (`Par`): the budget constant `c` in
B + B_nK + B_cov + Ψ + τ ≤ 2M + c, the jump constant `cz` in Z ≤ 2M + cz, the floor-drop constants
`cb` and `b0` in b0 ≤ B ≤ 2M + cb, and the h8 constants a8 and μ. For a whole tour these are 2, 0, 0, 0,
17 and 0, which give back Theorem 3.

Restarted at e with the constants read off e (`parOf`), each further move adds at most 2 to the
budget, to Z, and to the floor drop B. A floor-lowering sit lands the knight within 2 of a point
that was above the floor, so the floor drops at most 2 per move.

So at the end, with Δ ≤ dM moves played after `pre`, the end of Theorem 3's argument (`end_bound`)
gives:
- T ≤ 421 + Z_end + 65·B_end − (7/6)W − W²/12;
- W ≥ 2·B_end − (used + 2Δ − w), and W ≥ 0;
- Z_end ≤ Z + 2Δ;
- B ≤ B_end ≤ B + 2Δ.

The right side only grows with Δ, so Δ = dM is the worst case. Over B_end ∈ [B, B + 2·dM], the
maximum is at the clamp of (K + 188)/2 (`gB_max`): g rises with slope 65 while u = 0, and is
concave with its peak at u = 188 after that. ∎

At the start, ubF equals Theorem 3's 15,606⅚. It is at least the final time at every prefix of the
1,300 stored strict tours (`python3 tools/prefix_bound.py check`).

**Lemma B (pits; Lean: `KM5.pit_dead`).** Suppose the knight stands at most c high, no point has an
altitude in (c, c + 2], h8 is above c + 2, and the opposite of every point at most c high is above
c + 2. Then no legal tour continues.

*Proof.* A sit sinks only points at the knight's altitude (at most c), and only the sitter's
opposite rises, which is above c + 2. So low points stay low and high points stay high. A jump
climbs at most 2, so the knight lands on a low point. h8 stays high. ∎

The search takes for c the top of the run of altitudes at most 2 apart that starts at the knight's
altitude. Lemma B needs no strictness.

**The search** (`tools/prove_bound.cpp`) is a depth-first search over every strict prefix: all pinned
moves, at most 3 visits per point, h8 only as the final jump. It carries Theorem 3's state along
(`XS.step`, on the integer engine `src/eng.h`) and does not extend a prefix when:
- ubF ≤ X (Lemma A), or
- it ends in a pit (Lemma B), or
- its game state (altitudes, visits, position) has already been searched completely from a prefix
  that took at least as long. The same suffixes are legal from both, so their totals are no larger.

Finishing jumps are evaluated exactly; a finish above X would be reported as a counterexample.

| X | Positions searched | Time | Result |
|---|---|---|---|
| 15,300 | 4,844 | 0.0 s | proved |
| 15,200 | 100,040 | 0.4 s | proved |
| 15,150 | 497,519 | 1.6 s | proved |
| 15,120 | 15,053,741 | 35 s | proved; largest pruned bound 15,119⅓ |
| **15,115** | **30,157,964** | **84 s** | **proved; largest pruned bound 15,114⅓**, no finish above 57 |
| 15,112 | — | > 25 min | not finished |
| 15,110 | > 537 million | > 29 min | not finished (table full) |

```
g++ -O2 -std=c++17 -o prove_bound tools/prove_bound.cpp
./prove_bound 15115 27      # about 84 s and 3.2 GB
```

**What is trusted.** Lemmas A and B are proved in Lean, with the standard axioms only. The
enumeration itself is C++. It is cross-checked against `tools/prefix_bound.py`, which uses exact
fractions and its own move generator. Run without the table (`./prove_bound check X` and
`python3 tools/prefix_bound.py prove X`), the two searches agree node for node, at every depth:
- at X = 15,300: 4,890 positions, 22,131 bound prunings, 6,350 pits;
- at X = 15,200: 104,830 positions, 521,535 bound prunings, 115,026 pits.

The table is keyed by two independent 64-bit hashes. A collision could only skip a state wrongly,
and at 30 million entries the chance of one is below 10⁻²⁰.

**Why it stops at 15,115.** Below that the positions left are long early dances whose bound hardly
falls. One example is (0,b1)(0,b3)(2,d4)(0,c4)(0,b4)(25,a2)(24,a3)(0,a4)… Its bound falls from
15,111 to 15,101 over 25 moves while 750 minutes pass.

Each such dance move adds exactly 2 to the rescue budget, the most Theorem 3 allows, and climbs
2. So the bound sees no loss, and the search would have to enumerate every dance of that kind. To
go lower, the bound itself has to charge dances, which again needs the geometry described at the
end of Theorem 3.

## Attempts to narrow the strict gap

**A longer strict tour.** The repo's diversity search ran in the background: a penalty trial (`dlns`)
forbids 2–4 of the record's sitting squares, then plain LNS (`lns`, `--free 0`, strict only) starts from
the result. Every candidate was re-checked with `tools/check_tour.py` and `subagents/strict_check.py`.
It found no strict tour above 9,329. Over about 16 CPU-hours it completed 24 polishing runs, starting
from the ten best tours (8,900–9,329) with 2–4 sitting squares penalised early or late in the tour.
Seven runs returned exactly 9,329, and the rest returned 8,957–9,282.

The record is close to saturated within its family:
- its dance has a sitter in 31 of the 32 opposite pairs (all except a1/h8, since h8 can never be sat
  on);
- only 4 visits are unused (e2 once, h1 three times), and both squares are in the half that the
  dance knocks out;
- four sitter squares lose dance sits to the opening (d5, f7, f8 and g8, 6 sits in all), and the
  visit trial that found 9,329 penalised exactly those visits; no tour avoided them all.

So a longer strict tour most likely needs a different opening or merge, not a better dance.

**A tighter bound.** This is Theorem 3. The obstacle recorded here earlier was a rescue that brings
a whole batch of stranded points back at once: bounding it seemed to need a bound on how many
points can share one height. Theorem 3 avoids that. It measures a rescue by floor drop instead of
by rescued points: everything rescued in one climb was knocked out within the last few units of
floor drop, whatever the batch size. What remains between 9,329 and 15,606 is mostly the geometry
of rescues, described at the end of Theorem 3.

**Branch and bound from the start.** Theorem 3's invariant gives a bound for every partial tour:
- replay the invariant along the moves played so far;
- then bound every strict completion by the visits left, the rescue budget already spent, the jumps
  so far, and the climb to h8 still owed.

The climb owed is the waste of the stretch where h8 first sinks: h8's altitude, minus 2, minus the
highest floor at which a rescuable knock-out can have happened.

At the start this bound equals 15,606⅚. It is never below a tour's final length at any prefix of
the 1,300 strict stored tours. `tools/prefix_bound.py` computes it (`trace`, `bfs`, `best`, `check`).

- **Along the record** it falls from 15,607 to 10,193 during the 98-move opening, then stays flat
  through the whole dance (10,149–10,162), and ends at 10,215. In the bound's accounting, nearly all
  of the record's waste is in its opening.
- **Breadth-first over all strict prefixes.**
  - 35,796 distinct positions after 6 moves, with bounds from 15,142 to 15,477; 14,610 of them are
    not pits.
  - Among those, the record's 6-move prefix (15,242) ranks 5,166th.
- **The top-ranked 6-move prefix is a dead end.** (3,a2)(12,a3)(12,a2)(12,a3)(12,a2)(12,a1) dances on
  a1–a3 at once and wastes nothing in the bound's terms. But it sinks a1, a2, a3, c2, c3 and d2 to
  −11…−9 while every other point stays at 2 or more. A jump climbs at most 2, so the knight can never
  leave. Exhaustive search confirms that no strict tour starts this way.
- **With pits pruned,** 68,261 live positions remain after 7 moves, with bounds from 15,075 to
  15,376. The pit test is a gap of more than 2 in the altitudes above the knight, below h8.
  - The top one, (0,c2)(3,d2)(0,b2)(35,c2)(0,d2)(0,d1)(0,e1), is the record's opening two moves
    faster. It is the same position as the record after 9 moves, except that it lifts g7 to 19
    instead of g8 to 15.
  - The record needs that lift: h8 can only be entered from a neighbour at altitude 15 or more,
    and g8 at 15 is the record's stepping stone.
  - From this prefix a goal-directed search found no way to h8 in 200 million nodes. From the
    record's own prefix the same search finds one in 29 nodes.
  - Nine of the twelve top-ranked prefixes are stuck the same way, and none of the twelve grafts
    onto a known tour.
- **Best-first search inside a top branch** barely moves the bound.
  - In the top branch it fell from 15,376 to about 15,005 after 220,000 expansions, reaching only
    23 moves deep.
  - In the best branch that can still reach h8, it fell from 15,375 to 15,075 after 60,000
    expansions.
  - Pruning a branch would need its bound below 9,329.

- **Real tours from a top branch.** `subagents/families/bin/lnsfix.cpp` is LNS with the prefix held
  fixed.
  - For the best completable top prefix, (0,c2)(0,a2)(9,a1)(0,b1)(0,c1)(0,e1)(4,f1), the start tour
    came from the goal-directed search: 22 moves, 22 minutes.
  - Three 20-minute seeds reached 645, 745 and 1,017 minutes.
  - The control fixes the record's own 7-move prefix and also starts from a short completion
    (34 moves, 49 minutes). It reached 765 in the same time.
  - All four results are legal, strict and free of unnecessary waits.
  - So 20 minutes of LNS from a short start rebuilds neither branch: the record's branch, which
    holds a 9,329 tour, does no better here than the top-bound one. Telling the branches apart
    would need the long evolutions that produced the records.

- **A\* from the root.** Always expanding the position with the highest bound, with finished tours as
  goal nodes at their exact length, is exact if it completes, but it cannot complete.
  - After 120,000 expansions the frontier's highest bound was 15,178, and the frontier held 625,157
    positions.
  - The 18 finished tours generated (best 24 minutes) sat far below the frontier, so none was ever
    expanded.
  - Every live position after 7 moves has a bound above 15,075, and the smallest bound falls by
    only 67 per move. So A\* would have to expand essentially every position to about depth 90
    before it could discard any.

So this bound cannot guide a search towards long tours. It measures wasted rescue budget, and the
prefixes it ranks highest are the ones that skip necessary preparation, like the record's lift of
g8. It can still prune exhaustively near the top, which is Theorem 4: depth-first with a fixed
target, the pit test, and a transposition table, instead of best-first. That proves every strict
tour lasts at most 15,114 in 84 s. Below that, the cheap early dances described there keep the
bound flat.

**Weighted A\* and beam search** (`tools/wbeam.cpp`) try to use the bound to build tours, not to
prune them.
- Layers by depth keep the W states with the highest T + w·(UB − T). With w = 1 this is the bound
  alone; with w = 0 it is greedy on time spent.
- With W = 2,000, w = 1 finished at best 29 minutes and w = 0.5 at 16. With w ≤ 0.2 nothing
  finished: the beam died near move 60 with about 2,800 minutes spent.
- With W = 20,000, w = 0.7 reached 24 minutes, and w = 0.3 died at move 61 with 2,886 spent.
- So a beam either follows the bound into short tours or follows time into positions with no way
  to h8. Neither comes near the LNS records.

**The record's endgame is optimal.** `subagents/exact/bb.cpp --nobound` enumerates every strict
completion of a fixed prefix of the record, with a transposition table and no bound
(`subagents/exact/sweep9329_nobound.log`).

| Moves left free | Positions | Time | Best completion |
|---|---|---|---|
| last 13 (from move 172) | 133,640 | 0.1 s | 9,329: the record's own |
| last 15 | 1,441,645 | 0.9 s | 9,329 |
| last 17 | 7,845,375 | 5.5 s | 9,329 |
| last 19 | 73,317,135 | 50 s | 9,329 |
| **last 21** (from move 164) | 412,658,901 | 286 s | **9,329** |
| last 23 | more than 1.3 billion | > 15 min | not finished |

So the record's last 21 moves are the best possible after its first 164: a longer strict tour must
differ from the record somewhere in its first 164 moves. Searches pruned by the heuristic suffix
bound of `bb.cpp`, which is not proved, found nothing better from moves 150, 140 or 130 either.

