# Review: opening (rev_search), 2026-10-31 21:15 UTC

## 1. Status and claims checked
| Item | Verdict | How checked |
|---|---|---|
| Insertion polish (`insert`, K=2,3,5) on 8,123 | no gain (8,123), consistent | `tmp/ins{2,3,5}.txt` all 8,123 |
| `reroute` full tours `tmp/full/r_*.txt` (300) | **VERIFIED legal** on a sample | `check_tour.py` on r_0, r_1, r_150 and r_299 gives `ok=True T=8123 maxvisits=3 unnecessary=[]` (135-143 moves) |
| Rerouted 44-move prefix + fresh L3 dance (`rr0_l3`): best 7,362, against the record prefix (`ctrl_l3`): best 7,989 | **INCONCLUSIVE** | see the issues below |
| `splice_nrpa --objective splice`, L2 | only re-finds 8,123 | `tmp/spl_l2.log` |
| No best.txt / NOTES.md yet | n/a | |

## 2. Issues
- **MAJOR (evidence): ctrl against rr0 is underpowered, and the truncated restarts are counted.** ctrl has 6 restarts. The last one, 6,980, is cut off by `--time` at 235k of 1M rollouts. rr0 has 5, and its last one, 5,092, is cut at 30k. Completed restarts only: ctrl {7518, 7783, 7509, 7989, 7688}, mean 7,697; rr0 {7362, 7034, 6954, 7316}, mean 7,167. That is suggestive, but n=4-5 at one anchor, and the time per restart differs (rr0 is about 1.5x slower). Use >= 10 completed restarts each. Also note the conclusion it would support: "saving visits in the wrong places (by the min-pair loss) does not help the dance". That is a statement about the **loss metric**, not about rerouting.
- **MAJOR: the loss proxy is not validated.** `loss = Σ min(v_p, v_opp)` (MODE0) counts every pair, including points that are never in the dance groups. MODE1/MODE2 (sit-weighted) exist but aren't compared. Before spending L3/L4 compute on prefixes, check that the proxy predicts dance quality. Take about 10 prefixes spanning the loss range, run L3 x 5 completed restarts on each, and compute the rank correlation between loss and median dance score. If rho < 0.5, fix the metric first. The idealised-dance estimate `dance_pot` is known to be optimistic too (README: 8,725 estimated, 7,621 achieved).
- **MINOR: `reroute` writes outputs without checking legality.** `apply_move` ignores visit caps and legality. In MODE0, CAP=3 allows the reused suffix to exceed 3 visits. Replay each output with `pinned_moves` (or the checker) before writing. The samples I checked were fine.
- **MINOR: the splice objective is sound but nearly inert.** It is sound because the signature hash only *nominates* a resync point and `splice_replay` re-checks legality and visits exactly. It is inert because a rejoin needs the same relative altitudes for every point <= 2 above the knight, so in practice it only rejoins the seed's own trajectory (100% 8,123). Relax it: rejoin at any dance start with the same two-level group sets and h8 inside them, then *search* the dance there (a short L2) instead of replaying the seed.
- **MINOR: `insert` (closed loops + replay of the same (delta, q)) has low EV.** A loop of zero hops only spends visits, and a loop with sits changes altitudes, so the replay breaks. STOP this line.

## 3. Recommendation: CONTINUE, PIVOTED toward the merge region that lns just proved has headroom
lns found **8,130** with pinned moves only. It changed moves 15-60: extra zero hops h5-h4-h6-h4-h6 and a7/a8, and a reorganised set of climb sits: (52, c7), (27, c8), (6, d8), (30, d8), (24, c8). It entered the dance through (80, h5) at move 59, and the later dance is the same as 8,123's. That is your mission's region, and the gain came from **re-sequencing climb sits**, not only from saving visits. Your analysis tools (`analyse.py`, `pairs.py`) are the right instruments to explain it.

## 4. Next round
1. **Explain 8,130 against 8,123** (cheap, about 10 min). Run `pairs.py` on both, with the dance start at 59 and 44 respectively. Report sits per pair, the visits spent before the dance, the group sizes at dance entry, and which pairs gained sits. Success means a one-paragraph mechanism in NOTES.md that tells the other agents what to target.
2. **Validate or replace the loss proxy** (see issue 2). About 10 prefixes x 5 completed L3 restarts, about 25 min. Success means rho >= 0.5, or a better proxy, for example the number of dance-group points with spare visits at dance entry.
3. **Merge-region search seeded from 8,130.** Run `splice_nrpa` with the relaxed "dance-start rejoin" (issue 4), or plain fresh L3 with anchors 15-45 on 8,130, 20 min. Success means a checker-valid tour > 8,130.
