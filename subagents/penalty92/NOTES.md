# penalty92: automated penalty + order-trial family sweep from 9,275 (and 9,271)  (Round 9)

Goal: a tour above 9,275 from a new family. 2 heavy threads (2 workers), nice -n 5. Start 23:16 (2026-11-13).

## Tools (this folder)
- `bin/` copies of families' `dlns`, `lns_r6`, `polish`, `bb`; `fam.py` (sit sequence, block signature, block-level
  edit distance), `gen.py` (sitting blocks with move ranges and opposite squares).
- `worker.py WID SECS`: pulls jobs from `queue.txt` (flock), appends `results.tsv`, logs to `events.log`.
  - `dl|NAME|BASE|PEN|rule;rule`: dlns (`--rule sq,sq:FROM:TO`, `--free 0`, base as start and only donor, DLT=120 s,
    time-based annealing) -> feasible best (no penalised sit) = raw tour -> block signature; skipped if it equals a
    known family (`known_sigs.txt`: 8,830 ... 9,275 + every new signature found here) -> plain `lns_r6 --free 0`
    on that tour alone, LT=420 s.
  - `lns|NAME|START|SEED|TIME|donors|focus`: more plain-LNS seeds on a family.
- `penalty92_saver.sh`: every 30 s, any `y/ lns/ pol/` tour above 9,275 that check_tour accepts -> `NEW_RECORD_<m>.txt`.
- Distance = block-level edit distance of the sit signature to 9,275 / size of the symmetric difference of the
  sitter sets (9,271 is 12/1, 9,268 11/1, 9,080 16/8 on this scale).
- Rule types (block = 9,275's sitting block, dance from move 95): S = ban the block's squares from move 80;
  P = two blocks; O = block + its opposite squares; X = ban block squares over the whole tour (pen 300) to push it
  to the opposite side; L/E = order trials (L: ban the block before its current end -> it must come later;
  E: ban it from its current start -> earlier); M = early merge sits f3/f7 and g6/h3 in moves 40-90 (pen 500).
  Prefix A_ = base 9,275, B_ = base 9,271.

## Notes
- dlns with `--iters` huge freezes the temperature at t0 (schedule is it/ITERS); the worker runs time-based (no --iters).
- The merge sits are hard to remove: a 25 s dlns with f3,f7 banned in moves 40-90 (pen 500) found no feasible tour.
