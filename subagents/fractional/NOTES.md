# fractional: free (and fractional) waits on top of the strict engine

## Headline (round 2)
- **New strict record: 8,235** (`NEW_RECORD_8235.txt`, also `best.txt`).
  - `check_tour.py`: `ok=True maxvisits=3 unnecessary=[]`. `strict_check.py`: STRICT.
  - It is 8,211 with the dance re-ordered from move 96: the d1/e1 block is visited after the c1 sit
    instead of before c5. That is +24, all pinned.
  - How I first got it: the free-wait search found 8,239 (one floppy free wait at move 153). Setting
    that wait to 0 and re-pinning later waits gives 8,235 (`repin.py ... 153 0`).
  - It is not specific to free waits: the strict control run (`--free 0`, same settings) finds it in under
    5 s. Warm level-2 NRPA from 8,211 finds it right away.
- **Free-wait scoreboard** (kept separate, see RULES.md): **8,239** (`FREEWAIT_8239.txt`), 8,183
  (`FREEWAIT_8183.txt`, base 8,130), 8,176 (`FREEWAIT_8176.txt`, base 8,123).
  - The 8,183 and 8,176 free waits are *rigid*: only one value works, even when later waits are
    re-pinned. They are like the 4x4 example's.
  - The +4 free wait in 8,239 is *floppy*: every s in [0,4] works after re-pinning.

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
bash subagents/fractional/campaign3.sh   # seed best.txt (8,235): strict vs free, 3 rngs x 240 s each
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

### Round 2 (seed 8,211) -- see the table filled in below

## What failed
- Fresh (non-warm) free-sink NRPA is poor: the free moves dilute the policy and lead into pumps
  (anchor 40, level 2: 2,729, vs 5,005 pinned-only).
- Level 3 with free sinks costs about 250 s per restart (the necessity filter dominates), so the A/B uses
  level 2.
- Without `free_ok`, the search "wins" with giant-group pumps (11,465). The checker rejects those
  (72 unnecessary waits).

## Recommendation
- Treat free waits as legal only if the orchestrator accepts the 4x4-example argument (RULES.md), and
  even then report them separately.
- Run `postproc.py` on every free-wait pool: floppy free waits re-pin to strict tours, and that is how
  8,235 surfaced.
- Re-run warm level-2 strict NRPA from every new strict record. It found +24 in seconds, so the dance
  of 8,211 was not at a local optimum.
