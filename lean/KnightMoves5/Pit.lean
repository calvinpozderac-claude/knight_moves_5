import KnightMoves5.StrictBound

/-!
# Pits: positions from which h8 can never be reached

A jump climbs at most 2. A point only sinks at a sit at its own altitude, and the only point that
rises is the sitter's opposite. So if the knight stands at most `c` high, no point has an altitude
in `(c, c + 2]`, h8 is above `c + 2`, and the opposite of every point at most `c` high is above
`c + 2`, then the points at most `c` high stay at most `c` high, the others stay above `c + 2`, and
the knight can never leave the low ones. This is the pit test of `tools/prove_bound.cpp`
(`alive_gap`), which takes for `c` the top of the run of altitudes at most 2 apart that starts at
the knight's altitude. The rules of strictness are not needed.
-/

namespace KM5

/-- A pit with ceiling `c`. -/
def Pit (c : ℚ) (s : St) : Prop :=
  s.alt s.pos ≤ c ∧ (∀ x, s.alt x ≤ c ∨ c + 2 < s.alt x) ∧
  (∀ x, s.alt x ≤ c → c + 2 < s.alt (opp x)) ∧ c + 2 < s.alt h8

lemma pit_step {c : ℚ} {s : St} {m : Move} (hp : Pit c s) (h0 : 0 ≤ m.wait)
    (hj : JumpOK (sit s.alt s.pos m.wait) s.pos m.dest) : Pit c (s.next m) ∧ m.dest ≠ h8 := by
  obtain ⟨hk, hgap, hopp, h8h⟩ := hp
  have hd := sinkAmt_nonneg (A := s.alt) (P := s.pos) h0
  -- the low points stay low and the high points stay high
  have hlo : ∀ x, s.alt x ≤ c → sit s.alt s.pos m.wait x ≤ c := by
    intro x hx
    by_cases hg : InGroup s.alt s.pos x
    · rw [sit_of_inGroup _ hg]; linarith
    · have hxo : x ≠ opp s.pos := by
        rintro rfl; have := hopp _ hk; linarith
      rw [sit_of_not_inGroup _ hg hxo]; exact hx
  have hhi : ∀ x, c + 2 < s.alt x → c + 2 < sit s.alt s.pos m.wait x := by
    intro x hx
    have hg : ¬ InGroup s.alt s.pos x := fun h => by have := h.1; linarith
    exact lt_of_lt_of_le hx (le_sit_of_not_inGroup h0 hg)
  have hgap' : ∀ x, sit s.alt s.pos m.wait x ≤ c ∨ c + 2 < sit s.alt s.pos m.wait x := by
    intro x; rcases hgap x with h | h
    · exact Or.inl (hlo x h)
    · exact Or.inr (hhi x h)
  have hP : sit s.alt s.pos m.wait s.pos ≤ c := hlo _ hk
  have hdz := jump_dz hj
  have hQ : sit s.alt s.pos m.wait m.dest ≤ c := by
    rcases hgap' m.dest with h | h
    · exact h
    · linarith [hdz.2]
  have h8' := hhi _ h8h
  refine ⟨⟨hQ, hgap', ?_, h8'⟩, ?_⟩
  · intro x hx
    rcases hgap x with h | h
    · exact hhi _ (hopp x h)
    · have := hhi x h; simp only [St.next] at hx; linarith
  · rintro hq; rw [hq] at hQ; linarith

/-- From a pit no sequence of legal moves reaches h8. -/
lemma pit_no_h8 : ∀ (l : List Move) (c : ℚ) (s : St), Pit c s → MovesOK s l → h8 ∉ l.map Move.dest
  | [], _, _, _, _ => by simp
  | m :: l, c, s, hp, hm => by
    obtain ⟨h1, h2⟩ := pit_step hp hm.1 hm.2.1
    have ih := pit_no_h8 l c (s.next m) h1 hm.2.2
    simp only [List.map_cons, List.mem_cons, not_or]
    exact ⟨fun h => h2 h.symm, ih⟩

/-- **Pit lemma.** No legal tour continues past a prefix that ends in a pit. -/
theorem pit_dead (pre suf : List Move) (c : ℚ) (hp : Pit c (run start pre)) (hL : Legal (pre ++ suf)) :
    suf = [] := by
  have hm := hL.1; rw [movesOK_append] at hm
  have hn := pit_no_h8 suf c _ hp hm.2
  by_contra hne
  have hlast := hL.2.1
  rw [List.map_append, List.getLast?_append_of_ne_nil _ (by simpa using hne)] at hlast
  exact hn (List.mem_of_getLast? hlast)

end KM5
