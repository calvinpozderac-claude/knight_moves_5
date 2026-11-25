import KnightMoves5.Unbounded
import KnightMoves5.StrictBound
import KnightMoves5.Pinned

/-!
# Main results

* `KM5.no_longest_tour`: no tour that obeys every rule is a longest one.
* `KM5.unbounded`: under the puzzle's rules as worded (every wait `t > 0` must have some `t'` that
  makes a move of the tour illegal), there are legal tours longer than any given number of minutes.
* `KM5.longTour_100288`: an explicit 100,288-minute tour that obeys every rule.
* `KM5.every_length_above_574`: every length above 574 minutes is achieved exactly.
* `KM5.strict_bound`: a legal tour in which every wait is pinned by its own jump lasts at most
  24,329 minutes.
* `KM5.record9329_valid`: the 9,329-minute record obeys every rule and is strict.
* `KM5.strict_allNecessary`: strict waits are necessary in the puzzle's sense.
* `KM5.strict_iff_strictSem`: `Strict` is exactly "every positive wait is pinned by its own jump"
  (every small change of the wait breaks that move's jump).
-/

namespace KM5

example : ¬ ∃ best, Valid best ∧ ∀ tour, Valid tour → time tour ≤ time best := no_longest_tour
example : ∀ N : ℚ, ∃ tour, Valid tour ∧ N < time tour := unbounded
example : ∀ tour, Legal tour → Strict start tour → time tour ≤ 24329 := strict_bound
example : Valid record9329 ∧ Strict start record9329 ∧ time record9329 = 9329 := record9329_valid

end KM5
