# Knight Moves 5: an AlphaZero agent for the swamp-knight puzzle

Code for Jane Street's "Knight Moves 5" puzzle, aimed at the longest possible tour. It has an
exact tour checker, the search programs that found the record tours, and an AlphaZero-style
agent: C++ tree search plus a small neural network trained by self-play.

## The puzzle

- **Board:** an 8x8 lattice of altitudes. The knight starts on a1 at minute 0 and must reach h8.
- **Jumps:** each jump changes the coordinates by a permutation of (0, ±1, ±2). That is a
  knight move on level ground, one step with a ±2 altitude change, or two steps in a line with
  a ±1 altitude change.
- **Sinking:** while the knight waits on a point, every point at that altitude sinks together,
  at 1 unit per n minutes for a group of n points. The diametrically opposite point rises at the
  same rate, unless it is in the group.
- **Constraints:**
  - Every wait must be necessary.
  - Points other than h8 can be visited at most 3 times (the start counts).
  - The tour ends on the first arrival at h8.
- **Goal:** maximize the total time.

## Results so far

| Tour | Minutes | How it was found |
|---|---|---|
| `data/tours/record_7066.txt` | **7,066** | AlphaZero self-play seeded from the 6,303 tour, alternated with beam search guided by its network, over three rounds |
| `data/tours/tour_6303.txt` | 6,303 | Beam search (`experiments/py/beam*.py`) |
| `data/tours/from_scratch_2952.txt` | 2,952 | AlphaZero from scratch, starting after the 23-move opening |

Check them all exactly (fractions, every rule) with `python3 tools/verify_tours.py`.

## Layout

```
src/eng.h, src/az.cpp      game engine and tree search (self-play, eval, beam, improve)
train/az_train.py          training loop (NumPy; or PyTorch with --backend torch)
train/torch_backend.py     GPU trainer: same network, same Adam, same weight file
tools/                     exact checker, tour verifier, PyTorch parity check
data/                      the 23-move opening and the best tours
weights/                   trained networks (from_scratch_net.bin, seeded_record_net.bin)
results/logs/              training logs from the session
viz/                       2D and 3D view of the tours (open the .html in a browser)
experiments/               every other search program from the session, as-is
```

## Setup

You need a C++17 compiler, Python 3.9+, and NumPy.

**Linux, macOS, or WSL2 (recommended on Windows):**
```
sudo apt install build-essential python3-numpy   # Ubuntu / WSL2
make                                             # builds build/az
```

**Windows without WSL:** use CMake with Visual Studio 2022, then run the trainer with `python`
instead of `python3`. It finds `build/Release/az.exe` on its own. This route hasn't been
tested; WSL2 is the safer bet.
```
cmake -B build
cmake --build build --config Release
```

**GPU (optional):** install PyTorch with CUDA, then check that its trainer matches the NumPy one:
```
pip install torch          # RTX 50-series cards need a CUDA 12.8 or newer build
python3 tools/parity_check.py
```

## Training

Run from the repo root. Each run directory gets `log.txt`, `log.csv`, `best_overall.txt` (the
best finished tour), the current weights `w_cur.bin`, a snapshot every 10 rounds, and its
settings. Add `--resume` to continue a run.

The current experiment has two settings in common:
- Every game starts at a1, T=0, with no opening and no restarts from the best tour.
- Partial credit: a stuck tour keeps half its minutes until the agent first reaches h8. That
  credit then fades to zero over 60 rounds.

The two runs differ only in how hard they explore:
```
python3 train/az_train.py --run runs/a1_tau --start a1 --credit 0.5 --tau1 2 --tau2 1 --workers 16
python3 train/az_train.py --run runs/a1_C   --start a1 --credit 0.5 --cpuct 4.5     --workers 16
```

In the session, on 2 cloud cores, these ran for 31 and 26 rounds before the workspace
restarted. Neither had reached h8 yet, but both were surviving much longer:
- Games grew from about 7 moves (untrained) to about 40.
- Stuck tours averaged about 1,000 and 800 minutes.
- The longest stuck tours reached 2,697 and 3,728 minutes.

Logs: `results/logs/06_*`. For comparison, with the opening and restarts (`05_*`), all three
higher-exploration settings reached h8 in their first round. The original settings took 14
rounds.

The main options (`--help` lists them all):

| Option | Default | Meaning |
|---|---|---|
| `--workers` | half your logical CPUs | parallel self-play processes |
| `--games`, `--sims` | 32, 1600 | games per round; tree-search simulations per move |
| `--steps`, `--batch` | 150, 256 | training steps per round, and their batch size |
| `--start` | `a1` | `a1`, or `opening` (start after the fixed 23-move opening) |
| `--cpuct` | 1.5 | exploration constant C (move values are rescaled to 0–1 in the tree) |
| `--tau1`, `--tau2`, `--temp-moves` | 1, 0, 25 | move temperature for the first 25 moves, then for the rest (0 = always the most-visited move) |
| `--dir-alpha`, `--dir-eps` | 0.3, 0.25 | Dirichlet noise on the root move probabilities |
| `--credit`, `--credit-ramp` | 0, 60 | partial credit for stuck tours (0 = the puzzle's rules) |
| `--restart-prob`, `--seed-tour` | 0, none | Go-Explore restarts from a random point on the best tour |
| `--self-imitation` | 0 | copies of the best tour added to the training data each round |
| `--backend`, `--device` | numpy, cuda | train in NumPy on the CPU, or in PyTorch |

### On a Ryzen 9 9950X with an RTX 5070 Ti

- **Self-play is most of the time, and it runs on the CPU.** The tree search evaluates the
  network inside `az`, one position at a time, so it scales with `--workers`. Try 16 (one per
  physical core) and 32, and keep whichever finishes rounds faster. In the session, 2 cloud
  cores took 10–40 seconds per round.
- **With more cores, consider more games per round.** For example, `--games 64 --steps 300`
  keeps the same ratio of training to fresh data.
- **The GPU only speeds up the training step.** Use `--backend torch --device cuda`. To use the
  GPU during self-play, the search would need batching: evaluating leaf positions from many
  games at once. That's a bigger change, and it only pays off with a larger network.

### Reproducing the earlier runs

- **The original from-scratch run (2,952):**
  `--start opening --restart-prob 0.5 --workers 2 --games 32 --rounds 127`.
  In that run, restarts began before the first finish, from an early stuck tour; here they wait
  for a finished tour.
- **The exploration test:** the same command, plus `--cpuct 4.5`, or `--tau1 2 --tau2 1`, or
  both. Logs: `results/logs/05_*`.
- **The record (7,066):**
  1. Seed self-play from the best tour and keep it in the training data:
     ```
     python3 train/az_train.py --run runs/seeded --start opening --init-weights weights/from_scratch_net.bin \
         --seed-tour data/tours/tour_6303.txt --restart-prob 0.9 --self-imitation 4 --rounds 100
     ```
  2. Run a beam search guided by the trained network:
     ```
     build/az beam runs/seeded/w_cur.bin 100000 0.3 beam.txt 1
     ```
  3. Start a new run seeded with whichever tour is better, and repeat.

### Other modes of `az`

Run these from the repo root, or set `OPENING_FILE`. They start after the opening; for `eval`,
set `OPENING=0` to start at a1 instead.

- `build/az eval <weights> <sims> <seed> <out.txt>`: one greedy game with a large search.
- `build/az beam <weights> <width> <lambda> <out.txt> 1`: beam search ranked by
  minutes + λ × 10000 × the network's value. The trailing 1 keeps only positions from which h8
  can still be entered, which assumes the opening start.
- `build/az improve <weights> <tour.txt> <sims> <stride> <beta> <out.txt>`: replays a tour and
  lets the search take over from points along it.

## Checking a tour

```
python3 tools/check_tour.py "(0, b1), (0, d1), (8, d2), ..."
python3 tools/verify_tours.py
```

`check_tour.py` reports whether the tour is legal, its total minutes, the most visits to any
point, and any unnecessary sinks. `verify_tours.py` checks every tour in `data/tours/`, plus the
four question tours (high finish, no repeats, all points equal, short waits).

## How the agent works

- **Moves:** 24 jump types. The wait before each jump is implied by the jump, so every wait is
  necessary by construction.
- **Network input** (1,418 features):
  - each point's altitude relative to the knight (clipped to ±8)
  - each point's visit count
  - the knight's square
  - 10 summary features
- **Network:** two hidden layers of 256 units. It outputs 24 move logits and a value: the
  expected minutes still to come.
- **Search:** PUCT, with values rescaled to 0–1.
  - Each wait earns its minutes / 10,000.
  - Reaching h8 ends the game.
  - Getting stuck forfeits every minute so far, or all but the `--credit` fraction.
- **Value target:** half the actual remaining minutes, half the search's own estimate at the root.
