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
