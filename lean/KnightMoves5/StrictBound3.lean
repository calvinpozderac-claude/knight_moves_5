import KnightMoves5.StrictBound

/-!
# Theorem 3: every strict tour lasts at most 16,094⅓ minutes

The proof of Theorem 2 (`StrictBound.lean`) charges every floor-lowering sit for all 64 points
following the floor down. This file proves that they cannot: a sit that lowers the floor and sinks
more than 2 leaves its opposite point more than 2 above the floor (a **knock-out**). Notation as in
`proofs/UPPER_BOUND.md`: `m` the running floor, `B = -m` its total drop, `M` moves, `h` the knight's
height above the floor.

* **Depth identity.** Let `μ x` be the floor just after `x` last sank and `ρ x` the rise of `x`
  since then. Then `μ x + ρ x ≤ alt x`, so `T ≤ 421 + R - ∑ μ - ∑ ρ`.
* **Knock-out records.** Every floor-lowering sit that sinks more than 2 is recorded (`Rec`) with its
  floor drop `del`, the knight's height `h`, the floor `mb` before it, and the opposite point `own`.
  A record stays open until `own` sinks again. Open records satisfy `μ own ≥ mb`, and the drops of
  a record and of all newer ones fit under the floor drop since that record (`TailOK`). With at most
  3 records per point (visits), `12 ∑ (μ - m) ≥ W² + 2W`, where `W` is the open records' total drop
  (`TailOK.quad`, `group_sum`).
* **Rescues are paid for by climbing.** An opposite point can only sink again at a sit at its own
  altitude, so the knight must climb back up to the altitude it had at the knocked-out sit. The
  potential `Psi` (the segment's best height, the knight's current height, and a bonus per record)
  shows `B + B_nK + B_cov ≤ 2M + 2`, where `B_cov` is the drop of the records whose opposite sank
  again and `B_nK` the drop of floor-lowering sits that sink at most 2 (`xinv_step`). Hence
  `W ≥ 2B - 2M - 2`.
* **Unpaid rises.** The rise of an open record's opposite is still in `ρ`, so `W ≤ ∑ ρ + (D - R)`.

Together: `T ≤ 421 + 2M + 65B - 7W/6 - W²/12` with `W ≥ max 0 (2B - 2M - 2)`, which is at most
`67 M + 3431⅓`, and `M ≤ 189` gives `16,094⅓`.
-/

namespace KM5.B3
open KM5

/-- A floor-lowering sit that sank more than 2 (it knocked its opposite out): the opposite point
`own`, the floor drop `del`, the knight's height `h` before the sit, the floor `mb` before the sit,
and whether `own` has sunk since (`cov`). -/
structure Rec where
  own : Sq
  del : ℚ
  h : ℚ
  mb : ℚ
  cov : Bool
  rd : ℚ
  lv : ℚ

/-- Records are listed newest first; each record's floor drop, plus those of all newer records, is
at most the floor drop since that record's sit. -/
def TailOK (m : ℚ) : ℚ → List Rec → Prop
  | _, [] => True
  | s, r :: l => s + r.del ≤ r.mb - m ∧ TailOK m (s + r.del) l

lemma TailOK.mono {m : ℚ} : ∀ {s s' : ℚ} {l : List Rec}, TailOK m s l → s' ≤ s → TailOK m s' l
  | _, _, [], _, _ => trivial
  | _, _, _ :: _, ⟨h1, h2⟩, hs => ⟨by linarith, h2.mono (by linarith)⟩

lemma TailOK.shift {m : ℚ} (x : ℚ) : ∀ {s : ℚ} {l : List Rec}, TailOK m s l → TailOK (m - x) (s + x) l
  | _, [], _ => trivial
  | _, r :: _, ⟨h1, h2⟩ => ⟨by linarith, by
      have := h2.shift x; rwa [show _ + x + r.del = _ + r.del + x by ring]⟩

lemma TailOK.filter {m : ℚ} (p : Rec → Bool) : ∀ {s : ℚ} {l : List Rec}, (∀ r ∈ l, 0 ≤ r.del) →
    TailOK m s l → TailOK m s (l.filter p)
  | _, [], _, _ => trivial
  | s, r :: l, hd, ⟨h1, h2⟩ => by
    have hl : ∀ r ∈ l, 0 ≤ r.del := fun x hx => hd x (List.mem_cons_of_mem _ hx)
    have hr := hd r (List.mem_cons_self ..)
    by_cases hp : p r
    · rw [List.filter_cons_of_pos hp]; exact ⟨h1, TailOK.filter p hl h2⟩
    · rw [List.filter_cons_of_neg hp]; exact (TailOK.filter p hl h2).mono (by linarith)

lemma TailOK.map_cov {m : ℚ} (f : Rec → Rec) (hf : ∀ r, (f r).del = r.del ∧ (f r).mb = r.mb) :
    ∀ {s : ℚ} {l : List Rec}, TailOK m s l → TailOK m s (l.map f)
  | _, [], _ => trivial
  | _, r :: _, ⟨h1, h2⟩ => ⟨by rw [(hf r).1, (hf r).2]; exact h1, by
      rw [(hf r).1]; exact TailOK.map_cov f hf h2⟩

lemma TailOK.mono_m {m m' : ℚ} (hm : m' ≤ m) : ∀ {s : ℚ} {l : List Rec}, TailOK m s l → TailOK m' s l
  | _, [], _ => trivial
  | _, _ :: _, ⟨h1, h2⟩ => ⟨by linarith, TailOK.mono_m hm h2⟩

def dsum (l : List Rec) : ℚ := (l.map Rec.del).sum

/-- If some records satisfy `p`, the drops of all of them (plus `s`) fit under the floor drop since
the oldest one. -/
lemma TailOK.exists_cover {m : ℚ} (p : Rec → Bool) : ∀ {s : ℚ} {l : List Rec}, (∀ r ∈ l, 0 ≤ r.del) →
    TailOK m s l → (∃ r ∈ l, p r) → ∃ r ∈ l, p r ∧ s + dsum (l.filter p) ≤ r.mb - m
  | _, [], _, _, ⟨_, h, _⟩ => by simp at h
  | s, r :: l, hd, ⟨h1, h2⟩, hex => by
    have hl : ∀ r ∈ l, 0 ≤ r.del := fun x hx => hd x (List.mem_cons_of_mem _ hx)
    have hr := hd r (List.mem_cons_self ..)
    by_cases hl' : ∃ x ∈ l, p x
    · obtain ⟨x, hx, hpx, hle⟩ := TailOK.exists_cover p hl h2 hl'
      refine ⟨x, List.mem_cons_of_mem _ hx, hpx, ?_⟩
      by_cases hp : p r
      · rw [List.filter_cons_of_pos hp]; simp only [dsum, List.map_cons, List.sum_cons] at hle ⊢
        linarith
      · rw [List.filter_cons_of_neg hp]; simp only [dsum] at hle ⊢; linarith
    · push Not at hl'
      have hp : p r := by
        obtain ⟨x, hx, hpx⟩ := hex
        rcases List.mem_cons.mp hx with rfl | hx
        · exact hpx
        · exact absurd hpx (by simpa using hl' x hx)
      have hnil : l.filter p = [] := List.filter_eq_nil_iff.mpr (fun x hx => by simpa using hl' x hx)
      refine ⟨r, List.mem_cons_self .., hp, ?_⟩
      rw [List.filter_cons_of_pos hp, hnil]; simpa [dsum] using h1

/-- **The quadratic loss bound.** Under the tail condition, `4 ∑ (mb - m) ≥ (s + W)² - s²`, where
`W` is the total drop of the records. -/
lemma TailOK.quad {m : ℚ} : ∀ {s : ℚ} {l : List Rec}, 0 ≤ s → (∀ r ∈ l, 0 ≤ r.del ∧ r.del ≤ 2) →
    TailOK m s l → (s + dsum l) ^ 2 - s ^ 2 + 2 * dsum l ≤ 4 * (l.map (fun r => r.mb - m)).sum
  | _, [], _, _, _ => by simp [dsum]
  | s, r :: l, hs, hd, ⟨h1, h2⟩ => by
    have hl : ∀ r ∈ l, 0 ≤ r.del ∧ r.del ≤ 2 := fun x hx => hd x (List.mem_cons_of_mem _ hx)
    have hr := hd r (List.mem_cons_self ..)
    have ih := TailOK.quad (by linarith) hl h2
    simp only [dsum, List.map_cons, List.sum_cons] at ih ⊢
    nlinarith


/-! ### The extended state -/

/-- `r`'s opposite point sinks at a sit on `P` (and `r` was still open). -/
def hit (A : Alt) (P : Sq) (r : Rec) : Bool := !r.cov && decide (InGroup A P r.own)

def mark (A : Alt) (P : Sq) (r : Rec) : Rec := if hit A P r = true then { r with cov := true } else r

@[simp] lemma mark_del (A : Alt) (P : Sq) (r : Rec) : (mark A P r).del = r.del := by
  unfold mark; split <;> rfl
@[simp] lemma mark_mb (A : Alt) (P : Sq) (r : Rec) : (mark A P r).mb = r.mb := by
  unfold mark; split <;> rfl
@[simp] lemma mark_h (A : Alt) (P : Sq) (r : Rec) : (mark A P r).h = r.h := by
  unfold mark; split <;> rfl
@[simp] lemma mark_own (A : Alt) (P : Sq) (r : Rec) : (mark A P r).own = r.own := by
  unfold mark; split <;> rfl
@[simp] lemma mark_rd (A : Alt) (P : Sq) (r : Rec) : (mark A P r).rd = r.rd := by
  unfold mark; split <;> rfl
@[simp] lemma mark_lv (A : Alt) (P : Sq) (r : Rec) : (mark A P r).lv = r.lv := by
  unfold mark; split <;> rfl
lemma mark_cov (A : Alt) (P : Sq) (r : Rec) : (mark A P r).cov = (r.cov || hit A P r) := by
  unfold mark hit; by_cases h : r.cov <;> by_cases g : InGroup A P r.own <;> simp [h, g]

def bonus (r : Rec) : ℚ := max 0 (2 - r.h)
lemma bonus_nonneg (r : Rec) : 0 ≤ bonus r := le_max_left _ _

def bsum (l : List Rec) : ℚ := (l.map bonus).sum
def covSum (l : List Rec) : ℚ := dsum (l.filter (fun r => r.cov))
def pendSum (l : List Rec) : ℚ := dsum (l.filter (fun r => !r.cov))

@[simp] lemma dsum_nil : dsum [] = 0 := rfl
@[simp] lemma dsum_cons (r : Rec) (l : List Rec) : dsum (r :: l) = r.del + dsum l := by simp [dsum]
@[simp] lemma dsum_append (l l' : List Rec) : dsum (l ++ l') = dsum l + dsum l' := by simp [dsum]
@[simp] lemma bsum_nil : bsum [] = 0 := rfl
@[simp] lemma bsum_cons (r : Rec) (l : List Rec) : bsum (r :: l) = bonus r + bsum l := by simp [bsum]
@[simp] lemma bsum_append (l l' : List Rec) : bsum (l ++ l') = bsum l + bsum l' := by simp [bsum]

lemma bsum_nonneg (l : List Rec) : 0 ≤ bsum l :=
  List.sum_nonneg (by simp only [List.mem_map]; rintro _ ⟨r, -, rfl⟩; exact bonus_nonneg r)

lemma bsum_split (l : List Rec) :
    bsum l = bsum (l.filter (fun r => r.cov)) + bsum (l.filter (fun r => !r.cov)) := by
  induction l with
  | nil => simp
  | cons r l ih => by_cases h : r.cov <;> simp [h, ih] <;> ring

lemma bsum_map_mark (A : Alt) (P : Sq) (l : List Rec) : bsum (l.map (mark A P)) = bsum l := by
  induction l with
  | nil => simp
  | cons r l ih => simp [ih, bonus]

lemma covSum_map_mark (A : Alt) (P : Sq) (l : List Rec) :
    covSum (l.map (mark A P)) = covSum l + dsum (l.filter (hit A P)) := by
  induction l with
  | nil => simp [covSum]
  | cons r l ih =>
    simp only [covSum] at ih ⊢
    simp only [List.map_cons, List.filter_cons, mark_cov]
    by_cases hc : r.cov <;> by_cases hh : hit A P r <;> simp [hc, hh, ih] <;>
      simp_all [hit] <;> ring

lemma pendSum_map_mark (A : Alt) (P : Sq) (l : List Rec) :
    pendSum (l.map (mark A P)) = pendSum l - dsum (l.filter (hit A P)) := by
  induction l with
  | nil => simp [pendSum]
  | cons r l ih =>
    simp only [pendSum] at ih ⊢
    simp only [List.map_cons, List.filter_cons, mark_cov]
    by_cases hc : r.cov <;> by_cases hh : hit A P r <;> simp [hc, hh, ih] <;>
      simp_all [hit] <;> ring

lemma single_le_bsum {l : List Rec} {r : Rec} (h : r ∈ l) : bonus r ≤ bsum l :=
  List.single_le_sum (by simp only [List.mem_map]; rintro _ ⟨r, -, rfl⟩; exact bonus_nonneg r) _
    (List.mem_map_of_mem h)

/-- The tour's state, with the bookkeeping of the proof. -/
structure XS where
  alt : Alt
  pos : Sq
  m : ℚ
  mu : Sq → ℚ
  rho : Sq → ℚ
  recs : List Rec
  Hm : ℚ
  T : ℚ
  R : ℚ
  D : ℚ
  Z : ℚ
  B : ℚ
  BnK : ℚ
  Bcov : ℚ
  M : ℕ
  vis : Sq → ℕ

def rise (A : Alt) (P : Sq) (t : ℚ) : ℚ := ∑ x, (if Rises A P x then sinkAmt A P t else 0)

def XS.step (e : XS) (mv : Move) : XS :=
  if e.alt e.pos - sinkAmt e.alt e.pos mv.wait < e.m then
    { alt := sit e.alt e.pos mv.wait, pos := mv.dest, m := e.alt e.pos - sinkAmt e.alt e.pos mv.wait,
      mu := fun x => if InGroup e.alt e.pos x then e.alt e.pos - sinkAmt e.alt e.pos mv.wait else e.mu x,
      rho := fun x => if InGroup e.alt e.pos x then 0
        else e.rho x + if Rises e.alt e.pos x then sinkAmt e.alt e.pos mv.wait else 0,
      recs := (if 2 < sinkAmt e.alt e.pos mv.wait then
          [⟨opp e.pos, e.m - (e.alt e.pos - sinkAmt e.alt e.pos mv.wait), e.alt e.pos - e.m, e.m, false,
            rise e.alt e.pos mv.wait, sinkAmt e.alt e.pos mv.wait - rise e.alt e.pos mv.wait⟩]
          else []) ++ (e.recs.map (mark e.alt e.pos)).filter (fun r => !r.cov),
      Hm := sit e.alt e.pos mv.wait mv.dest - (e.alt e.pos - sinkAmt e.alt e.pos mv.wait),
      T := e.T + mv.wait, R := e.R + rise e.alt e.pos mv.wait, D := e.D + sinkAmt e.alt e.pos mv.wait,
      Z := e.Z + (sit e.alt e.pos mv.wait mv.dest - sit e.alt e.pos mv.wait e.pos),
      B := e.B + (e.m - (e.alt e.pos - sinkAmt e.alt e.pos mv.wait)),
      BnK := e.BnK + (if 2 < sinkAmt e.alt e.pos mv.wait then 0
        else e.m - (e.alt e.pos - sinkAmt e.alt e.pos mv.wait)),
      Bcov := e.Bcov + dsum (e.recs.filter (hit e.alt e.pos)), M := e.M + 1,
      vis := fun x => e.vis x + if x = mv.dest then 1 else 0 }
  else
    { alt := sit e.alt e.pos mv.wait, pos := mv.dest, m := e.m,
      mu := fun x => if InGroup e.alt e.pos x then e.m else e.mu x,
      rho := fun x => if InGroup e.alt e.pos x then 0
        else e.rho x + if Rises e.alt e.pos x then sinkAmt e.alt e.pos mv.wait else 0,
      recs := e.recs.map (mark e.alt e.pos),
      Hm := max e.Hm (sit e.alt e.pos mv.wait mv.dest - e.m),
      T := e.T + mv.wait, R := e.R + rise e.alt e.pos mv.wait, D := e.D + sinkAmt e.alt e.pos mv.wait,
      Z := e.Z + (sit e.alt e.pos mv.wait mv.dest - sit e.alt e.pos mv.wait e.pos),
      B := e.B, BnK := e.BnK, Bcov := e.Bcov + dsum (e.recs.filter (hit e.alt e.pos)), M := e.M + 1,
      vis := fun x => e.vis x + if x = mv.dest then 1 else 0 }

/-- The potential of the rescue argument. -/
def Psi (e : XS) : ℚ :=
  max 0 (e.Hm - 2) - covSum e.recs + max 0 (2 - (e.alt e.pos - e.m)) + bsum e.recs


lemma covSum_filter_pend (l : List Rec) : covSum (l.filter (fun r => !r.cov)) = 0 := by
  induction l with
  | nil => rfl
  | cons r l ih => by_cases h : r.cov <;> simp_all [covSum, List.filter_cons]

lemma pendSum_filter_pend (l : List Rec) : pendSum (l.filter (fun r => !r.cov)) = pendSum l := by
  simp [pendSum, List.filter_filter]

lemma len_own_map_mark (A : Alt) (P : Sq) (y : Sq) (l : List Rec) :
    ((l.map (mark A P)).filter (fun r => decide (r.own = y))).length =
      (l.filter (fun r => decide (r.own = y))).length := by
  induction l with
  | nil => rfl
  | cons r l ih => by_cases h : r.own = y <;> simp [List.filter_cons, h, ih]

lemma len_filter_le (p q : Rec → Bool) (l : List Rec) :
    ((l.filter q).filter p).length ≤ (l.filter p).length := by
  rw [List.filter_comm]; exact List.length_filter_le _ _

/-- The covered records fit under the segment's best height: the heart of the rescue argument. -/
lemma cover_bracket {m Hm : ℚ} {l : List Rec} (ht : TailOK m 0 l) (hd : ∀ r ∈ l, 0 ≤ r.del)
    (hc : ∀ r ∈ l, r.cov = true → r.mb + r.h ≤ m + Hm) :
    covSum l ≤ max 0 (Hm - 2) + bsum (l.filter (fun r => r.cov)) := by
  by_cases hex : ∃ r ∈ l, r.cov = true
  · obtain ⟨r, hr, hrc, hle⟩ := ht.exists_cover (fun r => r.cov) hd hex
    have h1 := hc r hr hrc
    have h2 : bonus r ≤ bsum (l.filter (fun r => r.cov)) := single_le_bsum (by simp [hr, hrc])
    have h3 : 2 - r.h ≤ bonus r := le_max_right _ _
    have h4 := le_max_right 0 (Hm - 2)
    simp only [covSum] at hle ⊢; linarith
  · push Not at hex
    have : l.filter (fun r => r.cov) = [] := List.filter_eq_nil_iff.mpr (fun r hr => by simpa using hex r hr)
    simp [covSum, this]

lemma psi_nonneg {e : XS} (ht : TailOK e.m 0 e.recs) (hd : ∀ r ∈ e.recs, 0 ≤ r.del)
    (hc : ∀ r ∈ e.recs, r.cov = true → r.mb + r.h ≤ e.m + e.Hm) : 0 ≤ Psi e := by
  have h1 := cover_bracket ht hd hc
  have h2 := bsum_split e.recs
  have h3 := bsum_nonneg (e.recs.filter (fun r => !r.cov))
  have h4 := le_max_left 0 (2 - (e.alt e.pos - e.m))
  unfold Psi; linarith

def pendF (f : Rec → ℚ) (l : List Rec) : ℚ := ((l.filter (fun r => !r.cov)).map f).sum
def ownF (f : Rec → ℚ) (y : Sq) (l : List Rec) : ℚ :=
  (((l.filter (fun r => !r.cov)).filter (fun r => decide (r.own = y))).map f).sum

lemma pendF_marked_le (A : Alt) (P : Sq) (f : Rec → ℚ) (hf : ∀ r, f (mark A P r) = f r) :
    ∀ l : List Rec, (∀ r ∈ l, 0 ≤ f r) → pendF f (l.map (mark A P)) ≤ pendF f l
  | [], _ => le_rfl
  | r :: l, h => by
    have ih := pendF_marked_le A P f hf l (fun x hx => h x (List.mem_cons_of_mem _ hx))
    have hr := h r (List.mem_cons_self ..)
    simp only [pendF] at ih ⊢
    by_cases hc : r.cov <;> by_cases hh : hit A P r <;>
      simp [List.filter_cons, mark_cov, hc, hh, hf] <;> simp_all [hit] <;> linarith

lemma ownF_marked_le (A : Alt) (P : Sq) (f : Rec → ℚ) (hf : ∀ r, f (mark A P r) = f r) (y : Sq) :
    ∀ l : List Rec, (∀ r ∈ l, 0 ≤ f r) → ownF f y (l.map (mark A P)) ≤ ownF f y l
  | [], _ => le_rfl
  | r :: l, h => by
    have ih := ownF_marked_le A P f hf y l (fun x hx => h x (List.mem_cons_of_mem _ hx))
    have hr := h r (List.mem_cons_self ..)
    simp only [ownF] at ih ⊢
    by_cases hc : r.cov <;> by_cases hh : hit A P r <;> by_cases ho : r.own = y <;>
      simp [List.filter_cons, mark_cov, hc, hh, hf, ho] <;> simp_all [hit] <;> linarith

lemma ownF_marked_zero (A : Alt) (P : Sq) (f : Rec → ℚ) (y : Sq) (hy : InGroup A P y)
    (l : List Rec) : ownF f y (l.map (mark A P)) = 0 := by
  unfold ownF
  have : ((l.map (mark A P)).filter (fun r => !r.cov)).filter (fun r => decide (r.own = y)) = [] := by
    rw [List.filter_eq_nil_iff]
    intro r hr
    rw [List.mem_filter, List.mem_map] at hr
    obtain ⟨⟨r0, -, rfl⟩, hc⟩ := hr
    rw [mark_cov] at hc
    simp only [Bool.not_eq_true', Bool.or_eq_false_iff] at hc
    simp only [mark_own, decide_eq_true_eq]
    rintro rfl
    have := hc.2; simp [hit, hc.1, hy] at this
  rw [this]; rfl

lemma pendF_filter (f : Rec → ℚ) (l : List Rec) : pendF f (l.filter (fun r => !r.cov)) = pendF f l := by
  simp [pendF, List.filter_filter]

lemma ownF_filter (f : Rec → ℚ) (y : Sq) (l : List Rec) :
    ownF f y (l.filter (fun r => !r.cov)) = ownF f y l := by
  simp [ownF, List.filter_filter]

lemma pendF_append (f : Rec → ℚ) (l l' : List Rec) : pendF f (l ++ l') = pendF f l + pendF f l' := by
  simp [pendF]

lemma ownF_append (f : Rec → ℚ) (y : Sq) (l l' : List Rec) :
    ownF f y (l ++ l') = ownF f y l + ownF f y l' := by
  simp [ownF]

lemma pendF_le (f g : Rec → ℚ) : ∀ l : List Rec, (∀ r ∈ l, f r ≤ g r) → pendF f l ≤ pendF g l
  | [], _ => le_rfl
  | r :: l, h => by
    have ih := pendF_le f g l (fun x hx => h x (List.mem_cons_of_mem _ hx))
    have hr := h r (List.mem_cons_self ..)
    simp only [pendF] at ih ⊢
    by_cases hc : r.cov <;> simp [List.filter_cons, hc] <;> linarith

lemma pendF_add (f g : Rec → ℚ) (l : List Rec) :
    pendF (fun r => f r + g r) l = pendF f l + pendF g l := by
  simp [pendF, List.sum_map_add]

/-- A sum over records, grouped by their opposite point. -/
lemma sum_own (f : Rec → ℚ) : ∀ l : List Rec,
    (l.map f).sum = ∑ y, ((l.filter (fun r => decide (r.own = y))).map f).sum
  | [] => by simp
  | r :: l => by
    have ih := sum_own f l
    have key : ∀ y, (((r :: l).filter (fun r => decide (r.own = y))).map f).sum =
        (if r.own = y then f r else 0) + ((l.filter (fun r => decide (r.own = y))).map f).sum := by
      intro y; by_cases hy : r.own = y <;> simp [List.filter_cons, hy]
    simp only [key, Finset.sum_add_distrib, Finset.sum_ite_eq, Finset.mem_univ, ite_true,
      List.map_cons, List.sum_cons, ih]

lemma pendF_own (f : Rec → ℚ) (l : List Rec) : pendF f l = ∑ y, ownF f y l := by
  unfold pendF ownF; exact sum_own f _

lemma rise_eq (A : Alt) (P : Sq) (t : ℚ) :
    rise A P t = if Rises A P (opp P) then sinkAmt A P t else 0 := by
  unfold rise
  rw [Finset.sum_eq_single (opp P)]
  · intro x _ hx; simp [Rises, hx]
  · simp

lemma sit_rise {A : Alt} {P x : Sq} (t : ℚ) (hx : ¬ InGroup A P x) :
    sit A P t x = A x + if Rises A P x then sinkAmt A P t else 0 := by
  rw [sit_eq]; simp [hx]

structure XInv (e : XS) : Prop where
  sum : ∑ x, e.alt x = 421 - e.T + e.R
  knight : e.alt e.pos = e.Z - e.D
  floor : ∀ x, e.m ≤ e.alt x
  hB : e.B = -e.m
  B0 : 0 ≤ e.B
  RD : e.R ≤ e.D
  ZM : e.Z ≤ 2 * e.M
  mu : ∀ x, e.m ≤ e.mu x ∧ e.mu x + e.rho x ≤ e.alt x
  rho0 : ∀ x, 0 ≤ e.rho x
  rl : ∀ r ∈ e.recs, 0 ≤ r.rd ∧ 0 ≤ r.lv ∧ r.del ≤ r.rd + r.lv
  rdown : ∀ y, ownF Rec.rd y e.recs ≤ e.rho y
  lvl : pendF Rec.lv e.recs ≤ e.D - e.R
  tail : TailOK e.m 0 e.recs
  del : ∀ r ∈ e.recs, 0 ≤ r.del ∧ r.del ≤ 2
  pend : ∀ r ∈ e.recs, r.cov = false → r.mb ≤ e.mu r.own ∧ r.mb + r.h ≤ e.alt r.own
  cov : ∀ r ∈ e.recs, r.cov = true → r.mb + r.h ≤ e.m + e.Hm
  hm : e.alt e.pos - e.m ≤ e.Hm
  pot : e.B + e.BnK + e.Bcov + Psi e ≤ 2 * e.M + 2
  split : e.B = e.BnK + e.Bcov + pendSum e.recs
  cnt : ∀ x, (e.recs.filter (fun r => decide (r.own = opp x))).length +
    (if x = e.pos then 1 else 0) ≤ e.vis x

lemma opp_opp (x : Sq) : opp (opp x) = x := by simp [opp]

lemma opp_inj {x y : Sq} (h : opp x = opp y) : x = y := by
  rw [← opp_opp x, h, opp_opp]

lemma hit_iff {A : Alt} {P : Sq} {r : Rec} : hit A P r = true ↔ r.cov = false ∧ InGroup A P r.own := by
  simp [hit]

/-- The covered records after a sit (old ones and the newly hit ones) still fit. -/
lemma marked_cov {e : XS} (hI : XInv e) (r : Rec) (hr : r ∈ e.recs.map (mark e.alt e.pos))
    (hc : r.cov = true) : r.mb + r.h ≤ e.m + e.Hm := by
  obtain ⟨r0, h0, rfl⟩ := List.mem_map.mp hr
  rw [mark_cov] at hc; simp only [mark_mb, mark_h]
  rcases Bool.or_eq_true _ _ |>.mp hc with h | h
  · exact hI.cov r0 h0 h
  · obtain ⟨hp, hg⟩ := hit_iff.mp h
    have := (hI.pend r0 h0 hp).2; have := hI.hm; rw [hg.1] at *; linarith


lemma pot_nonfl (Hm h h' : ℚ) (h1 : h ≤ Hm) (h2 : 0 ≤ h') (h3 : h' - h ≤ 2) :
    max 0 (max Hm h' - 2) - max 0 (Hm - 2) + max 0 (2 - h') - max 0 (2 - h) ≤ 2 := by
  simp only [max_def]; split_ifs <;> linarith

lemma lemma1 {A : Alt} {P Q : Sq} {t m : ℚ} (h0 : 0 ≤ t) (hj : JumpOK (sit A P t) P Q)
    (hst : 0 < t → ¬ InGroup A P Q) (hfloor : ∀ x, m ≤ A x) : sinkAmt A P t ≤ 2 + A P - m := by
  rcases eq_or_lt_of_le h0 with h | h
  · rw [← h]; simp; have := hfloor P; linarith
  · have hQ := sit_of_not_inGroup t (hst h) (ne_opp_of_jumpOK hj)
    have := jump_dz hj; rw [hQ, sit_self] at this
    have := hfloor Q; linarith

lemma opp_le_sit {A : Alt} {P : Sq} {t m : ℚ} (h0 : 0 ≤ t) (hfloor : ∀ x, m ≤ A x)
    (hfl : A P - sinkAmt A P t < m) : A P ≤ sit A P t (opp P) := by
  have hn : ¬ InGroup A P (opp P) := fun h => h.2 rfl
  unfold sit; rw [if_neg hn]
  by_cases hr : Rises A P (opp P)
  · rw [if_pos hr]; have := hfloor (opp P); linarith
  · rw [if_neg hr]
    have : A (opp P) = A P := by
      by_contra hne; exact hr ⟨rfl, hne⟩
    rw [this]

lemma xinv_step {e : XS} {mv : Move} (hI : XInv e) (h0 : 0 ≤ mv.wait)
    (hj : JumpOK (sit e.alt e.pos mv.wait) e.pos mv.dest)
    (hst : 0 < mv.wait → ¬ InGroup e.alt e.pos mv.dest) : XInv (e.step mv) := by
  have hmc := marked_cov hI
  obtain ⟨A, P, m, mu, rho, recs, Hm, T, R, D, Z, B, BnK, Bcov, M, vis⟩ := e
  obtain ⟨t, Q⟩ := mv
  obtain ⟨hsum, hk, hfloor, hB, hB0, hRD, hZM, hmu, hrho0, hrl, hrdown, hlvl, htail, hdel, hpend, hcov,
    hhm, hpot, hsplit, hcnt⟩ := hI
  simp only at h0 hj hst hsum hk hfloor hB hB0 hRD hZM hmu hrho0 hrl hrdown hlvl htail hdel hpend hcov hhm hpot hsplit hcnt hmc
  have hrise0 : 0 ≤ rise A P t := rise_nonneg A P h0
  have hrlm : ∀ r ∈ recs.map (mark A P), 0 ≤ r.rd ∧ 0 ≤ r.lv ∧ r.del ≤ r.rd + r.lv := by
    intro r hr; obtain ⟨r0, h0', rfl⟩ := List.mem_map.mp hr; simpa using hrl r0 h0'
  have hlvm : pendF Rec.lv (recs.map (mark A P)) ≤ pendF Rec.lv recs :=
    pendF_marked_le A P Rec.lv (fun r => mark_lv A P r) recs (fun r hr => (hrl r hr).2.1)
  have hrdm : ∀ y, ownF Rec.rd y (recs.map (mark A P)) ≤ ownF Rec.rd y recs := fun y =>
    ownF_marked_le A P Rec.rd (fun r => mark_rd A P r) y recs (fun r hr => (hrl r hr).1)
  have hrho' : ∀ x, ¬ InGroup A P x → 0 ≤ rho x + (if Rises A P x then sinkAmt A P t else 0) := by
    intro x _; have := hrho0 x; have := sinkAmt_nonneg (A := A) (P := P) h0; split_ifs <;> linarith
  have hd0 : 0 ≤ sinkAmt A P t := sinkAmt_nonneg h0
  have hP' : sit A P t P = A P - sinkAmt A P t := sit_self A P t
  have hgrp : ∀ x, InGroup A P x → sit A P t x = A P - sinkAmt A P t := fun x hx => sit_of_inGroup t hx
  have hlow : ∀ x, ¬ InGroup A P x → A x ≤ sit A P t x := fun x hx => le_sit_of_not_inGroup h0 hx
  have hdz := jump_dz hj
  have hL1 := lemma1 h0 hj hst hfloor
  have hrd : rise A P t ≤ sinkAmt A P t := rise_le A P h0
  have hsum' : ∑ x, sit A P t x = 421 - (T + t) + (R + rise A P t) := by
    rw [sum_sit, hsum]; unfold rise; ring
  have hcovm := covSum_map_mark A P recs
  have hpendm := pendSum_map_mark A P recs
  have hbm := bsum_map_mark A P recs
  have hbs := bsum_split (recs.map (mark A P))
  have htailm : TailOK m 0 (recs.map (mark A P)) :=
    htail.map_cov _ (fun r => ⟨mark_del .., mark_mb ..⟩)
  have hdelm : ∀ r ∈ recs.map (mark A P), 0 ≤ r.del ∧ r.del ≤ 2 := by
    intro r hr; obtain ⟨r0, h0', rfl⟩ := List.mem_map.mp hr; simpa using hdel r0 h0'
  have hbr := cover_bracket htailm (fun r hr => (hdelm r hr).1) hmc
  have hpendmk : ∀ r ∈ recs.map (mark A P), r.cov = false →
      ¬ InGroup A P r.own ∧ r.mb ≤ mu r.own ∧ r.mb + r.h ≤ A r.own := by
    intro r hr hc; obtain ⟨r0, h0', rfl⟩ := List.mem_map.mp hr
    rw [mark_cov] at hc; simp only [Bool.or_eq_false_iff] at hc
    have hp := hpend r0 h0' hc.1
    refine ⟨?_, by simpa using hp.1, by simpa using hp.2⟩
    intro hg; have := hc.2; rw [mark_own] at hg; simp [hit, hc.1, hg] at this
  have hlen : ∀ y, ((recs.map (mark A P)).filter (fun r => decide (r.own = y))).length =
      (recs.filter (fun r => decide (r.own = y))).length := fun y => len_own_map_mark A P y recs
  unfold XS.step
  by_cases hfl : A P - sinkAmt A P t < m
  · simp only [if_pos hfl]
    have hdpos : 0 < sinkAmt A P t := by have := hfloor P; linarith
    have ht0 : 0 < t := lt_of_le_of_ne h0 (by rintro rfl; simp at hdpos)
    have hQ := hst ht0
    have hQ' := hlow Q hQ
    have hq1 : m - (A P - sinkAmt A P t) ≤ sit A P t Q - (A P - sinkAmt A P t) := by
      have := hfloor Q; linarith
    have hq2 : sit A P t Q - (A P - sinkAmt A P t) ≤ 2 := by have := hdz.2; rw [hP'] at this; linarith
    have hopp := opp_le_sit h0 hfloor hfl
    have hoppG : ¬ InGroup A P (opp P) := fun h => h.2 rfl
    set L := (recs.map (mark A P)).filter (fun r => !r.cov) with hL
    have hLm : ∀ r ∈ L, r ∈ recs.map (mark A P) ∧ r.cov = false := by
      intro r hr; simpa [hL] using hr
    have hLb : bsum L = bsum recs - bsum ((recs.map (mark A P)).filter (fun r => r.cov)) := by
      rw [← hbm, hbs]; ring
    have hLp : pendSum L = pendSum recs - dsum (recs.filter (hit A P)) := by
      rw [hL, pendSum_filter_pend, hpendm]
    have hLc : covSum L = 0 := covSum_filter_pend _
    have hLt : TailOK m 0 L := htailm.filter _ (fun r hr => (hdelm r hr).1)
    have hLlen : ∀ y, (L.filter (fun r => decide (r.own = y))).length ≤
        (recs.filter (fun r => decide (r.own = y))).length := by
      intro y; rw [← hlen y]; exact len_filter_le _ _ _
    refine ⟨hsum', ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;> dsimp only
    · linarith
    · intro x; by_cases hg : InGroup A P x
      · rw [hgrp x hg]
      · have := hlow x hg; have := hfloor x; linarith
    · linarith
    · linarith
    · linarith
    · push_cast; linarith
    · intro x; by_cases hg : InGroup A P x
      · simp only [hg, if_true, add_zero]; exact ⟨le_rfl, (hgrp x hg).symm.le⟩
      · simp only [hg, if_false]; rw [sit_rise t hg]; have := hmu x
        constructor <;> linarith
    · intro x; by_cases hg : InGroup A P x
      · simp [hg]
      · simp only [hg, if_false]; exact hrho' x hg
    · intro r hr
      rcases List.mem_append.mp hr with hr | hr
      · split_ifs at hr
        · simp at hr; subst hr
          refine ⟨hrise0, by linarith, ?_⟩; have := hfloor P; dsimp only; linarith
        · simp at hr
      · exact hrlm r (hLm r hr).1
    · intro y
      rw [ownF_append, hL, ownF_filter]
      by_cases hgy : InGroup A P y
      · simp only [hgy, if_true]; rw [ownF_marked_zero A P _ y hgy]
        have : opp P ≠ y := fun h => hgy.2 h.symm
        split_ifs <;> simp [ownF, this]
      · simp only [hgy, if_false]
        have h1 := hrdm y; have h2 := hrdown y
        have h3 : ownF Rec.rd y (if 2 < sinkAmt A P t then
            [⟨opp P, m - (A P - sinkAmt A P t), A P - m, m, false, rise A P t,
              sinkAmt A P t - rise A P t⟩] else []) ≤ if Rises A P y then sinkAmt A P t else 0 := by
          split_ifs with hK hR hR
          · by_cases hy : opp P = y
            · subst hy; simp [ownF, rise_eq, hR]
            · simp [ownF, hy, hd0]
          · by_cases hy : opp P = y
            · subst hy; simp [ownF, rise_eq, hR]
            · simp [ownF, hy]
          · simp [ownF, hd0]
          · simp [ownF]
        linarith
    · rw [pendF_append, hL, pendF_filter]
      have : pendF Rec.lv (if 2 < sinkAmt A P t then
          [⟨opp P, m - (A P - sinkAmt A P t), A P - m, m, false, rise A P t,
            sinkAmt A P t - rise A P t⟩] else []) ≤ sinkAmt A P t - rise A P t := by
        split_ifs
        · simp [pendF]
        · simp [pendF]; linarith
      linarith
    · by_cases hK : 2 < sinkAmt A P t
      · simp only [hK, if_true, List.singleton_append]
        refine ⟨by simp, ?_⟩
        have := hLt.shift (m - (A P - sinkAmt A P t))
        rwa [show m - (m - (A P - sinkAmt A P t)) = A P - sinkAmt A P t by ring] at this
      · simp only [hK, if_false, List.nil_append]; exact hLt.mono_m (by linarith)
    · intro r hr
      rcases List.mem_append.mp hr with hr | hr
      · split_ifs at hr
        · simp at hr; subst hr; constructor <;> linarith
        · simp at hr
      · exact hdelm r (hLm r hr).1
    · intro r hr hc
      rcases List.mem_append.mp hr with hr | hr
      · split_ifs at hr
        · simp at hr; subst hr
          simp only [hoppG, if_false]
          exact ⟨(hmu (opp P)).1, by linarith⟩
        · simp at hr
      · obtain ⟨hg, h1, h2⟩ := hpendmk r (hLm r hr).1 (hLm r hr).2
        simp only [hg, if_false]
        exact ⟨h1, le_trans h2 (hlow _ hg)⟩
    · intro r hr hc
      rcases List.mem_append.mp hr with hr | hr
      · split_ifs at hr
        · simp at hr; subst hr; simp at hc
        · simp at hr
      · simp [(hLm r hr).2] at hc
    · exact le_rfl
    · -- the potential
      have hb0 : 0 ≤ max 0 (Hm - 2) := le_max_left _ _
      have e1 : max 0 (sit A P t Q - (A P - sinkAmt A P t) - 2) = 0 := max_eq_left (by linarith)
      have e2 : max 0 (2 - (sit A P t Q - (A P - sinkAmt A P t))) =
          2 - (sit A P t Q - (A P - sinkAmt A P t)) := max_eq_right (by linarith)
      unfold Psi at hpot ⊢; simp only at hpot ⊢
      rw [e1, e2]
      by_cases hK : 2 < sinkAmt A P t
      · simp only [hK, if_true, List.singleton_append, covSum, bsum_cons, List.filter_cons,
          Bool.false_eq_true, if_false]
        simp only [covSum] at hLc hbr hcovm hpot; rw [hLc]
        have hbon : bonus ⟨opp P, m - (A P - sinkAmt A P t), A P - m, m, false, rise A P t,
            sinkAmt A P t - rise A P t⟩ = max 0 (2 - (A P - m)) := rfl
        rw [hbon, hLb]; push_cast; linarith
      · simp only [hK, if_false, List.nil_append]
        simp only [covSum] at hLc hbr hcovm hpot ⊢; rw [hLc, hLb]
        have : m - (A P - sinkAmt A P t) ≤ max 0 (2 - (A P - m)) :=
          le_trans (by linarith) (le_max_right _ _)
        push_cast; linarith
    · by_cases hK : 2 < sinkAmt A P t
      · simp only [hK, if_true, List.singleton_append, pendSum, List.filter_cons, Bool.not_false,
          if_true, dsum_cons]
        simp only [pendSum] at hLp hsplit; rw [hLp]; linarith
      · simp only [hK, if_false, List.nil_append]; rw [hLp]; linarith
    · intro x
      have h1 := hcnt x; have h2 := hLlen (opp x)
      by_cases hK : 2 < sinkAmt A P t
      · simp only [hK, if_true, List.singleton_append, List.filter_cons]
        by_cases hx : opp P = opp x
        · have := opp_inj hx; subst this
          simp only [decide_true, if_true, List.length_cons]
          simp at h1; split_ifs <;> omega
        · simp only [hx, decide_false, Bool.false_eq_true, if_false]
          split_ifs at h1 ⊢ <;> omega
      · simp only [hK, if_false, List.nil_append]
        split_ifs at h1 ⊢ <;> omega
  · simp only [if_neg hfl]
    push Not at hfl
    refine ⟨hsum', ?_, ?_, hB, hB0, ?_, ?_, ?_, ?_, hrlm, ?_, ?_, htailm, hdelm, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;>
      dsimp only
    · linarith
    · intro x; by_cases hg : InGroup A P x
      · rw [hgrp x hg]; exact hfl
      · exact le_trans (hfloor x) (hlow x hg)
    · linarith
    · push_cast; linarith
    · intro x; by_cases hg : InGroup A P x
      · simp only [hg, if_true, add_zero]; exact ⟨le_rfl, by rw [hgrp x hg]; exact hfl⟩
      · simp only [hg, if_false]; rw [sit_rise t hg]; have := hmu x
        constructor <;> linarith
    · intro x; by_cases hg : InGroup A P x
      · simp [hg]
      · simp only [hg, if_false]; exact hrho' x hg
    · intro y
      by_cases hgy : InGroup A P y
      · simp only [hgy, if_true]; rw [ownF_marked_zero A P _ y hgy]
      · simp only [hgy, if_false]
        have h1 := hrdm y; have h2 := hrdown y
        have : 0 ≤ (if Rises A P y then sinkAmt A P t else 0) := by split_ifs <;> linarith
        linarith
    · linarith
    · intro r hr hc
      obtain ⟨hg, h1, h2⟩ := hpendmk r hr hc
      simp only [hg, if_false]
      exact ⟨h1, le_trans h2 (hlow _ hg)⟩
    · intro r hr hc
      have := hmc r hr hc; have := le_max_left Hm (sit A P t Q - m); linarith
    · exact le_max_right _ _
    · unfold Psi at hpot ⊢; simp only at hpot ⊢
      rw [hcovm, hbm]
      have hq0 : 0 ≤ sit A P t Q - m := by
        by_cases hg : InGroup A P Q
        · rw [hgrp Q hg]; linarith
        · have := hlow Q hg; have := hfloor Q; linarith
      have := pot_nonfl Hm (A P - m) (sit A P t Q - m) hhm hq0 (by rw [hP'] at hdz; linarith [hdz.2])
      push_cast; linarith
    · rw [hpendm]; linarith
    · intro x; have h1 := hcnt x; rw [hlen (opp x)]; split_ifs at h1 ⊢ <;> omega


/-! ### Along the tour -/

def XS.run (e : XS) (l : List Move) : XS := l.foldl XS.step e

@[simp] lemma xrun_nil (e : XS) : e.run [] = e := rfl
@[simp] lemma xrun_cons (e : XS) (mv : Move) (l : List Move) : e.run (mv :: l) = (e.step mv).run l := rfl

lemma step_st (e : XS) (mv : Move) : (⟨(e.step mv).alt, (e.step mv).pos⟩ : St) = St.next ⟨e.alt, e.pos⟩ mv := by
  unfold XS.step St.next; split_ifs <;> rfl

lemma xinv_run : ∀ (l : List Move) (e : XS), XInv e → MovesOK ⟨e.alt, e.pos⟩ l → Strict ⟨e.alt, e.pos⟩ l →
    XInv (e.run l) ∧ (⟨(e.run l).alt, (e.run l).pos⟩ : St) = run ⟨e.alt, e.pos⟩ l ∧
    (e.run l).T = e.T + time l ∧ (e.run l).M = e.M + l.length ∧
    ∀ x, (e.run l).vis x = e.vis x + (l.map Move.dest).count x
  | [], e, hI, _, _ => ⟨hI, rfl, by simp [time], by simp, by simp⟩
  | mv :: l, e, hI, hm, hs => by
    have h1 := xinv_step hI hm.1 hm.2.1 hs.1
    have hst := step_st e mv
    have hm' : MovesOK ⟨(e.step mv).alt, (e.step mv).pos⟩ l := by rw [hst]; exact hm.2.2
    have hs' : Strict ⟨(e.step mv).alt, (e.step mv).pos⟩ l := by rw [hst]; exact hs.2
    obtain ⟨hI', hS', hT', hM', hV'⟩ := xinv_run l _ h1 hm' hs'
    refine ⟨by simpa using hI', ?_, ?_, ?_, ?_⟩
    · rw [xrun_cons, hS', hst]; rfl
    · rw [xrun_cons, hT', time_cons]
      have : (e.step mv).T = e.T + mv.wait := by unfold XS.step; split <;> rfl
      rw [this]; ring
    · rw [xrun_cons, hM']
      have : (e.step mv).M = e.M + 1 := by unfold XS.step; split <;> rfl
      rw [this]; simp; ring
    · intro x; rw [xrun_cons, hV' x]
      have : (e.step mv).vis x = e.vis x + if x = mv.dest then 1 else 0 := by
        unfold XS.step; split <;> rfl
      rw [this, List.map_cons, List.count_cons]
      by_cases h : mv.dest = x
      · have h1 : (mv.dest == x) = true := beq_iff_eq.mpr h
        rw [if_pos h.symm, h1, if_pos rfl]; omega
      · have h1 : (mv.dest == x) = false := beq_false_of_ne h
        rw [if_neg (Ne.symm h), h1]; simp only [Bool.false_eq_true, ite_false]; omega

def xstart : XS :=
  { alt := init, pos := a1, m := 0, mu := fun _ => 0, rho := fun _ => 0, recs := [], Hm := 0, T := 0,
    R := 0, D := 0,
    Z := 0, B := 0, BnK := 0, Bcov := 0, M := 0, vis := fun x => if x = a1 then 1 else 0 }

lemma xinv_start : XInv xstart := by
  have ha1 : init a1 = 0 := by decide +kernel
  refine ⟨?_, ?_, init_nonneg, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, trivial, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;>
    simp [xstart, init_sum, ha1, Psi, covSum, pendSum, init_nonneg, ownF, pendF]
  all_goals first | (intros; exact le_rfl) | norm_num

/-- Group a sum over records by their opposite points. -/
lemma group_sum (f : Rec → ℚ) (g : Sq → ℚ) : ∀ l : List Rec, (∀ r ∈ l, f r ≤ g r.own) →
    (l.map f).sum ≤ ∑ x, ((l.filter (fun r => decide (r.own = x))).length : ℚ) * g x
  | [], _ => by simp
  | r :: l, h => by
    have ih := group_sum f g l (fun r' hr' => h r' (List.mem_cons_of_mem _ hr'))
    have hr := h r (List.mem_cons_self ..)
    have key : ∀ x, ((((r :: l).filter (fun r => decide (r.own = x))).length : ℕ) : ℚ) * g x =
        (if r.own = x then g x else 0) + ((l.filter (fun r => decide (r.own = x))).length : ℚ) * g x := by
      intro x; by_cases hx : r.own = x <;> simp [List.filter_cons, hx]; ring
    simp only [key, Finset.sum_add_distrib, Finset.sum_ite_eq, Finset.mem_univ, if_true,
      List.map_cons, List.sum_cons]
    linarith

/-- **Theorem 3, by length.** A strict tour of `M` moves lasts at most `67 M + 3431⅓` minutes. -/
theorem bound_length (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 67 * tour.length + 10294 / 3 := by
  obtain ⟨hI, hst, hT, hM, hV⟩ := xinv_run tour xstart xinv_start hL.1 hS
  set e := xstart.run tour
  have hT' : e.T = time tour := by rw [hT]; simp [xstart]
  have hM' : (e.M : ℚ) = tour.length := by rw [hM]; simp [xstart]
  have hpos : e.pos = h8 := by
    have := congrArg St.pos hst; simp only at this; rw [this]; exact run_pos _ _ _ hL.2.1
  have hvis : ∀ x, e.vis x = (visits tour).count x := by
    intro x; rw [hV x]; simp [xstart, visits, List.count_cons]; split_ifs <;> simp_all; omega
  -- the rescue argument: W ≥ 2B - 2M - 2
  have hpsi := psi_nonneg hI.tail (fun r hr => (hI.del r hr).1) hI.cov
  have hpot := hI.pot
  have hsplit := hI.split
  set W := dsum (e.recs.filter (fun r => !r.cov)) with hW
  have hWp : pendSum e.recs = W := rfl
  rw [hWp] at hsplit
  have hW0 : 0 ≤ W := by
    simp only [hW, dsum]
    exact List.sum_nonneg (by simp only [List.mem_map]; rintro _ ⟨r, hr, rfl⟩
                              exact (hI.del r (List.mem_of_mem_filter hr)).1)
  have hWB : 2 * e.B - 2 * e.M - 2 ≤ W := by linarith
  -- the loss of the knocked-out points: 12 ∑ (mu - m) ≥ W² + 2W
  have hpt : TailOK e.m 0 (e.recs.filter (fun r => !r.cov)) := hI.tail.filter _ (fun r hr => (hI.del r hr).1)
  have hq := hpt.quad le_rfl (fun r hr => hI.del r (List.mem_of_mem_filter hr))
  rw [← hW] at hq
  have hq' : W ^ 2 + 2 * W ≤ 4 * ((e.recs.filter (fun r => !r.cov)).map (fun r => r.mb - e.m)).sum := by
    have : ((0 : ℚ) + W) ^ 2 - 0 ^ 2 = W ^ 2 := by ring
    linarith
  have hcnt3 : ∀ y, ((e.recs.filter (fun r => !r.cov)).filter (fun r => decide (r.own = y))).length ≤ 3 := by
    intro y
    have h1 := len_filter_le (fun r => decide (r.own = y)) (fun r => !r.cov) e.recs
    have h2 := hI.cnt (opp y); rw [opp_opp, hvis] at h2
    have h3 : (visits tour).count (opp y) ≤ 3 := by
      by_cases hy : opp y = h8
      · rw [hy, hL.2.2.1]; norm_num
      · exact hL.2.2.2 _ hy
    omega
  have hgrp := group_sum (fun r => r.mb - e.m) (fun x => e.mu x - e.m) (e.recs.filter (fun r => !r.cov))
    (fun r hr => by
      have := (hI.pend r (List.mem_of_mem_filter hr) (by simpa using (List.mem_filter.mp hr).2)).1
      show r.mb - e.m ≤ e.mu r.own - e.m; linarith)
  have hloss : ((e.recs.filter (fun r => !r.cov)).map (fun r => r.mb - e.m)).sum ≤
      3 * ∑ x, (e.mu x - e.m) := by
    refine le_trans hgrp ?_
    rw [Finset.mul_sum]; apply Finset.sum_le_sum; intro x _
    have h1 : ((((e.recs.filter (fun r => !r.cov)).filter (fun r => decide (r.own = x))).length : ℕ) : ℚ) ≤ 3 := by
      exact_mod_cast hcnt3 x
    have h2 : 0 ≤ e.mu x - e.m := by linarith [(hI.mu x).1]
    nlinarith
  -- the rises of the knocked-out points are never paid back: W ≤ ∑ rho + (D - R)
  have hWr : W ≤ pendF Rec.rd e.recs + pendF Rec.lv e.recs := by
    rw [← pendF_add]
    have := pendF_le Rec.del (fun r => r.rd + r.lv) e.recs (fun r hr => (hI.rl r hr).2.2)
    exact this
  have hrdsum : pendF Rec.rd e.recs ≤ ∑ y, e.rho y := by
    rw [pendF_own]; exact Finset.sum_le_sum (fun y _ => hI.rdown y)
  have hlvl := hI.lvl
  -- the identity
  have hmu_le : ∑ x, (e.mu x + e.rho x) ≤ ∑ x, e.alt x := Finset.sum_le_sum (fun x _ => (hI.mu x).2)
  rw [Finset.sum_add_distrib] at hmu_le
  have hsum := hI.sum
  have hmsum : ∑ x, (e.mu x - e.m) = ∑ x, e.mu x - 64 * e.m := by
    rw [Finset.sum_sub_distrib]; simp
  have hk := hI.knight; rw [hpos] at hk
  have hk8 := hI.floor h8
  have hRD := hI.RD; have hZM := hI.ZM; have hB := hI.hB
  rw [← hT', ← hM']
  -- T ≤ 421 + 2M + 65B - 7W/6 - W²/12
  have hmain : e.T ≤ 421 + 2 * e.M + 65 * e.B - 7 / 6 * W - W ^ 2 / 12 := by nlinarith
  by_cases hu : 2 * e.B - 2 * e.M - 2 ≤ 0
  · nlinarith
  · push Not at hu
    have hW2 : (2 * e.B - 2 * e.M - 2) ^ 2 ≤ W ^ 2 := by nlinarith
    nlinarith [sq_nonneg (2 * e.B - 2 * e.M - 2 - 188)]

/-- **Theorem 3.** A strict tour that obeys the rules lasts at most 16,094⅓ minutes. -/
theorem bound (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 48283 / 3 := by
  have h1 := bound_length tour hL hS
  have h2 : (tour.length : ℚ) ≤ 189 := by exact_mod_cast length_le tour hL
  linarith

end KM5.B3

namespace KM5

/-- **Theorem 3.** A strict tour that obeys the rules lasts at most 16,094⅓ minutes. -/
theorem strict_bound3 (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 48283 / 3 := B3.bound tour hL hS

/-- **Theorem 3, by length.** A strict tour of `M` moves lasts at most `67 M + 3431⅓` minutes. -/
theorem strict_bound3_length (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 67 * tour.length + 10294 / 3 := B3.bound_length tour hL hS

end KM5
