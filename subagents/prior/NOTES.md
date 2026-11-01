# prior: learned move prior for NRPA (GNRPA bias)

Code-only round (CPU busy): built, trained and smoke-tested; no long searches. Details and commands in `RUN.md`.

## Method
- Data (`gen.cpp`): 1,276 distinct tours (records, curriculum, openings). Every state on a tour gets the best
  final score of any tour through it. For each of the 45k distinct positions on a tour with >= 2 legal non-h8
  moves, every pinned move is labelled with the best score through its child state (0 = no known tour).
- Features (42, `prior.h`): wait minutes, sink size, sinking group size, jump dz, the target's group size, groups
  2 below/above, gap to and size of the biggest other group, visits left at the target and at its opposite,
  the opposite of the knight leaving the group, distance/altitude gap to h8, h8 in the target's group or 2
  below, phase (visits used / 130), visits left in the target's group and the one below, h8 sinking,
  plus (dropped in the recommended model, too slow) static reachability of h8, reach size, mobility.
- Model (`train_prior.py`, numpy): listwise softmax over each position's moves, target softmax((L-Lmax)/50)
  over moves with a known tour, position weight Lmax/8123 + 0.02. MLP 42-16-1 tanh, Adam, 20 epochs, L2 1e-3.
- Search (`pnrpa.cpp`): logit = NRPA weight + beta * prior, in rollouts and in adapt; per-thread cache of
  exp(beta * prior) per state (80-92% hits at level 2).

## Numbers
- Ranking, top-1 on held-out positions (7911/8002/8123 + 15% of other tours held out), dance positions
  (move >= 35, tour >= 7,000): MLP 0.74-0.78, linear 0.70, "most wait" 0.61, uniform 0.19. Opening positions:
  0.51 vs 0.33 (zero-wait) / 0.16 uniform. Dropping the reach/mobility features did not hurt (0.78 vs 0.74).
- Level 2, fresh weights, equal wall time, from the 8,123 tour (table in RUN.md): anchor 35, 20 s: stock mean
  3,504 / best 4,924 vs prior mean 6,500-6,770 / best 6,950-7,315. Anchor 60, 15 s: stock 5,635 / 6,408 vs
  prior 7,460-7,715 / 7,790-7,911. The model trained without the three records does as well (so it is not
  just replaying 8,123). Rollouts are 4-5x slower with the prior.
- best.txt: 7,930 (checker ok), a 4 s level-3 test from anchor 60 (model trained on all tours, 8,123 included).

## Caveats
- Few long tours in the data (only ~650 positions from tours >= 7,000), so the dance knowledge is thin;
  the prior is mostly "sit 4 in the upper group, hop onto the other group, prefer points with visits left".
- At level 3/4 the gain at equal time is untested; a strong prior may also cut diversity (beta 3 was worse
  than 2 at level 2). The level-4 cost is ~4-5x stock, so use `--iters 50` at level 4.

## Next
1. Equal-time A/B at level 3 (anchor 35-46) and level 4 `--iters 50` (anchor 35), beta 1/2, stock vs prior.
2. Self-training: add the NRPA pool tours (>= 7,500) to the data and retrain (gen 2 s + train 20 s), repeat.
3. Use the prior in opening search (`--objective opening`) and from a1 with small anchors.

# Round 2 (2026-11-01)

## (1) Equal-time A/B at level 3 (`ab.sh`, `ab_summary.py`, logs in tmp/ab/)
Stock NRPA vs prior (`prior_h16c_holdout.txt`, beta 2; trained without any tour >= 7,500 and without
8,127/8,130/8,211, provenance in `prior_h16c_holdout.txt.meta`). Fresh weights (`--seed-adapt 0 --no-warm`),
anchors 40 / 55 / 70 of 8,211, one thread per arm, both arms run at the same time, 720 s per anchor, --rng 11.
Only completed restarts counted (the restart cut by the time limit is marked PARTIAL and dropped).

| anchor | arm | completed restarts | median | IQR | min | max |
|---|---|---|---|---|---|---|
| 40 | prior | 10 | 7,724 | 7,662-7,839 | 7,249 | 8,095 |
| 40 | stock | 30 | 6,122 | 5,288-6,430 | 4,187 | 6,746 |
| 40 | stock, best of 3 (= one prior restart's time) | 10 | 6,482 | 6,348-6,609 | 5,438 | 6,746 |
| 55 | prior | 13 | 7,837 | 7,802-7,850 | 7,631 | 7,918 |
| 55 | stock | 33 | 6,714 | 6,108-7,096 | 4,127 | 7,584 |
| 55 | stock, best of 3 | 11 | 7,075 | 6,734-7,386 | 6,518 | 7,584 |
| 70 | prior | 14 | 8,178 | 8,169-8,239 | 8,097 | **8,255** |
| 70 | stock | 31 | 7,849 | 7,746-8,009 | 7,022 | 8,240 |
| 70 | stock, best of 2 | 15 | 7,994 | 7,913-8,070 | 7,755 | 8,240 |

The prior wins at every anchor, on median, quartiles and the best result, at equal wall time. The more
search is left (earlier anchor), the bigger the gain: +1,240 median at 40, +760 at 55, +180 at 70.
Prior restarts take 50-67 s against 22-24 s for stock. 8,255 (prior, anchor 70) is **strict** and beats the
strict record 8,235 (it leaves 8,235 at move 81). Stock also found a strict 8,240 at anchor 70
(tmp/stock_8240.txt). Both pass the checker. The free-wait record 8,292 (data/tours/record_8292.txt)
is still higher.

## Label fix (review): `train_prior.py --mask` / `--unk-gap G`
- `--mask` (softmax over labelled moves only): held-out dance top-1 0.63, about the same as max-wait (0.61).
  Too few positions have 2 or more labelled moves, so this throws away the signal.
- `--unk-gap 100` (an unlabelled move counts as Lmax - 100 minutes, so it is "worse but not dead"): top-1 0.77,
  the same as before, but softer (p(best) 0.37 instead of 0.63). Used as the second arm below.

## (2) How 8,290 (strict) and 8,329 (record, 2 free waits) were found
1. `bash round2_search.sh 1500` (02:51 UTC): retrains the prior on every data/tours file (incl. 8,211-8,292),
   polish's 8,127 and the A/B pools -> `prior_r2.txt` (+ `prior_r2_unk.txt`, `--unk-gap 100`), then fresh
   level-3 pnrpa from anchors 30-70 of `data/tours/record_8235.txt` (same first 95 moves as 8,211):
   `./pnrpa --level 3 --threads 1 --time 1500 --rng 21 --seed-tours ../../data/tours/record_8235.txt
    --anchor-min 30 --anchor-max 70 --seed-adapt 0 --no-warm --prior prior_r2.txt --beta 2 --pool tmp/r2_pool_r2.txt`
   Strict **8,290** at restart 8 (anchor 64, ~500 s), again at anchors 63 and 67; the `--unk-gap` model
   also reached 8,290 (and 8,255). Saved as `STRICT_8290.txt` (strict_check: STRICT).
2. Free-wait pass on 8,290 with fractional's fnrpa (built from ../fractional/fnrpa.cpp into tmp/fnrpa):
   `./tmp/fnrpa --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 240 --amin 35 --amax 999 --rng 1
    --seed tmp/s8290.txt --out tmp/fw/fw_8290_r1.txt --pool tmp/fw/pool.txt --pool-min 8200`
   8,294 at restart 0 and **8,329** within 26 s (free waits at moves 141 (80, d4) and 154 (72, g8));
   checker ok, maxvisits 3, unnecessary []. `NEW_RECORD_8329.txt`.

Unfinished in round 2 (container restart): the second A/B rng at anchor 40 and the free-wait pass over the
other pool tours >= 8,200 (8,255, 8,253, 8,240, 8,239, 8,205).

# Round 3 (2026-11-01)

## Results
- **8,334 (record, 3 free waits at moves 100, 132, 145)** = free-wait pass (`r3_fw.sh`, fnrpa warm L2 120 s, rng 3)
  on the **strict 8,295** skeleton (`STRICT_8295.txt` = data/tours/strict_8295.txt). 8,295 came from
  `r3_guided.sh` (L3 pnrpa, anchors 60-75 of lns's 8,297, prior_r3, `--beta-set 1,1.5,2,2.5`, rng 101).
  The same 8,295 skeleton through the pass with other rngs gave 8,299 (x4) and 8,307: the pass is
  high-variance, so run several rngs per skeleton (`fw_multi.sh`).
- Other fw passes (tmp/fw3/summary.txt): 8,291 -> 8,330 / 8,295; 8,290 -> 8,329 / 8,294 / 8,295;
  8,297 -> 8,301; 8,255 -> 8,302; 8,162 (outside-basin) -> 8,162.
- `prior_r3.txt` / `prior_r3_unk.txt`: retrained on every data/tours file, 8,127, lns 8,297 and all pools
  (provenance in the .meta files).

## Seed-collision check (review pass 1)
- L2, anchor 65 of 8,329, 10 s, rng 1 vs rng 2 (tmp/rngtest/): at beta 0 the two rngs give different
  finishes (12 restarts each, one coincidental equal value); at beta 2 rng 2 returns 8,205 in 4 of its first
  5 restarts. So the rng plumbing is fine (`mt19937(rng*1000003 + thread)`, fresh per restart);
  the collapse comes from the prior.
- Fix: `--init-noise S` (random N(0, S) initial NRPA weights per restart) and `--beta-set` (random beta per
  restart, 0 = stock). With `--init-noise 1`: 12 restarts give 10 distinct finishes.

## Dance re-search, anchors 55-80 (r3b.sh: `--beta-set 0.5,...,2.5 --init-noise 0.5`, 240 s per run)
Identical finishes count once (`r3_summary.py`):

| base | rng 301 | rng 302 | rng 303 | distinct finishes (all rngs) | best |
|---|---|---|---|---|---|
| 8,297 | 8295 x3 | 8163, 8240, 8295 | 8131, 8240(2), 8295(2) | 5 | 8,295 |
| 8,329 | 7988, 8205 | 7837, 8290(2) | 8147, 8205, 8290(2) | 5 | 8,290 |
| 8,330 | 8206, 8291(2) | 8206, 8291(2) | 8174, 8206, 8291(3) | 3 | 8,291 |

Each base converges to one best skeleton (8,295 / 8,290 / 8,291). That skeleton then carries the free-wait gains.

## Climb track (7,200 family, lns's run9 tour, 7,879; anchors 20-60, prior_r3_unk, beta-set 0..2)
3 rngs x 600 s: bests 7,693 / **8,162** / 7,481. Every finish was distinct (23 of 23). 8,162 shares only
13 moves with 8,123 and 56 with its seed, and has one free wait at move 39. It is the best tour outside the
8,211 family so far (+283 over the seed). The free-wait pass found nothing to add (8,162 -> 8,162).

## Round 3, resumed (12:12-13:27 UTC; 4 cores from 12:24)
- **8,338 (record)**: `NEW_RECORD_8338.txt`, checker ok, 4 free waits ((127,84,d3), (132,80,c1), (134,80,d4),
  (147,72,g8)). It came from the free-wait pass (`fw_multi.sh`: fnrpa warm L2, 120-180 s) on the **strict
  8,297** skeleton (`STRICT_8297.txt`, strict_check STRICT; the strict record). That skeleton was found by
  `r3c.sh dance` (L3 pnrpa, anchors 55-80 of 8,334, prior_r3, beta-set 0.5-2.5, init-noise 0.5, rng 502),
  and again in a separate run (anchors 55-95, rng 703).
  Free-wait pass on 8,297, 15 runs with different rngs: 8,338 x3 (rngs 81, 90, 31), 8,334 x1, 8,301 x11.
- Free-wait pass on the 8,295 skeletons: the original one gave 8,299 x5 and 8,307 x2 over 7 rngs (the 8,334 seen
  earlier was 1 of 13 runs); a second 8,295 skeleton (from the 8,334 base) gave 8,334 in 3 of 3 runs.
  Warm fnrpa from 8,334 itself: 8,334 in 5 of 5 runs (no gain).
- Dance re-search, 55-80, rngs 501-503 (distinct finishes per run): 8,334 base: {8224, 8244}, {8210, 8297},
  {8207}; 8,330 base: {8206, 8291}, {8240, 8291}, {8131, 8240}; 8,329 base: {7713, 8290}, {8255, 8290}.
  Anchors 55-95 of 8,334, rngs 701-704: {8210, 8295}, {8210, 8264, 8295}, {8255, 8275, 8295, 8297},
  {8210, 8275, 8295}. So only the 8,334 base produced a better strict skeleton (8,297, 2 of 7 runs).
- Climb track from the 8,162 7,200-family tour (anchors 20-60, 3 rngs x 400 s): bests 7,964 / 7,988 / 7,838.
  All 13 finishes are distinct, but none beats its seed. The free-wait pass on 7,988 gives 7,989.

## Recommendation
The loop that works: guided L3 dance re-search (anchors 55-95) from the current record -> new strict
skeleton -> free-wait pass with 6-10 rngs (the pass is high-variance). Each turn of the loop gave +4 to +40.
Loop from 8,338 (anchors 55-95, rngs 801-803, 330 s): distinct finishes {8210, 8264, 8295, 8297},
{8210, 8257, 8260, 8295}, {8275, 8295, 8297}. No strict skeleton better than 8,297 yet.
Next: (a) more of the loop from 8,338 at level 4 --iters 50; (b) add free-sink moves inside pnrpa, so the prior search sees free-wait
gains directly; (c) give the outside-basin climb track (8,162) more depth, e.g. level 4 with --iters 50, or lns.

# Round 4
## 8,348 (record): free waits at moves 100 (104, c3), 117 (88, b1), 119 (22, b3)
- Skeleton search: `bash r4.sh dance 8338 901/902 2900` and `bash r4.sh dance 8334 903 2900` (level 4, --iters 50,
  anchors 55-95, prior_r3, beta-set 0.5-2.5, init-noise 0.5). 23 completed restarts, but only 2 distinct values:
  8,295 and 8,297. The deeper search collapses onto the same skeleton values, though the pool held 3 new
  distinct strict 8,295 tours (different tours with the same minutes).
- Free-wait pass (`fw4.sh`: 4 workers, 8 rngs x 120 s per new skeleton) on 5 new skeletons: one 8,295 skeleton
  (`tmp/r4/round4a_skel/skel_0_5.txt`) gave **8,348** with rng 1055 (1 of 8; the other 7 gave 8,299). Results for the
  other skeletons: 8,299 x7 + 8,300; 8,299 x7 + 8,300; 8,299 x8; 8,303 x4, 8,306, 8,307 x3.
  Here the free waits land earlier (moves 100-119), so the pass rewrites the merge-to-dance stretch.
- Climb basin (L4 iters 50, anchors 20-60 of 8,162, 2900 s): 3 restarts, 7,296 / 7,729 / 7,894. No gain.
