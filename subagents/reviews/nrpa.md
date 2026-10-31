# Review: nrpa (rev_search), 2026-10-31 21:15 UTC

## 1. Status and claims checked
| Claim | Verdict | How checked |
|---|---|---|
| `gnrpa` replays 8,123 | **VERIFIED** | `gnrpa --level 1 --time 2 --seed-tours data/tours/record_8123.txt --anchor-min 134 --anchor-max 134` gives best 8,123, and the checker confirms `ok=True T=8123 maxvisits=3 unnecessary=[]` |
| GNRPA bias implementation | correct | The bias enters both the rollout and the `adapt` softmax, as in Cazenave's GNRPA. `BIAS*d*n/100` equals the old `BIAS*minutes/100`. The rollout rate with a prior is the same within noise (about 31-36k/s for none, `--bsink4 3` and `--bsink4 10`, `NRPA_BENCH=20000`, anchor 40). |
| "`--bsink4` helps L3": `L3 A40 []: n=5 mean 6964 max 7279`; `[--bsink4 3]: n=4 mean 7482 max 8038`; `[--bsink4 6]: n=2 mean 7690` | **UNSUPPORTED (anecdote)** | `tmp/l3cmp_a40.txt`, see the issues below |
| No best.txt / NOTES.md yet | n/a | |

## 2. Issues
- **MAJOR: `l2cmp.sh` averages in the last, time-truncated restart.** `--time` stops the final restart part-way, and its partial result is counted as "finished". With n = 2-5, one truncated restart moves the mean by hundreds of minutes, and it penalises configurations with slower restarts. Fix: count only restarts that completed, i.e. that ran all `ITERS^LEVEL` rollouts. Print the rollout count per restart, or drop the last line.
- **MAJOR: the comparison is underpowered and confounded.** It uses one anchor (40), one rng (default `--rng 1`), and n = 2-5. The spread between L3 restarts at one anchor is about +/-400 minutes (the completed ctrl restarts in opening/tmp/ctrl_l3.log range from 7,509 to 7,989), so 6,964 against 7,482 is within noise. Experiment that would settle it: >= 12 completed L3 restarts per configuration, at 2 anchors (40 and 60), with the same rng list for every configuration. Report median, IQR and max. Budget: about 3 configurations x 2 anchors x 12 x 30 s, about 36 min on one core.
- **MAJOR (strategy): L3 from anchor 40 is about 400-1,100 below the record.** Even the best L3 result (8,038) is < 8,123. A prior that raises the L3 *mean* only matters if it raises the L4 *maximum*. Once a prior wins at L3, test it at L4 with >= 3 runs, not one.
- **MINOR: every configuration writes to the same `tmp/cmp_out.txt` and `tmp/cmp.log`,** so finished tours from earlier configurations are lost. Use per-configuration filenames and a `--pool`.
- **MINOR: `--bsink4 10` is a near-hard constraint** (e^10 = 22,000x). It turns GNRPA into a scripted dance and stops it from finding the non-4 sinks that the climb and merge need. Prefer 1-4 with a sweep, or condition it on "in a dance state".
- **MINOR: `STAB` multiplies the rollouts per level-1 step.** That is fine at equal wall time. Report it as "same time, fewer adaptations" and not as same-cost.

## 3. Recommendation: CONTINUE, re-anchored, with a proper protocol
Two cheap clean gains landed while you were running. lns found **8,130** with pinned moves only, by changing moves 15-60. fractional found **8,176** with one in-group sink at move 96. So: (a) re-anchor on 8,130, and (b) note that the lns gain sits in the climb/merge, where a fresh-dance NRPA anchored at >= 40 cannot reach. Your edge is strong fresh dances (L4) and priors that make L4 better. Keep that, but the beam/GNRPA claims need the protocol above.

## 4. Next round
1. **Prior sweep at L3.** {none, `--bsink4 2`, `--bsink4 4`, `--bopp 1`} x anchors {40, 60} x 12 completed restarts, with the same rngs for every configuration. Success means a configuration whose median beats none by > 1 IQR at both anchors.
2. **L4 from the 8,130 tour's dance start.** In the 8,130 tour the dance starts at about move 59. Run `--seed-tours subagents/lns/NEW_RECORD_8130.txt --anchor-min 58 --anchor-max 62 --seed-adapt 0 --no-warm`, level 4, 1 thread, about 60 min, with the winning prior from (1) if there is one, otherwise none. Success means > 8,130. Also record the L4 final scores, as a spread for the next round.
3. **Optional, if time remains: anchor sweep on 8,130.** L3 fresh, anchors 30-60 (the region lns changed), 20 min. This measures whether NRPA can find merge-region gains or only dance gains. Success means any finished tour >= 8,130 from an anchor <= 45.
