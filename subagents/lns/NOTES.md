# lns: large-neighbourhood search / simulated annealing over whole tours

**Round 1 results: 8,130 (run1), then 8,211 (run3). Round 2: 8,241 (one free wait, which the checker accepts). See the Round 2 section below.** Verified with
`python3 tools/check_tour.py "$(sed -n 2p subagents/lns/NEW_RECORD_8130.txt)"`, which prints
`moves=156 ok=True T=8130 maxvisits=3 unnecessary=[]`. The same tour is in `best.txt`.
The review also found 0 in-group ("free") sinks and no fractions, so it uses only standard pinned moves.

## Method (`lns.cpp`, built on `../../src/eng.h`)
- **Representation.** A tour is a list of `(target square, dz)`, where dz = alt[q] - alt[p] right after the jump.
  The wait is implied: delta = dz - (alt[q] - alt[p]) must be >= 0. When the sitting square and the target are
  in the same group, only dz = 0 knight moves are allowed. Any edited sequence can therefore be replayed and
  scored exactly; the waits are derived again, so changes earlier in the tour carry through.
- **Repair (`Search`).**
  - `win_dfs`: from the state before move i, a DFS over up to D pinned moves.
  - At every DFS node on the same square as the square before move j of a source tour Y (j in a window),
    `replay(Y[j..])` runs, deriving the waits again ("re-sync").
  - If that replay breaks at move k, `patch` runs a DFS of depth PD (1-3) that tries to re-sync at
    Y[k..k+R]. This is recursive up to 2-4 levels, with a node budget (`--budget`).
  - Every state the search passes is checked for a finishing jump into h8. The best candidate that differs
    from the current tour is returned. The replay memo is a 64-bit hash, so the search is not exhaustive:
    a collision only prunes.
- **Operators** (op ids as in the log):
  - 0: replace a window. DFS depth 1-5, re-sync at j in [i, i+D+2]. Covers deletions and insertions too.
  - 1: skip ahead. DFS depth 1-3, re-sync at j in [i+1, i+12].
  - 2: detour or repeat. DFS depth 2-6, re-sync at j in [i-4, i+4].
  - 3: jump anywhere in the tour itself. DFS depth 1-2, any j.
  - 4: move a segment (or-opt, length up to 24).
  - 5: reverse a segment. This is mostly invalid; in v2, 70% of its share goes to ops 1 and 2.
  - 6: duplicate a dance block.
  - 7 (v2): two-point crossover, cur[0..a) + Z[c..d) + cur[b..], where Z is another record or elite tour.
  - 8 (v2): a random walk of 1-6 moves, then DFS 1-3 and re-sync.
  - Ops 4-7 build a modified Y and replay it with patching at the seams.
- **Acceptance.** Simulated annealing on minutes: accept if dT >= 0, otherwise with probability exp(dT/temp).
  There are 4 geometric cycles from `--t0 40` to `--t1 1`. An elite pool keeps 40 tours, and the search
  restarts from it when the current tour is more than 400 below the best, or when it stalls.
- Single thread, `nice -n 5`, about 100-140 iterations/s on the shared machine.

## Runs and numbers
| run | binary | seeds / start | schedule | best | first improvement |
|---|---|---|---|---|---|
| run1 | `lns_v1` | 8123, 8002, 7911 (start 8123), seed 7 | wall clock, 1500 s | **8,130** | 8126 at 250 s (op 2, i=41, D=5); 8127 at 367 s (op 2, i=32); 8130 at 575 s (op 1, i=41, D=3) |
| run2 s11 | `lns_v2` | 8130 + 8123, 8002, 7911 + 12 elite 8130 variants, focus 1-60 p=0.7 | 55,000 iters (deterministic) | 8,130 | none |
| run2 s12 | same | same | same | 8,130 | none |
| run2 s13 | same | same | same | 8,130 | none |
| run3 s21 | `lns_v2` | 12 variants of 8123 from `subagents/opening/tmp/full/r_0..11` + records, no 8130 | 55,000 iters | **8,211** | 8126 at it 7,913 (see Round 2 section) |
| test | lns | 8123, 30-60 s | wall clock | 8,123 | none |

Other results:
- One-point crossovers between all pairs of 8123, 8002, 7911 and 7200 (prefix of A, DFS depth 2, re-sync
  anywhere in B) found nothing above the better parent.
- A beam search over the tail with an idealised-dance heuristic (`dance_pot2`, still in `lns.cpp`, run with
  `BEAMTEST=200`) was useless. It reached 3.7k-7.5k from mid-dance states, because the heuristic ignores
  adjacency and the beam cannot reach h8.
- Op 2 and op 1 produced every gain. Ops 0 and 3 make most of the plateau moves (equal-minute rewrites).
  Ops 4-7 rarely get accepted.

**How 8,130 differs from 8,123.** From about move 60 on, the dance is the same as 8,123's (it starts at
g2/g1 with groups 35/25-26). The climb/merge in moves 15-60 is rewritten:
- extra zero-wait shuffles (h4/h6, a7/a8/c8),
- the sits around d7/c7/c8/d8/b8 are reorganised (52 c7, 27 c8, 6 d8, 30 d8, 24 c8),
- the dance is entered through h6 -> h5 (80).

The tour is 156 jumps long (8,123 has 135). So the headroom sits in the climb/merge, not in the dance.

## Reproduce
```
cd subagents/lns
g++ -O3 -march=native -std=c++17 -o lns_v1 lns_v1.cpp   # exact source of the binary that found 8,130
nice -n 5 ./lns_v1 --tours ../../data/tours/record_8123.txt ../../data/tours/record_8002.txt \
   ../../data/tours/record_7911.txt --time 1500 --xover 0 --seed 7 --t0 40 --budget 400000 --out tmp/run1_best.txt
```
- `lns_v1.cpp` was reconstructed afterwards by undoing the later edits. Compiling it gives a binary whose
  `.text` is byte-identical to `lns_v1` (`objdump -d`). The only other difference is the embedded source
  file name.
- v1's annealing temperature depends on wall-clock time, so run1's exact trajectory cannot be repeated on a
  loaded machine.
- `lns_v2.cpp` (identical to the current `lns.cpp`) adds `--iters N`, which makes the schedule depend only
  on the iteration count and the seed. It also adds `--focus LO HI P` (with probability P, the window
  start i is drawn from [LO, HI]), ops 7 and 8, and multi-tour input files (up to 12 tours per file).
- Deterministic 3-seed run: `./run2.sh`. Lineage from 8123 variants: `./run3.sh`.
- `saver.sh` checks every `tmp/run*_best.txt` with the exact checker and updates `best.txt` and
  `NEW_RECORD_*.txt`.
- `dump.py FILE [from]` prints group sizes and visits for each move.

## Recommendation for next round
1. **Put the compute on moves 1-60, with bigger neighbourhoods.** The +7 came from op 1/2 windows at i = 32-41.
   - Allow DFS depth 6-8 there, with a wider re-sync range ([i-6, i+20]).
   - Use a larger patch depth (PD 3-4), so that one change in the merge can re-sync with the dance 10-20 moves
     later.
   - Then, from 8,130, take a *visit-freeing* pass (`tools/reroute_zero_wait.py`), followed by SA. 8,130 has
     many zero-wait shuffles that may hold visits back.
2. **Secondary objective for the plateau.** Most accepted moves have equal minutes. Rank ties by free visits
   on points that are still in the final groups, so the SA drifts toward tours with slack that a later
   insertion can turn into sits.
3. **Run more seeds and lineages from different 8123 variants** (run3 style). The deterministic seeds from
   8130 plateaued, so diversity of the starting tour matters more than more iterations.
4. **Combine with structural search.** Give NRPA or nnastar the 8,130 prefix (the first 61 moves) as an
   anchor, or start from LNS-improved merges and re-search the dance.
