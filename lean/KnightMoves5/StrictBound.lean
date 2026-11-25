import KnightMoves5.Lemmas

/-!
# Theorem 2: every strict tour lasts at most 24,329 minutes

Notation along a tour: `T` minutes so far, `R` total rise of opposite points, `D` total sink of the
knight's square, `Z` total altitude change over the jumps, `mf` the running floor (lowest altitude
seen so far), `B` its total drop, `c` the number of moves without a floor-lowering sit, `M` moves.

The proof carries the invariant `Inv` along the tour (`inv_step`, `inv_run`):

* `∑ altitudes = 421 - T + R` (each sit of `t` minutes removes `t` altitude units and the
  opposite point gives back at most its own rise);
* the knight's altitude is `Z - D`;
* every altitude is `≥ mf ≥ -B`: a strict sit sinks at most `2 + (knight - floor)`, so it lowers
  the floor by at most `min d 2` (Lemma 1/2 of `proofs/UPPER_BOUND.md`);
* `R ≤ D`, `Z ≤ 2M`, `B ≤ 2 (M - c)`;
* the knight's height above the floor is at most `2 (c + 1)`;
* `h8` is still at altitude `≥ 17`, or `c ≥ 8` (Lemma 3: the knight must climb 17 to meet `h8`).

At the end `T = 421 + R - ∑ altitudes ≤ 421 + 64 B + R`, `R ≤ D ≤ 2M + B`, `B ≤ 2 (M - 8)` and
`M ≤ 189`, so `T ≤ 421 + 65 B + 2 M ≤ 24,329`.
-/

namespace KM5

open Finset

/-- Bookkeeping along a tour. -/
structure Acc where
  T : ℚ
  R : ℚ
  D : ℚ
  Z : ℚ
  B : ℚ
  mf : ℚ
  c : ℕ
  M : ℕ

def Inv (s : St) (a : Acc) : Prop :=
  (∑ x, s.alt x) = 421 - a.T + a.R ∧
  s.alt s.pos = a.Z - a.D ∧
  (∀ x, a.mf ≤ s.alt x) ∧ -a.B ≤ a.mf ∧ a.mf ≤ 0 ∧
  a.R ≤ a.D ∧ a.Z ≤ 2 * a.M ∧ a.B ≤ 2 * ((a.M : ℚ) - a.c) ∧ a.c ≤ a.M ∧
  s.alt s.pos - a.mf ≤ 2 * ((a.c : ℚ) + 1) ∧
  (17 ≤ s.alt h8 ∨ 8 ≤ a.c)

lemma sit_eq (A : Alt) (P x : Sq) (t : ℚ) :
    sit A P t x = A x - (if InGroup A P x then sinkAmt A P t else 0) +
      (if Rises A P x then sinkAmt A P t else 0) := by
  unfold sit
  by_cases hg : InGroup A P x
  · have : ¬ Rises A P x := fun h => h.2 hg.1
    simp [hg, this]
  · by_cases hr : Rises A P x <;> simp [hg, hr]

lemma le_sit_of_not_inGroup {A : Alt} {P x : Sq} {t : ℚ} (ht : 0 ≤ t) (hx : ¬ InGroup A P x) :
    A x ≤ sit A P t x := by
  rw [sit_eq]; have := sinkAmt_nonneg (A := A) (P := P) ht
  split_ifs <;> simp_all

lemma sum_sit (A : Alt) (P : Sq) (t : ℚ) :
    ∑ x, sit A P t x = ∑ x, A x - t + ∑ x, (if Rises A P x then sinkAmt A P t else 0) := by
  simp_rw [sit_eq, Finset.sum_add_distrib, Finset.sum_sub_distrib]
  rw [Finset.sum_ite, Finset.sum_const_zero, add_zero, Finset.sum_const, nsmul_eq_mul]
  rw [show (Finset.univ.filter (InGroup A P)) = group A P from rfl, group_card_mul_sinkAmt]

lemma rise_le (A : Alt) (P : Sq) {t : ℚ} (ht : 0 ≤ t) :
    ∑ x, (if Rises A P x then sinkAmt A P t else 0) ≤ sinkAmt A P t := by
  calc ∑ x, (if Rises A P x then sinkAmt A P t else 0)
      ≤ ∑ x, (if x = opp P then sinkAmt A P t else 0) := by
        apply Finset.sum_le_sum; intro x _
        have := sinkAmt_nonneg (A := A) (P := P) ht
        by_cases h : x = opp P
        · subst h; simp only [Rises, true_and, ite_true]; split_ifs <;> simp [this]
        · simp [Rises, h]
    _ = sinkAmt A P t := by simp

lemma rise_nonneg (A : Alt) (P : Sq) {t : ℚ} (ht : 0 ≤ t) :
    0 ≤ ∑ x, (if Rises A P x then sinkAmt A P t else 0) := by
  apply Finset.sum_nonneg; intro x _
  have := sinkAmt_nonneg (A := A) (P := P) ht
  split_ifs <;> simp [this]

lemma absEq_le {z c : ℚ} (h : AbsEq z c) (hc : 0 ≤ c) : -c ≤ z ∧ z ≤ c := by
  rcases h with rfl | rfl <;> constructor <;> linarith

/-- A legal jump changes the altitude by at most 2 either way. -/
lemma jump_dz {A : Alt} {P Q : Sq} (h : JumpOK A P Q) : -2 ≤ A Q - A P ∧ A Q - A P ≤ 2 := by
  unfold JumpOK at h
  rcases h with ⟨-, -, h⟩ | ⟨-, -, h⟩ | ⟨-, -, h⟩ | ⟨-, -, h⟩ | ⟨-, -, h⟩ | ⟨-, -, h⟩
  all_goals first
    | (have := absEq_le h (by norm_num); constructor <;> linarith)
    | (constructor <;> linarith)

lemma two_mul_succ_ge {c : ℕ} (h : (17 : ℚ) ≤ 2 * ((c : ℚ) + 1)) : 8 ≤ c := by
  have : (15 : ℚ) / 2 ≤ c := by linarith
  have : (7 : ℚ) < c := by linarith
  exact_mod_cast this

/-- One move preserves the invariant. -/
lemma inv_step {s : St} {a : Acc} {m : Move} (hI : Inv s a) (h0 : 0 ≤ m.wait)
    (hj : JumpOK (sit s.alt s.pos m.wait) s.pos m.dest)
    (hst : 0 < m.wait → ¬ InGroup s.alt s.pos m.dest) :
    ∃ a' : Acc, Inv (s.next m) a' ∧ a'.T = a.T + m.wait ∧ a'.M = a.M + 1 := by
  obtain ⟨A, P⟩ := s
  obtain ⟨t, Q⟩ := m
  simp only at h0 hj hst
  obtain ⟨hsum, hk, hmf, hB, hmf0, hRD, hZ, hBc, hcM, hh, hH8⟩ := hI
  simp only at hsum hk hmf hh hH8
  set d := sinkAmt A P t with hd
  have hd0 : 0 ≤ d := sinkAmt_nonneg h0
  set rd := ∑ x, (if Rises A P x then d else 0) with hrd
  have hrd_le : rd ≤ d := rise_le A P h0
  have hrd0 : 0 ≤ rd := rise_nonneg A P h0
  have hP' : sit A P t P = A P - d := sit_self A P t
  have hdz := jump_dz hj
  set Δ := sit A P t Q - sit A P t P with hΔ
  have hQ' : sit A P t Q = A P - d + Δ := by rw [hΔ, hP']; ring
  -- Lemma 1: a strict sit sinks at most 2 + (knight - floor).
  have hL1 : d ≤ 2 + A P - a.mf := by
    rcases eq_or_lt_of_le h0 with h | h
    · have : d = 0 := by rw [hd, ← h]; simp
      have := hmf P; linarith
    · have hQo := ne_opp_of_jumpOK hj
      have hQ := sit_of_not_inGroup t (hst h) hQo
      have := hmf Q; linarith
  have hsum' : ∑ x, sit A P t x = 421 - (a.T + t) + (a.R + rd) := by
    rw [sum_sit, hsum]; ring
  have hlow : ∀ x, ¬ InGroup A P x → A x ≤ sit A P t x := fun x hx => le_sit_of_not_inGroup h0 hx
  have hgrp : ∀ x, InGroup A P x → sit A P t x = A P - d := fun x hx => sit_of_inGroup t hx
  -- h8 still high, or c ≥ 8
  have h8step : ∀ c' : ℕ, a.c ≤ c' → (17 ≤ sit A P t h8 ∨ 8 ≤ c') := by
    intro c' hc'
    rcases hH8 with hH8 | hH8
    · by_cases hg : InGroup A P h8
      · right
        have : (17 : ℚ) ≤ 2 * ((a.c : ℚ) + 1) := by have := hg.1; linarith
        have := two_mul_succ_ge this; omega
      · left; exact le_trans hH8 (hlow h8 hg)
    · right; omega
  by_cases hfl : A P - d < a.mf
  · -- the sit lowers the floor
    refine ⟨⟨a.T + t, a.R + rd, a.D + d, a.Z + Δ, a.B + min d 2, A P - d, a.c, a.M + 1⟩,
      ?_, rfl, rfl⟩
    refine ⟨hsum', ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · show sit A P t Q = _; rw [hQ']; linarith
    · intro x
      by_cases hg : InGroup A P x
      · show A P - d ≤ sit A P t x; rw [hgrp x hg]
      · show A P - d ≤ sit A P t x; have := hlow x hg; have := hmf x; linarith
    · show -(a.B + min d 2) ≤ A P - d
      rcases min_choice d 2 with h | h <;> rw [h]
      · have := hmf P; linarith
      · linarith
    · show A P - d ≤ 0; linarith
    · show a.R + rd ≤ a.D + d; linarith
    · show a.Z + Δ ≤ 2 * ((a.M + 1 : ℕ) : ℚ); push_cast; linarith
    · show a.B + min d 2 ≤ 2 * (((a.M + 1 : ℕ) : ℚ) - a.c); push_cast
      have := min_le_right d 2; linarith
    · show a.c ≤ a.M + 1; omega
    · show sit A P t Q - (A P - d) ≤ 2 * ((a.c : ℚ) + 1)
      rw [hQ']; have : (0 : ℚ) ≤ a.c := by positivity
      linarith
    · exact h8step a.c le_rfl
  · -- the floor stays
    push Not at hfl
    refine ⟨⟨a.T + t, a.R + rd, a.D + d, a.Z + Δ, a.B, a.mf, a.c + 1, a.M + 1⟩, ?_, rfl, rfl⟩
    refine ⟨hsum', ?_, ?_, hB, hmf0, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · show sit A P t Q = _; rw [hQ']; linarith
    · intro x
      by_cases hg : InGroup A P x
      · show a.mf ≤ sit A P t x; rw [hgrp x hg]; exact hfl
      · show a.mf ≤ sit A P t x; exact le_trans (hmf x) (hlow x hg)
    · show a.R + rd ≤ a.D + d; linarith
    · show a.Z + Δ ≤ 2 * ((a.M + 1 : ℕ) : ℚ); push_cast; linarith
    · show a.B ≤ 2 * (((a.M + 1 : ℕ) : ℚ) - ((a.c + 1 : ℕ) : ℚ)); push_cast; linarith
    · show a.c + 1 ≤ a.M + 1; omega
    · show sit A P t Q - a.mf ≤ 2 * (((a.c + 1 : ℕ) : ℚ) + 1)
      rw [hQ']; push_cast; linarith
    · exact h8step (a.c + 1) (Nat.le_succ _)

lemma time_cons (m : Move) (l : List Move) : time (m :: l) = m.wait + time l := by
  simp [time]

lemma inv_run : ∀ (l : List Move) (s : St) (a : Acc), Inv s a → MovesOK s l → Strict s l →
    ∃ a', Inv (run s l) a' ∧ a'.T = a.T + time l ∧ a'.M = a.M + l.length
  | [], s, a, hI, _, _ => ⟨a, hI, by simp [time], by simp⟩
  | m :: l, s, a, hI, hm, hs => by
    obtain ⟨a₁, hI₁, hT₁, hM₁⟩ := inv_step hI hm.1 hm.2.1 hs.1
    obtain ⟨a₂, hI₂, hT₂, hM₂⟩ := inv_run l _ a₁ hI₁ hm.2.2 hs.2
    refine ⟨a₂, hI₂, ?_, ?_⟩
    · rw [hT₂, hT₁, time_cons]; ring
    · rw [hM₂, hM₁]; simp; ring

lemma init_sum : ∑ x, init x = 421 := by decide +kernel

lemma init_nonneg : ∀ x, 0 ≤ init x := by decide +kernel

lemma inv_start : Inv start ⟨0, 0, 0, 0, 0, 0, 0, 0⟩ := by
  have ha1 : init a1 = 0 := by decide +kernel
  have hh8 : (17 : ℚ) ≤ init h8 := by decide +kernel
  refine ⟨?_, ?_, init_nonneg, ?_, ?_, ?_, ?_, ?_, ?_, ?_, Or.inl hh8⟩ <;>
    simp [start, init_sum, ha1]

/-- The knight ends where its last jump lands. -/
lemma run_pos : ∀ (s : St) (l : List Move) (q : Sq), (l.map Move.dest).getLast? = some q →
    (run s l).pos = q
  | _, [], _, h => by simp at h
  | s, [m], q, h => by simp at h; simp [run, St.next, h]
  | s, m :: m' :: l, q, h => by
    rw [run_cons]; apply run_pos; simpa using h

/-- Every point other than h8 is visited at most 3 times and h8 once, so a tour has at most
189 moves. -/
lemma length_le (tour : List Move) (hL : Legal tour) : tour.length ≤ 189 := by
  obtain ⟨-, -, h8c, h3⟩ := hL
  have hsum : ∑ x : Sq, (visits tour).count x = tour.length + 1 := by
    have := Multiset.sum_count_eq_card (s := (Finset.univ : Finset Sq))
      (m := ((visits tour : List Sq) : Multiset Sq)) (fun a _ => Finset.mem_univ a)
    simpa [visits] using this
  have hsplit : ∑ x : Sq, (visits tour).count x =
      (visits tour).count h8 + ∑ x ∈ Finset.univ.erase h8, (visits tour).count x :=
    (Finset.add_sum_erase Finset.univ (fun x => (visits tour).count x) (Finset.mem_univ h8)).symm
  have hle : ∑ x ∈ Finset.univ.erase h8, (visits tour).count x ≤ ∑ x ∈ Finset.univ.erase h8, 3 :=
    Finset.sum_le_sum fun x hx => h3 x (Finset.ne_of_mem_erase hx)
  have hcard : (Finset.univ.erase h8).card = 63 := by decide
  rw [Finset.sum_const, hcard, smul_eq_mul] at hle
  omega

/-- **Theorem 2.** A strict tour that obeys the rules lasts at most 24,329 minutes. -/
theorem strict_bound (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 24329 := by
  obtain ⟨a, hI, hT, hM⟩ := inv_run tour start _ inv_start hL.1 hS
  simp only [zero_add] at hT hM
  obtain ⟨hsum, hk, hmf, hB, hmf0, hRD, hZ, hBc, hcM, hh, hH8⟩ := hI
  have hpos : (run start tour).pos = h8 := run_pos _ _ _ hL.2.1
  rw [hpos] at hk hh
  have hM189 : (a.M : ℚ) ≤ 189 := by rw [hM]; exact_mod_cast length_le tour hL
  -- every altitude is at least the floor
  have hfloor : 64 * a.mf ≤ ∑ x, (run start tour).alt x := by
    have := Finset.card_nsmul_le_sum Finset.univ (fun x => (run start tour).alt x) a.mf
      (fun x _ => hmf x)
    simpa using this
  have hc8 : (8 : ℚ) ≤ a.c := by
    rcases hH8 with h | h
    · have := hmf h8
      exact_mod_cast two_mul_succ_ge (by linarith)
    · exact_mod_cast h
  have hk' := hmf h8
  rw [← hT]
  nlinarith

end KM5

namespace KM5

/-- **Theorem 2, by length.** A strict tour of `M` moves lasts at most `132·M − 619` minutes
(at most 24,329 since `M ≤ 189`). -/
theorem strict_bound_length (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 132 * tour.length - 619 := by
  obtain ⟨a, hI, hT, hM⟩ := inv_run tour start _ inv_start hL.1 hS
  simp only [zero_add] at hT hM
  obtain ⟨hsum, hk, hmf, hB, hmf0, hRD, hZ, hBc, hcM, hh, hH8⟩ := hI
  have hpos : (run start tour).pos = h8 := run_pos _ _ _ hL.2.1
  rw [hpos] at hk hh
  have hMq : (a.M : ℚ) = tour.length := by rw [hM]
  have hfloor : 64 * a.mf ≤ ∑ x, (run start tour).alt x := by
    have := Finset.card_nsmul_le_sum Finset.univ (fun x => (run start tour).alt x) a.mf
      (fun x _ => hmf x)
    simpa using this
  have hc8 : (8 : ℚ) ≤ a.c := by
    rcases hH8 with h | h
    · have := hmf h8
      exact_mod_cast two_mul_succ_ge (by linarith)
    · exact_mod_cast h
  have hk' := hmf h8
  rw [← hT, ← hMq]
  nlinarith

end KM5
