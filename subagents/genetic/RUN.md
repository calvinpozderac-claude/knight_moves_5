# genetic: memetic / genetic algorithm over whole tours

## Method
- **Genome** = the tour as a list of genes (target square, dz), with dz = alt[q] - alt[p] right after the jump.
  The wait is implied (pinned moves, as in `src/eng.h`): delta = dz - (alt[q] - alt[p]) >= 0. So a "sit 4 and hop
  up 2" gene keeps its meaning when it is replayed after a different prefix.
- **Fitness** = replay from a1; the tour ends at the first arrival at h8. Every state passed is checked for a
  finishing jump, so a child that breaks down late still finishes at its best passed chance. Finished tours
  score their minutes; unfinished ones -1e7 + T*exp(-dist to h8), so they always rank lower.
- **Repair** (at the first illegal gene): candidates are the same target with the other dz, skipping 1..`--skip`
  genes, or a bridge of 1..`--bridge` free moves (DFS) after which gene i..i+skip is legal again. Each candidate
  is scored by how many of the next `--look` genes then replay legally (small penalty per bridge move or skipped
  gene) and the best is applied. After `--max-repairs` repairs, or when nothing fits, the rest is completed by NRPA
  level 1 (`--comp-iters`), its rollouts biased (`--tail-bias`) toward the (from, to) pairs of the abandoned tail.
- **Crossover**: cut both parents where the knight is on the same square; the cut pair is drawn with probability
  ~ exp(-dist/`--xtau`), dist = number of points whose altitude relative to the knight differs (+0.5 per differing
  visit count). One-point A[0..i)+B[j..) or (40%) two-point A[0..i1)+B[j1..j2)+A[i2..). Partner B comes from the
  island or (`--parch`) from the seed archive.
- **Mutation**: delete / insert / replace a window of 1-3 moves, flip a dz, regenerate the tail with NRPA-1, or
  (`--p-deep` of tail mutations) with a warm-started NRPA level `--deep-level` seeded by the parent's own tail.
- **Memetic step** every `--ls-every` generations on the island best and one elite: first-improvement local
  search (delete 1-3, replace/insert every legal move at every cut, `--ls2` random 2-move replacements), each
  neighbour evaluated by replay + repair, `--ls-evals` evaluations max. Lamarckian: the repaired tour is kept.
- **Population**: steady state, `--pop` per island. A child replaces its parent if better (deterministic crowding),
  else the most similar (longest common prefix) of `--crowd` random members if better; exact duplicates and
  more than `--max-same` members with the same minutes are rejected (otherwise neutral clones of the best take over).
- **Islands**: `--threads` islands, one thread each, ring migration of the best + one random member every
  `--mig-every` generations, an island re-seeds after `--stall` generations without improvement.
- **Seeds**: by default every `.txt` in `data/tours/` (records, curriculum.txt, openings.txt); any file with
  `(wait, sq)` pairs works (record format, pool format `minutes<TAB>tour`, curriculum lines), via
  `--seeds f1 f2 ...`. The best `--seed-top` distinct finished tours form the archive.

## Build
```
g++ -O3 -march=native -std=c++17 -pthread -o subagents/genetic/genetic subagents/genetic/genetic.cpp
```

## Run (from the repo root)
Self-check (parses all seeds, replays the best five, prints their minutes; 8123 replays to 8123):
```
subagents/genetic/genetic --selfcheck
```
Smoke run (40 s, 1 thread):
```
mkdir -p subagents/genetic/tmp
nice -n 15 subagents/genetic/genetic --threads 1 --time 40 --out subagents/genetic/tmp/smoke_best.txt \
    --pool subagents/genetic/tmp/smoke_pool.txt
```
Full run (suggested: 4 islands, 1 hour; ~10 MB RAM in total):
```
nice -n 5 subagents/genetic/genetic --threads 4 --time 3600 --rng 1 \
    --seeds data/tours/*.txt runs/*pool*.txt \
    --out subagents/genetic/tmp/ga_best.txt --pool subagents/genetic/tmp/ga_pool.txt --pool-min 7500 \
    > subagents/genetic/tmp/ga.log 2>&1
```
Variants worth trying: `--min-cut 34` (keep the climb, evolve only merge + dance), `--p-deep 0.2 --deep-level 2`
(more NRPA polishing), `--pop 128 --stall 10000` (slower, more diverse), `--parch 0.5` with a big pool of
other agents' tours as `--seeds` (transplants their dances).

Outputs:
- `--out FILE`: best tour so far (line 1 minutes, line 2 tour), rewritten atomically on every new best.
- `--pool FILE`: every finished tour that enters a population with >= `--pool-min` minutes and within
  `--pool-slack` (150) of the best so far, deduplicated, `minutes<TAB>tour`. About 150-180 lines in the first 40 s
  of a run, then about 1 per second per island (about 2 KB per line). Lower `--pool-slack` for long runs.
- stdout: new bests, every 10 s a status line (best, children/s, completions, repairs, rollouts, per-island best,
  median and number of distinct scores) and operator statistics (made/accepted/better than parent/finished).
- At exit (time-out, Ctrl-C or SIGTERM) the best is checked with `python3 tools/check_tour.py`
  (`--checker none` to skip).

## Smoke-test results (machine fully loaded by other agents, nice 15, 1 core)
- `--selfcheck`: all seed files parse (1268 of 1272 curriculum lines finish); 8123, 8002, 7911, 7200 and 7066
  replay to their exact minutes; plain replay+finalize about 120k tours/s.
- Default seeds, 40-45 s, 1 island: about 5-14k children/s (including local-search evaluations), about 300
  NRPA-1 completions/s, 15-20k rollouts/s. The population stays diverse (37-43 distinct scores out of 64,
  median about 8050-8090). Best = the seed (8,123, later 8,130 once `data/tours/record_8130.txt` appeared); no
  improvement over the best seed in these short runs. All 117 pool tours of the first smoke run were checked
  with `tools/check_tour.py`: all `ok=True`, minutes equal, `maxvisits=3`, `unnecessary=[]`.
- Seeds without the >7200 records (curriculum + 7066 + 7200), 50 s: 7200 -> 7234 (crossover / repair on its own
  finds small gains in the dance).
- 4 islands on one core (taskset), 15 s: migration and restarts work; 8.5 MB RSS. SIGINT stops cleanly and
  still writes the result.
