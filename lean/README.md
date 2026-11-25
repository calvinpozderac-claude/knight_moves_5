# Lean formalization

Machine-checked proofs, in Lean 4 with Mathlib, of the results in `proofs/UPPER_BOUND.md`.

| File | Contents |
|---|---|
| `KnightMoves5/Rules.lean` | The rules, over exact rationals. Covers the board, the sinking group and the opposite-point rule, legal jumps, visits ≤ 3, the end at the first arrival on h8, the puzzle's necessity rule (`Necessary`, `Valid`), and strict tours (`Strict`) |
| `KnightMoves5/Lemmas.lean` | Runs split over `++`. A point and its opposite are never one jump apart. A strict wait is never free (`strict_allNecessary`) |
| `KnightMoves5/Record.lean` | The 9,329-minute record as data, and `record9329_valid` (it obeys every rule, is strict, and lasts 9,329 minutes) |
| `KnightMoves5/StrictBound.lean` | **Theorem 2** `strict_bound`: every strict tour lasts at most 24,329 minutes |
| `KnightMoves5/Unbounded.lean` | **Theorem 1** `unbounded`: for every N there is a tour obeying every rule that lasts more than N minutes. Also `every_length_above_574` (every length above 574 minutes is achieved exactly) and `longTour_100288`, a concrete 100,288-minute tour |
| `KnightMoves5/Main.lean` | The main statements in one place |

The main statements:

```lean
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

`tools/gen_tour.py NAME TOURFILE DOC` turns a tour file into a Lean `List Move`, for checking
another tour the way `Record.lean` checks the record.
