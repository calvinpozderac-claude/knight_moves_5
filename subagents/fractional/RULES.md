# Free waits: the ruling, and the case for and against (now historical)

**Current record: 8,334** (`data/tours/record_8334.txt`, prior, 3 free waits, checker ok). Before it: 8,330 (lns) and 8,329 (prior + free-wait pass). This folder's records, in order: 8,264 -> 8,288 -> 8,292.

## RULING (user, round 2): free waits are legal
The puzzle's only condition on waits: if a jump (t, P) has t > 0, there must be **some** t' such that
replacing (t, P) with (t', P) makes that move or a later move illegal. Free waits that a later move needs
are legal. The two scoreboards are merged, and the current record is at the top.

What this means for the checker, under that ruling:
- **Sound.** `necessity_check` only accepts a wait if one of its probes (t +/- 1/997, 0, t+1, 2t, all
  other waits fixed) breaks the tour. A breaking probe is such a t', so an accepted tour is always legal.
- **Possibly incomplete in theory.** It could flag a legal wait whose only breaking t' lies outside the
  probe set. With other waits fixed, the working set of t' is a finite set of points plus intervals. If
  neither +/- 1/997 breaks the tour, then t almost surely lies inside a working interval. A breaking t' must then be
  past that interval's end, and 0, t+1 and 2t usually reach it, but not always. This can only reject
  good tours; it never accepts bad ones.
- **`fnrpa`'s `free_ok` filter is stricter still.** It only probes +e, so it only ever loses candidates.

**Pump tours are truly illegal, not just unwitnessed.** In a pump tour, a big group (containing h8) sinks by
free waits while every later move is a dz=0 move inside that one group, for example the 11,465 tour in
round 1. Hold the other waits fixed and change one of those waits to any t' >= 0: every later jump stays
legal, because dz=0 moves inside a group that sinks together are unaffected. That holds even though
group sizes change as the group sinks onto lower points. So no witness t' exists, under any probe set.
`check_tour.py` rejects them (72 unnecessary waits), and `fnrpa`'s `free_ok` filter rejects them during
the search. The probe set's incompleteness therefore does not hide a huge legal tour.

**Precision on incompleteness.** If neither t +/- 1/997 breaks the tour, then t *almost surely* lies inside a
working interval. The exception is the measure-zero case where t +/- 1/997 land exactly on other isolated
working points, which a second epsilon would rule out. For a near-record tour the checker rejects, there
is a cheap exact witness search:
1. solve each later jump's affine dz equation for t';
2. add the group-membership breakpoints;
3. simulate each candidate.

**Exact fractions.** Tours must give waits as exact integers or fractions `a/b`. The checker uses Fraction
arithmetic, and a decimal wait would be misparsed.

**Under the ruling, "floppy" free waits are legal.** A floppy wait is one where a range of values would
work once the later waits are recomputed. It is legal as long as some t' breaks the tour with the other
waits fixed. AGAINST-2 and the three-part test below go beyond the rule. Don't use them as filters.

The analysis below was written before the ruling. It is **historical**. In particular, the "three-part
acceptance test" is stricter than the user's rule, so a tour does not need to pass it.

## Definitions
- **Pinned wait.** The jump that follows is legal only at this exact wait. Nudge the wait and that jump
  becomes illegal. Every move in `src/eng.h` is pinned, and so is every dance sit: the jump is legal at
  dz=-2 with no wait, then at dz=+2 after a 4-unit sink, and illegal in between.
- **Free wait.** The jump that follows is a dz=0 knight move to another member of the knight's own group.
  The target sinks along with the knight, so the jump is legal at *every* wait, including 0. The wait
  matters only because later jumps depend on the offset it creates: the group sinks relative to
  everything else, and the opposite point rises. `subagents/strict_check.py` flags exactly these. A
  positive wait is flagged if the next jump stays legal after a nudge of +1/997.
- **Pinned play stays on the integers (proof).** From an all-integer state, a non-integer sink leaves only
  in-group jumps legal. The opposite point is never a jump neighbour, because its offset
  (N-1-2x, N-1-2y) is odd in both coordinates. So a fractional altitude can only come from a free wait.
  In this puzzle, "fractional waits" are a special case of free waits, not a separate freedom.

## The puzzle's own 4x4 example (`tools/sim.py`, `ex`, total 28)
I checked it with `python3 subagents/fractional/strict_check4.py --example`, which is strict_check on
any board:
```
tour (True, 28) free waits [(5, '5', 'a2', 'legal-at-0'), (6, '3', 'c1', 'legal-at-0'), (7, '2', 'a2', 'legal-at-0')]
```
- The three waits 5 (b4->a2), 3 (a2->c1) and 2 (c1->a2) are free **in exactly the sense strict_check
  uses**. Each next jump is a dz=0 knight move inside the knight's group, legal with no wait at all.
- Wait 5 is a sink of **5/2 units** (n=2), so the example's own altitudes become fractional.
- The other four positive waits (1 at a1->b3, 15 at c1->d1, 1 at d1->d2 and 1 at c2->b2) are pinned.
- The example also fixes the opposite-point convention: at move 7, the opposite point is in the group,
  stays put, and is not counted in n.

### How rigid those free waits are (`nec_detail.py --example`, `repin.py --example M`)
| wait | dropping it to 0 (other waits fixed) | probes that break the tour | working sinks, other waits fixed (1/12 grid) | working sinks, later waits re-pinned (1/6 grid) |
|---|---|---|---|---|
| ex 5 (5, a2), 5/2 units | breaks | all 5 | only 5/2 | only 5/2 |
| ex 6 (3, c1), 1 unit | breaks | all 5 | only 1 | only 1 |
| ex 7 (2, a2), 1 unit | breaks | all 5 | only 1 | only 1 |
| 8,176 move 96 (23, b3), 1 unit | breaks | all 5 | only 1 | only 1 |
| 8,183 move 113 (23, b3), 1 unit | (same pattern) | all 5 | | only 1 |
| *counterexample* `examples/frac_free_8123.5.txt` move 129 | breaks | all 5 | only 1/2 | **every s in [0,4]** |

"Re-pinned" means the later jumps keep their squares and their dz, and every later pinned wait is
recomputed. The free waits in our 8,176 and 8,183 tours are as rigid as the example's: only one value
works, even when the rest of the tour is allowed to re-adjust. My counterexample at move 129 is
"floppy". Taken alone, the wait is necessary. But a whole interval of values gives a legal tour once the
later waits are recomputed (T = 8123 + s). That is the weakest kind of free wait.

## The case FOR free waits
1. **The official example uses them.** Three of its seven positive waits are free, including a fractional
   one. Any reading under which the example is a legal tour must allow free waits. This argument
   assumes, as `tools/sim.py` and the orchestrator say, that the 4x4 example is the one from the puzzle.
   I could not fetch janestreet.com from this container to check.
2. **The strict reading isn't the literal one.** A dance sit's jump is legal before the wait (at dz=-2).
   So "the wait was needed to make the jump possible" fails for every sit, and the strict
   ("pinned") reading is already an interpretation. It is not the literal text. The reading that accepts
   the whole example is "you can't change this wait without breaking the tour". That is what
   `necessity_check` tests.
3. **Ours are no weaker than the example's.** Each free wait in 8,176 and 8,183 passes every probe. It is
   the only working value on a fine grid with the other waits fixed, and the only one when the later
   waits are re-pinned. The example's free waits behave exactly the same way.

## The case AGAINST (historical; AGAINST-2 is moot under the ruling)
1. **The natural reading of "every wait must be necessary"** is local: necessary *for the jump that
   follows*. A free wait is never necessary in that sense, because the jump is legal at 0.
2. **"Necessary for later moves" is circular.** The later waits are fixed numbers chosen *after*
   this wait. With the later waits held fixed, almost any wait looks necessary, because changing it
   breaks some later pinned jump. The `frac_free_8123.5` counterexample shows the failure: it passes
   `check_tour.py` although a whole interval of values for that wait gives legal tours. Our records
   are rigid in this sense, but a rule that is fair to the counterexample would need a stronger
   definition than the checker's.
3. **The example may be illustrative rather than optimised.** Jane Street may not have intended it as a
   test of the necessity clause. Its free waits may simply not have been considered. This argument is
   weaker than FOR-1, because a published example is normally meant to be legal.
4. **Every pre-existing record (7,911 -> 8,211)** follows the strict convention, so a mixed scoreboard
   would compare unlike things.

## Is `necessity_check` (tools/sim.py) a sound test?
It perturbs one wait at a time, with all other waits fixed, to t+1/997, t-1/997, 0, t+1 and 2t. It
calls the wait necessary if **any** of these breaks the tour.
- **Model.** With the other waits fixed, every altitude is affine in the perturbed value t'. Each later
  jump is then either legal for all t' or for one isolated t', apart from finitely many breakpoints
  where group membership changes. So the set of working t' is a finite set of points, plus intervals
  over which nothing depends on t'.
- **What the probes show.** The +/-1/997 probes check that t is *isolated* in that set, the core of
  "can't be changed". That is sound for pinned waits and good evidence for free ones.
- **It isn't a sound test of "can't be dropped".** Because a single breaking probe is enough, a wait
  that could be set to 0 still passes when its +/-1/997 probe breaks the tour. The checker reports such
  a wait as necessary.
- **It isn't a complete test of "the only value that works" either.** A distant isolated t' can also
  work, and no finite probe set can exclude it. Only an exact test closes that gap. That test would
  solve every later jump's affine equation for its candidate t', add the membership breakpoints, and
  simulate each candidate.
- **It never re-optimises later waits** (the circularity in AGAINST-2), so it accepts floppy free waits
  like the counterexample.
- **It isn't free of false alarms in principle.** For example, t-1/997 < 0 is skipped, and a probe can
  hit an accidental coincidence. In practice I saw none.

**(Historical, superseded by the ruling.) The stronger acceptance test I recommended for any free-wait tour:**
- all five probes break the tour, including drop-to-0;
- with the other waits fixed, the wait is the only working value on a 1/12 grid up to 3s+2
  (`nec_detail.py`);
- it is also the only working value when the later waits are re-pinned (`repin.py`).

8,176, 8,183 and the official example pass all three; the floppy counterexample fails the third.

## Verdict (historical, before the ruling)
- If the 4x4 example is official, free waits are **legal by example**: the example itself uses three, one
  of them fractional.
- The 8,176 and 8,183 tours (and any future FREEWAIT tour that passes the three-part test above) are then
  legitimate.
- Until someone confirms that against the original puzzle page, they stay on a separate scoreboard. A
  free-wait tour beating the strict record should be reported as "record under the example-consistent
  reading", never mixed with strict tours.
