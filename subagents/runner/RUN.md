# runner: job queue, scheduler, verifier, leaderboard

`runner.py` (Python 3, stdlib only, nothing to build) runs everyone's searches once cores are free,
then collects and re-verifies every tour they produce. It reads `tools/check_tour.py` / `tools/sim.py`
and does not change them.

```
R="python3 subagents/runner/runner.py"          # run from the repo root (any cwd works)
$R add NAME [--threads N --time S --nice K --cwd DIR --pool FILE --retries R --after a,b --priority P] -- CMD...
$R run [--cores N] [--exit-when-idle] [--respect-load] [--kill-on-exit] [--collect-every 300]
$R status                                       # running jobs (CPU%, RSS, last log line), queue, best so far
$R collect [--full] [--verify-ties]             # scan + re-verify + LEADERBOARD.md (the scheduler also runs it)
$R cancel NAME... | --all [--kill]              # SIGINT (or SIGKILL) to the job's processes
$R retry NAME...                                # put a failed/cancelled/done job back to pending
```
Global options go before the subcommand: `--queue` (default `subagents/runner/queue.txt`),
`--out` (LEADERBOARD/global best, default `subagents/runner/`), `--tmp` (state, logs, caches, default
`subagents/runner/tmp/`), `--root` (repo to scan), `--record 8123`, `--scan PATH` (extra places to scan).

## Queue (`queue.txt`)
One job per line, JSON or shorthand `key=value ... -- command`, `#` comments. The file is re-read every
poll (2 s), so jobs can be appended (`add` or an editor) while the scheduler runs. Keys:
`name` (unique), `cmd`, `threads` (1), `time` (3600 s, the program budget), `grace` (60), `nice` (5),
`cwd` (repo-relative), `pool` (relative to cwd), `agent`/`method` (leaderboard labels; agent defaults to
the `subagents/<x>/` of cwd/pool, method to the job name), `retries` (0), `retry_delay` (30),
`retry_on_timeout` (false), `priority` (0, higher first), `after` (jobs that must be `done` first), `env`.
`cmd` may use `{threads} {time} {pool} {name} {root} {cwd} {logdir}`. Example:
```
{"name": "lns_s7", "cmd": "./lns --threads {threads} --time {time} --seed 7 --pool {pool}", "cwd": "subagents/lns", "pool": "tmp/pool_s7.txt", "time": 3600, "retries": 1}
name=nrpa_l4 threads=1 time=3600 cwd=subagents/nrpa pool=tmp/l4.txt -- ./gnrpa --level 4 --time {time} --out {pool}
```

## Scheduler (`run`)
- Keeps the sum of `threads` of running jobs <= `--cores` (default nproc). Launch order: priority, then
  queue order; smaller jobs fill gaps. A job asking for more than `--cores` runs alone.
  `--respect-load` also counts other processes' load (loadavg): use it while other agents share the box.
- Each job runs as `cd cwd; nice -n K timeout -s INT -k 30 (time+grace) bash -c CMD` in its own session,
  stdout+stderr to `tmp/logs/<name>.<attempt>.log` (`<name>.log` links to the latest), exit code to
  `<name>.<attempt>.rc`. Env: `RUNNER_JOB, RUNNER_THREADS, RUNNER_TIME, RUNNER_POOL, OMP_NUM_THREADS`.
- Status after exit: rc 0 `done`; 124/137 past the time limit `timeout` (counts as finished, retried only
  with `retry_on_timeout`); anything else `failed`, retried `retries` times after `retry_delay` s.
  Jobs whose `after` dependency failed are reported as blocked.
- Jobs are detached: stopping the scheduler (Ctrl-C/SIGTERM) leaves them running and the next `run`
  adopts them (state in `tmp/state.json`, rc files). `--kill-on-exit` stops them instead. Only one
  scheduler per tmp dir (flock).
- Runs `collect` (niced 10, in a subprocess) at start, every `--collect-every` s and after jobs finish
  (at most every `--collect-min-gap` 30 s). `--exit-when-idle` exits when nothing is left, after a final collect.

## Collector (`collect`)
- Scans every text file under `subagents/*/` (best.txt, NEW_RECORD_*.txt, pools, tmp/, job logs), `runs/`,
  `data/tours/`, every queue job's `pool`, and `--scan` paths. Any line with a tour `(w, sq), ...` ending at
  h8 is a candidate; its score is the sum of waits (exact Fraction; claimed numbers are ignored). Large files
  (> 1 MB) are read incrementally (append-only pools), small ones re-read when changed. Per file it keeps the
  top 25 plus everything >= 8123. Tours are deduplicated by content.
- Re-verification uses `tools/check_tour.py`'s `check()` (exact Fractions: legal, <= 3 visits, no
  unnecessary waits); results are cached in `tmp/collect_cache.json`, so every distinct tour is checked
  once. It verifies, highest first, every candidate down to the first verified tour, down to the first
  *strict* one, and the best of every agent/method (`--verify-ties`: also all tours tied with the best).
  A tour above the record is checked again with the checker's command line before any alert.
- **strict** flag: integer waits, integer altitudes, every wait pinned by its jump (the engine model).
  Tours with `free` waits (wait then a dz=0 jump inside the own group), non-integer waits or fractional
  altitudes pass the checker but may be checker-only (e.g. `subagents/fractional/CHECKER_ONLY_8176.txt`);
  they are flagged everywhere and kept apart in `global_best_strict.txt`.
- Outputs: `LEADERBOARD.md` (global best, global best strict, best per agent, best per agent/method with
  digits in file names folded to N, failed claims >= 7500, alert list), `global_best.txt` and
  `global_best_strict.txt` (line 1 minutes, line 2 tour; never go down), `tmp/best_history.log`,
  `tmp/alerts.log`.
- **Alert**: a verified tour > `--record` (8123) prints a `!!!` banner with a bell (stdout, and stderr if it goes
  elsewhere) and writes `RECORD_ALERT_<minutes>_<hash>.txt` (tour + checker output + sources) when it beats
  every earlier alert of its class (strict / not strict); other new tours > record get one line in the
  output and the alert log.

## Smoke tests (2026-10-31, machine fully loaded by other agents, load ~13 on 4 cores)
- Trivial queue (scratchpad, `--cores 3`): echo, a 2-thread sleep, `flaky` (fails twice, `retries 3`,
  done on attempt 3), `alwaysfail` (`retries 1`, failed after 2 attempts), `toolong` (`time 2 grace 1`, gets SIGINT
  at 3 s, its trap stops it cleanly, status `timeout` rc 124), a dependent job (ran after its deps), a job
  blocked by a failed dependency (reported, scheduler exited), an 8-thread job (ran alone), a pool job
  (attributed to its `agent/method`), a garbage line and a duplicate name (both skipped with a warning). 9 s total.
- Restart/adopt: scheduler SIGTERMed with 2 jobs running; they kept running; a new `run` adopted them;
  `cancel` (SIGINT -> cancelled rc 130), `cancel --kill` (rc 137), `add` while running (picked up at once).
  `status` showed per-job CPU% (a CPU-spinning job at nice 15 got 32-37%) and RSS.
- `--respect-load` with load 15: launched nothing; without it the job started.
- Collector on the real repo: 460 files, 3,363 complete tour lines scanned in ~4 s; first full verification 37
  checks in 58 s (each ~0.5 s unloaded, 1-2 s under this load); incremental re-run 7 s. Replaying
  `record_8123.txt` verifies at 8123. A fake 8124 (one wait +1) is rejected ("illegal jump d1->d2").
  It found and verified, from other agents' current files: **8130 strict** (`subagents/lns/best.txt`,
  156 moves), 8127/8126 (lns), 8126 (polish), and checker-only 8176 / 8130 / 8123.5 (fractional, flagged
  `free`/non-int). Banner raised for 8176 (not strict) and 8130 (strict).

## Suggested full run
```
python3 subagents/runner/runner.py add ...      # or edit subagents/runner/queue.txt
nohup nice -n 5 python3 subagents/runner/runner.py run --cores 4 --exit-when-idle > subagents/runner/tmp/sched.log 2>&1 &
python3 subagents/runner/runner.py status
```
Memory: the runner itself uses ~25 MB (collector ~50 MB); jobs need whatever they need.
Keep `time` <= ~4200 s (brief: jobs under ~70 min). Add `--respect-load` while other agents are running.
