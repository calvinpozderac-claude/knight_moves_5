# families: new families of Knight Moves 5 tours (Round 7)

Goal: families (different dance order or group sizes) whose polish + LNS ceiling beats 8,609.
3 heavy threads, nice -n 5. Start 14:48.

## Tools (all in this folder)
- `bin/heur_search` (src/heur_search.cpp), `bin/polish`, `bin/lns_r6`, `bin/bb` (copies of lns84's binaries).
- `bin/dlns` (`bin/dlns.cpp`): lns84's lns_r6 + a **diversity penalty**: `--forbid a1,b2,... --pen P --ffrom K
  [--sitmin 40]`. Every sit (wait >= sitmin) that leaves a forbidden square at move index >= K costs P minutes in
  the SA's acceptance and best tracking. The best tour with no forbidden sit is written to
  `<out>.feas_best.txt`. Forces the search off a dance block of the record.
- `sig.py` (dance signature: sitting squares, wait >= 60, compressed to blocks), `sits.py` (sits/visits board).
- `pol.sh TAG IN TIME [WMAX]`, `lns.sh TAG SEED ITERS START LO HI P DONORS...`, `dlns.sh TAG SEED ITERS START
  FORBID PEN FFROM DONORS...`, `saver.sh` (any `*_best.txt` above 8,609 that check_tour accepts ->
  `NEW_RECORD_<m>.txt`). Event log: `events.log`.

## Families known before this round (dance signature = sitting-block order, `sig.py`)
| family | example | dance (blocks) | sitter side |
|---|---|---|---|
| F1 (AlphaZero) | 8,609 / 8,563 / 8,553 | b7/b8 a7/a8 a6/b6 c6/c7 c8/d8 c4/c5 a5/b5 b2/b3 a2/a3 a4/b4 .. e2/e3 e4/e5/e6 e7/e8 f7/f6 | left three files + e-file; climb spends the bottom-right |
| F1b (8,484 line) | 8,605 | ... c4/c5 **a4/b4 a2/a3 b2/b3 a5/b5** c4/c5 e2/e3 ... | same, middle blocks reversed |
| F1c (8,559) | 8,559 | ... a5/b5 a3/b3 a2/b2 a3/a4 a4/b4 ... | other pairing (polish63's job) |
| F2 (8,123 line) | 8,348 / 8,123 / strict 8,297 | h5/h6 h2/h3 g1/h1 g2/g3 f1/f2 e3/f3 b5/c5 a4/a5 a2/a3 b2/b3 b4/c4 ... e6/f6 f8/g8 | bottom-right 3x3, then left middle, ends f8/g8 |
| F2b (7,911 / 8,002) | 8,002 | h5/h6 g1/g2 h1/h2 g3/h3 e3/f3 b5/c5 ... e8/f8 f7/f8 | as F2, other pairing |
| F3 (CLIMB_8166) | 8,166 | a5/a4 a2/a3 g1/g2 h1/h2 g3/h3 e3/f3 d2/e2 f1/f2 ... f5/f6 f7/f8 | starts left, then bottom-right |
All three reach essentially the same state at the dance start (T = 655-656 with groups 32/30 or 33/29);
they differ in which visits the climb spends and so which point of each opposite pair can sit.

## Step 1: heur_search from a1 (option a) - not viable here
`bin/heur_search --threads 3 --seed 300000 --time 90 --tau 1`: best **301 minutes** in 90 s (9.2M expansions,
1.5M finishes). Raw heuristic tours are ~8,300 minutes short; the AlphaZero run needed hours of training on top
of them. Not pursued.

## Step 2: fresh NRPA from the dance start of strict 8,607 (option c) - too weak
`build/nrpa --level 3 --seed-adapt 0 --no-warm`, anchors 46-62 (2 threads) and 30-46 (1 thread): first
restarts 7,211 / 7,453 / 7,788 (anchors 46-62) and 4,697 / 5,823 (anchors 30-46), ~22 s each. 800+ below the
record, more than polish + LNS adds (50-120). Killed after ~1 min.

## Step 3: modern polish on the older families (600 s, wmax 12)
| input | polished |
|---|---|
| F2 8,348 | 8,348 (reroute only, converged) |
| F2b 8,002 | 8,006 |
| F3 8,166 | 8,166 (converged 471 s) |
These were already locally optimal (prior's NRPA + free-wait pass); polish gives nothing.

## Step 4 (running): diversity-forced LNS from 8,609 and LNS on F3
- X1: dlns, forbid sits on b7,b8 from move 50 (the dance start block), pen 150, 100k it.
- X2: dlns, forbid sits on e2..e6 from move 100 (the e-file tail), pen 150, 100k it.
- LF3: lns_r6 on F3 8,166 (never LNS'd before), focus 1-140 p 0.85, 60k it.
