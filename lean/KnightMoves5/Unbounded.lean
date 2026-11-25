import KnightMoves5.Record

/-!
# Theorem 1: under the rules as worded, there is no longest tour

`longTour W` is the first 53 moves of the 9,329 record (238 minutes, knight on d5 at altitude 6,
level with h8), followed by

  `(W, b4), (50, d5), (0, f4), (0, g6), (0, g5), (0, g6), (0, h8)`.

* `(W, b4)`: the 26 points at altitude 6 (`L`) sink `W / 26`; d5's opposite e4 rises. d5 → b4 is
  a knight move inside `L`, legal for every `W`.
* `(50, d5)`: b4's opposite g5 is in `L` at the same altitude, so it stays; the other 25 sink 2.
* g6 → g5 is a one-step jump up exactly 2, then back down, then a knight move onto h8.

For every `W > 286` the tour is legal (`longTour_legal`), every wait is necessary in the puzzle's
sense (`longTour_allNecessary`; the `W` wait breaks the tour at `t' = 52`), and it lasts
`288 + W` minutes.
-/

namespace KM5

open Finset

def pre53 : List Move := record9329.take 53

/-- The seven moves after the record's first 53. -/
def sfx (W : ℚ) : List Move :=
  [⟨W, (1, 3)⟩, ⟨50, (3, 4)⟩, ⟨0, (5, 3)⟩, ⟨0, (6, 5)⟩, ⟨0, (6, 4)⟩, ⟨0, (6, 5)⟩, ⟨0, (7, 7)⟩]

def longTour (W : ℚ) : List Move := pre53 ++ sfx W

def s53 : St := run start pre53

/-- The 26 points at altitude 6 after move 53 (h8 among them). -/
def L : Finset Sq := univ.filter fun x => s53.alt x = 6

lemma pre53_moves : MovesOK start pre53 := by decide +kernel
lemma pre53_strict : Strict start pre53 := by decide +kernel
lemma pre53_time : time pre53 = 238 := by decide +kernel
lemma pre53_length : pre53.length = 53 := by decide +kernel
lemma s53_pos : s53.pos = (3, 4) := by decide +kernel
lemma s53_d5 : s53.alt (3, 4) = 6 := by decide +kernel
lemma s53_e4 : s53.alt (4, 3) = 3 := by decide +kernel
lemma s53_ge : ∀ x, -5 ≤ s53.alt x := by decide +kernel
lemma L_card : L.card = 26 := by decide +kernel
lemma e4_notin : (4, 3) ∉ L := by decide +kernel
lemma mem_L : (1, 3) ∈ L ∧ (3, 4) ∈ L ∧ (5, 3) ∈ L ∧ (6, 5) ∈ L ∧ (6, 4) ∈ L ∧ (7, 7) ∈ L := by
  decide +kernel

lemma opp_d5 : opp (3, 4) = (4, 3) := by decide
lemma opp_b4 : opp (1, 3) = (6, 4) := by decide

lemma mem_L_iff {x : Sq} : x ∈ L ↔ s53.alt x = 6 := by simp [L]

/-! ### Move 54: the long wait on d5 -/

def A54 (W : ℚ) : Alt := sit s53.alt (3, 4) W

lemma inGroup54 (x : Sq) : InGroup s53.alt (3, 4) x ↔ x ∈ L := by
  rw [mem_L_iff, InGroup, s53_d5, opp_d5]
  constructor
  · exact fun h => h.1
  · intro h; refine ⟨h, ?_⟩; rintro rfl; rw [s53_e4] at h; norm_num at h

lemma group54 : group s53.alt (3, 4) = L := by
  ext x; simp [group, inGroup54]

lemma A54_L (W : ℚ) {x : Sq} (hx : x ∈ L) : A54 W x = 6 - W / 26 := by
  rw [A54, sit_of_inGroup _ ((inGroup54 x).2 hx), s53_d5, sinkAmt, group54, L_card]; norm_num

lemma A54_e4 (W : ℚ) : A54 W (4, 3) = 3 + W / 26 := by
  have h1 : ¬ InGroup s53.alt (3, 4) (4, 3) := by rw [inGroup54]; exact e4_notin
  have h2 : Rises s53.alt (3, 4) (4, 3) := by
    refine ⟨opp_d5.symm, ?_⟩; rw [s53_e4, s53_d5]; norm_num
  simp [A54, sit, h1, h2, s53_e4, sinkAmt, group54, L_card]

lemma A54_other (W : ℚ) {x : Sq} (hx : x ∉ L) (he : x ≠ (4, 3)) : A54 W x = s53.alt x := by
  rw [A54, sit_of_not_inGroup _ (by rwa [inGroup54]) (by rwa [opp_d5])]

/-! ### Move 55: the 50-minute wait on b4 -/

def A55 (W : ℚ) : Alt := sit (A54 W) (1, 3) 50

lemma inGroup55 {W : ℚ} (hW : 286 < W) (x : Sq) :
    InGroup (A54 W) (1, 3) x ↔ x ∈ L ∧ x ≠ (6, 4) := by
  rw [InGroup, A54_L W mem_L.1, opp_b4]
  by_cases hx : x ∈ L
  · simp [hx, A54_L W hx]
  · simp only [hx, false_and, iff_false, not_and]
    intro h
    by_cases he : x = (4, 3)
    · subst he; rw [A54_e4] at h; linarith
    · rw [A54_other W hx he] at h; have := s53_ge x; linarith

lemma group55 {W : ℚ} (hW : 286 < W) : group (A54 W) (1, 3) = L.erase (6, 4) := by
  ext x; simp [group, inGroup55 hW, and_comm]

lemma A55_L {W : ℚ} (hW : 286 < W) {x : Sq} (hx : x ∈ L) (hg : x ≠ (6, 4)) :
    A55 W x = 6 - W / 26 - 2 := by
  rw [A55, sit_of_inGroup _ ((inGroup55 hW x).2 ⟨hx, hg⟩), A54_L W mem_L.1, sinkAmt, group55 hW,
    Finset.card_erase_of_mem mem_L.2.2.2.2.1, L_card]
  norm_num

lemma A55_g5 {W : ℚ} (hW : 286 < W) : A55 W (6, 4) = 6 - W / 26 := by
  have h1 : ¬ InGroup (A54 W) (1, 3) (6, 4) := by simp [inGroup55 hW]
  have h2 : ¬ Rises (A54 W) (1, 3) (6, 4) := by
    rintro ⟨-, h⟩; exact h (by rw [A54_L W mem_L.2.2.2.2.1, A54_L W mem_L.1])
  simp only [A55, sit, h1, h2, ite_false]; exact A54_L W mem_L.2.2.2.2.1

/-! ### Legality -/

lemma sfx_moves {W : ℚ} (hW : 286 < W) : MovesOK s53 (sfx W) := by
  have hW0 : (0 : ℚ) ≤ W := by linarith
  obtain ⟨hb4, hd5, hf4, hg6, hg5, hh8⟩ := mem_L
  have hL55 : ∀ {x : Sq}, x ∈ L → x ≠ (6, 4) → A55 W x = 6 - W / 26 - 2 := A55_L hW
  simp only [sfx, MovesOK, St.next, s53_pos, sit_zero]
  refine ⟨hW0, ?_, by norm_num, ?_, le_rfl, ?_, le_rfl, ?_, le_rfl, ?_, le_rfl, ?_, le_rfl, ?_, trivial⟩
  · -- d5 → b4: a knight move inside L, legal whatever W is
    show JumpOK (A54 W) (3, 4) (1, 3)
    exact jumpOK_level (by rw [A54_L W hb4, A54_L W hd5]) (by decide)
  · show JumpOK (A55 W) (1, 3) (3, 4)
    exact jumpOK_level (by rw [hL55 hb4 (by decide), hL55 hd5 (by decide)]) (by decide)
  · show JumpOK (A55 W) (3, 4) (5, 3)
    exact jumpOK_level (by rw [hL55 hd5 (by decide), hL55 hf4 (by decide)]) (by decide)
  · show JumpOK (A55 W) (5, 3) (6, 5)
    exact jumpOK_level (by rw [hL55 hf4 (by decide), hL55 hg6 (by decide)]) (by decide)
  · -- g6 → g5: one step, up exactly 2 (g5 stayed behind)
    show JumpOK (A55 W) (6, 5) (6, 4)
    refine jumpOK_rank_step ?_ (by decide) (by decide)
    rw [hL55 hg6 (by decide), A55_g5 hW]; left; ring
  · show JumpOK (A55 W) (6, 4) (6, 5)
    refine jumpOK_rank_step ?_ (by decide) (by decide)
    rw [hL55 hg6 (by decide), A55_g5 hW]; right; ring
  · show JumpOK (A55 W) (6, 5) (7, 7)
    exact jumpOK_level (by rw [hL55 hg6 (by decide), hL55 hh8 (by decide)]) (by decide)

lemma longTour_moves {W : ℚ} (hW : 286 < W) : MovesOK start (longTour W) :=
  (movesOK_append _ _ _).2 ⟨pre53_moves, sfx_moves hW⟩

lemma visits_longTour (W : ℚ) : visits (longTour W) = visits (longTour 0) := by
  simp [visits, longTour, sfx]

lemma dests_longTour (W : ℚ) : (longTour W).map Move.dest = (longTour 0).map Move.dest := by
  simp [longTour, sfx]

theorem longTour_legal {W : ℚ} (hW : 286 < W) : Legal (longTour W) := by
  refine ⟨longTour_moves hW, ?_, ?_, ?_⟩
  · rw [dests_longTour]; decide +kernel
  · rw [visits_longTour]; decide +kernel
  · rw [visits_longTour]; decide +kernel

theorem longTour_time (W : ℚ) : time (longTour W) = 288 + W := by
  simp [longTour, time, List.sum_append] at *
  have := pre53_time; simp [time] at this
  rw [this, sfx]; norm_num; try ring

/-! ### Necessity of every wait -/

/-- With `t' = 52` the group lands exactly on altitude 4, the b4 sit then has 29 points, and the
jump g6 → g5 is no longer a rise of 2. -/
lemma break_W : ¬ MovesOK start (longTour 52) := by decide +kernel

/-- Without the 50-minute sit, g5 and g6 stay level and the jump g6 → g5 is illegal. -/
lemma break_50 (W : ℚ) : ¬ MovesOK s53 [⟨W, (1, 3)⟩, ⟨0, (3, 4)⟩, ⟨0, (5, 3)⟩, ⟨0, (6, 5)⟩,
    ⟨0, (6, 4)⟩, ⟨0, (6, 5)⟩, ⟨0, (7, 7)⟩] := by
  obtain ⟨-, -, -, hg6, hg5, -⟩ := mem_L
  intro h
  simp only [MovesOK, St.next, s53_pos, sit_zero] at h
  obtain ⟨-, -, -, -, -, -, -, -, -, h5, -⟩ := h
  have h5' : JumpOK (A54 W) (6, 5) (6, 4) := h5
  unfold JumpOK at h5'
  rw [A54_L W hg6, A54_L W hg5, sub_self] at h5'
  revert h5'; decide

theorem longTour_allNecessary {W : ℚ} (hW : 286 < W) : AllNecessary (longTour W) := by
  intro i hi hpos
  have hlen : (longTour W).length = 60 := by simp [longTour, sfx, pre53_length]
  by_cases h53 : i < 53
  · refine necessary_of_strict_prefix (k := 53) (longTour_moves hW) ?_ h53 hi hpos
    have : (longTour W).take 53 = pre53 := by
      simp [longTour, List.take_append, pre53_length]
    rw [this]; exact pre53_strict
  · have hget : (longTour W)[i] = (sfx W)[i - 53]'(by simp [sfx]; omega) := by
      simp only [longTour]
      rw [List.getElem_append_right (by rw [pre53_length]; omega)]
      simp [pre53_length]
    rw [hget] at hpos
    have hi' : i - 53 < 7 := by rw [hlen] at hi; omega
    by_cases h54 : i = 53
    · subst h54
      refine ⟨52, by norm_num, ?_⟩
      have : setWait (longTour W) 53 52 = longTour 52 := by
        rw [longTour, ← pre53_length, sfx, setWait_append]; rfl
      rw [this]; exact break_W
    by_cases h55 : i = 54
    · subst h55
      refine ⟨0, le_rfl, ?_⟩
      have : setWait (longTour W) 54 0 = pre53 ++ [⟨W, (1, 3)⟩, ⟨0, (3, 4)⟩, ⟨0, (5, 3)⟩,
          ⟨0, (6, 5)⟩, ⟨0, (6, 4)⟩, ⟨0, (6, 5)⟩, ⟨0, (7, 7)⟩] := by
        rw [longTour, show 54 = pre53.length + 1 by rw [pre53_length], setWait_append_right]
        rfl
      rw [this, movesOK_append]
      exact fun h => break_50 W h.2
    · exfalso
      have : i - 53 ∈ ({2, 3, 4, 5, 6} : Finset ℕ) := by simp; omega
      simp at this
      rcases this with h | h | h | h | h <;> simp [sfx, h] at hpos

/-- **Theorem 1.** Under the puzzle's rules as worded, tours can be arbitrarily long. -/
theorem unbounded (N : ℚ) : ∃ tour, Valid tour ∧ N < time tour := by
  refine ⟨longTour (max N 0 + 287), ⟨longTour_legal ?_, longTour_allNecessary ?_⟩, ?_⟩
  · have := le_max_right N 0; linarith
  · have := le_max_right N 0; linarith
  · rw [longTour_time]; have := le_max_left N 0; linarith

end KM5

namespace KM5

/-- A concrete instance: the tour of 100,288 minutes from `proofs/UPPER_BOUND.md` obeys every rule. -/
theorem longTour_100288 : Valid (longTour 100000) ∧ time (longTour 100000) = 100288 :=
  ⟨⟨longTour_legal (by norm_num), longTour_allNecessary (by norm_num)⟩,
    by rw [longTour_time]; norm_num⟩

end KM5

namespace KM5

/-- Every length above 574 minutes is achieved exactly by a tour that obeys every rule. -/
theorem every_length_above_574 (T : ℚ) (hT : 574 < T) : ∃ tour, Valid tour ∧ time tour = T :=
  ⟨longTour (T - 288), ⟨longTour_legal (by linarith), longTour_allNecessary (by linarith)⟩,
    by rw [longTour_time]; ring⟩

end KM5

namespace KM5

/-- **There is no longest route:** no tour that obeys every rule is at least as long as every
other such tour. -/
theorem no_longest_tour : ¬ ∃ best, Valid best ∧ ∀ tour, Valid tour → time tour ≤ time best := by
  rintro ⟨best, -, h⟩
  obtain ⟨tour, hv, hlt⟩ := unbounded (time best)
  exact absurd (h tour hv) (not_le.mpr hlt)

end KM5
