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
