# fractional: free (and fractional) waits on top of the strict engine

## Headline
- **The rules are settled** (user, round 2): a free wait is legal if some t' breaks that move or a later
  one. `check_tour.py` is a sound test of that (RULES.md).
- **Records from this folder** (all `check_tour.py ok=True maxvisits=3 unnecessary=[]`):
  - 8,235 (strict): `NEW_RECORD_8235.txt`
  - 8,264 (1 free wait): `NEW_RECORD_8264.txt`
  - 8,288 (1 free wait): `NEW_RECORD_8288.txt`
  - **8,292** (2 free waits): `NEW_RECORD_8292.txt` = `best.txt`
- **The overall record is now 8,329** (prior + free-wait pass). Round 3 found nothing above it here
  (see the round-3 table).
- **Mechanisms:**
  - The +53: a rigid free sink at a1 (a1 -> b3, 1 unit). It also raises h8, which is opposite a1.
    That moves h8's group, and a late zero-wait hop becomes a sit.
  - The +24 (8,211 -> 8,235): a pure dance reorder (the d1/e1 block later). Strict warm L2 finds it in
    seconds.
  - The +4 (8,288 -> 8,292): a second free sink at d3, move 129. It came from 1 of 655 finishes, in
    1 of 5 seeds.
  - Smaller, common gains: the floppy +4 at the f6 -> g8 endgame hop.

## Method
1. **Theory** (RULES.md):
   - Pinned play stays on integer altitudes. Fractional waits can only come from free waits (in-group
     dz=0 knight moves).
   - `pinned_moves` is complete for pinned play: both dz signs, any multi-unit sink. The target never
     rises, because the opposite point is never a neighbour.
   - So the only rule freedom the engine excludes is free waits.
2. **`fnrpa.cpp`**: single-thread NRPA over the extended move set.
   - Altitudes are scaled by `--sc` (2 = half units).
   - It generates pinned moves exactly as `eng.h` does.
   - With `--free 1` it adds free sinks of `--fstep..--fmax` scaled units, each followed by an in-group
     knight move.
   - Candidate bests must pass `free_ok`. It sends each free sink an infinitesimal +e. If the tour
     still works, the candidate is rejected and scored as unfinished. This rules out giant-group
     "pumps", where every later move stays inside one group. It is stricter than the checker, which
     also accepts if 0, t+1 or 2t break the tour.
   - `--warm K` seeds the weights K times toward the seed suffix and starts from it as the best.
   - The anchor k is drawn uniformly from `--amin..--amax` (clamped to the seed length - 1).
   - It prints its argv, writes exact fractional minutes on line 1, and `--pool` appends every finished
     tour at or above `--pool-min`.
3. **Python tools** (run from the repo root):
   - `classify.py FILE`: pinned, zero and free waits; fractional sinks.
   - `repin.py FILE MOVE [SINK]` (or `--example MOVE`): sets one free sink to k/6 units (k = 0..36),
     re-pins every later wait (same squares, same dz), and runs the checker. It is a rigid/floppy test
     and also a strictifier.
   - `nec_detail.py FILE|--example`: per free wait, which of the checker's probes break the tour,
     drop-to-0, and the working values on a 1/12 grid with the other waits fixed.
   - `strict_check4.py FILE|--example`: `subagents/strict_check.py` for any board, plus a
     "legal-at-0" flag.
   - `postproc.py POOL...`: for every pool tour, zeros each subset of up to 3 free waits, re-pins,
     and keeps the best variant that passes the checker and has no free waits.

Build: `g++ -O2 -std=c++17 -o subagents/fractional/tmp/fnrpa subagents/fractional/fnrpa.cpp`

## Exact commands
```
B=subagents/fractional/tmp/fnrpa
# round 1
$B --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 2 --time 180 --amin 35 --amax 120 --out subagents/fractional/tmp/w1.txt   # 8,176 (rng 1)
$B --sc 1 --free 1 --fstep 1 --fmax 4 --warm 10 --level 3 --time 1200 --amin 35 --amax 110 --rng 7 --out subagents/fractional/tmp/r1.txt   # 8,176 again (restart 1, anchor 45); killed after 2 restarts
bash subagents/fractional/campaign.sh    # round 1: seeds 8,123 / 8,130, level 2, 240 s each (sc2 r2 lost to the container restart)
# round 2
bash subagents/fractional/campaign2.sh   # seed 8,211: free vs strict A/B, 5 rngs x 240 s each, then 2 x sc=2
bash subagents/fractional/campaign3.sh   # (stopped after the ruling; never ran)
bash subagents/fractional/campaign4.sh   # seeds 8,264/8,235/8,239 (stopped after its first runs: 8,288 found)
bash subagents/fractional/campaign5.sh   # seed 8,288 (8,292 at g_all_r32)
bash subagents/fractional/campaign6.sh; bash subagents/fractional/campaign7.sh   # seed 8,292 (interrupted by the container restart)
bash subagents/fractional/campaign8.sh   # round 3: seed 8,329, 5 seeds x 240 s
bash subagents/fractional/campaign9.sh   # round 3: 8,329 with sc 2 (2 seeds) and level 3 for 600 s
bash subagents/fractional/campaign10.sh  # round 3: lns's 7,200-family tour (7,897), anchors 6.., 3 seeds
bash subagents/fractional/campaign11.sh  # round 3: iterated warm L2 on the 7,200 family
python3 subagents/fractional/sweep_many.py subagents/fractional/tmp/sweep_in.txt   # insertion sweep over the pool tours
python3 subagents/fractional/insert.py data/tours/record_8329.txt [OUT] [--flex]   # insertion sweep
python3 subagents/fractional/postproc.py subagents/fractional/tmp/pool*.txt
python3 subagents/fractional/strict_check4.py --example
python3 subagents/fractional/nec_detail.py --example
python3 subagents/fractional/repin.py --example 5
```

## Results
Every run is level 2 with `--warm 10`, 240 s on 1 thread, anchors 35..(seed length - 1). The restart
counts are completed restarts.

### Round 1 (seed 8,123 or 8,130)
| run | best | restarts | spread of finishes (count x minutes) |
|---|---|---|---|
| 8123 free sc1 rng1 | 8,130 | 152 | 130x8127, 15x8123, 7x8130 |
| 8123 free sc1 rng2 | 8,130 | 174 | 157x8127, 9x8130, 8x8123 |
| 8123 free sc1 rng3 | **8,176** | 251 | 221x8127, 16x8123, 12x8130, 2x8176 |
| 8130 free sc1 rng1 | 8,137 | 183 | 164x8134, 11x8130, 8x8137 |
| 8130 free sc1 rng2 | 8,137 | 229 | 212x8134, 10x8130, 6x8137, 1x8133 |
| 8130 free sc1 rng3 | 8,137 | 328 | 290x8134, 22x8130, 16x8137 |
| 8130 free sc2 rng1 | **8,183** | 327 | 300x8134, 18x8130, 5x8133.5, 3x8137 |

- The **+53 mechanism**: a rigid free sink at a1, (23, b3) with sink 1. a1 is opposite h8, so h8 also
  rises 1.
  - That shifts which group h8 ends in.
  - The late zero-wait hop (0, g8) turns into a sit.
  - The finish improves (64 vs 51), and the dance sits re-balance.
- It appears in 3 of 7 runs (2/3 from 8,123, 1/4 from 8,130).
- The common +4 is the floppy free sink at the f6 -> g8 hop.
- Half-unit sinks (sc 2) found 8,183, but its free wait is a whole unit; no fractional sink was ever
  part of a best tour.

### Round 2 (each 240 s, level 2, warm)
| seed | run | best | restarts | spread of finishes |
|---|---|---|---|---|
| 8211 | free r1 | **8,264** | 447 | 389x8215, 41x8239, 16x8211, 1x8264 |
| 8211 | free r2 | 8,239 | 486 | 407x8215, 52x8239, 25x8211, 2x8214 |
| 8211 | strict r1 | 8,235 | 542 | 394x8211, 148x8235 |
| 8211 | strict r2 | 8,235 | 398 | 282x8211, 116x8235 |
| 8264 | free r21 | **8,288** | 472 | 413x8264, 59x8288 |
| 8235 | free r21 | 8,239 | 73 (killed) | 70x8239, 3x8235 |
| 8288 | free all r31/r32/r33 | 8,288 / **8,292** / 8,288 | 444/655/420 | r32: 1x8292 |
| 8288 | free early (anchors 35-80) r34/r35 | 8,288 / 8,288 | 338/235 | all 8288 |
| 8288 | free level 3, 600 s, r36 | 8,288 | 10 | all 8288 |
| 8292 | free r41/r42 | 8,292 / 8,292 | 481/7 (killed by the container restart) | all 8292 |

At equal time from 8,211 (2 seeds each): free reached 8,264 and 8,239; strict reached 8,235 twice. The
strict runs were stopped after the ruling.

### Round 3
- **Insertion sweep** (`insert.py`, with and without `--flex`) over every in-group zero-wait hop:
  - each hop gets a free sink of s = 1/2..6 units, with every later wait re-pinned;
  - pairs are built from the best singles;
  - `--flex` also tries the other dz sign when the original one would need a negative sink.

  | tour | in-group zero-wait hops | improving singles | improving pairs |
  |---|---|---|---|
  | 8,329 | 23 | none | none |
  | 8,297 | 15 | none | none |
  | 8,292 | 21 | none | none |
  | 8,288 | 22 | none | none |

  Every insertion either breaks a later jump or needs a negative later wait. With the squares fixed, the
  records are rigid: a new free sink only pays when the following squares change too, and that needs
  search.
- **Warm level-2 from 8,329** (`campaign8.sh`, 5 seeds x 240 s): every finish was 8,329 in all 5 seeds
  (473/420/694/440/420 restarts).
- **Variants** (`campaign9.sh`): sc 2 half-unit sinks gave 8,329 in 2 seeds (408/624 restarts), and
  level 3 for 600 s gave 8,329 (10 restarts). The record is a fixed point of warm free-sink NRPA.
- **Insertion sweep over the prior/lns pools** (`sweep_many.py`: 32 distinct tours of at least 8,250, plain
  and `--flex`):

  | base | after the sweep |
  |---|---|
  | strict 8,295 | 8,299 |
  | 8,291 | 8,295 |
  | 8,290 (x3) | 8,294 |
  | 8,278 | 8,286 |
  | 8,259 | 8,263 |
  | 8,255 | 8,259 |
  | 8,095 (prior a30-45 r3) | 8,142 with `--flex` (8,111 plain) |
  | 8,330, 8,329, 8,309, 8,297, 8,294, 8,293, 8,288 | unchanged |

  The usual +4 is the floppy f6 -> g8 endgame sink. Nothing beats 8,330.
- **The 7,200 family** (lns `tmp/r9_s81_best.txt`, 7,897, which leaves 8,123's climb at move 6;
  `campaign10.sh`, anchors 6..end, warm L2, 3 seeds x 240 s):
  - **7,967 in 3/3 seeds** (397-422 restarts each); finishes are about 60% 7,912, 30% 7,908, and 2-3
    restarts per seed at 7,967.
  - The free waits are placed by the search: 7,967 has 4 (a2, c4, f2, f7), against 3 in the seed.
  - Saved as `tmp/n_r71.txt`.
  - `campaign11.sh` then iterates warm L2 (5 x 240 s, each run seeded with the best so far); the result is
    in the next line.


## What failed
- Fresh (non-warm) free-sink NRPA is poor: the free moves dilute the policy and lead into pumps
  (anchor 40, level 2: 2,729, vs 5,005 pinned-only).
- Level 3 with free sinks costs about 250 s per restart (the necessity filter dominates), so the A/B uses
  level 2.
- Without `free_ok`, the search "wins" with giant-group pumps (11,465). The checker rejects those
  (72 unnecessary waits).

## Recommendation (next round)
- **Warm L2 and L3 from a record converge at once** (every finish equals the seed). Insertion with
  the squares fixed finds nothing. New gains need **new squares around a free sink**:
  - Run guided or fresh NRPA from merge and early-dance anchors with free moves on (prior's guided
    search plus the `--free 1` move set).
  - Then run `postproc.py` and `insert.py` on every pool tour.
- **The a1 free sink (+53) has been found three times** in 3 independent bases. Free sinks at points
  whose opposite is h8 or another group-boundary point are the high-value pattern. A targeted generator
  could offer "free sink at P where opp(P) is in the other group" first.
- Keep `free_ok` (+e only) as the in-search filter. The checker decides.
