import KnightMoves5.Rules

/-!
# General facts about the rules

* runs of tours split over `++`;
* a point and its opposite are never one jump apart;
* the knight's own square always sinks, so a group has at least one point;
* a strict wait is necessary in the puzzle's sense (`strict_allNecessary`).
-/

namespace KM5

/-- The state after playing a list of moves. -/
def run (s : St) (l : List Move) : St := l.foldl St.next s

@[simp] lemma run_nil (s : St) : run s [] = s := rfl
@[simp] lemma run_cons (s : St) (m : Move) (l : List Move) : run s (m :: l) = run (s.next m) l := rfl

lemma run_append (s : St) (l₁ l₂ : List Move) : run s (l₁ ++ l₂) = run (run s l₁) l₂ := by
  simp [run, List.foldl_append]

lemma movesOK_append (s : St) (l₁ l₂ : List Move) :
    MovesOK s (l₁ ++ l₂) ↔ MovesOK s l₁ ∧ MovesOK (run s l₁) l₂ := by
  induction l₁ generalizing s with
  | nil => simp [MovesOK]
  | cons m l ih => simp [MovesOK, ih, and_assoc]

lemma strict_append (s : St) (l₁ l₂ : List Move) :
    Strict s (l₁ ++ l₂) ↔ Strict s l₁ ∧ Strict (run s l₁) l₂ := by
  induction l₁ generalizing s with
  | nil => simp [Strict]
  | cons m l ih => simp [Strict, ih, and_assoc]

lemma setWait_append (l₁ l₂ : List Move) (m : Move) (t : ℚ) :
    setWait (l₁ ++ m :: l₂) l₁.length t = l₁ ++ { m with wait := t } :: l₂ := by
  induction l₁ with
  | nil => simp [setWait]
  | cons a l ih => simpa [setWait, List.modify_succ_cons] using ih

lemma setWait_append_right (l₁ l₂ : List Move) (k : ℕ) (t : ℚ) :
    setWait (l₁ ++ l₂) (l₁.length + k) t = l₁ ++ setWait l₂ k t := by
  induction l₁ with
  | nil => simp
  | cons a l ih =>
    rw [List.cons_append, List.length_cons, show l.length + 1 + k = (l.length + k) + 1 by omega]
    simpa [setWait, List.modify_succ_cons] using ih

/-- Planar part of a legal jump. -/
def PlanarOK (P Q : Sq) : Prop :=
  (Nat.dist P.1 Q.1, Nat.dist P.2 Q.2) ∈ [(0, 1), (0, 2), (1, 0), (1, 2), (2, 0), (2, 1)]

instance (P Q : Sq) : Decidable (PlanarOK P Q) := inferInstanceAs (Decidable (_ ∈ _))

lemma JumpOK.planar {A : Alt} {P Q : Sq} (h : JumpOK A P Q) : PlanarOK P Q := by
  unfold JumpOK at h; unfold PlanarOK
  rcases h with h | h | h | h | h | h <;> simp [h.1, h.2.1]

/-- A point and its opposite are never one jump apart (their file and rank differences are odd). -/
lemma not_planarOK_opp : ∀ P : Sq, ¬ PlanarOK P (opp P) := by decide

lemma ne_opp_of_jumpOK {A : Alt} {P Q : Sq} (h : JumpOK A P Q) : Q ≠ opp P := by
  rintro rfl; exact not_planarOK_opp P h.planar

lemma opp_ne_self : ∀ P : Sq, opp P ≠ P := by decide

lemma self_inGroup (A : Alt) (P : Sq) : InGroup A P P := ⟨rfl, (opp_ne_self P).symm⟩

lemma self_mem_group (A : Alt) (P : Sq) : P ∈ group A P := by
  simp [group, self_inGroup]

lemma card_group_pos (A : Alt) (P : Sq) : 0 < (group A P).card :=
  Finset.card_pos.mpr ⟨P, self_mem_group A P⟩

lemma sinkAmt_nonneg {A : Alt} {P : Sq} {t : ℚ} (ht : 0 ≤ t) : 0 ≤ sinkAmt A P t :=
  div_nonneg ht (by positivity)

lemma group_card_mul_sinkAmt (A : Alt) (P : Sq) (t : ℚ) : ((group A P).card : ℚ) * sinkAmt A P t = t := by
  have : ((group A P).card : ℚ) ≠ 0 := by exact_mod_cast (card_group_pos A P).ne'
  unfold sinkAmt; field_simp

@[simp] lemma sinkAmt_zero (A : Alt) (P : Sq) : sinkAmt A P 0 = 0 := by simp [sinkAmt]

@[simp] lemma sit_zero (A : Alt) (P : Sq) : sit A P 0 = A := by
  funext x; simp [sit]

lemma sit_self (A : Alt) (P : Sq) (t : ℚ) : sit A P t P = A P - sinkAmt A P t := by
  simp [sit, self_inGroup]

lemma sit_of_inGroup {A : Alt} {P x : Sq} (t : ℚ) (hx : InGroup A P x) :
    sit A P t x = A P - sinkAmt A P t := by
  simp [sit, hx, hx.1]

/-- A knight move between two points at the same altitude is legal. -/
lemma jumpOK_level {A : Alt} {P Q : Sq} (h : A Q = A P)
    (hk : (Nat.dist P.1 Q.1 = 1 ∧ Nat.dist P.2 Q.2 = 2) ∨ (Nat.dist P.1 Q.1 = 2 ∧ Nat.dist P.2 Q.2 = 1)) :
    JumpOK A P Q := by
  unfold JumpOK; rw [h, sub_self]
  rcases hk with ⟨h1, h2⟩ | ⟨h1, h2⟩
  · exact Or.inr (Or.inr (Or.inr (Or.inl ⟨h1, h2, rfl⟩)))
  · exact Or.inr (Or.inr (Or.inr (Or.inr (Or.inr ⟨h1, h2, rfl⟩))))

/-- A one-step jump along a rank with an altitude change of exactly ±2 is legal. -/
lemma jumpOK_rank_step {A : Alt} {P Q : Sq} (h : AbsEq (A Q - A P) 2)
    (h1 : Nat.dist P.1 Q.1 = 0) (h2 : Nat.dist P.2 Q.2 = 1) : JumpOK A P Q :=
  Or.inl ⟨h1, h2, h⟩

/-- A point outside the group that is not the opposite point keeps its altitude. -/
lemma sit_of_not_inGroup {A : Alt} {P x : Sq} (t : ℚ) (hx : ¬ InGroup A P x) (hxo : x ≠ opp P) :
    sit A P t x = A x := by
  simp [sit, hx, Rises, hxo]

/-- **A strict wait is pinned by its own jump:** sinking another half unit makes that jump illegal. -/
lemma pinned {A : Alt} {P Q : Sq} {t : ℚ} (hQ : ¬ InGroup A P Q) (hj : JumpOK (sit A P t) P Q) :
    ¬ JumpOK (sit A P (t + (group A P).card / 2)) P Q := by
  have hQo := ne_opp_of_jumpOK hj
  have hn : ((group A P).card : ℚ) ≠ 0 := by exact_mod_cast (card_group_pos A P).ne'
  have key : sit A P (t + (group A P).card / 2) Q - sit A P (t + (group A P).card / 2) P =
      (sit A P t Q - sit A P t P) + 1 / 2 := by
    rw [sit_of_not_inGroup _ hQ hQo, sit_of_not_inGroup _ hQ hQo, sit_self, sit_self]
    unfold sinkAmt; field_simp; ring
  intro h'
  unfold JumpOK at hj h'
  simp only [key] at h'
  generalize sit A P t Q - sit A P t P = z at hj h'
  generalize Nat.dist P.1 Q.1 = dx at hj h'
  generalize Nat.dist P.2 Q.2 = dy at hj h'
  unfold AbsEq at hj h'
  rcases hj with ⟨rfl, rfl, h | h⟩ | ⟨rfl, rfl, h | h⟩ | ⟨rfl, rfl, h | h⟩ | ⟨rfl, rfl, h⟩ |
      ⟨rfl, rfl, h | h⟩ | ⟨rfl, rfl, h⟩ <;> subst h <;> norm_num at h'

lemma necessary_of_split (l₁ l₂ : List Move) (m : Move) (h : MovesOK start (l₁ ++ m :: l₂))
    (hs : 0 < m.wait → ¬ InGroup (run start l₁).alt (run start l₁).pos m.dest) (hpos : 0 < m.wait) :
    Necessary (l₁ ++ m :: l₂) l₁.length := by
  set s := run start l₁
  have hmv : MovesOK s (m :: l₂) := ((movesOK_append _ _ _).1 h).2
  refine ⟨m.wait + (group s.alt s.pos).card / 2, by
    have := hmv.1; have : (0 : ℚ) ≤ (group s.alt s.pos).card := by positivity
    linarith, ?_⟩
  intro hbad
  rw [setWait_append, movesOK_append] at hbad
  exact pinned (hs hpos) hmv.2.1 hbad.2.2.1

/-- In a tour with legal moves, every strict wait is necessary in the puzzle's sense. -/
theorem strict_allNecessary {tour : List Move} (h : MovesOK start tour) (hs : Strict start tour) :
    AllNecessary tour := by
  intro i hi hpos
  have key : tour = tour.take i ++ tour[i] :: tour.drop (i + 1) := by
    conv_lhs => rw [← List.take_append_drop i tour, List.drop_eq_getElem_cons hi]
  have hlen : (tour.take i).length = i := by simp; omega
  have h' : MovesOK start (tour.take i ++ tour[i] :: tour.drop (i + 1)) := key ▸ h
  have hs' : Strict start (tour.take i ++ tour[i] :: tour.drop (i + 1)) := key ▸ hs
  have := necessary_of_split _ _ _ h' ((strict_append _ _ _).1 hs').2.1 hpos
  rwa [← key, hlen] at this

/-- Waits inside a strict prefix of a tour with legal moves are necessary. -/
theorem necessary_of_strict_prefix {tour : List Move} {k : ℕ} (h : MovesOK start tour)
    (hs : Strict start (tour.take k)) {i : ℕ} (hik : i < k) (hi : i < tour.length)
    (hpos : 0 < (tour[i]).wait) : Necessary tour i := by
  have key : tour = tour.take i ++ tour[i] :: tour.drop (i + 1) := by
    conv_lhs => rw [← List.take_append_drop i tour, List.drop_eq_getElem_cons hi]
  have hlen : (tour.take i).length = i := by simp; omega
  have hk : tour.take k = tour.take i ++ tour[i] :: (tour.take k).drop (i + 1) := by
    have hik' : i < (tour.take k).length := by simp; omega
    conv_lhs => rw [← List.take_append_drop i (tour.take k), List.drop_eq_getElem_cons hik']
    simp [List.take_take, Nat.min_eq_left hik.le]
  rw [hk, strict_append] at hs
  have h' : MovesOK start (tour.take i ++ tour[i] :: tour.drop (i + 1)) := key ▸ h
  have := necessary_of_split _ _ _ h' hs.2.1 hpos
  rwa [← key, hlen] at this

end KM5
