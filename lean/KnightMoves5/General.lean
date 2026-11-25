import KnightMoves5.Rules

/-!
# The rules on an `n × n` board

The same rules as `Rules.lean`, for any board size, start square and target. `toGen_valid` shows that
the 8 × 8 rules used everywhere else are exactly the instance `n = 8`, start a1, target h8.
`Example4.lean` checks the puzzle's own 4 × 4 worked example against these rules.
-/

namespace KM5.Gen

abbrev Sq (n : ℕ) := Fin n × Fin n
abbrev Alt (n : ℕ) := Sq n → ℚ

variable {n : ℕ}

def opp (p : Sq n) : Sq n := (Fin.rev p.1, Fin.rev p.2)

def InGroup (A : Alt n) (P x : Sq n) : Prop := A x = A P ∧ x ≠ opp P

instance (A : Alt n) (P : Sq n) : DecidablePred (InGroup A P) :=
  fun _ => inferInstanceAs (Decidable (_ ∧ _))

def group (A : Alt n) (P : Sq n) : Finset (Sq n) := Finset.univ.filter (InGroup A P)

def sinkAmt (A : Alt n) (P : Sq n) (t : ℚ) : ℚ := t / (group A P).card

def Rises (A : Alt n) (P x : Sq n) : Prop := x = opp P ∧ A x ≠ A P

instance (A : Alt n) (P : Sq n) : DecidablePred (Rises A P) :=
  fun _ => inferInstanceAs (Decidable (_ ∧ _))

def sit (A : Alt n) (P : Sq n) (t : ℚ) : Alt n := fun x =>
  if InGroup A P x then A x - sinkAmt A P t
  else if Rises A P x then A x + sinkAmt A P t
  else A x

def JumpOK (A : Alt n) (P Q : Sq n) : Prop :=
  let dx := Nat.dist P.1 Q.1
  let dy := Nat.dist P.2 Q.2
  let dz := A Q - A P
  (dx = 0 ∧ dy = 1 ∧ AbsEq dz 2) ∨ (dx = 0 ∧ dy = 2 ∧ AbsEq dz 1) ∨
  (dx = 1 ∧ dy = 0 ∧ AbsEq dz 2) ∨ (dx = 1 ∧ dy = 2 ∧ dz = 0) ∨
  (dx = 2 ∧ dy = 0 ∧ AbsEq dz 1) ∨ (dx = 2 ∧ dy = 1 ∧ dz = 0)

instance (A : Alt n) (P Q : Sq n) : Decidable (JumpOK A P Q) := by
  unfold JumpOK; infer_instance

/-- A move: (wait, target). -/
abbrev Move (n : ℕ) := ℚ × Sq n

/-- Every wait is non-negative and every jump legal, from altitudes `A` with the knight on `P`. -/
def MovesOK : Alt n → Sq n → List (Move n) → Prop
  | _, _, [] => True
  | A, P, m :: ms => 0 ≤ m.1 ∧ JumpOK (sit A P m.1) P m.2 ∧ MovesOK (sit A P m.1) m.2 ms

instance decMovesOK : ∀ (A : Alt n) (P : Sq n) (l : List (Move n)), Decidable (MovesOK A P l)
  | _, _, [] => isTrue trivial
  | A, P, m :: ms =>
    have := decMovesOK (sit A P m.1) m.2 ms
    inferInstanceAs (Decidable (_ ∧ _ ∧ _))

def time (l : List (Move n)) : ℚ := (l.map Prod.fst).sum

/-- Legal tour from `s0` to `tgt` on the board `init` (apart from necessity). -/
def Legal (init : Alt n) (s0 tgt : Sq n) (l : List (Move n)) : Prop :=
  MovesOK init s0 l ∧ (l.map Prod.snd).getLast? = some tgt ∧
  (s0 :: l.map Prod.snd).count tgt = 1 ∧ ∀ x, x ≠ tgt → (s0 :: l.map Prod.snd).count x ≤ 3

def setWait (l : List (Move n)) (i : ℕ) (t : ℚ) : List (Move n) := l.modify i fun m => (t, m.2)

def Necessary (init : Alt n) (s0 : Sq n) (l : List (Move n)) (i : ℕ) : Prop :=
  ∃ t' : ℚ, 0 ≤ t' ∧ ¬ MovesOK init s0 (setWait l i t')

/-- Every rule of the puzzle, including necessity as worded. -/
def Valid (init : Alt n) (s0 tgt : Sq n) (l : List (Move n)) : Prop :=
  Legal init s0 tgt l ∧ ∀ i (hi : i < l.length), 0 < (l[i]).1 → Necessary init s0 l i

end KM5.Gen

namespace KM5

/-- A move of the 8 × 8 puzzle as a move of the general rules. -/
def toGen (m : Move) : Gen.Move 8 := (m.wait, m.dest)

lemma sit_eq_gen : sit = Gen.sit (n := 8) := rfl
lemma jumpOK_eq_gen : JumpOK = Gen.JumpOK (n := 8) := rfl

lemma movesOK_iff_gen : ∀ (s : St) (l : List Move),
    MovesOK s l ↔ Gen.MovesOK s.alt s.pos (l.map toGen)
  | _, [] => Iff.rfl
  | s, m :: ms => by
    simp only [MovesOK, Gen.MovesOK, List.map_cons, toGen, St.next, sit_eq_gen, jumpOK_eq_gen]
    rw [movesOK_iff_gen ⟨Gen.sit s.alt s.pos m.wait, m.dest⟩ ms]

lemma movesOK_start_iff (l : List Move) : MovesOK ⟨init, a1⟩ l ↔ Gen.MovesOK init a1 (l.map toGen) :=
  movesOK_iff_gen ⟨init, a1⟩ l

lemma map_setWait (l : List Move) (i : ℕ) (t : ℚ) :
    (setWait l i t).map toGen = Gen.setWait (l.map toGen) i t := by
  induction l generalizing i with
  | nil => simp [setWait, Gen.setWait]
  | cons a l ih =>
    cases i with
    | zero => simp [setWait, Gen.setWait, toGen]
    | succ i => simpa [setWait, Gen.setWait, List.modify_succ_cons] using ih i

/-- The 8 × 8 rules are the general rules with `n = 8`, start a1 and target h8. -/
theorem toGen_valid (tour : List Move) : Valid tour ↔ Gen.Valid init a1 h8 (tour.map toGen) := by
  have hdest : (tour.map toGen).map Prod.snd = tour.map Move.dest := by simp [toGen]
  have hwait : ∀ i (hi : i < tour.length), ((tour.map toGen)[i]'(by simpa using hi)).1 = (tour[i]).wait := by
    intro i hi; simp [toGen]
  unfold Valid Gen.Valid Legal Gen.Legal AllNecessary Necessary Gen.Necessary visits start
  rw [movesOK_start_iff, hdest]
  constructor
  · rintro ⟨hL, hN⟩
    refine ⟨hL, fun i hi hpos => ?_⟩
    have hi' : i < tour.length := by simpa using hi
    obtain ⟨t', h0, h⟩ := hN i hi' (by rw [← hwait i hi']; exact hpos)
    exact ⟨t', h0, by rw [← map_setWait, ← movesOK_start_iff]; exact h⟩
  · rintro ⟨hL, hN⟩
    refine ⟨hL, fun i hi hpos => ?_⟩
    obtain ⟨t', h0, h⟩ := hN i (by simpa using hi) (by rw [hwait i hi]; exact hpos)
    exact ⟨t', h0, by rw [movesOK_start_iff, map_setWait]; exact h⟩

end KM5
