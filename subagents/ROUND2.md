# Round 2: three research agents and one reviewer

The first pass is over. The team is pruned to the three most promising directions plus one reviewer.
The rules in `BRIEF.md` still apply (own folder, no git operations, `nice`, exact checker).

**Record: 8,211 minutes, strict** (`data/tours/record_8211.txt`, found by lns). "Strict" means every
positive wait is pinned: nudging the wait makes the next jump illegal. Check it with
`python3 subagents/strict_check.py FILE`. Other tours from the first pass:
- 8,130 (lns), 8,127 (polish): both strict.
- 8,176 (fractional): one free wait, where the knight sinks together with its target. The puzzle's own
  4x4 example uses waits like this, but the rules question is still open, so these tours go on a
  separate scoreboard.

Proven facts:
- The end of each record is exactly optimal (exact): 8,123 after move 116, 8,130 after move 130.
- Every gain so far came from the climb and merge (moves 15-60), not from the dance.

| Agent | Direction | Cores |
|---|---|---|
| lns | LNS / annealing on the climb and merge, from 8,211 and its relatives; borrow polish's operators | 1 |
| prior | learned-prior NRPA: prove it at level 3 with an equal-time A/B, then run fresh guided searches at merge and early-dance anchors of 8,211; polish the results | 2 |
| fractional | free waits on top of the strict records (separate scoreboard), plus a precise rules write-up | 1 |
| rev_portfolio | reviews all three each round; guards the evidence standard and the portfolio balance | cheap |

Evidence standard (from `reviews/PORTFOLIO.md`): fixed tasks, equal CPU-seconds, at least 5 seeds,
full spread reported; count only completed restarts.

Round budget: about 90 minutes of wall-clock time per agent.

## Rules ruling (from the user, 2026-11-01)
The puzzle's only stipulation on waits: if a jump (t, P) has t > 0, there must exist a t' such that
replacing (t, P) with (t', P) makes that move or a later move illegal. So a free wait is legal whenever
changing it breaks a later move. `tools/check_tour.py`'s `necessity_check` tests exactly this (it tries
several t'; finding one that breaks the tour proves necessity, so the checker never accepts an invalid
tour). **The scoreboards are merged: any tour the checker accepts counts.** Record: 8,239
(`data/tours/record_8239.txt`, one free wait at move 153).
