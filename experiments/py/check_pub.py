from sim import *
s = "(0, b1), (0, b3), (0, c3), (0, a2), (9, a3), (0, a4), (0, a5), (0, c4), (0, b4), (0, b6), (0, b7), (0, c7), (6, a8), (18, b6), (0, d5), (0, f4), (0, g2), (0, h4), (0, f3), (68, d3), (0, b3), (95, a3), (0, a4), (0, a5), (0, c4), (0, e4), (2, e5), (0, e6), (0, e7), (0, f7), (0, g7), (1, h7), (0, h8)"
import re
moves = [(int(a), b) for a, b in re.findall(r'\((\d+),\s*([a-h]\d)\)', s)]
print(len(moves))
print(simulate(MAIN_ROWS, moves, verbose=True))
print('unnecessary:', necessity_check(MAIN_ROWS, moves))
