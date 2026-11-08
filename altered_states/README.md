# Altered States: a 5×5 grid worth 127 points

Jane Street puzzle: fill a 5×5 grid with letters. A US state scores its length if it can be
spelled by King's moves: consecutive letters sit on distinct, adjacent squares, including
diagonally adjacent ones, and squares may be revisited. The puzzle's own example spells OHIO
with one O. Each state counts once. HAWAII needs two adjacent I's.

## Answer

**Score: 127**

```
C H D O R
W A T A K
I R U N S
I S O I A
S M C L L
```

Rows reading across: **CHDOR, WATAK, IRUNS, ISOIA, SMCLL**

15 states (paths use chess coordinates, a–e left to right, 1 at the bottom):

| State | Pts | Path |
|---|---|---|
| NORTHCAROLINA | 13 | d3-c2-b3-c4-b5-a5-b4-b3-c2-d1-d2-d3-d4 |
| SOUTHCAROLINA | 13 | b2-c2-c3-c4-b5-a5-b4-b3-c2-d1-d2-d3-d4 |
| NORTHDAKOTA | 11 | d3-c2-b3-c4-b5-c5-d4-e4-d5-c4-b4 |
| SOUTHDAKOTA | 11 | b2-c2-c3-c4-b5-c5-d4-e4-d5-c4-b4 |
| LOUISIANA | 9 | d1-c2-c3-d2-e3-d2-e2-d3-d4 |
| WISCONSIN | 9 | a4-a3-b2-c1-c2-d3-e3-d2-d3 |
| ARKANSAS | 8 | d4-e5-e4-d4-d3-e3-d4-e3 |
| COLORADO | 8 | c1-c2-d1-c2-b3-b4-c5-d5 |
| ILLINOIS | 8 | d2-d1-e1-d2-d3-c2-d2-e3 |
| MISSOURI | 8 | b1-a2-a1-b2-c2-c3-b3-a3 |
| MONTANA | 7 | b1-c2-d3-c4-d4-d3-d4 |
| ALASKA | 6 | e2-d1-e2-e3-e4-d4 |
| HAWAII | 6 | b5-b4-a4-b4-a3-a2 |
| KANSAS | 6 | e4-d4-d3-e3-d4-e3 |
| UTAH | 4 | c3-c4-b4-b5 |
| **Total** | **127** | |

Comments for the submission:

- The grid is built around one large cluster. NORTH and SOUTH share the TH, which leads into
  both CAROLINA and DAKOTA, giving 48 points from four states.
- The two L's in the bottom-right corner exist only for ILLINOIS. The two I's on the left edge
  exist only for HAWAII.
- Every square is needed: changing any single letter drops the score to 121 or less.

Check it: `python3 score.py CHDOR WATAK IRUNS ISOIA SMCLL`

## How it was found

Every method below was run on the same 4-core machine. Scores are the best raw score found.
Unless noted, "×2" means two independent random seeds.

### 0. Scoring (`score.py`, `as.cpp`)

- **Python scorer (`score.py`).** It does a layer-by-layer reachability check per state: the
  cells where letter *i* can sit are the King-neighbours of the cells for letter *i−1* that
  hold the right letter. It prints a witness path for every state. On the puzzle's 3×3
  example it gives 13 (IDAHO, IOWA, OHIO), which matches the puzzle.
- **C++ scorer (`as.cpp`).** It does the same check with 25-bit masks: neighbourhoods come
  from shifts, and an exact per-cell expansion is used for doubled letters such as SS, LL and
  II, because a square isn't its own neighbour. Single-cell changes are re-scored
  incrementally: only the states that contain an affected letter are recomputed. Speed is
  about 10M grid evaluations per second on 4 threads.
- **Cross-check (`crosscheck.py`).** The two scorers agreed on 400/400 mutated grids.

### 1. Local search and annealing (`as.cpp`, `bench.sh`)

Moves: change one cell to a random letter (drawn by how often letters occur in state names, or
uniformly), or swap two cells (30%). Q is never used because no state contains it.

The "smoothed" variants also give each absent state partial credit of λ·f²/L, where f is the
longest piece of the state that can be spelled (λ = 0.5). Only the true score is reported.

Tuning: 30 s per run on 4 threads (`results/tuning/`).

| SA run length | T 3→0.3 | T 2→0.2 | T 5→0.5 |
|---|---|---|---|
| 2M moves | 123 | 120 | 122 |
| 20M moves | 127 | 127 | 127 |
| 100M moves (did not finish in 30 s) | 127 | 127 | 127 |

Short runs freeze too early, and 20M-move runs reached 127 under every temperature schedule
tried.

Benchmark: 120 s per run, 1 thread at low priority, while two agents used the other cores
(`results/bench/summary.txt`).

| Method | Seed 1 | Seed 2 | Notes |
|---|---|---|---|
| Hill climbing with random restarts (best of all 1-cell changes and swaps) | 91 | 88 | about 84k climbs per run; local optima are poor |
| Hill climbing on the smoothed score | 111 | 117 | partial credit helps a lot |
| Simulated annealing (20M moves, T 3→0.3) | 123 | **127** | |
| Simulated annealing, smoothed | **127** | **127** | 3.5× slower per move, still reached 127 both times |
| Parallel tempering (16 replicas, T 0.3–6) | **127** | **127** | swap acceptance about 0.53 |
| Parallel tempering, smoothed | **127** | **127** | |
| Iterated local search (kick 1–5 cells, climb, accept if not worse) | 105 | 105 | gets stuck in a deep basin; worst of the restart-based methods |

### 2. Search moves that know the state names (`lns/lns.cpp`, 2 threads, 90 s per run)

| Method | Best |
|---|---|
| State insertion: pick an absent state, write in its cheapest King's walk (the one that changes the fewest cells), repair with a short SA, use late acceptance | **127**, **127** |
| Same, started from the 127 grid | 127 (no escape) |
| Same, seeded with NORTH/SOUTH × CAROLINA/DAKOTA | **127** |
| Same, seeded with WEST VIRGINIA/VIRGINIA/ARKANSAS/KANSAS | 122 |
| Tabu search over the full set of 1-cell changes and swaps (tenure 12–24) | 114, 115 |
| Late-acceptance hill climbing | 106, 51 |
| Genetic algorithm: population 24, block crossover trying all 8 rotations/reflections, SA polish | 114, 120 (the population converged) |

`results/lns/overlap.txt` lists the longest pieces that state names share. The top ones are
THCAROLINA (10 letters), VIRGINIA (8), THDAKOTA (8) and KANSAS (6). The 127 grid uses three of
these four.

### 3. Exact constraint programming (`cpsat/`, OR-Tools CP-SAT, 2 workers)

The model:

- x[cell][letter] says which letter each cell holds.
- y[state][position][cell] means "this letter of the state can sit here". It requires the cell
  to hold that letter and, after the first letter, a King-neighbour holding the previous one.
- present[state] requires some cell for the state's last letter.
- The objective maximises the total length of the states present.

Redundant letter-count constraints and a corner symmetry break were added. The model has about
11k variables.

| Experiment | Time | Best | Bound |
|---|---|---|---|
| Whole model, hinted with the 127 grid | 480 s | 127 (the hint) | 412, the useless total of all 50 lengths |
| Whole model, no hint | 300 s | 92 | 412 |
| Re-solve windows of the 127 grid exactly with the rest fixed: 3×3/3×4 blocks, 10–12 random cells, path cells | 600 s | 127 | 4 of 14 windows proved optimal; no contiguous block finished in 60 s |
| Can the 15 states plus one more fit? Tried for each of the 35 missing states | 12 s each | — | all 35 came back UNKNOWN |

CP-SAT is far weaker than annealing here, and the LP bound is too loose to prove anything.

### 4. Checking the 127 grid (`as.cpp --mode kopt`, `results/kopt/`)

Every grid that differs from the 127 grid in at most k cells was scored exactly:

| k | Grids checked | Better | Ties | Best other score |
|---|---|---|---|---|
| 1 | 600 | 0 | 0 | 121 |
| 2 | 172,800 | 0 | 0 | 121 |
| 3 | 31,795,200 | 0 | 0 | 121 |
| 4 | 4,196,966,400 | 0 | 0 | 121 |

So the 127 grid is a strict local optimum: no change of up to 4 cells matches it (4.2 billion grids, 431 s on 2 threads).

### 5. Search without the CAROLINA/DAKOTA states (`results/alt/`)

To check whether the four-Carolina/Dakota cluster is a trap, SA was run with those four states
removed from the list (90 s, 2 threads). The best grid then scores **114** against the full
list. The cluster is worth 48 points, and the best grids without it do about 13 points worse.

## Round 2: three more agents, plus exact fills around the cluster

These runs were aimed at escaping the 127 basin. Every run used 1 thread. Logs are in
`results/explore_a/` to `results/explore_d/`.

### Agent A: change what the search rewards (`explore_a/wsa.cpp`)

| Experiment | Best true score |
|---|---|
| Guided local search: weights of absent states ×1.25 after each annealing cycle, capped at 4 (300 s) | 127 (same grid) |
| Same with ×1.5, cap 6, restarts (300 s) | 117 |
| Normalised weights (mean 1, range 0.3–3) (400 s) | 121 |
| Both CAROLINAs given weight 0 / both DAKOTAs given weight 0 (150 s each) | 119 / 121 |
| All four Carolina/Dakota states given weight −0.5 (150 s) | 114 |
| LOUISIANA, WISCONSIN, ILLINOIS, MISSOURI given weight 0 (150 s) | 123 |
| 177 short runs, each dropping 15% of states and doubling 15% (450 s) | 127 (same grid) |

About 100 distinct sets of states scored 118 or more. Every one of them contains NORTH/SOUTH ×
CAROLINA/DAKOTA, and all but one also contain KANSAS and ARKANSAS. Without the 127 grid, the
best sets of states score 123:

- `CHTUNDAROIMWKLSOSAILIHNDI`
- `LIDAWANIOHSKRLIOTAOFUDHCN`

### Agent B: build a compact core, then fill the rest (`explore_b/core.cpp`)

For 17 cores, about 30 layouts each were made with as many empty squares as possible. The empty
squares were filled by annealing, then the whole grid was polished. The best result for each
core:

| Core | Best |
|---|---|
| CD + CONNECTICUT, and the NEW-states | 127 (same grid) |
| CD + NEW HAMPSHIRE | 123 |
| CD + RHODE ISLAND / PENNSYLVANIA / WEST VIRGINIA, CALIFORNIA + CAROLINAs | 121 |
| A-heavy states | 120 |
| MASSACHUSETTS, MISSISSIPPI, MISSOURI, MINNESOTA, TENNESSEE | 118 |
| VIRGINIA / WEST VIRGINIA / PENNSYLVANIA | 112 |
| KANSAS group | 111 |

80 runs of annealing with random weights (600 s) also gave 127, the same grid.

### Agent C: search over sets of states (`explore_c/xc.cpp`)

A fast checker answers "can one grid hold all of these states?". It rebuilds the 127 set from a
random grid in 1.6 s.

- **Set search from the 127 set:** about 760 nearby sets were proposed (add 1–3 states, drop 0–2,
  even accepting sets up to 8 points worse). The checker found a grid for none of them.
- **Exclusion sweep:** each of the 15 states was dropped in turn. The best result without one of
  them is 123.
- **Long annealing:** 300M moves with richer moves (2×2 rotations, row/column shifts, copying a
  neighbour, writing a 2–3 letter piece of a state along a walk) gave 127.

### Parent: exact fills around the Carolina/Dakota cluster (`explore_d/`)

1. **The 127 grid's own layout.** The 15 squares that the four CD states use in the 127 grid were
   fixed, and CP-SAT optimised the other 10 squares exactly against all 50 states. It proved
   127 optimal in 0.9 s, so no refill of that layout does better.
2. **Other layouts.** `compact.cpp` searches for minimal layouts of the four CD states. 14 squares
   is the smallest it found; the 127 grid's layout needs 15. Each layout was then filled exactly
   with CP-SAT (`fill_layouts.py`). Fill quality falls with core size:

   | Squares used by the CD layout | Mean exact fill |
   |---|---|
   | 14 | 103 |
   | 15 | 98 |
   | 16 | 92 |
   | 18 | 83 |

3. **Sweeps.**
   - First sweep: 913 layouts. 815 fills were proved optimal and 98 hit the 30 s limit. Best:
     **118**.
   - Second sweep: 456 of 2,023 further layouts finished before the 30-minute job limit. 266 were
     proved optimal. Best: **117**.
4. **Coverage.** The layout space is not saturated. Three 5-minute generator runs found 2,290
   distinct layouts with at most 15 squares and barely overlapped. So this is a large sample, not
   a complete list.
5. **Whole grid with CD forced.** A whole-grid CP-SAT run with the four CD states required stayed
   at 127 for 15 min, with the bound stuck at 412.
6. **Long annealing.** Three single-thread 400M-move SA runs all gave 127. A 15-minute single-thread parallel-tempering run (24 replicas, T 0.25–8)
   also gave 127.

## Round 3: exact re-optimisation around kept states (`explore_e/xlns.py`)

Each step works as follows:

1. Keep a random subset of the states currently present.
2. Fix only a minimal set of squares that spells them, padded or trimmed with random squares so
   that 7–13 squares are free (10–15 in the "big" run).
3. Let CP-SAT re-optimise every free square exactly against all 50 states (cap of 8–15 s).

The current grid is always a valid fill, so a step never loses points. Equal-scoring new grids are
accepted, and 2–4 random squares are kicked after 25 steps without progress. Unlike the earlier
square-window re-solves, the Carolina/Dakota squares themselves get freed and rebuilt. Each walk
ran 25 min on 1 thread, at about 4 s per step.

| Start | Steps | Distinct grids | Best |
|---|---|---|---|
| 127 grid, 10–15 squares freed | 115 | 9 | 127 (no other grid at 127 found) |
| 123 `CHTUNDAROIMWKLSOSAILIHNDI` | 390 | 23 | 123 |
| 123 `LIDAWANIOHSKRLIOTAOFUDHCN` | 358 | 40 | 123 (start), walk reached 114 after kicks |
| 118 (best compact-layout fill) | 389 | 13 | 118 (start), walk reached 114 after kicks |

A first version spun without solving when no layout fitted the free-square range. Its logs are
kept in `results/explore_e/v1_spinning/` and are not counted above.

Every exact neighbourhood of the 127 grid that was tried
returned 127 or less.

## Conclusion

**127 is the best score found.** The same grid, up to rotation and reflection, was reached by:

- annealing in its raw and smoothed forms
- parallel tempering in its raw and smoothed forms
- state-insertion search
- guided local search
- random reweighting
- the core-plus-fill design
- long rich-move annealing

What is proved:

- No change of up to 4 squares improves or ties the grid.
- No refill of the squares around its own Carolina/Dakota layout beats it.
- Nothing above 127 turned up in the roughly 1,000 other layouts whose fills were proved optimal.
  This is a sample: the layout space is not exhausted, and about 200 fills hit the time limit.

The best grids that differ from it score 123. A grid above 127, if it exists, needs a placement of
the cluster that was not sampled, or a state set none of these methods reached. The CP-SAT bound
never moved off 412, so optimality is not proved.

## Files

| File | What it is |
|---|---|
| `states.txt` | the 50 state names, without spaces |
| `score.py` | independent exact scorer; prints paths |
| `as.cpp` | C++ search engine: `hc`, `sa`, `pt`, `ils`, `kopt`, `score` (`--states` lets you use a different state list) |
| `crosscheck.py` | checks that the C++ and Python scorers agree |
| `bench.sh` | runs the equal-budget method benchmark |
| `lns/lns.cpp`, `lns/overlap.py` | state-insertion search, tabu, late acceptance, genetic algorithm; shared-substring analysis |
| `cpsat/*.py` | CP-SAT model, whole-model solve, window re-solve, plus-one feasibility |
| `explore_a/`, `explore_b/`, `explore_c/` | round-2 agents: reweighting, core-plus-fill, state-set search |
| `explore_d/` | compact Carolina/Dakota layout generator and exact CP-SAT fills |
| `explore_e/xlns.py` | exact re-optimisation around random subsets of present states |
| `results/` | logs of every run quoted above |

To build: `g++ -O3 -march=native -std=c++17 -pthread -o build/as as.cpp`

Example run: `./build/as --mode sa --time 60 --threads 4 --iters 20000000`

The puzzle page could not be fetched from the sandbox (DNS blocked), so the rules used are the
ones quoted in the task.
