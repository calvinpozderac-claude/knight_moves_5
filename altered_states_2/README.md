# Altered States 2

Jane Street puzzle: place letters in a 5x5 grid to score as many points as possible.
A U.S. state scores its 2020 census population (once) if its name can be spelled by king's moves
from square to square, with at most one letter "altered" (replaced by another letter).

## Answer

```
H M L F S
O A I O U
T S N R C
R E G A H
O Y W I N
```

**Entry: `hmlfsoaioutsnrcregahoywin`**, worth **265,326,530 points**: 32 states, 80.22% of the
330,759,736 available. That is 100M above the 165,379,868 needed to qualify.

Citations earned: **20S** (32 states), **200M**, **C2C** (for example Virginia, West Virginia, Ohio,
Indiana, Illinois, Missouri, Kansas, Colorado, Utah, Arizona, California). It misses **PA**, **M8**,
**4C** (no New Mexico) and **NOCAL**. The sections at the end give the best boards for those.

How each state is spelled (output of `python3 score.py hmlfsoaioutsnrcregahoywin --paths`, rows and
columns counted from the top left):

| State | Points | Spelled as | Squares |
|---|---:|---|---|
| California | 39,538,223 | HALIFORNIA | r1c1 r2c2 r1c3 r2c3 r1c4 r2c4 r3c4 r3c3 r2c3 r2c2 |
| Texas | 29,145,505 | TETAS | r3c1 r4c2 r3c1 r2c2 r3c2 |
| Florida | 21,538,187 | FLORIMA | r1c4 r1c3 r2c4 r3c4 r2c3 r1c2 r2c2 |
| New York | 20,201,249 | NEWYORT | r3c3 r4c2 r5c3 r5c2 r5c1 r4c1 r3c1 |
| Illinois | 12,812,508 | IMLINOIS | r2c3 r1c2 r1c3 r2c3 r3c3 r2c4 r2c3 r3c2 |
| Ohio | 11,799,448 | OHMO | r2c1 r1c1 r1c2 r2c1 |
| Georgia | 10,711,908 | GNORGIA | r4c3 r3c3 r2c4 r3c4 r4c3 r5c4 r4c4 |
| North Carolina | 10,439,388 | NORCHCAROLINA | r3c3 r2c4 r3c4 r3c5 r4c5 r3c5 r4c4 r3c4 r2c4 r1c3 r2c3 r3c3 r2c2 |
| Michigan | 10,077,331 | MIRHIGAN | r1c2 r2c3 r3c4 r4c5 r5c4 r4c3 r4c4 r3c3 |
| New Jersey | 9,288,994 | NEWGERSEY | r3c3 r4c2 r5c3 r4c3 r4c2 r4c1 r3c2 r4c2 r5c2 |
| Virginia | 8,631,393 | MIRGINIA | r1c2 r2c3 r3c4 r4c3 r5c4 r5c5 r5c4 r4c4 |
| Arizona | 7,151,502 | ARILONA | r4c4 r3c4 r2c3 r1c3 r2c4 r3c3 r2c2 |
| Indiana | 6,785,528 | INAIANA | r2c3 r3c3 r2c2 r2c3 r2c2 r3c3 r2c2 |
| Missouri | 6,154,913 | MIFSOURI | r1c2 r2c3 r1c4 r1c5 r2c4 r2c5 r3c4 r2c3 |
| Wisconsin | 5,893,718 | WIACONSIN | r5c3 r5c4 r4c4 r3c5 r2c4 r3c3 r3c2 r2c3 r3c3 |
| Colorado | 5,773,714 | COLORANO | r3c5 r2c4 r1c3 r2c4 r3c4 r4c4 r3c3 r2c4 |
| Minnesota | 5,706,494 | MIANESOTA | r1c2 r2c3 r2c2 r3c3 r4c2 r3c2 r2c1 r3c1 r2c2 |
| South Carolina | 5,118,425 | SOURHCAROLINA | r1c5 r2c4 r2c5 r3c4 r4c5 r3c5 r4c4 r3c4 r2c4 r1c3 r2c3 r3c3 r2c2 |
| Alabama | 5,024,279 | ALAHAMA | r2c2 r1c3 r2c2 r1c1 r2c2 r1c2 r2c2 |
| Louisiana | 4,657,757 | LOLISIANA | r1c3 r2c4 r1c3 r2c3 r3c2 r2c3 r2c2 r3c3 r2c2 |
| Oregon | 4,237,256 | ORENON | r5c1 r4c1 r4c2 r3c3 r2c4 r3c3 |
| Oklahoma | 3,959,353 | OMLAHOMA | r2c1 r1c2 r1c3 r2c2 r1c1 r2c1 r1c2 r2c2 |
| Utah | 3,271,616 | OTAH | r2c1 r3c1 r2c2 r1c1 |
| Iowa | 3,190,369 | IOLA | r2c3 r2c4 r1c3 r2c2 |
| Arkansas | 3,011,524 | ARIANSAS | r4c4 r3c4 r2c3 r2c2 r3c3 r3c2 r2c2 r3c2 |
| Kansas | 2,937,880 | HANSAS | r1c1 r2c2 r3c3 r3c2 r2c2 r3c2 |
| Idaho | 1,839,106 | IMAHO | r2c3 r1c2 r2c2 r1c1 r2c1 |
| West Virginia | 1,793,716 | WESTAIRGINIA | r5c3 r4c2 r3c2 r3c1 r2c2 r2c3 r3c4 r4c3 r5c4 r5c5 r5c4 r4c4 |
| Hawaii | 1,455,271 | HAWANI | r4c5 r4c4 r5c3 r4c4 r3c3 r2c3 |
| Maine | 1,362,359 | HAINE | r1c1 r2c2 r2c3 r3c3 r4c2 |
| Montana | 1,084,225 | MOATANA | r1c2 r2c1 r2c2 r3c1 r2c2 r3c3 r2c2 |
| Alaska | 733,391 | ALASOA | r2c2 r1c3 r2c2 r3c2 r2c1 r2c2 |

Missing (65.4M in all): Pennsylvania 13.0M, Washington 7.7M, Massachusetts 7.0M, Tennessee 6.9M,
Maryland 6.2M, Kentucky 4.5M, Connecticut 3.6M, Nevada 3.1M, Mississippi 3.0M, New Mexico 2.1M,
Nebraska 2.0M, New Hampshire 1.4M, Rhode Island 1.1M, Delaware 1.0M, South Dakota 0.9M,
North Dakota 0.8M, Vermont 0.6M, Wyoming 0.6M.

Is it the best possible board? There is strong evidence, but not a proof:

- **It is a local optimum.** No change of up to 4 squares improves it, and neither does any
  5-square change inside a 4x4 box (proved exhaustively, section 8).
- **Nothing scored higher in any run.** The runs logged 4,767 restarts in total, about 2.3 hours
  on 4 cores.
- **The search keeps converging on it.** Starting from random boards, it was found 28 times
  (always as one of its 8 rotations and reflections), by 8 different runs with different seeds,
  schedules and methods. Annealing and parallel tempering both found it. Restarts that began from
  a perturbed copy of it found it 36 more times and found nothing better.

## Files

| File | What it is |
|---|---|
| `data/states.csv` | 50 states, 2020 census population, postal code, coast (E = Atlantic, W = Pacific) |
| `data/borders.csv` | pairs of states with a positive-length border (Four Corners point contacts excluded) |
| `src/as2.cpp` | the solver: fast scorer, simulated annealing, parallel tempering, exhaustive polish |
| `score.py` | independent checker in Python (any n x n): score, states, tracings, citations |
| `sweep.py` | runs one solver setting, logs every restart, appends a summary row |
| `analyze.py` | pools all logs, merges the 8 symmetric copies of each board, lists the top boards |
| `results/sweep_summary.csv` | one row per experiment (the tables below) |
| `results/logs/*.csv` | every restart of every experiment: grid, score, number of states, objective, citations |
| `results/best.txt` | the answer and the best boards for each citation |

## Rules as implemented

- A path visits a sequence of squares, each a king's move (one of the 8 neighbours) from the
  previous one. Squares may be reused, but a step must go to a different square. The example
  needs both: INLINOIS goes I-N-L-I-N-O-I-S over the same I three times.
- A state counts if some path spells its name (letters only: `NEWYORK`, `NORTHCAROLINA`, ...)
  with at most one position where the grid letter differs from the name.
- Each state scores its population once, however many times it appears.

**Check against the puzzle.** `python3 score.py thoainesl` gives 32,913,047 for the example:
Illinois (INLINOIS), Ohio (THIO), Utah (HTAH), Iowa (IOHA) and Idaho (ITAHO). That is the puzzle's
number and its five states.

**Population data.** Wikipedia and janestreet.com could not be reached from this machine, so
`data/states.csv` was written from the 2020 census apportionment table and checked against the
puzzle in two ways. The 50 populations add to 330,759,736, exactly twice the puzzle's 165,379,868
threshold. The five example states add to the puzzle's 32,913,047. California is 39,538,223, as
the puzzle says.

## Scoring engine

The board is stored as 26 bitmasks of 25 bits, one per letter, marking the squares that hold it.
For a name s0 s1 ... sL-1 the scorer keeps two sets of squares:

- `d0`: squares where the first i letters can end with no alteration
- `d1`: squares where the first i letters can end with at most one alteration

They are updated with

```
d0 <- N(d0) & M[s_i]
d1 <- (N(d1) & M[s_i]) | N(d0)
```

Here `N(x)`, the union of king-neighbourhoods, is five shifts and masks. The state is on the board
iff `d1` is non-empty at the end. A full board (50 states) costs about 0.7 microseconds, so one
core evaluates about 1.4 million boards a second.

`score.py` is a separate implementation: recursive search with memoisation over (letter, square,
alterations used). It also returns an explicit path for each state. The two agree on every board
checked: the 9 boards in `results/best.txt`, 300 boards sampled from the logs and 200 random
boards.

## Methods tested and results

All runs were on 4 cores. "Restart" means one annealing run from a random board. Scores are in
millions. Every restart is in `results/logs/<label>.csv`.

### 1. Simulated annealing (the main method)

- **Moves.** Mostly, set one random square to a new letter. Sometimes (probability `--swap`,
  default 0.2), swap the letters of two random squares. New letters are drawn 85% from the letter
  frequencies of the state names weighted by population, and 15% uniformly.
- **Acceptance.** Metropolis on the score, with a geometric temperature schedule from `t0` to `t1`.
  Each restart keeps the best board it visited.

**Temperature schedule** (40 restarts x 5M moves each; about 38 s on 4 cores per row):

| t0 | t1 | mean | median | sd | best |
|---|---|---:|---:|---:|---:|
| 1e6 | 1e4 | 242.82 | 243.37 | 6.23 | 255.108 |
| 1e6 | 1e5 | 245.91 | 246.91 | 7.72 | 259.970 |
| 1e6 | 3e5 | 248.62 | 249.30 | 6.81 | 260.062 |
| 3e6 | 2e4 | 256.46 | 256.19 | 4.02 | 264.695 |
| 3e6 | 1e4 | 256.83 | 256.86 | 3.30 | 262.810 |
| 3e6 | 1e5 | 257.59 | 257.49 | 2.83 | 263.593 |
| 3e6 | 3e5 | 258.44 | 258.79 | 2.95 | 264.410 |
| **3e6** | **1e6** | **259.82** | **259.67** | **2.80** | **265.327** |
| 5e6 | 3e5 | 257.20 | 256.76 | 3.26 | 262.556 |
| 1e7 | 1e4 | 256.34 | 256.17 | 3.76 | 264.143 |
| 1e7 | 1e5 | 256.94 | 256.50 | 3.32 | 264.798 |
| 1e7 | 3e5 | 256.48 | 257.16 | 2.92 | 262.556 |

Starting cold (1e6) gets trapped. Ending warm is better: at 1e6 the walk still moves between
basins, and the restart keeps the best board it saw. Single moves typically change the score by a
few million, so these temperatures are of that order. The 3e6 to 2e4 row is the first baseline run
(`baseline.csv`).

**Restart length at a fixed budget** (t0 = 3e6, t1 = 3e5, 200M moves per row; 240M for the 20M row):

| moves per restart | restarts | mean | sd | best |
|---|---:|---:|---:|---:|
| 1.25M | 160 | 254.36 | 4.67 | 265.327 |
| 5M | 40 | 258.44 | 2.95 | 264.410 |
| 20M | 12 | 260.62 | 1.69 | 264.143 |

Longer restarts give better boards on average. For the single best board, many medium restarts
work as well (see section 5).

**Swap moves** (t0 = 3e6, t1 = 3e5, 40 x 5M):

| P(swap) | mean | sd | best |
|---|---:|---:|---:|
| 0 | 254.82 | 4.13 | 262.688 |
| 0.2 | 258.44 | 2.95 | 264.410 |
| 0.5 | 258.85 | 2.62 | 262.810 |

Swaps help: they move a letter without losing it. Anything from 0.2 to 0.5 works.

**Near-miss bonus.** The objective adds `w` x the population of states that need exactly 2
alterations, to reward boards that are one letter from another state. Same schedule, 40 x 5M:

| w | mean score | sd | best |
|---|---:|---:|---:|
| 0 | 258.44 | 2.95 | 264.410 |
| 0.1 | 258.62 | 3.02 | 264.695 |
| 0.3 | 258.37 | 3.48 | 265.327 |

No measurable effect. The extra pass also costs 55% more time per move, so it is off by default.

### 2. Exhaustive polish

`as2 polish GRID --depth D` tries every change of up to D squares (each to any of 26 letters) and
takes the best improvement, until none is left. Depth 2 is about 200k boards; depth 3 is about
40M boards (24 s).

- The top three boards of the baseline run did not improve at depth 2. Annealing already ends in
  2-square local optima.
- The 265,326,530 board does not improve at depth 3.

So the remaining gap is between basins, not inside one.

### 3. Parallel tempering

`as2 pt` runs 16 copies of the board at temperatures spaced geometrically from `tlo` to `thi`.
Each copy makes 200 Metropolis moves, then neighbouring temperatures try to swap boards; each
thread runs its own ensemble. With 2e5 to 3e6 for 120 s on 4 threads, one thread found
265,326,530 and the others ended at 263.1M, 263.1M and 264.1M. It is no better than annealing
restarts for the same time (section 5).

### 4. Iterated local search

Restart from the best board with a fraction of its squares randomised, then anneal again
(4 minutes each):

| randomised | t0 to t1 | restarts | mean | best | best board refound |
|---|---|---:|---:|---:|---:|
| 30% | 1e6 to 5e5 | 256 | 253.33 | 265.327 | 26 (10%) |
| 60% | 2e6 to 1e6 | 256 | 260.50 | 265.327 | 10 (4%) |

With 30% kicks and a low temperature, restarts often end in poorer basins they cannot climb out
of (mean 253.3M), and refind the best board only 10% of the time. Kicks of 60% behave like fresh
restarts. Neither found anything better.

### 5. Production run and how often each method finds the best board

The production run was 900 s on 4 cores: 243 restarts of 20M moves, t0 = 3e6, t1 = 1e6. Its mean
was 261.94 with sd 1.92, and no restart beat 265,326,530, which it found 14 times (5.8%).

**Same-time comparison.** Each method had 4 minutes on each of 4 cores. For annealing, "one
core's best" is estimated by resampling the run's restarts, with as many restarts as one core
completes in 240 s.

| method | restarts | mean per restart | one core's best in 240 s (mean) | P(one core finds 265,326,530 in 240 s) |
|---|---:|---:|---:|---:|
| **SA, 5M moves, 3e6 to 1e6** | 258 | 259.68 | **265.09** | **0.82** (6 of 258 restarts) |
| SA, 20M moves, 3e6 to 1e6 (production run) | 243 | 261.94 | 264.97 | 0.62 (14 of 243) |
| SA, 5M moves, 3e6 to 2e6 | 265 | 256.72 | 264.22 | 0.00 (0 of 265) |
| PT, 16 replicas, 3e5 to 3e6 | 4 ensembles | n/a | 265.06 (per ensemble) | 2 of 4 ensembles |

Ending at 2e6 is too hot. The walk no longer settles into the deep basin, although this run did
find a new runner-up (264,897,652). Medium annealing restarts are the most efficient way to reach
the top board; parallel tempering is close behind.

### 6. Top boards over all experiments

`python3 analyze.py` pools every restart and merges the 8 symmetric copies of each board:

```
$ python3 analyze.py --top 10 --by-score
 265,326,530  32 states  found  64x  hmlfsoaioutsnrcregahoywin  [20S 200M C2C]  in 10 runs
 264,897,652  28 states  found   2x  cidovlanihtorscyesaigwnmw  [20S 200M]  in 2 runs
 264,798,019  30 states  found   6x  kdanurhigocorstlaneamiwdy  [20S 200M]  in 4 runs
 264,695,111  29 states  found   9x  anwypvioeghlsrntcaniemidv  [20S 200M PA]  in 5 runs
 264,410,348  31 states  found  14x  dasilwinoayergcostairuonm  [20S 200M]  in 5 runs
 264,409,206  28 states  found   3x  kodanrylirmasogcineewhtnp  [20S 200M PA]  in 3 runs
 264,275,018  32 states  found   6x  hcmdugiaotsrnshteoiawyflw  [20S 200M]  in 3 runs
 264,142,662  30 states  found  44x  chtrnoaeoylrsewginsmdaoci  [20S 200M 4C C2C]  in 13 runs
 263,833,497  30 states  found   1x  dasilwinoayergckstairuonm  [20S 200M]  in 1 runs
 263,611,218  31 states  found   3x  coladyericwsnghsaotamilen  [20S 200M]  in 3 runs
```

`--by-score` merges boards that differ only in a square no state uses.

The same few boards top the list in experiment after experiment, and most runner-ups recur too.
They are close in score but leave out different states: 264,695,111 includes Pennsylvania, and
264,142,662 earns 4C.

### 7. Citations

The same annealer was run with `--award` (see Reproduce) to find the best board that earns a given
citation. The table also uses every restart of every other run that happened to earn it
(`python3 analyze.py --by-score --require ...`). The "cost" column is how many points below the
answer each board is.

| Citations required | Best board found | Score | Cost | Citations it earns | Found |
|---|---|---:|---:|---|---:|
| none (the answer) | `hmlfsoaioutsnrcregahoywin` | 265,326,530 | 0 | 20S 200M C2C | 64x |
| 20S, 200M or C2C | the answer already earns them | 265,326,530 | 0 | | |
| PA | `anwypvioeghlsrntcaniemidv` | 264,695,111 | 0.63M | 20S 200M PA | 9x |
| 4C | `chtrnoaeoylrsewginsmdaoci` | 264,142,662 | 1.18M | 20S 200M 4C C2C | 44x |
| M8 | `dattigieoskrnspyoaiuilmch` | 237,605,369 | 27.7M | 20S 200M M8 | 7x |
| NOCAL | `nahipwtscmesogannirkodalo` | 230,609,692 | 34.7M | 20S 200M NOCAL C2C | 20x |
| M8 + 4C | `eitomwesaimsnrguiaoyphclf` | 214,983,909 | 50.3M | 20S 200M M8 4C C2C | 2x |
| M8 + NOCAL | `nlmhttaciurossplinetvadnt` | 213,327,134 | 52.0M | 20S 200M PA M8 NOCAL | 11x |
| all but M8 | `davyknilkssorancauethimwp` | 219,236,135 | 46.1M | 20S 200M PA 4C NOCAL C2C | 1x |
| all seven | not found | | | | |

What this shows:

- **PA and 4C are almost free.**
  - The best Pennsylvania board gains PA, MD, NV and WY, and loses MO, CO, OK, UT, WV, HI and MT.
  - The best Four Corners board gains NM, TN and NV, and loses AL, OK, WV, HI and MT. It also
    earns C2C.
- **NOCAL costs 34.7M.** That is less than California itself (39.5M): losing CA, NJ, OK and MT
  frees squares for TN, MD, NV and MS.
- **M8 costs 27.7M.** MISSISSIPPI and MASSACHUSETTS each have three doubled letters, and one
  alteration fixes only one of them. The best M8 board has two diagonally adjacent S squares (for
  both SS pairs) and a TT pair, and alters the rest. It gains MA, MD, MS and NV but loses NC, SC,
  NJ, WI, CO, UT, OK, WV and HI.
- **All seven together** need M8, 4C and PA on a board with no California. A coast-to-coast
  chain must then end in Oregon or Washington, and the board must still reach 200M. The best
  boards found get 6 of the 7, everything but M8 (219.2M). Every board with M8 had at most 5
  citations:
  - 20S, 200M, M8, 4C and C2C (215.0M)
  - 20S, 200M, PA, M8 and NOCAL (213.3M)
  - 20S, 200M, M8, NOCAL and C2C (205.9M)

  The search tried for all seven in five runs:
  - random starts, with a penalty of 20M and of 40M per missing unit
  - starts seeded from the 6-citation board, the M8 + NOCAL board and the M8 + 4C board

  Including the single-citation runs, that is 2,669 restarts with citation penalties, about 1.5
  hours on 4 cores. Only two boards had M8, 4C and NOCAL together, and both were under 200M. The
  best of them, `nlmhptaciurosstlinetvadmw`, scores 190,412,691 with 20S, M8, 4C and NOCAL, but no
  PA and no coast-to-coast chain. Seven citations at once may be impossible, but this search
  cannot prove that.

### 8. Second round: three parallel search agents (one core each, about 80 minutes each)

The goal of this round was a higher score only. Each agent took a different line of attack, and
its code and logs are in `agents/<name>/`. **None found a board above 265,326,530.** The
1-hour parallel-tempering control run (`results/logs/PT_long_1core.csv`) also ended on that same
board.

**`agents/meta/`: different search methods** (`meta.cpp`, with an incremental scorer that
re-traces only the states affected by a change):

| method | runs | mean | runs reaching 265.33M |
|---|---:|---:|---:|
| plain SA (reimplemented) | 71 | 259.60 | 0 |
| SA + structured moves (row/col shift, 2x2 rotate, adjacent pair, plant 3 letters of a missing state) | 93 | 259.94 | 3 |
| SA on randomly reweighted state values, then the true score | 88 | 259.26 | 3 |
| short SA + exhaustive 3-square search | 407 | 255.70 | 3 |
| late-acceptance hill climbing | 61 | 249.26 | 0 |
| memetic GA, from scratch, replace worst | 578 children | 262.03 | 0 (stuck at 264.14) |
| memetic GA, from scratch, replace most similar | 617 children | 256.18 | 4 |
| memetic GA seeded with runner-ups (best board excluded) | 662 children | 259.13 | rebuilt it after ~85 s |

Row/column swaps and 3x3 symmetries were almost never accepted (under 0.1%), so they are useless
as moves.

**`agents/targeted/`: forcing states in or out** (`as2t.cpp`):

- *Best board containing each missing big state:*

  | Must contain | Best score |
  |---|---:|
  | WA or NV | 264,897,652 |
  | PA | 264,695,111 |
  | TN | 264,409,206 |
  | MA | 261,438,923 |
  | CT | 261,046,205 |
  | KY | 260,044,457 |
  | MS | 258,788,356 |

  Every one costs at least 0.43M.
- *Conflicts.* Across the 947 distinct boards scoring at least 255M, WA conflicts with TN, PA
  with MI and MO, and MA/KY/CT with MN/MI/NJ. MA, KY, CT, NH, RI and SD never appear among the
  top 50 boards.
- *Banning one of the best board's states, then releasing the ban:* nothing went above 264.7M.
- *Ruin and recreate* (blank a 3x3 window or 5 to 8 squares, re-anneal, accept within 1.5M to 3M):
  46,600 iterations from the best board found nothing better.

**`agents/exact/`: exact methods** (`bb.cpp`, a branch-and-bound search that treats unassigned
squares as wildcards to get an upper bound):

- *Proven, with no sampling, for the best board:*
  - no change of up to 4 squares improves it (all 12,650 square sets);
  - no change of 5 squares inside any 3x3 or 4x4 box does (15,492 square sets);
  - no change of any 2x3 or 3x2 rectangle does;
  - nor does any change of 6 squares inside a 3x3 box, over the 40% of those square sets that
    were checked.
- *Runner-ups:* none of the 13 runner-up boards (264.9M and below) can reach 265.33M with 4 or
  fewer changes.
- *Cross-check:* the targeted agent independently checked all 4-square changes (2.0 billion
  boards, restricted to 21 letters) and every 5-square change inside a 3x3 window (3.4 billion
  boards). It found nothing either.
- *Global proof not achieved.*
  - CP-SAT with letter variables plus reachability had a bound still above 300M after 120 s,
    with only one 3x3 window free.
  - SAT (CaDiCaL) could not even rebuild a board containing the best board's own 32 states
    within 150 s.
  - So proving "no board holds a set of states worth more than 265.33M" is out of reach at this
    budget.

**Conclusion.** Eight different search families (four in round one, plus GA, LAHC, LNS and exact
branch-and-bound) all top out at the same board. It is optimal against any change of up to 4
squares, and any 5-square change inside a 4x4 box. 265,326,530 is very likely the global
optimum.

## Reproduce

```
cd altered_states_2
make                                             # build/as2
python3 score.py thoainesl                       # the puzzle's example: 32,913,047
./build/as2 score hmlfsoaioutsnrcregahoywin      # the answer
python3 score.py hmlfsoaioutsnrcregahoywin --paths
./build/as2 sa --threads 4 --restarts 10 --iters 5000000 --t0 3e6 --t1 1e6 --seed 1
./build/as2 polish hmlfsoaioutsnrcregahoywin --depth 3
./build/as2 pt --threads 4 --seconds 120 --replicas 16 --tlo 2e5 --thi 3e6
./build/as2 sa --award NOCAL --threads 4 --iters 5000000 --t0 3e6 --t1 1e6 --seconds 240
python3 sweep.py LABEL [as2 sa options]          # log + summarise one setting
python3 analyze.py --top 20                      # best distinct boards over all logs
```

`--award LIST` (any of `20S,200M,PA,M8,4C,NOCAL,C2C`) subtracts `--penalty` (default 20M) per unit
of shortfall: each missing required state, California being present (3 units), each state missing
from the cheapest coast-to-coast chain, and so on. The search then trades points for the citations.
