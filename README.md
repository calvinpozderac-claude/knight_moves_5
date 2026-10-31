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
| `data/tours/record_7200.txt` | **7,200** | NRPA (`build/nrpa`), warm-started from the 7,066 tour: 6 minutes on 2 threads |
| `data/tours/record_7066.txt` | 7,066 | AlphaZero self-play seeded from the 6,303 tour, alternated with beam search guided by its network, over three rounds |
| `data/tours/tour_6303.txt` | 6,303 | Beam search (`experiments/py/beam*.py`) |
| `data/tours/from_scratch_2952.txt` | 2,952 | AlphaZero from scratch, starting after the 23-move opening |

Check them all exactly (fractions, every rule) with `python3 tools/verify_tours.py`.

## Layout

```
src/eng.h, src/az.cpp      game engine and tree search (self-play, eval, beam, improve)
src/opening_search.cpp     finds varied openings that can still finish
src/nrpa.cpp               Nested Rollout Policy Adaptation search (best for pushing the record)
train/az_train.py          training loop (NumPy; or PyTorch with --backend torch)
train/torch_backend.py     GPU trainer: same network, same Adam, same weight file
tools/                     exact checker, tour verifier, tour harvester and selector, PyTorch parity check
data/                      the 23-move opening, the best tours, and the curriculum tour sets
scripts/                   Ubuntu setup; generating more curriculum tours
weights/                   trained networks (from_scratch_net.bin, seeded_record_net.bin)
results/logs/              training logs from the session
viz/                       2D and 3D view of the tours (open the .html in a browser)
experiments/               every other search program from the session, as-is
```

## Setup

### Ubuntu with an NVIDIA GPU (for example a Ryzen 9 9950X with an RTX 5070 Ti)

1. **Install the NVIDIA driver (once).** RTX 50-series cards need NVIDIA's open kernel
   modules, version 570 or newer. The PyTorch build that pip installs uses CUDA 13, which
   needs driver 580 or newer.
   ```
   nvidia-smi                     # if this already lists the card with driver 580 or newer, skip to step 2
   sudo ubuntu-drivers install    # installs Ubuntu's recommended open driver, replacing any other NVIDIA driver
   sudo systemctl reboot -i       # or restart from the desktop; a plain `sudo reboot` can be blocked by a logged-in desktop
   nvidia-smi                     # should list the card and "CUDA Version: 13.x" or higher
   ```
   Before the reboot, `nvidia-smi` reports "Driver/library version mismatch". That only means
   the new driver isn't loaded yet.
2. **Get the tools and code, and set up Python.**
   ```
   sudo apt install -y build-essential git python3-venv python3-dev tmux
   git clone https://github.com/calvinpozderac-claude/knight_moves_5 && cd knight_moves_5
   bash scripts/setup_ubuntu.sh
   ```
   The script:
   - builds `build/az`
   - creates `.venv` with NumPy and PyTorch
   - checks that PyTorch can use the GPU
   - checks the GPU trainer against the NumPy one
3. **Train inside tmux,** so training continues if you close the terminal. Detach with
   Ctrl-b d, and come back with `tmux attach -t knight`.
   ```
   tmux new -s knight
   source .venv/bin/activate
   python3 train/az_train.py --run runs/a1_dist1 --start a1 --dist-k 1 --backend torch --device cuda
   ```
   Watch progress with `tail -f runs/a1_dist1/log.txt`. After a reboot, run the same command
   with `--resume` added.

### Other platforms

You need a C++17 compiler, Python 3.9+, and NumPy. On other Linux distributions, macOS, or
WSL2 (recommended on Windows), run `make` to build `build/az`, then `pip install numpy`.

**Windows without WSL:** use CMake with Visual Studio 2022, then run the trainer with `python`
instead of `python3`. It finds `build/Release/az.exe` on its own. This route hasn't been
tested; WSL2 is the safer bet.
```
cmake -B build
cmake --build build --config Release
```

**GPU (optional):** install a CUDA build of PyTorch, then check that its trainer matches the
NumPy one:
```
pip install torch          # on Linux this is a CUDA 13 build, which supports RTX 50-series cards
python3 tools/parity_check.py
```

## Searching for longer tours with NRPA (recommended for the record)

AlphaZero is built for two-player games. It learns a reusable policy over millions of positions.
This puzzle asks for one excellent sequence, in a one-player game with no chance. Nested Rollout
Policy Adaptation (NRPA, Rosin 2011) is a standard method for that kind of problem; it holds
records on single-player puzzles such as Morpion Solitaire. It needs no network, and it uses
every CPU core.

- **Rollouts:** each plays a whole tour, choosing moves by learned weights.
- **Nesting:** at every level it runs `--iters` searches one level down, then shifts the weights
  toward the best sequence so far.
- **Puzzle-specific details:**
  - A rollout that passes a chance to finish keeps it: its tour ends at the last point where h8
    was reachable.
  - Unfinished rollouts rank below all finished ones, by minutes × exp(−distance to h8).
- **Seeding:** each restart can start from a known tour (`--seed-tours`) or from the best so far
  (`--seed-best P`, the share of restarts that do). It shifts the weights toward that tour and
  begins with it as its best, so the search can only improve on it.
- **Without seeds** it doesn't find h8 from a1. That's what `build/opening_search` and the
  curriculum tour set are for.

Run from the repo root (`make` builds `build/nrpa`):
```
mkdir -p runs
# polish the record on all cores for 2 hours (level 3: one restart is about a million rollouts)
build/nrpa --level 3 --threads 32 --time 7200 --seed-tours data/tours/record_7200.txt --seed-best 1 --out runs/nrpa_best.txt
# explore from all 654 openings as well, then polish the best
build/nrpa --level 3 --threads 32 --time 7200 --seed-tours data/tours/curriculum.txt --seed-best 0.5 --out runs/nrpa_wide.txt
```
- **Output:** every improvement is written to `--out` (line 1 minutes, line 2 the tour). Check it
  with `python3 tools/check_tour.py "$(sed -n 2p runs/nrpa_best.txt)"`.
- **Speed:** about 15,000 rollouts per second per thread.
- **Levels:** level 2 restarts take about a second each. Level 3 takes 1–2 minutes and searches
  much more thoroughly. Level 4 runs for hours.
- **Other options:** `--iters` (100) and `--alpha` (1) are NRPA's usual settings. `--seed-adapt`
  (10) sets how strongly a seed tour biases the weights.

## Training

Run from the repo root. Each run directory gets `log.txt`, `log.csv`, `best_overall.txt` (the
best finished tour), the current weights `w_cur.bin`, a snapshot every 10 rounds, and its
settings. Add `--resume` to continue a run; other options given with `--resume` override the saved
settings (except the network size).

**Current approach: a reverse curriculum from known tours.** `data/tours/curriculum.txt` holds
1,272 complete tours with 654 different openings. Every one is checked exactly with
`tools/harvest_tours.py`.
- **Range:** 10 to 7,066 minutes and 15 to 135 jumps.
- **Harvested (114 tours):** found in the logs, searches, and training runs from the session.
- **Network-generated (518 tours):** the trained networks branching off those tours at random
  points, and playing from the opening with randomness. Mostly long tours, which all use the
  record's opening.
- **Opening search (640 tours, also in `data/tours/openings.txt`):** `build/opening_search`
  plays random 12–30-move openings from a1, then searches for a finish. It walks without
  waiting, sits to bring a square next to h8 to the right height, and repeats up to three
  times. About 1 random opening in 2,000 can still finish. These tours are short (10–148
  minutes), and nearly every one has its own opening.

Each game starts K moves into a tour picked at random. Every `--curriculum-every` rounds, K drops
by 1, until games start at a1:
```
python3 train/az_train.py --run runs/curriculum --start-tours data/tours/curriculum.txt \
    --curriculum-start 20 --curriculum-every 10 --backend torch --device cuda
```
- **Picking.** By default every tour is equally likely, which puts about 44% of games on the
  record's opening (where the long tours are) and the rest across 653 other openings. Lower
  `--curriculum-balance` toward 0 to weight openings more evenly instead.
- **End mode.** `--curriculum-mode end --curriculum-start 1` starts each game 1 move before
  that tour's finish, and moves the start back 1 move per step. This is the classic reverse
  curriculum: finishes come early, and it takes longer to reach a1.
- **Pacing.** `--curriculum-min-finish 0.5` steps back only once at least half the games
  finish.
- **Reading the log.** Each log line ends with where games started, e.g. `start start12` or
  `start a1`. Until it says `a1`, a "best" tour shares its first K moves with a known tour.

To add more tours, run `bash scripts/generate_tours.sh [games_per_job] [parallel_jobs]`. It
generates new tours with the networks, verifies them, and merges them into the set.

**A distance-weighted target.** Every game starts at a1, T=0, with no opening and no restarts
from the best tour. It uses the original exploration settings (C = 1.5;
τ = 1 for the first 25 moves, then greedy).
- A finished tour scores its minutes, as in the puzzle.
- A stuck tour scores minutes × exp(−d). Here d is the straight-line 3D distance (x, y,
  altitude) from the knight to h8, scaled so d = 1 at the start.

So a long tour counts more the closer it ends to h8:
```
python3 train/az_train.py --run runs/a1_dist1 --start a1 --dist-k 1
```
A first try with exp(−5d) (`--dist-k 5`) was too steep. A stuck tour at the start keeps 0.7%,
and the agent's games ended around d = 2.5, keeping about 0.0004%. That amounted to "stuck = 0"
again, and over 110 rounds the knight kept drifting away from h8.
`<run>/best_target.txt` holds the tour with the highest target so far, finished or not.

**Before that: flat partial credit, which never finished.** Two runs let stuck tours keep 50%
of their minutes (`--credit 0.5`, plus `--cpuct 4.5` or `--tau1 2 --tau2 1`). They ran 684 and
892 rounds, about 22,000 and 28,500 games, and neither reached h8. The agents learned to sink
their own group for minutes and ended a median 30–48 units below h8. Logs: `results/logs/06_*`.

For comparison, with the opening and restarts (`05_*`), all three higher-exploration settings
reached h8 in their first round. The original settings took 14 rounds.

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
| `--dist-k` | 0 | if > 0, a stuck tour keeps exp(−k·d) of its minutes instead (d = scaled 3D distance to h8) |
| `--start-tours` | none | tour file for the curriculum (one tour per line) |
| `--curriculum-mode`, `--curriculum-start`, `--curriculum-every` | start, 20, 10 | where games start (K moves in, or K before the finish) and how often K steps |
| `--curriculum-min-finish`, `--curriculum-balance` | 0, 1 | finish rate needed to step; how evenly openings are picked |
| `--restart-prob`, `--seed-tour` | 0, none | Go-Explore restarts from a random point on the best tour |
| `--self-imitation` | 0 | copies of the best tour added to the training data each round |
| `--backend`, `--device` | numpy, cuda | train in NumPy on the CPU, or in PyTorch |
| `--gpu-selfplay`, `--infer-leaves` | off, 8 | evaluate self-play positions in batches in the trainer (GPU with torch) |
| `--hidden`, `--layers` | 256, 2 | network width and number of hidden layers |
| `--classic`, `--q-max-weight` | off, 0.5 | classic AlphaZero averaging; weight of the best result in node values |
| `--reuse` | off | training steps per round = reuse × new positions / batch (about how many times each position is trained on); guards against overfitting |
| `--no-follow-exact` | off | single-player: don't make near-greedy moves follow the best proven line |
| `--est-weight` | 0.5 | share of the search's own estimate (when above the achieved result) in value targets; 0 = achieved results only |
| `--val-frac`, `--val-size` | 0.05, 20000 | games held out for the validation loss (`vlp`, `vlv` in the log) |

### On a Ryzen 9 9950X with an RTX 5070 Ti

- **Self-play is most of the time, and it runs on the CPU.** The tree search evaluates the
  network inside `az`, one position at a time, so it scales with `--workers`. Try 16 (one per
  physical core) and 32, and keep whichever finishes rounds faster. In the session, 2 cloud
  cores took 10–40 seconds per round.
- **With more cores, consider more games per round.** For example, `--games 64 --steps 300`
  keeps the same ratio of training to fresh data.
- **Put the network on the GPU for self-play too:** `--gpu-selfplay --backend torch --device cuda`.
  - Each worker plays all its games at once and sends its leaf positions to the trainer
    process, which evaluates them in large batches on the GPU with the same network it trains.
    The workers keep only the tree search on the CPU.
  - Give it many games per round (`--games 512` gives each of 16 workers 32 at once).
  - `--infer-leaves` (default 8) sets how many positions each game sends per batch. Raise it
    for bigger batches.
- **Bigger networks:** `--hidden` (width, default 256) and `--layers` (hidden layers,
  default 2). Without `--gpu-selfplay`, a bigger network slows the CPU self-play.
  - A run's network size is fixed when the run starts, so use a new `--run` directory to
    change it.
  - The included weights are 256 × 2.

A run that uses both cards fully:
```
python3 train/az_train.py --run runs/gpu_curriculum --start-tours data/tours/curriculum.txt \
    --curriculum-mode end --curriculum-start 1 --curriculum-every 10 --curriculum-min-finish 0.5 \
    --hidden 512 --layers 4 --games 512 --steps 600 --batch 1024 --buffer 500000 \
    --gpu-selfplay --backend torch --device cuda
```

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
- **Network:** by default two hidden layers of 256 units, about 435,000 parameters (set with
  `--hidden` and `--layers`). It outputs 24 move logits and a value: the expected minutes
  still to come.
- **Search:** PUCT, with values rescaled to 0–1.
  - Each wait earns its minutes / 10,000.
  - Reaching h8 ends the game.
  - Getting stuck forfeits every minute so far, or all but the `--credit` fraction.
- **Single-player search (default; `--classic` turns it off).** This is a one-player
  deterministic game, so it doesn't average the way two-player AlphaZero does:
  - **Node values** blend the average and the best result found (`--q-max-weight`, default
    0.5). A move is never rated below a line proven to reach a real end, since that line can
    always be replayed exactly.
  - **Solved subtrees:** once every move under a node has been searched to the end, its exact
    best value is final. The search stops visiting it, and passes "solved" upward. A solved
    root means the best move is known, and it gets played.
  - **Tree reuse:** the subtree under the chosen move becomes the next root.
  - **Caches** (per worker, keyed on the full position including the clock): network outputs
    are never recomputed, and solved values are reused across moves and games.
  - **Tours found inside the search:** every h8 the search reaches is a real tour. The best one
    per game is reported (`found` in the log) and saved, even if the moves played went
    elsewhere.
- **Value target:**
  - **Single-player:** the best result actually achieved from the position is a floor. That's
    what was played, plus any exact line that this search or a later one found, passed back
    along the game. The search's estimate can only raise the target, by half the difference.
  - **Classic:** half the actual remaining minutes, half the search's average at the root.
- **Policy target:** the share of search visits each move got, or the proven best move when the
  root is solved.
