# guided84: prior-guided NRPA anchored across 8,484, then the free-wait pass

Goal: a checker-accepted tour above 8,484 (`data/tours/record_8484.txt`, new basin).
2 heavy threads, `nice -n 5`. All code here is copied from `../prior/` and `../fractional/`.

## Build (eng.h changed on main; all rebuilt here)
```
g++ -O3 -march=native -std=c++17 -pthread -o pnrpa pnrpa.cpp      # ../prior/pnrpa.cpp
g++ -O3 -march=native -std=c++17 -o gen gen.cpp                     # ../prior/gen.cpp
g++ -O3 -march=native -std=c++17 -pthread -o fnrpa fnrpa.cpp      # ../fractional/fnrpa.cpp
g++ -O3 -march=native -std=c++17 -pthread -o nrpa nrpa_stock.cpp  # ../../src/nrpa.cpp, include path fixed
```
Replay check: `gen` on 8,484 + 8,337 reports best 8484; pnrpa (anchor 150) and fnrpa (anchor 150-160) both
replay 8,484 to exactly 8484 / 8484.000; check_tour ok=True maxvisits=3 unnecessary=[].

## Structure of 8,484 (`python3 ../lns/dump.py ../../data/tours/record_8484.txt`)
- climb 1-43: c2 d2 d1 b1 c1 ... up the h-file to f8 (alt 8), groups <= 13 points.
- merge 44-66: f6 e4 f4 h5 ... big waits (78, 88, 124) merge the board into two groups of ~31 / ~26-33.
- dance 67-125: two-group ping-pong (b7/b8, a7/a8, a6/b6, c6/c7, c8/d8, c5/c4, b4/a4, a3/a2, b2/b3, b5/a5),
  waits 124 down to 66.
- dance re-cross 126-145: d6 d5 d3 d4 e4 e3 e2 d2 d1 c1 c3 e4 (small waits 0-48), then
  dance tail 146-160: e5/e6, e8/e7, f7/f6, f7 -> h8.

## Priors (`train_prior.py --hidden 16 --epochs 20 --l2 1e-3 --wpow 1 --drop 28,29,35,36,37`)
- `prior_g1.txt`: gen over all `data/tours/*.txt` (incl. 8,484 and 8,337) + polish 8127 + lns 8297 +
  prior/tmp/r3/prevpool.txt (1,386 tours, 52,447 positions). Only ~144 positions are from 8,484.
- `prior_g1_hold.txt`: the same without 8,484 (8,337 kept).
- Old-basin reference: `../prior/prior_r3.txt`.

## Scripts
- `phaseB.sh S RNG`: L3 pnrpa (prior_g1, beta-set 0.5-2.5, init-noise 0.5) vs stock nrpa, fresh weights,
  anchors 44-130 of 8,484, 1 thread each, S seconds.
- `fw.sh ID N MIN T UNTIL POOL...`: free-wait pass (fnrpa warm L2, 8 rngs x T s) on every distinct skeleton >= MIN.
- `summ.py LOG...`: completed restarts by anchor band. `distinct.py MIN POOL...`: distinct finishes.
- `newrec.sh FILE...`: any checker-accepted tour above the record -> NEW_RECORD_<m>.txt.

## Results
### Phase B (01:47-01:56, 520 s per arm, rng 4101): `bash phaseB.sh 2400 4101` (stopped early, signal clear)
Fresh L3, anchors 44-130 of 8,484, completed restarts only (`python3 summ.py tmp/B/prior.log tmp/B/stock.log`):

| band | prior_g1 n / median / max | stock n / median / max |
|---|---|---|
| merge 44-66 | 3 / 7,938 / 8,152 | 7 / 6,742 / 7,567 |
| dance 67-100 | 8 / 7,848 / 8,066 | 20 / 7,872 / 8,027 |
| dance 101-125 | 6 / 7,955 / 7,971 | 7 / 7,923 / 8,152 |
| 126-130 | 1 / 8,236 | 1 / 8,236 |

Prior restarts take ~27 s, stock ~15 s. Prior helps in the merge (+1,200 median), is neutral in the dance.
Neither comes within 248 of 8,484: the best (8,236, both arms) keeps 8,484's first 127-129 moves.
Fresh L2 probes (stock, 8 s): anchor 135 -> 8,484 (45 of 58 rollout-restarts), 142 and 148 -> 8,484 too.
So the hard part of 8,484 is moves ~126-134 (the d-file re-cross d6 d5 d3 d5 d3 d5 d3 d4 e4 with waits 0-48):
fresh NRPA starting before move ~130 never finds it and falls to ~7,950-8,240.
Holdout prior (`gen ... --val-file record_8484.txt`, trained without 8,484): top-1 on 8,484's 144 positions 0.65
(dance 0.67) vs max-wait 0.46 / uniform 0.16; the old basin prior reached 0.78 on old-basin dances.

`prior_g2.txt`: g1 data + phase B pools, positions whose best tour >= 8,349 (= 8,484's 160 positions) weighted x30
(`--boost-min 8349 --boost 30`, new option in train_prior.py).

### Phase C (from 01:57): `bash phaseC.sh 1500 4201`, prior_g2, L3, anchors 44-150, beta-set 0.5-2.5, noise 0.5
- arm `sa1` (`--seed-adapt 1 --no-warm`: one NRPA adapt step toward 8,484's continuation, then a free search):
  **restart 0 (anchor 87, beta 0.5) -> 8,488, strict, checker ok=True maxvisits=3 unnecessary=[]**
  (`NEW_RECORD_8488.txt`, strict_check STRICT). It keeps 8,484's first 97 moves and reverses the order of the
  four dance pairs after c5/c4: 8,484 does b4/a4, a3/a2, b2/b3, b5/a5, c5 c4 d4; 8,488 does a5/b5, b3/b2,
  a2/a3, a4/b4, c4 d4. Moves 126-161 (the re-cross and the tail) are unchanged.
- arm `fresh` (seed-adapt 0): 2 restarts (7,821 / 8,050), stopped at 01:58 to free a thread for the free-wait pass.
- Free-wait pass: `bash fwloop.sh 0 1 8400 120 UNTIL "tmp/C/pool_*.txt tmp/D/pool_*.txt"` (1 worker, 8 rngs x 120 s
  per distinct skeleton >= 8,400, highest first; summary in tmp/fw/summary.txt).
- Phase C sa1 stopped at 02:00 after 8 completed restarts: merge 7,880 / 7,938; dance 67-100: 8,488;
  101-125: 7,957, 8,484; 126-150: 8,484 x2, 8,236. Distinct >= 8,400: 8,488 and 8,484 (a copy).

### Phase D (from 02:01): `bash phaseD.sh a 2400 4301 67 125 1,2,3`
- `pnrpa2` = pnrpa.cpp + `--sa-set` (random seed-adapt count per restart, logged as "sa N").
- `prior_g3.txt`: g2 data + phase C pools + 8,488, boost x30 on positions with best >= 8,349 (220 positions).
- seeds `tmp/seeds2.txt` = 8,484 and 8,488 (random per restart), anchors 67-125 (dance), sa 1/2/3.
- D-a (rng 4301, noise 0.5, sa 1/2/3), stopped at 02:08 after 11 restarts: 8,488 x6, 8,484 x3, 8,328, 8,221.
  All >= 8,400 are copies of the two seeds (distinct.py): seed-adapt >= 1 sticks to the seed's dance.
  Seed 8,484 at anchors 94 and 114 returned 8,484, not the 8,488 reorder.
- D-b (from 02:08): `NZ=1.0 bash phaseD.sh b 2400 4302 67 125 1,2` (more noise for diversity).
- Bug fixed: a `pkill -f pool_sa1.txt` also killed the free-wait worker (its argv held the pool names), so
  fw.sh now keys rngs on the skeleton hash and skips (skeleton, rng) pairs already in tmp/fw/summary.txt.
- D-b stopped 02:10 after 5 restarts (8,488 x3, 8,484, 7,972): still only copies.
- `pnrpa3` = pnrpa2 + `--tail-from M --tail-adapt N`: N adapt steps on the seed's tail (moves M..end, played from
  the seed's own state at M), so the search remembers the re-cross/end game but searches the dance fresh.
  `phaseE.sh TAG S RNG AMIN AMAX SASET TAILFROM TAILADAPT [NOISE]`.
- E-c (02:10-02:12, rng 4303, anchors 67-118, sa 0, tail-from 124, tail-adapt 3): 5 restarts, 8,488 x4, 8,484 --
  the tail memory works (fresh dance search now reaches 8,484-8,488 instead of ~7,950), but every result is a copy
  of a seed: the dance after move ~90 looks locally optimal.
- E-d (from 02:12): `bash phaseE.sh d 2400 4304 44 90 0 124 3` (merge + early dance, fresh, tail memory).
