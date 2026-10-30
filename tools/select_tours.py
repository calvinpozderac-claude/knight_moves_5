"""Pick a balanced, diverse curriculum set from verified tours (output of tools/harvest_tours.py).

    python3 tools/select_tours.py IN.txt OUT.txt [--bucket 500] [--per-bucket 30] [--seed 0]

Tours are grouped by final minutes into buckets (0-500, 500-1000, ...). From each bucket at most
--per-bucket tours are kept, preferring tours whose first 20 moves differ from every tour already
chosen, so the curriculum's start positions are as varied as possible. Only tours with whole-unit
sinks (replayable by build/az) are used. OUT.txt: "<minutes> <jumps> <tour>" per line.
"""
import argparse, random, re
from collections import Counter, defaultdict

ap = argparse.ArgumentParser()
ap.add_argument('inp'); ap.add_argument('out')
ap.add_argument('--bucket', type=int, default=500)
ap.add_argument('--per-bucket', type=int, default=30)
ap.add_argument('--prefix', type=int, default=20, help='moves that define a distinct start')
ap.add_argument('--seed', type=int, default=0)
a = ap.parse_args()

tours = {}
for line in open(a.inp):
    parts = line.split(' ', 3)
    if len(parts) < 4 or parts[2] != 'int':
        continue
    T, n, tour = int(float(parts[0])), int(parts[1]), parts[3].strip()
    tours[tour] = (T, n)

rng = random.Random(a.seed)
by_bucket = defaultdict(list)
for tour, (T, n) in tours.items():
    by_bucket[T // a.bucket].append(tour)

def prefix(tour):
    return tuple(re.findall(r'\(\d+, [a-h][1-8]\)', tour)[:a.prefix])

chosen, seen = [], set()
for b in sorted(by_bucket):
    pool = by_bucket[b][:]
    rng.shuffle(pool)
    fresh = [t for t in pool if prefix(t) not in seen]
    # first pass: distinct prefixes; second pass: fill up with the rest
    pick = []
    for t in fresh:
        if len(pick) >= a.per_bucket: break
        if prefix(t) not in seen:
            pick.append(t); seen.add(prefix(t))
    for t in pool:
        if len(pick) >= a.per_bucket: break
        if t not in pick:
            pick.append(t)
    chosen += pick

chosen.sort(key=lambda t: tours[t][0])
with open(a.out, 'w') as f:
    for t in chosen:
        T, n = tours[t]
        f.write(f'{T} {n} {t}\n')
cnt = Counter(tours[t][0] // a.bucket for t in chosen)
print(f'{len(tours)} tours in, {len(chosen)} chosen; distinct {a.prefix}-move starts: {len({prefix(t) for t in chosen})}; '
      f'distinct 10-move openings: {len({prefix(t)[:10] for t in chosen})}')
print('per bucket: ' + ', '.join(f'{b * a.bucket}-{(b + 1) * a.bucket}: {cnt[b]}' for b in sorted(cnt)))
