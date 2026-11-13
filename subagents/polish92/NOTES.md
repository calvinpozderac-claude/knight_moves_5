# polish92: polish + tail proof + focused LNS on the 9,275 line (Round 9)

Goal: a tour above 9,275. 1 heavy thread, nice -n 5. Start 23:17 (2026-11-13).

## Tools (this folder)
- `bin/lns_r6`, `bin/polish`, `bin/bb`, `bin/dlns`: copies of `subagents/families/bin`. All replay 9,275 / 9,271 /
  9,268 exactly (bb k=179: FOUND 9275; lns_r6/dlns: replay 9275/9271/9268; polish: input 9275).
- `pol.sh TAG IN TIME [WMAX]`, `lns.sh TAG SEED ITERS START LO HI P DONORS...` (`--free 0`), `exact.sh TOUR TAG TIME
  [STEP]` (bb strict tail proof, writes `focus_TAG.txt`), `polish92_saver.sh` (pol/, lns/, keep/ above 9,275 that
  check_tour accepts -> `NEW_RECORD_<m>.txt`; heartbeat `saver.alive`). Event log `events.log`.
- Inputs `in/record_9275.txt`, `in/record_9271.txt`, `in/record_9268.txt` (all check_tour ok, maxvisits 3, unnecessary=[]).

## Log
- 23:20 polish P9275 (720 s, wmax 12) started; saver running.
