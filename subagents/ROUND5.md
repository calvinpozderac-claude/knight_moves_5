# Round 5: beat 8,484

**Target: any tour above 8,484 that `tools/check_tour.py` accepts** (ok=True, maxvisits<=3, unnecessary=[]).
The rules allow "free" waits that are only needed for later moves (ruling in `ROUND2.md`).

`data/tours/record_8484.txt` (8,484, strict, found on `main` by AlphaZero-style self-play) and its sibling
`data/tours/record_8337.txt` are a **new basin**. They share no opening moves with 8,123/8,348: the climb
starts c2 d2 d1 b1 c1 ..., heads up the h-file, and has 161 moves. None of our tools has been applied to it yet.
What worked on the old basin (see `SUMMARY.md`):
- free-wait pass (`subagents/fractional/tmp/fnrpa`, source `subagents/fractional/fnrpa.cpp`): +40-50 on strict tours;
- prior-guided NRPA (`subagents/prior/pnrpa`) at anchors 55-95 of the record, giving new strict skeletons;
- LNS (`subagents/lns/lns_v3`, source `lns_v2.cpp`/`lns.cpp`) and the polisher (`subagents/polish/polish`) on the climb and merge.
- exact enumeration (`subagents/exact/bb`) proved the old tails optimal; may do the same here.

Machine: 4 cores, no GPU. `src/eng.h` changed on main (merged in): rebuild what you use and confirm it replays
8,484 to exactly 8,484. Write any tour above 8,484 that the checker accepts to
`subagents/<you>/NEW_RECORD_<m>.txt` the moment you have it. Never delete record files. No git operations.

| Agent | Folder | Cores | Direction |
|---|---|---|---|
| fw84 | `subagents/fw84/` | 1 | free waits on 8,484 and 8,337: fnrpa pass with many rngs, insertion sweep, then warm searches |
| guided84 | `subagents/guided84/` | 2 | prior-guided NRPA anchored across 8,484 (retrain the prior with this basin), then the free-wait pass on each distinct skeleton |
| lns84 | `subagents/lns84/` | 1 | LNS + polisher over the whole 8,484 tour; crossovers with 8,337; exact tail proof |

## Pruned to one idea (user request)
LNS + polisher (lns84) had already reached 8,596 when pruning, against 8,488 for guided84 and no gain for fw84,
so only lns84 continues, on all 4 cores. fw84 and guided84 are stopped; their notes are kept.
