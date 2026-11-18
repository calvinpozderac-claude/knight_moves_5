# freesit notes (started 2026-11-17 ~04:50 UTC; resumed after a usage-limit/container restart)
Goal: tour > 9,290 by recovering lost dance sits (see ../ROUND10.md): penalty on VISITS to dance-sitter squares before the dance.
Files: sfl.cpp (copy of ../sfl/sfl.cpp; to be patched), freesit_saver.sh (checker-verifies tours > 9,290 in runs/ -> NEW_RECORD_<m>.txt).
## Log
- 04:50 dir created, saver script written, not yet started.
- 05:05 sfl.cpp patched: --vpen P --vsq g8,f8,... --vrange LO HI (P minutes per arrival at listed squares in moves LO..HI; acceptance/elite/restart use T-penalty; raw>9290 saved normally, adjusted best in runs/TAG_best.txt.adj). Replays 9290 = 9290. Saver running (freesit_saver.sh).
- Batch A (from 9290, focus 1-130, 40k iters): A1 80 g8,f8; A2 80 d5,c2,f7; A3 100 all five; A4 150 all five.
- Batch A (40k it, 10 min each, penalty P=80-150 on arrivals before move 91): no raw gain; searches only remove 1-3 arrivals (c2/f7/d5 shifted by 2 moves) and lose 68-70 raw (9222, 124 sits vs 127), A3 & A4 reached the same 9222 tour. Unpenalised LNS from that 9222 returns to 9290 (B1) i.e. freed visit is not converted to a sit; B2-B4 <= 9226 at 9 min.
- dance_pot2 beam from 9290's move-91 state gives only 8096 (W=3000): can't rebuild a dance from scratch with it. pot at move 91 = 9402.
- Batch C (16 jobs from 2 p9290 ties, single/pair-square penalties 100-200, focus 20-130): queued, workers running (queue.txt).
- 09:57 restart after limit: saver + 4 workers restarted; batch C first 4 jobs (C1-C4) all 9290, remaining 8 queued jobs resumed (C5-C8 lost mid-run are not requeued).
- 10:28 Batch C (C1-C12, single/pair penalties 100-200 on g8,f8,d5,c2,f7 before move 91): all 9290, no raw gain. Visit penalty frees 1-3 arrivals but LNS cannot convert them into dance sits (raw loss 68). Next idea: dedicated sit-block insertion operator / manual c2 block construction.
- 10:29 New phase (coordinator: build operator). State at move 91: c2 and f7 both in group 0 (36 pts, opposite pair), 0 visits left; c2 move 1 is forced (a1->c2 or b1). So an operator needs visits freed in the prefix first: H1-H4 = hard penalty 1000 on f7 / c2 / both arrivals before 91, to see what raw a prefix without them gives.
- 10:58 H1-H4 (1000/arrival): f7 hard-penalised reaches raw 9102 with f7 arrivals 3->1 in prefix (c2 and both: 8456 / worse). Queued I1-I4: unpenalised LNS from the H1/H4 tours to see if the 2 freed f7 visits turn into sits.
- 11:08 **9294** (NEW_RECORD_9294.txt, checker ok, 164 moves): I4 = unpenalised LNS (focus 1-178 p0.6 t0 60 free 4) from H4's tour (f7 hard-penalised, 9078). Also 9292. Route works: penalise f7 arrivals hard, then relax.
- 11:25 J1-J6 running (from 9294/H4/H1 with free 6): 9295, 9297 found (NEW_RECORD_9297.txt). fsp-on-start of H4 tour with --free 6 gives the 9294.
- 11:53 J1-J4: from 9294 -> 9297 (J1), others 9294/9209. K1-K3 (plain from 9297), L1-L3 (hard penalty 1000 on g8/d5/f8 before 91 from 9297) running.
- 12:32 **9300** (NEW_RECORD_9300.txt, checker ok) from L2/L3 (hard penalty 1000 on d5/f8 arrivals before move 91, from 9297). Queued M1-M2 plain LNS from 9300.
- 12:42 **9307** (NEW_RECORD_9307.txt, checker ok, 173 moves) from L-jobs: op 7 crossover after the hard-penalty tour. Queue N1-N8 from 9307 (plain + hard penalty d5/f8/g8/f7/d5+f8).
- 12:52 **9382** (NEW_RECORD_9382.txt, checker ok, 173 moves) from N4 (hard penalty 1000 f8 arrivals before 91, from 9307; 9336/9337 on the way). Queue P1-P6 from 9382.
- 13:13 **9513** (NEW_RECORD_9513.txt, checker ok) from the P-round (9387 on the way). Queue Q1-Q6 from 9513.
- 13:42 **9520** (NEW_RECORD_9520.txt, checker ok). Q-round still running; saver and workers left running.
- 13:43 Phase 4 from 9520: saver threshold 9520; queue R1-R16 (plain x4, hard penalty 1000 on f8,d5,g8,f7,g7,h7,e8,g6,h6,e6,f6,c2 arrivals before 91, from 9520).
- 13:53 **9724** (NEW_RECORD_9724.txt, checker ok). Queue S1-S4 from 9724 put in front.
- 14:13 9766, **9774** (NEW_RECORD_9774.txt, checker ok). T1-T4 from 9774 queued at front.
- 14:33 **9811** (NEW_RECORD_9811.txt, checker ok). U1-U4 from 9811 queued at front.
- 14:54 **9936** (NEW_RECORD_9936.txt, checker ok). V1-V4 from 9936 queued at front.
- 15:24 9937, 9941, **9943** (NEW_RECORD_9943.txt, checker ok). Gains slowing (+7 in 30 min). W1-W4 from 9943 queued.
- 15:54 plateau at 9943 for 30 min. Queued X1 (force b1 start: penalty on c2 at move 1), X2 (b1,c1 opposite sitters), X3 (g8,f8,d5 at 700), X4 (b1).
- 16:44 no gain after 9943 over ~1h20 (W, X jobs: b1 start, b1/c1, g8+f8+d5 penalties all <= 9943). Saver and workers still running.
- 16:44 Phase 5 (from 9943): saver threshold 9943. 12 diverse seeds in/div*_*.txt (farthest-point from the 609 distinct >=9850 tours); jobs Ydiv* = plain LNS from each, donor pool of 7 high tours (op 7 crossover).
- 17:15 Ydiv jobs (diverse seeds + crossover pool): best 9941/9940/9937, no gain over 9943. Anatomy of 9943: sitters with prefix arrivals and few late sits: c3 3/1, c1 2/1, f8 2/1, g8 2/1, d1 2/1, e5 1/1, c5 1/1, d5 1/2. Queued Z1-Z8 hard penalties (1000) on c3,c1,d1,e5,c5,f8+g8,c3+c1,d5 before move 85.
- 17:44 9944 found (NEW_RECORD_9944.txt, from Z jobs). Z1-Z4 no gain; queued relax jobs ZR3/5/6 from adjusted Z tours and ZR9 from 9944.
- 18:34 ZR/Z5-8 no gain. Queued AA1-AA8: plain LNS from 9944 with varied focus/temperature (t0 40-150) and a crossover pool.
- 19:04 9945 (NEW_RECORD_9945.txt, checker ok) from AA2 (focus 35-130, t0 60).
- 19:40 end of phase 5: best 9945. Saver/workers left running.
- 20:02 Phase 6: runl.sh (TL env = time cap, default 1600). nec_ok is already score-gated in record()/fsp (only improving candidates are checked), so no change there. Queue LA1/LA2 (plain, 6000 s, 150k it from 9945), LB1/LB2 (hard exclusion f8+g8 / c3+d1 before 85, 200k it, 6000 s). Saver threshold 9945.
- 21:42 LA1/LA2 (long plain 6000 s, ~100k it) and LB1/LB2 (hard exclusion 200k it: raw 9599/9629 adjusted) all ended at 9945. Queued MA1-MA4: 5000 s relax from the LB adjusted tours with the other adjusted tour as donor.
- 22:42 MA1-4 (relax from LB adj tours) no gain so far. Queued chained NP/NR i=1..4: 2400 s hard exclusion on f7,d5,g8,c5 then 2400 s relax from its adjusted tour.
- 00:02 9947 (NEW_RECORD_9947.txt, checker ok) from the chained NP/NR jobs.
- 00:32 end of phase 6: best 9947 (NR1: relax after f7 hard exclusion); NR2/NR3 9945; NR4 failed to run (donor path issue). Saver/workers idle/running.
- 01:02 Phase 7: saver threshold 9947. chain.sh = 2400 s hard exclusion then 2400 s relax (falls back to the start tour if no adjusted tour). 12 chains CH1-12 from 9947 and ties (in/t47_*.txt) on f7 x4, f8, d5, g8, c3, c5, c1, d1, e5 with 3 donor mixes. (NR4's c5 chain had no adj tour so its relax step never ran.)
- 01:36 container restart killed CH1-CH4 (no result); requeued them at front of the 12-chain queue; saver (threshold 9947) and workers restarted.
- 02:48 RULE CHANGE: free waits not allowed; only STRICT tours count (check_tour ok AND strict_check STRICT). Old NEW_RECORD_9291..9947 are free-wait tours = invalid (kept only as history). New phase: sfl.cpp record() now rejects any candidate with a free sink; runs use --free 0 (no fsp). newstrict.py/strict_saver.sh write STRICT_RECORD_<m>.txt only if both tools pass (threshold 9282; runs/s*). Strict seeds: in/st*_*.txt (25 strict tours 9270-9282 gathered from penalty92/data/sfl). Queue: schain.sh chains (2400 s hard exclusion at 1000/arrival on f7,f8,d5,g8,c3,c1,d1 before 85, then relax) sC1-7 from record_9282, plus sP1-4 long plain strict LNS from st0/st5/st10/st14.
- 03:23 strict chains sC1-4 (f7,f8,d5,g8 exclusion then relax, 100k it each) all end at 9282 (strict); sC5-7 and sP1-4 running.
- 03:43 strict: sC5-7, sP1 also 9282. Queued sD1-6 (pair-square exclusions from other 9282 ties, 8 donors) and sQ3/sQ6 (hot t0 120 plain).
- 05:05 strict phase end: all 23 jobs (sC1-7, sD1-6 chains, sP1-4, sQ3/6) end at 9282; no STRICT_RECORD above 9282.
- 05:06 Phase strict-2. (2) on 2 cores (worker2.sh/queue2.txt): in/fam*_*.txt = best strict tour per merge-sit signature (60 signatures from families/penalty92/lns84/f5push/polish92/graft92/data, 9000+); plain strict LNS sF_* from the 7 most distant (fam7,14,17,4,8,6,9) + chains sG_* (f7/f8 exclusion) on fam7, fam14.
- (1) Strict sit-insertion operator: sfl_ins (sfl.cpp INSWEEP mode): at every step k, exhaustive strict window DFS of depth 1..8 pinned moves from the state before k, re-sync anywhere in X[k..k+D+20] with later dz kept / waits re-derived, patch DFS + skeleton reroute on breaks (budget 3M nodes per (k,D)); script insweep.sh. Results:
  * on 9282 (k 88-100): best per insertion 9274-9278, i.e. every insertion loses 4-190 (no sitter has a spare visit: spare visits are only on expelled opposites d1,e4,e5,e6,f2,f6,g2,h1).
  * on the exclusion-adjusted strict tours (freed visits on f7/d5/c1/d1/...: sC1p 9216, sC3p 8979, sC7p 9034, sD5p 9159, sC6p, sD1p): over k=20..176 the best insertion never beats the start (gain 0, +1 once). A freed visit on one sitter cannot become a sit alone: dance sits alternate between the two groups, so an extra sit needs a partner sitter with a spare visit in the other group (e.g. g8+f8 for the end block f8 g8 f8 g8 f8 h8), and the g8/f8 hard exclusions found no climb without them.
  * the one dance transit in 9282 is move 151 (b7 -> c5, 0 wait): a block-parity loss (a7/b7 block has 5 sits, next block starts in b7's own group). Fix needs a block order with matching parity -> trying strict --blockp runs.
- 05:36 (2) sF jobs from fam7/fam14/fam17 reached 9282 but via crossover with the 9282 donors (converged back). Requeued (queue2) sH_* with ONLY the seed itself as donor (8 families) and sI_* f7 chains self-donor. (1) sB0/sB3 strict --blockp 1 (block order trials, focus 85-177): 9282, block ops accepted (~300) but no gain.
