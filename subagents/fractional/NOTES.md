# fractional: waits the integer engine leaves out

(Draft. The campaign numbers are filled in below as runs finish.)

## The question
`src/eng.h` only generates **pinned** moves: the wait before a jump is the one sink (a whole number of
units) that makes that jump's dz legal. The exact checker (`tools/sim.py`) accepts any rational wait. It
calls a wait necessary if at least one probe (t +/- 1/997, 0, t+1, 2t, with every other wait fixed) breaks the
tour. What does that extra freedom allow?

## Findings

### 1. With pinned waits, altitudes stay integers (proof)
Start from an all-integer state. A fractional sink puts the knight's group off the integer lattice, and every
other point stays on it, except the opposite point, which can never be a jump neighbour (its offset
(7-2x, 7-2y) is odd in both coordinates). So the only legal next jump is a dz=0 knight move to a member of
the knight's own group. That jump is legal for every wait, so it is not pinned. By induction, pinned
play never leaves the integers. **"Fractional" therefore really means "free sinks"**, plus their step
size. Once a free sink has put a group off the lattice, later *pinned* sinks can be fractional too: they
are the ones that bring it back.

### 2. Longer waits
`pinned_moves` already generates every pinned option: both dz=+2 and dz=-2 for steps, both +1 and -1 for
two-steps, and any multi-unit sink. The dance's 4-unit sits pass through a legal dz=-2 jump on the way.
- The target can never rise, because the opposite point is never a neighbour.
- A knight move to a point above the knight is impossible.
- An in-group target only allows a 0-wait knight move as a pinned move.

So for pinned play the engine is complete. The checker's group rule matches the engine: membership is
fixed when the wait starts, and an opposite point in the group stays put and is not counted in n.

### 3. What the engine misses: free sinks
A **free sink** works like this: the knight sinks its own group by s units (any rational s > 0), then makes
a dz=0 knight move to another member of that group. The jump doesn't need the wait. The wait is
necessary only because later jumps depend on the offset it creates (and on the opposite point's rise).

- **The checker accepts them.** Examples, all `ok=True maxvisits=3 unnecessary=[]`:
  - `examples/frac_free_8123.5.txt` is the record with a **half-unit** free sink at move 129
    (`(9, g8)`, n=18) and the later waits re-pinned: T = 16247/2.
  - `repin.py data/tours/record_8123.txt 129` shows that every sink s in [0,4] on a 1/6 grid gives a legal
    tour, with T = 8123 + s (8,127 at s=4). Move 70 behaves the same way.
- **The puzzle's own 4x4 example uses them.** This is `tools/sim.py` `ex`, T=28; `nec_detail.py --example`.
  Moves 5 `(5, a2)` (n=2, sink **5/2**, fractional), 6 `(3, c1)` and 7 `(2, a2)` are free sinks. For each
  of them, every probe breaks the tour, including dropping it to 0, and on a 1/12 grid it is the only
  working value when the other waits are fixed.
- **Which readings accept the example.** The strict "pinned" reading (the wait must be forced by its own
  jump) rejects the official example. The reading "this wait can't be changed without breaking the
  tour" accepts both the example and our tours.

### 4. Is the necessity check a sound test? (`nec_detail.py`)
When the other waits are fixed, every altitude is affine in the one perturbed wait t'. Each later jump's
legality is then either an identity in t' or true at one isolated t'. Group-membership changes happen
only at isolated breakpoints. So the set of working t' is a finite set of points, plus intervals where
nothing depends on t'.
- The probes t +/- 1/997 test whether t is isolated in that set, which is what "necessary" needs.
- The checker accepts a wait if **any** probe breaks the tour. So it would also accept a wait that can
  be dropped to 0 as long as the +/- 1/997 probe breaks the tour. As a test of "dropping it breaks the
  tour", it is too weak.
- It is not a complete uniqueness test either. A distant isolated t' could also work, and no finite
  probe set rules that out.
- I therefore apply a stricter filter to every reported free-sink tour:
  - +e, -e **and** 0 must all break the tour;
  - on a 1/12-unit grid up to 3s+2, the actual value must be the only one that works.

  For the 8,176 tour's single free sink (move 96 `(23, b3)`, n=23, sink 1), all five probes break and it
  is unique on the grid. The 3 free sinks of the official example pass the same test.

## Scoreboards (kept separate)
| Reading | Best | File | Verified |
|---|---|---|---|
| strict (every wait pinned) | 8,123 here (lns has 8,130 pinned) | `best.txt` (= record_8123) | checker ok |
| example-consistent (free waits allowed, each necessary) | **8,176** | `CHECKER_ONLY_8176.txt` | checker ok + nec_detail strict filter |

The 8,176 tour comes from the 8,123 record, with moves 46-135 re-searched. Its free sink is at a1 at
move 96: sinking a1's group by 1 also raises h8 (opposite a1) by 1. It was found twice: in w1, a level-2
warm run, at anchor 73, and in r1, a level-3 warm run with rng 7, at restart 1, anchor 45.

## Tools (all in this folder)
- `fnrpa.cpp`: single-thread NRPA over the extended move set.
  - Altitudes are scaled by `--sc`; pinned moves are as in eng.h.
  - Optional free sinks of `--fstep..--fmax` scaled units, followed by an in-group knight move.
  - A lazy infinitesimal necessity filter (`free_ok`) is applied to candidate bests. It only tests +e,
    so it is not the checker's full test; it rejects tours like giant-group pumps, where all later moves
    stay inside one group.
  - `--warm K` seeds the weights from the seed tour's suffix and starts from it as the best.
  - `--amin/--amax` set the anchor range.
- `classify.py FILE`: pinned, zero and free waits, and which waits are fractional.
- `repin.py FILE MOVE [SINK]`: change one free sink to k/6 units, re-pin all later waits (same squares,
  same dz), and run the checker.
- `nec_detail.py FILE|--example`: per free wait, which probes break the tour, drop-to-0, and the
  uniqueness grid.
- `campaign.sh`: the sequential level-2 campaign below.

Build: `g++ -O2 -std=c++17 -o subagents/fractional/tmp/fnrpa subagents/fractional/fnrpa.cpp`

Reproduce the runs (from the repo root):
```
B=subagents/fractional/tmp/fnrpa
# w1 (found 8,176 at ~146 s):
$B --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 180 --amin 35 --amax 120 --out subagents/fractional/tmp/w1.txt
# w2 (half-unit free sinks, 8,130):
$B --sc 2 --free 1 --fstep 1 --fmax 8 --warm 10 --level 2 --time 180 --amin 35 --amax 120 --out subagents/fractional/tmp/w2.txt
# r1 (level 3; restart 0 -> 8,130, restart 1 -> 8,176):
$B --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 3 --time 1200 --amin 35 --amax 110 --rng 7 --out subagents/fractional/tmp/r1.txt
bash subagents/fractional/campaign.sh     # seeds 8,123 and 8,130, sc 1 and 2, level 2, 240 s per run
```
Without warm starts (fresh weights), free moves dilute the policy and pump into dead ends. A fresh level-2
run at anchor 40 gave only 2,729 (pinned-only: 5,005). Use warm starts.

## Campaign results
(filled in below)
