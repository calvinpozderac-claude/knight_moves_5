# Round 8: push F5, find the next family

Record **8,812** (`data/tours/record_8812.txt`, family F5, strict). Families and tools: `subagents/families/NOTES.md`
(`sig.py` family signatures, `bin/dlns` penalised LNS, `lns.sh`, `pol.sh`, `saver.sh`).
New on main: `data/tours/record_8595.txt` (frozen-network search, strict, family F1 variant: extra b2/b3 a3/a4 block).

| Agent | Folder | Cores | Task |
|---|---|---|---|
| f5push | `subagents/f5push/` | 1.5 | exact tail proof (bb) on 8,812 -> focused LNS on F5 (>=4 seeds) -> polish; also polish + short LNS on main's 8,595 |
| families | `subagents/families/` | 2.5 | penalty runs on F5's own blocks (and F1's) to force the next family; measure each new family's ceiling |

Rules as before: `--free 0` unless the necessity of every generated free wait is checked; only tours accepted by
`tools/check_tour.py` count; NEW_RECORD_<m>.txt at once for anything above 8,812; no git; never delete records.
