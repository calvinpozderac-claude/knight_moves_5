# nmcs: notes (round 1b, code-ready)

`RUN.md` has the method, the build line, the commands and the smoke numbers. This file covers what I
learned while building it. Nothing ran longer than 60 s (1 thread, nice 15).

## Method
- NMCS keeps the best sequence found so far and plays its next move at each step. Lazy NMCS rates every
  move with a few cheap playouts and keeps only the best k of them, plus the move on the memorised best
  sequence. SP-MCTS uses max-backup and PUCT with the softmax prior, and moves its root one step along
  the best sequence after a set number of iterations.
- All three run over the pinned moves of `src/eng.h`. The state carries an altitude histogram, so the
  playout features cost O(1) per move.

## What mattered
- **The playout policy is everything for NMCS.** From move 35 of the record:
  - Uniform playouts finish 17% of the time, but at a mean of 511 minutes (best 1,582 in 20k).
  - The "dance step" feature (`--fd`) is the one that mattered. It fires when a move sinks and the sunk
    group ends 2 below the target. With it, the best of 20k playouts rose to about 5,900.
  - Feature weights were tuned by the mean NMCS level-1 result over 300 restarts. That mean went from
    4,234 (no visit feature) to 4,908 with the defaults (`fw .25 fr 10 fv 3 fg .5 fd 10 tau .8`).
- **Visits feature:** a positive `--fv` (prefer targets with more visits left) beats a negative one for
  NMCS. This holds even though the record mostly alternates within a pair until both are used up.
  `NMCS_DIAG=2` shows each record move's rank under the policy. The record's zero-wait repositioning
  hops and its 3-unit sits get probability of about 0, so the search has to find them through
  branching.
- **Temperature:** at higher levels, a flatter policy (`--tau 2`) gave better maxima from move 100. From
  move 35 at level 2, `--tau 0.8` was better (mean 6,210 against 6,007).
- **MCTS:** PUCT (prior = softmax) clearly beats UCT (means of about 5,950 against 5,185). It needs
  step commits; without them it reached only 3,899. At equal playouts it still trails NMCS level 2
  (mean 6,210).
- **NMCS level 3 against NRPA level 3, from move 100 (40 s each):**
  - NMCS L3 at tau 2: 2.5 s per restart, mean 7,825, best 7,966.
  - NRPA L3: 20 s per restart; its two results were 7,994 and 7,840.
  - So NMCS is in the same range at about 1/8 of the time per restart. Neither recovers the record's
    8,123 from that position.

## Best
`best.txt`: **7,966** (fresh NMCS L3 after move 100 of the record, verified ok=True, maxvisits=3,
unnecessary=[]). It does not beat the record and was not meant to; this round was code and smoke tests.

## Recommendation for the run phase
1. Run Lazy NMCS level 3 fresh after move 34–41, on 4 threads for 1 h (`RUN.md` command A). Each restart
   is estimated at 6–10 min, so that gives about 25–40 dances. Compare its pool with NRPA level 4's.
2. Run a warm NMCS L3 polish of 8,123 at tau 2 with random anchors 60–125 (command C). It is cheap and
   can only improve.
3. If (1) is competitive, try NMCS L4 on short suffixes (anchor ≥ 90; about 5–10 min per restart) and a
   level-dependent tau.
4. A better policy matters more than the search method. Two options:
   - Add features for zero-wait repositioning toward a fresh pair, and for "the partner of the sitting
     point is already gone".
   - Learn the weights: fit the softmax to pool tours (log-likelihood) or by cross-entropy, then feed
     them to NMCS.
