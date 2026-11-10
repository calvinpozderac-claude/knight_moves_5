# goal: design the dance start first, then search for an opening that reaches it

Build (from the repo root):
```
g++ -O3 -march=native -std=c++17 -pthread -o subagents/goal/goal subagents/goal/goal.cpp
subagents/goal/goal selfcheck            # replays the record: must print 8123 minutes, legal yes
```

## Method

1. **`goal design`**: replays a tour (default `data/tours/record_8123.txt`), prints the score of every
   dance start between `--kmin` and `--kmax`, describes the target at `--k` (default: the first dance
   start, move 42 of the record: 31+26 points, knight on h5, which already has 3 visits), and writes a
   targets file: the base target, a greedy chain of edits, and the best single edits. The edits are:
   free one visit of a group point, merge an outside point into a group, move a point to the other group,
   start the dance on another upper-group square. Each target is scored by **dbeam**, an exact legal
   dance found by a beam over the dance moves (see the top of `goal.cpp`), after screening with **bound3**
   (the README's idealised dance plus adjacency, the final sit and joins).
2. **`goal search`**: a beam search (multithreaded) from a1 or from the first `--start-k` moves of
   `--start-tour`. Priority = T + target estimate − distance to the nearest target. The distance is
   frame-free (the target's upper/lower points must share one level r / r−2): 150 per distinct level
   left (`--w-clu`), 5 per unit of altitude off (`--w-alt`), 100 per visit beyond the target's
   (`--w-vis`). It adds a lookahead (`--lookahead 2`: best priority after one sit, or one hop then a sit)
   and diversity (at most `--bucket-cap` states per knight square × levels left). Every dance start it
   reaches is a candidate (not expanded further unless `--expand-dance`). At the end the best `--rerank`
   by bound3 and the `--rerank-close` closest to a target are re-scored with dbeam. Each dbeam dance
   is a finished tour, so it also goes to best.txt and pool.txt. `--finish-top N` then finishes the N
   best prefixes with full NRPA at `--finish-level`.

## Commands

Smoke (about 1 minute, one thread):
```
subagents/goal/goal design --out-dir subagents/goal/tmp/design            # ~11 s, writes targets.txt
subagents/goal/goal search --targets subagents/goal/tmp/design/targets.txt \
    --start-tour data/tours/record_8123.txt --start-k 30 --max-depth 14 --beam 4000 --threads 1 --time 35 \
    --rerank 60 --rerank-close 30 --out-dir subagents/goal/tmp/s30
python3 tools/check_tour.py "$(sed -n 2p subagents/goal/tmp/s30/best.txt)"
build/nrpa --prefix-file subagents/goal/tmp/s30/prefix_001.txt --level 3 --threads 1 --time 60 --out subagents/goal/tmp/s30/fin.txt
```
(`subagents/goal/targets_record42.txt` is a copy of the design output, so the design step can be skipped.)

Full run (4 threads, about 1 hour). Search from move 25 (the climb is not fixed after that), then
finish the best prefixes with fresh NRPA:
```
nice -n 5 subagents/goal/goal search --targets subagents/goal/targets_record42.txt \
    --start-tour data/tours/record_8123.txt --start-k 25 --max-depth 24 --beam 50000 --threads 4 --time 2400 \
    --rerank 1000 --rerank-close 300 --dbeam 3000 --finish-top 8 --finish-level 3 --finish-time 1200 \
    --out-dir subagents/goal/tmp/full25
# then a level-4 NRPA on each of the best few prefixes (about 1 hour each, one thread each):
for i in 001 002 003 004; do nice -n 5 build/nrpa --prefix-file subagents/goal/tmp/full25/prefix_$i.txt --level 4 \
    --threads 1 --time 3600 --pool subagents/goal/tmp/full25/nrpa_pool.txt --pool-min 7500 \
    --out subagents/goal/tmp/full25/nrpa_$i.txt & done; wait
```
Variants: `--start-k 10` or `--start-k 0` (from a1; use `--w-t 0.2`, otherwise the beam piles up opening minutes
and never forms a dance), several `--rng` with `--noise 30`, or a different design `--k`/`--tour`.

Memory: about 30 MB per 10k beam width at depth 25 (the layers' links, the children and up to 2 × `--max-found`
candidates). Beam 50,000 needs well under 1 GB.

## Output files (in `--out-dir`)
- `prefixes.txt`: one candidate per line: `dbeam  bound3  T  moves  target  dist  prefix` (dbeam −1 = not
  re-ranked), best first, at most `--write-max` lines. Rewritten every 10 s and at the end.
- `prefix_001.txt` … (`--top-out`, 50): line 1 the estimate, line 2 the prefix, the format `build/nrpa --prefix-file` reads.
- `best.txt` (minutes, then the tour) and `pool.txt` (`minutes<TAB>tour`, ≥ `--pool-min` 7,500): finished tours
  from dbeam and the NRPA finishes. Every tour is in the checker's format.
- `goal design` writes `targets.txt` (tab separated: name, estimate, T, knight, 64 altitudes, 64 visits);
  you can edit it by hand to make your own targets.

Ctrl-C or `--time` stops the search cleanly (it stops mid-layer, then re-ranks and writes).

## Smoke results (one thread at nice 15 on the shared, fully loaded machine)
- `selfcheck`: the record replays to 8,123, legal; text round trip ok.
- `design` (11 s): the record's dance start at move 42 scores ideal 7,474 / bound3 7,212 / dbeam 7,234 (the real
  tour made 8,123). Best variant: dbeam 7,758 (+524) after 4 edits: start the dance on f2 instead of h5 (h5 already
  has 3 visits), move b1 to the upper group, free a visit of e8 and of c4.
- dbeam separates openings the bounds cannot: dance start of 8,123/8,002 → 7,554, of 7,200 → 6,552 (width 3000);
  bound3 rates them the other way (7,731 vs 8,195).
- `search` from move 30, beam 4,000, 33 s: 349k nodes (about 11k nodes/s per thread including lookahead), reaches the
  record's own target (dist 0 to the base target, dbeam 7,234) and finds two prefixes dbeam rates higher (7,306,
  verified tour `best.txt`, 7,306 minutes). An NRPA level-2 finish of those prefixes made only 4,605 (level 2 is too
  weak; use level 3–4).
- `search` from move 25, beam 3,000, 13 s: best dbeam 7,002 (verified). With beam 5,000–20,000 the record's own path
  is pruned around move 33–37 (the climb has plateaus of hops); with `--lookahead 2` and beam 10,000 it survives to
  move 36. The full run's beam 50,000 should keep it.
- `search` from a1, beam 2,000, 45 s: no dance start (h8 is never sunk), as the README predicts for searches from a1.
