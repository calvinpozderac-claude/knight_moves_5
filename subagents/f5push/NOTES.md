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
