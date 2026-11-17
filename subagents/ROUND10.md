# Round 10: one agent, target the lost sits

Record: **9,290** (`data/tours/record_9290.txt`, checker ok). Tools: `subagents/sfl/` (LNS with free sinks inside
the search and an exact necessity check; `NOTES.md` there), `subagents/families/bin/` (dlns penalty LNS, polish,
bb), `subagents/tour_anatomy.py FILE` (every sit with its group size, visits per point, sits per point, final
altitudes).

## Where the record's minutes are, and where they are lost (orchestrator's analysis of 9,290)
- Score identity: every minute, the n points of the knight's group sink 1/n each, so **T = total descent of all
  points**. A sit of d units in a group of n is worth n*d; in the dance d = 4.
- Pre-dance (moves 1-91): 1,082 minutes. The dance starts at move 92 with **all 63 visitable points in two groups**
  (36 + 27, h1 alone), so the merge is already at the maximum group total.
- Dance (moves 92-178): 8,208 minutes from 85 sits. Each new sitter's first sit expels its opposite point, so the
  groups shrink by one point per sitter; that part is forced.
- A pair of opposite points can give at most 3 sits (one sitter, 3 visits). 31 pairs are usable (a1/h8 is not), so
  the dance could have **93 sits; it has 85**. The 8 missing sits, each worth about 64-140 minutes:

| Lost sits | Where the visit went | Rough value |
|---|---|---|
| g8: 2 | climb, moves 28 and 33 (g8 is used to raise b1) | 2 x 64 |
| f8: 1 | climb, move 29 | 76 |
| d5: 1 | merge, move 46 (f5 -> d5 -> e5) | ~96 |
| c5: 1 | dance transit b7 -> c5 -> c4 at move 152-153 (no sit on c5) | 64 |
| c2 / f7 pair: 3 | neither sits in the dance; both are still in the groups at the end with no visits left (c2: moves 1, 11, 60; f7: moves 30, 44, 82). c2 is next to b2 and c3 of the other group, so a c2 block fits the current layout | 3 x 72-140 |

  Together roughly **+500 to +600 minutes** if the opening and merge stopped spending visits on dance sitters.
  The opening has slack: 11 visits on non-sitter points are unused (d1, e1, e6, f1, f6, g2, g3, h2 one each; h1 three).
- At the end the groups hold exactly the 30 sitters (no visits left) plus c2, f7, a1 and h8.
- What is saturated: strict LNS and penalty rebuilds on *sits* (75+ rules return to this sitter set); the free-wait
  pass after the search (fixed point 9,286); sfl from 9,290 (37+ jobs, 193 distinct 9,290s).
- What nobody has tried: a penalty on **visits** (arrivals) to dance-sitter squares before the dance, so the search
  frees those visits and the dance can turn them into sits.
