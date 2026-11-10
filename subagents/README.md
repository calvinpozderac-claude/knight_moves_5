# Subagent experiments

Five parallel agents, each trying a different approach to beat the 8,123-minute record
(`data/tours/record_8123.txt`). Each agent works in `subagents/<name>/` and writes a `NOTES.md`
with its method, commands and results. `subagents/RESULTS.md` is the running scoreboard.

## Round 1 (searching): nrpa, exact, lns, opening, fractional — see `BRIEF.md`
## Round 1b (code-ready, run later): see `CODE_BRIEF.md`

| Folder | Method |
|---|---|
| `astar_cpu/` | CPU value-guided best-first search (nnastar without the GPU) |
| `beam_dom/` | Large beam search with dominance pruning and diversity |
| `nmcs/` | Nested Monte Carlo Search, Lazy NMCS, single-player MCTS |
| `goexplore/` | Go-Explore / MAP-Elites archive search |
| `genetic/` | Memetic / genetic algorithm with splice crossover and repair |
| `macro/` | Macro-move (sit + min-visit hops) search |
| `polish/` | Tour polisher: re-routing, sit insertion, window re-search |
| `runner/` | Job queue, scheduler, verifier and leaderboard |
| `prior/` | Learned move prior for NRPA (GNRPA bias) |
| `goal/` | Goal-conditioned opening search toward a designed dance start |

## Reviewers (see `REVIEW_BRIEF.md`; output in `reviews/`)
| Reviewer | Covers |
|---|---|
| `rev_search` | nrpa, exact, lns, opening, fractional |
| `rev_code_a` | astar_cpu, beam_dom, nmcs, goexplore, genetic |
| `rev_code_b` | macro, polish, runner, prior, goal |
| `rev_portfolio` | whole portfolio: headroom analysis, CPU allocation, evidence standard (`reviews/PORTFOLIO.md`) |

Reviews are relayed to the agents by the orchestrator at each iteration.
