# Lean formalization

Machine-checked proofs, in Lean 4 with Mathlib, of the results in `proofs/UPPER_BOUND.md`.

| File | Contents |
|---|---|
| `KnightMoves5/Rules.lean` | The rules, over exact rationals. Covers the board, the sinking group and the opposite-point rule, legal jumps, visits ≤ 3, the end at the first arrival on h8, the puzzle's necessity rule (`Necessary`, `Valid`), and strict tours (`Strict`) |
| `KnightMoves5/Lemmas.lean` | Runs split over `++`. A point and its opposite are never one jump apart. A strict wait is never free (`strict_allNecessary`) |
| `KnightMoves5/Record.lean` | The 9,329-minute record as data, and `record9329_valid` (it obeys every rule, is strict, and lasts 9,329 minutes) |
| `KnightMoves5/StrictBound.lean` | **Theorem 2** `strict_bound`: every strict tour lasts at most 24,329 minutes. By length, `strict_bound_length`: a strict tour of M moves lasts at most 132·M − 619 |
| `KnightMoves5/Unbounded.lean` | **Theorem 1** `no_longest_tour` and `unbounded`: for every N there is a tour obeying every rule that lasts more than N minutes. Also `every_length_above_574` (every length above 574 minutes is achieved exactly) and `longTour_100288`, a concrete 100,288-minute tour |
| `KnightMoves5/Pinned.lean` | `strict_iff_strictSem`: `Strict` is exactly "every positive wait is pinned by its own jump", meaning every change of the wait by less than n/2 minutes breaks that jump |
| `KnightMoves5/Certify.lean` | `valid_of_certificate`: reduces `Valid tour` for a concrete tour to kernel-decidable checks |
| `KnightMoves5/Certs/R8348.lean` | Example certificate: the 8,348 tour, whose 3 non-strict waits are each broken by an explicit t′. Not in the default build: `lake build KnightMoves5.Certs.R8348` |
| `KnightMoves5/General.lean` | The same rules on an n × n board with any start and target. `toGen_valid` proves that the 8 × 8 rules are exactly the instance n = 8 |
| `KnightMoves5/Example4.lean` | The puzzle's own 4 × 4 worked example (19 moves, 28 minutes) obeys the rules (`valid`). Three of its waits are pinned only by later moves (`waits_5_6_7_not_strict`), so the example itself relies on the literal reading of the necessity rule |
| `test/DiffTest.lean` | Differential test: on 200 tours (real ones and mutants), Lean's `MovesOK` agrees with the Python simulator. Each verdict is proved by the kernel. Regenerate with `python3 test/gen_difftest.py` |
| `KnightMoves5/Main.lean` | The main statements in one place |

The main statements:

```lean
theorem no_longest_tour : ¬ ∃ best, Valid best ∧ ∀ tour, Valid tour → time tour ≤ time best
theorem unbounded (N : ℚ) : ∃ tour, Valid tour ∧ N < time tour
theorem strict_bound (tour : List Move) (hL : Legal tour) (hS : Strict start tour) : time tour ≤ 24329
theorem record9329_valid : Valid record9329 ∧ Strict start record9329 ∧ time record9329 = 9329
```

`Valid tour` is the puzzle's rules exactly as worded:
- all waits are ≥ 0 and all jumps are legal;
- the tour ends on its first arrival at h8;
- every other point is visited at most 3 times, the start on a1 included;
- every wait t > 0 has some t′ ≥ 0 that makes the tour illegal.

**Trust.** Every theorem depends only on `propext`, `Classical.choice` and `Quot.sound`. Check with
`#print axioms`. There is no `sorry` and no `native_decide`. Concrete tours are evaluated by the
kernel with `decide +kernel`, in exact rational arithmetic. The record takes about 40 s.

## Build

Lean `v4.37.0-rc1` (`lean-toolchain`), with Mathlib pinned in `lake-manifest.json`.

```
lake exe cache get   # prebuilt Mathlib (needs cache.mathlib.org)
lake build           # about 2 minutes
```

**Certify any tour:**
```
python3 tools/certify.py ../data/tours/record_9329.txt R9329 > KnightMoves5/Certs/R9329.lean
lake build KnightMoves5.Certs.R9329        # about 45 s for a record-length tour
```
The script finds, with the exact Python simulator, a breaking t′ for every non-strict wait. The
generated file proves `Valid tour` and `time tour = T`, with every check run by the kernel.
