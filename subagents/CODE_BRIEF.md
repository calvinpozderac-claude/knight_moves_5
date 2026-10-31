# Brief for the "code-ready" subagents (round 1b)

Read `subagents/BRIEF.md` first: the puzzle, the record (8,123), the checker, the folder and git rules
all apply. The difference: **the CPU is fully busy with five search agents right now, so your job is to
write, build and smoke-test code, not to run long searches.** Your program will be run later by the
orchestrator, when cores free up.

Compute rules for you:
- No job longer than ~60 seconds, and always `nice -n 15`, one thread. Compile with
  `g++ -O3 -march=native -std=c++17` (add `-pthread` if threaded). Smoke-test on small budgets.
- Programs must take a `--threads N` (or equivalent) and a `--time SECONDS` budget, write results
  incrementally (best tour so far + a pool of finished tours >= 7,500 min, one per line:
  `minutes<TAB>tour`), and stop cleanly on time-out or Ctrl-C.
- Inputs: start from a1 or from the state after the first k moves of a seed tour (e.g.
  `data/tours/record_8123.txt`); reuse `src/eng.h` (include it by relative path or copy it).
- Every tour you output must be in the checker's format `(wait, sq), (wait, sq), ...` and pass
  `python3 tools/check_tour.py`. Self-check: replaying the 8,123 tour through your engine must give 8,123.

Deliverables in `subagents/<your-name>/`:
- the source, a build line, and `RUN.md`: what the method is, exact commands for a short smoke run and
  for a full run (with suggested `--threads/--time` and expected memory), and what output files appear;
- smoke-test results (a few lines in RUN.md: score reached in N seconds, rollouts/s or nodes/s);
- final report (your last message, <= 10 lines): what you built, smoke result, full-run command.
