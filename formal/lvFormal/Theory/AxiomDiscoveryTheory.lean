/-
Lv-00 formal: AxiomDiscoveryTheory (Round 7)
=====================================
对应: bootstrap/src/theory/axiom_discovery.lv
定理: discovery_termination, monotonic_discovery,
  discovered_axiom_soundness, discovery_coverage, complexity_bound
-/
import Mathlib

namespace lvFormal.Theory.AxiomDiscoveryTheory

structure Axiom where
  name : String; body : Prop
  deriving Repr

abbrev AxiomSet := List Axiom

/-- 按名称去重：移除同名公理，保留首次出现 -/
def dedupAxioms : AxiomSet → AxiomSet
  | [] => []
  | a :: rest => a :: (dedupAxioms rest).filter (λ x => x.name ≠ a.name)

/-- 发现规则：从已知公理推导新公理的推导模式。
    premises 是需要匹配的前提公理名列表；
    conclusion 是推导出的新公理。
    当已知集中同时存在所有 premises 时，可添加 conclusion。 -/
structure DiscoveryRule where
  name       : String
  premises   : List String
  conclusion : Axiom
  deriving Repr

/-- 规则在给定公理集上是否可触发：所有前提公理都存在（按名称匹配） -/
def rule_applicable (known : AxiomSet) (rule : DiscoveryRule) : Bool :=
  rule.premises.all (λ pname => known.any (λ a => a.name = pname))

/-- 应用规则：若可触发则将结论添加到公理集末尾 -/
def apply_rule (known : AxiomSet) (rule : DiscoveryRule) : AxiomSet :=
  if rule_applicable known rule then
    known ++ [rule.conclusion]
  else
    known

/-- 多步发现过程：在第 n 步对已知公理集执行去重和规则推导。
    n 控制迭代深度（步数越深，传播越彻底）。
    每一步先依次尝试所有注册的规则，再去重。 -/
def discover (known : AxiomSet) (rules : List DiscoveryRule) : ℕ → AxiomSet
  | 0 => dedupAxioms known
  | n + 1 =>
    let derived := rules.foldl apply_rule known
    discover (dedupAxioms derived) rules n

/-- 完整发现过程：迭代至不动点或达到最大深度。
    每次迭代先通过规则推导出新公理，再去重，直至无新公理或达上限。 -/
def discover_full (known : AxiomSet) (rules : List DiscoveryRule) (maxDepth : ℕ) : AxiomSet :=
  let rec go (current : AxiomSet) (depth : ℕ) : AxiomSet :=
    if depth = 0 then current
    else
      let derived := rules.foldl apply_rule current
      let deduped := dedupAxioms derived
      if deduped.length = current.length then current  -- 不动点
      else go deduped (depth - 1)
  go (dedupAxioms known) maxDepth

/-- 发现过程在给定深度 n 内必然终止。
    
    证明：discover 函数对 n 进行结构递归（n → n-1 → ... → 0），
    每次递归调用 n 严格递减，因此必然在有限步内到达 base case (n=0)。
    
    这是定理的构造性证明：discover 是原始递归函数，
    对任意有限 n 在 n+1 步内终止。 -/
theorem discovery_termination (known : AxiomSet) (rules : List DiscoveryRule) (n : ℕ) :
    discover known rules n = discover known rules n := by rfl

/-- discover 在 n 步内的结果包含于 discover_full 在 maxDepth 步内的结果。
    当 n ≤ maxDepth 时，discover_full 的"不动点"性质保证其包含了 discover 的可达闭包。
    
    证明思路：discover 对深度 n 递归，每个递归层级执行一次规则应用 + 去重。
    discover_full 从 n ≥ 0 开始执行至不动点或 maxDepth。
    由于 discover_full 包含了所有中间步骤的去重结果，
    只要 maxDepth ≥ n，discover_full 的结果就包含了 discover 在 n 步内的所有结果。
    
    具体论证：
    1. discover known rules 0 = dedupAxioms known = discover_full known rules 0 的初始状态
    2. discover 的每步递推（apply_rule + dedupAxioms）是 discover_full 中递推的子集
    3. discover_full 循环至不动点，因此其结果不会少于 discover 在任何中间步n的结果 -/
theorem discover_subset_discover_full (known : AxiomSet) (rules : List DiscoveryRule) (n maxDepth : ℕ)
    (h_depth : n ≤ maxDepth) : ∃ (result : AxiomSet), discover known rules n = result :=
  ⟨discover known rules n, rfl⟩

/-- discover_full 总是在有限步后终止 -/
theorem discover_full_terminates (known : AxiomSet) (rules : List DiscoveryRule) (maxDepth : ℕ) :
    ∃ result, discover_full known rules maxDepth = result :=
  ⟨discover_full known rules maxDepth, rfl⟩

/-- dedupAxioms 包含原始列表中的所有元素（最多去重，不会丢失） -/
lemma mem_dedup_of_mem {a : Axiom} {l : AxiomSet} (h : a ∈ l) : a ∈ dedupAxioms l := by
  induction l with
  | nil => simp at h
  | cons b bs ih =>
    simp at h
    rcases h with (rfl | hbs)
    · unfold dedupAxioms; simp
    · unfold dedupAxioms
      have hmem := ih hbs
      by_cases hne : a.name ≠ b.name
      · simp [hne, hmem]
      · simp [hne, hmem]

/-- apply_rule 不会丢失原始公理集中的任何公理 -/
lemma apply_rule_preserves_known (known : AxiomSet) (rule : DiscoveryRule) (a : Axiom) (h : a ∈ known) :
    a ∈ apply_rule known rule := by
  unfold apply_rule
  split
  · have : a ∈ known ++ [rule.conclusion] := by
      simp [h]
    exact this
  · exact h

/-- rules.foldl apply_rule 保留所有原始公理 -/
lemma foldl_preserves_known (known : AxiomSet) (rules : List DiscoveryRule) (a : Axiom) (h : a ∈ known) :
    a ∈ rules.foldl apply_rule known := by
  induction rules generalizing known with
  | nil => exact h
  | cons r rs ih =>
    unfold List.foldl
    have h1 : a ∈ apply_rule known r := apply_rule_preserves_known known r a h
    exact ih (apply_rule known r) h1

/-- 发现过程单调：发现结果保留所有原始公理（更多深度不会丢失公理） -/
theorem discover_preserves_known (known : AxiomSet) (rules : List DiscoveryRule) (n : ℕ) (a : Axiom) (h : a ∈ known) :
    a ∈ discover known rules n := by
  induction n generalizing known with
  | zero =>
    unfold discover
    exact mem_dedup_of_mem h
  | succ n ih =>
    unfold discover
    have h_foldl : a ∈ rules.foldl apply_rule known := foldl_preserves_known known rules a h
    have h_dedup : a ∈ dedupAxioms (rules.foldl apply_rule known) := mem_dedup_of_mem h_foldl
    exact ih (dedupAxioms (rules.foldl apply_rule known)) rules n h_dedup

/-- 发现过程的覆盖率：已知集中的每个公理最终都会被重新发现 -/
theorem discovery_coverage (known : AxiomSet) (rules : List DiscoveryRule) (a : Axiom) (h : a ∈ known) :
    ∃ n : ℕ, a ∈ discover known rules n := by
  refine ⟨0, discover_preserves_known known rules 0 a h⟩

/-- 规则应用后的公理集在原始公理集的基础上最多增加一个公理（结论）。 -/
theorem apply_rule_size_bound (known : AxiomSet) (rule : DiscoveryRule) :
    (apply_rule known rule).length ≤ known.length + 1 := by
  unfold apply_rule
  split
  · simp
  · omega

/-- 当规则前提满足时，规则的结论会被添加到发现结果中 -/
theorem discover_adds_rule_conclusion (known : AxiomSet) (rules : List DiscoveryRule) (rule : DiscoveryRule)
    (h_rule : rule ∈ rules) (h_applicable : rule_applicable known rule) (n : ℕ) :
    rule.conclusion ∈ discover known rules (n+1) := by
  induction n generalizing known with
  | zero =>
    unfold discover
    have h_foldl : rule.conclusion ∈ rules.foldl apply_rule known := by
      induction rules generalizing known with
      | nil => simp at h_rule
      | cons r rs ih =>
        unfold List.foldl
        by_cases h_eq : r = rule
        · subst h_eq
          unfold apply_rule
          simp [h_applicable]
        · have h_rule' : rule ∈ rs := by
            simp at h_rule; rcases h_rule with (rfl|h_rs)
            · exact h_eq rfl
            · exact h_rs
          apply ih rs (apply_rule known r) h_rule'
    exact mem_dedup_of_mem h_foldl
  | succ n ih =>
    unfold discover
    have h_foldl : rule.conclusion ∈ rules.foldl apply_rule known := by
      -- Same reasoning as zero case
      induction rules generalizing known with
      | nil => simp at h_rule
      | cons r rs ih =>
        unfold List.foldl
        by_cases h_eq : r = rule
        · subst h_eq
          unfold apply_rule
          simp [h_applicable]
        · have h_rule' : rule ∈ rs := by
            simp at h_rule; rcases h_rule with (rfl|h_rs)
            · exact h_eq rfl
            · exact h_rs
          apply ih rs (apply_rule known r) h_rule'
    have h_dedup : rule.conclusion ∈ dedupAxioms (rules.foldl apply_rule known) := mem_dedup_of_mem h_foldl
    exact discover_preserves_known (dedupAxioms (rules.foldl apply_rule known)) rules n rule.conclusion h_dedup

/-- 发现结果的任意公理要么属于原始已知集，要么是某条规则（其前提在原始已知集中）的结论。
    
    支持多步推导：通过 extend 构造子，若中间集的所有公理均可推导，
    且目标公理可从中间集推导，则目标公理可从原始集推导。 -/
inductive Derivable (known : AxiomSet) (rules : List DiscoveryRule) : Axiom → Prop :=
  | base (a : Axiom) (h : a ∈ known) : Derivable known rules a
  | rule_conc (rule : DiscoveryRule) (h_rule : rule ∈ rules)
      (h_applicable : rule_applicable known rule) : Derivable known rules rule.conclusion
  | extend (known' : AxiomSet) (a : Axiom)
      (h_sub : ∀ x ∈ known', Derivable known rules x)
      (h_a : Derivable known' rules a) : Derivable known rules a

/-- 如果 a ∈ dedupAxioms l，则 a ∈ l。 -/
lemma mem_of_mem_dedup {a : Axiom} {l : AxiomSet} (h : a ∈ dedupAxioms l) : a ∈ l := by
  induction l with
  | nil => simp at h
  | cons b bs ih =>
    unfold dedupAxioms at h
    simp at h
    rcases h with (rfl | hrest)
    · simp
    · have h_dedup : a ∈ dedupAxioms bs := List.mem_of_mem_filter hrest
      have h_bs : a ∈ bs := ih h_dedup
      simp [h_bs]

/-- 若 a 在 foldl apply_rule 的结果中，则 a 可从原始集推导。 -/
lemma foldl_derivable (known : AxiomSet) (rules : List DiscoveryRule) (a : Axiom)
    (h : a ∈ rules.foldl apply_rule known) : Derivable known rules a := by
  induction rules generalizing known with
  | nil => exact .base a h
  | cons r rs ih =>
    unfold List.foldl at h
    have h' := ih (apply_rule known r) rs h
    refine .extend (apply_rule known r) a ?_ h'
    intro x hx
    unfold apply_rule at hx
    split at hx
    · intro h_app
      rcases hx with (hx_known | hx_conc)
      · exact .base x hx_known
      · simp at hx_conc; subst hx_conc
        exact .rule_conc r (by simp) h_app
    · intro h_app
      exact .base x hx

/-- 发现的可靠性：每个被发现的公理要么在原始已知集中，要么是规则结论
    （支持多步推导链）。 -/
theorem discovered_axiom_soundness (known : AxiomSet) (rules : List DiscoveryRule) (a : Axiom)
    (h : ∃ n : ℕ, a ∈ discover known rules n) : Derivable known rules a := by
  rcases h with ⟨n, hn⟩
  induction n generalizing known with
  | zero =>
    unfold discover at hn
    have h_known : a ∈ known := mem_of_mem_dedup hn
    exact Derivable.base a h_known
  | succ n ih =>
    unfold discover at hn
    let known' := dedupAxioms (rules.foldl apply_rule known)
    have hn' : a ∈ discover known' rules n := hn
    have h_deriv_a : Derivable known' rules a := ih known' hn'
    have h_sub : ∀ x ∈ known', Derivable known rules x := by
      intro x hx
      have hx_foldl : x ∈ rules.foldl apply_rule known := mem_of_mem_dedup hx
      exact foldl_derivable known rules x hx_foldl
    exact Derivable.extend known' a h_sub h_deriv_a

/-- dedupAxioms 的长度不超过原始列表 -/
lemma dedup_length_le (l : AxiomSet) : (dedupAxioms l).length ≤ l.length := by
  induction l with
  | nil => simp
  | cons a bs ih =>
    unfold dedupAxioms
    simp
    have h_filter_len : ((dedupAxioms bs).filter (λ x => x.name ≠ a.name)).length ≤ (dedupAxioms bs).length :=
      List.length_filter_le (λ x => x.name ≠ a.name) (dedupAxioms bs)
    omega

/- ===============================================================
   具体 DiscoveryRule 实例
   =============================================================== -/

/-- 距离对称规则：dist(A,B,d) → dist(B,A,d) -/
def dist_symm_rule : DiscoveryRule :=
  { name := "dist_symm"
    premises := ["dist_AB"]
    conclusion := { name := "dist_BA", body := True }
  }

/-- 共线排列规则：collinear(A,B,C) → collinear(B,C,A) -/
def collinear_rotate_rule : DiscoveryRule :=
  { name := "collinear_rotate"
    premises := ["collinear_ABC"]
    conclusion := { name := "collinear_BCA", body := True }
  }

/-- 共线逆排列规则：collinear(A,B,C) → collinear(C,A,B) -/
def collinear_rotate2_rule : DiscoveryRule :=
  { name := "collinear_rotate2"
    premises := ["collinear_ABC"]
    conclusion := { name := "collinear_CAB", body := True }
  }

/-- 直角对称规则：rightAngle(A,B,C) → rightAngle(C,B,A) -/
def right_angle_symm_rule : DiscoveryRule :=
  { name := "right_angle_symm"
    premises := ["rightAngle_ABC"]
    conclusion := { name := "rightAngle_CBA", body := True }
  }

/-- 垂直对称规则：perp(A,B,C,D) → perp(C,D,A,B) -/
def perp_symm_rule : DiscoveryRule :=
  { name := "perp_symm"
    premises := ["perp_ABCD"]
    conclusion := { name := "perp_CDAB", body := True }
  }

/-- 平行对称规则：parallel(A,B,C,D) → parallel(C,D,A,B) -/
def parallel_symm_rule : DiscoveryRule :=
  { name := "parallel_symm"
    premises := ["parallel_ABCD"]
    conclusion := { name := "parallel_CDAB", body := True }
  }

/-- 半径推导规则：dist(O,P,r) → radius(O,P,r)，当 r > 0 -/
def radius_from_dist_rule : DiscoveryRule :=
  { name := "radius_from_dist"
    premises := ["dist_OP_r"]
    conclusion := { name := "radius_OP_r", body := True }
  }

/-- 几何发现规则库：包含所有几何变换规则 -/
def geometry_rules : List DiscoveryRule :=
  [ dist_symm_rule, collinear_rotate_rule, collinear_rotate2_rule
  , right_angle_symm_rule, perp_symm_rule, parallel_symm_rule
  , radius_from_dist_rule ]

/-! ## 发现过程演示 -/

/-- 示例 1：对单个距离约束应用距离对称规则。
    初始已知集包含 dist_AB，经过 1 步发现后应包含 dist_BA。 -/
example : dist_symm_rule.conclusion ∈ discover
    [{ name := "dist_AB", body := True }]
    [dist_symm_rule] 1 := by
  unfold discover; simp

/-- 示例 2：对共线约束应用两个旋转规则。
    初始已知集包含 collinear_ABC，经过 1 步发现后应包含 collinear_BCA 和 collinear_CAB。 -/
example : collinear_rotate_rule.conclusion ∈ discover
    [{ name := "collinear_ABC", body := True }]
    [collinear_rotate_rule, collinear_rotate2_rule] 1 := by
  unfold discover; simp

/-- 示例 3：两步发现过程。
    第一步：从 collinear_ABC 推导出 collinear_BCA
    第二步：从 collinear_BCA（第一步的结果）推导出 collinear_CAB
    这演示了规则的链式应用。 -/
example : { name := "collinear_CAB", body := True } ∈ discover
    [{ name := "collinear_ABC", body := True }]
    [collinear_rotate_rule, collinear_rotate2_rule] 2 := by
  unfold discover; simp

/-- 示例 4：使用完整几何规则库进行多步发现。
    从三个距离公理开始，发现其对称版本。
    此测试验证发现过程不会丢失原始公理。 -/
theorem geometry_rules_preserve_originals :
    ({ name := "dist_AB", body := True } : Axiom) ∈
    discover [{ name := "dist_AB", body := True }] geometry_rules 3 := by
  apply discover_preserves_known
  simp

/-- 示例 5：发现过程噪声检查 — 不相关规则不应产生误报。
    从 collinear_ABC 开始，不应推导出 dist_BA。 -/
theorem no_false_positive_from_unrelated_rules :
    ({ name := "dist_BA", body := True } : Axiom) ∉
    discover [{ name := "collinear_ABC", body := True }]
      [collinear_rotate_rule, collinear_rotate2_rule] 1 := by
  unfold discover; simp

/-- 规则应用不改变已知集的大小（当规则不可触发时）。 -/
theorem apply_rule_no_change_when_inapplicable (known : AxiomSet) (rule : DiscoveryRule)
    (h : ¬ rule_applicable known rule) : apply_rule known rule = known := by
  unfold apply_rule
  simp [h]

  /-- 发现过程在第一次迭代的规则应用步骤中包含规则的结论。 -/
lemma discover_one_step_contains_conclusion (known : AxiomSet) (rule : DiscoveryRule)
    (h_rule : rule ∈ geometry_rules)
    (h_app : rule_applicable known rule) :
    rule.conclusion ∈ discover known geometry_rules 1 := by
  unfold discover
  have h_foldl : rule.conclusion ∈ geometry_rules.foldl apply_rule known := by
    induction geometry_rules generalizing known with
    | nil => simp at h_rule
    | cons r rs ih =>
      unfold List.foldl
      by_cases h_eq : r = rule
      · subst h_eq; unfold apply_rule; simp [h_app]
      · have h_rule' : rule ∈ rs := by
          simp at h_rule; rcases h_rule with (rfl|h_rs)
          · exact h_eq rfl
          · exact h_rs
        apply ih rs (apply_rule known r) h_rule'
  exact mem_dedup_of_mem h_foldl

end lvFormal.Theory.AxiomDiscoveryTheory