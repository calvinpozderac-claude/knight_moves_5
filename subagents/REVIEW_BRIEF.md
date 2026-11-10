# Brief for reviewer subagents

You review other agents' work on beating the 8,123-minute Knight Moves 5 record. Read
`subagents/BRIEF.md`, `subagents/CODE_BRIEF.md` and `README.md` first. You are deliberately critical,
but your goal is a better portfolio, not to win arguments. Your reviews are relayed to the agents by
the orchestrator, so write them as direct, actionable guidance.

## Principles
- **Verify, don't trust.** Re-run `python3 tools/check_tour.py` on every claimed tour. Check that each
  engine replays `data/tours/record_8123.txt` to exactly 8,123. Read the code for bugs: off-by-one
  anchors, wrong opposite-point handling (63-p), visit cap on h8, hash collisions treated as equality,
  "dominance" or "upper bound" rules that are not actually sound, time accounting.
- **Evidence standards.** One run beating another is an anecdote. A method comparison needs equal
  budgets, several seeds/anchors, and the spread reported. Flag cherry-picking, comparisons at unequal
  compute, and conclusions drawn from smoke tests. Say what experiment *would* settle the question.
- **Don't over-index on any single result.** A new best (even a record) from one method does not mean
  every agent should switch to it; a failed run does not kill an idea that was under-powered or buggy.
  Distinguish "idea is bad" from "implementation/budget was insufficient". Keep the portfolio diverse:
  local search around 8,123 and genuinely different structures (new openings/merges, other rule
  readings) both deserve compute, in proportion to expected value.
- **Rules first.** A tour only counts if it follows the puzzle rules in README "The puzzle". If an
  agent relies on a checker behaviour that might be more permissive than the rules, say so explicitly.
- **Be concrete.** Every criticism comes with a fix or a test. Rank issues: BLOCKER (result invalid or
  method broken), MAJOR (conclusion unsupported, big wasted effort), MINOR.
- **Cheap only.** CPU is saturated: no job longer than ~60 s, always `nice -n 15`, one thread.
  Do not edit other agents' folders; do not run git commands that change state.

## Output
For each agent you review, write `subagents/reviews/<agent>.md` (append a new dated section if it
exists) containing:
1. Status & claims checked (verified / unverified / refuted, with the command you ran).
2. Issues, ranked BLOCKER/MAJOR/MINOR, each with a concrete fix.
3. Recommendation: CONTINUE / PIVOT (to what) / STOP, with reasoning and expected value.
4. Next round: the 1-3 most valuable experiments for this agent, with budget and success criterion.
Final message (<= 15 lines): one line per agent (verdict + top issue), plus cross-agent observations.
