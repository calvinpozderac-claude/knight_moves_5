# explore: new strict tour families (started 2026-11-21 19:06 UTC, agent freesit)
Goal: completely new tour families (not polishing 9330). Strict only: saver.sh/newstrict.py (threshold 9330) writes
STRICT_RECORD_<m>.txt only if tools/check_tour.py ok (unnecessary=[]) AND every positive wait is pinned.

## Tools
- desc.py: family descriptor (dance start ds = first state >= move 40 with >= 61 points on the knight's level + the denser level
  +-2; dance level pair (absolute altitudes); sitter set = squares with a positive wait at/after ds; first 15 squares; first move).
- gather.py -> arch/all_strict.tsv: 1814 distinct strict tours >= 8000 found anywhere in the repo (9084 distinct tour lines).
- arch.py: MAP-Elites archive; cell = (level pair, first move, sitter cluster [Jaccard >= 0.8 to a cluster representative]);
  arch/families.tsv = best per cell (+ Jaccard to the 9330 sitters), arch/cells/*.txt = tour files.
- nov.cpp/nov: freesit's strict sfl (record() rejects free sinks, --free 0) + novelty penalty:
  --novp P --novsit <9330 sitters> --novj J: penalty P/4 + P*(J-thr)/(1-thr) when sitter Jaccard to the record > J;
  --novlv P l1,l2..: penalty P when the lower dance level is one of the record family's (-12,-11,-10,-9);
  --novfirst P sq: penalty when the first move is sq. Penalty enters every acceptance (adjusted score), as --vpen does.
- run.sh (novelty LNS job), worker.sh/queue.txt (3 workers), NRPA unseeded level 3 (build/nrpa, 30 min per rng) on core 4.

## Families in the repo before this phase (arch/families.tsv, strict)
Record family: levels (-12..-9) c2 start, sitter Jaccard 0.94-1.0 (9150-9330). Other families: (-5,-3) c2 9080 (J 0.75);
(-4,-2) c2 8887; (-3,-1) c2 8703; (-3,-1)/( -5,-3) b1 8604-8607 (J 0.71); (-2,0) b1 8297 (J 0.27); (0,2) b1 8162 (J 0.29).

## Log
- Round A: 18 novelty LNS jobs (40k it, t0 60, whole tour, novj 0.8, record levels forbidden at 3000) from the 3 most diverse
  seeds of each non-record family (in/f_*.txt), donors = same family only. NRPA unseeded rng 11-18 on core 4.
- NRPA level 3 unseeded: no finished tour in 8 min (unfinished ~1200 min), as in the README (unseeded searches never find the climb trick); stopped, core 4 -> LNS worker. Fixed seed files whose tour field had been split on a tab (5 seeds; 1 dropped: checker-invalid).
- 19:24 Round A (plain novelty LNS) gives no gain on these seeds (8297, 8607, 8127 unchanged after 40k it): they are LNS local optima. Remaining A jobs dropped. Round B: family-jump chains pchain.sh (1200 s with visit penalty 300 on 3-6 random own dance sitters after ds + novelty, then 1200 s relax with novelty only), one per seed (17); second set kept in qB1.txt.
- 20:30 beamcells.sh: dance block beam from ds-4/ds/ds+4 of every archive cell: record-family cells unchanged (9330, 9282) or +5 (9180->9185); perturbed new-family tours gain a lot (7935->8484, 7369->7715, 7214->7487, 7274->7426), 8946->8962. pchain.sh now: perturb -> block beam -> relax (applies to the queued B1 chains).
