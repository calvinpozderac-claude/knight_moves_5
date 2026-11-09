# families: new families of Knight Moves 5 tours (Round 7)

**Result: 8,812 (`NEW_RECORD_8812.txt`, LX2e s45 at the end of its 150k iterations), +203 over 8,609, STRICT
(no free waits); check_tour ok=True, maxvisits=3, unnecessary=[].** Also three distinct 8,810 finishes (md5
3cbb5def, e28e09e8, bbbdc365; `keep/`). All are in the new family F5 below. Found by diversity-forced LNS (penalising the e-file tail of 8,609) followed by plain LNS.

## Families table (raw = first tour of the family; polished = after polish; LNS = best after LNS)
| family | signature (sitting blocks, `sig.py`) | raw | polished | LNS best |
|---|---|---|---|---|
| **F5 (new, d-file)** | f7/h3 **c8/d8 c6/c7 a7/b7 a8/b8 a6/b6** c5/c6 c4/c5 b4/c4 a4/b4 a3/a4 a2/a3 a2/b2 b2/b3 b3/c3 c2/c3 **d3/d4 a5/d5 a5/b5 d6/d7** e7/e8 f8/g8 | 8,421 (X2f) | 8,421 | **8,812** (dlns 8,557 -> lns 8,595 -> 8,703 / 8,810 x3 / 8,812; polish of 8,795 -> 8,809, of 8,810 -> 8,810) |
| F1 (AlphaZero/A*, before this round) | b7/b8 a7/a8 a6/b6 c6/c7 c8/d8 c4/c5 a5/b5 b2/b3 a2/a3 a4/b4 .. e2/e3 e4/e5/e6 e7/e8 f7/f6 | 8,553 | 8,607 | 8,609 (lns84 r6) |
| X1 (new: no b7/b8 block) | f5/h4 a6/h3 a7/a8 a6/b6 c6/c7 a7/d8 c8/d8 ... e3/e4 **g1/g2** e2/e3 ... f6/f7 f8 | 8,371 | 8,429 | 8,429 (dlns X1b 80k) |
| X3f (new: no e7/e8/f6/f7 end) | ... a4/b4 c4/e5 e3/e4 e2/e3 c2/d2 c1/e4 e5/e6 | 8,281 | 8,281 | 8,281 |
| F2 (8,123 line) | h5/h6 h2/h3 g1/h1 g2/g3 f1/f2 e3/f3 b5/c5 a4/a5 ... e6/f6 f8/g8 | 8,348 | 8,348 | 8,348 (earlier rounds) |
| F2b (7,911/8,002) | h5/h6 g1/g2 h1/h2 g3/h3 e3/f3 ... e8/f8 f7/f8 | 8,002 | 8,006 | - |
| F3 (CLIMB_8166) | a5/a4 a2/a3 g1/g2 h1/h2 g3/h3 ... f7/f8 | 8,166 | 8,166 | 8,166 (34k it, nothing) |
Distinct families with a polished score: 7 (3 new: F5, X1, X3f).

**What makes F5 better:** the sitters are the a-d files (d-file instead of the e-file of F1), the top-left is swept
in the opposite order (starting c8/d8 rather than b7/b8), and the tail ends e7/e8 -> f8/g8 -> h8. The merge is
also new (f7 36, f6 100, e5/e6 44+44, f5 58, then h3 -> h1 116 at T = 655).

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

## Step 4: diversity-forced LNS from 8,609 and LNS on F3
**Pitfall found (15:28, flagged by the orchestrator):** lns_r6/dlns with `--free 2` (the lns84 default) can generate
free sinks (wait on a same-altitude knight jump) without checking necessity. Under the e-file penalty, X3 found
tails where the two dance groups merge into ONE group (n = 41, h8 included) and the knight keeps "sitting" with
free sinks: 8,635 -> 8,649 internally, but `check_tour.py` rejects every such wait (unnecessary=[133..160]); X2's
8,478 had the same defect at moves 152-156. The saver never wrote them (it requires unnecessary=[]); the files
were moved to `x/bad/`. It is not a bug in the penalty code but a hole the penalty pushed the search into: the
lns objective does not test necessity of free sinks. Fix: `dlns.sh` now runs with `--free 0` (free waits already
in the input tours, e.g. 8,609's (48, 25, g6), are kept; no new ones are generated), and every reported tour is
re-checked with check_tour. X2/X3 were restarted as X2f/X3f.
Runs (all from 8,609, donors 8,609 / 8,563 / strict 8,607, t0 40, otherwise lns84's flags):
- X1 (s1, `--free 2`, but its result is clean): forbid b7,b8 from move 50, pen 150, 100k it -> **8,371** (no b7/b8
  block; the dance starts a7/a8 and g1/g2 become sitters in the tail). Polish 600 s -> **8,429**; dlns X1b
  (s12, 80k, from 8,429) -> 8,429.
- X3f (s31, free 0): forbid e7,e8,f6,f7 from move 120 -> 8,281; polish -> 8,281.
- X2f (s21, free 0): forbid e2..e6 from move 100 -> **8,421** (still rising at the end), a new dance order:
  c8/d8 c6/c7 a7/b7 a8/b8 a6/b6 c5/c6 c4/c5 c3/d3 b3/b4 a4/b4 a3/a4 ... d3/d4 d4/d5 d6/d7 e7/e8: sitters a-d files,
  the d-file replaces the e-file, the top-left is swept in reverse. Polish -> 8,421.
- X2g (s22, dlns continuation from 8,421): **8,421 -> 8,556 in 6 min**, 8,557 at the end (80k).
- LF3: lns_r6 on F3 8,166 (never LNS'd before), 34k it: nothing (stopped).

## Step 5: plain LNS on the new d-file family (F5)
- LX2 (s41, lns_r6 `--free 0`, no penalty, start 8,557, donors only F5 tours, focus 1-150 p 0.85):
  8,557 -> 8,565 -> 8,590 -> 8,594 -> **8,595** within 9 min (ops 7/2 in the dance and climb). Checker ok,
  unnecessary=[]. It keeps the F5 dance (sig: c8/d8 c6/c7 a7/b7 a8/b8 a6/b6 c5/c6 c4/c5 c3/c4 c3/d3 d3/d4 b3/b4
  a4/b4 a3/a4 a2/a3 a2/b2 b1/b2 a5/b5 d4/d5 d6/d7 e7/e8).

## Step 6: the F5 family breaks the record (16:20-)
- LX2 (s41, 90k): 8,595 -> 8,697 -> **8,703** (it 87k). LX2b (s42, from 8,595, donors F5 only): 8,597 ... 8,618 ->
  8,649 (op 6 a=145, duplicate block in the d-file tail) -> 8,693 (it 22k) -> 8,712 -> 8,795 (it 52k) -> 8,810
  (it ~62k). All checker ok, unnecessary=[]; 8,712 is STRICT (strict_check.py).
- Gains come mostly from the dance tail (ops at i = 110-150: the d-file / c2-c3 / a5-d5 region) plus small
  climb tweaks (i = 15-25). The merge also changed: around move 44-62 the tour now sits f7 36, f6 100, e5/e6 44+44,
  f5 58 and reaches h1 at T = 655 with groups 29/28 (dance first sit h3->h1 116).
- NOTE: the saver started at 14:50 was not running at 16:20 (it died silently); 8,703/8,712 were written by
  hand after check_tour, and the saver was restarted at 16:22.
- Running 16:26: LX2b (to ~16:54), polish of 8,795 (900 s), LX2d s44 and LX2e s45 (from 8,795, 2100 s).
- Final (16:50): LX2b, LX2d (s44), LX2e (s45) all end at 8,810 (three distinct tours); polish of 8,795 -> 8,809
  (783 s), polish of the e28e09e8 8,810 -> 8,810 (reroute only). Plateau at 8,810.

## Lessons
- The diversity penalty is what found the family: forbidding the e-file sits (e2..e6 after move 100) made the
  search rebuild the whole dance on the d-file; the dlns continuation took that 8,421 to 8,557, and plain LNS
  without the penalty (donors only from the new family, so it cannot fall back to F1) took it to 8,810 in ~20 min.
  Forbidding the dance-start block (X1) or the final block (X3f) produced families 180-330 lower.
- Always run lns/dlns with `--free 0` (or check every output): with `--free 2` the penalised search found tails
  that merge the two dance groups and "sit" with free sinks, which check_tour rejects (8,649, 8,478; `x/bad/`).
- Saver scripts can die silently: check that they are alive.
- Next: exact tail proof (bb) on 8,810; more forced variants of F5 (e.g. forbid its d6/d7 or c8/d8 block) to
  find the next family; climb/merge crossovers between F5 tours and F1's climb.
- 16:59: LX2e s45 ended at **8,812** (STRICT, checker ok; NEW_RECORD_8812.txt, keep/). LX2d s44: 8,810. All jobs and the
  saver stopped 17:00.

# Round 8: penalise F5's own blocks (start 18:30, after a container restart; 3 threads)
Record at start: 8,812 (`data/tours/record_8812.txt`, F5). f5push handles F5's tail proof and focused LNS.
- `bin/dlns` now takes several rules: `--rule "sq,sq,...:FROM:TO"` (a sit of >= 40 minutes leaving one of the
  squares at a move index in [FROM, TO) costs `--pen`). `ylns.sh TAG SEED ITERS START PEN "RULES" DONORS...`
  runs it with `--free 0`; logs and outputs in `y/`. Saver threshold raised to 8,812 and it now also watches
  `y/` and the `*feas_best.txt` files (started with setsid).
- main's `record_8595.txt` (F1 variant): sig f5/h4 b8/h3 b7/b8 a7/a8 a6/b6 c6/c7 c8/d8 c6/c7 c4/c5 a5/b5 b2/b3
  a2/a3 **b2/b3 a3/a4** a4/b4 c4/e3 e2/e3 c1/e4 e4/e5 e5/e6 e7/e8 e7/f7 f6 (F1 with an extra b2/b3 a3/a4 block).
Runs (pen 150, 120k it, TAIL = no sits on d3-d7 or e2-e6 from move 100):
- Y1 s101: from 8,812, TAIL.
- Y2 s102: from 8,812, TAIL + no sits on c8,d8,b7,b8 in moves 55-99 (neither F5's nor F1's first block).
- Y3 s103: from X1 8,429 (a7/a8-first dance), TAIL.
Results so far (all feasible bests checker ok, unnecessary=[]):
- Y1 (TAIL from 8,812): **8,658** at it 23k (no gain after): new family **F6 "d-first"**: f7/h3 **d3/d4 d5/d7 d6/d7**
  c8/d8 c6/c7 a7/b7 a8/b8 a6/b6 c5/c6 c4/c5 b4/c4 a4/b4 a3/a4 a2/b2 b3/c3 c2/c3 a5/b3 a5/b5 e7/e8 f8/g8: the d-file
  block moves from the end of F5's dance to its start (before the c8/d8 sweep).
- Y2 (TAIL + no c8/d8/b7/b8 first block): 8,433, also d-first (d4/e4 c3/d3 d4/d6 d6/d7 then c8/d8 after move 100). Stopped.
- Y3 (TAIL from X1 8,429): collapses (feasible 7,435). Stopped.
- Y4 (no d3-d7 sits from move 60, no e2-e6 from 100): 8,083, family "F7": c8/d8 c7/e3 e2/e3 a5/b5 c5/c6 ... e7/e8
  f8/g8 (e-file early, no d-file).
- Plain LNS on F6 (`--free 0`, donors only F6 tours): LF6 s61 8,658 -> 8,691 -> **8,804** (it 88k); LF6b s62 (from
  8,691) -> 8,800. Both keep the d-first signature. LF6c s63 started from 8,804.
