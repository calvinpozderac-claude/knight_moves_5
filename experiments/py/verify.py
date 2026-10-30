import sys, pickle
from sim import simulate, necessity_check, MAIN_ROWS
moves = pickle.load(open(sys.argv[1], 'rb'))
ok, T, msg = simulate(MAIN_ROWS, moves)
print('moves', len(moves), 'ok', ok, 'T', T, msg)
bad = necessity_check(MAIN_ROWS, moves)
print('unnecessary sinks at moves:', bad)
from collections import Counter
c = Counter(q for _, q in moves); c['a1'] += 1
print('max visits (excluding h8):', max(v for k, v in c.items() if k != 'h8'), 'h8 visits', c['h8'])
print(', '.join(f'({t}, {q})' for t, q in moves))
