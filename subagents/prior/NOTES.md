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
