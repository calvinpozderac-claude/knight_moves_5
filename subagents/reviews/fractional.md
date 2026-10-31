# Review: fractional (rev_search), 2026-10-31 21:15 UTC

## 1. Status and claims checked
| Claim | Verdict | How checked |
|---|---|---|
| `CHECKER_ONLY_8176.txt` = 8,176 | **VERIFIED by the checker** | `check_tour.py` gives `moves=135 ok=True T=8176 maxvisits=3 unnecessary=[]` |
| What it relies on | one in-group ("free") sink | `classify.py` finds 97 pinned, 37 zero and 1 free sink, at move 96 `(23, b3)`: the knight sits 1 unit at a1 (n=23), then makes a dz=0 knight move to b3 inside the same group. There are no fractional altitudes. |
| Is that wait pinned by the rest of the tour? | **yes, uniquely** | `repin.py ... CHECKER_ONLY_8176.txt 96` re-pins every later wait for sinks k/6, k=0..36, and only sink=1 gives a legal tour. My own re-pin with sink 0 also fails (a negative wait is needed downstream). |
| `best.txt` = 8,123 | verified (the record itself) | checker |

### The rules question: does 8,176 rely on the checker being looser than the rules?
I think **probably not**, and I think you are being too pessimistic by labelling it CHECKER_ONLY:
- `tools/verify_tours.py` runs "the puzzle example (should be 28)" (`EX_ROWS`, 4x4). I classified its waits. Moves 5 `(5, a2)`, 6 `(3, c1)` and 7 `(2, a2)` are **in-group sinks of exactly your kind**: dz=0 before and after the wait. Move 5 also makes altitudes fractional (sink 5/2). I re-pinned each of the three with sink 0, and the rest of the example breaks, just as yours does. So the puzzle's own worked example uses waits whose necessity comes from later moves, not from the jump they precede. Move 7 also shows that the example fixes the opposite-in-group convention the checker uses: O stays put and is not counted in n.
- The strict "pinned" reading (the wait must be forced by its own jump) would reject the official example. The weakest reading ("you could reach that square without waiting") would reject every dance sit, because the down-jump dz=-2 is legal at t=0. The only reading that accepts the example is "changing this wait breaks the tour", which is what `necessity_check` approximates.
- The remaining gap is that `necessity_check` only probes t +/- 1/997, 0, t+1 and 2t. Your sink is unique on a 1/6 grid, which passes a stronger "this exact wait is the only one that works" test too.

**Ask for the orchestrator/user:** confirm against the original puzzle text that the 4x4 example is the official one and that it has those three waits. If it does, 8,176 should be reported as a record candidate, stated as "valid under the reading used by the puzzle's own example". Keep 8,130 (lns, pinned-only) as the conservative record.

## 2. Issues
- **MAJOR: no NOTES.md, and the commands that found the results aren't recorded.** `tmp/w1.log` and `w2.log` do not print their arguments (`--sc`, `--free`, `--fstep`, `--fmax`, `--level`, `--rng`). Print the full argv at startup and write NOTES.md, including the rules argument above. Your claim depends on it.
- **MAJOR (evidence): 8,176 is one hit.** It came at 146 s of one 180 s run: one restart out of about 250, anchor 73. The other run (w2) only reached 8,130. Most restarts give +4 (8,127) from one trivially placed free sink. Report how often each finish value occurs and what it depends on (anchor, `--fmax`, `--fstep`) over several seeds.
- **MINOR: `free_ok` is stricter than the checker, which is fine, but say so.** It only tests the infinitesimal perturbation, while the checker also accepts a wait if 0, t+1 or 2t breaks the tour. So some tours that the checker (and the example) would accept get pushed down to "unfinished". That is conservative, so keep it, but count how often a rejected tour would pass `check_tour.py`.
- **MINOR: the policy code caps `d` at 63 (`min(d,63)`),** so with `--sc` > 1, large climb sinks share codes. That costs efficiency only.
- **MINOR: the output writes minutes with `%.6g`.** A fractional total (e.g. 8176.5) on line 1 breaks scripts that use integer `-gt`, such as lns/saver.sh. Write an exact fraction, or floor it and add a separate exact field.
- **Insight to state in NOTES.** With pinned moves, altitudes stay integers, so a "rational wait" can only arise from a free sink of a non-integer number of units (as in the example's 5/2). "Fractional" really means "free sinks", plus the step size `--fstep/--sc` of those sinks. This is consistent with what your classify/repin tools already show.

## 3. Recommendation: CONTINUE (high EV: +53 on one restart)
This is the largest single gain in the portfolio, from a rule freedom that the integer engine excludes by construction. Expected value is high, as long as the rules question is settled.

## 4. Next round
1. **Stack on the new pinned record.** lns found 8,130 by changing moves 15-60. Its a1 sit near move 112 is followed by `(0, b3)`, the same spot as your free sink. A plain re-pin with a 1-unit sink there fails (I tried 1/2, 1, 2 and 3; the later dz pattern differs), so this needs search: `fnrpa --seed subagents/lns/NEW_RECORD_8130.txt --free 1 --amin 60 --amax 125`, level 3, 3 rngs x 15 min. Success means a checker-valid tour > 8,176.
2. **Fractional free sinks.** Run `--sc 2 --fstep 1` (half-unit sinks, like the example's 5/2) against `--sc 1` at equal time, 3 seeds each x 10 min, seeded from 8,176. Success means a higher best or a higher rate of +50-class finishes.
3. **Characterise the +53 mechanism.** At move 96 the 1-unit sink also raises h8 (h8 is opposite a1) into the knight's level, so the groups become 24/24 one unit apart. Write down why that pays (a parity shift that adds sits?). Then add a targeted move generator, e.g. "free sink at a1 when h8 is 1 below", and test it at levels 2-3. Success means 8,176 is reproduced in < 60 s from several anchors.
