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
