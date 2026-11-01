# Scoreboard

Starting record: 8,123 (data/tours/record_8123.txt)

| Round | Agent | Approach | Best | Notes |
|---|---|---|---|---|
| 1b | goexplore | Go-Explore / MAP-Elites archive (45 s smoke, 1 thread) | 8,092 (verified) | keeps record opening; full run pending |
| 1 | lns | LNS / annealing, gain in climb+merge (moves 15-60) | **8,130 (verified, strict: integer pinned waits)** | NEW RECORD; source of the binary used (`lns_v1`) not kept; 1 seed |
| 1b | polish | tour polisher | 8,126 (verified, strict) | |
| 1 | fractional | in-group sink (wait 23 at a1, then level knight move to b3 in the same group) | 8,176 (passes checker; not strict) | valid only if in-group sinks count as "necessary"; the puzzle's own 4x4 example uses such sinks — needs a rules decision |
| 1b | macro | macro-move NRPA/DFS/beam | 8,123 (rebuilt record) | last 17 macros of 8,123 exhaustively optimal in macro space; compression only 97->83 |
| 1b | astar_cpu | CPU value-guided best-first (MLP, nnastar port) | 5,063 (smoke model, 45 s data) | full pipeline ~1h45 on 4 cores untested |
| 1b | prior | learned move prior (GNRPA bias, MLP ranker) | 7,930 (4 s L3 smoke, anchor 60) | L2 equal-time: prior beats stock at anchors 35 & 60, holdout model (no records) equally good; L3/L4 A/B open |
| 1b | genetic | memetic GA, splice crossover + repair | no gain over seeds (7,200->7,234 without records in 50 s) | crossover beats first parent ~30%; single mutations rarely help |
| 1b | polish | reroute + sit insertion + window re-search + NRPA tail | 8,127 (verified, strict; from 8,123 in 60 s) | below lns 8,130; big gains on older tours (7,200->7,458, 6,303->7,124) |
| 1b | nmcs | NMCS / Lazy NMCS / SP-MCTS | 7,966 (anchor 100, NMCS L3) | playout policy dominates; weaker than NRPA so far |
| 1 | exact | full suffix enumeration (bound unsound, not used for proofs) | 8,123 | PROVEN: 8,123 optimal after move 116; 8,130 optimal after move 130; 8,002 after 118; 7,911 after 126 |
| 1 | lns | LNS v2 (after relay: windows in moves 1-60, seeded from 8,130) | **8,211 (verified, STRICT)** | NEW RECORD (+88 over 8,123) |
| 2 | fractional | free-wait engine run from 8,211 (strict output) | **8,235 (verified, STRICT)** | NEW RECORD |
| 2 | fractional | free-wait board | 8,239 (1 free wait at move 153) | separate scoreboard |
| 2 | — | rules ruling: free waits legal when needed for later moves | **record = 8,239** (fractional) | scoreboards merged |
| 2 | fractional | free-wait search | **8,292 (verified)** | record |
| 2 | lns | LNS + free-wait seeds | 8,288 (verified) | |
| 2 | prior | learned-prior NRPA | 8,255 (verified) | |
| 2 | lns | LNS + free waits | 8,297 (verified) | |
| 2 | prior | guided NRPA + free-wait pass | **8,329 (verified)** | record |
| 3 | lns | LNS + free waits | **8,330 (verified)** | record (+1) |
| 3 | prior | guided dance re-search (strict) | 8,295 (verified, STRICT) | strict record |
| 3 | prior | guided NRPA + free-wait pass (3 free waits) | **8,334 (verified)** | record |
| 3 | prior (sole agent) | guided NRPA + free-wait pass, 4 cores | **8,338 (verified)** | record |
| 3 | prior | strict skeleton (L3 prior, random beta + init noise) | 8,297 (verified, STRICT) | strict record |
| 4 | prior | deeper guided search + free-wait pass | **8,348 (verified)** | record |
