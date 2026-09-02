import lvFormal.Theory.Axioms.Instances_Core
open lvFormal.Theory.Axioms.Instances
open lvFormal.Theory.Axioms.RuleTemplate

namespace lvFormal
namespace Theory
namespace Axioms
namespace Instances

/-! ## Field Theory 公理包实例 -/

def fieldTheoryTemplateNamesRaw : List String :=
  ["addition_associativity", "addition_commutativity", "additive_identity", "additive_inverse",
   "multiplication_associativity", "multiplication_identity", "multiplication_commutativity", "distributivity",
   "multiplicative_inverse", "zero_not_one", "multiplication_cancellation",
   "addition", "subtraction", "multiplication", "division", "negation", "reciprocal",
   "subfield_test", "field_extension", "algebraic_element", "transcendental_element", "field_tower",
   "automorphism_group", "galois_group", "fixed_field", "galois_extension", "galois_correspondence", "normal_extension",
   "finite_field", "prime_field", "algebraic_closure", "real_closure",
   "polynomial_ring", "irreducible_polynomial", "minimal_polynomial", "polynomial_root", "degree_of_extension"]

def fieldTheoryTemplates : List PackageTemplate :=
  fieldTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "field_theory" })

theorem fieldTheoryTemplates_length : fieldTheoryTemplates.length = 37 := by
  decide

def fieldTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "polynomial_root_by_radicals", reducesTo := "abel_ruffini_theorem", dependencies := ["polynomial_ring", "polynomial_root", "irreducible_polynomial", "field_extension", "galois_group"], externalRef := "https://en.wikipedia.org/wiki/Abel%E2%80%93Ruffini_theorem", greenVerified := true },
    { name := "galois_group_computation", reducesTo := "undecidable", dependencies := ["galois_group", "polynomial_ring", "field_extension", "automorphism_group", "irreducible_polynomial"], externalRef := "https://en.wikipedia.org/wiki/Galois_theory", greenVerified := true },
    { name := "field_isomorphism_problem", reducesTo := "undecidable", dependencies := ["field_extension", "subfield_test", "algebraic_element", "degree_of_extension"], externalRef := "https://en.wikipedia.org/wiki/Field_(mathematics)", greenVerified := true },
    { name := "algebraic_closure_uniqueness", reducesTo := "undecidable", dependencies := ["algebraic_closure", "field_extension", "algebraic_element"], externalRef := "https://en.wikipedia.org/wiki/Algebraically_closed_field", greenVerified := true },
    { name := "transcendence_degree_basis", reducesTo := "undecidable", dependencies := ["transcendental_element", "field_extension", "field_tower"], externalRef := "https://en.wikipedia.org/wiki/Transcendence_degree", greenVerified := true },
    { name := "field_embedding_existence", reducesTo := "undecidable", dependencies := ["field_extension", "subfield_test", "algebraic_element", "transcendental_element"], externalRef := "https://en.wikipedia.org/wiki/Field_(mathematics)", greenVerified := true },
    { name := "inverse_galois_problem", reducesTo := "undecidable", dependencies := ["galois_group", "galois_extension", "field_extension", "polynomial_ring", "irreducible_polynomial"], externalRef := "https://en.wikipedia.org/wiki/Inverse_Galois_problem", greenVerified := true } ]

theorem fieldTheoryUnconstructibles_length : fieldTheoryUnconstructibles.length = 7 := by
  decide

def fieldTheoryPackage : AxiomPackageInstance :=
  { name := "field_theory", version := "1.0.0", templates := fieldTheoryTemplates,
    unconstructibles := fieldTheoryUnconstructibles, bottomGeometry := "field_theory_abstract",
    negationEncoding := "classical_equality", contradictionBehavior := "explosion_principle" }

def fieldTheoryExecutableRules : List ExecutableRule :=
  fieldTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem fieldTheoryExecutableRules_length : fieldTheoryExecutableRules.length = 37 := by
  decide

/-! ## Order Theory 公理包实例 -/

def orderTheoryTemplateNamesRaw : List String :=
  ["reflexivity", "antisymmetry", "transitivity",
   "least_element", "greatest_element", "minimal_element", "maximal_element",
   "upper_bound", "lower_bound", "least_upper_bound", "greatest_lower_bound",
   "totality", "well_foundedness", "well_order",
   "monotone_function", "antitone_function", "order_embedding", "order_isomorphism",
   "dual_order", "product_order", "induced_suborder", "ordinal_sum",
   "strict_order", "covering_relation", "chain", "antichain", "interval", "hasse_diagram",
   "zorn_lemma", "dilworth_decomposition", "knaster_tarski_fixed_point", "szpilrajn_extension"]

def orderTheoryTemplates : List PackageTemplate :=
  orderTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "order_theory" })

theorem orderTheoryTemplates_length : orderTheoryTemplates.length = 32 := by
  decide

def orderTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "poset_dimension", reducesTo := "NP-hard optimization problem", dependencies := ["partial_order", "realizer"], externalRef := "https://en.wikipedia.org/wiki/Order_dimension", greenVerified := true },
    { name := "counting_linear_extensions", reducesTo := "#P-complete", dependencies := ["partial_order", "topological_sort"], externalRef := "https://en.wikipedia.org/wiki/Linear_extension", greenVerified := true },
    { name := "poset_isomorphism", reducesTo := "GI-hard", dependencies := ["partial_order", "graph_isomorphism"], externalRef := "https://en.wikipedia.org/wiki/Graph_isomorphism_problem", greenVerified := true },
    { name := "infinite_poset_width", reducesTo := "transfinite methods", dependencies := ["partial_order", "antichain", "zfc_set_theory"], externalRef := "https://en.wikipedia.org/wiki/Antichain", greenVerified := true },
    { name := "order_automorphism_group", reducesTo := "computationally intractable", dependencies := ["partial_order", "group_theory"], externalRef := "https://en.wikipedia.org/wiki/Automorphism_group", greenVerified := false },
    { name := "poset_convex_realizability", reducesTo := "undecidable", dependencies := ["partial_order", "convex_geometry"], externalRef := "https://en.wikipedia.org/wiki/Convex_geometry", greenVerified := true },
    { name := "poset_dimension_at_least_4", reducesTo := "NP-complete", dependencies := ["partial_order", "poset_dimension"], externalRef := "https://doi.org/10.1016/0012-365X(84)90132-1", greenVerified := true },
    { name := "chain_partition_minimization", reducesTo := "NP-hard", dependencies := ["partial_order", "dilworth_theorem"], externalRef := "https://en.wikipedia.org/wiki/Dilworth%27s_theorem", greenVerified := true } ]

theorem orderTheoryUnconstructibles_length : orderTheoryUnconstructibles.length = 8 := by
  decide

def orderTheoryPackage : AxiomPackageInstance :=
  { name := "order_theory", version := "1.0.0", templates := orderTheoryTemplates,
    unconstructibles := orderTheoryUnconstructibles, bottomGeometry := "hasse_diagram_poset",
    negationEncoding := "classical_order_negation", contradictionBehavior := "explosion_principle" }

def orderTheoryExecutableRules : List ExecutableRule :=
  orderTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem orderTheoryExecutableRules_length : orderTheoryExecutableRules.length = 32 := by
  decide

/-! ## Point-Set Topology 公理包实例 -/

def pointSetTopologyTemplateNamesRaw : List String :=
  ["open_set_empty", "open_set_full", "open_set_arbitrary_union", "open_set_finite_intersection",
   "closed_set", "closure", "interior", "boundary", "exterior", "neighborhood", "open_neighborhood", "limit_point",
   "continuous_map", "homeomorphism", "continuous_composition", "continuous_identity", "embedding", "quotient_map",
   "connected_space", "disconnected_space", "path_connected", "connected_component", "locally_connected", "separation_by_open_sets",
   "compact_space", "sequentially_compact", "locally_compact", "compact_subset_closed", "heine_borel", "tychonoff_product",
   "T0_kolmogorov", "T1_fréchet", "T2_hausdorff", "T3_regular", "T3half_tychonoff", "T4_normal", "T5_completely_normal", "T6_perfectly_normal",
   "convergent_sequence", "cauchy_sequence", "metric_space", "metric_topology", "complete_metric_space"]

def pointSetTopologyTemplates : List PackageTemplate :=
  pointSetTopologyTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "point_set_topology" })

theorem pointSetTopologyTemplates_length : pointSetTopologyTemplates.length = 43 := by
  decide

def pointSetTopologyUnconstructibles : List UnconstructibleProblem :=
  [ { name := "homeomorphism_problem", reducesTo := "undecidable", dependencies := ["continuous_map", "homeomorphism", "closure", "open_set_arbitrary_union"], externalRef := "https://en.wikipedia.org/wiki/Homeomorphism", greenVerified := true },
    { name := "homotopy_equivalence_problem", reducesTo := "undecidable", dependencies := ["continuous_map", "homeomorphism", "path_connected"], externalRef := "https://en.wikipedia.org/wiki/Homotopy", greenVerified := true },
    { name := "topological_isomorphism_problem", reducesTo := "undecidable", dependencies := ["continuous_map", "homeomorphism", "open_set_arbitrary_union", "open_set_finite_intersection"], externalRef := "https://en.wikipedia.org/wiki/Topological_space", greenVerified := true },
    { name := "compactness_recognition", reducesTo := "undecidable", dependencies := ["compact_space", "open_set_arbitrary_union", "open_set_finite_intersection"], externalRef := "https://en.wikipedia.org/wiki/Compact_space", greenVerified := true },
    { name := "metrizability_problem", reducesTo := "undecidable", dependencies := ["metric_space", "metric_topology", "T3half_tychonoff"], externalRef := "https://en.wikipedia.org/wiki/Metrization_theorem", greenVerified := true },
    { name := "covering_space_classification", reducesTo := "undecidable", dependencies := ["continuous_map", "path_connected", "connected_space"], externalRef := "https://en.wikipedia.org/wiki/Covering_space", greenVerified := true },
    { name := "fundamental_group_computation", reducesTo := "undecidable", dependencies := ["continuous_map", "path_connected", "connected_component"], externalRef := "https://en.wikipedia.org/wiki/Fundamental_group", greenVerified := true } ]

theorem pointSetTopologyUnconstructibles_length : pointSetTopologyUnconstructibles.length = 7 := by
  decide

def pointSetTopologyPackage : AxiomPackageInstance :=
  { name := "point_set_topology", version := "1.0.0", templates := pointSetTopologyTemplates,
    unconstructibles := pointSetTopologyUnconstructibles, bottomGeometry := "topological_space_open_sets",
    negationEncoding := "set_complement_in_topology", contradictionBehavior := "explosion_principle" }

def pointSetTopologyExecutableRules : List ExecutableRule :=
  pointSetTopologyTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem pointSetTopologyExecutableRules_length : pointSetTopologyExecutableRules.length = 43 := by
  decide

/-! ## Graph Theory 公理包实例 -/

def graphTheoryTemplateNamesRaw : List String :=
  ["vertex_set", "edge_set", "incidence",
   "adjacency", "vertex_degree", "handshaking_lemma", "maximum_degree", "minimum_degree",
   "walk", "path", "path_length", "distance", "connected", "connected_component", "component_count", "vertex_connectivity",
   "cycle", "girth", "circumference", "acyclic", "tree", "forest", "spanning_tree", "leaf",
   "subgraph", "induced_subgraph", "complement", "graph_union", "graph_intersection", "vertex_deletion", "edge_deletion", "edge_contraction", "line_graph", "cartesian_product", "graph_join", "disjoint_union",
   "complete_graph", "empty_graph", "path_graph", "cycle_graph", "bipartite", "complete_bipartite", "regular", "planar", "eulerian", "hamiltonian",
   "matching", "maximum_matching", "perfect_matching", "vertex_cover", "minimum_vertex_cover", "independent_set", "maximum_independent_set", "dominating_set",
   "vertex_coloring", "chromatic_number", "chromatic_polynomial", "edge_coloring", "chromatic_index", "clique",
   "euler_formula_planar", "konigs_theorem", "mengers_theorem", "kuratowskis_theorem", "four_color_theorem",
   "graph_isomorphism", "graph_automorphism", "treewidth", "graph_minor", "spectral_properties"]

def graphTheoryTemplates : List PackageTemplate :=
  graphTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "graph_theory" })

theorem graphTheoryTemplates_length : graphTheoryTemplates.length = 70 := by
  decide

def graphTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "graph_3_coloring", reducesTo := "NP_complete", dependencies := ["vertex_coloring", "chromatic_number", "three_colorability"], externalRef := "https://en.wikipedia.org/wiki/Graph_coloring#Computational_complexity", greenVerified := true },
    { name := "hamiltonian_cycle", reducesTo := "NP_complete", dependencies := ["cycle", "hamiltonian", "path"], externalRef := "https://en.wikipedia.org/wiki/Hamiltonian_path_problem", greenVerified := true },
    { name := "subgraph_isomorphism", reducesTo := "NP_complete", dependencies := ["subgraph", "graph_isomorphism", "induced_subgraph"], externalRef := "https://en.wikipedia.org/wiki/Subgraph_isomorphism_problem", greenVerified := true },
    { name := "graph_isomorphism_problem", reducesTo := "GI_complexity_class", dependencies := ["graph_isomorphism", "graph_automorphism"], externalRef := "https://en.wikipedia.org/wiki/Graph_isomorphism_problem", greenVerified := true },
    { name := "treewidth_computation", reducesTo := "NP_complete", dependencies := ["treewidth", "subgraph", "vertex_deletion"], externalRef := "https://en.wikipedia.org/wiki/Treewidth", greenVerified := true },
    { name := "maximum_clique", reducesTo := "NP_complete", dependencies := ["clique", "complement", "independent_set"], externalRef := "https://en.wikipedia.org/wiki/Clique_problem", greenVerified := true },
    { name := "maximum_independent_set_problem", reducesTo := "NP_complete", dependencies := ["independent_set", "maximum_independent_set", "vertex_cover"], externalRef := "https://en.wikipedia.org/wiki/Independent_set_(graph_theory)", greenVerified := true },
    { name := "minimum_vertex_cover_problem", reducesTo := "NP_complete", dependencies := ["vertex_cover", "minimum_vertex_cover", "maximum_matching"], externalRef := "https://en.wikipedia.org/wiki/Vertex_cover", greenVerified := true },
    { name := "minimum_dominating_set", reducesTo := "NP_complete", dependencies := ["dominating_set", "adjacency"], externalRef := "https://en.wikipedia.org/wiki/Dominating_set", greenVerified := true },
    { name := "graph_k_coloring", reducesTo := "NP_complete", dependencies := ["vertex_coloring", "chromatic_number", "bipartite"], externalRef := "https://en.wikipedia.org/wiki/Graph_coloring", greenVerified := true },
    { name := "steiner_tree", reducesTo := "NP_complete", dependencies := ["spanning_tree", "tree", "path"], externalRef := "https://en.wikipedia.org/wiki/Steiner_tree_problem", greenVerified := true },
    { name := "feedback_vertex_set", reducesTo := "NP_complete", dependencies := ["acyclic", "cycle", "vertex_deletion"], externalRef := "https://en.wikipedia.org/wiki/Feedback_vertex_set", greenVerified := true },
    { name := "graph_homomorphism", reducesTo := "NP_complete", dependencies := ["vertex_coloring", "complete_graph", "adjacency"], externalRef := "https://en.wikipedia.org/wiki/Graph_homomorphism", greenVerified := true },
    { name := "bandwidth_minimization", reducesTo := "NP_complete", dependencies := ["adjacency", "path_length"], externalRef := "https://en.wikipedia.org/wiki/Bandwidth_(graph_theory)", greenVerified := true } ]

theorem graphTheoryUnconstructibles_length : graphTheoryUnconstructibles.length = 14 := by
  decide

def graphTheoryPackage : AxiomPackageInstance :=
  { name := "graph_theory", version := "1.0.0", templates := graphTheoryTemplates,
    unconstructibles := graphTheoryUnconstructibles, bottomGeometry := "graph_incidence_structure",
    negationEncoding := "classical_edge_complement", contradictionBehavior := "explosion_principle" }

def graphTheoryExecutableRules : List ExecutableRule :=
  graphTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem graphTheoryExecutableRules_length : graphTheoryExecutableRules.length = 70 := by
  decide

/-! ## Number Theory 公理包实例 -/

def numberTheoryTemplateNamesRaw : List String :=
  ["divisibility", "congruence", "euler_totient", "fermat_little_theorem", "chinese_remainder", "quadratic_residue",
   "prime_number", "prime_distribution", "twin_primes", "goldbach_conjecture", "prime_number_theorem", "dirichlet_theorem",
   "algebraic_integer", "ring_of_integers", "ideal_theory", "class_number", "unit_group", "ramification_theory", "dedekind_domain",
   "riemann_zeta", "dirichlet_l_function", "modular_form", "l_function", "euler_product", "functional_equation",
   "diophantine_equation", "pell_equation", "elliptic_curve", "mordell_weil_theorem", "faltings_theorem",
   "p_adic_numbers", "adeles_ideles", "local_global_principle", "hensel_lemma",
   "algebraic_number", "transcendental_number", "liouville_number", "catalan_constant"]

def numberTheoryTemplates : List PackageTemplate :=
  numberTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "number_theory" })

theorem numberTheoryTemplates_length : numberTheoryTemplates.length = 38 := by
  decide

def numberTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "riemann_hypothesis", reducesTo := "open_problem", dependencies := ["riemann_zeta", "functional_equation", "l_function"], externalRef := "https://en.wikipedia.org/wiki/Riemann_hypothesis", greenVerified := true },
    { name := "goldbach_conjecture_verification", reducesTo := "open_problem", dependencies := ["goldbach_conjecture", "prime_distribution", "divisibility"], externalRef := "https://en.wikipedia.org/wiki/Goldbach%27s_conjecture", greenVerified := true },
    { name := "twin_prime_conjecture", reducesTo := "open_problem", dependencies := ["twin_primes", "prime_distribution", "prime_number_theorem"], externalRef := "https://en.wikipedia.org/wiki/Twin_prime", greenVerified := true },
    { name := "class_number_computation", reducesTo := "undecidable", dependencies := ["class_number", "ring_of_integers", "ideal_theory"], externalRef := "https://en.wikipedia.org/wiki/Class_number_problem", greenVerified := true },
    { name := "generalized_riemann_hypothesis", reducesTo := "open_problem", dependencies := ["dirichlet_l_function", "functional_equation", "l_function"], externalRef := "https://en.wikipedia.org/wiki/Generalized_Riemann_hypothesis", greenVerified := true },
    { name := "ideal_class_group_computation", reducesTo := "undecidable", dependencies := ["ideal_theory", "class_number", "unit_group"], externalRef := "https://en.wikipedia.org/wiki/Ideal_class_group", greenVerified := true },
    { name := "transcendence_of_constants", reducesTo := "open_problem", dependencies := ["transcendental_number", "l_function", "catalan_constant"], externalRef := "https://en.wikipedia.org/wiki/Transcendental_number_theory", greenVerified := true } ]

theorem numberTheoryUnconstructibles_length : numberTheoryUnconstructibles.length = 7 := by
  decide

def numberTheoryPackage : AxiomPackageInstance :=
  { name := "number_theory", version := "1.0.0", templates := numberTheoryTemplates,
    unconstructibles := numberTheoryUnconstructibles, bottomGeometry := "integer_number_line",
    negationEncoding := "classical_divisibility_complement", contradictionBehavior := "explosion_principle" }

def numberTheoryExecutableRules : List ExecutableRule :=
  numberTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem numberTheoryExecutableRules_length : numberTheoryExecutableRules.length = 38 := by
  decide

/-! ## Measure Theory 公理包实例 -/

def measureTheoryTemplateNamesRaw : List String :=
  ["sigma_algebra_contains_X", "sigma_algebra_complement", "sigma_algebra_countable_union", "sigma_algebra_countable_intersection", "sigma_algebra_contains_empty", "sigma_algebra_set_difference", "sigma_algebra_symmetric_difference",
   "measure_null_empty_set", "measure_non_negativity", "measure_countable_additivity", "measure_finite_additivity", "measure_monotonicity", "measure_countable_subadditivity", "measure_continuity_from_below", "measure_continuity_from_above",
   "outer_measure_null_empty", "outer_measure_monotonicity", "outer_measure_countable_subadditivity",
   "caratheodory_measurability", "measurable_sets_form_sigma_algebra", "outer_measure_restriction_complete",
   "null_set", "null_set_subset_measurable", "almost_everywhere", "measure_completion",
   "sigma_finite_measure", "semifinite_measure", "localizable_measure",
   "signed_measure", "hahn_decomposition", "jordan_decomposition", "total_variation_measure", "complex_measure",
   "absolute_continuity", "mutual_singularity", "radon_nikodym_theorem", "lebesgue_decomposition",
   "product_sigma_algebra", "product_measure", "fubini_theorem", "tonelli_theorem",
   "monotone_convergence_theorem", "fatou_lemma", "dominated_convergence_theorem", "uniform_integrability",
   "pre_measure", "caratheodory_outer_measure_construction", "caratheodory_extension_theorem", "dynkin_pi_lambda_theorem",
   "counting_measure", "dirac_measure", "lebesgue_measure_interval", "lebesgue_measure_rn", "borel_sigma_algebra", "hausdorff_measure", "hausdorff_dimension", "pushforward_measure", "measure_restriction",
   "lp_norm", "linf_norm", "holder_inequality", "minkowski_inequality", "jensen_inequality",
   "probability_measure", "conditional_expectation", "sigma_algebra_independence"]

def measureTheoryTemplates : List PackageTemplate :=
  measureTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "measure_theory" })

theorem measureTheoryTemplates_length : measureTheoryTemplates.length = 66 := by
  decide

def measureTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "vitali_set_non_measurable", reducesTo := "axiom_of_choice", dependencies := ["zfc_set_theory", "lebesgue_measure_rn"], externalRef := "https://en.wikipedia.org/wiki/Vitali_set", greenVerified := true },
    { name := "banach_tarski_paradox", reducesTo := "non_measurable_decomposition", dependencies := ["zfc_set_theory", "lebesgue_measure_rn"], externalRef := "https://en.wikipedia.org/wiki/Banach%E2%80%93Tarski_paradox", greenVerified := true },
    { name := "universal_measure_on_all_subsets", reducesTo := "non_measurable_set_existence", dependencies := ["zfc_set_theory", "lebesgue_measure_rn"], externalRef := "https://en.wikipedia.org/wiki/Non-measurable_set", greenVerified := true },
    { name := "all_sets_measurable_consistency", reducesTo := "independence_from_ZF_DC", dependencies := ["zfc_set_theory", "measure_null_empty_set"], externalRef := "https://en.wikipedia.org/wiki/Solovay_model", greenVerified := true },
    { name := "hausdorff_dimension_computation", reducesTo := "algorithmic_undecidability", dependencies := ["computability_theory", "hausdorff_measure"], externalRef := "https://en.wikipedia.org/wiki/Hausdorff_dimension", greenVerified := false },
    { name := "measure_space_isomorphism_classification", reducesTo := "isomorphism_problem", dependencies := ["zfc_set_theory", "measure_countable_additivity"], externalRef := "https://en.wikipedia.org/wiki/Standard_probability_space", greenVerified := false },
    { name := "measure_extension_uniqueness_without_sigma_finite", reducesTo := "non_uniqueness_of_extension", dependencies := ["caratheodory_extension_theorem", "sigma_finite_measure"], externalRef := "https://en.wikipedia.org/wiki/Carath%C3%A9odory%27s_extension_theorem", greenVerified := true },
    { name := "riemann_integrability_decision", reducesTo := "lebesgue_measure_zero_set", dependencies := ["lebesgue_measure_rn", "null_set"], externalRef := "https://en.wikipedia.org/wiki/Riemann_integral#Integrability", greenVerified := true },
    { name := "non_measurable_set_existence", reducesTo := "axiom_of_choice", dependencies := ["zfc_set_theory", "lebesgue_measure_rn"], externalRef := "https://en.wikipedia.org/wiki/Non-measurable_set", greenVerified := true } ]

theorem measureTheoryUnconstructibles_length : measureTheoryUnconstructibles.length = 9 := by
  decide

def measureTheoryPackage : AxiomPackageInstance :=
  { name := "measure_theory", version := "1.0.0", templates := measureTheoryTemplates,
    unconstructibles := measureTheoryUnconstructibles, bottomGeometry := "measure_space_extended_reals",
    negationEncoding := "classical_equality", contradictionBehavior := "explosion_principle" }

def measureTheoryExecutableRules : List ExecutableRule :=
  measureTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem measureTheoryExecutableRules_length : measureTheoryExecutableRules.length = 66 := by
  decide


end Instances
end Axioms
end Theory
end lvFormal
