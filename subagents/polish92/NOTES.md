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
- Polish (wmax 12, ext 3/4, node limit 3M, nrpa L2 tails 45/60/90): **no gains**.
  - 9,275: converged at 358 s, 9,275 (reroute to 175 jumps, checker ok). 
  - 9,271: 600 s (time up), 9,271 (reroute to 175 jumps).
  - 9,268: converged at 377 s, 9,268 (reroute to 175 jumps).
  These LNS tours are already polish-optimal (single tail sit insertions etc. do not help).
- 23:39 exact tail proofs (bb, strict, step 2): 9,275 polished (240 s/k), then 9,271, 9,268 (150 s/k).
- Exact tail proofs (bb `--nobound --alpha 0`, strict continuations, step 2; indices on the polished 175-jump tours):
  - 9,275 (pol/P9275_best): k = 171 ... 157 fast, k = 155 42.7 s, **k = 153 proven** (92 s, 167M nodes, prefix T
    7,672); k = 151 timed out (240 s, 431M nodes, nothing better). Focus 1..152.
  - 9,271 (pol/P9271_best): **k = 151 proven**, k = 149 timed out (150 s).
  - 9,268 (pol/P9268_best): **k = 151 proven**, k = 149 timed out (150 s).
- 23:55 LNS queue (`seedq.sh 60000 1 152 921 922 923 924`): start = pol/P9275_best (175 jumps; the proof's
  indexing) or any higher tour found, donors `don/` = the three raw tours + their polished reroutes (+ new bests),
  focus 1..152 p 0.85, `--free 0`; every new best polished 600 s and fed back to `don/`.
- s921 (from P9275, 60k, 10.3 min; ~5.8k it/min): **9,275, nothing**; elite all 9,275.
- Queued q3.sh after q2: 150k-iteration LNS from P9271 (s925) and P9268 (s926), focus 1..150 (their proofs), same donors.
- s922, s923 (from P9275, 60k each): **9,275, nothing** (elite all 9,275). s924 reached 20k (9,275) when the
  container restarted (~00:29 container clock); log kept as `lns/L_s924_killed.log`.
- 00:31 restart: saver restarted; donors + penalty92's two distinct 9,275s (`don/pen9275_b8a997d6.txt` = its
  A_S_c3d3_L1b, 177 moves; `don/pen9275_23904b34.txt` = A_S_c3d3_L2, 179 moves; both checker ok).
  q4.sh: s924 rerun (60k, P9275, focus 1..152), s927 (60k from pen b8a997d6, focus 1..154), s925 (150k from P9271,
  focus 1..150), s926 (120k from P9268, focus 1..150).
- 00:42 CPU contended (4/4 cores busy, ~3.5k it/min): s925 cut to 80k, s926 to 60k (same-length in-place edit of q4.sh).
- s924 rerun (60k from P9275, with the penalty92 donors): **9,275, nothing**.
- s927 (60k from penalty92's 9,275 b8a997d6, focus 1..154): **9,275, nothing**. 01:03 s925 (80k from P9271) started.
