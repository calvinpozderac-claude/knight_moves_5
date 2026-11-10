# fw84: free waits on 8,484 and 8,337

## Setup
- `fnrpa.cpp` = copy of `subagents/fractional/fnrpa.cpp` (includes the new `src/eng.h`).
  Build: `g++ -O2 -std=c++17 -o subagents/fw84/tmp/fnrpa subagents/fw84/fnrpa.cpp`
- Replay check: `./tmp/fnrpa --seed ../../data/tours/record_8484.txt --time 0 --free 1` prints
  "161 moves, 8484.000 minutes"; record_8337 -> "164 moves, 8337.000". Checker on 8,484: ok=True maxvisits=3 unnecessary=[].
- `classify.py` on 8,484: 115 pinned waits, 46 zero waits, no free waits (strict).
- `fw_multi.sh T "RNGS" UNTIL TAG AMIN AMAX SEED...`: fw pass (warm 10, level 2, free sinks 1-4 units), pool >= 8300,
  then `newrec.py` checks every distinct pool tour above 8,484 with the checker and writes NEW_RECORD_<m>.txt.
- `sweep84.py FILE OUTPREFIX [ROUNDS]`: exhaustive insertion sweep (see the docstring).

## Runs (all from subagents/fw84, 1 thread, nice -n 5)

### Step 1-2: fw pass on 8,484, 16 rngs (tag p1)
`bash fw_multi.sh 120 "1 2" UNTIL p1 35 999 tmp/seed8484.txt` then (to save time, since every finish was the seed)
`bash fw_multi.sh 60 "3 4 ... 16" UNTIL p1 35 999 tmp/seed8484.txt` (warm 10, level 2, free sinks 1-4).
- **16/16 rngs -> 8,484.** 2,744 finishes (126-278 restarts per rng), every one exactly 8,484.000; zero unfinished.
- Distinct finishes: 7 tours, all 8,484 (the seed 2,737x plus 6 equal-score variants, 1-3x each), saved as
  `tmp/alt8484_{1..6}.txt`, all checker ok. Variants reroute moves 59-67 (g4/h2/f1/g3/f5/d6 block) or play
  (76, c3) instead of (76, d2) at move 141. alt2, alt3, alt5 contain one free wait each, at equal minutes.

### Step 3: exhaustive insertion sweep on 8,484
`python3 sweep84.py tmp/seed8484.txt tmp/sw8484 4` (868 options: 18 in-group zero-wait hops x 48 sinks k/6, k=1..48,
plus 4 pinned alternative sinks at out-of-group zero-wait hops; plain and flex re-pinning).
- Only 14 (option, mode) pairs give a legal tour, all exactly 8,484: move 28 (h3->g5) free sink 1/6..1 unit
  (the later waits compensate, the total is unchanged), and move 55 sink 4. Every other insertion breaks a later jump
  under re-pinning. **8,484 is rigid with its squares fixed**, like 8,329/8,297 in the old basin.
