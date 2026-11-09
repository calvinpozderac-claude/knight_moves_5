# polish63: polish + LNS on record_8559 / record_8563 (round 7)

## Setup (14:48)
- Sources copied from subagents/lns84/ (lns.cpp = round-6 version with `--start`, polish.cpp and bb.cpp/common.h
  with free-wait support), rebuilt here with lns84's flags against current src/eng.h.
- Replay check: lns reads 8559 -> 8559, 8563 -> 8563; bb at k=n-2 gives 8559 / 8563; polish reads both exactly.
- Scripts: `pol.sh TAG IN TIME [WMAX]`, `lns.sh TAG SEED ITERS START FOCUSFILE DONORS...`, `exact.sh TOUR TAG T`,
  `saver.sh` (checker-verifies any run/*_best.txt above 8,609 -> NEW_RECORD_<T>.txt), `tail.py N files...`
  (last N squares + common-tail lengths). Logs: run/, event log run/events.log.
- Originals: 8,559 and 8,563 share their last 48 moves; both share the last 20 squares (17 moves with waits)
  with 8,609 / strict 8,607 (e5/e6/e8/e7/f7/f6 block from move ~146).

## Step 1: polish (900 s, wmax 12, 1 thread; `pol.sh`)
- **8,559 -> 8,607** (converged at 595 s): w=0 at 17 (+1, after reroute), **w=1 at 136 (+40)**, w=8 at 20 (+3),
  w=10 at 100 (+4). Same pattern as round 6. Checker ok=True maxvisits=3 unnecessary=[], 157 moves.
  **The result is byte-identical to data/tours/strict_8607.txt** (same md5 of the tour line): polish alone
  maps 8,559 onto the existing strict 8,607, including its dance tail.
- **8,563 -> 8,607** (converged at 449 s): w=0 at 17 (+1), w=1 at 136 (+40), w=8 at 20 (+3). **Identical to the
  polished 8,559 and to strict_8607** (md5 51b824a8, = lns84 r6/P8549_best.txt). Both new tours collapse onto
  one existing tour within 10 minutes of polishing.
- Note on "only 11 shared moves with 8,609": that is a positional count. Aligned (difflib over (wait,square)),
  8,559 shares 131 of 166 moves with 8,609 and 127 with 8,549; same opening, same dance order.

## Step 2: exact tail on the polished best (90 s per k; P8563 skipped, identical tour)
- Optimum 8,607 for k = 153 ... **135** (k=135: 35 s, 57M nodes); k=133 timed out at 90 s (145M nodes, nothing
  better). LNS focus 1-134 p 0.85 (round 6 had this tour proven from 133).

## Step 3: LNS (`q2b.sh`, 30k iters, free waits `--free 2`, T0 40, round-6 flags)
Since both polished tours are the same tour, the two lineages are:
- L8559 s301-303: start = polished best (= strict 8,607), focus 1-134.
- L8563 s311-313: start = the **original, unpolished 8,563** (`--start` keeps it), focus whole tour, so this
  lineage has to make its own way up from 8,563 (with the same donors available for crossover).
Donors (both): originals 8,559/8,563, record 8,609, strict 8,607, polish pools of both.
- L8559 s301 (from 8,607): 30k, nothing (8,607).
- L8563 s311 (from original 8,563): 8,599 at it 129 (op 2 i=126: the dance-tail sit), 8,600 (op 7 tail crossover),
  8,603/8,604/8,606 (tail op 1, climb ops) -> **8,607** at it 5,981, then nothing. Checker ok, 161 moves,
  md5 837c850d: a re-route of strict 8,607 (two extra zero-wait hops h2/h3 and h4/g4, g5 instead of f4 for one
  92-min sit); last 104 moves identical to strict 8,607 / 8,609.
- L8559 s302 (from 8,607): nothing (8,607).
- L8563 s312 (from 8,563): climbs via tail ops (8,579-8,600) and climb ops to 8,604, then **8,607** at it 28,645
  (op 7 tail crossover a=130). md5 26cd46ca, 163 moves, checker ok; last 104 squares = strict 8,607's.
- L8559 s303 (from 8,607): nothing (8,607). So 0 of 3 seeds from the polished start moved (round 6's L8549
  seeds on this exact start reached 8,609 twice with more donors: 8,605 family incl. the (48,25,g6) free wait).
- L8563 s313 (from 8,563): 8,592 ... 8,604 (tail crossovers), 8,607 (op 7 climb crossover a=11), **8,608** at
  it 20,718 (op 2 i=151 D=8). md5 f9373050, 162 moves, checker ok=True maxvisits=3 unnecessary=[]. Not one of
  lns84's r6 tours. Differs from 8,609 only in the climb (moves 15-58: f3/f5 shuffles, g4 sit instead of g6,
  no g2/f4 pair); its last 104 moves are identical to 8,609's.

## Step 4: polish of the distinct LNS finishes (`q3.sh`, 420 s each, wmax 12)
Inputs run/C/: 8608_f9373050 (s313), 8607_837c850d (s311), 8607_26cd46ca (s312).
