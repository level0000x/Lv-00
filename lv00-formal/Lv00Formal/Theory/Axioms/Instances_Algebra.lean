import Lv00Formal.Theory.Axioms.Instances_Core
open Lv00Formal.Theory.Axioms.Instances
open Lv00Formal.Theory.Axioms.RuleTemplate

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace Instances

/-! ## Group Theory 公理包实例

以下内容对照 `group_theory.lvz` 与 `test_axiom_group_theory.c`：
- 34 个模板（4 核心公理 + 8 初等推论 + 4 核心构造器 + 18 导出构造器）；
- 7 个不可构造问题（全部 green_verified=true）；
- bottom_geometry = group_theory_abstract；
- negation_encoding = classical_equality；
- contradiction_behavior = explosion_principle。
-/

/-- Group Theory 包中的 34 个模板。 -/
def groupTheoryTemplates : List PackageTemplate :=
  [ -- Core Axioms (4)
    { name := "closure", paramCount := 2, group := "core_axiom" },
    { name := "associativity", paramCount := 3, group := "core_axiom" },
    { name := "identity", paramCount := 1, group := "core_axiom" },
    { name := "inverse", paramCount := 1, group := "core_axiom" },
    -- Elementary Consequences (8)
    { name := "identity_uniqueness", paramCount := 0, group := "elementary" },
    { name := "inverse_uniqueness", paramCount := 1, group := "elementary" },
    { name := "left_cancellation", paramCount := 3, group := "elementary" },
    { name := "right_cancellation", paramCount := 3, group := "elementary" },
    { name := "double_inverse", paramCount := 1, group := "elementary" },
    { name := "inverse_of_product", paramCount := 2, group := "elementary" },
    { name := "identity_is_own_inverse", paramCount := 0, group := "elementary" },
    { name := "product_with_identity", paramCount := 1, group := "elementary" },
    -- Core Constructors (4)
    { name := "multiply", paramCount := 2, group := "core_constructor" },
    { name := "power_positive", paramCount := 2, group := "core_constructor" },
    { name := "power_negative", paramCount := 2, group := "core_constructor" },
    { name := "power_zero", paramCount := 1, group := "core_constructor" },
    -- Derived Constructors (18)
    { name := "commutator", paramCount := 2, group := "derived" },
    { name := "conjugation", paramCount := 2, group := "derived" },
    { name := "element_order", paramCount := 1, group := "derived" },
    { name := "center", paramCount := 0, group := "derived" },
    { name := "centralizer", paramCount := 1, group := "derived" },
    { name := "subgroup_test", paramCount := 2, group := "derived" },
    { name := "left_coset", paramCount := 2, group := "derived" },
    { name := "right_coset", paramCount := 2, group := "derived" },
    { name := "normal_subgroup_test", paramCount := 2, group := "derived" },
    { name := "quotient_group", paramCount := 2, group := "derived" },
    { name := "direct_product", paramCount := 2, group := "derived" },
    { name := "free_group", paramCount := 1, group := "derived" },
    { name := "abelianization", paramCount := 1, group := "derived" },
    { name := "commutator_subgroup", paramCount := 1, group := "derived" },
    { name := "homomorphism", paramCount := 3, group := "derived" },
    { name := "kernel", paramCount := 1, group := "derived" },
    { name := "image", paramCount := 1, group := "derived" },
    { name := "first_isomorphism_theorem", paramCount := 1, group := "derived" }
  ]

/-- Group Theory 模板数量。 -/
theorem groupTheoryTemplates_length : groupTheoryTemplates.length = 34 := by
  decide

/-- Group Theory 包中的 7 个不可构造问题。 -/
def groupTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "word_problem", reducesTo := "undecidable",
      dependencies := ["closure", "associativity", "inverse", "multiply"],
      externalRef := "https://en.wikipedia.org/wiki/Word_problem_for_groups", greenVerified := true },
    { name := "conjugacy_problem", reducesTo := "undecidable",
      dependencies := ["closure", "associativity", "inverse", "conjugation", "multiply"],
      externalRef := "https://en.wikipedia.org/wiki/Conjugacy_problem", greenVerified := true },
    { name := "group_isomorphism_problem", reducesTo := "undecidable",
      dependencies := ["homomorphism", "kernel", "image", "multiply", "inverse"],
      externalRef := "https://en.wikipedia.org/wiki/Group_isomorphism_problem", greenVerified := true },
    { name := "triviality_problem", reducesTo := "undecidable",
      dependencies := ["identity", "inverse", "multiply", "closure"],
      externalRef := "https://en.wikipedia.org/wiki/Adian%E2%80%93Rabin_theorem", greenVerified := true },
    { name := "finiteness_problem", reducesTo := "undecidable",
      dependencies := ["identity", "inverse", "multiply", "associativity", "closure"],
      externalRef := "https://en.wikipedia.org/wiki/Adian%E2%80%93Rabin_theorem", greenVerified := true },
    { name := "simple_group_recognition", reducesTo := "undecidable",
      dependencies := ["identity", "inverse", "multiply", "associativity", "closure"],
      externalRef := "https://en.wikipedia.org/wiki/Adian%E2%80%93Rabin_theorem", greenVerified := true },
    { name := "torsion_freeness_problem", reducesTo := "undecidable",
      dependencies := ["identity", "inverse", "multiply", "associativity", "closure"],
      externalRef := "https://en.wikipedia.org/wiki/Adian%E2%80%93Rabin_theorem", greenVerified := true }
  ]

/-- Group Theory 不可构造问题数量。 -/
theorem groupTheoryUnconstructibles_length : groupTheoryUnconstructibles.length = 7 := by
  decide

/-- Group Theory 公理包实例。 -/
def groupTheoryPackage : AxiomPackageInstance :=
  { name := "group_theory",
    version := "1.0.0",
    templates := groupTheoryTemplates,
    unconstructibles := groupTheoryUnconstructibles,
    bottomGeometry := "group_theory_abstract",
    negationEncoding := "classical_equality",
    contradictionBehavior := "explosion_principle" }

/-- 由全部 Group Theory 模板生成的规则实例。 -/
def groupTheoryExecutableRules : List ExecutableRule :=
  groupTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

/-- 生成的 Group Theory 规则实例数量与模板数量一致。 -/
theorem groupTheoryExecutableRules_length : groupTheoryExecutableRules.length = 34 := by
  decide

/-! ## ZFC Set Theory 公理包实例

以下内容对照 `zfc_set_theory.lvz` 与 `test_axiom_zfc_set_theory.c`：
- 27 个模板（8 组：存在与构造、结构与正则性、公理模式、选择公理、核心构造器、良基性与归纳、关系与函数、基数与序数）；
- 10 个不可构造问题（全部 green_verified=true）；
- bottom_geometry = zfc_cumulative_hierarchy；
- negation_encoding = classical_complement_in_set_universe；
- contradiction_behavior = explosion_principle。
-/

/-- ZFC Set Theory 包中的 27 个模板。 -/
def zfcSetTheoryTemplates : List PackageTemplate :=
  [ -- Group I: Existence & Construction Axioms (5)
    { name := "extensionality", paramCount := 2, group := "existence_construction" },
    { name := "pairing", paramCount := 2, group := "existence_construction" },
    { name := "union", paramCount := 1, group := "existence_construction" },
    { name := "power_set", paramCount := 1, group := "existence_construction" },
    { name := "infinity", paramCount := 0, group := "existence_construction" },
    -- Group II: Structural & Regularity (1)
    { name := "regularity", paramCount := 1, group := "structural" },
    -- Group III: Axiom Schemas (2)
    { name := "specification", paramCount := 3, group := "axiom_schema" },
    { name := "replacement", paramCount := 3, group := "axiom_schema" },
    -- Group IV: Axiom of Choice (1)
    { name := "choice", paramCount := 1, group := "choice" },
    -- Group V: Core Constructors (10)
    { name := "empty_set", paramCount := 0, group := "core_constructor" },
    { name := "singleton", paramCount := 1, group := "core_constructor" },
    { name := "ordered_pair", paramCount := 2, group := "core_constructor" },
    { name := "cartesian_product", paramCount := 2, group := "core_constructor" },
    { name := "binary_union", paramCount := 2, group := "core_constructor" },
    { name := "binary_intersection", paramCount := 2, group := "core_constructor" },
    { name := "set_difference", paramCount := 2, group := "core_constructor" },
    { name := "big_intersection", paramCount := 1, group := "core_constructor" },
    { name := "subset_relation", paramCount := 2, group := "core_constructor" },
    -- Group VI: Well-Foundedness & Induction (4)
    { name := "epsilon_induction", paramCount := 2, group := "well_foundedness" },
    { name := "transitive_closure", paramCount := 1, group := "well_foundedness" },
    { name := "ordinal_successor", paramCount := 1, group := "well_foundedness" },
    -- Group VII: Relation & Function Constructors (5)
    { name := "relation", paramCount := 3, group := "relation_function" },
    { name := "function", paramCount := 3, group := "relation_function" },
    { name := "function_application", paramCount := 2, group := "relation_function" },
    { name := "image", paramCount := 2, group := "relation_function" },
    { name := "inverse_image", paramCount := 2, group := "relation_function" },
    -- Group VIII: Cardinal & Ordinal Constructors (3)
    { name := "equinumerous", paramCount := 2, group := "cardinal_ordinal" },
    { name := "cardinality", paramCount := 1, group := "cardinal_ordinal" },
    { name := "well_order", paramCount := 2, group := "cardinal_ordinal" }
  ]

/-- ZFC Set Theory 模板数量。 -/
theorem zfcSetTheoryTemplates_length : zfcSetTheoryTemplates.length = 29 := by
  decide

/-- ZFC Set Theory 包中的 10 个不可构造问题。 -/
def zfcSetTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "continuum_hypothesis", reducesTo := "independent_of_ZFC",
      dependencies := ["infinity", "power_set", "cardinality", "choice"],
      externalRef := "https://en.wikipedia.org/wiki/Continuum_hypothesis", greenVerified := true },
    { name := "generalized_continuum_hypothesis", reducesTo := "independent_of_ZFC",
      dependencies := ["continuum_hypothesis", "cardinality", "replacement"],
      externalRef := "https://en.wikipedia.org/wiki/Continuum_hypothesis#Generalized_continuum_hypothesis", greenVerified := true },
    { name := "axiom_of_choice_independence", reducesTo := "independent_of_ZF",
      dependencies := ["choice", "well_order"],
      externalRef := "https://en.wikipedia.org/wiki/Axiom_of_choice#Independence", greenVerified := true },
    { name := "inaccessible_cardinal_existence", reducesTo := "equiconsistent_with_ZFC",
      dependencies := ["cardinality", "power_set", "regularity"],
      externalRef := "https://en.wikipedia.org/wiki/Inaccessible_cardinal", greenVerified := true },
    { name := "suslin_hypothesis", reducesTo := "independent_of_ZFC",
      dependencies := ["well_order", "cartesian_product", "infinity"],
      externalRef := "https://en.wikipedia.org/wiki/Suslin%27s_problem", greenVerified := true },
    { name := "whitehead_problem", reducesTo := "independent_of_ZFC",
      dependencies := ["function", "cardinality", "choice"],
      externalRef := "https://en.wikipedia.org/wiki/Whitehead_problem", greenVerified := true },
    { name := "zfc_consistency", reducesTo := "unprovable_in_ZFC",
      dependencies := ["specification", "replacement", "infinity"],
      externalRef := "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems", greenVerified := true },
    { name := "measurable_cardinal_existence", reducesTo := "transcends_ZFC",
      dependencies := ["cardinality", "power_set", "choice"],
      externalRef := "https://en.wikipedia.org/wiki/Measurable_cardinal", greenVerified := true },
    { name := "axiom_of_constructibility", reducesTo := "independent_of_ZFC",
      dependencies := ["power_set", "replacement", "infinity"],
      externalRef := "https://en.wikipedia.org/wiki/Axiom_of_constructibility", greenVerified := true },
    { name := "martins_axiom", reducesTo := "independent_of_ZFC",
      dependencies := ["choice", "continuum_hypothesis", "cardinality"],
      externalRef := "https://en.wikipedia.org/wiki/Martin%27s_axiom", greenVerified := true }
  ]

/-- ZFC Set Theory 不可构造问题数量。 -/
theorem zfcSetTheoryUnconstructibles_length : zfcSetTheoryUnconstructibles.length = 10 := by
  decide

/-- ZFC Set Theory 公理包实例。 -/
def zfcSetTheoryPackage : AxiomPackageInstance :=
  { name := "zfc_set_theory",
    version := "1.0.0",
    templates := zfcSetTheoryTemplates,
    unconstructibles := zfcSetTheoryUnconstructibles,
    bottomGeometry := "zfc_cumulative_hierarchy",
    negationEncoding := "classical_complement_in_set_universe",
    contradictionBehavior := "explosion_principle" }

/-- 由全部 ZFC Set Theory 模板生成的规则实例。 -/
def zfcSetTheoryExecutableRules : List ExecutableRule :=
  zfcSetTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

/-- 生成的 ZFC Set Theory 规则实例数量与模板数量一致。 -/
theorem zfcSetTheoryExecutableRules_length : zfcSetTheoryExecutableRules.length = 29 := by
  decide

/-! ## Boolean Algebra 公理包实例 -/

/-- Boolean Algebra 包中的 29 个模板。 -/
def booleanAlgebraTemplates : List PackageTemplate :=
  [ { name := "join_associativity", paramCount := 3, group := "lattice" },
    { name := "meet_associativity", paramCount := 3, group := "lattice" },
    { name := "join_commutativity", paramCount := 2, group := "lattice" },
    { name := "meet_commutativity", paramCount := 2, group := "lattice" },
    { name := "join_absorption", paramCount := 2, group := "lattice" },
    { name := "meet_absorption", paramCount := 2, group := "lattice" },
    { name := "join_identity", paramCount := 1, group := "identity" },
    { name := "meet_identity", paramCount := 1, group := "identity" },
    { name := "meet_distributes_over_join", paramCount := 3, group := "distributive" },
    { name := "join_distributes_over_meet", paramCount := 3, group := "distributive" },
    { name := "complement_join", paramCount := 1, group := "complement" },
    { name := "complement_meet", paramCount := 1, group := "complement" },
    { name := "huntington_equation", paramCount := 2, group := "huntington" },
    { name := "complement", paramCount := 1, group := "constructor" },
    { name := "meet", paramCount := 2, group := "constructor" },
    { name := "join", paramCount := 2, group := "constructor" },
    { name := "double_negation", paramCount := 1, group := "derived" },
    { name := "de_morgan_join", paramCount := 2, group := "derived" },
    { name := "de_morgan_meet", paramCount := 2, group := "derived" },
    { name := "join_idempotence", paramCount := 1, group := "derived" },
    { name := "meet_idempotence", paramCount := 1, group := "derived" },
    { name := "join_bounded_top", paramCount := 1, group := "derived" },
    { name := "meet_bounded_bottom", paramCount := 1, group := "derived" },
    { name := "consensus", paramCount := 3, group := "derived" },
    { name := "sheffer_stroke", paramCount := 2, group := "derived" },
    { name := "peirce_arrow", paramCount := 2, group := "derived" },
    { name := "material_implication", paramCount := 2, group := "derived" },
    { name := "exclusive_or", paramCount := 2, group := "derived" },
    { name := "biconditional", paramCount := 2, group := "derived" } ]

theorem booleanAlgebraTemplates_length : booleanAlgebraTemplates.length = 29 := by
  decide

/-- Boolean Algebra 包中的 6 个不可构造问题。 -/
def booleanAlgebraUnconstructibles : List UnconstructibleProblem :=
  [ { name := "boolean_satisfiability", reducesTo := "NP_complete", dependencies := ["join", "meet", "complement"], externalRef := "https://en.wikipedia.org/wiki/Boolean_satisfiability_problem", greenVerified := true },
    { name := "tautology_checking", reducesTo := "coNP_complete", dependencies := ["join", "meet", "complement"], externalRef := "https://en.wikipedia.org/wiki/Tautology_(logic)", greenVerified := true },
    { name := "boolean_equivalence_checking", reducesTo := "coNP_complete", dependencies := ["join", "meet", "complement"], externalRef := "https://en.wikipedia.org/wiki/Boolean_algebra_(structure)", greenVerified := true },
    { name := "boolean_formula_minimization", reducesTo := "NP_hard", dependencies := ["join", "meet", "complement", "consensus"], externalRef := "https://en.wikipedia.org/wiki/Boolean_satisfiability_problem", greenVerified := true },
    { name := "minimal_circuit_synthesis", reducesTo := "NP_hard", dependencies := ["join", "meet", "complement", "sheffer_stroke"], externalRef := "https://en.wikipedia.org/wiki/Circuit_complexity", greenVerified := true },
    { name := "equational_theory_with_subalgebra", reducesTo := "undecidable", dependencies := ["join", "meet", "complement"], externalRef := "https://plato.stanford.edu/entries/boolalg-math/#decid", greenVerified := true } ]

theorem booleanAlgebraUnconstructibles_length : booleanAlgebraUnconstructibles.length = 6 := by
  decide

def booleanAlgebraPackage : AxiomPackageInstance :=
  { name := "boolean_algebra", version := "1.0.0", templates := booleanAlgebraTemplates,
    unconstructibles := booleanAlgebraUnconstructibles, bottomGeometry := "boolean_algebra_2element",
    negationEncoding := "classical_complement", contradictionBehavior := "explosion_principle" }

def booleanAlgebraExecutableRules : List ExecutableRule :=
  booleanAlgebraTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem booleanAlgebraExecutableRules_length : booleanAlgebraExecutableRules.length = 29 := by
  decide

/-! ## Ring Theory 公理包实例 -/

/-- Ring Theory 包中的 54 个模板。 -/
def ringTheoryTemplateNamesRaw : List String :=
  ["additive_closure", "additive_associativity", "additive_identity", "additive_inverse", "additive_commutativity",
   "multiplicative_closure", "multiplicative_associativity", "multiplicative_identity", "left_distributivity", "right_distributivity",
   "additive_identity_uniqueness", "additive_inverse_uniqueness", "multiplicative_identity_uniqueness", "zero_multiplication", "negative_multiplication", "negative_negative_product", "zero_ring_condition", "additive_cancellation", "double_additive_inverse", "negative_of_sum", "zero_is_own_add_inverse", "negative_one_times",
   "add", "multiply", "negate", "zero", "one", "subtract",
   "characteristic", "power_positive", "scalar_multiple", "binomial_theorem", "unit", "multiplicative_inverse", "zero_divisor", "nilpotent", "idempotent", "subring_test", "left_ideal", "right_ideal", "two_sided_ideal", "principal_ideal", "quotient_ring", "homomorphism", "kernel", "image", "first_isomorphism_theorem", "direct_product", "polynomial_ring", "matrix_ring", "commutator", "center", "unit_group", "jacobson_radical"]

def ringTheoryTemplates : List PackageTemplate :=
  ringTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "ring_theory" })

theorem ringTheoryTemplates_length : ringTheoryTemplates.length = 54 := by
  decide

/-- Ring Theory 包中的 8 个不可构造问题。 -/
def ringTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "hilberts_tenth_problem", reducesTo := "undecidable", dependencies := ["add", "multiply", "additive_identity", "multiplicative_identity", "power_positive"], externalRef := "https://en.wikipedia.org/wiki/Hilbert%27s_tenth_problem", greenVerified := true },
    { name := "word_problem_for_rings", reducesTo := "undecidable", dependencies := ["additive_closure", "additive_associativity", "additive_inverse", "multiplicative_closure", "multiplicative_associativity", "left_distributivity", "right_distributivity", "additive_identity", "multiplicative_identity"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_for_groups", greenVerified := true },
    { name := "ring_isomorphism_problem", reducesTo := "undecidable", dependencies := ["homomorphism", "kernel", "image", "add", "multiply", "additive_inverse", "multiplicative_identity"], externalRef := "https://en.wikipedia.org/wiki/Ring_isomorphism", greenVerified := true },
    { name := "triviality_problem_rings", reducesTo := "undecidable", dependencies := ["zero", "one", "additive_identity", "multiplicative_identity", "zero_ring_condition"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_for_groups", greenVerified := true },
    { name := "zero_divisor_recognition", reducesTo := "undecidable", dependencies := ["zero_divisor", "multiply", "zero_multiplication", "multiplicative_identity", "additive_identity"], externalRef := "https://en.wikipedia.org/wiki/Zero_divisor", greenVerified := true },
    { name := "nilpotent_element_recognition", reducesTo := "undecidable", dependencies := ["nilpotent", "multiply", "power_positive", "zero_multiplication"], externalRef := "https://en.wikipedia.org/wiki/Nilpotent", greenVerified := true },
    { name := "commutativity_recognition", reducesTo := "undecidable", dependencies := ["commutator", "multiply", "additive_commutativity", "multiplicative_associativity"], externalRef := "https://en.wikipedia.org/wiki/Commutative_ring", greenVerified := true },
    { name := "ideal_membership_unrestricted", reducesTo := "undecidable", dependencies := ["left_ideal", "right_ideal", "two_sided_ideal", "multiply", "add", "additive_inverse", "additive_identity"], externalRef := "https://en.wikipedia.org/wiki/Ideal_(ring_theory)", greenVerified := true } ]

theorem ringTheoryUnconstructibles_length : ringTheoryUnconstructibles.length = 8 := by
  decide

def ringTheoryPackage : AxiomPackageInstance :=
  { name := "ring_theory", version := "1.0.0", templates := ringTheoryTemplates,
    unconstructibles := ringTheoryUnconstructibles, bottomGeometry := "ring_theory_abstract",
    negationEncoding := "classical_equality", contradictionBehavior := "explosion_principle" }

def ringTheoryExecutableRules : List ExecutableRule :=
  ringTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem ringTheoryExecutableRules_length : ringTheoryExecutableRules.length = 54 := by
  decide

/-! ## Peano Arithmetic 公理包实例 -/

/-- Peano Arithmetic 包中的 70 个模板。 -/
def peanoArithmeticTemplateNamesRaw : List String :=
  ["zero_not_successor", "successor_injective", "add_zero_left", "add_successor_right", "add_zero_right", "mul_zero", "mul_successor_right", "induction_schema", "induction_on_addition", "induction_on_multiplication", "strong_induction", "less_than_definition", "less_than_irreflexive", "less_than_transitive", "less_than_total", "zero_is_least", "no_largest_element", "successor_not_equal", "successor_distinct", "addition_commutative", "addition_associative", "addition_cancellative", "multiplication_commutative", "multiplication_associative", "distributivity_left", "distributivity_right", "mul_identity_right", "mul_identity_left", "mul_zero_commutes", "no_zero_divisors", "order_add_right", "order_add_left", "order_mul_positive", "exp_zero", "exp_successor", "exp_addition_law", "exp_multiplication_law", "exp_power_law", "divisibility_definition", "division_algorithm", "euclidean_gcd", "bezout_identity", "prime_definition", "unique_prime_factorization", "infinitude_of_primes", "euclid_lemma", "successor", "predecessor", "add", "subtract_truncated", "multiply", "exponentiate", "less_than_compare", "less_or_equal_compare", "equality_compare", "maximum", "minimum", "factorial", "quotient", "remainder", "divisibility_test", "gcd", "lcm", "primality_test", "next_prime", "prime_factorization", "beta_function_encode", "beta_function_decode", "bounded_forall", "bounded_exists"]

def peanoArithmeticTemplates : List PackageTemplate :=
  peanoArithmeticTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "peano_arithmetic" })

theorem peanoArithmeticTemplates_length : peanoArithmeticTemplates.length = 70 := by
  decide

/-- Peano Arithmetic 包中的 8 个不可构造问题。 -/
def peanoArithmeticUnconstructibles : List UnconstructibleProblem :=
  [ { name := "godel_sentence", reducesTo := "godel_first_incompleteness", dependencies := ["successor", "add", "multiply", "induction_schema", "beta_function_encode", "beta_function_decode"], externalRef := "https://en.wikipedia.org/wiki/G%C3%B6del%27s_incompleteness_theorems", greenVerified := true },
    { name := "consistency_of_PA", reducesTo := "godel_second_incompleteness", dependencies := ["successor", "add", "multiply", "induction_schema", "beta_function_encode"], externalRef := "https://en.wikipedia.org/wiki/G%C3%B6del%27s_second_incompleteness_theorem", greenVerified := true },
    { name := "goodstein_theorem", reducesTo := "transfinite_induction_up_to_epsilon_0", dependencies := ["successor", "add", "multiply", "exponentiate", "induction_schema"], externalRef := "https://en.wikipedia.org/wiki/Goodstein%27s_theorem", greenVerified := true },
    { name := "paris_harrington_principle", reducesTo := "independence_from_PA", dependencies := ["successor", "add", "multiply", "induction_schema", "beta_function_encode"], externalRef := "https://en.wikipedia.org/wiki/Paris%E2%80%93Harrington_theorem", greenVerified := true },
    { name := "kirby_paris_hydra", reducesTo := "transfinite_induction_up_to_epsilon_0", dependencies := ["successor", "add", "multiply", "induction_schema"], externalRef := "https://en.wikipedia.org/wiki/Hydra_game", greenVerified := true },
    { name := "truth_predicate_for_PA", reducesTo := "tarski_undefinability", dependencies := ["successor", "add", "multiply", "induction_schema", "beta_function_encode"], externalRef := "https://en.wikipedia.org/wiki/Tarski%27s_undefinability_theorem", greenVerified := true },
    { name := "halting_problem_for_PA", reducesTo := "turing_halting_problem", dependencies := ["successor", "add", "multiply", "induction_schema", "beta_function_encode"], externalRef := "https://en.wikipedia.org/wiki/Halting_problem", greenVerified := true },
    { name := "epsilon_0_consistency", reducesTo := "gentzen_consistency_proof", dependencies := ["successor", "add", "multiply", "induction_schema", "strong_induction"], externalRef := "https://en.wikipedia.org/wiki/Epsilon_numbers_(mathematics)", greenVerified := true } ]

theorem peanoArithmeticUnconstructibles_length : peanoArithmeticUnconstructibles.length = 8 := by
  decide

def peanoArithmeticPackage : AxiomPackageInstance :=
  { name := "peano_arithmetic", version := "1.0.0", templates := peanoArithmeticTemplates,
    unconstructibles := peanoArithmeticUnconstructibles, bottomGeometry := "peano_arithmetic_discrete",
    negationEncoding := "classical_first_order_logic", contradictionBehavior := "explosion_principle" }

def peanoArithmeticExecutableRules : List ExecutableRule :=
  peanoArithmeticTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem peanoArithmeticExecutableRules_length : peanoArithmeticExecutableRules.length = 70 := by
  decide


end Instances
end Axioms
end Theory
end Lv00Formal
