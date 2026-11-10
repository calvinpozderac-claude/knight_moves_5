# f5push: push family F5 (record 8,812) - Round 8

Goal: a tour above 8,812. 2 heavy threads, nice -n 5. Start 18:30.

## Tools (this folder)
- `bb.cpp` + `common.h` (copy of lns84's free-wait-aware bb; rebuilt: `g++ -O2 -march=native -o bin/bb bb.cpp`).
- `bin/lns_r6`, `bin/polish`: copies of families' binaries.
- `exact.sh TOUR TAG TIME [STEP]` (log `exact.log`, writes `focus_TAG.txt`), `lns.sh` / `pol.sh` (families'
  scripts, paths changed; lns default `--free 0`), `saver.sh` (any `pol/*_best.txt` / `lns/*_best.txt` above
  8,812 that check_tour accepts -> `NEW_RECORD_<T>.txt`; heartbeat in `saver.alive`). Event log `events.log`.

## Log
- 18:31-18:38 **Exact tail proof on 8,812** (`exact.sh in/r8812.txt E8812 240`, step 2): optimum 8,812 for
  k = 171 ... 153 (<= 0.6 s each), **k = 151 proven** (156 s, 128M nodes, prefix T 7,159); k = 149 timed out
  (240 s, 192M nodes, nothing better). The last 25 moves of 8,812 (from move 151: the d6/d7 / e7/e8 / f8/g8
  end of the dance) are optimal for its prefix (strict continuations). LNS focus 1..150.
- 18:31-18:41 polish of main's raw 8,595 (F1 variant, 600 s, wmax 12): 8595 -> 8596 -> 8600 (w=6 at 103)
  -> **8,604** (time up, 156 jumps).
- 18:46 the first saver's heartbeat stopped at 18:32 although the process lived; restarted it (absolute path
  D=...), heartbeat ok.
- 18:43-18:50 **Exact tail proof on 8,810 e28e09e8** (175 moves): k = 171 ... 153 fast, **k = 151 proven** (81 s,
  68M nodes, prefix T 7,157), k = 149 timed out (240 s, 200M nodes, nothing better). Same proven tail as 8,812:
  the 8,812 and this 8,810 differ only before move 151 (prefix T 7,159 vs 7,157).
- LNS queues (`qA.sh`, `qB.sh` -> `seedq.sh 60000 SEEDS`): start = best checked tour, donors = `don/` (8,812,
  the three 8,810s, families' F5 NEW_RECORD 8,795-8,809) + own NEW_RECORDs, focus 1 150 0.85, `--free 0`,
  60k iterations, otherwise families' flags; new bests polished 600 s wmax 12 and added to `don/`.
  Core B: seeds 501, 503, 505. Core A: S8595 side LNS (s601, 70k / 900 s) then seeds 502, 504, 506.
- 18:49-19:04 side task S8595 s601 (start = polished 8,604, donors raw 8,595 + polish pool, focus 1 135 0.85,
  900 s, 51k it): **8,604, nothing**. The F1 variant (extra b2/b3 a3/a4 block) stays below F1's 8,609.
- **19:00 LNS F5 s501 (from 8,812): 8,812 -> 8,815 (op 1 i=120) -> 8,816 (op 2 i=15) -> 8,823 (op 2 i=128) ->
  8,824 (op 0 i=18) -> 8,827 (op 0 i=96) -> 8,828 (op 2 i=37) -> 8,829 (op 1 i=38), its 35-42k.**
  `NEW_RECORD_8829.txt`: 168 moves, check_tour ok=True maxvisits=3 unnecessary=[], strict_check STRICT.
  Its dance changed in the left-bottom part: sig ... a4/b4 a2/a3 a2/b2 b2/b3 **a3/a5 a5/b5 b3/c3 c3/d3** c2/c3
  d3/d4 **d4/d5** d6/d7 e7/e8 f8/g8 (84 sits, was 82); the merge also changed (g3 120 instead of h1 120).
  Saver wrote 8,824 / 8,827 / 8,828 / 8,829 on the way. Polish of 8,829 started 19:07 (600 s, wmax 12).
- Polish of 8,829 (PF5_s501, 600 s, wmax 12): reroute only, 8,829. LNS F5 s502 (from 8,829, 60k): nothing
  above 8,829.
- **19:36 LNS F5 s503 (from 8,829): 8,830 at it 44,674 (op 2 i=18 D=6, climb)**. `NEW_RECORD_8830.txt`, 173 moves,
  check_tour ok=True maxvisits=3 unnecessary=[], STRICT. Same dance signature as 8,829 except c2/c3 and c3/d3
  swapped.
- LNS F5 s504 (from 8,829, 60k): nothing above 8,829. Polish of 8,830 (PF5_s503, 600 s, wmax 12): reroute only.
- 19:52-19:57 **Exact tail proof on 8,830** (173 moves): k = 169 ... 153 fast, **k = 151 proven** (50 s, 65M
  nodes, prefix T 7,257), k = 149 timed out (240 s, 320M nodes, nothing better). Last 23 moves optimal for
  the prefix. Focus stays 1..150.
- 19:53 s505 and 19:57 s506 started from 8,830 (60k each).
- s505, s506 (from 8,830, 60k each, s506 with the 8,830 proof's focus): nothing above 8,830. All jobs and the
  saver stopped 20:14.

## Summary
- **Best: 8,830** (`NEW_RECORD_8830.txt`, 173 moves, STRICT; check_tour ok=True maxvisits=3 unnecessary=[]),
  +18 over 8,812. Path: LNS s501 from 8,812 -> 8,829 (seven steps at it 35-42k, ops in the climb i=15-38 and
  the dance i=96-128; the bottom-left of the dance was reordered: a3/a5 a5/b5 b3/c3 c3/d3, 84 sits) ->
  s503 from 8,829 -> 8,830 (op 2 i=18, climb).
- Tail proofs (bb, strict continuations, 240 s per k, step 2): 8,812 proven from move 151 of 175, the 8,810
  e28e09e8 from 151 of 175 (same tail, prefix 2 lower), 8,830 from 151 of 173; k = 149 timed out in all three.
- Seed spread (60k each, focus 1-150 p 0.85, --free 0, F5 donors only): s501 8,812 -> 8,829; s502 8,829 ->
  8,829; s503 8,829 -> 8,830; s504 8,829 -> 8,829; s505, s506 8,830 -> 8,830. Distinct finishes above 8,812:
  one 8,829, one 8,830 (plus the intermediate 8,824/8,827/8,828 of s501). Polish (600 s, wmax 12) of 8,829 and
  8,830: reroute only.
- Side task, main's 8,595 (F1 variant): polish -> 8,604, LNS 51k -> 8,604. Below F1's 8,609 ceiling.
- Lesson: one fresh seed from 8,812 found +17 in ~40k iterations, while four seeds from 8,829/8,830 found +1
  at most; F5 now looks close to its local ceiling at ~8,830.
