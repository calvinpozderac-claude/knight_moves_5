# goexplore: Go-Explore / MAP-Elites quality-diversity search

## Method
- **Archive.** A lock-free open-addressing table of cells, keyed by a coarse state descriptor:
  - stage: 0 = h8 not sunk yet, 1 = h8 sunk but not dancing, 2 = dance. Dance means the knight's
    group and the group 2 below hold at least `--dmin` (30) points, with h8 in one of them.
  - knight altitude minus h8 altitude, clamped to ±8;
  - move count / `--mb` (4);
  - size of h8's group / `--gb` (3);
  - visits left on points within 2 of h8's altitude, / `--vb` (6). This is the sitting capacity
    left; plain visits left is always 188 minus the move count, so it adds nothing;
  - opposite pairs (a1/h8 excluded) that still have a member which can be visited within that
    band, / `--pb` (2).

  Each cell keeps its elite: the path from a1 to the best state seen with that descriptor, ranked
  by minutes T (`--fit T`, the default). With `--fit pot`, dance cells are ranked by T plus the
  idealised-dance estimate from `src/nrpa.cpp` instead. Each cell also stores `val`, the best
  finished tour known to pass through any state with that descriptor, and its selection counters.
- **Loop (per thread).**
  1. Pick a cell. Its weight is novelty × quality:
     - novelty is `1/sqrt(1+times chosen) + R/sqrt(1+times chosen since its last improvement)`
       (`--recent R`, 1);
     - quality is `exp((val - best)/TEMP)` (`--temp`, 200). A cell with no known finish counts as
       `best - UNK` (`--unk`, 400). `--uniform` picks cells uniformly (plain MAP-Elites).
  2. Restore the cell's state by replaying its path.
  3. Explore. With probability `--p-nrpa` (0.5), run one NRPA search at level `--level` (1),
     `--iters` (100) per level, starting from fresh weights at that state. Otherwise run `--batch`
     (32) uniform random playouts (`--bias` adds a minutes-waited prior).
  4. Every state visited by every playout is offered to the archive. A new descriptor creates a
     cell; a better state replaces the cell's elite.
  5. Every finished tour sets `val` (a max) on every cell along its path, goes to the pool, and,
     if it is a new best, to `--out`. Playouts remember the best chance to finish they passed, as
     in `src/nrpa.cpp`.
- **Seeds.** By default all the tours in `data/tours/record_*.txt`, `tour_6303.txt`,
  `from_scratch_2952.txt` and `curriculum.txt` (1,279 tours, 7,651 cells). Every state of every
  seed is inserted, and each cell gets that seed's finish as its `val`. To give your own files,
  repeat `--seed FILE`. Any file with one tour per line works, including nrpa pools (lines of
  `minutes\ntour`) and this program's pools (`minutes<TAB>tour`).
- **Search best.** It counts only tours that are not seeds, so rediscovering 8,123 doesn't count.
  Each new search best of at least `--pool-min` minutes is run through
  `python3 tools/check_tour.py` before it is written to `--out`. Over 8,123 it also goes to
  `<record-dir>/NEW_RECORD_<minutes>.txt`.
- **Self-check at start-up.** `data/tours/record_8123.txt` must replay to 8,123 in the engine, or
  the program exits.

## Build
```
cd subagents/goexplore && g++ -O3 -march=native -std=c++17 -pthread -o goexplore goexplore.cpp
```
It includes `../../src/eng.h`. **Run it from the repo root**: the seeds and the checker are found
by relative path.

## Smoke run (about 45 s, 1 thread)
```
G=subagents/goexplore; mkdir -p $G/tmp
nice -n 15 $G/goexplore --threads 1 --time 45 --report 15 --pool $G/tmp/pool.txt --out $G/tmp/best.txt \
    --ckpt $G/tmp/archive.bin --record-dir $G
```

## Full run (suggested)
```
G=subagents/goexplore; mkdir -p $G/tmp
nice -n 5 $G/goexplore --threads 4 --time 3600 --table-log2 22 --pool $G/tmp/pool.txt --pool-min 7500 \
    --out $G/tmp/best.txt --ckpt $G/tmp/archive.bin --ckpt-every 600 --record-dir $G > $G/tmp/run.log 2>&1
# resume (use the same --mb/--gb/--vb/--pb/--dmin/--fit; the checkpoint refuses other settings):
nice -n 5 $G/goexplore --threads 4 --time 3600 --table-log2 22 --resume $G/tmp/archive.bin --pool $G/tmp/pool.txt \
    --out $G/tmp/best.txt --ckpt $G/tmp/archive.bin --record-dir $G >> $G/tmp/run.log 2>&1
```
- Add `--no-seeds` to skip re-seeding on resume. Re-seeding is harmless: it only raises elites
  and values.
- Variants worth a run:
  - `--fit pot`;
  - `--level 2 --p-nrpa 0.2`: stronger exploration, each NRPA call takes about 0.5 s;
  - `--uniform`: plain MAP-Elites selection;
  - `--temp 400 --unk 600`: more diversity;
  - finer cells: `--mb 2 --vb 3`.
- **Memory.**
  - Base table: about 56 B per slot. At 2^21 slots that is 112 MB; at 2^22 it is 225 MB.
  - Each cell adds about 250 B of path. The table stops creating cells at 70% load, with
    "archive FULL" in the log.
  - 2^22 slots allows 2.9M cells, about 1 GB in all.
  - At the smoke run's growth rate, 4 threads for 1 hour stay under 2.9M cells.
- **Ctrl-C / SIGTERM** stops cleanly: the threads join and the checkpoint is written.

## Output files
| File | Contents |
|---|---|
| `--out` (`best.txt`) | Best new (non-seed) tour: line 1 minutes, line 2 the tour. Verified by the checker when ≥ `--pool-min`. |
| `--pool` | Every distinct new finished tour of at least `--pool-min` (7,500) minutes, one per line: `minutes<TAB>tour`. Seeds are excluded. |
| `--ckpt` | Binary archive: every cell's key, fitness, value, counters and path, plus the search best. Written every `--ckpt-every` seconds and at exit, through a temp file and rename. |
| `<record-dir>/NEW_RECORD_<m>.txt` | Only for a checker-verified tour over 8,123. |
| stdout | Every 10 s (`--report`): cells by stage, playouts/s, finishes, improvements, search best, and the share of picks by stage. |

## Smoke-test results (1 thread, nice 15, machine fully loaded by other agents)
- **Self-check.** record_8123 replays to 8,123 minutes.
- **45 s run.**
  - Seeding: 7,651 cells from 1,279 tours.
  - After 45 s: 137k cells (360 opening, 99k merge, 37k dance) and 877k playouts. After the
    first ~10 s, 26k–65k playouts/s.
  - 120k finished tours, 750 distinct new ones of 7,500 or more in the pool.
  - Best new tour: **8,092** after 18 s, verified (`ok=True`, `maxvisits=3`, `unnecessary=[]`).
  - About 78% of picks went to merge-stage cells.
- **The first ~10 s are slow** (about 3k playouts/s): almost every state is new and gets copied
  into the archive, and every new search best of 7,500 or more calls the Python checker.
- **Resume.** The run resumed from its own checkpoint (152k cells), and the archive kept
  growing. A resume with mismatched descriptor settings was refused, as it should be.
- **Memory.** About 160 MB RSS at 170k cells (2^21 table).
