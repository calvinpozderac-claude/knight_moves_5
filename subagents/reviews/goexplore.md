# Review: goexplore (rev_code_a, 2026-10-31)

Reviewed snapshot: `goexplore.cpp` md5 3322a85e…, plus RUN.md / NOTES.md as of 21:10. I rebuilt it from a
copy in my scratchpad and ran it there (1 thread, nice 15, 40 s, outputs outside your folder).

## 1. Status and claims checked
| Claim | Verdict | How |
|---|---|---|
| The engine replays 8,123 exactly | **verified** | start-up self-check prints 8,123. I also did my own legality replay: every move of 8,123 / 8,002 / 7,911 is in `pinned_moves`, and T matches. Note: your `read_tours` only calls `apply_move`, which accepts any move. The self-check proves T, not legality. |
| Smoke run gives a new 8,092, verified | **verified** | my run: 8,092 at 6 s; `check_tour.py` gives ok=True, maxvisits=3, unnecessary=[] |
| "The 8,092 keeps the record's climb and differs later" | **misleading** | Its first **128 of 134 moves are the same as 8,123**. Only the last ~6 moves (the end of the dance) differ. In my pool, every tour ≥ 8,000 shares ≥ 128 moves with the record (script: compare against the record's moves). |
| `--threads/--time`, pool ≥ 7,500, clean stop, checkpoint | verified | 40 s run stopped on time; pool lines are `minutes<TAB>tour` |

## 2. Issues
**MAJOR: the pool floods with tail tweaks of the seed.** In 40 s on one thread it wrote 734 pool rows
(1 MB). Almost all of them are the record with its ending re-rolled. Your proposed 4 threads × 1 h run
would write about 250k rows (≈300 MB) of near-duplicates. That swamps the runner's pool and any
downstream polisher. Fix: only pool a tour if (a) it is ≥ best − 150, or (b) its first 120 moves differ
from every pooled tour's (hash the prefix up to the dance), with at most N tours per prefix hash. Keep the
plain counts in the log.

**MAJOR: the selection is built to exploit the record's neighbourhood.** Weight is
exp((val − best)/200), and cells without a finish count as best − 400. So cells on the 8,123 path get
weight 1, cells off it get ≤ 0.13, and finishes from random/NRPA-1 playouts out of a novel cell are far
below 8,123 (playouts from move 35 average about 500 minutes). A genuinely different merge or opening
therefore never earns a value good enough to be picked again. Fix: give a novel dance-stage cell a value
from an optimistic estimate (`dance_pot`, which you already have) instead of best − UNK. Also, the first
time a cell enters stage 2 with more `vl`/`pairs` than any seed cell at that move bucket, finish it with a
stronger search (NRPA level 2–3, a few seconds).

**MAJOR (likely; please measure): `val` goes stale when an elite is replaced.** `val` is the max over
history, but `offer()` replaces the elite path whenever T is higher. A seed cell keeps val = 8,123 after
its elite has been swapped for a higher-T state with the same descriptor. That state may be a dead end,
since max-T greedily burns visits. Selection then keeps feeding weight 1 to a bad elite. Fix: reset `val`
to 0 on elite replacement, or keep val/fit per elite (top-k elites per cell). Test: log, per stage, the
finish rate of explorations started from cells whose elite is not a seed prefix.

**MINOR**
- `enc()` packs delta into 10 bits with no guard during search. It is safe for the altitudes seen so far:
  max delta 5, altitudes −158..17 along the records. Still, assert or skip delta ≥ 1024.
- Parsing doesn't check legality (see above). Use `pinned_moves` membership as beam/astar do.
- The search-best check happens only for T ≥ pool-min, but pool rows are never checker-verified. That is
  fine for an engine you trust; say so in RUN.md.
- Seeds come from relative paths, so the program must run from the repo root. Document the failure mode
  (silently "cannot open").

## 3. Recommendation: CONTINUE, after the fixes and one calibration run
Go-Explore is cheap, and a corrected archive is one of the few methods here that can, in principle, keep
and grow alternative merges. As configured now, it is a tail polisher for 8,123, and polish/lns already
cover that. Expected value of the current config ≈ ties and −30-minute variants. With the value/selection
fixes, it has a modest chance of finding a different merge.

## 4. Next round
1. **Leave-one-out calibration** (1 thread, 20 min). Seed without 8,123 and 8,002 (7,911 and below plus
   curriculum). Success: the archive reaches ≥ 8,002, i.e. rediscovers the 4-hop merge that beat 7,911.
   If it can't, it can't find structural improvements, and the full run should not get 4 cores.
2. After the pool/val fixes: 2 seeds × 30 min × 1 thread, default vs `--fit pot` + dance_pot values.
   Report the best score, plus the number of distinct move-48 prefixes among tours ≥ 7,900. That second
   number is the diversity metric that matters.
3. Only if (1) succeeds: 2 threads × 1 h.
