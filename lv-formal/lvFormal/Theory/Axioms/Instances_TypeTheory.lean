import lvFormal.Theory.Axioms.Instances_Core
open lvFormal.Theory.Axioms.Instances
open lvFormal.Theory.Axioms.RuleTemplate

namespace lvFormal
namespace Theory
namespace Axioms
namespace Instances

/-! ## Game Theory 公理包实例 -/

def gameTheoryTemplateNamesRaw : List String :=
  ["player_set", "strategy_space", "strategy_profile", "utility_function", "game_tuple",
   "preference_completeness", "preference_transitivity", "expected_utility", "independence_axiom", "continuity_axiom",
   "best_response", "nash_equilibrium", "nash_existence_finite", "mixed_strategy", "expected_payoff_mixed", "mixed_strategy_support",
   "zero_sum_condition", "maximin", "minimax", "minimax_theorem", "saddle_point", "game_value",
   "characteristic_function", "grand_coalition", "imputation", "core", "bondareva_shapley_theorem", "shapley_value", "shapley_efficiency", "shapley_symmetry", "shapley_dummy", "shapley_additivity",
   "strict_dominance", "weak_dominance", "iterated_elimination", "pareto_efficiency",
   "game_tree", "information_set", "perfect_information", "imperfect_information", "subgame_perfect_equilibrium", "backward_induction",
   "evolutionarily_stable_strategy", "fitness", "replicator_dynamics", "nash_implies_ess",
   "social_choice_function", "incentive_compatibility", "vcg_mechanism", "arrow_impossibility",
   "repeated_game", "folk_theorem", "correlated_equilibrium", "trembling_hand_perfect", "proper_equilibrium"]

def gameTheoryTemplates : List PackageTemplate :=
  gameTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "game_theory" })

theorem gameTheoryTemplates_length : gameTheoryTemplates.length = 55 := by
  decide

def gameTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "nash_equilibrium_computation", reducesTo := "PPAD_complete", dependencies := ["nash_equilibrium", "mixed_strategy", "expected_payoff_mixed", "best_response"], externalRef := "https://doi.org/10.1145/1060590.1060645", greenVerified := true },
    { name := "game_isomorphism", reducesTo := "graph_isomorphism_hard", dependencies := ["game_tuple", "strategy_space", "utility_function"], externalRef := "https://en.wikipedia.org/wiki/Game_theory", greenVerified := true },
    { name := "dominance_solvable_check", reducesTo := "coNP_complete", dependencies := ["strict_dominance", "weak_dominance", "iterated_elimination"], externalRef := "https://doi.org/10.1016/0899-8256(90)90004-T", greenVerified := true },
    { name := "core_nonemptiness", reducesTo := "coNP_complete", dependencies := ["core", "characteristic_function", "bondareva_shapley_theorem"], externalRef := "https://en.wikipedia.org/wiki/Core_(game_theory)", greenVerified := true },
    { name := "shapley_value_computation", reducesTo := "sharp_P_complete", dependencies := ["shapley_value", "characteristic_function"], externalRef := "https://doi.org/10.1016/0022-0531(88)90317-7", greenVerified := true },
    { name := "ess_existence", reducesTo := "undecidable_in_general", dependencies := ["evolutionarily_stable_strategy", "fitness", "replicator_dynamics"], externalRef := "https://en.wikipedia.org/wiki/Evolutionarily_stable_strategy", greenVerified := true },
    { name := "subgame_perfect_equilibrium_computation", reducesTo := "PSPACE_complete", dependencies := ["subgame_perfect_equilibrium", "game_tree", "backward_induction"], externalRef := "https://doi.org/10.1145/331527.331534", greenVerified := true },
    { name := "mechanism_design_optimal", reducesTo := "undecidable_in_general", dependencies := ["incentive_compatibility", "vcg_mechanism", "social_choice_function"], externalRef := "https://doi.org/10.1016/0022-0531(86)90006-0", greenVerified := true },
    { name := "correlated_equilibrium_finding", reducesTo := "polynomial_time_solvable", dependencies := ["correlated_equilibrium", "nash_equilibrium", "mixed_strategy"], externalRef := "https://doi.org/10.1016/0022-0531(87)90037-8", greenVerified := true },
    { name := "bayesian_nash_equilibrium", reducesTo := "PPAD_complete", dependencies := ["nash_equilibrium", "mixed_strategy", "expected_utility"], externalRef := "https://doi.org/10.1145/353468.353481", greenVerified := true } ]

theorem gameTheoryUnconstructibles_length : gameTheoryUnconstructibles.length = 10 := by
  decide

def gameTheoryPackage : AxiomPackageInstance :=
  { name := "game_theory", version := "1.0.0", templates := gameTheoryTemplates,
    unconstructibles := gameTheoryUnconstructibles, bottomGeometry := "utility_space_convex_polytope",
    negationEncoding := "classical_deviation_negation", contradictionBehavior := "explosion_principle" }

def gameTheoryExecutableRules : List ExecutableRule :=
  gameTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem gameTheoryExecutableRules_length : gameTheoryExecutableRules.length = 55 := by
  decide

/-! ## Homotopy Type Theory 公理包实例 -/

def homotopyTypeTheoryTemplateNamesRaw : List String :=
  ["identity_type_path", "path_induction", "refl_identity", "transport_identification", "concat_path",
   "homotopy_equivalence", "quasi_inverse", "contr_map", "fiber_construction", "equivalence_induction", "ua_univalence",
   "universe_type", "univalence_axiom", "ua_equivalence", "is_inhabited_universe", "large_universe",
   "circle_type", "sphere_type", "interval_type", "quotient_type", "propositional_truncation", "set_truncation",
   "fiber_type", "total_space", "pullback_fibration", "cofiber_cospace", "suspension_loop",
   "homotopy_group_type", "connectedness_level", "n_type_deck", "truncation_closure", "equivalences_power",
   "hott_propositions_as_types", "hott_logic_levels", "propositional_resizing", "impredicative_universe", "classical_logic_hoq"]

def homotopyTypeTheoryTemplates : List PackageTemplate :=
  homotopyTypeTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "homotopy_type_theory" })

theorem homotopyTypeTheoryTemplates_length : homotopyTypeTheoryTemplates.length = 37 := by
  decide

def homotopyTypeTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "univalence_proof_checker", reducesTo := "undecidable", dependencies := ["univalence_axiom", "ua_equivalence", "homotopy_equivalence"], externalRef := "https://homotopytypetheory.org/book/", greenVerified := true },
    { name := "canonicity_in_hoq", reducesTo := "open_problem", dependencies := ["path_induction", "identity_type_path", "univalence_axiom"], externalRef := "https://ncatlab.org/nlab/show/canonicity", greenVerified := true },
    { name := "set_membership_hott", reducesTo := "open_problem", dependencies := ["n_type_deck", "set_truncation", "hott_propositions_as_types"], externalRef := "https://ncatlab.org/nlab/show/homotopy+type+theory", greenVerified := true },
    { name := "higher_inductive_coherence", reducesTo := "undecidable", dependencies := ["circle_type", "sphere_type", "interval_type", "quotient_type"], externalRef := "https://homotopytypetheory.org/book/", greenVerified := true },
    { name := "univalence_extensionality", reducesTo := "open_problem", dependencies := ["univalence_axiom", "ua_equivalence", "transport_identification"], externalRef := "https://ncatlab.org/nlab/show/univalence+axiom", greenVerified := true },
    { name := "constructive_univalence", reducesTo := "open_problem", dependencies := ["univalence_axiom", "ua_equivalence", "homotopy_equivalence", "quasi_inverse"], externalRef := "https://homotopytypetheory.org/book/", greenVerified := true } ]

theorem homotopyTypeTheoryUnconstructibles_length : homotopyTypeTheoryUnconstructibles.length = 6 := by
  decide

def homotopyTypeTheoryPackage : AxiomPackageInstance :=
  { name := "homotopy_type_theory", version := "1.0.0", templates := homotopyTypeTheoryTemplates,
    unconstructibles := homotopyTypeTheoryUnconstructibles, bottomGeometry := "homotopy_type_theory_univalent_foundations",
    negationEncoding := "identity_type_path_to_empty_type", contradictionBehavior := "explosion_principle" }

def homotopyTypeTheoryExecutableRules : List ExecutableRule :=
  homotopyTypeTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem homotopyTypeTheoryExecutableRules_length : homotopyTypeTheoryExecutableRules.length = 37 := by
  decide

/-! ## Dependent Type Theory 公理包实例 -/

def dependentTypeTheoryTemplateNamesRaw : List String :=
  ["pi_type", "sigma_type", "identity_type", "natural_number_type", "universe_type",
   "lambda_abstraction_dependent", "pair_dependent", "refl", "zero", "successor",
   "application_dependent", "projection_first", "projection_second", "induction_natural", "path_induction",
   "beta_reduction_dependent", "eta_expansion_dependent", "computation_natural", "computation_identity",
   "canonicity", "normalization", "decidable_type_checking", "undecidable_type_inhabitation", "cumulativity",
   "inductive_family", "pattern_matching", "universe_polymorphism", "coercions", "type_class",
   "propositions_as_types", "proof_relevance", "curry_howard_dependent", "constructive_existential"]

def dependentTypeTheoryTemplates : List PackageTemplate :=
  dependentTypeTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "dependent_type_theory" })

theorem dependentTypeTheoryTemplates_length : dependentTypeTheoryTemplates.length = 33 := by
  decide

def dependentTypeTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "type_inhabitation_dependent", reducesTo := "undecidable", dependencies := ["pi_type", "sigma_type", "lambda_abstraction_dependent", "application_dependent"], externalRef := "https://en.wikipedia.org/wiki/Type_inhabitation", greenVerified := true },
    { name := "type_equality_decidability", reducesTo := "undecidable", dependencies := ["identity_type", "pi_type", "sigma_type", "beta_reduction_dependent"], externalRef := "https://ncatlab.org/nlab/show/convertibility", greenVerified := true },
    { name := "normalization_order", reducesTo := "undecidable", dependencies := ["beta_reduction_dependent", "normalization", "lambda_abstraction_dependent", "application_dependent"], externalRef := "https://en.wikipedia.org/wiki/Normalisation_property_(abstract_rewriting)", greenVerified := true },
    { name := "universe_consistency", reducesTo := "undecidable", dependencies := ["universe_type", "cumulativity", "pi_type"], externalRef := "https://en.wikipedia.org/wiki/Girard%27s_paradox", greenVerified := true },
    { name := "parametricity_verification", reducesTo := "undecidable", dependencies := ["pi_type", "universe_type", "lambda_abstraction_dependent", "application_dependent"], externalRef := "https://ncatlab.org/nlab/show/parametricity", greenVerified := true },
    { name := "termination_checking_dependent", reducesTo := "undecidable", dependencies := ["natural_number_type", "induction_natural", "beta_reduction_dependent", "normalization"], externalRef := "https://en.wikipedia.org/wiki/Termination_analysis", greenVerified := true } ]

theorem dependentTypeTheoryUnconstructibles_length : dependentTypeTheoryUnconstructibles.length = 6 := by
  decide

def dependentTypeTheoryPackage : AxiomPackageInstance :=
  { name := "dependent_type_theory", version := "1.0.0", templates := dependentTypeTheoryTemplates,
    unconstructibles := dependentTypeTheoryUnconstructibles, bottomGeometry := "dependent_type_theory_terms",
    negationEncoding := "identity_type_to_empty_type", contradictionBehavior := "explosion_principle" }

def dependentTypeTheoryExecutableRules : List ExecutableRule :=
  dependentTypeTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem dependentTypeTheoryExecutableRules_length : dependentTypeTheoryExecutableRules.length = 33 := by
  decide

/-! ## Simple Type Theory 公理包实例 -/

def simpleTypeTheoryTemplateNamesRaw : List String :=
  ["base_type", "function_type", "product_type", "sum_type", "unit_type",
   "variable", "lambda_abstraction", "application", "beta_reduction", "eta_expansion", "alpha_conversion",
   "var_rule", "abs_rule", "app_rule", "conv_rule", "prod_formation", "prod_intro", "prod_elim", "prod_beta",
   "proposition_as_type", "proof_as_term", "implication_as_function_type", "conjunction_as_product", "disjunction_as_sum", "negation_as_function_to_false",
   "type_safety_progress", "type_safety_preservation", "strong_normalization", "decidability_of_typing", "principal_type_property",
   "let_binding", "fixpoint_for_product", "pair_constructor", "pair_elimination", "inductive_type_sketch",
   "type_equivalence", "definitional_equality", "beta_eta_equivalence", "subtype_relation"]

def simpleTypeTheoryTemplates : List PackageTemplate :=
  simpleTypeTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "simple_type_theory" })

theorem simpleTypeTheoryTemplates_length : simpleTypeTheoryTemplates.length = 39 := by
  decide

def simpleTypeTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "type_inhabitation_general", reducesTo := "undecidable", dependencies := ["var_rule", "abs_rule", "app_rule", "function_type", "product_type"], externalRef := "https://en.wikipedia.org/wiki/Type_inhabitation", greenVerified := true },
    { name := "beta_normalization_order", reducesTo := "undecidable", dependencies := ["beta_reduction", "lambda_abstraction", "application", "strong_normalization"], externalRef := "https://en.wikipedia.org/wiki/Beta_normal_form", greenVerified := true },
    { name := "type_equivalence_decidability", reducesTo := "undecidable", dependencies := ["type_equivalence", "beta_reduction", "conv_rule", "product_type"], externalRef := "https://ncatlab.org/nlab/show/convertibility", greenVerified := true },
    { name := "polymorphic_type_inhabitation", reducesTo := "undecidable", dependencies := ["function_type", "product_type", "var_rule", "abs_rule"], externalRef := "https://en.wikipedia.org/wiki/System_F", greenVerified := true },
    { name := "termination_checking", reducesTo := "undecidable", dependencies := ["beta_reduction", "lambda_abstraction", "application", "strong_normalization"], externalRef := "https://en.wikipedia.org/wiki/Normalisation_property_(abstract_rewriting)", greenVerified := true },
    { name := "proof_irrelevance", reducesTo := "undecidable", dependencies := ["proposition_as_type", "proof_as_term", "definitional_equality", "beta_eta_equivalence"], externalRef := "https://ncatlab.org/nlab/show/proof+irrelevance", greenVerified := true } ]

theorem simpleTypeTheoryUnconstructibles_length : simpleTypeTheoryUnconstructibles.length = 6 := by
  decide

def simpleTypeTheoryPackage : AxiomPackageInstance :=
  { name := "simple_type_theory", version := "1.0.0", templates := simpleTypeTheoryTemplates,
    unconstructibles := simpleTypeTheoryUnconstructibles, bottomGeometry := "simply_typed_lambda_calculus_terms",
    negationEncoding := "function_type_to_empty_type", contradictionBehavior := "explosion_principle" }

def simpleTypeTheoryExecutableRules : List ExecutableRule :=
  simpleTypeTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem simpleTypeTheoryExecutableRules_length : simpleTypeTheoryExecutableRules.length = 39 := by
  decide


end Instances
end Axioms
end Theory
end lvFormal
