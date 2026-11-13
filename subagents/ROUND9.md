# Round 9: beat 9,275

Record **9,275** (`data/tours/record_9275.txt`, strict), from the user's automated family search on main.
Its lineage (all strict, in `data/tours/`), with family signatures (`python3 subagents/families/sig.py FILE`):

| Tour | Dance signature (sitting blocks) |
|---|---|
| 9,275 | f3/f7 g6/h3 a2/b2 a3/b3 a4/b4 a5/b5 a6/b6 c6/c7 c8/d8 a8/b8 a7/b7 c4/c5 c3/d3 d4/d5 d6/d7 e7/e8 f8/g8 |
| 9,271 | f3/f7 g6/h3 a2/b2 a3/b3 a4/a5 a6/b6 b4/b5 c3/c4 d3/d4 c5/d5 c5/c6 c6/c7 c7/c8 a7/b7 a8/b8 c8/d6 d6/d7 e7/e8 f8/g8 |
| 9,268 | f3/f7 g6/h3 d3/d4 c3/c4 b2/b3 a2/a3 a4/b4 a5/b5 a6/b6 c5/c6 c5/d5 ... e7/e8 f8/g8 ("new merge": early sits on f3, h3, g6; groups up to 37 points) |
| 9,080 | f7/h3 f3/g3 e3/e4 d3/d4 c3/c4 b5/c5 b3/b4 a2/b2 ... d8/e8 d7/e7 f7/f8 g8 (sitters moved to f3/g3/e3/e4) |
| 8,946 / 8,900 / 8,887 / 8,842 | earlier families (see README on main) |

How the user's family search got there (README): **penalise a few of a tour's dance sits -> LNS rebuilds the tour
around the ban (raw score drops) -> plain LNS on that tour alone (same-family donors only) climbs past the old
record in 7-35 min**; "order trials" (moving one sitting block earlier/later) on top.

Tools (copy into your own folder; all replay these tours exactly, `--free 0`):
- `subagents/families/bin/lns_r6` (LNS with `--start`, `--focus`, donors), `bin/dlns` (penalty LNS, `--rule "squares:FROM:TO"`),
  `bin/polish`, `bin/bb` (exact tail proof), `sig.py`; scripts `lns.sh`, `ylns.sh`, `pol.sh`; see `subagents/families/NOTES.md`.

| Agent | Folder | Cores | Task |
|---|---|---|---|
| polish92 | `subagents/polish92/` | 1 | polish + tail proof + focused LNS on 9,275 / 9,271 / 9,268 |
| penalty92 | `subagents/penalty92/` | 2 | automated penalty + order-trial sweep from 9,275 (and 9,271) to find the next family |
| graft92 | `subagents/graft92/` | 1 | crossovers: 9,275's new merge (bigger groups) x other families' sitter sets (9,080's f3/g3/e3/e4 side, 8,900, 8,946); merges with even bigger groups |

Rules: own folder; `nice -n 5`; jobs < 60 min in the background; `--free 0`; no git; never delete records; any tour
above 9,275 accepted by `tools/check_tour.py` (ok=True, maxvisits<=3, unnecessary=[]) goes to `NEW_RECORD_<m>.txt`
immediately (use a saver script with a unique name, e.g. `<agent>_saver.sh`, and never kill other agents' processes).
