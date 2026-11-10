# genetic: notes

Round 1b (code-ready): this is code plus smoke tests only. The method, build line, commands and outputs are in
`RUN.md`; the source is `genetic.cpp` (one file; it includes `../../src/eng.h`).

## What was built
A memetic GA over whole tours: genes (square, dz) with implied pinned waits; replay-with-repair (other dz /
skip / bridge DFS scored by lookahead legality, then NRPA-1 completion biased toward the abandoned tail);
same-square, altitude-similarity-weighted one- and two-point crossover; window mutations and NRPA tail
regeneration (sometimes a warm-started level-2 NRPA); first-improvement local search on elites; islands with
ring migration, restarts, pool output, `--time`, Ctrl-C.

## Smoke results (short runs only, CPU busy)
| run | seeds | time | best |
|---|---|---|---|
| default seeds, rng 1-5 | data/tours (incl. 8123, later 8130) | 40-50 s each | = best seed (8123 / 8130), no gain |
| no records above 7200 | curriculum + 7066 + 7200 | 50 s | 7200 -> 7234 |

Pool tours (117 of them) all pass `tools/check_tour.py`. `best.txt` holds 8,130 = `data/tours/record_8130.txt`
(found by another agent, read in as a seed). It is **not** a GA improvement.

## Observations
- Without diversity control the population fills up with neutral copies of the best tour within seconds (all 64
  at 8123). Rejecting more than 2 members with the same minutes, plus crowding replacement, keeps about 40 distinct
  scores.
- Crossover is the only operator that often beats its first parent: about 30% of one-point children do, mostly
  weaker parents taking a better dance. Single-window mutations almost never improve a good tour. The memetic
  local search improves about half of the elites it is given, but only by a few minutes.
- Most of the CPU time goes into NRPA-1 completions once the population has converged (repairs fail more often).

## Recommendation for the next round
- Run it for 1 h with 4 islands on the pooled tours of all agents (`--seeds data/tours/*.txt <pools>` with
  `--parch 0.3`): crossover is most useful when the archive holds many different dances and merges.
- Try `--min-cut 34` (only merge + dance evolve) and `--p-deep 0.2` (more NRPA-2 tail polishing) as a second
  variant.
