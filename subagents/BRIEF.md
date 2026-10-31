# Brief shared by all subagents

Goal: find a legal Knight Moves 5 tour longer than the current record, **8,123 minutes**
(`data/tours/record_8123.txt`, line 1 = minutes, line 2 = tour). Read `README.md` first (rules,
the "dance" structure, NRPA, nnastar). The engine is `src/eng.h` (integer altitudes, pinned moves:
the wait before each jump is implied by the jump). Exact checker:
`python3 tools/check_tour.py "$(sed -n 2p FILE)"` -> must print `ok=True`, `maxvisits=3` (or less),
`unnecessary=[]`. A tour only counts if this checker accepts it.

Facts from earlier work (README):
- Every good tour: ~35-move "climb" (raise e8/e7/d8, walk up to d8, sink it with h8), a short merge, then
  the "dance": two big groups 2 apart; knight sits in the upper group (sinks 4 units) then hops onto an
  orthogonal neighbour in the other group. Each sit is worth ~70-136 minutes.
- Limits: 3 visits per point (opening/repositioning visits are lost sits), and when the knight sits,
  the opposite point leaves the group (each opposite pair supplies roughly one sitting point).
- 8,002 beat 7,911 by reaching the dance with 4 zero-wait hops instead of 8 (freed visits -> more sits).
- 8,123 = nnastar (NN-guided A*), not reproducible here (no GPU).
- NRPA (`build/nrpa`, `make build/nrpa`) does ~25k rollouts/s/thread; level 3 ~30 s, level 4 ~1 h on one core.

## Machine & rules of engagement
- 4 CPU cores, 15 GB RAM, **no GPU**, shared by 5 agents running at once. **Use at most 1 heavy thread
  at a time** (e.g. `build/nrpa --threads 1`), run long jobs with `nice -n 5` and a `timeout`.
- Foreground shell calls time out at 10 minutes: run long jobs in the background (Bash
  `run_in_background`) writing to a log, then check the log; keep any single job under ~70 minutes.
- Work only inside your own folder `subagents/<your-name>/`. Copy and modify sources there (e.g.
  `subagents/<name>/foo.cpp`, include `../../src/eng.h` or a copy). Do not edit files outside your folder.
  Put large/temporary outputs (> 1 MB) in `subagents/<name>/tmp/` (git-ignored by the orchestrator).
- **Do not run `git commit`, `git push`, `git checkout`, or anything that changes git state.** The
  orchestrator commits your folder.
- `ortools` and `numpy` are installed for python3. No torch.

## Deliverables (in your folder)
- `NOTES.md`: method, exact commands to reproduce, what you tried, numbers (best score per variant),
  what failed and why, and your recommendation for the next round.
- `best.txt`: your best verified tour (line 1 minutes, line 2 tour, same format as data/tours).
- If you beat 8,123: also `NEW_RECORD_<minutes>.txt`, verified with the checker, and say so first in
  your final report.
- Final report (your last message): best score, whether verified, 5-10 line summary of findings, and
  concrete next ideas. Keep it short.
