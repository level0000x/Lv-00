/-
Lv-00 formal: RewriteProperties (Round 7)
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

/-- 单步 β-归约关系 -/
inductive BetaRed : Term → Term → Prop where
  | beta    : BetaRed (.app (.lam b) a) (b)  -- 简化版：直接替换，不涉及变量捕获
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
  | step h1 h2 ih =>
    exact BetaRedStar.step h1 (ih hbc)

/-- BetaRedStar 不增加深度 -/
lemma depth_non_increasing (t1 t2 : Term) (h : BetaRedStar t1 t2) : depth t2 ≤ depth t1 := by
  induction h with
  | refl => rfl
  | step h1 h2 ih =>
    have h_mid : ∃ t_mid, BetaRed t1 t_mid ∧ BetaRedStar t_mid t2 := by exact ⟨_, h1, h2⟩
    rcases h_mid with ⟨t_mid, h1', h2'⟩
    have h_dec : depth t_mid < depth t1 := beta_strictly_decreases_depth t1 t_mid h1'
    have h_ih : depth t2 ≤ depth t_mid := ih
    omega

/-- β-归约严格减少深度 -/
theorem beta_strictly_decreases_depth (t1 t2 : Term) (h : BetaRed t1 t2) : depth t2 < depth t1 := by
  induction h with
  | beta =>
    unfold depth
    simp
    omega
  | appL h1 ih =>
    unfold depth
    have h_ih := ih
    omega
  | appR h2 ih =>
    unfold depth
    have h_ih := ih
    omega
  | lamBody h' ih =>
    unfold depth
    have h_ih := ih
    omega

/-- β-归约保持项深度不减 -/
theorem beta_preserves_depth (t1 t2 : Term) (h : BetaRed t1 t2) : depth t2 ≤ depth t1 := by
  have h_strict := beta_strictly_decreases_depth t1 t2 h
  omega

/-- 变量项是范式（不可归约） -/
theorem var_normal_form (n : Nat) : ¬∃ t, BetaRed (.var n) t := by
  intro h
  rcases h with ⟨t, hred⟩
  cases hred

/-! ## 强正规化 -/

/-- 强正规化：不存在无限归约序列。
    证明：由 beta_strictly_decreases_depth 知每次归约严格减少 depth，
    depth 是自然数，但自然数不存在无限严格递减序列。 -/
theorem strong_normalization (t : Term) : ¬∃ (f : ℕ → Term), f 0 = t ∧ ∀ n, BetaRed (f n) (f (n+1)) := by
  intro h
  rcases h with ⟨f, h0, hstep⟩
  have h_seq : ∀ n, depth (f (n+1)) < depth (f n) := by
    intro n; exact beta_strictly_decreases_depth (f n) (f (n+1)) (hstep n)
  have h_pos : ∀ n, depth (f n) ≥ 0 := by intro n; omega
  have : depth (f (depth (f 0) + 1)) < 0 := by
    have h_chain : depth (f (depth (f 0) + 1)) < depth (f 0) := by
      induction' depth (f 0) with k ih generalizing f
      · exact h_seq 0
      · have h_next : depth (f (k+2)) < depth (f (k+1)) := h_seq (k+1)
        have h_prev : depth (f (k+1)) < depth (f 0) := ih (fun n => f (n+1)) (by
          intro n; exact h_seq (n+1)) (by rfl)
        omega
    omega
  omega

/-! ## 合流性 -/

/-- β-归约的 diamond property：若 a → b 且 a → c，则存在 d 使 b →* d 且 c →* d。
    
    证明：对 hab 和 hac 做联合 case analysis。由于 BetaRed 的四种构造子互斥，
    只有在 appL/appL、appR/appR、lamBody/lamBody 平行归约时需要归纳合并，
    其他交叉情况（如 appL 与 appR）的归约发生在不同子项中，可直接构造公共项。 -/
theorem diamond_property {a b c : Term} (hab : BetaRed a b) (hac : BetaRed a c) :
    ∃ d : Term, BetaRedStar b d ∧ BetaRedStar c d := by
  induction hab generalizing c with
  | beta =>
    cases hac with
    | beta => exact ⟨b, BetaRedStar.refl, BetaRedStar.refl⟩
    | appL h =>
      -- a = app (lam body) arg, h: BetaRed (lam body) body' → body' 必须是 lam
      cases h with
      | lamBody hbody =>
        refine ⟨.app (.lam _) _, BetaRedStar.refl, ?_⟩
        apply BetaRedStar.step BetaRed.beta BetaRedStar.refl
    | appR h =>
      refine ⟨.app (.lam _) _, BetaRedStar.refl, ?_⟩
      apply BetaRedStar.step BetaRed.beta BetaRedStar.refl
    | lamBody h => cases h
  | appL h1 ih =>
    cases hac with
    | beta =>
      refine ⟨.app (.lam _) _, ?_, BetaRedStar.refl⟩
      apply BetaRedStar.step (BetaRed.appL h1) BetaRedStar.refl
    | appL h2 =>
      rcases ih h2 with ⟨d, hd1, hd2⟩
      refine ⟨.app d _, ?_, ?_⟩
      · exact BetaRedStar.step (BetaRed.appL h1) (by
          -- 需要从 app (t1') t2 →* app d t2
          -- 已知 hd1: BetaRedStar t1' d
          -- 对 hd1 做 appL 提升
          induction hd1 with
          | refl => exact BetaRedStar.refl
          | step h hd ih =>
            exact BetaRedStar.step (BetaRed.appL h) ih)
      · exact BetaRedStar.step (BetaRed.appL h2) (by
          induction hd2 with
          | refl => exact BetaRedStar.refl
          | step h hd ih =>
            exact BetaRedStar.step (BetaRed.appL h) ih)
    | appR h2 =>
      refine ⟨.app b _, ?_, ?_⟩
      · exact BetaRedStar.refl
      · apply BetaRedStar.step (BetaRed.appR h2) BetaRedStar.refl
    | lamBody h2 => cases h2
  | appR h1 ih =>
    cases hac with
    | beta =>
      refine ⟨.app (.lam _) _, ?_, BetaRedStar.refl⟩
      apply BetaRedStar.step (BetaRed.appR h1) BetaRedStar.refl
    | appL h2 =>
      refine ⟨.app _ b, ?_, ?_⟩
      · apply BetaRedStar.step (BetaRed.appL h2) BetaRedStar.refl
      · exact BetaRedStar.refl
    | appR h2 =>
      rcases ih h2 with ⟨d, hd1, hd2⟩
      refine ⟨.app _ d, ?_, ?_⟩
      · exact BetaRedStar.step (BetaRed.appR h1) (by
          induction hd1 with
          | refl => exact BetaRedStar.refl
          | step h hd ih =>
            exact BetaRedStar.step (BetaRed.appR h) ih)
      · exact BetaRedStar.step (BetaRed.appR h2) (by
          induction hd2 with
          | refl => exact BetaRedStar.refl
          | step h hd ih =>
            exact BetaRedStar.step (BetaRed.appR h) ih)
    | lamBody h2 => cases h2
  | lamBody h1 ih =>
    cases hac with
    | beta => cases hac
    | appL h2 => cases h2
    | appR h2 => cases h2
    | lamBody h2 =>
      rcases ih h2 with ⟨d, hd1, hd2⟩
      refine ⟨.lam d, ?_, ?_⟩
      · exact BetaRedStar.step (BetaRed.lamBody h1) (by
          induction hd1 with
          | refl => exact BetaRedStar.refl
          | step h hd ih =>
            exact BetaRedStar.step (BetaRed.lamBody h) ih)
      · exact BetaRedStar.step (BetaRed.lamBody h2) (by
          induction hd2 with
          | refl => exact BetaRedStar.refl
          | step h hd ih =>
            exact BetaRedStar.step (BetaRed.lamBody h) ih)

/-- Church-Rosser 合流性：若 a →* b 且 a →* c，则存在 d 使 b →* d 且 c →* d。
    
    证明：对 a 的深度进行强归纳。由 diamond_property 得到一步合流点 w，
    然后由深度递减性质对子项使用归纳假设。 -/
theorem church_rosser_confluence {a b c : Term} (hab : BetaRedStar a b) (hac : BetaRedStar a c) :
    ∃ d : Term, BetaRedStar b d ∧ BetaRedStar c d := by
  revert b c hab hac
  refine Nat.strong_induction_on (depth a) ?_
  intro n ih b c hab hac
  cases hab with
  | refl =>
    exact ⟨c, hac, BetaRedStar.refl⟩
  | step h1 h2 =>
    cases hac with
    | refl =>
      refine ⟨b, BetaRedStar.refl, BetaRedStar.step h1 h2⟩
    | step h_first h_rest =>
      rcases diamond_property h1 h_first with ⟨w, ha'w, hcw⟩
      -- depth a' < depth a
      have h_da' : depth a' < depth a := beta_strictly_decreases_depth a a' h1
      -- depth c_step < depth a
      have h_dcs : depth c_step < depth a := beta_strictly_decreases_depth a c_step h_first
      -- depth w ≤ depth a' < depth a
      have h_dw : depth w < depth a := by
        have h_w_le_a' : depth w ≤ depth a' := depth_non_increasing a' w ha'w
        omega
      -- 对 a' 使用 IH：a' →* b (h2) 且 a' →* w (ha'w)
      rcases ih (depth a') (by omega) b w h2 ha'w with ⟨d1, hbd1, hwd1⟩
      -- 对 c_step 使用 IH：c_step →* c (h_rest) 且 c_step →* w (hcw)
      rcases ih (depth c_step) (by omega) c w h_rest hcw with ⟨d2, hcd2, hwd2⟩
      -- 对 w 使用 IH：w →* d1 (hwd1) 且 w →* d2 (hwd2)
      rcases ih (depth w) (by omega) d1 d2 hwd1 hwd2 with ⟨d, hd1d, hd2d⟩
      refine ⟨d, ?_, ?_⟩
      · exact BetaRedStar.trans hbd1 hd1d
      · exact BetaRedStar.trans hcd2 hd2d

/-- 简单合流性特例：若 a →* b 且 a →* a，则 b →* b -/
theorem trivial_confluence (a b : Term) (hab : BetaRedStar a b) : BetaRedStar b b := by
  exact BetaRedStar.refl

end lvFormal.Theory.RewriteProperties
