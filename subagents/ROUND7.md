# Round 7: polish the newest tours, then produce new families

Record: **8,609** (`data/tours/record_8609.txt`); best strict 8,607 (`data/tours/strict_8607.txt`).
Round 6 (`subagents/lns84/NOTES.md`, "Round 6"): polish + LNS took each AlphaZero tour (8,537-8,553) to
8,603-8,607 within 15 min, then everything converged on one dance tail at 8,607-8,609. Gains beyond that
need a different dance order or group sizes, i.e. a new family.

New on main: `record_8559.txt`, `record_8563.txt` (frozen-network A*, strict; share 109 moves with each other,
only 11 with 8,609).

| Agent | Folder | Cores | Task |
|---|---|---|---|
| polish63 | `subagents/polish63/` | 1 | polish + LNS on 8,559 / 8,563 (lns84's tools and flags) |
| families | `subagents/families/` | 3 | generate new families on CPU (heur_search from a1, diversity-forced LNS), measure each family's ceiling with polish + LNS |

Rules: work in your own folder; `nice -n 5`; jobs under ~60 min, in the background; no git operations; never
delete record files. Any tour above 8,609 accepted by `tools/check_tour.py` goes to `NEW_RECORD_<m>.txt` at once.
Reuse binaries/sources from `subagents/lns84/` (lns with `--start`, free waits; polish and bb patched for free waits).
