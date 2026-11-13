# graft92: crossovers and merge engineering (Round 9)

Goal: a tour above 9,275. 1 heavy thread, nice -n 5, `--free 0`. Start 23:16 (2026-11-13).

## Tools (this folder)
- `bin/lns_r6`, `bin/dlns`, `bin/polish`, `bin/bb`: copies of `subagents/families/bin`.
- `bin/graft` (`bin/graft.cpp` = dlns.cpp +): `--gw W --gref G --grange LO HI` rewards W per point of the largest
  sit group (any wait > 0, move index in [LO,HI)) above G; "feasible" (`*.feas_best.txt`) then means gmax > G and
  no rule hit. `--sitmin` applies to `--rule`. `POTDUMP=1` prints per move: group sizes and lns's idealised
  dance potential `dance_pot2`.
- `lns.sh` (plain lns_r6, `--free 0`), `ylns.sh` (bin/graft with penalties/rewards), `queue1.sh`,
  `graft92_saver.sh` (lns/, y/, x/ above 9,275 that check_tour accepts -> `NEW_RECORD_<m>.txt`; heartbeat
  `saver.alive`). Event log `events.log`.

## Analysis
- Direct crossovers (lns_r6 `--xover 1`: prefix of A + DFS + resync anywhere in B, all 9 pairs of 8,830..9,275):
  nothing above the parents (best combos just reproduce the B tour; 8,830 x 8,946 -> 8,952 only).
- 9,275's merge spends f3 (3 visits), g3 (3), e3 (3), e4 (2) in moves 55-95 (small sits of 25-60 min). Those
  are exactly 9,080's dance sitters, so a 9,275-prefix + 9,080-dance graft is infeasible as is.
- Group sizes at dance sits (n = minutes/4): 9,275 starts U/L = 37/25 (a2/b2) and decays to 19/16; L hits 16 by
  c3 and stays there. 9,080 starts 31/31 and decays slowly to 17/17. 9,268 starts 36/27.
- dance_pot2 (idealised dance value) max along tour vs score: 9,275 9,661; 9,271 9,661; 9,268 9,722;
  9,080 9,471; 8,946 9,414; 8,900 9,393; 8,830 9,370. Gap score-pot: 386-540 (correlates with score).

## Runs
- G1 s901 (23:18, 40 min): plain lns_r6 from 9,275, donors 9,080, 8,946, 8,900, 9,271, 9,268 (cross-family),
  focus 40-100 p 0.5.
- queue1 (after G1): YA = graft penalty from 9,275 (no sits >= 20 min on f3,g3,e3,e4 in moves 60-95, i.e. keep
  them for the dance; no b6,c6,d5,d6 sits from move 96, i.e. 9,080's sitter swap), pen 150, donors 9,080 + 9,275
  family; then plain LNS on its feasible best. YG = group reward gw 100 over gmax 37; then plain LNS.
