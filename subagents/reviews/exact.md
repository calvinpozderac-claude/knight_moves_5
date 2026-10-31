# Review: exact (rev_search), 2026-10-31 21:15 UTC

## 1. Status and claims checked
| Claim | Verdict | How checked |
|---|---|---|
| Engine replays 8,123 | **VERIFIED** | `./stats ../../data/tours/record_8123.txt` ends with `final T=8123` (replay through `pinned_moves`, waits matched exactly) |
| `ub_future` is an upper bound on future minutes | **REFUTED** | `./bb ../../data/tours/record_8123.txt 126 --check --alpha -100000000 --nobound --tt 20` gives **31 violations**, e.g. `exact 241 > ub 140 at T=7676 pos=f6`. From k=124: **304 violations** out of 41,188 nodes checked. |
| 8,123 is optimal given its first 124 moves | **VERIFIED (exhaustive)** | the same command from k=124 and k=126 gives `proven_ub_total=8123`. With `--nobound` and alpha = -1e8, the TT never cuts (the cut needs `T+ub <= ALPHA`), so this is a full enumeration with no dependence on the hash. The k=118 enumeration is running now. |
| No result files / NOTES.md yet | n/a | |

## 2. Issues
- **BLOCKER (for any "proven" claim): the bound is unsound.** `ub_future` is a heuristic and is violated in practice (above). Every `DONE ... => no tour > alpha with this prefix` line from a run *with* the bound proves nothing. Likely causes, each a relaxation that has to be widened:
  1. The knight's component uses only orthogonal (between levels) and knight (within a level) edges. Two-away jumps (dz +/-1, after a 1- or 3-unit sink) and hops through points outside the two levels can reach more sitting points.
  2. Points outside {top, top-2} are ignored, but a sinking group absorbs any point it passes. Growing n, and sinks > 4 onto a point 4 below (sink 6 is legal), are both possible.
  3. The block removal (`g[other]--`) is a guess, as the in-code "??" says, not a maximum over the choices.
  4. The endgame (sits near h8, the final wait into h8) does not follow the 4·n pattern. Your violations sit exactly there (f6/g8 at T of about 7,650).

  Fix: either prove a relaxation that is sound by construction, or call the method "heuristic B&B" and never "proven". Keep `--check`. It is exactly the right tool; run it on several tours and suffix lengths before trusting any bound change.
- **MAJOR: the TT holds only upper bounds and is used only for cutoffs, so exhaustive mode gets no transposition savings.** With alpha = -1e8, every transposition is searched again. Store an `exact` flag. A node is exact when no descendant was cut by the bound or the TT; with `--nobound` that is always true. On a hit, return exact values whatever alpha is. This turns the enumeration into a DP over distinct states and should push the exhaustive frontier from k of about 120 back much further.
- **MINOR: hash equality is treated as state equality.** The key is a 64-bit hash, so a collision (rare, but possible over 10^8 nodes) could cut wrongly. For proof runs, verify a second independent 64-bit hash, or store the full state in a side table.
- **MINOR: misleading output.** In `--check` mode, `best=-100000000 ... => no tour > alpha` is printed. Print "exhaustive optimum of suffix = r" instead, and don't print the "no tour > alpha" line when alpha was not a real target.
- **MINOR: the move ordering and the `onTour` flag are fine.** The DFS stack is recorded correctly in `save_solution`. The final move into h8 is handled as a leaf and respects the "first arrival ends the tour" rule.

## 3. Recommendation: PIVOT, from "B&B with a heuristic bound" to "exact suffix DP + headroom measurement"
A sound, tight bound for the whole dance is a research problem, and the current one is wrong at the end of the tour, which is the very region where B&B is cheap. The deliverable with clear value is to say *where the headroom is*. For example, "8,123 is suffix-optimal from move 124 (exhaustive)" tells everyone to stop polishing the tail. Do the same for the new 8,130 (lns, pinned) and 8,176 (fractional, one in-group sink; the free sink is not in your move set) tours.

## 4. Next round
1. **Exact TT flag + DP.** Find the furthest-back k at which an exhaustive optimum of the 8,123 suffix finishes in 10 min. Success means k <= 110, with the result "suffix-optimal from k" or a better tour.
2. **Repair the bound with check mode.** For each violation, log the move sequence that beats the bound and widen the matching relaxation. Iterate until there are 0 violations from k = 110..126 on 8,123, 8,130 and 8,002. Even then, label it "empirically valid".
3. **Headroom table.** Using the exhaustive DP, report (k, optimum of suffix from k) for k = 110..130 on the top 3 tours. Success means the table is in NOTES.md, and the orchestrator can cut tail-polishing compute everywhere if every row equals the tour's own score.
