#!/usr/bin/env python3
"""Per-run summary of completed restarts (PARTIAL dropped) with identical finishes counted once:
the pool line (minutes, tour) of a restart is not printed by pnrpa, so distinct = distinct minute values."""
import re, sys, glob, statistics as st
for fn in sys.argv[1:]:
    sc = [int(m.group(1)) if m.group(0).startswith('finished') else 0 for line in open(fn) if 'PARTIAL' not in line
          for m in [re.search(r'(?:finished|unfinished) (\d+) minutes', line)] if m and 'restart' in line]
    if not sc: print(f'{fn}: no completed restarts'); continue
    d = sorted(set(sc))
    print(f'{fn}: n={len(sc)} distinct={len(d)} median {st.median(sc):.0f} best {max(sc)}  distinct values {d[-6:]}')
