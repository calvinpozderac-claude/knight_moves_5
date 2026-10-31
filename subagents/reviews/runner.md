# Review of `runner/` (reviewer rev_code_b, 2026-10-31, round 1b)

Code reviewed: `subagents/runner/runner.py` (in progress, ~1,100 lines; no RUN.md yet).

## 1. Status and claims checked
- **Collector, verified.** Ran
  `nice -n 15 python3 subagents/runner/runner.py --out $SCRATCH/out --tmp $SCRATCH/tmp --queue $SCRATCH/queue.txt collect --max-seconds 40`.
  It scanned 389 files and 2,937 tour lines and did 34 exact checks in 44 s (about 1.2 s per check). It
  stopped cleanly at the budget and wrote LEADERBOARD.md and global_best.txt.
- **Exact verifier, verified.** `verify()` imports `tools/check_tour.check`, which uses exact Fractions.
  It requires `ok and maxvisits<=3 and unnecessary==[]`. Before any record alert it also runs the
  checker CLI as a second check. An unverified or failed candidate never becomes the global best. The
  move regex is the checker's own. Lines whose waits the checker can't parse (decimals) are counted as
  "bad" rather than dropped silently. Good.
- **Scheduler, verified.** Ran with a scratch queue (`run --exit-when-idle --cores 3`):
  - a job that traps SIGINT stopped at time+grace (status `timeout`, rc 124);
  - a job that ignores SIGINT was SIGKILLed 30 s later (rc 137);
  - an `after=` dependent was reported as "blocked by failed deps".
- **lns 8127, re-checked by me.** The lns `NEW_RECORD_8127.txt` (since superseded by 8130) passes the
  checker, and every positive wait in it is pinned.

## 2. Issues
**BLOCKER: the leaderboard crowns a disputed tour as "Global best verified: 8176 -- NEW RECORD!" and
writes a RECORD_ALERT for it.**
- The tour is `subagents/fractional/CHECKER_ONLY_8176.txt`, also found in `fractional/tmp/w1.txt`.
- Move 96 of that tour, `(23, b3)`, is a **free wait**: the jump to b3 is a dz=0 knight move inside the
  knight's own group, so it is legal at any wait. `fractional/classify.py` shows this.
- The checker accepts it only because changing the wait breaks later moves. The fractional agent itself
  labels it checker-only. Under the README rule "every wait must be necessary" it is at best disputed.
- The runner is the place that decides what gets announced, so this must not reach the headline.
- Fix: add a `strict` flag to `verify()`. Every positive wait must change the jump it precedes: the
  target must not be at the knight's altitude before the wait, which is the engine's pinned model.
  Integer-only waits could be a further flag.
  - The headline, `global_best.txt` and record alerts should use strict-verified tours only.
  - Show checker-only tours in their own leaderboard column, marked "rule-disputed".
  - Don't key this on file names.
  - Test: 8176 goes to the disputed column; 8123, 8127 and 8130 stay strict.

**MAJOR: the alert threshold is the static `--record 8123`.**
- lns is now at 8130 and writes many snapshots, and every distinct tour above 8123 fires a banner.
- Fix: alert when a tour beats max(record, current strict global best). Keep one "new best" history
  line per improvement.

**MAJOR: claimed minutes are never compared with the checker's T.**
- The score shown is the sum of the parsed waits. The file's own line-1 or `minutes<TAB>` value is
  ignored, and `verify()` returns T but nothing compares it.
- Fix: record `claimed`, `sum(waits)` and the checker's `T`, and flag any mismatch. A mismatch means an
  engine or minutes bug in the agent that wrote the file, which is cheap to catch here.

**MINOR**
- **Missing `add` options.** `add` lacks `--grace`, `--retry-delay`, `--retry-on-timeout` and `--env`
  (`add ... --grace 1` fails with "unrecognized arguments"). Only the JSON queue lines support them.
- **Timeouts block dependents.** A search job that ran its whole budget and was interrupted cleanly gets
  status `timeout` (rc 124 from `timeout`), which blocks `after=` dependents. Consider an
  `after_ok=done,timeout` option.
- **Core count ignores other agents.** The default `--cores` is nproc (4), which ignores the 5 agents
  already searching. Make `--respect-load` the default, or require `--cores`.
- **Collector priority.** The collector runs at `nice 10`. Use 15: a full collect is several CPU-minutes
  (400 checks × ~1.2 s).
- **Thread count is not enforced.** `threads` is trusted metadata. Warn when the command contains
  neither `{threads}` nor `--threads`.
- **No RUN.md.** Also missing: an example queue with real jobs (nrpa L4 anchors 35..46, macro, polish
  over the merged pool).

## 3. Recommendation: CONTINUE
The core of the runner is solid: exact re-verification, refusal to promote unverified tours, and a
working scheduler. Fixing the strict/disputed split matters more than any new feature. Without it the
orchestrator could announce a record that a rules-minded reader would reject.

## 4. Next round
1. **Strict/disputed split** (≤30 min of coding; the test takes seconds). Success: on the current repo,
   the headline is the best strict tour (lns 8130 or later), 8176 is shown as disputed, and no
   RECORD_ALERT is written for disputed tours.
2. **Minutes cross-check and dynamic alert threshold.** Success: a hand-made file with a wrong line-1
   value is flagged, and a second 8130 snapshot does not fire a banner.
3. **RUN.md plus a sample queue** for the first free-core window: nrpa L4 fresh at anchors 35–46, then
   polish of the merged pool, each with `--threads 1` and `--time`. Success: `run --once` launches them
   and `status` shows them.
