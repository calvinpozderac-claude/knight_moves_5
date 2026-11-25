import KnightMoves5.Unbounded
import KnightMoves5.StrictBound
import KnightMoves5.Pinned
import KnightMoves5.Example4

/-!
# Main results

* `KM5.no_longest_tour`: no tour that obeys every rule is a longest one.
* `KM5.unbounded`: under the puzzle's rules as worded (every wait `t > 0` must have some `t'` that
  makes a move of the tour illegal), there are legal tours longer than any given number of minutes.
* `KM5.longTour_100288`: an explicit 100,288-minute tour that obeys every rule.
* `KM5.every_length_above_574`: every length above 574 minutes is achieved exactly.
* `KM5.strict_bound_length`: a strict tour of `M` moves lasts at most `132·M − 619` minutes.
* `KM5.strict_bound`: a legal tour in which every wait is pinned by its own jump lasts at most
  24,329 minutes.
* `KM5.record9329_valid`: the 9,329-minute record obeys every rule and is strict.
* `KM5.strict_allNecessary`: strict waits are necessary in the puzzle's sense.
* `KM5.toGen_valid`: the 8 × 8 rules are the general `n × n` rules (`General.lean`) with `n = 8`.
* `KM5.Example4.valid`: the puzzle's own 4 × 4 worked example (28 minutes) obeys the general rules;
  three of its waits are pinned only by later moves (`waits_5_6_7_not_strict`).
* `KM5.strict_iff_strictSem`: `Strict` is exactly "every positive wait is pinned by its own jump"
  (every small change of the wait breaks that move's jump).
-/

namespace KM5

example : ¬ ∃ best, Valid best ∧ ∀ tour, Valid tour → time tour ≤ time best := no_longest_tour
example : ∀ N : ℚ, ∃ tour, Valid tour ∧ N < time tour := unbounded
example : ∀ tour, Legal tour → Strict start tour → time tour ≤ 24329 := strict_bound
example : Valid record9329 ∧ Strict start record9329 ∧ time record9329 = 9329 := record9329_valid

end KM5
