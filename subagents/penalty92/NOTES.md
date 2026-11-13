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
- 23:23 merge bans (f3,f7 or g6,h3 in moves 40-90, pen 500, 120 s): no feasible tour at all. Single-square merge
  bans (A_M_f7, A_M_h3) queued.
- Same-sitter order variants fall back to 9,275: A_S_c3d3 (raw 8,817, c3/d3 block replaced by e6/f6 late) -> plain
  LNS 9,267 (order c5/d5 c3/c4 d3/d4, sitters unchanged), and two more seeds from it / from its raw tour both end
  at **9,275 with 9,275's exact signature** (two tours distinct from the record, md5 23904b34, b8a997d6). A_E_c4c5's
  raw tour already had 9,275's signature (skipped). A_L_c6c7 (c6/c7 c8/d8 after the a8/b8 a7/b7 sweep): raw 9,197
  -> 9,263. So 9,275 is a strong attractor for its sitter set; the queue was reordered at 23:50 toward sitter-moving
  rules (the user's successful rule shapes: b6,c6,d4,d5 (-> 9,080), c6,c7,c8 (-> 8,887), c3,c4,d3 (-> 8,842),
  a5-a8,b5,c4 (-> 8,900), b6,b7,c8 (-> 8,946), as U_ rules) and block+opposites (O_).
