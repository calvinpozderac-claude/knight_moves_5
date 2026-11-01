# subagent "opening": visit budget of the 8,123 climb/merge, and re-routing it

Result: **no tour above 8,123.** Best tour found from scratch after the record's move 44: 7,989
(`best.txt`, NRPA level 3, checker ok=True maxvisits=3 unnecessary=[]).

## 1. Visit-budget analysis of the 8,123 tour (`analyse.py`, `pairs.py`, `board.py`)

```
python3 analyse.py ../../data/tours/record_8123.txt          # move by move: T, knight altitude, group sizes
python3 pairs.py   ../../data/tours/record_8123.txt 44       # per opposite pair: visits before move 44, sits, zero hops
python3 board.py   ../../data/tours/record_8123.txt 44       # board at move 44 (U/L = upper/lower group, else alt diff; digit = visits)
```
- Moves 1-44 (climb + merge): 15 sits, 290 minutes. Dance (moves 45-135): 91 moves = **81 sits + 10 zero-wait hops**,
  7,833 minutes, mean 96.7 min/sit (136 at the start, 68 at the end). Dance start at move 44 (knight on h3, groups 31+26).
- Bound at move 44: 31 usable pairs x 3 = 93 sits. Loss = sum over pairs of min(v[p], v[63-p]):
  - **6 sits lost in the climb**: d1/e8, e1/d8, d2/e7 (the raise trick: sit on the partner, then walk over the top
    point; inherent), plus c2/f7, e2/d7, c4/f5 (walking from the a1 corner to b6: c2, e2, c4 are visited, and their
    partners f7, d7, f5 are on the climb). Bound = 87.
  - **6 sits lost in the dance**: e1/d8 2 (e1 has 2 spare visits, used once as a zero hop), d1 1, b2 1, c3/f6 1,
    b4/g5 1. All 10 zero-wait dance hops start from points whose partner does the sitting (a1, c1, d3, d4, g4 ...)
    except e1 and f6, so the hops are already almost free.
  - The top-left corner (a7, a8, b8, c8, c6, b7) is never visited: the bound counts those pairs through their partners.
- 135 of 186 visits used.

## 2. Re-routing the climb with the sits fixed (`reroute.cpp`)
Keeps every sitting move (point, sink) of moves 1..M in order. Every zero-wait stretch and every landing is free;
beam search segment by segment, the final altitudes must equal the record's at move M.
```
g++ -O2 -std=c++17 -o tmp/reroute reroute.cpp
./tmp/reroute ../../data/tours/record_8123.txt 44 1 30000 10 tmp/rr44           # cost = pair loss at move 44
MODE1=1 ./tmp/reroute ../../data/tours/record_8123.txt 44 2 30000 10 tmp/m1    # cost = visits on record-dance sitters, record dance kept legal
MODE2=1 ./tmp/reroute ../../data/tours/record_8123.txt 134 2 20000 300 tmp/full/r   # re-route the whole tour
```
- Pair-loss objective: best re-route has loss 5 (bound 88, frees c4/f5) but walks over g1 h1 h2 f1 f3, the record
  dance's best sitters (their partners are in the unused corner). NRPA level 3 after it: 7,362 / 7,034 / 6,954 / 7,316
  vs 7,518 / 7,783 / 7,989 / 7,688 / 6,980 after the record's own 44 moves (3-4 min each set). **The bound is
  miscalibrated: a visit on a good sitter costs a sit even when its partner is free, because the partner is not
  reachable in a good dance.**
- Keeping the record dance legal (MODE1): no re-route frees any sitter visit (cost 6 = record). c2, e2, c4, b4 are
  forced by the altitude map. MODE2 (whole tour): same cost as the record (108). **The 8,123 zero-wait routing is
  already optimal for its sit sequence.**

## 3. Adding sits to the record (`insert.cpp`, `splice_nrpa.cpp`)
- `insert.cpp`: at every move, insert every closed loop of 2-5 pinned moves that returns to the same point, then
  replay the rest of the record exactly. Nothing is legal (`./tmp/insert TOUR 5 40`, 2M loops, 0.15 s). Spare
  visits (d1, e1, b2) are never in opposite groups when adjacent (d1 and e1 are both in the upper group).
  Also nothing on the 300 re-routed tours.
- `splice_nrpa.cpp` (copy of src/nrpa.cpp, `--objective splice --splice-tour FILE`): a rollout scores the best total
  it gets by re-joining the record (same knight point and same altitudes relative to the knight as after record
  move j, then the record's remaining moves replayed exactly). With fresh weights from random anchors it searches for
  windows that re-join the record with more minutes. 130 level-2 restarts from anchors 30-130 (8 min) and a run from
  anchors 0-44 (7 min, see below) all re-join at exactly 8,123.
```
g++ -O3 -march=native -std=c++17 -pthread -o tmp/splice_nrpa splice_nrpa.cpp
./tmp/splice_nrpa --objective splice --splice-tour ../../data/tours/record_8123.txt --seed-tours ../../data/tours/record_8123.txt \
   --anchor-min 30 --anchor-max 130 --seed-adapt 0 --no-warm --level 2 --threads 1 --time 480 --pool tmp/spl_pool.txt --pool-min 8124 --out tmp/spl_l2.txt
```


## 4. Round 2 (after the coordinator's review)

### 4a. What 8,130 (subagents/lns) changed relative to 8,123
`python3 analyse.py ../lns/NEW_RECORD_8130.txt`, `python3 pairs.py ../lns/NEW_RECORD_8130.txt 60`.
- Moves 1-15 identical. Moves 16-27 insert zero hops (d5 f5, h4 h6 h4 h6): same altitudes, T=37 at d8.
- Moves 28-34 identical to 8,123's 24-30 (T=70 after f7 -> d7).
- **Merge, 7 sits in both:** 8,123 sits c7 8, d7 6, e7 52, d7 12, h5 63, h6 19, h5 60 = 220 min (moves 31-42).
  8,130 sits c7 8, e7 52, c7 27, b8 6, d7 30, b8 24, h6 80 = 227 min (moves 35-59). It uses the top-left corner that
  8,123 never touches (zero hops a7 a8 a7 a8 c8 b8, sits on c7 and b8). The partners of those points (f2, g1) are
  still below the groups, so they rise into the dance and are not lost ("partner below"). One merge sit on h6
  (80 at n=20) replaces 8,123's three sits around h5/h6 (63+19+60 at n=7-21).
- **Dance entry is the same state:** groups 31+26, h8 2 below the knight. T=297 against 290. 8,130 enters at g2
  (move 61) instead of h3 (move 44). The dance minutes are identical (7,833). The 8,130 dance is the 8,123 dance
  with g and h swapped in the first 18 sits (g2 g1 ... h1 h2 ... h3 g3, the same minutes sequence). One change near
  the end: the d1 sit of 80 is split into 60 + 20 around a zero hop to b1. That gives the same minutes and uses
  d1's spare visit and b1.
- Per point (8,123 -> 8,130): b8 sits 0->2, c7 1->2, d7 2->1, h5 1->0, h6 2->1. Visits rise on non-sitters only
  (a7 0->2, a8 0->2, c8 0->3, d5 1->3, d8 1->3, d6, f5, b7, c3, d3, b1). **The sitter-visit ledger before the dance is
  unchanged (d1, c2, d2, e2, b4, c4).** So the +7 is minutes in the merge at an identical dance entry. No sits
  were freed.
- Same re-route on 8,130 (`MODE1=1 ./tmp/reroute ../lns/NEW_RECORD_8130.txt 61 2 30000 5 tmp/MODE1_8130`): cost 6,
  equal to its own. It gives a legal 148-move version of 8,130 (`tmp/MODE1_NEW_RECORD_8130_0.txt`, checker ok,
  8,130) that frees the non-sitter corner visits.

### 4b. Bottom climb (moves 1-12): exhaustive, no cheaper route exists
`bottom.cpp`: exhaustive DFS from a1 over **every** pinned move (any sit at any point, any sink, any zero hop) with
T <= 37 and at most L moves, that reaches exactly 8,123's altitudes and knight point (b4) after move 12, and from
which 8,123's moves 13-44 still replay legally. The cost is the visits on points that sit later in 8,123
(the record costs 5: d1, c2, d2, e2, c4).
```
g++ -O2 -std=c++17 -o tmp/bottom bottom.cpp
./tmp/bottom ../../data/tours/record_8123.txt 12 14      # L=12: 62M nodes; L=14: 207M nodes, about 3 min
```
- L <= 12 and L <= 14: **no route cheaper than the record.** The cheapest is the record's own route (at L=14 with one
  extra a1-b1 detour). L=16 was stopped for lack of CPU. So c2, e2 and c4 cannot be freed while keeping the same
  state at move 12. Freeing them needs a different altitude state after the bottom climb (different raise sinks), so the
  top climb must change too. That is a whole new climb, not a re-route.
- The sit-order permutations of the three raise sits (`PERM=...` in reroute) are infeasible or cost more (021: 7).
- Sitter-weighted cost without keeping the record dance legal (`MODE3`): cost 6 for 8,123 (move 44) and for
  8,130 (move 61), equal to the records.

## Conclusions / recommendations
1. The 8,123 tour is locally tight: zero-wait routing is optimal for its sits, no short loop can be added, and windows
   that re-join it give no gain. Beating it needs a different sit sequence in the climb/merge **and** a dance as
   good as nnastar's. NRPA level 3 alone falls 130-1,100 short (best 7,989 of 5 runs from the record's move 44).
2. A calibrated dance estimate must charge visits on the dance's natural sitters (bottom-right half: rows 1-5),
   not only opposite-pair conflicts. The pair bound (87 from move 44) overestimates the reachable 81 sits by 6, and
   ranks prefixes wrongly (rr44 bound 88 did worse than the record's 87).
3. Most promising: re-search the dance from move 44 of 8,123 with level 4 (or nnastar from that anchor); 6 sits are
   lost inside the dance (e1/d8, d1, b2, c3/f6, b4/g5), worth roughly 6 x 70 = 400 minutes if a dance can use them.
   A CP-SAT/ILP dance planner on the "domino ping-pong + knight-hop transfer" structure (every dance is pairs of
   adjacent points in opposite groups, alternated 3 times, joined by one hop) could give an exact dance bound.
4. Climb alternatives worth testing with splice-NRPA plus a level-3 reality check: change the order of the three raise
   sits (d1, d2, e1), so that the walk from the a1 corner avoids c2/e2/c4 (frees up to 3 bound sits).
