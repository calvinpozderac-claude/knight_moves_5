# Review of `macro/` (reviewer rev_code_b, 2026-10-31, round 1b)

Code reviewed: `subagents/macro/macro.cpp` (in progress; no RUN.md yet). I built my own copy and ran its
own `--selfcheck`, plus a 40 s smoke run.

## 1. Status and claims checked
Selfcheck runs, of the form
`macro --seed-tours data/tours/record_8123.txt --selfcheck --anchor-min K --anchor-max K`:

| Seed | Anchor | Engine replay | Rebuilt from macros |
|---|---:|---|---|
| 8123 | 35 | 8123 | 8123, all 83 macros offered |
| 7911 | 35 | 7911 | 7911 |
| 8123 | 0 | 8123 | **−1 (invalid)**; 94 of 95 macros offered (the 14-hop zero-wait stretch after `(20, e2)` exceeds `--max-hops 4`) |
| 7911 | 0 | 7911 | **−1 (invalid)** |

- **Smoke run.** `--anchor-min 40 --anchor-max 40 --level 2 --time 40 --threads 1` reached 5,341.
  - It did 21,925 macro rollouts in 40 s, about **550 rollouts/s**.
  - Raw NRPA rollouts from the same anchor run at ~15,600/s on this machine (I benchmarked with
    `prior/pnrpa`, prior off). So a macro rollout costs ~25× a raw one.

**Does min-visit routing lose optimal tours? Yes, in two ways.**
1. **One path per (sit, endpoint).**
   - `reach_from` keeps a single min-cost path for each endpoint r. Inside the dance every square is
     "useful", so all hops cost the same and ties go to the lowest square index.
   - Paths with the same endpoint but different visit footprints are discarded. Altitudes don't depend
     on the path, since zero-wait hops change nothing, but the 3-visit cap does.
   - This is why the anchor-0 rebuilds fail.
2. **`--max-hops 4`.** It cuts long repositioning stretches. Dijkstra minimises cost, not hops, so an
   endpoint whose min-cost path is >4 hops is dropped even if a shorter path exists.

## 2. Issues
**MAJOR: the macro space doesn't contain the known optimum for anchors below ~35.**
- Fix: for each endpoint, keep up to K paths with distinct visit footprints, restricted to squares that
  still matter (visits < 3, within 2 of the current altitude). Make the path index part of the
  endpoint code.
- Otherwise, document that the macro search is valid only from anchor ≥ 35 and enforce it.
- Test: the selfcheck rebuild must give 8123 at anchors 0, 23 and 35.

**MAJOR: the DFS is described as "exact over the macro space"; it is not.**
- **The bound is not admissible.** The selfcheck's own "T+pot − final" line is negative on the record
  line:
  - 8123: −1776 with 82 macros left, −345 with 80 left;
  - 7911: −633 and −33.
  So `bound + slack <= best` prunes the record line whenever the DFS tail reaches back that far. The
  greedy idealised dance also isn't a relaxation's maximum: it ignores joins (sinking onto a third
  altitude enlarges a group) and sinks other than 4.
- **`h8_alive_after` is not a necessary condition.**
  - A neighbour of h8 can rise later, when the knight sits on its opposite square near a1.
  - h8 itself rises when the knight sits on a1.
- **The DFS transposition table ignores remaining depth.** It prunes on `tt[h] >= T` only, so a state
  first seen deep with no depth left blocks the same state reached shallower.
- Fix:
  - Drop "exact" from the docs.
  - Store (T, depth) in the TT.
  - Use the pot bound only after the last join (for example, when the selfcheck shows a constant
    surplus), or with `--slack` ≥ 400 by default.
  - Add a regression test: replay every pool tour ≥ 7,900 and assert that no prefix along it is pruned
    by `h8_alive_after` or by the bound.

**MAJOR: no evidence yet that compression pays for a 25× slower rollout.**
- What would settle it: an equal-wall-time comparison against `build/nrpa`:
  - fresh L3 at anchors {35, 40, 46};
  - 4 seeds each, 1 thread, ~10 min per run for both;
  - report mean, max and spread of the best result.
- Macro NRPA needs fewer decisions per tour (~60 macros vs ~100 moves). Whether that buys more than the
  cost is an empirical question.

**MINOR**
- **Checker path.** `CHECKER = "tools/check_tour.py"` is relative, so the record check silently fails
  unless cwd is the repo root. Resolve it relative to the binary, or fail loudly.
- **Missing outputs.** RUN.md and smoke numbers are missing. The pool format (`minutes<TAB>tour`) is
  correct.

## 3. Recommendation: CONTINUE, but fix the representation first
The idea fits the structure well: decisions are sits, and hops are only visit bookkeeping. But until the
macro space provably contains 8123 from an early anchor, and the DFS stops claiming exactness, results
from it can't be compared fairly with raw NRPA.

## 4. Next round
1. **Multi-path endpoints** (K = 3–4 footprints). Success: selfcheck rebuilds 8123 and 7911 at anchors
   0, 23 and 35.
2. **Prune regression test** over all pool tours ≥ 7,900 (cheap, seconds). Success: zero false prunes
   with the default settings, or measured default slack values that achieve that.
3. **Equal-time A/B** against `build/nrpa` L3: anchors 35/40/46, 4 seeds, 10 min each, 1 thread (run
   when cores free up). Success: macro's median best ≥ raw NRPA's at the same time. Otherwise park it
   as a dance-tail DFS polisher.
