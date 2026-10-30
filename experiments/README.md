# Experiments (as-is)

Exploratory searches from the session that produced the tours in `data/` and the answers in
`tools/verify_tours.py`. They're kept for reference, not polished: paths, constants and
start states are hard-coded, and some write output files into the working directory.

Build a C++ one from this folder (it uses the copies of `eng.h` and `phaseA.txt` here):

    g++ -O3 -march=native -std=c++17 -o maxx maxx.cpp

Run the Python ones from `py/` (they import each other and `sim.py`).

## C++ (`cpp/`)
| Files | What it does |
|---|---|
| `maxx`, `maxx2` | Beam search for the highest finishing altitude of h8 |
| `ramp`, `mramp*`, `potimp`, `ctg*`, `target*`, `mtarget`, `mclimb*`, `climb` | Searches that shape the terrain into a "final ramp" for a high finish |
| `corner*`, `improve` | Exhaustive macro DFS (walk, sit once, leave) and iterated improvement |
| `grp` | Largest same-altitude group at the finish |
| `uniq`, `uniq2` | Tours that never repeat a point (longest and most jumps) |
| `halfmacro*` | Searches allowing half- and third-unit sinks |

## Python (`py/`)
| Files | What it does |
|---|---|
| `core.py`, `st.py`, `sim.py` | Integer-altitude engine, state helpers, exact Fraction simulator |
| `phaseA*.py` | The 23-move opening that puts h8 into the knight's sinking group |
| `beam*.py` | Beam searches for long tours (the 6,303-minute tour came from these) |
| `macro*.py`, `designer.py`, `raises*.py` | Macro searches and final-ramp design |
| `cpsat.py` | CP-SAT model of the tour (OR-Tools) |
| `verify.py`, `knight_tour_verify.py`, `check_pub.py` | Older checkers |

## History (`history/`)
The first AlphaZero version, which scored only finished tours (terminal value). It plateaued at
about 1,500 minutes (2,282 with restarts), before the per-wait reward version in `src/az.cpp`.
