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
