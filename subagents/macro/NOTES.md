# macro: notes

Code-only round (see `RUN.md` for the method, commands and smoke numbers). Best tour: 8,123, the record
rebuilt through the macro engine and accepted by the checker. Nothing new was found; there were no long runs.

## What the abstraction does and doesn't buy
- **Little compression in the dance:** the record from move 38 is 97 raw moves but 83 macros. Only 13 of those
  moves are zero-wait hops. The "10× shallower" hope holds only where many hops occur, in the opening and the merge.
- **Hop routing is done right:** the hop choices are made by min-cost routing instead of by chance.
  - All 83 record macros are offered by the generator with the same hop count.
  - The record rebuilds exactly from its macros, from moves 35, 38, 44 and 60.
- **Fresh level-2 searches are better:** after move 44, macro NRPA reached 5.0–6.6k per restart, against
  4.2–5.5k for raw NRPA. But a restart costs about 15× more (about 650 rollouts/s against 10k/s on this loaded machine).
- **Exact tails are the clearest win:** the DFS proves the record's last 17 macros optimal over the
  macro space in under 6 s. With a heuristic −200 slack, 21 macros are searched completely.
  - **So:** the record can't be improved by changing only its end, short of about 20 sits. The gains must come
    earlier (merge or early dance), which matches the README.
- **Beam is weak:** the idealised-dance estimate is flat (8642) along a good dance, so beam ranking is blind
  until the dance breaks.
- **From a1 there is no finish:** the same as raw NRPA. A first version of the h8-reachability pruning also
  ignored rises (the climb raises squares by sitting on their opposites), which killed every move from a1.
  That is fixed.

## Recommendation for the next round
1. **Fresh level-3 macro NRPA after moves 36–48 with the tail polish on** (RUN.md, command 1). Compare it
   with raw level 4 at equal CPU.
2. **DFS on every pool tour ≥ 7,900 at K = 15–20.** It is cheap, and it is exact over the macro space.
3. **A tighter, adjacency-aware dance bound:**
   - **What it should count:** each sit needs an orthogonal neighbour in the other group with visits left.
   - **What it would buy:** it would make the DFS reach about 30 macros, and make the beam useful.
4. **Hybrid:** run raw NRPA through the climb and merge, and macro search after it.
