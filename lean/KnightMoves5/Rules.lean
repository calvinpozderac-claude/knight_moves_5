import Mathlib

/-!
# Knight Moves 5: the rules

A faithful model of the puzzle, over exact rationals.

* The board is `Fin 8 × Fin 8` (file, rank), 0-indexed: `a1 = (0, 0)`, `h8 = (7, 7)`.
* A move `⟨t, Q⟩` waits `t` minutes on the knight's square `P`, then jumps to `Q`.
* While the knight waits on `P` (altitude `k`), every point at altitude `k` except `P`'s opposite
  sinks `t / n`, where `n` is the number of those points.  The opposite point rises `t / n`, unless
  it is also at altitude `k`, in which case it stays put and is not counted in `n`.
* A jump changes (file, rank, altitude) by a permutation of `(0, ±1, ±2)`.
* The tour ends on its first arrival at `h8`; every other point is visited at most 3 times, the
  start on `a1` included.
* **Necessity** (the rule as worded): if a move has `t > 0`, some other value `t'` of that wait must
  make some move of the tour illegal.
-/

namespace KM5

/-- A lattice point: (file, rank), both 0-indexed. -/
abbrev Sq := Fin 8 × Fin 8

def a1 : Sq := (0, 0)
def h8 : Sq := (7, 7)

/-- The diametrically opposite point. -/
def opp (p : Sq) : Sq := (Fin.rev p.1, Fin.rev p.2)

/-- Altitudes of all 64 points. -/
abbrev Alt := Sq → ℚ

/-- The puzzle's board: `initTable r f` is the altitude at rank `r`, file `f` (rank 1 first). -/
def initTable : Fin 8 → Fin 8 → ℚ := ![
  ![0, 2, 4, 3, 5, 6, 2, 4],
  ![1, 2, 0, 1, 2, 5, 7, 6],
  ![0, 3, 1, 4, 2, 7, 10, 7],
  ![2, 6, 4, 2, 5, 9, 8, 11],
  ![4, 10, 7, 9, 6, 8, 7, 9],
  ![4, 7, 5, 8, 8, 6, 13, 10],
  ![7, 9, 11, 9, 10, 12, 14, 12],
  ![9, 8, 10, 12, 11, 8, 10, 17]]

def init : Alt := fun p => initTable p.2 p.1

/-- `x` sinks while the knight waits on `P`: same altitude as `P`, and not `P`'s opposite. -/
def InGroup (A : Alt) (P x : Sq) : Prop := A x = A P ∧ x ≠ opp P

instance (A : Alt) (P : Sq) : DecidablePred (InGroup A P) :=
  fun _ => inferInstanceAs (Decidable (_ ∧ _))

/-- The sinking group. -/
def group (A : Alt) (P : Sq) : Finset Sq := Finset.univ.filter (InGroup A P)

/-- How far the group sinks during a wait of `t` minutes. -/
def sinkAmt (A : Alt) (P : Sq) (t : ℚ) : ℚ := t / (group A P).card

/-- The opposite point rises unless it is at the knight's altitude. -/
def Rises (A : Alt) (P x : Sq) : Prop := x = opp P ∧ A x ≠ A P

instance (A : Alt) (P : Sq) : DecidablePred (Rises A P) :=
  fun _ => inferInstanceAs (Decidable (_ ∧ _))

/-- Altitudes after waiting `t` minutes on `P`. -/
def sit (A : Alt) (P : Sq) (t : ℚ) : Alt := fun x =>
  if InGroup A P x then A x - sinkAmt A P t
  else if Rises A P x then A x + sinkAmt A P t
  else A x

/-- `|z| = c`, written without `abs` so that the kernel evaluates it directly. -/
def AbsEq (z c : ℚ) : Prop := z = c ∨ z = -c

instance (z c : ℚ) : Decidable (AbsEq z c) := inferInstanceAs (Decidable (_ ∨ _))

/-- A legal jump: (|Δfile|, |Δrank|, |Δaltitude|) is a permutation of (0, 1, 2). -/
def JumpOK (A : Alt) (P Q : Sq) : Prop :=
  let dx := Nat.dist P.1 Q.1
  let dy := Nat.dist P.2 Q.2
  let dz := A Q - A P
  (dx = 0 ∧ dy = 1 ∧ AbsEq dz 2) ∨ (dx = 0 ∧ dy = 2 ∧ AbsEq dz 1) ∨
  (dx = 1 ∧ dy = 0 ∧ AbsEq dz 2) ∨ (dx = 1 ∧ dy = 2 ∧ dz = 0) ∨
  (dx = 2 ∧ dy = 0 ∧ AbsEq dz 1) ∨ (dx = 2 ∧ dy = 1 ∧ dz = 0)

instance (A : Alt) (P Q : Sq) : Decidable (JumpOK A P Q) := by
  unfold JumpOK; infer_instance

/-- A move: wait `wait` minutes, then jump to `dest`. -/
structure Move where
  wait : ℚ
  dest : Sq
deriving DecidableEq

/-- Altitudes and the knight's square. -/
structure St where
  alt : Alt
  pos : Sq

/-- The state after a move (the jump itself changes no altitude). -/
def St.next (s : St) (m : Move) : St := ⟨sit s.alt s.pos m.wait, m.dest⟩

def start : St := ⟨init, a1⟩

/-- Every wait is non-negative and every jump is legal. -/
def MovesOK : St → List Move → Prop
  | _, [] => True
  | s, m :: ms => 0 ≤ m.wait ∧ JumpOK (sit s.alt s.pos m.wait) s.pos m.dest ∧ MovesOK (s.next m) ms

instance decMovesOK : ∀ (s : St) (l : List Move), Decidable (MovesOK s l)
  | _, [] => isTrue trivial
  | s, m :: ms =>
    have := decMovesOK (s.next m) ms
    inferInstanceAs (Decidable (_ ∧ _ ∧ _))

/-- Total length of the tour in minutes. -/
def time (tour : List Move) : ℚ := (tour.map Move.wait).sum

/-- Every point the knight stands on, the start included. -/
def visits (tour : List Move) : List Sq := a1 :: tour.map Move.dest

/-- A legal tour (apart from necessity): legal moves, it ends on its first arrival at h8, and every
other point is visited at most 3 times. -/
def Legal (tour : List Move) : Prop :=
  MovesOK start tour ∧ (tour.map Move.dest).getLast? = some h8 ∧
  (visits tour).count h8 = 1 ∧ ∀ x, x ≠ h8 → (visits tour).count x ≤ 3

/-- The tour with the wait of move `i` replaced by `t`. -/
def setWait (tour : List Move) (i : ℕ) (t : ℚ) : List Move :=
  tour.modify i fun m => { m with wait := t }

/-- The puzzle's necessity rule for move `i`, as worded: some other value of the wait makes some
move of the tour illegal. -/
def Necessary (tour : List Move) (i : ℕ) : Prop :=
  ∃ t' : ℚ, 0 ≤ t' ∧ ¬ MovesOK start (setWait tour i t')

def AllNecessary (tour : List Move) : Prop :=
  ∀ i (hi : i < tour.length), 0 < (tour[i]).wait → Necessary tour i

/-- A tour that satisfies every rule of the puzzle. -/
def Valid (tour : List Move) : Prop := Legal tour ∧ AllNecessary tour

/-- Strict tours: every wait is pinned by its own jump, i.e. the jump's target is not in the
sinking group (so any small change of the wait changes that jump's altitude difference). -/
def Strict : St → List Move → Prop
  | _, [] => True
  | s, m :: ms => (0 < m.wait → ¬ InGroup s.alt s.pos m.dest) ∧ Strict (s.next m) ms

instance decStrict : ∀ (s : St) (l : List Move), Decidable (Strict s l)
  | _, [] => isTrue trivial
  | s, m :: ms =>
    have := decStrict (s.next m) ms
    inferInstanceAs (Decidable (_ ∧ _))

end KM5
