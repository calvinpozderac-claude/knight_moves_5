import KnightMoves5.Lemmas

/-!
# `Strict` is exactly "pinned by its own jump"

A wait is *pinned by its own jump* when every small change of the wait makes that move's jump
illegal. For a move whose jump is legal, this holds exactly when the target is outside the sinking
group (`pinned_iff`):

* target outside the group: the jump's altitude difference moves with the wait, so every change
  smaller than `n / 2` minutes breaks it (`pinned_local`);
* target inside the group: the target sinks with the knight, so the jump is legal for every wait
  (`jump_free_of_inGroup`).

So `Strict` (a structural condition) is the semantic notion used by `subagents/strict_check.py`.
-/

namespace KM5

/-- If the target sinks with the knight, the jump's legality does not depend on the wait. -/
lemma jump_free_of_inGroup {A : Alt} {P Q : Sq} (hQ : InGroup A P Q) (t t' : ℚ) :
    JumpOK (sit A P t) P Q ↔ JumpOK (sit A P t') P Q := by
  have h : ∀ s : ℚ, sit A P s Q - sit A P s P = 0 := by
    intro s; rw [sit_of_inGroup s hQ, sit_self]; ring
  unfold JumpOK; rw [h t, h t']

/-- If the target is outside the group, every change of the wait by less than `n / 2` minutes
breaks the jump. -/
lemma pinned_local {A : Alt} {P Q : Sq} {t t' : ℚ} (hQ : ¬ InGroup A P Q)
    (hj : JumpOK (sit A P t) P Q) (hne : t' ≠ t) (hclose : |t' - t| < (group A P).card / 2) :
    ¬ JumpOK (sit A P t') P Q := by
  have hQo := ne_opp_of_jumpOK hj
  have hn : (0 : ℚ) < (group A P).card := by exact_mod_cast card_group_pos A P
  set e := (t' - t) / (group A P).card with he
  have key : sit A P t' Q - sit A P t' P = (sit A P t Q - sit A P t P) + e := by
    rw [sit_of_not_inGroup _ hQ hQo, sit_of_not_inGroup _ hQ hQo, sit_self, sit_self, he]
    unfold sinkAmt; field_simp; ring
  have he0 : e ≠ 0 := by
    rw [he]; exact div_ne_zero (sub_ne_zero.mpr hne) hn.ne'
  have he1 : |e| < 1 / 2 := by
    rw [he, abs_div, abs_of_pos hn, div_lt_iff₀ hn]; linarith
  have hlt := abs_lt.mp he1
  intro h'
  unfold JumpOK at hj h'
  rw [key] at h'
  generalize sit A P t Q - sit A P t P = z at hj h'
  generalize Nat.dist P.1 Q.1 = dx at hj h'
  generalize Nat.dist P.2 Q.2 = dy at hj h'
  unfold AbsEq at hj h'
  rcases hj with ⟨rfl, rfl, h | h⟩ | ⟨rfl, rfl, h | h⟩ | ⟨rfl, rfl, h | h⟩ | ⟨rfl, rfl, h⟩ |
      ⟨rfl, rfl, h | h⟩ | ⟨rfl, rfl, h⟩ <;> subst h <;> norm_num at h' <;>
    first
    | exact he0 (by linarith)
    | (rcases h' with h' | h' <;> first | exact he0 (by linarith) | linarith)

/-- A wait is pinned by its own jump: some window around it in which every other wait breaks
the jump. -/
def PinnedBy (A : Alt) (P Q : Sq) (t : ℚ) : Prop :=
  ∃ ε > 0, ∀ t', t' ≠ t → |t' - t| < ε → ¬ JumpOK (sit A P t') P Q

/-- For a move with a legal jump, "pinned by its own jump" is exactly "target outside the group". -/
theorem pinned_iff {A : Alt} {P Q : Sq} {t : ℚ} (hj : JumpOK (sit A P t) P Q) :
    PinnedBy A P Q t ↔ ¬ InGroup A P Q := by
  constructor
  · rintro ⟨ε, hε, h⟩ hQ
    have := h (t + ε / 2) (by linarith) (by rw [add_sub_cancel_left, abs_of_pos (by linarith)]; linarith)
    exact this ((jump_free_of_inGroup hQ t _).1 hj)
  · intro hQ
    have hn : (0 : ℚ) < (group A P).card := by exact_mod_cast card_group_pos A P
    exact ⟨(group A P).card / 2, by positivity, fun t' hne hc => pinned_local hQ hj hne hc⟩

/-- The semantic version of `Strict`: every positive wait is pinned by its own jump. -/
def StrictSem : St → List Move → Prop
  | _, [] => True
  | s, m :: ms => (0 < m.wait → PinnedBy s.alt s.pos m.dest m.wait) ∧ StrictSem (s.next m) ms

/-- For tours with legal moves, `Strict` is exactly "every positive wait is pinned by its own jump". -/
theorem strict_iff_strictSem : ∀ (s : St) (l : List Move), MovesOK s l → (Strict s l ↔ StrictSem s l)
  | _, [], _ => Iff.rfl
  | s, m :: ms, h => by
    simp only [Strict, StrictSem]
    rw [strict_iff_strictSem _ ms h.2.2, pinned_iff h.2.1]

end KM5
