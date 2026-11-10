# Review of `polish/` (reviewer rev_code_b, 2026-10-31, round 1b)

Code reviewed: `subagents/polish/polish.cpp` (in progress; no RUN.md yet). I built my own copy with
`g++ -O3 -march=native -std=c++17 -pthread` and ran it from my scratch dir.

## 1. Status and claims checked
- **Replays the record exactly, verified.** On `data/tours/record_8123.txt` it reports 8123 minutes and
  135 jumps. All 4 reroute variants come back "unchanged", so 8123 is already visit-minimal under this
  re-routing.
- **Window pass on 8123 (`--time 40 --threads 1`).**
  - 648 of 1,187 windows done in 40 s (~105k nodes/s).
  - No gain. That is not evidence against the method: only half the windows were tried.
- **lns 8127 (`--time 30`).**
  - A w=1 window cut 143 to 141 jumps at the same 8127 minutes.
  - The output passes `tools/check_tour.py` (ok=True, maxvisits=3, unnecessary=[]).
- **Are later waits re-derived correctly? Yes, mechanically.**
  - `replay` keeps each later key move's square and its post-jump dz. It recomputes the sink as
    `delta = dz - (alt[q] - alt[p])` against the current altitudes, flipping the sign of dz if delta
    would be negative.
  - `apply_move` then charges `n × delta` with the current group size, including the opposite-point rise.
  - Every candidate goes through `validate()`, which checks pinned waits, caps, h8 only at the end, and
    the minutes, and is accepted only if the minutes match.
  - So the polisher cannot emit an invalid tour. It can only miss improvements.

## 2. Issues
**MAJOR: the search neighbourhood is too small for the gains that actually exist.**
- The 8123 → 8127/8130 gains (lns) change the **merge region**: the sit skeletons differ from move ~31
  on, with different sits at b7, d8 and a6 and a different dance order. That is a 10–20-move
  restructuring of the middle.
- `--wmax 8 --ext 2` (at most 10 replacement moves, depth-limited DFS) and tail-only NRPA cannot express
  that.
- Fix: add a **middle-window NRPA**:
  - keep the prefix up to i;
  - run NRPA level 2–3 over moves i..i+W (W = 15–30);
  - score each rollout by replaying the remaining skeleton with `replay()`.
  This is what lns does successfully. Doing it inside the polisher gives it the same reach, with exact
  wait re-derivation.

**MAJOR: "keep dz" is one fixed repair rule.**
- After an inserted or removed sit the dance may need the *other* group to be the upper one, and
  keeping dz then fails or flips arbitrarily.
- Fix: try both dz signs per key move inside a small beam (2^k is fine for k ≤ 6 key moves after the
  window), or re-plan the first 2–3 tail sits with the DFS before replaying the rest.

**MINOR**
- **Same-minute improvements not saved.** `report()` writes `--out` only when `T > GLOBAL_BEST`, so a
  same-minute, fewer-jump tour (like the 141-jump 8127) is not saved to best.txt, only to the pool. The
  file is then a worse seed for the next stage. Use `better(T, n, ...)`.
- **`--polished` appends.** Re-runs accumulate duplicates. Truncate it at start or document this.
- **Transposition-table pruning.** Hash collisions in the window TT (`hashS`, 64-bit) are treated as
  equality. The rule itself (stored T ≥ current and stored depth ≤ current) is sound, so this only
  costs completeness. Low risk, but say so in RUN.md.
- **Window order.** Tasks run w=0 first for every i, then w=1, and so on, with first-improvement
  restarts. With a fixed `--time`, the large windows (where the value is) are rarely reached. Put
  w=4..8 first around the merge (moves 25–60).
- **Strict input parsing.** `parse_line` needs exactly `(%d, sq)` with one space. Fine for our files,
  but say so.

## 3. Recommendation: CONTINUE, with a focus shift
It is a correct and useful post-processor. Every engine's pool should go through it (cheap, safe). As a
record-beater its current neighbourhood is too local. Expected value goes up a lot with middle-window
NRPA and a merge-region-first schedule.

## 4. Next round
1. **Middle-window NRPA:** windows starting at moves 25..60, W ∈ {15, 25}, L2, ~60 s each, on 8123 and
   8130, 1 thread. Success: it reproduces a ≥8,127 tour from 8123. lns did this, so it is a fair
   recall test.
2. **Merge-first window schedule:** run `--wmax 8 --ext 3` restricted to moves 25–60 for 10 min on 8123
   and 8130. Success: any gain, or a full pass over all windows reported with nodes and time, which
   rules out this neighbourhood properly.
3. **RUN.md:** write it with the smoke numbers above, plus a full-run command (`--in` the merged pool
   ≥8,000, `--top 20`, `--per-tour 600`, `--threads 1`).
