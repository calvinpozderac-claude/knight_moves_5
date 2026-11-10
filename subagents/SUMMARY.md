# Summary: multi-agent push on the Knight Moves 5 record

**Result: 8,123 → 8,348 minutes (+225).** The record is `data/tours/record_8348.txt`.
`tools/check_tour.py` accepts it (ok=True, maxvisits=3, unnecessary=[]). It has three free waits, at moves
100, 117 and 119. The best tour with no free waits is 8,297 (`data/tours/strict_8297.txt`).

All work ran on one 4-core machine with no GPU, over about 4 search rounds. Each agent's code, commands and
notes are in `subagents/<agent>/` (`NOTES.md`, `RUN.md`). Reviews are in `subagents/reviews/`. The full
scoreboard is `subagents/RESULTS.md`.

## Record progression

| Record | Found by | How |
|---|---|---|
| 8,123 | (start) | NN-guided A* (nnastar), from before this session |
| 8,130 | lns | LNS/annealing that rewrote the climb and merge (moves 15-60) of 8,123 |
| 8,211 | lns | LNS v2 with windows in moves 1-60, seeded with 12 rerouted 8,123 variants plus crossover |
| 8,235 | fractional | free-wait engine run from 8,211; the result happened to be strict |
| 8,239 | fractional | one free wait (move 153); counted after the rules ruling |
| 8,292 | fractional | free-wait search, two free waits |
| 8,329 | prior | prior-guided NRPA found a strict 8,290, then the free-wait pass |
| 8,330 | lns | LNS with free waits; the same lineage as 8,329 rewritten in the merge (+1) |
| 8,334 | prior | strict 8,295 + free-wait pass (three free waits) |
| 8,338 | prior | strict 8,297 + free-wait pass (four free waits) |
| **8,348** | prior | level-4 guided search gave a different strict 8,295; free-wait pass (1 of 8 rngs) |

## The rules question: free waits
A "free" wait is one where the knight's next jump stays legal whatever the wait, because the target sinks with
the knight's group. Such a wait is only necessary because of later moves. The user ruled on this with the
puzzle's own wording: a wait t > 0 is legal if *some* t' would make that move or a later one illegal.
`necessity_check` in `tools/sim.py` tests exactly that. It accepts a wait only when it finds a breaking t', so
it never accepts an illegal tour, though it may reject a legal one. The puzzle's 4x4 example uses three such
waits. `subagents/strict_check.py` lists the free waits in any tour. Free waits gave about +40-50 on top of
the best strict tours.

## What each method did

### Search agents (round 1)
| Agent | Method | Best | Verdict |
|---|---|---|---|
| lns | LNS / simulated annealing over whole tours (destroy-and-repair windows, crossovers) | 8,330 | **Produced the first big jump (8,211).** Gains came from the climb and merge; the 8,211 jump needed a varied starting set, not more iterations |
| fractional | Rational and free waits beyond the integer pinned engine (`fnrpa`) | 8,292 | **Opened the free-wait lever.** Its free-wait pass is the second step of every later record |
| nrpa | GNRPA (fixed prior bias), stabilised and beam variants | 7,805 | Code correct; claimed gains came from too few runs. Folded into prior |
| exact | Exhaustive suffix enumeration (DFS + transposition table) | — | **Proved** 8,123 is optimal after move 116 and 8,130 after move 130. Its upper bound turned out unsound, so only the full enumeration counts as proof. Tail polishing is finished |
| opening | Climb/merge visit-budget analysis, rerouted prefixes | 7,989 | 300 rerouted 8,123 variants, used as LNS seeds (this led to 8,211) |

### Code-ready agents (round 1b; built and smoke-tested, mostly not given full runs)
| Agent | Method | Smoke best | Finding |
|---|---|---|---|
| prior | Learned move prior (42 features, small MLP ranker) added to NRPA's weights | 7,930 → **8,348** | **The strongest new search method.** At equal time, level 3, with a holdout model that never saw the records, its worst restart beat stock NRPA's best at anchors 40 and 55 |
| polish | Re-routing, sit insertion, window re-search, NRPA tail | 8,127 | Turned 8,123 into 8,127 in 60 s; large gains on old tours (6,303 → 7,124) |
| goexplore | Go-Explore / MAP-Elites archive | 8,092 | Stays near the record's tail |
| nmcs | NMCS, Lazy NMCS, SP-MCTS | 7,966 | Weaker than NRPA; the playout policy matters more than the search method |
| genetic | Memetic GA, splice crossover + repair | 8,130 (a seed) | Settles on near-copies of the record within seconds |
| macro | Macro moves (sit + min-visit hops) | 8,123 | The record's last 17 macros are exactly optimal; compression only 97 → 83 |
| astar_cpu | CPU port of NN-guided A* | 5,063 | Works end to end; never got its ~1h45 full run |
| beam_dom | Beam with dominance pruning | 6,798 | Its estimate is not an upper bound; too weak |
| goal | Goal-conditioned opening search | 7,306 | Dance-start scorer is off by about ±1,000 minutes, so it can't guide search |
| runner | Job queue, exact re-verification, leaderboard | — | Infrastructure; found the first records sitting in other agents' folders |

### Reviewers
Four reviewers in round 1, then one (`rev_portfolio`) in rounds 2-3. They re-verified every claimed tour and
caught several problems:
- exact's unsound bound;
- possible test-on-train leakage in prior;
- comparisons that used too few runs or counted restarts cut short by the time limit;
- a deleted record file;
- identical finishes counted as separate samples.

They also kept a headroom estimate. The reviewer's own analysis of the 4x4 example reversed its earlier
"strict is natural" reading. Evidence standard used: equal CPU time, several seeds, distinct finishes only.

## The loop that produced the last five records
1. **Guided strict search:** prior-guided NRPA (`subagents/prior/pnrpa`, level 3/4, random β and initial
   weight noise per restart), anchored at moves 55-95 of the current record. It returns strict skeletons of
   8,290-8,297.
2. **Free-wait pass:** `subagents/fractional/tmp/fnrpa` (warm level 2, 120-180 s) on each distinct skeleton,
   with 6-10 rngs, because results vary a lot between rngs. 8,348 came from 1 of 8 rngs on one particular
   8,295 skeleton.

Commands: `subagents/prior/NOTES.md` (scripts `r3c.sh`, `r4.sh`, `r5.sh`, `fw_multi.sh`, `fw4.sh`).

## What we learned
- **The dance is close to saturated; the climb and merge are not.** Every gain came from moves 15-60 or from
  free waits. Exhaustive search proved the record tails optimal.
- **One basin.** Every tour above 8,200 shares 8,123's first 13-15 moves and its dance-step order. The best
  tour outside that family is 8,166 (`subagents/prior/CLIMB_8166.txt`), about 180 short.
- **Strict skeletons have plateaued at 8,297.** About 200 guided searches since round 3 found nothing
  higher. Recent gains came from rare, lucky free-wait placements.
- The learned prior beats stock NRPA, but it pulls every restart toward the same tour. Random β and noise
  help only partly.

## Recommended next steps
1. Build free-wait moves, with the necessity check, into the guided search (`pnrpa`), so it plans around
   free waits instead of adding them afterwards.
2. Many more distinct strict skeletons of 8,290 or more, each with 8 or more free-wait rngs.
3. Give a real budget to a second basin (a new bottom climb or dance order), for example the 8,166 family,
   so we stop relying on one lineage.
4. Give the untested code-ready methods (astar_cpu, goexplore) their full runs, seeded without the records,
   to see whether they can find structure rather than re-polish.

## Caveats
- Several container restarts and usage-limit pauses killed background runs mid-round, so some planned
  comparisons ended with fewer seeds than intended (for example prior's second seed at anchor 40).
- `check_tour.py` reads only integers and a/b fractions; decimal waits are silently dropped.
- Records in `data/tours/` are permanent copies. Agents' own `NEW_RECORD_*` files may be renamed or removed by
  their scripts.
