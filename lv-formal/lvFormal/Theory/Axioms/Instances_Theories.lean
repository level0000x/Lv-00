import lvFormal.Theory.Axioms.Instances_Core
open lvFormal.Theory.Axioms.Instances
open lvFormal.Theory.Axioms.RuleTemplate

namespace lvFormal
namespace Theory
namespace Axioms
namespace Instances

/-! ## Lattice Theory 公理包实例 -/

def latticeTheoryTemplateNamesRaw : List String :=
  ["meet_idempotence", "meet_commutativity", "meet_associativity",
   "join_idempotence", "join_commutativity", "join_associativity",
   "absorption_join_over_meet", "absorption_meet_over_join",
   "bottom_identity", "top_identity",
   "meet_distributes_over_join", "join_distributes_over_meet",
   "modular_law", "complement_existence",
   "meet", "join", "leq_from_meet", "leq_from_join", "top_element", "bottom_element", "complement",
   "strict_less_than", "incomparable", "covering_relation", "meet_irreducible", "join_irreducible", "is_atom", "is_coatom",
   "sublattice_test", "lattice_homomorphism_test", "lattice_isomorphism_test", "direct_product", "dual_lattice", "interval_sublattice",
   "ideal", "filter", "prime_ideal_test", "prime_filter_test", "maximal_ideal_test", "congruence_relation_test", "quotient_lattice",
   "is_distributive", "is_modular", "is_complemented", "is_boolean_algebra", "is_heyting_algebra", "is_complete", "is_chain",
   "whitman_condition", "dedekind_macneille_completion", "birkhoff_representation", "stone_representation", "knaster_tarski_fixed_point"]

def latticeTheoryTemplates : List PackageTemplate :=
  latticeTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "lattice_theory" })

theorem latticeTheoryTemplates_length : latticeTheoryTemplates.length = 53 := by
  decide

def latticeTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "lattice_variety_membership", reducesTo := "equational_theory_undecidability", dependencies := ["meet", "join", "meet_associativity", "join_associativity"], externalRef := "https://en.wikipedia.org/wiki/Lattice_(order)", greenVerified := true },
    { name := "finite_lattice_embeddability", reducesTo := "finite_representation_problem", dependencies := ["meet", "join", "sublattice_test"], externalRef := "https://en.wikipedia.org/wiki/Lattice_(order)", greenVerified := true },
    { name := "congruence_lattice_problem", reducesTo := "universal_algebra_undecidability", dependencies := ["congruence_relation_test", "quotient_lattice", "ideal"], externalRef := "https://en.wikipedia.org/wiki/Lattice_(order)", greenVerified := true },
    { name := "free_lattice_word_problem", reducesTo := "exp_space_hardness", dependencies := ["meet", "join", "whitman_condition"], externalRef := "https://en.wikipedia.org/wiki/Free_lattice", greenVerified := true },
    { name := "lattice_isomorphism_problem", reducesTo := "graph_isomorphism_hardness", dependencies := ["lattice_isomorphism_test", "lattice_homomorphism_test"], externalRef := "https://en.wikipedia.org/wiki/Lattice_(order)", greenVerified := true },
    { name := "equational_basis_for_lattice_variety", reducesTo := "finite_basis_problem", dependencies := ["meet_distributes_over_join", "join_distributes_over_meet", "modular_law"], externalRef := "https://en.wikipedia.org/wiki/Lattice_(order)", greenVerified := true },
    { name := "lattice_identity_entailment", reducesTo := "equational_unification", dependencies := ["meet", "join", "absorption_join_over_meet", "absorption_meet_over_join"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_(mathematics)", greenVerified := true } ]

theorem latticeTheoryUnconstructibles_length : latticeTheoryUnconstructibles.length = 7 := by
  decide

def latticeTheoryPackage : AxiomPackageInstance :=
  { name := "lattice_theory", version := "1.0.0", templates := latticeTheoryTemplates,
    unconstructibles := latticeTheoryUnconstructibles, bottomGeometry := "lattice_partial_order",
    negationEncoding := "complement_in_complemented_lattice", contradictionBehavior := "explosion_principle" }

def latticeTheoryExecutableRules : List ExecutableRule :=
  latticeTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem latticeTheoryExecutableRules_length : latticeTheoryExecutableRules.length = 53 := by
  decide

/-! ## Lie Theory 公理包实例 -/

def lieTheoryTemplateNamesRaw : List String :=
  ["lie_algebra_bilinearity_left", "lie_algebra_bilinearity_right", "lie_algebra_alternating", "lie_algebra_anticommutativity", "jacobi_identity",
   "lie_group_structure", "lie_group_smooth_manifold", "lie_group_smooth_multiplication", "lie_group_smooth_inversion",
   "tangent_space_at_identity", "exponential_map", "exponential_local_diffeomorphism", "lie_bracket_from_commutator", "homomorphism_induces_algebra_hom",
   "skew_symmetry_derived", "jacobi_as_derivation", "baker_campbell_hausdorff", "exp_of_zero_is_identity", "exp_of_negative_is_inverse", "exponential_differential_equation", "adjoint_representation", "adjoint_is_homomorphism",
   "lie_subalgebra", "lie_ideal", "quotient_lie_algebra", "solvable_derived_series", "nilpotent_lower_central_series", "simple_lie_algebra", "semisimple_lie_algebra", "abelian_lie_algebra",
   "general_linear_algebra_gln", "special_linear_algebra_sln", "orthogonal_algebra_son", "special_orthogonal_algebra", "symplectic_algebra_sp2n", "unitary_algebra_un", "special_unitary_algebra_sun", "euclidean_algebra_se3",
   "lie_algebra_representation", "adjoint_representation_algebra", "irreducible_representation", "universal_enveloping_algebra", "poincare_birkhoff_witt", "casimir_element",
   "cartan_subalgebra", "root_of_lie_algebra", "root_system", "killing_form", "cartan_criterion_semisimple", "weyl_group",
   "lie_bracket", "exponential", "adjoint_action", "direct_sum_lie_algebra", "derived_algebra", "center_lie_algebra",
   "matrix_commutator", "cross_product_bracket", "lie_algebra_homomorphism", "lie_algebra_isomorphism", "semidirect_product_lie_algebra", "universal_covering_group", "lie_subgroup", "identity_component",
   "levi_decomposition", "ados_theorem", "lies_third_theorem", "hilberts_fifth_problem", "highest_weight_theory", "dynkin_diagram_classification"]

def lieTheoryTemplates : List PackageTemplate :=
  lieTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "lie_theory" })

theorem lieTheoryTemplates_length : lieTheoryTemplates.length = 70 := by
  decide

def lieTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "lie_algebra_isomorphism_problem", reducesTo := "group_isomorphism_problem", dependencies := ["lie_algebra_homomorphism", "lie_algebra_isomorphism"], externalRef := "https://en.wikipedia.org/wiki/Group_isomorphism_problem", greenVerified := false },
    { name := "lie_group_isomorphism_problem", reducesTo := "lie_algebra_isomorphism_problem", dependencies := ["lie_group_structure", "lie_algebra_isomorphism"], externalRef := "https://en.wikipedia.org/wiki/Lie_group", greenVerified := false },
    { name := "nilpotency_testing_infinite_dimensional", reducesTo := "word_problem_for_groups", dependencies := ["nilpotent_lower_central_series", "lie_bracket"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_for_groups", greenVerified := false },
    { name := "solvable_quotient_computation", reducesTo := "group_isomorphism_problem", dependencies := ["solvable_derived_series", "quotient_lie_algebra"], externalRef := "https://en.wikipedia.org/wiki/Solvable_group", greenVerified := false },
    { name := "lie_algebra_word_problem", reducesTo := "word_problem_for_groups", dependencies := ["lie_bracket", "exponential"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_for_groups", greenVerified := false },
    { name := "representation_equivalence_problem", reducesTo := "lie_algebra_isomorphism_problem", dependencies := ["lie_algebra_representation", "irreducible_representation"], externalRef := "https://en.wikipedia.org/wiki/Representation_theory", greenVerified := false },
    { name := "invariant_subspace_lattice", reducesTo := "lie_algebra_isomorphism_problem", dependencies := ["lie_algebra_representation", "lie_subalgebra"], externalRef := "https://en.wikipedia.org/wiki/Invariant_subspace_problem", greenVerified := false } ]

theorem lieTheoryUnconstructibles_length : lieTheoryUnconstructibles.length = 7 := by
  decide

def lieTheoryPackage : AxiomPackageInstance :=
  { name := "lie_theory", version := "1.0.0", templates := lieTheoryTemplates,
    unconstructibles := lieTheoryUnconstructibles, bottomGeometry := "lie_group_smooth_manifold_with_lie_algebra_tangent_space",
    negationEncoding := "classical_equality", contradictionBehavior := "explosion_principle" }

def lieTheoryExecutableRules : List ExecutableRule :=
  lieTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem lieTheoryExecutableRules_length : lieTheoryExecutableRules.length = 70 := by
  decide

/-! ## Model Theory 公理包实例 -/

def modelTheoryTemplateNamesRaw : List String :=
  ["first_order_language", "structure_model", "satisfaction_relation", "theory", "theory_consistency",
   "elementary_substructure", "completeness_theorem", "compactness_theorem", "downward_lowenheim_skolem", "upward_lowenheim_skolem",
   "vaught_test", "omitting_types_theorem", "interpolation_theorem",
   "ultraproduct", "elementary_chain", "model_completion",
   "prime_model", "saturated_model", "homogeneous_model",
   "complete_type", "type_space", "stability", "forking", "independence_relation", "rank",
   "algebraically_closed_field", "real_closed_field", "densely_linearly_ordered", "peano_arithmetic", "presburger_arithmetic",
   "quantifier_elimination", "model_completeness", "decidability_of_theory", "elementary_equivalence", "elementary_embedding"]

def modelTheoryTemplates : List PackageTemplate :=
  modelTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "model_theory" })

theorem modelTheoryTemplates_length : modelTheoryTemplates.length = 35 := by
  decide

def modelTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "first_order_validity", reducesTo := "undecidable", dependencies := ["first_order_language", "satisfaction_relation", "completeness_theorem"], externalRef := "https://en.wikipedia.org/wiki/Entscheidungsproblem", greenVerified := true },
    { name := "peano_arithmetic_decidability", reducesTo := "undecidable", dependencies := ["peano_arithmetic", "theory_consistency", "decidability_of_theory"], externalRef := "https://en.wikipedia.org/wiki/Peano_axioms", greenVerified := true },
    { name := "theory_isomorphism", reducesTo := "undecidable", dependencies := ["theory", "structure_model", "elementary_equivalence"], externalRef := "https://en.wikipedia.org/wiki/Model_theory", greenVerified := true },
    { name := "model_satisfiability", reducesTo := "undecidable", dependencies := ["theory", "structure_model", "satisfaction_relation", "compactness_theorem"], externalRef := "https://en.wikipedia.org/wiki/Satisfiability", greenVerified := true },
    { name := "elementary_equivalence_problem", reducesTo := "undecidable", dependencies := ["elementary_equivalence", "structure_model", "satisfaction_relation"], externalRef := "https://en.wikipedia.org/wiki/Elementary_equivalence", greenVerified := true },
    { name := "stable_theory_classification", reducesTo := "undecidable", dependencies := ["stability", "complete_type", "forking", "rank"], externalRef := "https://en.wikipedia.org/wiki/Stable_theory", greenVerified := true } ]

theorem modelTheoryUnconstructibles_length : modelTheoryUnconstructibles.length = 6 := by
  decide

def modelTheoryPackage : AxiomPackageInstance :=
  { name := "model_theory", version := "1.0.0", templates := modelTheoryTemplates,
    unconstructibles := modelTheoryUnconstructibles, bottomGeometry := "first_order_structures_models",
    negationEncoding := "classical_negation_in_logic", contradictionBehavior := "explosion_principle" }

def modelTheoryExecutableRules : List ExecutableRule :=
  modelTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem modelTheoryExecutableRules_length : modelTheoryExecutableRules.length = 35 := by
  decide

/-! ## Classical Propositional Logic 公理包实例 -/

def classicalPropositionalLogicTemplateNamesRaw : List String :=
  ["axiom_K_weakening", "axiom_S_distribution", "axiom_C_contrapositive",
   "frege_proposition_8", "frege_proposition_28", "frege_proposition_31", "frege_proposition_41",
   "RW_tautology", "RW_addition", "RW_commutation", "RW_association", "RW_distribution",
   "modus_ponens", "uniform_substitution",
   "negation", "implication", "falsum", "verum",
   "conjunction", "disjunction", "biconditional", "exclusive_or", "sheffer_stroke", "peirce_arrow",
   "hypothetical_syllogism", "modus_tollens", "disjunctive_syllogism",
   "conjunction_introduction", "conjunction_elimination_left", "conjunction_elimination_right",
   "disjunction_introduction_left", "disjunction_introduction_right", "biconditional_introduction",
   "double_negation_elimination", "double_negation_introduction", "reductio_ad_absurdum", "ex_falso_quodlibet",
   "law_of_excluded_middle", "law_of_non_contradiction", "deduction_theorem", "proof_by_cases", "contraposition",
   "exportation", "importation",
   "de_morgan_conjunction", "de_morgan_disjunction", "conjunction_idempotence", "disjunction_idempotence",
   "conjunction_commutativity", "disjunction_commutativity", "conjunction_associativity", "disjunction_associativity",
   "conjunction_distributes_over_disjunction", "disjunction_distributes_over_conjunction",
   "absorption_conjunction_disjunction", "absorption_disjunction_conjunction",
   "constructive_dilemma", "destructive_dilemma", "peirces_law"]

def classicalPropositionalLogicTemplates : List PackageTemplate :=
  classicalPropositionalLogicTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "classical_propositional_logic" })

theorem classicalPropositionalLogicTemplates_length : classicalPropositionalLogicTemplates.length = 59 := by
  decide

def classicalPropositionalLogicUnconstructibles : List UnconstructibleProblem :=
  [ { name := "propositional_satisfiability", reducesTo := "NP_complete", dependencies := ["negation", "implication", "conjunction"], externalRef := "https://en.wikipedia.org/wiki/Boolean_satisfiability_problem", greenVerified := true },
    { name := "tautology_checking", reducesTo := "coNP_complete", dependencies := ["negation", "implication", "disjunction"], externalRef := "https://en.wikipedia.org/wiki/Tautology_(logic)", greenVerified := true },
    { name := "minimal_proof_length", reducesTo := "NP_hard_approximation", dependencies := ["axiom_K_weakening", "axiom_S_distribution", "axiom_C_contrapositive", "modus_ponens"], externalRef := "https://en.wikipedia.org/wiki/Proof_theory", greenVerified := true },
    { name := "propositional_interpolation", reducesTo := "PiP2_complete", dependencies := ["negation", "implication", "conjunction"], externalRef := "https://en.wikipedia.org/wiki/Craig_interpolation", greenVerified := true },
    { name := "proof_equivalence_checking", reducesTo := "coNP_complete", dependencies := ["negation", "implication", "biconditional", "conjunction"], externalRef := "https://en.wikipedia.org/wiki/Logical_equivalence", greenVerified := true },
    { name := "shortest_implicational_proof", reducesTo := "NP_hard", dependencies := ["axiom_K_weakening", "axiom_S_distribution", "axiom_C_contrapositive", "modus_ponens"], externalRef := "https://en.wikipedia.org/wiki/Implicational_logic", greenVerified := true } ]

theorem classicalPropositionalLogicUnconstructibles_length : classicalPropositionalLogicUnconstructibles.length = 6 := by
  decide

def classicalPropositionalLogicPackage : AxiomPackageInstance :=
  { name := "classical_propositional_logic", version := "1.0.0", templates := classicalPropositionalLogicTemplates,
    unconstructibles := classicalPropositionalLogicUnconstructibles, bottomGeometry := "classical_propositional_2valued",
    negationEncoding := "material_implication_to_falsum", contradictionBehavior := "explosion_principle" }

def classicalPropositionalLogicExecutableRules : List ExecutableRule :=
  classicalPropositionalLogicTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem classicalPropositionalLogicExecutableRules_length : classicalPropositionalLogicExecutableRules.length = 59 := by
  decide

/-! ## Intuitionistic Logic 公理包实例 -/

def intuitionisticLogicTemplateNamesRaw : List String :=
  ["axiom_then_1_weakening", "axiom_then_2_distribution", "axiom_and_1_elim_left", "axiom_and_2_elim_right", "axiom_and_3_intro",
   "axiom_or_1_intro_left", "axiom_or_2_intro_right", "axiom_or_3_elim", "axiom_false_efq",
   "modus_ponens", "implication", "conjunction", "disjunction", "falsum",
   "double_negation_intro", "triple_negation_reduction", "negation_intro_schema", "verum", "identity",
   "transitivity", "conjunction_commutativity", "conjunction_associativity", "disjunction_commutativity", "disjunction_associativity",
   "conj_over_disj_distribution", "disj_over_conj_distribution", "contrapositive_constructive_intro",
   "excluded_middle_not_false", "double_negated_equivalence", "modus_tollens_constructive",
   "biconditional", "sheffer_stroke", "peirce_arrow", "exclusive_or", "non_contradiction",
   "substitution", "ch_k_combinator", "ch_s_combinator", "ch_pair", "ch_fst", "ch_snd", "ch_inl", "ch_inr", "ch_case",
   "lem_boundary_marker", "dne_boundary_marker", "peirce_boundary_marker", "classical_contrapositive_marker",
   "de_morgan_failure_marker", "wlem_boundary_marker"]

def intuitionisticLogicTemplates : List PackageTemplate :=
  intuitionisticLogicTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "intuitionistic_logic" })

theorem intuitionisticLogicTemplates_length : intuitionisticLogicTemplates.length = 50 := by
  decide

def intuitionisticLogicUnconstructibles : List UnconstructibleProblem :=
  [ { name := "law_of_excluded_middle_unconstructible", reducesTo := "lem_boundary_marker", dependencies := ["axiom_or_1_intro_left", "axiom_false_efq", "double_negation_intro"], externalRef := "https://en.wikipedia.org/wiki/Law_of_excluded_middle", greenVerified := true },
    { name := "double_negation_elim_unconstructible", reducesTo := "dne_boundary_marker", dependencies := ["axiom_false_efq", "double_negation_intro", "triple_negation_reduction"], externalRef := "https://en.wikipedia.org/wiki/Double_negation#Double_negation_elimination", greenVerified := true },
    { name := "peirce_law_unconstructible", reducesTo := "peirce_boundary_marker", dependencies := ["axiom_then_1_weakening", "axiom_then_2_distribution", "modus_ponens"], externalRef := "https://en.wikipedia.org/wiki/Peirce%27s_law", greenVerified := true },
    { name := "ipl_provability_pspace_complete", reducesTo := "sat_tautology_checking", dependencies := ["axiom_then_1_weakening", "axiom_then_2_distribution", "modus_ponens"], externalRef := "https://en.wikipedia.org/wiki/Intuitionistic_logic#Decidability", greenVerified := true },
    { name := "classical_proof_constructive_translation", reducesTo := "double_negation_translation_complexity", dependencies := ["double_negation_intro", "axiom_then_1_weakening", "axiom_then_2_distribution"], externalRef := "https://en.wikipedia.org/wiki/Double-negation_translation", greenVerified := true },
    { name := "disjunction_nondefinability", reducesTo := "connective_independence", dependencies := ["axiom_or_1_intro_left", "axiom_or_2_intro_right", "axiom_or_3_elim"], externalRef := "https://en.wikipedia.org/wiki/Intuitionistic_logic#Non-interdefinability_of_operators", greenVerified := true },
    { name := "admissibility_checking_exptime", reducesTo := "rule_admissibility_decision", dependencies := ["modus_ponens", "axiom_then_1_weakening", "axiom_then_2_distribution"], externalRef := "https://en.wikipedia.org/wiki/Admissible_rule", greenVerified := true } ]

theorem intuitionisticLogicUnconstructibles_length : intuitionisticLogicUnconstructibles.length = 7 := by
  decide

def intuitionisticLogicPackage : AxiomPackageInstance :=
  { name := "intuitionistic_logic", version := "1.0.0", templates := intuitionisticLogicTemplates,
    unconstructibles := intuitionisticLogicUnconstructibles, bottomGeometry := "intuitionistic_heyting_algebra_open_topology",
    negationEncoding := "brouwer_heyting_kolmogorov_implication_to_falsum", contradictionBehavior := "explosion_principle" }

def intuitionisticLogicExecutableRules : List ExecutableRule :=
  intuitionisticLogicTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem intuitionisticLogicExecutableRules_length : intuitionisticLogicExecutableRules.length = 50 := by
  decide

/-! ## Topos Theory 公理包实例 -/

def toposTheoryTemplateNamesRaw : List String :=
  ["terminal_object", "product", "pullback", "equalizer", "initial_object", "coproduct", "pushout", "coequalizer",
   "exponential", "evaluation", "curry", "uncurry", "exponential_functorial", "internal_hom_adjunction",
   "subobject_classifier", "truth_morphism", "characteristic_function", "subobject_classification", "false_morphism",
   "negation", "conjunction", "disjunction", "implication", "biconditional",
   "power_object", "membership_relation", "power_universal_property", "singleton_map", "power_as_exponential",
   "union", "intersection", "complement", "subset_relation",
   "natural_numbers_object", "zero_morphism", "successor_morphism", "nno_universal_property", "addition_nno", "multiplication_nno", "predecessor",
   "w_type", "heyting_implication", "heyting_negation", "non_contradiction", "double_negation_intro", "double_negation_elim", "excluded_middle", "peirce_law",
   "geometric_morphism", "inverse_image_functor", "direct_image_functor", "geometric_adjunction",
   "point_of_topos", "essential_geometric_morphism", "logical_morphism", "preserves_classifier", "preserves_exponentials", "preserves_nno",
   "lt_topology", "lt_topology_truth", "lt_topology_idempotent", "lt_topology_meets",
   "sheaf_for_topology", "double_negation_topology", "sheafification",
   "slice_topos", "presheaf_topos", "sheaf_topos", "grothendieck_topos", "classifying_topos",
   "morphism_object", "internal_category", "internal_presheaf",
   "topos_is_heyting", "topos_is_pretopos", "topos_is_lccc", "topos_is_extensive", "topos_is_adhesive",
   "mitchell_benabou_language", "kripke_joyal_semantics", "diaconescu_theorem"]

def toposTheoryTemplates : List PackageTemplate :=
  toposTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "topos_theory" })

theorem toposTheoryTemplates_length : toposTheoryTemplates.length = 81 := by
  decide

def toposTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "internal_logic_decidability", reducesTo := "undecidable", dependencies := ["subobject_classifier", "negation", "excluded_middle"], externalRef := "https://ncatlab.org/nlab/show/internal+logic", greenVerified := true },
    { name := "law_of_excluded_middle", reducesTo := "intuitionistic_logic", dependencies := ["subobject_classifier", "disjunction", "negation"], externalRef := "https://ncatlab.org/nlab/show/law+of+excluded+middle", greenVerified := true },
    { name := "double_negation_elimination", reducesTo := "intuitionistic_logic", dependencies := ["subobject_classifier", "negation"], externalRef := "https://ncatlab.org/nlab/show/double+negation", greenVerified := true },
    { name := "axiom_of_choice_internal", reducesTo := "intuitionistic_logic", dependencies := ["product", "exponential", "subobject_classifier"], externalRef := "https://ncatlab.org/nlab/show/axiom+of+choice", greenVerified := true },
    { name := "propositional_extensionality", reducesTo := "intuitionistic_logic", dependencies := ["subobject_classifier", "biconditional"], externalRef := "https://ncatlab.org/nlab/show/propositional+extensionality", greenVerified := true },
    { name := "geometric_morphism_classification", reducesTo := "undecidable", dependencies := ["geometric_morphism", "point_of_topos"], externalRef := "https://ncatlab.org/nlab/show/geometric+morphism", greenVerified := true },
    { name := "classifying_topos_construction", reducesTo := "decidable_for_geometric_theories", dependencies := ["classifying_topos", "presheaf_topos", "sheaf_topos"], externalRef := "https://ncatlab.org/nlab/show/classifying+topos", greenVerified := true },
    { name := "topos_morphism_equivalence", reducesTo := "undecidable", dependencies := ["geometric_morphism", "logical_morphism"], externalRef := "https://ncatlab.org/nlab/show/topos", greenVerified := true },
    { name := "internal_theorem_proving", reducesTo := "undecidable", dependencies := ["mitchell_benabou_language", "kripke_joyal_semantics", "natural_numbers_object"], externalRef := "https://ncatlab.org/nlab/show/Mitchell-B%C3%A9nabou+language", greenVerified := true },
    { name := "sheaf_coherence", reducesTo := "decidable_for_finite_sites", dependencies := ["sheaf_for_topology", "lt_topology"], externalRef := "https://ncatlab.org/nlab/show/sheaf", greenVerified := true } ]

theorem toposTheoryUnconstructibles_length : toposTheoryUnconstructibles.length = 10 := by
  decide

def toposTheoryPackage : AxiomPackageInstance :=
  { name := "topos_theory", version := "1.0.0", templates := toposTheoryTemplates,
    unconstructibles := toposTheoryUnconstructibles, bottomGeometry := "elementary_topos",
    negationEncoding := "heyting_negation", contradictionBehavior := "blocking" }

def toposTheoryExecutableRules : List ExecutableRule :=
  toposTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem toposTheoryExecutableRules_length : toposTheoryExecutableRules.length = 81 := by
  decide

end Instances
end Axioms
end Theory
end lvFormal
