# penalty92: automated penalty + order-trial family sweep from 9,275 (and 9,271)  (Round 9)

Goal: a tour above 9,275 from a new family. 2 heavy threads (2 workers), nice -n 5. Start 23:16 (2026-11-13).

## Tools (this folder)
- `bin/` copies of families' `dlns`, `lns_r6`, `polish`, `bb`; `fam.py` (sit sequence, block signature, block-level
  edit distance), `gen.py` (sitting blocks with move ranges and opposite squares).
- `worker.py WID SECS`: pulls jobs from `queue.txt` (flock), appends `results.tsv`, logs to `events.log`.
  - `dl|NAME|BASE|PEN|rule;rule`: dlns (`--rule sq,sq:FROM:TO`, `--free 0`, base as start and only donor, DLT=120 s,
    time-based annealing) -> feasible best (no penalised sit) = raw tour -> block signature; skipped if it equals a
    known family (`known_sigs.txt`: 8,830 ... 9,275 + every new signature found here) -> plain `lns_r6 --free 0`
    on that tour alone, LT=420 s.
  - `lns|NAME|START|SEED|TIME|donors|focus`: more plain-LNS seeds on a family.
- `penalty92_saver.sh`: every 30 s, any `y/ lns/ pol/` tour above 9,275 that check_tour accepts -> `NEW_RECORD_<m>.txt`.
- Distance = block-level edit distance of the sit signature to 9,275 / size of the symmetric difference of the
  sitter sets (9,271 is 12/1, 9,268 11/1, 9,080 16/8 on this scale).
- Rule types (block = 9,275's sitting block, dance from move 95): S = ban the block's squares from move 80;
  P = two blocks; O = block + its opposite squares; X = ban block squares over the whole tour (pen 300) to push it
  to the opposite side; L/E = order trials (L: ban the block before its current end -> it must come later;
  E: ban it from its current start -> earlier); M = early merge sits f3/f7 and g6/h3 in moves 40-90 (pen 500).
  Prefix A_ = base 9,275, B_ = base 9,271.

## Notes
- dlns with `--iters` huge freezes the temperature at t0 (schedule is it/ITERS); the worker runs time-based (no --iters).
- The merge sits are hard to remove: a 25 s dlns with f3,f7 banned in moves 40-90 (pen 500) found no feasible tour.
- 23:23 merge bans (f3,f7 or g6,h3 in moves 40-90, pen 500, 120 s): no feasible tour at all. Single-square merge
  bans (A_M_f7, A_M_h3) queued.
- Same-sitter order variants fall back to 9,275: A_S_c3d3 (raw 8,817, c3/d3 block replaced by e6/f6 late) -> plain
  LNS 9,267 (order c5/d5 c3/c4 d3/d4, sitters unchanged), and two more seeds from it / from its raw tour both end
  at **9,275 with 9,275's exact signature** (two tours distinct from the record, md5 23904b34, b8a997d6). A_E_c4c5's
  raw tour already had 9,275's signature (skipped). A_L_c6c7 (c6/c7 c8/d8 after the a8/b8 a7/b7 sweep): raw 9,197
  -> 9,263. So 9,275 is a strong attractor for its sitter set; the queue was reordered at 23:50 toward sitter-moving
  rules (the user's successful rule shapes: b6,c6,d4,d5 (-> 9,080), c6,c7,c8 (-> 8,887), c3,c4,d3 (-> 8,842),
  a5-a8,b5,c4 (-> 8,900), b6,b7,c8 (-> 8,946), as U_ rules) and block+opposites (O_).
- 00:16 worker 2 was killed by the 60-min background limit during A_U_b6b7c8's LNS (at 9,178 after 294 s from raw
  8,358, dist 13/4: c6/c7 a7/a8 b7/b8 c8/d8 d6/d7 d3/d4 c3/c4 c5/d5 d7/e7 tail; a new family that climbs fast).
  Workers now stop taking jobs after 2,600 s. Worker fix: an lns job whose run never improves no longer errors.
- 00:30 container restart killed workers + saver (lost A_U_a5a8b5c4 and A_U_b6b7c8_L2, both requeued). Restarted
  00:31. graft92's H3 family (9,275 merge + 9,080 dance splice repaired, 9,264, `in/H3_best.txt`) added as base C_.

## Rule table (raw = dlns feasible best; dist = block edit distance / sitter-set difference vs 9,275; LNS = plain lns_r6 420 s)
| rule | squares:FROM:TO | raw | dist | LNS | dist after | family after LNS |
|---|---|---|---|---|---|---|
| A_M_f3f7 | f3,f7:40:90 (pen 500) | none | | | | |
| A_M_g6h3 | g6,h3:40:90 (pen 500) | none | | | | |
| A_S_d6d7 | d6,d7:80:999 | 8,766 | 2/2 | 8,869 | 7/2 | new (d7/d8 ... d3/d4 c3/d3 c3/c4 c4/c5 c5/d5 tail) |
| A_S_c3d3 | c3,d3:80:999 | 8,817 | 2/4 | 9,267 | 3/0 | same sitters; 2 more seeds -> 9,275 (9,275's own signature) |
| A_E_c4c5 (order) | c4,c5:149:999 | 9,209 | 0/0 | skip | | 9,275's |
| A_L_c6c7 (order) | c6,c7:80:131 | 9,197 | 4/0 | 9,263 | 4/0 | order variant (a8/b8 a7/b7 before c8/d8 c6/c7); +600 s: no gain |
| A_U_b6c6d4d5 | b6,c6,d4,d5:80:999 | none (120 s) | | | | requeued with 360 s |
| A_S_d4d5 | d4,d5:80:999 | 8,857 | 4/4 | 9,209 | 0/0 | 9,275's |
| A_U_c6c7c8 | c6,c7,c8:80:999 | 8,525 | 7/3 | 8,979 | 12/3 | new, stalls |
| A_O_c6c7 | c6,c7,f3,f2:80:999 | 8,838 | 1/2 | 9,275 | 0/0 | 9,275's |
| A_U_c3c4d3 | c3,c4,d3:80:999 | 8,331 | 4/3 | 9,273 | 0/0 | 9,275's |
| A_X_c4c5 | c4,c5:0:999 (pen 300) | 8,885 | 2/2 | 9,231 | 1/0 | same sitters |
| A_O_d6d7 | d6,d7,e3,e2:80:999 | 8,785 | 1/2 | 8,959 | 4/1 | new, low |
| A_U_b6b7c8 | b6,b7,c8:80:999 | 8,358 | 13/4 | 9,178 (killed at 294 s) | | new (see above) |
| A_U_a5a8b5c4 | a5,a6,a7,a8,b5,c4:80:999 (360 s) | none; partial best 8,413 (6 banned sits left) | 18 (sits) | 9,233 | 7/0 | same sitters |
| C_U_c6c7c8 (H3 base) | c6,c7,c8:80:999 | none; partial best 8,761 | | 9,244 | 12/0 | same sitters, H3-like order |
| A_M_g6h3 partial | g6,h3:40:90 | partial best 9,144 (g6 merge sit dropped, a2 sits instead) | 1/1 | 9,209 | 0/0 | 9,275's (g6 restored) |
| A_G_nog6 | g6:40:95 from that 9,144 (600 s dlns, pen 500) | 9,144 (no gain) | 1/1 | 9,207 | 0/0 | 9,275's |
| A_U_b6b7c8 seeds | from 9,178 / from raw 8,358 | | | 9,184 / 9,242 | 7/0, 6/0 | same sitters |
| A_U_b6b7c8_C | b6,b7,c8:80:999 continued 600 s from raw 8,358 | 8,485 | 8/3 | 9,169 | 9/1 | new (1 sitter moved), low |
| B_U_b6c6d4d5 (9,271 base) | b6,c6,d4,d5:80:999 (360 s) | none | | | | |
| A_U_a2a3b2b3 | a2,b2,a3,b3:80:999 (360 s) | 8,385 | 3/4 | 9,211 | 10/0 | same sitters, new order |

Worker change 00:45: when dlns finds no fully feasible tour, the partially penalised best (fewer banned sits) is used
as the raw tour (name suffix P), as the user's 8,900 came from a wrecked 7,227.

**Summary of the 9,275 sweep (to 01:24):** 20 rules, 15 rebuilt tours. Every rebuilt tour whose plain LNS went above
9,200 had put back 9,275's exact sitter set (sitter difference 0): 9,275 x3 (two distinct from the record), 9,273,
9,267, 9,263, 9,244, 9,242, 9,233, 9,231, 9,211, 9,209 x2. Tours that kept a moved sitter stalled at 8,869-9,169.
Merge bans (f3/f7, g6/h3, both, in moves 40-90) give no feasible tour within 120-600 s.

# Resumed 07:03 (after a usage limit + container restart): target 9,279
`in/record_9279.txt` (second merge class: f7/h3, d3/g6 early). Distances are now vs 9,279; saver threshold 9,279.
New rule type F_ = flip trial: ban one sitter or a small set of sitters for the dance (move 92 on 9,279, 80 on
9,275) so the opposite point sits instead. Prefix D_ = base 9,279.

## Rule table, 9,279 base (D_) and flip trials (07:05-07:52; dist vs 9,279)
| rule | squares:FROM:TO | raw | dist | LNS 420 s | dist after | family after LNS |
|---|---|---|---|---|---|---|
| D_F_c8d8 | c8,d8:92:999 | 8,830 | 3/2 | 9,123 | 6/0 | 9,279 sitters, new order |
| D_F_a8b8b7 | a8,b8,b7:92:999 | 8,453 | 3/3 | 9,128 | 6/0 | 9,279 sitters, new order |
| D_F_d8 | d8:92:999 | 8,901 | 5/1 | 9,096 | 8/0 | 9,279 sitters |
| D_F_c6c7 | c6,c7:92:999 | 8,840 | 1/2 | 9,277 | 0/0 | 9,279's own signature |
| A_F_d8 (9,275) | d8:80:999 | 8,999 | 11/2 | 9,176; +600 s 9,244 | 12/1 | 9,275 sitters, new order |
| D_F_c7 | c7:92:999 | 8,868 | 4/1 | 9,213 | 0/0 | 9,279's |
| D_F_d6d7 | d6,d7:92:999 | 8,764 | 1/2 | 9,277 | 0/0 | 9,279's |
| D_U_b6c6d4d5 | b6,c6,d4,d5:92:999 (300 s) | 8,181 | 11/4 | 9,201 (still rising) | 5/0 | 9,279 sitters, order a4/a5 ... b5/c5 b4/b5 c4/c5 b4/c4; 600 s continuation queued |
| D_F_c4c5 | c4,c5:92:999 | 8,893 | 1/2 | 9,213 | 0/0 | 9,279's |
Flip trials behave like the bans: plain LNS flips every sitter back within minutes (sitter difference 0 in all 9).
Two 9,277 finishes on 9,279's own signature (D_F_c6c7, D_F_d6d7) but nothing above 9,279.

07:50: record is now 9,282 (polish92 and graft92, one signature: 9,275's merge f3/f7 g6/h3 + 9,279's dance
c3/d3 b2/b3 ...). Bases E_ = polish92's 9,282 (`in/P_9282.txt`), G_ = graft92's (`in/G_9282.txt`), each the other's
donor in dlns (`dl` field 7 = donors). Saver threshold 9,282; distances vs 9,282.

## 9,282 bases (07:52-)
| rule | squares:FROM:TO | raw | dist (vs 9,282) | LNS 420 s | dist after | family after LNS |
|---|---|---|---|---|---|---|
| D_U_b6c6d4d5 +600 s | from its 9,201 | | | 9,207 | 6/1 | |
| E_U_b6c6d4d5 | b6,c6,d4,d5:91:999 (300 s, donor G) | none; partial best 8,790 | 5/4 | 9,204 | 4/0 | 9,282 sitters |
| E_F_c4c5 | c4,c5:91:999 | 8,896 | 1/2 | **9,282** | 0/0 | 9,282's own signature: a third distinct 9,282 (md5 b19142a0, `keep/E_F_c4c5_9282.txt`, checker ok), a tie |
| G_U_b6c6d4d5 | b6,c6,d4,d5:93:999 (300 s) | 8,458 | 9/2 | 9,148 | 10/0 | 9,282 sitters |
08:17: graft92's core is free (3 threads). Its repaired splices R9166 (9,279 merge x 9,080 dance) and R9222 (x 8,946)
have **9,282's dance sitter set** (R9166 differs only in the merge: g4 for f3), so they are reorderings, not
sitter-set changes; given one plain-LNS seed each (840 s / 720 s) on the third thread and worker 1.
| E_U_c6c7c8 | c6,c7,c8:91:999 (300 s) | 8,604 | 7/3 | 9,175 | 7/0 | 9,282 sitters |
| E_F_a7b7 | a7,b7:91:999 | 8,849 | 1/2 | **9,282** | 0/0 | a fourth distinct 9,282 (md5 cd42bdb9, `keep/E_F_a7b7_9282.txt`), a tie |
| R9166 seed (840 s) | plain LNS | 9,166 | | 9,180 | 13/2 | |
| R9222 seed (720 s) | plain LNS | 9,222 | | 9,244 | 10/0 | |

graft92's splice families R9089 / R9091 / R9222 / R9166 all have 9,282's dance sitter set (R9089 and R9091 lack the
f8 sit; R9166 has g4 for f3 in the merge), so they are reorderings, not sitter-set changes.

## 08:24- (4 threads, ~90 min): flip/ban rules with a KEEP stage
Since plain LNS put every moved sitter back, `dl` jobs now take a keep stage (fields 8-9): after the dlns raw tour,
a second dlns with the same rules at a small penalty (40 per banned sit, 600 s) climbs inside the new sitter set
unless restoring a sit gains more than 40; then plain LNS 420 s from that. Rows `<rule>_K` in results.tsv.
Queue: flips of single blocks/sitters of 9,282 (E_K_*) and a few on the splice families (R*_K_*).

| rule (keep pen) | squares:FROM:TO | raw | keep stage (dist; feasible) | plain LNS after | dist after |
|---|---|---|---|---|---|
| E_K_c8d8 (40) | c8,d8:91:999 | 8,897 | 9,261 (8/0; 8,893) | 9,268 | 8/1 (9,268's signature) |
| E_K_c6c7 (40) | c6,c7:91:999 | 8,845 | 9,258 (5/0; 8,845) | 9,270 | 4/0 |
| E_K_d6d7 (40) | d6,d7:91:999 | 8,742 | **9,282** (0/0: banned sits restored even at pen 40) | 9,282 (tie, md5 39760016) | 0/0 |
| R9091_K_c6c7c8 (40) | c6,c7,c8:93:999 (300 s) | 8,269 | 8,977 (12/0) | 9,009 | 8/0 |
| E_K_c3d3 | c3,d3:91:999 | 9,216, 9,282's signature: skipped | | | |
| E_K_d8 | d8:91:999 | none (partial best has a known signature) | | | |
| R9222_K_c4c5d4 (120) | c4,c5,d4:93:999 (300 s) | 8,578 | 8,754 (10/2; 8,578) | 9,136 | 12/0 |
| E_K_a8b8b7 (120) | a8,b8,b7:91:999 | 8,443 | 8,894 (2/2; 8,443) | 9,280 | 0/0 |
| E_K_d4d5 (120) | d4,d5:91:999 | 8,990 | 9,122 (1/2; 8,992; 1 banned sit left, d4/e4 block) | 9,282 (tie, md5 d27dc4ad) | 0/0 |
| R9089_K_d6d7 (120) | d6,d7:83:999 | 8,661 | 8,790 (11/2; 8,790) | 9,182 | 9/0 |
Pen 40 is too weak (a restored sitter is worth more than 40 per sit); at pen 120 the moved sitters stay but the kept
tours are 160-530 below 9,282, and plain LNS afterwards restores the sitters every time. Distinct 9,282 ties found
here: b19142a0, cd42bdb9, d27dc4ad, 39760016 (plus 9,280 and two 9,277s).

## 11:58- (2 threads, ~75 min): decaying keep penalty
`chain.sh TAG START RULE PEN:SECS ...` runs dlns stages, each from the previous stage's best, with the penalty
decaying 120 (1,200 s) -> 80 -> 50 -> 25 (600 s each), no plain LNS at the end. Chains: KD45 (E_K_d4d5's kept 9,122,
d4,d5 banned) and KR22 (R9222_K_c4c5d4's kept 8,754, c4,c5,d4 banned). The rest of the KEEP queue (E_K_e7e8,
E_K_b6c6d4d5, R9166_K_c4d4, E_K_c4c5, E_K_a2b2, E_K_c7, E_K_a6b6, E_K_c6c7c8) did not fit in the 2 threads.
Results (stage: best / banned sits / sitter difference vs 9,282):
| chain | start | pen 120, 1,200 s | pen 80 | pen 50 | pen 25 |
|---|---|---|---|---|---|
| KD45 (d4,d5 from move 91) | 9,122 (1 banned sit, d4/e4) | 9,164 / 0 / 0 | 9,170 / 0 / 0 | 9,170 | **9,178** / 0 / 0 |
| KR22 (c4,c5,d4 from move 93) | 8,754 (c4, c5 not sitting) | 8,763 / 1 / c4,c5 missing (feasible 8,519) | 8,763 | 9,148 / 8 / 0 | 9,148 / 0 |
KD45 kept the sitter set and moved the d4/d5 block before move 91 (dance starts d3/d4 c5/d5 c4/c5 ...): a reordering
at 9,178. KR22 held the c4/c5-free set at pen 120 and 80 but did not climb (8,763); at pen 50 it put the c4/c5 sits
back (9,148). The decaying penalty ends on 9,282's sitter set in both chains, with no new family.

12:40: the record is 9,286 (fw93, free waits on the 9,282 sitter set); saver threshold raised to 9,286. All jobs and
the saver stopped 12:50. No tour above 9,282 / 9,286 was found here (no NEW_RECORD file).

# Summary and recommendations (penalty92, round 9)
- About 75 rule trials (bans of single blocks, pairs and multi-block U_ shapes; block + opposites; whole-tour
  bans; order trials; merge bans; flip trials) from 9,275, 9,271, 9,279, 9,282 and graft92's splices, each with plain LNS,
  and later with a KEEP stage and a decaying penalty.
- Best: 9,282, four distinct ties (md5 b19142a0, cd42bdb9, d27dc4ad, 39760016; `keep/`, `lns/`), all with
  9,282's exact signature. Best tour with a new order but the same sitter set: 9,277 (on 9,279's own signature), 9,273, 9,270,
  9,267, 9,263, 9,244. Best tour holding a new sitter set: 9,169 (A_U_b6b7c8_C, one sitter moved); kept sets score
  8,519-9,136 even at penalty 120.
- No new family survived plain LNS: every rebuilt tour that climbed above about 9,200 had put back the record's sitter
  set (sitter difference 0). Restoring one moved sitter is worth more than 40 per sit, and kept sets stay 150-750 below.
  Merge bans (f3/f7, g6/h3 in moves 40-90) give no feasible tour; multi-block bans of 4 or more squares need 300-360 s of dlns
  or the partially penalised best.
- The a-d sitter set with the f3/f7 g6/h3 merge looks like a deep attractor. The 9,279 line (d3/g6 f7/h3 merge) and the
  splices R9089-R9222 share it.
- Recommendations: (1) change the merge rather than the dance: the 9,080 -> 9,268 jump came from early sits. Forcing a
  merge with an *extra* early sit (reward rather than ban, e.g. a bonus for sits on e/f/g squares in moves 40-90)
  needs a dlns change: dlns can only penalise. (2) A sitter-set similarity penalty, i.e. pay for every sit on 9,282's
  sitter set rather than banning a few squares, so LNS cannot just restore them one at a time. (3) Longer keep runs
  (an hour or more) on the one set that held at pen 120 with a real gain (KR22, c4/c5-free) are unlikely to pay: it did not
  climb in 1,800 s.
