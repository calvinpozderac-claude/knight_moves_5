# exact: branch-and-bound on the suffix of the 8,123 tour

Best: **8,123** (no improvement; `best.txt` = copy of `data/tours/record_8123.txt`, re-verified:
`ok=True maxvisits=3 unnecessary=[]`).

## Method
`bb.cpp` (+ `common.h`, includes `../../src/eng.h`): depth-first branch-and-bound over **all** pinned
moves of the integer engine (`pinned_moves(s, 3, allowH8)`), starting from the state after move k of a
tour.
- Fail-soft DFS: returns an upper bound on the future minutes of a state; finding a finish > alpha
  raises alpha and prints `FOUND`.
- Transposition table (2^24-2^25 entries) keyed on (altitudes relative to the knight, visits, position):
  the future of a state does not depend on T or the path, and the rules are translation-invariant in
  altitude, so stored upper bounds are valid everywhere. This is what makes the exhaustive mode work.
- Move ordering: the known tour's move first (incumbent stays prefix-consistent), then by wait.
- Bound (`ub_future`, a "dance relaxation", NOT admissible in general): dance = the two levels around the
  knight (top, top-2) with the most points; sitters = points in the knight's connected component of
  available dance points (ortho U<->L, knight moves within a level, vis<3); each opposite pair with both
  members in the dance gives max(remaining visits) sits and kills one point at its first sit; sits
  alternate U/L (so #sits <= 2*min(S_U,S_L)+1); each sit is worth 4 x current group size.
  `--check` (exhaustive mode) counts states where exact future > bound: ~1-2% of visited states,
  all off-dance states (merges, wandering). So results with the bound are strong evidence, not proofs.
- `--nobound`: pure exhaustive memoised search = a sound proof (modulo the engine = checker, which holds:
  every wait in an integer-altitude tour is a pinned move).
- `--lambda L`: prune if T + L*bound <= alpha (deliberately optimistic, a focused heuristic search).

## Results (prefix of record_8123, alpha = 8123, i.e. "is there anything > 8123?")
| k (moves kept) | mode | time | nodes | result |
|---|---|---|---|---|
| 134..120 | exhaustive (sound) | <1 s | <=0.6M | optimum = 8123 (suffix optimal) |
| **118** | **exhaustive (sound)** | 60 s | 78M | **proven: no tour > 8123 keeps the first 118 moves; 8123 suffix optimal** |
| 116 | exhaustive | >2 min | - | did not finish (growth ~x100 per 2 moves) |
| 112 | dance bound | 0.2 s | 45k | no improvement (evidence) |
| 108 | dance bound | 1.1 s | 276k | no improvement (evidence) |
| 104 | dance bound | >240 s | 59M | timeout, no improvement found |
| 104 | bound x0.9 | 36 s | 10.5M | no improvement (heuristic) |
| 100/96/90 | bound x0.85/0.8/0.75 | see sweep_lam.log | | (filled in below) |

Sanity check of the heuristic bound against exhaustive search on other tours (alpha 0, find the optimum):
record_8002 k=118,120 and record_7911 k=126,128: bounded search returns the same optimum as the exhaustive
one (8002, 7911) with 100-5000x fewer nodes. (Side result, sound: the last 16 moves of 8002 and the last
8 of 7911 are optimal.)

## Reproduce
```
cd subagents/exact && g++ -O2 -march=native -o bb bb.cpp
./bb ../../data/tours/record_8123.txt 118 --nobound --tt 25          # sound proof, ~60 s
./bb ../../data/tours/record_8123.txt 108 --tt 24                    # dance bound, ~1 s
./bb ../../data/tours/record_8123.txt 104 --lambda 0.9 --tt 24       # heuristic, ~36 s
./bb ../../data/tours/record_8123.txt 122 --nobound --check --alpha -100000000   # bound violation stats
./stats ../../data/tours/record_8123.txt     # per-move T, group sizes, remaining visits (stats.cpp)
```

## Conclusions / next ideas
- The end of the 8123 dance is locally optimal: nothing better exists after move 118 (proof), and the
  bounded search finds nothing after moves 104-112. Gains, if any, are earlier (merge / early dance),
  consistent with the README's diagnosis.
- Exhaustive search dies at ~k=116 (a 20-move suffix) because of zero-wait wandering and off-dance
  moves. To go further soundly one needs a provably admissible bound; the hard part is off-dance states
  (risen opposite points that can be re-joined, merges). A sound version: compute the bound only when
  every non-dance point is >= top+3 and no risen point is within reach, else fall back to exhaustive.
- Better use of exact search: as a *finisher* inside NRPA/beam (solve the last ~18 moves exactly at
  every rollout end / beam leaf), and with a macro-move model (sit + landing + up to 2 zero-wait hops)
  where the dance bound is admissible by construction.
- Stronger bound ingredients not yet used: each sit also needs a landing on an orthogonal neighbour in
  the other group (matching-type constraint between U and L sitters), and kills should be charged to
  the partner's actual group.
