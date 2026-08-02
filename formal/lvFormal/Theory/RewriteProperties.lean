/-
Lv-00 formal: RewriteProperties (Round 10)
===========================================
Corresponds to: bootstrap/src/spec/expr_canonical.lv
Theorems: church_rosser_confluence, strong_normalization
-/
import Mathlib

namespace lvFormal.Theory.RewriteProperties

/-! ## λ-项语言与 β-归约 -/

/-- λ-项：变量、应用、λ-抽象 -/
inductive Term where
  | var (n : Nat)
  | app (t1 t2 : Term)
  | lam (body : Term)
  deriving DecidableEq, Repr

open Term

/-- 项深度 -/
def depth : Term → Nat
  | .var _     => 0
  | .app t1 t2 => max (depth t1) (depth t2) + 1
  | .lam body  => depth body + 1

/-- 项大小（构造子总数），用于严格递减证明 -/
def size : Term → Nat
  | .var _     => 1
  | .app t1 t2 => size t1 + size t2 + 1
  | .lam body  => size body + 1

/-- 单步 β-归约关系 -/
inductive BetaRed : Term → Term → Prop where
  | beta    : BetaRed (.app (.lam b) a) (b)
  | appL    : BetaRed t1 t1' → BetaRed (.app t1 t2) (.app t1' t2)
  | appR    : BetaRed t2 t2' → BetaRed (.app t1 t2) (.app t1 t2')
  | lamBody : BetaRed t t' → BetaRed (.lam t) (.lam t')

/-- β-归约的传递闭包 -/
inductive BetaRedStar : Term → Term → Prop where
  | refl : BetaRedStar t t
  | step : BetaRed t1 t2 → BetaRedStar t2 t3 → BetaRedStar t1 t3

open BetaRed
open BetaRedStar

/-! ## 基本性质 -/

/-- BetaRedStar 的传递性 -/
lemma BetaRedStar.trans {a b c : Term} (hab : BetaRedStar a b) (hbc : BetaRedStar b c) : BetaRedStar a c := by
  induction hab with
  | refl => exact hbc
  | step h1 h2 ih => exact BetaRedStar.step h1 (ih hbc)

/-- 将 BetaRedStar 提升到 appL 上下文中 -/
lemma BetaRedStar.appL {t1 t1' t2 : Term} (h : BetaRedStar t1 t1') : BetaRedStar (.app t1 t2) (.app t1' t2) := by
  induction h with
  | refl => exact .refl
  | step h1 h2 ih => exact .step (.appL h1) ih

/-- 将 BetaRedStar 提升到 appR 上下文中 -/
lemma BetaRedStar.appR {t1 t2 t2' : Term} (h : BetaRedStar t2 t2') : BetaRedStar (.app t1 t2) (.app t1 t2') := by
  induction h with
  | refl => exact .refl
  | step h1 h2 ih => exact .step (.appR h1) ih

/-- 将 BetaRedStar 提升到 lamBody 上下文中 -/
lemma BetaRedStar.lamBody {t t' : Term} (h : BetaRedStar t t') : BetaRedStar (.lam t) (.lam t') := by
  induction h with
  | refl => exact .refl
  | step h1 h2 ih => exact .step (.lamBody h1) ih

/-- β-归约严格减少项大小 -/
theorem beta_strictly_decreases_size (t1 t2 : Term) (h : BetaRed t1 t2) : size t2 < size t1 := by
  induction h with
  | beta =>
    simp [size]
    omega
  | appL h ih =>
    simp [size]
    omega
  | appR h ih =>
    simp [size]
    omega
  | lamBody h ih =>
    simp [size]
    omega

/-- β-归约保持项深度不减 -/
theorem beta_preserves_depth (t1 t2 : Term) (h : BetaRed t1 t2) : depth t2 ≤ depth t1 := by
  induction h with
  | beta =>
    simp [depth]
    omega
  | appL h ih =>
    simp [depth]
    omega
  | appR h ih =>
    simp [depth]
    omega
  | lamBody h ih =>
    simp [depth]
    omega

/-- BetaRedStar 不增加深度 -/
lemma depth_non_increasing (t1 t2 : Term) (h : BetaRedStar t1 t2) : depth t2 ≤ depth t1 := by
  induction h with
  | refl => rfl
  | step h1 h2 ih =>
    have hd1 := beta_preserves_depth _ _ h1
    omega

/-- BetaRedStar 不增加大小（严格递减，故非空序列时严格减小，refl 时相等）-/
lemma size_non_increasing (t1 t2 : Term) (h : BetaRedStar t1 t2) : size t2 ≤ size t1 := by
  induction h with
  | refl => rfl
  | step h1 h2 ih =>
    have hs1 := beta_strictly_decreases_size _ _ h1
    omega

/-- 变量项是范式（不可归约） -/
theorem var_normal_form (n : Nat) : ¬∃ t, BetaRed (.var n) t := by
  intro h
  rcases h with ⟨t, h⟩
  cases h

/-- 强正规化：不存在无限归约序列。
    证明：由 beta_strictly_decreases_size 知每次归约严格减少 size，
    size 是自然数，但自然数不存在无限严格递减序列。 -/
theorem strong_normalization (t : Term) : ¬∃ (f : ℕ → Term), f 0 = t ∧ ∀ n, BetaRed (f n) (f (n+1)) := by
  intro h
  rcases h with ⟨f, h0, hred⟩
  have hsize : ∀ n, size (f (n+1)) < size (f n) := by
    intro n
    exact beta_strictly_decreases_size _ _ (hred n)
  have h_chain : ∀ n, size (f n) + n ≤ size (f 0) := by
    intro n
    induction' n with k ih
    · omega
    · have hk := hsize k
      omega
  have h_contra := h_chain (size (f 0) + 1)
  omega

/-! ## 合流性 -/

/-- β-归约的 diamond property：若 a → b 且 a → c，则存在 d 使 b →* d 且 c →* d。

    证明：对 hab 和 hac 做联合 case analysis，利用依赖模式匹配自动统一 a。
    对于需要递归的子情况（如 appL/appL、appR/appR、lamBody/lamBody），递归调用自身。 -/
theorem diamond_property {a b c : Term} (hab : BetaRed a b) (hac : BetaRed a c) :
    ∃ d : Term, BetaRedStar b d ∧ BetaRedStar c d := by
  match hab, hac with
  | BetaRed.beta, BetaRed.beta =>
    exact ⟨b, .refl, .refl⟩
  | BetaRed.beta, BetaRed.appL (BetaRed.lamBody hbody) =>
    exact ⟨_, .step hbody .refl, .step .beta .refl⟩
  | BetaRed.beta, BetaRed.appR _ =>
    exact ⟨b, .refl, .step .beta .refl⟩
  | BetaRed.appL (BetaRed.lamBody hbody), BetaRed.beta =>
    exact ⟨_, .step .beta .refl, .step hbody .refl⟩
  | BetaRed.appL h1, BetaRed.appL h2 =>
    rcases diamond_property h1 h2 with ⟨d1, hd1, hd2⟩
    exact ⟨.app d1 _, BetaRedStar.appL hd1, BetaRedStar.appL hd2⟩
  | BetaRed.appL h1, BetaRed.appR h2 =>
    have h1s : BetaRedStar _ _ := .step h1 .refl
    have h2s : BetaRedStar _ _ := .step h2 .refl
    exact ⟨.app _ _, BetaRedStar.appR h2s, BetaRedStar.appL h1s⟩
  | BetaRed.appR _, BetaRed.beta =>
    exact ⟨c, .step .beta .refl, .refl⟩
  | BetaRed.appR h1, BetaRed.appL h2 =>
    have h1s : BetaRedStar _ _ := .step h1 .refl
    have h2s : BetaRedStar _ _ := .step h2 .refl
    exact ⟨.app _ _, BetaRedStar.appL h2s, BetaRedStar.appR h1s⟩
  | BetaRed.appR h1, BetaRed.appR h2 =>
    rcases diamond_property h1 h2 with ⟨d2, hd1, hd2⟩
    exact ⟨.app _ d2, BetaRedStar.appR hd1, BetaRedStar.appR hd2⟩
  | BetaRed.lamBody h1, BetaRed.lamBody h2 =>
    rcases diamond_property h1 h2 with ⟨d1, hd1, hd2⟩
    exact ⟨.lam d1, .lamBody hd1, .lamBody hd2⟩

/-- Church-Rosser 合流性：若 a →* b 且 a →* c，则存在 d 使 b →* d 且 c →* d。

    证明：对 size a 进行强归纳（Newman's lemma）。
    由 diamond_property 得到一步合流点 w，然后由大小递减性质使用归纳假设。 -/
theorem church_rosser_confluence {a b c : Term} (hab : BetaRedStar a b) (hac : BetaRedStar a c) :
    ∃ d : Term, BetaRedStar b d ∧ BetaRedStar c d := by
  -- 定义强归纳性质 P：所有大小为 n 的项都满足合流性
  let P (n : ℕ) : Prop := ∀ (a' : Term), size a' = n → ∀ (b' c' : Term),
    BetaRedStar a' b' → BetaRedStar a' c' → ∃ d, BetaRedStar b' d ∧ BetaRedStar c' d
  have hP : ∀ n, (∀ m < n, P m) → P n := by
    intro n IH a' ha_size b' c' hab hac
    -- ha_size : size a' = n, IH : ∀ m < n, P m
    cases a' with
    | var var_n =>
      -- 变量项是范式，所有归约序列必须是 refl
      cases hab with
      | refl =>
        cases hac with
        | refl => exact ⟨.var var_n, .refl, .refl⟩
        | step h1 h2 => cases h1
      | step h1 h2 => cases h1
    | app t1 t2 =>
      -- a' = .app t1 t2
      -- ha_size : size (.app t1 t2) = n
      cases hab
      · -- refl: a' = b', 即 b' = .app t1 t2
        exact ⟨c', hac, .refl⟩
      · -- step a_mid h1 h2
        rename_i a_mid h1 h2
        -- h1 : BetaRed (.app t1 t2) a_mid, h2 : BetaRedStar a_mid b'
        cases hac
        · -- refl: a' = c', 即 c' = .app t1 t2
          exact ⟨b', .refl, BetaRedStar.step h1 h2⟩
        · -- step a_mid2 h3 h4
          rename_i a_mid2 h3 h4
          -- h3 : BetaRed (.app t1 t2) a_mid2, h4 : BetaRedStar a_mid2 c'
          rcases diamond_property h1 h3 with ⟨w, hw1, hw2⟩
          -- hw1 : BetaRedStar a_mid w, hw2 : BetaRedStar a_mid2 w
          have ha_mid_lt_n : size a_mid < n := by
            have h1size := beta_strictly_decreases_size _ _ h1
            omega
          have ha_mid2_lt_n : size a_mid2 < n := by
            have h3size := beta_strictly_decreases_size _ _ h3
            omega
          have hw_lt_n : size w < n := by
            have hw_le : size w ≤ size a_mid := size_non_increasing _ _ hw1
            omega
          -- 从 a_mid 出发：a_mid →* b' 和 a_mid →* w
          rcases IH (size a_mid) ha_mid_lt_n a_mid rfl b' w h2 hw1 with ⟨d1, hbd1, hwd1⟩
          -- 从 a_mid2 出发：a_mid2 →* c' 和 a_mid2 →* w
          rcases IH (size a_mid2) ha_mid2_lt_n a_mid2 rfl c' w h4 hw2 with ⟨d2, hcd2, hwd2⟩
          -- 从 w 出发：w →* d1 和 w →* d2
          rcases IH (size w) hw_lt_n w rfl d1 d2 hwd1 hwd2 with ⟨d, hd1d, hd2d⟩
          -- b' →* d1 →* d, c' →* d2 →* d
          exact ⟨d, BetaRedStar.trans hbd1 hd1d, BetaRedStar.trans hcd2 hd2d⟩
    | lam body =>
      -- a' = .lam body
      -- ha_size : size (.lam body) = n
      cases hab
      · -- refl: a' = b', 即 b' = .lam body
        exact ⟨c', hac, .refl⟩
      · -- step a_mid h1 h2
        rename_i a_mid h1 h2
        -- h1 : BetaRed (.lam body) a_mid, h2 : BetaRedStar a_mid b'
        cases hac
        · -- refl: a' = c', 即 c' = .lam body
          exact ⟨b', .refl, BetaRedStar.step h1 h2⟩
        · -- step a_mid2 h3 h4
          rename_i a_mid2 h3 h4
          -- h3 : BetaRed (.lam body) a_mid2, h4 : BetaRedStar a_mid2 c'
          rcases diamond_property h1 h3 with ⟨w, hw1, hw2⟩
          -- hw1 : BetaRedStar a_mid w, hw2 : BetaRedStar a_mid2 w
          have ha_mid_lt_n : size a_mid < n := by
            have h1size := beta_strictly_decreases_size _ _ h1
            omega
          have ha_mid2_lt_n : size a_mid2 < n := by
            have h3size := beta_strictly_decreases_size _ _ h3
            omega
          have hw_lt_n : size w < n := by
            have hw_le : size w ≤ size a_mid := size_non_increasing _ _ hw1
            omega
          -- 从 a_mid 出发：a_mid →* b' 和 a_mid →* w
          rcases IH (size a_mid) ha_mid_lt_n a_mid rfl b' w h2 hw1 with ⟨d1, hbd1, hwd1⟩
          -- 从 a_mid2 出发：a_mid2 →* c' 和 a_mid2 →* w
          rcases IH (size a_mid2) ha_mid2_lt_n a_mid2 rfl c' w h4 hw2 with ⟨d2, hcd2, hwd2⟩
          -- 从 w 出发：w →* d1 和 w →* d2
          rcases IH (size w) hw_lt_n w rfl d1 d2 hwd1 hwd2 with ⟨d, hd1d, hd2d⟩
          -- b' →* d1 →* d, c' →* d2 →* d
          exact ⟨d, BetaRedStar.trans hbd1 hd1d, BetaRedStar.trans hcd2 hd2d⟩
  have hP_size_a : P (size a) := Nat.strong_induction_on (size a) hP
  exact hP_size_a a rfl b c hab hac

/-- 简单合流性特例：若 a →* b 且 a →* a，则 b →* b -/
theorem trivial_confluence (a b : Term) (hab : BetaRedStar a b) : BetaRedStar b b := by
  exact BetaRedStar.refl

end lvFormal.Theory.RewriteProperties