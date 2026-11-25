import KnightMoves5.Lemmas

/-!
# Certifying any tour

`valid_of_certificate` reduces `Valid tour` to checks the kernel can run directly:

* `Legal tour` (decided);
* `StrictExcept start tour S 0`: every positive wait whose index is not in `S` is strict (decided);
* for each index `i ∈ S`, a witness `t' ≥ 0` with `¬ MovesOK start (setWait tour i t')` (decided).

`tools/certify.py` writes such a file for any tour.
-/

namespace KM5

/-- Strict at every index (counted from `k`) except those in `S`. -/
def StrictExcept : St → List Move → List ℕ → ℕ → Prop
  | _, [], _, _ => True
  | s, m :: ms, S, k =>
    (k ∉ S → 0 < m.wait → ¬ InGroup s.alt s.pos m.dest) ∧ StrictExcept (s.next m) ms S (k + 1)

instance decStrictExcept : ∀ (s : St) (l : List Move) (S : List ℕ) (k : ℕ),
    Decidable (StrictExcept s l S k)
  | _, [], _, _ => isTrue trivial
  | s, m :: ms, S, k =>
    have := decStrictExcept (s.next m) ms S (k + 1)
    inferInstanceAs (Decidable (_ ∧ _))

lemma strictExcept_get : ∀ (l : List Move) (s : St) (S : List ℕ) (k i : ℕ) (hi : i < l.length),
    StrictExcept s l S k → k + i ∉ S → 0 < (l[i]).wait →
    ¬ InGroup (run s (l.take i)).alt (run s (l.take i)).pos (l[i]).dest
  | [], _, _, _, _, hi, _, _, _ => absurd hi (by simp)
  | m :: ms, s, S, k, 0, _, h, hS, hp => by simpa using h.1 (by simpa using hS) hp
  | m :: ms, s, S, k, i + 1, hi, h, hS, hp => by
    have := strictExcept_get ms (s.next m) S (k + 1) i (by simpa using hi) h.2
      (by rwa [show k + 1 + i = k + (i + 1) by omega]) (by simpa using hp)
    simpa using this

theorem valid_of_certificate (tour : List Move) (S : List (ℕ × ℚ)) (hL : Legal tour)
    (hS : StrictExcept start tour (S.map Prod.fst) 0)
    (hW : ∀ p ∈ S, 0 ≤ p.2 ∧ ¬ MovesOK start (setWait tour p.1 p.2)) : Valid tour := by
  refine ⟨hL, ?_⟩
  intro i hi hpos
  by_cases hiS : i ∈ S.map Prod.fst
  · obtain ⟨p, hp, rfl⟩ := List.mem_map.1 hiS
    exact ⟨p.2, hW p hp⟩
  · have key : tour = tour.take i ++ tour[i] :: tour.drop (i + 1) := by
      conv_lhs => rw [← List.take_append_drop i tour, List.drop_eq_getElem_cons hi]
    have hlen : (tour.take i).length = i := by simp; omega
    have h' : MovesOK start (tour.take i ++ tour[i] :: tour.drop (i + 1)) := key ▸ hL.1
    have hst := strictExcept_get tour start _ 0 i hi hS (by simpa using hiS) hpos
    have := necessary_of_split _ _ _ h' (fun _ => hst) hpos
    rwa [← key, hlen] at this

end KM5
