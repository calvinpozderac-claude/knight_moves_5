# Round 3

Record: **8,329** (`data/tours/record_8329.txt`, prior: guided NRPA plus a free-wait pass; free waits at
moves 141 and 154). Others: lns 8,297, fractional 8,292, strict record 8,255 (prior).
Any tour `tools/check_tour.py` accepts counts (user ruling in ROUND2.md).

The container restarted again: background jobs are gone, files are intact. Rebuild binaries if missing.

Reviewer pass 2 (`reviews/PORTFOLIO.md`): every tour above 8,200 shares 8,123's first 13-15 moves and dance
order, so the portfolio is in one basin. The climb and merge (+0-300) are the largest untouched lever.
Rules: at least 3 improving seeds before claiming a method works; one record hit doesn't move cores.

| Agent | Cores | Task |
|---|---|---|
| prior | 2 | 1.25: guided L3 from anchors 30-45 and 60-75 of 8,329 / 8,297 with more diversity, every pool tour >= 8,200 through the free-wait pass. 0.75: new bottom climbs, guided search from anchors 12-35 |
| fractional | 1 | free-wait insertion sweep over every zero-wait hop of 8,329 and 8,297, then warm L2, 5 seeds; RULES.md fixes |
| lns | 1 | 2x2 starting-set test with free waits on, at least half seeded outside the 8,211 family; Round 2 notes |
| rev_portfolio | cheap | passes at ~30 and ~75 minutes |

Round budget: about 90 minutes of wall-clock time.

## Pruned to one agent (user request)
lns and fractional were stopped mid-round 3 (their files and notes are kept as-is); the reviewer is no longer
invoked. **prior** continues alone, on all 4 cores: guided NRPA with the learned move prior plus the free-wait
pass. It produced the last two records (8,329 and 8,334), and it combines the strongest search method from
round 1 with fractional's free-wait engine.
