# goexplore notes (round 1b: code and smoke tests only)

**Method.** Go-Explore / MAP-Elites over coarse state descriptors, explained in full in `RUN.md`.
- The descriptor is: stage (opening / merge / dance), knight altitude relative to h8, move-count
  bucket, size of h8's group, visits left within 2 of h8's altitude, and opposite pairs still able
  to sit.
- Each cell keeps its best-T state as a path from a1, plus the best finish known through it.
- Selection weight: novelty (rarely chosen, recently improved) × exp((val − best)/200).
- Exploration: NRPA level 1 or batches of random playouts. Every visited state goes into the
  archive.
- Seeds: all 1,279 tours in data/tours (record files and curriculum.txt).

**Design note.** Plain "visits left" is useless as a descriptor: every jump uses exactly one
visit, so it always equals 188 minus the move count. The archive uses the visits left inside the
dance band instead (`--trace FILE` prints the descriptor along a tour).

**Smoke results** (1 thread, nice 15, fully loaded machine):

| Variant | Time | Best new tour | Notes |
|---|---|---|---|
| Default (`--p-nrpa 0.5 --level 1`, fit T) | 45 s | **8,092** (verified) | 137k cells, 877k playouts, 750 distinct pool tours ≥ 7,500 |
| Default, earlier build | 30 s | 8,092 | 8,053 and 8,049 also new |
| `--fit pot`, resumed | 20 s | — | Ran fine. A resume with mismatched settings was refused, as it should be. |
| `--uniform --level 2 --p-nrpa 0.2` | 20 s | 6,957 | Uniform picks go to the curriculum's many short tours. Uniform selection is a poor choice here. |

- The rediscovered 8,123 seed does not count as a search best.
- The 8,092 tour keeps the record's climb (same first moves) and differs later. It is in
  `best.txt`.

**Not done** (CPU budget): no long runs. Whether the archive gets past 8,123 is untested.

**Recommendation for the full run.**
- `--threads 4 --time 3600 --table-log2 22`, as in `RUN.md`.
- Then compare `--fit pot`, `--temp 400 --unk 600` (more diversity) and `--level 2 --p-nrpa 0.2`.
- Also try `--seed` with other agents' pools, for more diverse elites.
- If merge cells dominate without progress, use finer buckets (`--mb 2 --vb 3`).
