import KnightMoves5.General

/-!
# The puzzle's own worked example (4 × 4)

The example board from the puzzle statement, and its 19-move, 28-minute example tour from a1 to d4.
Under the general rules (`General.lean`), the tour is legal and every wait is necessary as worded.
Three of its waits (moves 5–7) leave their own jump legal whatever their value and are pinned only
by later moves, so the example itself uses the literal reading of the necessity rule.
-/

namespace KM5.Example4

/-- The example board, rank 1 first: a1 = 7 (start), d4 = 14 (target). -/
def table : Fin 4 → Fin 4 → ℚ := ![
  ![7, 6, 5, 0],
  ![10, 4, 3, 1],
  ![8, 6, 9, 9],
  ![11, 10, 11, 14]]

def init : Gen.Alt 4 := fun p => table p.2 p.1

/-- (1, b3), (0, a3), (0, a2), (0, b4), (5, a2), (3, c1), (2, a2), (0, c1), (15, d1), (1, d2),
(0, c2), (1, b2), (0, b1), (0, b3), (0, c3), (0, c4), (0, a4), (0, b4), (0, d4) -/
def tour : List (Gen.Move 4) := [
  (1, (1, 2)), (0, (0, 2)), (0, (0, 1)), (0, (1, 3)), (5, (0, 1)), (3, (2, 0)), (2, (0, 1)),
  (0, (2, 0)), (15, (3, 0)), (1, (3, 1)), (0, (2, 1)), (1, (1, 1)), (0, (1, 0)), (0, (1, 2)),
  (0, (2, 2)), (0, (2, 3)), (0, (0, 3)), (0, (1, 3)), (0, (3, 3))]

theorem time_tour : Gen.time tour = 28 := by decide +kernel

/-- The puzzle's example tour obeys every rule. -/
theorem valid : Gen.Valid init (0, 0) (3, 3) tour := by
  refine ⟨⟨by decide +kernel, by decide +kernel, by decide +kernel, by decide +kernel⟩, ?_⟩
  intro i hi hpos
  have hlen : tour.length = 19 := rfl
  rw [hlen] at hi
  interval_cases i
  all_goals simp [tour] at hpos
  · exact ⟨3 / 2, by norm_num, by decide +kernel⟩
  · exact ⟨6, by norm_num, by decide +kernel⟩
  · exact ⟨9 / 2, by norm_num, by decide +kernel⟩
  · exact ⟨3, by norm_num, by decide +kernel⟩
  · exact ⟨16, by norm_num, by decide +kernel⟩
  · exact ⟨3 / 2, by norm_num, by decide +kernel⟩
  · exact ⟨3 / 2, by norm_num, by decide +kernel⟩

/-- Altitudes and knight square after the first `k` moves. -/
def stateAfter (k : ℕ) : Gen.Alt 4 × Gen.Sq 4 :=
  (tour.take k).foldl (fun s m => (Gen.sit s.1 s.2 m.1, m.2)) (init, (0, 0))

/-- Moves 5, 6 and 7 (indices 4–6) are not pinned by their own jump: each target is in the
sinking group, so it sinks with the knight. Their necessity comes from later moves. -/
theorem waits_5_6_7_not_strict : ∀ k ∈ [4, 5, 6],
    Gen.InGroup (stateAfter k).1 (stateAfter k).2 ((tour.getD k (0, (0, 0))).2) := by
  decide +kernel

end KM5.Example4
