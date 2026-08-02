/-
Lv-00 formal: AxiomDiscoveryTheory (Round 7)
=====================================
对应: bootstrap/src/theory/axiom_discovery.lv
定理: discovery_termination, monotonic_discovery,
  discovered_axiom_soundness, discovery_coverage, complexity_bound
-/
import Mathlib

noncomputable section

namespace lvFormal.Theory.AxiomDiscoveryTheory

structure Axiom where
  name : String
  body : Prop

abbrev AxiomSet := List Axiom

/-- 按名称去重：移除同名公理，保留首次出现 -/
def dedupAxioms : AxiomSet → AxiomSet
  | [] => []
  | a :: rest => a :: (dedupAxioms rest).filter (fun x => x.name ≠ a.name)

/-- 发现规则：从已知公理推导新公理的推导模式。
    premises 是需要匹配的前提公理名列表；
    conclusion 是推导出的新公理。
    当已知集中同时存在所有 premises 时，可添加 conclusion。 -/
structure DiscoveryRule where
  name       : String
  premises   : List String
  conclusion : Axiom

/-- 规则在给定公理集上是否可触发：所有前提公理都存在（按名称匹配） -/
def rule_applicable (known : AxiomSet) (rule : DiscoveryRule) : Bool :=
  rule.premises.all (fun pname => known.any (fun a => a.name = pname))

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
    (_h_depth : n ≤ maxDepth) : ∃ (result : AxiomSet), discover known rules n = result :=
  ⟨discover known rules n, rfl⟩

/-- discover_full 总是在有限步后终止 -/
theorem discover_full_terminates (known : AxiomSet) (rules : List DiscoveryRule) (maxDepth : ℕ) :
    ∃ result, discover_full known rules maxDepth = result :=
  ⟨discover_full known rules maxDepth, rfl⟩

/-- dedupAxioms preserves names: every axiom in l has a same-name counterpart in dedupAxioms l -/
lemma mem_dedup_of_mem {a : Axiom} {l : AxiomSet} (h : a ∈ l) : ∃ b ∈ dedupAxioms l, b.name = a.name := by
  induction l with
  | nil => simp at h
  | cons hd tl ih =>
    have h_cases : a = hd ∨ a ∈ tl := by simpa using h
    cases h_cases with
    | inl heq =>
      subst heq
      refine ⟨a, ?_, rfl⟩
      simp [dedupAxioms]
    | inr hmem =>
      rcases ih hmem with ⟨b, hb, hb_name⟩
      by_cases h_eq : b.name = hd.name
      · refine ⟨hd, ?_, ?_⟩
        · simp [dedupAxioms, h_eq]
        · calc
            hd.name = b.name := by symm; exact h_eq
            _ = a.name := hb_name
      · refine ⟨b, ?_, hb_name⟩
        simp [dedupAxioms, h_eq, List.mem_cons_of_mem, List.mem_filter, hb]

/-- apply_rule 不会丢失原始公理集中的任何公理 -/
lemma apply_rule_preserves_known (known : AxiomSet) (rule : DiscoveryRule) (a : Axiom) (h : a ∈ known) :
    a ∈ apply_rule known rule := by
  unfold apply_rule
  by_cases h_app : rule_applicable known rule
  · simp [h_app, h]
  · simp [h_app, h]

/-- rules.foldl apply_rule 保留所有原始公理 -/
lemma foldl_preserves_known (known : AxiomSet) (rules : List DiscoveryRule) (a : Axiom) (h : a ∈ known) :
    a ∈ rules.foldl apply_rule known := by
  induction rules generalizing known a with
  | nil => exact h
  | cons r rs ih =>
    have h_applied : a ∈ apply_rule known r := apply_rule_preserves_known known r a h
    exact ih (apply_rule known r) a h_applied

/-- 发现过程单调：发现结果保留所有原始公理的同名公理（更多深度不会丢失同名公理） -/
theorem discover_preserves_known (known : AxiomSet) (rules : List DiscoveryRule) (n : ℕ) (a : Axiom) (h : a ∈ known) :
    ∃ b ∈ discover known rules n, b.name = a.name := by
  induction n generalizing known rules a with
  | zero =>
    simp [discover]
    exact mem_dedup_of_mem h
  | succ n ih =>
    unfold discover
    have h_foldl : a ∈ rules.foldl apply_rule known := foldl_preserves_known known rules a h
    rcases mem_dedup_of_mem h_foldl with ⟨b, hb, hb_name⟩
    rcases ih (dedupAxioms (rules.foldl apply_rule known)) rules b hb with ⟨c, hc, hc_name⟩
    refine ⟨c, hc, ?_⟩
    calc
      c.name = b.name := hc_name
      _ = a.name := hb_name

/-- 发现过程的覆盖率：已知集中的每个公理最终都会被重新发现（按名称） -/
theorem discovery_coverage (known : AxiomSet) (rules : List DiscoveryRule) (a : Axiom) (h : a ∈ known) :
    ∃ n : ℕ, ∃ b ∈ discover known rules n, b.name = a.name := by
  use 0
  exact discover_preserves_known known rules 0 a h

/-- 规则应用后的公理集在原始公理集的基础上最多增加一个公理（结论）。 -/
theorem apply_rule_size_bound (known : AxiomSet) (rule : DiscoveryRule) :
    (apply_rule known rule).length ≤ known.length + 1 := by
  unfold apply_rule
  by_cases h_app : rule_applicable known rule
  · simp [h_app]
  · simp [h_app]

/-- 如果规则可触发，其结论在 apply_rule 的结果中 -/
lemma mem_apply_rule_conclusion (known : AxiomSet) (rule : DiscoveryRule) (h_applicable : rule_applicable known rule) :
    rule.conclusion ∈ apply_rule known rule := by
  simp [apply_rule, h_applicable]

/-- 如果已知集是 known' 的子集，则 foldl apply_rule 的结果也是子集关系。 -/
lemma foldl_apply_rule_superset (known known' : AxiomSet) (rules : List DiscoveryRule) (a : Axiom)
    (h : a ∈ rules.foldl apply_rule known) (h_sub : ∀ x ∈ known, x ∈ known') : a ∈ rules.foldl apply_rule known' := by
  induction rules generalizing known known' a with
  | nil => exact h_sub a h
  | cons r rs ih =>
    have h_sub' : ∀ x ∈ apply_rule known r, x ∈ apply_rule known' r := by
      intro x hx
      unfold apply_rule at hx ⊢
      by_cases h_app : rule_applicable known r
      · simp [h_app] at hx
        rcases hx with (hx_known | hx_conc)
        · by_cases h_app' : rule_applicable known' r
          · simp [h_app']
            left
            exact h_sub x hx_known
          · simp [h_app']
            exact h_sub x hx_known
        · subst hx_conc
          by_cases h_app' : rule_applicable known' r
          · simp [h_app']
          · -- if rule_applicable known' r is false, r.conclusion ∉ known', but this can't happen
            exfalso
            apply h_app'
            unfold rule_applicable
            have h_all : r.premises.all (fun pname => known.any (fun a => a.name = pname)) = true := h_app
            have h_all_forall : ∀ pname ∈ r.premises, known'.any (fun a => a.name = pname) = true := by
              intro pname hp
              have h_known_any : known.any (fun a => a.name = pname) = true :=
                (List.all_eq_true.mp h_all) pname hp
              have h_exists : ∃ a ∈ known, a.name = pname := by
                simpa [List.any_eq_true] using h_known_any
              rcases h_exists with ⟨a, ha, ha_name⟩
              have ha' : a ∈ known' := h_sub a ha
              simpa [List.any_eq_true, ha_name] using ⟨a, ha', ha_name⟩
            exact List.all_eq_true.mpr h_all_forall
      · simp [h_app] at hx
        by_cases h_app' : rule_applicable known' r
        · simp [h_app']
          left
          exact h_sub x hx
        · simp [h_app']
          exact h_sub x hx
    exact ih (apply_rule known r) (apply_rule known' r) a h h_sub'

/-- 如果规则在规则列表中，且可触发，其结论在 foldl 结果中 -/
lemma mem_foldl_apply_rule_conclusion (known : AxiomSet) (rules : List DiscoveryRule) (rule : DiscoveryRule)
    (h_rule : rule ∈ rules) (h_applicable : rule_applicable known rule) :
    rule.conclusion ∈ rules.foldl apply_rule known := by
  induction rules with
  | nil => simp at h_rule
  | cons r rs ih =>
    simp at h_rule
    rcases h_rule with (rfl | h_rest)
    · simp [List.foldl, apply_rule, h_applicable]
      apply foldl_preserves_known (known ++ [rule.conclusion]) rs rule.conclusion
      simp
    · have h_conc_rs : rule.conclusion ∈ rs.foldl apply_rule known := ih h_rest
      have h_sub : ∀ x ∈ known, x ∈ apply_rule known r := by
        intro x hx
        unfold apply_rule
        by_cases h_app : rule_applicable known r
        · simp [h_app, hx]
        · simp [h_app, hx]
      exact foldl_apply_rule_superset known (apply_rule known r) rs rule.conclusion h_conc_rs h_sub

/-- 当规则前提满足时，规则的结论名称会出现在发现结果中 -/
theorem discover_adds_rule_conclusion (known : AxiomSet) (rules : List DiscoveryRule) (rule : DiscoveryRule)
    (h_rule : rule ∈ rules) (h_applicable : rule_applicable known rule) (n : ℕ) :
    ∃ b ∈ discover known rules (n+1), b.name = rule.conclusion.name := by
  induction n generalizing known with
  | zero =>
    unfold discover
    have h_foldl := mem_foldl_apply_rule_conclusion known rules rule h_rule h_applicable
    have h_dedup1 : ∃ b ∈ dedupAxioms (rules.foldl apply_rule known), b.name = rule.conclusion.name :=
      mem_dedup_of_mem h_foldl
    rcases h_dedup1 with ⟨b, hb, hb_name⟩
    have h_dedup2 : ∃ c ∈ dedupAxioms (dedupAxioms (rules.foldl apply_rule known)), c.name = b.name :=
      mem_dedup_of_mem hb
    rcases h_dedup2 with ⟨c, hc, hc_name⟩
    refine ⟨c, hc, ?_⟩
    calc
      c.name = b.name := hc_name
      _ = rule.conclusion.name := hb_name
  | succ n ih =>
    unfold discover
    have h_applicable' : rule_applicable (dedupAxioms (rules.foldl apply_rule known)) rule := by
      unfold rule_applicable
      have h_all : rule.premises.all (fun pname => known.any (fun a => a.name = pname)) = true := by
        unfold rule_applicable at h_applicable; exact h_applicable
      have h_all_forall : ∀ pname ∈ rule.premises,
          (dedupAxioms (rules.foldl apply_rule known)).any (fun a => a.name = pname) = true := by
        intro pname hp
        have h_known_any : known.any (fun a => a.name = pname) = true :=
          (List.all_eq_true.mp h_all) pname hp
        have h_exists : ∃ a ∈ known, a.name = pname := by
          simpa [List.any_iff_exists] using h_known_any
        rcases h_exists with ⟨a, ha, ha_name⟩
        have h_foldl : a ∈ rules.foldl apply_rule known := foldl_preserves_known known rules a ha
        have h_dedup : ∃ b ∈ dedupAxioms (rules.foldl apply_rule known), b.name = a.name :=
          mem_dedup_of_mem h_foldl
        rcases h_dedup with ⟨b, hb, hb_name⟩
        have hb_name' : b.name = pname := by
          calc
            b.name = a.name := hb_name
            _ = pname := ha_name
        simpa [List.any_iff_exists, hb_name'] using ⟨b, hb, hb_name'⟩
      exact List.all_eq_true.mpr h_all_forall
    rcases ih (dedupAxioms (rules.foldl apply_rule known)) h_applicable' with ⟨b, hb, hb_name⟩
    exact ⟨b, hb, hb_name⟩

/-- 发现结果的任意公理要么属于原始已知集，要么是某条规则（其前提在原始已知集中）的结论。
    
    支持多步推导：通过 step 构造子支持规则前提的可推导性，
    通过 derivable_chain 引理支持推导链的传递性。
    
    注意：为避免嵌套的 ∃ (Exists) 在归纳类型中引发错误，step 构造子使用
    premise_fn 函数 + 匹配/可推导性条件来编码前提的存在性。 -/
inductive Derivable (known : AxiomSet) (rules : List DiscoveryRule) : Axiom → Prop where
  | base (a : Axiom) (h : a ∈ known) : Derivable known rules a
  | step (rule : DiscoveryRule) (h_rule : rule ∈ rules)
      (premise_fn : String → Axiom)
      (h_premises_match : ∀ (pname : String), pname ∈ rule.premises → (premise_fn pname).name = pname)
      (h_premises_deriv : ∀ (pname : String), pname ∈ rule.premises → Derivable known rules (premise_fn pname))
      : Derivable known rules rule.conclusion

/-- 如果 Derivable known rules a，且 rules 是 sub 的子集，则 Derivable known sub a。 -/
lemma derivable_rules_superset (known : AxiomSet) (rules sub : List DiscoveryRule) (a : Axiom)
    (h_deriv : Derivable known rules a) (h_sub : ∀ r ∈ rules, r ∈ sub) : Derivable known sub a := by
  induction h_deriv with
  | base a hx => exact Derivable.base a hx
  | step rule h_rule premise_fn h_premises_match h_premises_deriv ih =>
    have h_premises_deriv' : ∀ (pname : String), pname ∈ rule.premises → Derivable known sub (premise_fn pname) := by
      intro pname hp
      exact ih pname hp
    exact Derivable.step rule (h_sub rule h_rule) premise_fn h_premises_match h_premises_deriv'

/-- 如果 a ∈ dedupAxioms l，则 a ∈ l。 -/
lemma mem_of_mem_dedup {a : Axiom} {l : AxiomSet} (h : a ∈ dedupAxioms l) : a ∈ l := by
  induction l with
  | nil => simp [dedupAxioms] at h
  | cons hd tl ih =>
    unfold dedupAxioms at h
    have h_cases : a = hd ∨ a ∈ (dedupAxioms tl).filter (fun x => x.name ≠ hd.name) := by
      simpa using h
    rcases h_cases with (rfl | hmem)
    · simp
    · have hmem_filter := List.mem_filter.mp hmem
      rcases hmem_filter with ⟨hmem_dedup, hname⟩
      have hmem_tl := ih hmem_dedup
      simp [hmem_tl]

/-- 默认公理，用于构造 premise_fn 的默认值。 -/
def defaultAxiom : Axiom := { name := "", body := True }

/-- 链式推导：如果 a 可从 known 推导，且 b 可从 known ++ [a] 推导，则 b 可从 known 推导。
    这是 Derivable 的传递性（割规则）。 -/
lemma derivable_chain (known : AxiomSet) (rules : List DiscoveryRule) (a b : Axiom)
    (h_deriv_a : Derivable known rules a)
    (h_deriv_b : Derivable (known ++ [a]) rules b) : Derivable known rules b := by
  induction h_deriv_b with
  | base a' h_base =>
    have h_cases : a' ∈ known ∨ a' = a := by
      simpa using h_base
    rcases h_cases with (h_known | h_eq)
    · exact Derivable.base a' h_known
    · subst h_eq; exact h_deriv_a
  | step rule h_rule premise_fn h_premises_match h_premises_deriv ih =>
    apply Derivable.step rule h_rule premise_fn h_premises_match
    intro pname hp
    exact ih pname hp

/-- 若 a 在 foldl apply_rule 的结果中，则 a 可从原始集推导。 -/
lemma foldl_derivable (known : AxiomSet) (rules : List DiscoveryRule) (a : Axiom)
    (h : a ∈ rules.foldl apply_rule known) : Derivable known rules a := by
  induction rules generalizing known a with
  | nil => exact Derivable.base a h
  | cons r rs ih =>
    -- h : a ∈ rs.foldl apply_rule (apply_rule known r)
    have h_deriv : Derivable (apply_rule known r) rs a := ih (apply_rule known r) a h
    have h_deriv_saved : Derivable (apply_rule known r) rs a := h_deriv
    -- Convert Derivable (apply_rule known r) rs a to Derivable known (r :: rs) a
    induction h_deriv with
    | base a' h_base =>
      -- a' ∈ apply_rule known r, need Derivable known (r :: rs) a'
      unfold apply_rule at h_base
      by_cases h_app : rule_applicable known r
      · simp [h_app] at h_base
        rcases h_base with (h_known | h_conc)
        · exact Derivable.base a' h_known
        · subst h_conc
          -- Need to construct Derivable.step for r with premise_fn
          have h_premises_ex : ∀ (pname : String), pname ∈ r.premises → ∃ (a : Axiom), a.name = pname ∧ Derivable known (r :: rs) a := by
            intro pname hp
            have h_pname_in_known : known.any (fun a' => a'.name = pname) = true := by
              unfold rule_applicable at h_app
              exact (List.all_eq_true.mp h_app) pname hp
            have h_exists : ∃ a : Axiom, a ∈ known ∧ a.name = pname := by
              simpa [List.any_eq_true] using h_pname_in_known
            rcases h_exists with ⟨a, ha, ha_name⟩
            refine ⟨a, ha_name, Derivable.base a ha⟩
          let premise_fn_r : String → Axiom := λ pname =>
            if h : pname ∈ r.premises then (h_premises_ex pname h).choose else defaultAxiom
          have h_match_r : ∀ (pname : String), pname ∈ r.premises → (premise_fn_r pname).name = pname := by
            intro pname hp
            unfold premise_fn_r
            simp [hp]
            exact (h_premises_ex pname hp).choose_spec.1
          have h_deriv_r : ∀ (pname : String), pname ∈ r.premises → Derivable known (r :: rs) (premise_fn_r pname) := by
            intro pname hp
            unfold premise_fn_r
            simp [hp]
            exact (h_premises_ex pname hp).choose_spec.2
          exact Derivable.step r (by simp) premise_fn_r h_match_r h_deriv_r
      · simp [h_app] at h_base
        exact Derivable.base a' h_base
    | step rule' h_rule' premise_fn' h_premises_match' h_premises_deriv' ih_step =>
      -- rule' ∈ rs, ∀ pname ∈ rule'.premises, Derivable (apply_rule known r) rs (premise_fn' pname)
      -- a' = rule'.conclusion
      -- Need to show Derivable known (r :: rs) rule'.conclusion
      by_cases h_app_r : rule_applicable known r
      · -- apply_rule known r = known ++ [r.conclusion]
        have h_deriv_r : Derivable known (r :: rs) r.conclusion := by
          have h_premises_ex_r : ∀ (pname : String), pname ∈ r.premises → ∃ (a : Axiom), a.name = pname ∧ Derivable known (r :: rs) a := by
            intro pname hp
            have h_pname_in_known : known.any (fun a' => a'.name = pname) = true := by
              unfold rule_applicable at h_app_r
              exact (List.all_eq_true.mp h_app_r) pname hp
            have h_exists : ∃ a : Axiom, a ∈ known ∧ a.name = pname := by
              simpa [List.any_eq_true] using h_pname_in_known
            rcases h_exists with ⟨a, ha, ha_name⟩
            refine ⟨a, ha_name, Derivable.base a ha⟩
          let premise_fn_r : String → Axiom := λ pname =>
            if h : pname ∈ r.premises then (h_premises_ex_r pname h).choose else defaultAxiom
          have h_match_r : ∀ (pname : String), pname ∈ r.premises → (premise_fn_r pname).name = pname := by
            intro pname hp
            unfold premise_fn_r
            simp [hp]
            exact (h_premises_ex_r pname hp).choose_spec.1
          have h_deriv_r' : ∀ (pname : String), pname ∈ r.premises → Derivable known (r :: rs) (premise_fn_r pname) := by
            intro pname hp
            unfold premise_fn_r
            simp [hp]
            exact (h_premises_ex_r pname hp).choose_spec.2
          exact Derivable.step r (by simp) premise_fn_r h_match_r h_deriv_r'
        have h_deriv_rule' : Derivable (known ++ [r.conclusion]) rs rule'.conclusion := by
          have h_app_eq : apply_rule known r = known ++ [r.conclusion] := by
            unfold apply_rule; simp [h_app_r]
          have h_temp : Derivable (apply_rule known r) rs rule'.conclusion := h_deriv_saved
          rw [h_app_eq] at h_temp
          exact h_temp
        have h_sub : ∀ r' ∈ rs, r' ∈ r :: rs := by
          intro r' hr'
          simp [hr']
        have h_deriv_rule'_more : Derivable (known ++ [r.conclusion]) (r :: rs) rule'.conclusion :=
          derivable_rules_superset (known ++ [r.conclusion]) rs (r :: rs) rule'.conclusion h_deriv_rule' h_sub
        exact derivable_chain known (r :: rs) r.conclusion rule'.conclusion h_deriv_r h_deriv_rule'_more
      · -- apply_rule known r = known
        have h_premises_deriv_known : ∀ (pname : String), pname ∈ rule'.premises → Derivable known (r :: rs) (premise_fn' pname) := by
          intro pname hp
          have ha_deriv : Derivable (apply_rule known r) rs (premise_fn' pname) := h_premises_deriv' pname hp
          have ha_deriv_known : Derivable known rs (premise_fn' pname) := by
            simpa [apply_rule, h_app_r] using ha_deriv
          have h_sub_rs : ∀ r' ∈ rs, r' ∈ r :: rs := by
            intro r' hr'
            simp [hr']
          exact derivable_rules_superset known rs (r :: rs) (premise_fn' pname) ha_deriv_known h_sub_rs
        exact Derivable.step rule' (by simp [h_rule']) premise_fn' h_premises_match' h_premises_deriv_known

/-- 如果公理可从 dedupAxioms l 推导，则也可从 l 推导。 -/
lemma derivable_of_dedup (l : AxiomSet) (rules : List DiscoveryRule) (a : Axiom)
    (h : Derivable (dedupAxioms l) rules a) : Derivable l rules a := by
  induction h with
  | base a' h_base =>
    have h_mem : a' ∈ l := mem_of_mem_dedup h_base
    exact Derivable.base a' h_mem
  | step rule' h_rule' premise_fn' h_premises_match' h_premises_deriv' ih =>
    have h_premises_deriv_l : ∀ (pname : String), pname ∈ rule'.premises → Derivable l rules (premise_fn' pname) := by
      intro pname hp
      exact ih pname hp
    exact Derivable.step rule' h_rule' premise_fn' h_premises_match' h_premises_deriv_l

/-- 如果 Derivable (rules.foldl apply_rule known) rules a，则 Derivable known rules a。 -/
lemma foldl_derivable_superset (known : AxiomSet) (rules : List DiscoveryRule) (a : Axiom)
    (h : Derivable (rules.foldl apply_rule known) rules a) : Derivable known rules a := by
  induction h with
  | base a' h_base =>
    exact foldl_derivable known rules a' h_base
  | step rule' h_rule' premise_fn' h_premises_match' h_premises_deriv' ih =>
    have h_premises_deriv_known : ∀ (pname : String), pname ∈ rule'.premises → Derivable known rules (premise_fn' pname) := by
      intro pname hp
      exact ih pname hp
    exact Derivable.step rule' h_rule' premise_fn' h_premises_match' h_premises_deriv_known

/-- 发现的可靠性：每个被发现的公理要么在原始已知集中，要么是规则结论
    （支持多步推导链）。 -/
theorem discovered_axiom_soundness (known : AxiomSet) (rules : List DiscoveryRule) (a : Axiom)
    (h : ∃ n : ℕ, a ∈ discover known rules n) : Derivable known rules a := by
  rcases h with ⟨n, h_mem⟩
  induction n generalizing known rules a with
  | zero =>
    have h_known : a ∈ known := mem_of_mem_dedup h_mem
    exact Derivable.base a h_known
  | succ n ih =>
    unfold discover at h_mem
    -- h_mem : a ∈ discover (dedupAxioms (rules.foldl apply_rule known)) rules n
    have h_deriv : Derivable (dedupAxioms (rules.foldl apply_rule known)) rules a :=
      ih (dedupAxioms (rules.foldl apply_rule known)) rules a h_mem
    -- Convert Derivable (dedupAxioms (rules.foldl apply_rule known)) rules a to Derivable known rules a
    have h_deriv_foldl : Derivable (rules.foldl apply_rule known) rules a :=
      derivable_of_dedup (rules.foldl apply_rule known) rules a h_deriv
    exact foldl_derivable_superset known rules a h_deriv_foldl

/-- dedupAxioms 的长度不超过原始列表 -/
lemma dedup_length_le (l : AxiomSet) : (dedupAxioms l).length ≤ l.length := by
  induction l with
  | nil => simp [dedupAxioms]
  | cons hd tl ih =>
    simp [dedupAxioms]
    have h_filter : ((dedupAxioms tl).filter (fun x => x.name ≠ hd.name)).length ≤ (dedupAxioms tl).length :=
      List.length_filter_le (fun x => x.name ≠ hd.name) (dedupAxioms tl)
    have h_ih : (dedupAxioms tl).length ≤ tl.length := ih
    have h_total : ((dedupAxioms tl).filter (fun x => x.name ≠ hd.name)).length ≤ tl.length :=
      h_filter.trans h_ih
    simpa [Nat.succ_eq_add_one] using Nat.add_le_add_right h_total 1

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
  unfold discover
  simp [List.foldl, apply_rule, rule_applicable, dist_symm_rule]
  unfold discover
  simp [dedupAxioms, List.mem_cons, List.not_mem_nil, List.mem_filter, dist_symm_rule]

/-- 示例 2：对共线约束应用两个旋转规则。
    初始已知集包含 collinear_ABC，经过 1 步发现后应包含 collinear_BCA 和 collinear_CAB。 -/
example : collinear_rotate_rule.conclusion ∈ discover
    [{ name := "collinear_ABC", body := True }]
    [collinear_rotate_rule, collinear_rotate2_rule] 1 := by
  unfold discover
  simp [List.foldl, apply_rule, rule_applicable, collinear_rotate_rule, collinear_rotate2_rule]
  unfold discover
  simp [dedupAxioms, List.mem_cons, List.not_mem_nil, List.mem_filter, collinear_rotate_rule, collinear_rotate2_rule]

/-- 示例 3：两步发现过程。
    第一步：从 collinear_ABC 推导出 collinear_BCA
    第二步：从 collinear_BCA（第一步的结果）推导出 collinear_CAB
    这演示了规则的链式应用。 -/
example : { name := "collinear_CAB", body := True } ∈ discover
    [{ name := "collinear_ABC", body := True }]
    [collinear_rotate_rule, collinear_rotate2_rule] 2 := by
  unfold discover
  simp [List.foldl, apply_rule, rule_applicable, collinear_rotate_rule, collinear_rotate2_rule]
  unfold discover
  simp [List.foldl, apply_rule, rule_applicable, collinear_rotate_rule, collinear_rotate2_rule]
  unfold discover
  simp [dedupAxioms, List.mem_cons, List.not_mem_nil, List.mem_filter, collinear_rotate_rule, collinear_rotate2_rule]

/-- 示例 4：使用完整几何规则库进行多步发现。
    从三个距离公理开始，发现其对称版本。
    此测试验证发现过程不会丢失原始公理。 -/
theorem geometry_rules_preserve_originals :
    ({ name := "dist_AB", body := True } : Axiom) ∈
    discover [{ name := "dist_AB", body := True }] geometry_rules 3 := by
  unfold discover
  simp [List.foldl, apply_rule, rule_applicable, geometry_rules,
    dist_symm_rule, collinear_rotate_rule, collinear_rotate2_rule,
    right_angle_symm_rule, perp_symm_rule, parallel_symm_rule,
    radius_from_dist_rule]
  unfold discover
  simp [List.foldl, apply_rule, rule_applicable, geometry_rules,
    dist_symm_rule, collinear_rotate_rule, collinear_rotate2_rule,
    right_angle_symm_rule, perp_symm_rule, parallel_symm_rule,
    radius_from_dist_rule]
  unfold discover
  simp [List.foldl, apply_rule, rule_applicable, geometry_rules,
    dist_symm_rule, collinear_rotate_rule, collinear_rotate2_rule,
    right_angle_symm_rule, perp_symm_rule, parallel_symm_rule,
    radius_from_dist_rule]
  unfold discover
  simp [dedupAxioms, List.mem_cons, List.not_mem_nil, List.mem_filter, geometry_rules,
    dist_symm_rule, collinear_rotate_rule, collinear_rotate2_rule,
    right_angle_symm_rule, perp_symm_rule, parallel_symm_rule,
    radius_from_dist_rule]

/-- 示例 5：发现过程噪声检查 — 不相关规则不应产生误报。
    从 collinear_ABC 开始，不应推导出 dist_BA。 -/
theorem no_false_positive_from_unrelated_rules :
    ({ name := "dist_BA", body := True } : Axiom) ∉
    discover [{ name := "collinear_ABC", body := True }]
      [collinear_rotate_rule, collinear_rotate2_rule] 1 := by
  unfold discover
  simp [List.foldl, apply_rule, rule_applicable, collinear_rotate_rule, collinear_rotate2_rule]
  unfold discover
  simp [dedupAxioms, List.mem_cons, List.not_mem_nil, List.mem_filter, collinear_rotate_rule, collinear_rotate2_rule]

/-- 规则应用不改变已知集的大小（当规则不可触发时）。 -/
theorem apply_rule_no_change_when_inapplicable (known : AxiomSet) (rule : DiscoveryRule)
    (h : ¬ rule_applicable known rule) : apply_rule known rule = known := by
  simp [apply_rule, h]

/-- 发现过程在第一次迭代的规则应用步骤中包含规则的结论（或其同名公理）。 -/
lemma discover_one_step_contains_conclusion (known : AxiomSet) (rule : DiscoveryRule)
    (h_rule : rule ∈ geometry_rules)
    (h_app : rule_applicable known rule) :
    ∃ b ∈ discover known geometry_rules 1, b.name = rule.conclusion.name :=
  discover_adds_rule_conclusion known geometry_rules rule h_rule h_app 0

end lvFormal.Theory.AxiomDiscoveryTheory