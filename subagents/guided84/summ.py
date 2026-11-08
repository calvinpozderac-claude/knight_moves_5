# usage: python3 summ.py LOG...   per-log table of completed restarts by anchor band (merge 44-66, dance 67-100, 101-125, 126-150)
import sys, re, statistics as st
bands = [(1, 43, 'climb'), (44, 66, 'merge'), (67, 100, 'dance1'), (101, 125, 'dance2'), (126, 160, 'dance3')]
for f in sys.argv[1:]:
    rs = [(int(a), int(m)) for a, m, p in re.findall(r'restart \d+ \(anchor (\d+)[^)]*\): finished (\d+)[^\n]*?(PARTIAL)?\n', open(f).read()) if not p]
    print(f, 'completed', len(rs))
    for lo, hi, nm in bands:
        v = sorted(m for a, m in rs if lo <= a <= hi)
        if v: print(f'  {nm:7s} {lo}-{hi}: n={len(v):3d} median={st.median(v):6.0f} max={v[-1]} >=8400:{sum(x>=8400 for x in v)} >=8484:{sum(x>=8484 for x in v)}')
