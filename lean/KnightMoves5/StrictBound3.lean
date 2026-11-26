import KnightMoves5.StrictBound

/-!
# Theorem 3: every strict tour lasts at most 15,606⅚ minutes

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
  again and `B_nK` the drop of floor-lowering sits that sink at most 2 (`xinv_step`).
* **The climb to h8.** h8 starts at altitude 17 and only rises until it first sinks, so the knight
  must stand at altitude ≥ 17 at some sit (or end there). The climbing budget of that stretch can
  only rescue records made before it, whose floors are all `≤ 0`, so 15 of it is wasted
  (`h8s`, `covSum_le`). Hence `B + B_nK + B_cov ≤ 2M - 13` and `W ≥ 2B - 2M + 13`.
* **Unpaid rises.** The rise of an open record's opposite is still in `ρ`, so `W ≤ ∑ ρ + (D - R)`.

Together: `T ≤ 421 + 2M + 65B - 7W/6 - W²/12`, which is at most `67 M + 2943⅚`, and `M ≤ 189`
gives `15,606⅚`.

* **From a partial tour.** The invariant holds for any constants (`Par`), so it can be restarted at
  the state any prefix reaches. `prefix_bound` bounds every strict tour with that prefix by
  `ubF`, which is what the exhaustive search of Theorem 4 (`tools/prove_bound.cpp`) prunes with.
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
  /-- `0`: h8 has not sunk yet; `1`: it sank since the last floor-lowering sit; `2`: paid for. -/
  h8s : ℕ

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
      vis := fun x => e.vis x + if x = mv.dest then 1 else 0,
      h8s := if e.h8s = 2 then 2 else if e.h8s = 1 ∨ InGroup e.alt e.pos h8 then 2 else 0 }
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
      vis := fun x => e.vis x + if x = mv.dest then 1 else 0,
      h8s := if e.h8s = 2 then 2 else if e.h8s = 1 ∨ InGroup e.alt e.pos h8 then 1 else 0 }

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

/-- Everything covered fits under the floor's total drop: the records' floors are all `≤ 0`. -/
lemma covSum_le {m μ : ℚ} {l : List Rec} (ht : TailOK m 0 l) (hd : ∀ r ∈ l, 0 ≤ r.del)
    (hmb : ∀ r ∈ l, r.mb ≤ μ) (hm : m ≤ μ) : covSum l ≤ μ - m := by
  by_cases hex : ∃ r ∈ l, r.cov = true
  · obtain ⟨r, hr, -, hle⟩ := ht.exists_cover (fun r => r.cov) hd hex
    have := hmb r hr; simp only [covSum] at hle ⊢; linarith
  · push Not at hex
    have : l.filter (fun r => r.cov) = [] := List.filter_eq_nil_iff.mpr (fun r hr => by simpa using hex r hr)
    simp only [covSum, this]; simp; linarith

lemma psi_ge (e : XS) : e.Hm - 2 - covSum e.recs ≤ Psi e := by
  unfold Psi
  have := le_max_right 0 (e.Hm - 2); have := le_max_left 0 (2 - (e.alt e.pos - e.m))
  have := bsum_nonneg e.recs; linarith

/-- The constants of the invariant. Theorem 3 uses `pstart`; `prefix_bound` restarts the invariant
from any partial tour with constants read off its state. The climb to h8 wastes `a8 - 2 - μ`. -/
structure Par where
  c : ℚ
  cz : ℚ
  cb : ℚ
  b0 : ℚ
  a8 : ℚ
  μ : ℚ

structure XInv (p : Par) (e : XS) : Prop where
  sum : ∑ x, e.alt x = 421 - e.T + e.R
  knight : e.alt e.pos = e.Z - e.D
  floor : ∀ x, e.m ≤ e.alt x
  hB : e.B = -e.m
  B0 : 0 ≤ e.B
  RD : e.R ≤ e.D
  ZM : e.Z ≤ 2 * e.M + p.cz
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
  pot : e.B + e.BnK + e.Bcov + Psi e + (if e.h8s = 2 then p.a8 - 2 - p.μ else 0) ≤ 2 * e.M + p.c
  h8a : e.h8s = 0 → p.a8 ≤ e.alt h8
  h8b : e.h8s = 1 → p.a8 - e.m ≤ e.Hm
  h8c : e.h8s ≤ 2
  mb0 : ∀ r ∈ e.recs, r.mb ≤ p.μ
  split : e.B = e.BnK + e.Bcov + pendSum e.recs
  cnt : ∀ x, (e.recs.filter (fun r => decide (r.own = opp x))).length +
    (if x = e.pos then 1 else 0) ≤ e.vis x
  mμ : e.m ≤ p.μ
  BM : e.B ≤ 2 * e.M + p.cb
  Blo : p.b0 ≤ e.B

lemma opp_opp (x : Sq) : opp (opp x) = x := by simp [opp]

lemma opp_inj {x y : Sq} (h : opp x = opp y) : x = y := by
  rw [← opp_opp x, h, opp_opp]

lemma hit_iff {A : Alt} {P : Sq} {r : Rec} : hit A P r = true ↔ r.cov = false ∧ InGroup A P r.own := by
  simp [hit]

/-- The covered records after a sit (old ones and the newly hit ones) still fit. -/
lemma marked_cov {p : Par} {e : XS} (hI : XInv p e) (r : Rec) (hr : r ∈ e.recs.map (mark e.alt e.pos))
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

lemma xinv_step {p : Par} {e : XS} {mv : Move} (hI : XInv p e) (h0 : 0 ≤ mv.wait)
    (hj : JumpOK (sit e.alt e.pos mv.wait) e.pos mv.dest)
    (hst : 0 < mv.wait → ¬ InGroup e.alt e.pos mv.dest) : XInv p (e.step mv) := by
  have hmc := marked_cov hI
  obtain ⟨A, P, m, mu, rho, recs, Hm, T, R, D, Z, B, BnK, Bcov, M, vis, h8s⟩ := e
  obtain ⟨t, Q⟩ := mv
  obtain ⟨hsum, hk, hfloor, hB, hB0, hRD, hZM, hmu, hrho0, hrl, hrdown, hlvl, htail, hdel, hpend, hcov,
    hhm, hpot, hh8a, hh8b, hh8c, hmb0, hsplit, hcnt, hmμ, hBM, hBlo⟩ := hI
  simp only at h0 hj hst hsum hk hfloor hB hB0 hRD hZM hmu hrho0 hrl hrdown hlvl htail hdel hpend hcov hhm hpot hh8a hh8b hh8c hmb0 hsplit hcnt hmc hmμ hBM hBlo
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
  have hmbm : ∀ r ∈ recs.map (mark A P), r.mb ≤ p.μ := by
    intro r hr; obtain ⟨r0, h0', rfl⟩ := List.mem_map.mp hr; simpa using hmb0 r0 h0'
  have hcovle : covSum (recs.map (mark A P)) ≤ p.μ - m :=
    covSum_le htailm (fun r hr => (hdelm r hr).1) hmbm hmμ
  -- the climb to h8 is paid for once
  have hH8 : (h8s = 1 ∨ (h8s = 0 ∧ InGroup A P h8)) → p.a8 - m ≤ Hm := by
    rintro (h | ⟨h, hg⟩)
    · exact hh8b h
    · have := hh8a h; rw [hg.1] at this; linarith
  have hτ : (if (if h8s = 2 then 2 else if h8s = 1 ∨ InGroup A P h8 then 2 else 0) = 2 then p.a8 - 2 - p.μ
      else 0) ≤ (if h8s = 2 then p.a8 - 2 - p.μ else 0) -
        (covSum (recs.map (mark A P)) - max 0 (Hm - 2) - bsum ((recs.map (mark A P)).filter (fun r => r.cov))) := by
    have hb := bsum_nonneg ((recs.map (mark A P)).filter (fun r => r.cov))
    have hmx := le_max_right 0 (Hm - 2)
    by_cases h2 : h8s = 2
    · simp only [h2, if_true]; linarith
    · by_cases h1 : h8s = 1 ∨ InGroup A P h8
      · have hH : p.a8 - m ≤ Hm := hH8 (by
          by_cases h1' : h8s = 1
          · exact Or.inl h1'
          · exact Or.inr ⟨by omega, h1.resolve_left h1'⟩)
        simp only [h2, h1, if_true, if_false]; linarith
      · simp only [h2, h1, if_false]; norm_num; linarith
  have hτn : (if (if h8s = 2 then 2 else if h8s = 1 ∨ InGroup A P h8 then 1 else 0) = 2 then p.a8 - 2 - p.μ
      else 0) = (if h8s = 2 then p.a8 - 2 - p.μ else 0) := by
    by_cases h2 : h8s = 2
    · simp [h2]
    · by_cases h1 : h8s = 1 ∨ InGroup A P h8 <;> simp [h2, h1]
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
    refine ⟨hsum', ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_,
      ?_, ?_, ?_⟩ <;> dsimp only
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
        simp only [covSum] at hLc hbr hcovm hpot hτ; rw [hLc]
        have hbon : bonus ⟨opp P, m - (A P - sinkAmt A P t), A P - m, m, false, rise A P t,
            sinkAmt A P t - rise A P t⟩ = max 0 (2 - (A P - m)) := rfl
        rw [hbon, hLb]; push_cast; linarith
      · simp only [hK, if_false, List.nil_append]
        simp only [covSum] at hLc hbr hcovm hpot hτ ⊢; rw [hLc, hLb]
        have : m - (A P - sinkAmt A P t) ≤ max 0 (2 - (A P - m)) :=
          le_trans (by linarith) (le_max_right _ _)
        push_cast; linarith
    · intro h; have : h8s = 0 ∧ ¬ InGroup A P h8 := by
        by_cases h2 : h8s = 2
        · simp [h2] at h
        · by_cases h1 : h8s = 1 ∨ InGroup A P h8
          · simp [h2, h1] at h
          · push Not at h1; exact ⟨by omega, h1.2⟩
      exact le_trans (hh8a this.1) (hlow h8 this.2)
    · intro h; split_ifs at h <;> simp at h
    · split_ifs <;> omega
    · intro r hr
      rcases List.mem_append.mp hr with hr | hr
      · split_ifs at hr
        · simp at hr; subst hr; dsimp only; linarith
        · simp at hr
      · exact hmbm r (hLm r hr).1
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
    · linarith
    · push_cast; linarith
    · linarith
  · simp only [if_neg hfl]
    push Not at hfl
    refine ⟨hsum', ?_, ?_, hB, hB0, ?_, ?_, ?_, ?_, hrlm, ?_, ?_, htailm, hdelm, ?_, ?_, ?_, ?_, ?_, ?_,
      ?_, hmbm, ?_, ?_, hmμ, ?_, hBlo⟩ <;> dsimp only
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
      rw [hτn]; push_cast; linarith
    · intro h; have : h8s = 0 ∧ ¬ InGroup A P h8 := by
        by_cases h2 : h8s = 2
        · simp [h2] at h
        · by_cases h1 : h8s = 1 ∨ InGroup A P h8
          · simp [h2, h1] at h
          · push Not at h1; exact ⟨by omega, h1.2⟩
      exact le_trans (hh8a this.1) (hlow h8 this.2)
    · intro h
      have h1 : h8s = 1 ∨ (h8s = 0 ∧ InGroup A P h8) := by
        by_cases h2 : h8s = 2
        · simp [h2] at h
        · by_cases h1 : h8s = 1 ∨ InGroup A P h8
          · rcases h1 with h1 | h1
            · exact Or.inl h1
            · by_cases h1' : h8s = 1
              · exact Or.inl h1'
              · exact Or.inr ⟨by omega, h1⟩
          · simp [h2, h1] at h
      have := hH8 h1; have := le_max_left Hm (sit A P t Q - m); linarith
    · split_ifs <;> omega
    · rw [hpendm]; linarith
    · intro x; have h1 := hcnt x; rw [hlen (opp x)]; split_ifs at h1 ⊢ <;> omega
    · push_cast; linarith


/-! ### Along the tour -/

def XS.run (e : XS) (l : List Move) : XS := l.foldl XS.step e

@[simp] lemma xrun_nil (e : XS) : e.run [] = e := rfl
@[simp] lemma xrun_cons (e : XS) (mv : Move) (l : List Move) : e.run (mv :: l) = (e.step mv).run l := rfl

lemma step_st (e : XS) (mv : Move) : (⟨(e.step mv).alt, (e.step mv).pos⟩ : St) = St.next ⟨e.alt, e.pos⟩ mv := by
  unfold XS.step St.next; split_ifs <;> rfl

lemma xinv_run {p : Par} : ∀ (l : List Move) (e : XS), XInv p e → MovesOK ⟨e.alt, e.pos⟩ l →
    Strict ⟨e.alt, e.pos⟩ l → XInv p (e.run l) ∧ (⟨(e.run l).alt, (e.run l).pos⟩ : St) = run ⟨e.alt, e.pos⟩ l ∧
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
    Z := 0, B := 0, BnK := 0, Bcov := 0, M := 0, vis := fun x => if x = a1 then 1 else 0, h8s := 0 }

/-- The constants for a whole tour: the climb to h8 (altitude 17, floors at most 0) wastes 15. -/
def pstart : Par := ⟨2, 0, 0, 0, 17, 0⟩

lemma xinv_start : XInv pstart xstart := by
  have ha1 : init a1 = 0 := by decide +kernel
  have hh8 : (17 : ℚ) ≤ init h8 := by decide +kernel
  refine ⟨?_, ?_, init_nonneg, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, trivial, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_,
    ?_, ?_, ?_⟩ <;>
    simp [xstart, pstart, init_sum, ha1, Psi, covSum, pendSum, init_nonneg, ownF, pendF, hh8]
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

/-- The end of the argument, for any constants: a run that stands on h8, with every point visited
at most 3 times, has `T ≤ 421 + Z + 65B - 7W/6 - W²/12` for some `W ≥ 0` with
`W ≥ 2B - 2M - c + (a8 - 2 - μ)`. -/
lemma end_bound {p : Par} {e : XS} (hI : XInv p e) (hpos : e.pos = h8) (hv : ∀ x, e.vis x ≤ 3) :
    ∃ W : ℚ, 0 ≤ W ∧ 2 * e.B - 2 * e.M - p.c + (p.a8 - 2 - p.μ) ≤ W ∧
      e.T ≤ 421 + e.Z + 65 * e.B - 7 / 6 * W - W ^ 2 / 12 := by
  -- the rescue argument
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
  -- the climb to h8 wastes a8 - 2 - μ
  have hcl := covSum_le hI.tail (fun r hr => (hI.del r hr).1) hI.mb0 hI.mμ
  have hpg := psi_ge e
  have h15 : p.a8 - 2 - p.μ ≤ Psi e + (if e.h8s = 2 then p.a8 - 2 - p.μ else 0) := by
    rcases (by have := hI.h8c; omega : e.h8s = 0 ∨ e.h8s = 1 ∨ e.h8s = 2) with h | h | h
    · have h1 := hI.h8a h; have h2 := hI.hm; rw [hpos] at h2
      simp only [h]; norm_num; linarith
    · have h1 := hI.h8b h; simp only [h]; norm_num; linarith
    · simp only [h, if_true]; linarith
  have hWB : 2 * e.B - 2 * e.M - p.c + (p.a8 - 2 - p.μ) ≤ W := by linarith
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
    have h2 := hI.cnt (opp y); rw [opp_opp] at h2
    have h3 := hv (opp y)
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
  have hRD := hI.RD; have hB := hI.hB
  exact ⟨W, hW0, hWB, by nlinarith⟩

/-- **Theorem 3, by length.** A strict tour of `M` moves lasts at most `67 M + 2943⅚` minutes. -/
theorem bound_length (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 67 * tour.length + 17663 / 6 := by
  obtain ⟨hI, hst, hT, hM, hV⟩ := xinv_run tour xstart xinv_start hL.1 hS
  set e := xstart.run tour
  have hT' : e.T = time tour := by rw [hT]; simp [xstart]
  have hM' : (e.M : ℚ) = tour.length := by rw [hM]; simp [xstart]
  have hpos : e.pos = h8 := by
    have := congrArg St.pos hst; simp only at this; rw [this]; exact run_pos _ _ _ hL.2.1
  have hvis : ∀ x, e.vis x = (visits tour).count x := by
    intro x; rw [hV x]; simp [xstart, visits, List.count_cons]; split_ifs <;> simp_all; omega
  have hv : ∀ x, e.vis x ≤ 3 := by
    intro x; rw [hvis x]
    by_cases hx : x = h8
    · rw [hx, hL.2.2.1]; norm_num
    · exact hL.2.2.2 x hx
  obtain ⟨W, hW0, hWB, hmain⟩ := end_bound hI hpos hv
  have hZM := hI.ZM; have hB0 := hI.B0
  simp only [pstart] at hWB hZM
  rw [← hT', ← hM']
  by_cases hu : 2 * e.B - 2 * e.M + 13 ≤ 0
  · nlinarith
  · push Not at hu
    have hW2 : (2 * e.B - 2 * e.M + 13) ^ 2 ≤ W ^ 2 := by nlinarith
    nlinarith [sq_nonneg (2 * e.B - 2 * e.M + 13 - 188)]

/-- **Theorem 3.** A strict tour that obeys the rules lasts at most 15,606⅚ minutes. -/
theorem bound (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 93641 / 6 := by
  have h1 := bound_length tour hL hS
  have h2 : (tour.length : ℚ) ≤ 189 := by exact_mod_cast length_le tour hL
  linarith

/-! ### The bound from a partial tour

`ubF e` bounds every strict tour that starts with the moves that led to `e`. The invariant is
restarted at `e` with constants read off `e` (`parOf`): the rescue budget already spent, the jumps
so far, the floor, and the climb to h8 still owed. At most `dMoves e` moves remain (the unused
visits), each adds at most 2 to the budget, the jumps and the floor drop, and the bound is the
largest value the end of Theorem 3's argument allows for the final floor drop. -/

/-- The largest floor among the records, or the current floor. -/
def recMax (m : ℚ) : List Rec → ℚ
  | [] => m
  | r :: l => max r.mb (recMax m l)

lemma le_recMax (m : ℚ) : ∀ l : List Rec, m ≤ recMax m l
  | [] => le_rfl
  | r :: l => le_trans (le_recMax m l) (le_max_right _ _)

lemma mb_le_recMax (m : ℚ) : ∀ l : List Rec, ∀ r ∈ l, r.mb ≤ recMax m l
  | [], _, h => by simp at h
  | r :: l, r', h => by
    rcases List.mem_cons.mp h with rfl | h
    · exact le_max_left _ _
    · exact le_trans (mb_le_recMax m l r' h) (le_max_right _ _)

/-- At most this many moves remain: h8 once, and every other point up to 3 visits. -/
def dMoves (e : XS) : ℚ := 1 + ∑ x ∈ Finset.univ.erase h8, (3 - (e.vis x : ℚ))

/-- The altitude the knight must still reach before h8 sinks (`h8s = 2`: nothing owed). -/
def a8of (e : XS) : ℚ :=
  if e.h8s = 0 then e.alt h8 else if e.h8s = 1 then e.m + e.Hm else recMax e.m e.recs + 2

def usedB (e : XS) : ℚ := e.B + e.BnK + e.Bcov + Psi e

def parOf (e : XS) : Par := ⟨usedB e - 2 * e.M, e.Z - 2 * e.M, e.B - 2 * e.M, e.B, a8of e, recMax e.m e.recs⟩

/-- The budget left for the final floor drop. -/
def Kof (e : XS) : ℚ := usedB e + 2 * dMoves e - (a8of e - 2 - recMax e.m e.recs)

/-- Theorem 3's last line as a function of the final floor drop `x`. -/
def gB (K x : ℚ) : ℚ := 65 * x - 7 / 6 * max 0 (2 * x - K) - (max 0 (2 * x - K)) ^ 2 / 12

/-- **The prefix bound.** -/
def ubF (e : XS) : ℚ :=
  421 + e.Z + 2 * dMoves e + gB (Kof e) (min (max ((Kof e + 188) / 2) e.B) (e.B + 2 * dMoves e))

lemma xinv_restart {p : Par} {e : XS} (hI : XInv p e) : XInv (parOf e) e := by
  have hτ : (if e.h8s = 2 then a8of e - 2 - recMax e.m e.recs else 0) = 0 := by
    unfold a8of; split_ifs <;> simp_all
  exact { hI with
    ZM := by simp only [parOf]; linarith
    pot := by simp only [parOf]; rw [hτ]; unfold usedB; linarith
    h8a := fun h => by simp [parOf, a8of, h]
    h8b := fun h => by simp [parOf, a8of, h]
    mb0 := fun r hr => mb_le_recMax _ _ r hr
    mμ := le_recMax _ _
    BM := by simp only [parOf]; linarith
    Blo := le_rfl }

/-- The largest value of Theorem 3's last line over the possible final floor drops. -/
lemma gB_max {K B D x W : ℚ} (hD : 0 ≤ D) (hx1 : B ≤ x) (hx2 : x ≤ B + 2 * D) (hW0 : 0 ≤ W)
    (hW : 2 * x - K ≤ W) :
    65 * x - 7 / 6 * W - W ^ 2 / 12 ≤ gB K (min (max ((K + 188) / 2) B) (B + 2 * D)) := by
  unfold gB
  rcases le_or_gt ((K + 188) / 2) B with h1 | h1
  · -- the budget is already exceeded: the best final drop is the current one
    rw [max_eq_right h1, min_eq_left (by linarith)]
    rw [max_eq_right (by linarith : (0 : ℚ) ≤ 2 * B - K)]
    nlinarith [mul_nonneg (by linarith : (0 : ℚ) ≤ W - (2 * x - K)) (by linarith : (0 : ℚ) ≤ W + (2 * x - K) + 14),
      mul_nonneg (by linarith : (0 : ℚ) ≤ x - B) (by linarith : (0 : ℚ) ≤ 2 * B - K - 188)]
  · rw [max_eq_left h1.le]
    rcases le_or_gt ((K + 188) / 2) (B + 2 * D) with h2 | h2
    · -- the unconstrained best: 2x - K = 188
      rw [min_eq_left h2, max_eq_right (by linarith : (0 : ℚ) ≤ 2 * ((K + 188) / 2) - K)]
      nlinarith [sq_nonneg (W - 188)]
    · rw [min_eq_right h2.le]
      rcases le_or_gt (2 * (B + 2 * D) - K) 0 with h3 | h3
      · rw [max_eq_left h3]; nlinarith
      · rw [max_eq_right h3.le]
        rcases le_or_gt (2 * x - K) 0 with h4 | h4
        · nlinarith [mul_nonneg h3.le (by linarith : (0 : ℚ) ≤ 376 - (2 * (B + 2 * D) - K))]
        · nlinarith [mul_nonneg (by linarith : (0 : ℚ) ≤ W - (2 * x - K)) (by linarith : (0 : ℚ) ≤ W + (2 * x - K) + 14),
            mul_nonneg (by linarith : (0 : ℚ) ≤ 2 * (B + 2 * D) - K - (2 * x - K))
              (by linarith : (0 : ℚ) ≤ 376 - (2 * (B + 2 * D) - K) - (2 * x - K))]

/-- **Prefix bound.** Every strict tour that starts with `pre` lasts at most `ubF (xstart.run pre)`. -/
theorem prefix_bound (pre suf : List Move) (hL : Legal (pre ++ suf)) (hS : Strict start (pre ++ suf)) :
    time (pre ++ suf) ≤ ubF (xstart.run pre) := by
  have hm := hL.1; rw [movesOK_append] at hm
  have hs := hS; rw [strict_append] at hs
  obtain ⟨hI, hst, hT, hM, hV⟩ := xinv_run pre xstart xinv_start hm.1 hs.1
  set e := xstart.run pre with he
  have hst0 : (⟨e.alt, e.pos⟩ : St) = run start pre := hst
  have hm2 : MovesOK ⟨e.alt, e.pos⟩ suf := by rw [hst0]; exact hm.2
  have hs2 : Strict ⟨e.alt, e.pos⟩ suf := by rw [hst0]; exact hs.2
  obtain ⟨hI', hst', hT', hM', hV'⟩ := xinv_run suf e (xinv_restart hI) hm2 hs2
  set e' := e.run suf with he'
  have hpos : e'.pos = h8 := by
    have := congrArg St.pos hst'; simp only at this; rw [this, hst0, ← run_append]
    exact run_pos _ _ _ hL.2.1
  have hvis : ∀ x, e'.vis x = (visits (pre ++ suf)).count x := by
    intro x; rw [hV' x, hV x]; simp [xstart, visits, List.count_cons]; split_ifs <;> simp_all <;> omega
  have hv : ∀ x, e'.vis x ≤ 3 := by
    intro x; rw [hvis x]
    by_cases hx : x = h8
    · rw [hx, hL.2.2.1]; norm_num
    · exact hL.2.2.2 x hx
  obtain ⟨W, hW0, hWB, hmain⟩ := end_bound hI' hpos hv
  -- at most dMoves e moves remain
  have hcount : ∀ x, e.vis x + (suf.map Move.dest).count x ≤ 3 := fun x => by rw [← hV' x]; exact hv x
  have h8c : e.vis h8 + (suf.map Move.dest).count h8 ≤ 1 := by
    rw [← hV' h8, hvis h8, hL.2.2.1]
  have hsumc : ∑ x : Sq, ((suf.map Move.dest).count x : ℚ) = suf.length := by
    have := Multiset.sum_count_eq_card (s := (Finset.univ : Finset Sq))
      (m := ((suf.map Move.dest : List Sq) : Multiset Sq)) (fun a _ => Finset.mem_univ a)
    exact_mod_cast (by simpa using this)
  have hsplit : ∑ x : Sq, ((suf.map Move.dest).count x : ℚ) = ((suf.map Move.dest).count h8 : ℚ) +
      ∑ x ∈ Finset.univ.erase h8, ((suf.map Move.dest).count x : ℚ) :=
    (Finset.add_sum_erase Finset.univ (fun x => ((suf.map Move.dest).count x : ℚ)) (Finset.mem_univ h8)).symm
  have hle : ∑ x ∈ Finset.univ.erase h8, ((suf.map Move.dest).count x : ℚ) ≤
      ∑ x ∈ Finset.univ.erase h8, (3 - (e.vis x : ℚ)) := by
    apply Finset.sum_le_sum; intro x _
    have : ((e.vis x : ℕ) : ℚ) + ((suf.map Move.dest).count x : ℚ) ≤ 3 := by exact_mod_cast hcount x
    linarith
  have hle0 : ∑ x ∈ Finset.univ.erase h8, (0 : ℚ) ≤ ∑ x ∈ Finset.univ.erase h8, (3 - (e.vis x : ℚ)) := by
    apply Finset.sum_le_sum; intro x _
    have : ((e.vis x : ℕ) : ℚ) + ((suf.map Move.dest).count x : ℚ) ≤ 3 := by exact_mod_cast hcount x
    have : (0 : ℚ) ≤ ((suf.map Move.dest).count x : ℚ) := by positivity
    linarith
  have h8c' : ((suf.map Move.dest).count h8 : ℚ) ≤ 1 := by
    have : ((e.vis h8 : ℕ) : ℚ) + ((suf.map Move.dest).count h8 : ℚ) ≤ 1 := by exact_mod_cast h8c
    have : (0 : ℚ) ≤ ((e.vis h8 : ℕ) : ℚ) := by positivity
    linarith
  simp only [Finset.sum_const_zero] at hle0
  have hlen : (suf.length : ℚ) ≤ dMoves e := by unfold dMoves; linarith
  have hD0 : 0 ≤ dMoves e := by unfold dMoves; linarith
  -- the end of the argument with the restarted constants
  have hZM := hI'.ZM; have hBM := hI'.BM; have hBlo := hI'.Blo
  simp only [parOf] at hWB hZM hBM hBlo
  have hM'' : (e'.M : ℚ) = e.M + suf.length := by rw [hM']; push_cast; ring
  have hT'' : e'.T = time (pre ++ suf) := by
    rw [hT', hT]; simp [xstart, time, List.map_append, List.sum_append]
  rw [← hT'']
  have hx2 : e'.B ≤ e.B + 2 * dMoves e := by linarith
  have hW' : 2 * e'.B - Kof e ≤ W := by unfold Kof; linarith
  have := gB_max hD0 hBlo hx2 hW0 hW'
  unfold ubF; linarith

end KM5.B3

namespace KM5

/-- **Theorem 3.** A strict tour that obeys the rules lasts at most 15,606⅚ minutes. -/
theorem strict_bound3 (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 93641 / 6 := B3.bound tour hL hS

/-- **Theorem 3, by length.** A strict tour of `M` moves lasts at most `67 M + 2943⅚` minutes. -/
theorem strict_bound3_length (tour : List Move) (hL : Legal tour) (hS : Strict start tour) :
    time tour ≤ 67 * tour.length + 17663 / 6 := B3.bound_length tour hL hS

/-- **Theorem 3 from a partial tour.** Every strict tour that starts with the moves `pre` lasts at
most `B3.ubF` of the state they reach (`tools/prefix_bound.py`, `tools/prove_bound.cpp`). -/
theorem strict_prefix_bound (pre suf : List Move) (hL : Legal (pre ++ suf))
    (hS : Strict start (pre ++ suf)) : time (pre ++ suf) ≤ B3.ubF (B3.xstart.run pre) :=
  B3.prefix_bound pre suf hL hS

end KM5
