import lvFormal.Theory.Axioms.Instances_Core
open lvFormal.Theory.Axioms.Instances
open lvFormal.Theory.Axioms.RuleTemplate

namespace lvFormal
namespace Theory
namespace Axioms
namespace Instances

/-! ## Homological Algebra 公理包实例 -/

def homologicalAlgebraTemplateNamesRaw : List String :=
  ["zero_object", "biproduct", "kernel", "cokernel", "abelian_category", "exact_sequence",
   "chain_complex", "cochain_complex", "chain_map", "chain_homotopy", "mapping_cone", "cylinder",
   "homology_group", "cohomology_group", "long_exact_sequence", "snake_lemma", "five_lemma",
   "left_derived_functor", "right_derived_functor", "tor_functor", "ext_functor", "spectral_sequence", "derived_category",
   "injective_module", "projective_module", "flat_module", "free_resolution", "injective_resolution",
   "projective_dimension", "injective_dimension", "global_dimension", "regular_sequence",
   "group_cohomology", "sheaf_cohomology_application", "hochschild_homology", "cyclic_homology"]

def homologicalAlgebraTemplates : List PackageTemplate :=
  homologicalAlgebraTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "homological_algebra" })

theorem homologicalAlgebraTemplates_length : homologicalAlgebraTemplates.length = 36 := by
  decide

def homologicalAlgebraUnconstructibles : List UnconstructibleProblem :=
  [ { name := "projective_dimension_computation", reducesTo := "undecidable", dependencies := ["projective_dimension", "free_resolution", "projective_module", "abelian_category"], externalRef := "https://en.wikipedia.org/wiki/Projective_dimension", greenVerified := true },
    { name := "global_dimension_computation", reducesTo := "undecidable", dependencies := ["global_dimension", "projective_dimension", "abelian_category", "exact_sequence"], externalRef := "https://en.wikipedia.org/wiki/Global_dimension", greenVerified := true },
    { name := "spectral_sequence_convergence", reducesTo := "undecidable", dependencies := ["spectral_sequence", "homology_group", "cohomology_group", "long_exact_sequence"], externalRef := "https://en.wikipedia.org/wiki/Spectral_sequence", greenVerified := true },
    { name := "extension_group_computation", reducesTo := "undecidable", dependencies := ["ext_functor", "injective_resolution", "projective_module", "abelian_category"], externalRef := "https://en.wikipedia.org/wiki/Ext_functor", greenVerified := true },
    { name := "derived_equivalence_problem", reducesTo := "undecidable", dependencies := ["derived_category", "chain_complex", "chain_map", "abelian_category"], externalRef := "https://en.wikipedia.org/wiki/Derived_category", greenVerified := true },
    { name := "homological_conjecture_resolution", reducesTo := "undecidable", dependencies := ["global_dimension", "projective_dimension", "injective_dimension", "ext_functor"], externalRef := "https://en.wikipedia.org/wiki/Homological_algebra#Open_problems", greenVerified := true } ]

theorem homologicalAlgebraUnconstructibles_length : homologicalAlgebraUnconstructibles.length = 6 := by
  decide

def homologicalAlgebraPackage : AxiomPackageInstance :=
  { name := "homological_algebra", version := "1.0.0", templates := homologicalAlgebraTemplates,
    unconstructibles := homologicalAlgebraUnconstructibles, bottomGeometry := "abelian_category_chain_complexes",
    negationEncoding := "exact_sequence_kernel_cokernel", contradictionBehavior := "explosion_principle" }

def homologicalAlgebraExecutableRules : List ExecutableRule :=
  homologicalAlgebraTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem homologicalAlgebraExecutableRules_length : homologicalAlgebraExecutableRules.length = 36 := by
  decide

/-! ## Differential Geometry 公理包实例 -/

def differentialGeometryTemplateNamesRaw : List String :=
  ["smooth_manifold", "chart_atlas", "smooth_map", "tangent_space", "vector_field", "tensor_field",
   "riemannian_metric", "metric_compatibility", "length_of_curve", "geodesic", "exponential_map", "volume_form",
   "riemann_curvature_tensor", "ricci_curvature", "scalar_curvature", "sectional_curvature", "gauss_bonnet_theorem", "gauss_curvature",
   "levi_civita_connection", "covariant_derivative", "parallel_transport", "christoffel_symbols", "torsion_tensor",
   "submanifold", "immersion_embedding", "induced_metric", "second_fundamental_form", "mean_curvature", "shape_operator",
   "riemannian_manifold_complete", "hopf_rinow_theorem", "constant_curvature", "maximally_symmetric", "space_form",
   "pseudo_riemannian_metric", "lorentzian_manifold", "symplectic_manifold", "complex_manifold", "kahler_metric"]

def differentialGeometryTemplates : List PackageTemplate :=
  differentialGeometryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "differential_geometry" })

theorem differentialGeometryTemplates_length : differentialGeometryTemplates.length = 39 := by
  decide

def differentialGeometryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "geodesic_completeness_decision", reducesTo := "undecidable", dependencies := ["geodesic", "riemannian_manifold_complete", "exponential_map"], externalRef := "https://en.wikipedia.org/wiki/Geodesic_completeness", greenVerified := true },
    { name := "positive_mass_theorem", reducesTo := "open_problem", dependencies := ["scalar_curvature", "ricci_curvature", "riemann_curvature_tensor"], externalRef := "https://en.wikipedia.org/wiki/Positive_mass_theorem", greenVerified := true },
    { name := "exotic_sphere_existence", reducesTo := "undecidable", dependencies := ["smooth_manifold", "riemannian_metric", "ricci_curvature"], externalRef := "https://en.wikipedia.org/wiki/Exotic_sphere", greenVerified := true },
    { name := "poincare_conjecture_higher", reducesTo := "solved", dependencies := ["smooth_manifold", "riemann_curvature_tensor", "ricci_curvature"], externalRef := "https://en.wikipedia.org/wiki/Poincar%C3%A9_conjecture", greenVerified := true },
    { name := "curvature_bounded_below", reducesTo := "undecidable", dependencies := ["ricci_curvature", "scalar_curvature", "sectional_curvature"], externalRef := "https://en.wikipedia.org/wiki/Curvature_bounds", greenVerified := true },
    { name := "symplectic_embedding", reducesTo := "undecidable", dependencies := ["symplectic_manifold", "immersion_embedding", "riemannian_metric"], externalRef := "https://en.wikipedia.org/wiki/Symplectic_embedding", greenVerified := true } ]

theorem differentialGeometryUnconstructibles_length : differentialGeometryUnconstructibles.length = 6 := by
  decide

def differentialGeometryPackage : AxiomPackageInstance :=
  { name := "differential_geometry", version := "1.0.0", templates := differentialGeometryTemplates,
    unconstructibles := differentialGeometryUnconstructibles, bottomGeometry := "smooth_manifolds_with_riemannian_structure",
    negationEncoding := "tensor_field_complement", contradictionBehavior := "explosion_principle" }

def differentialGeometryExecutableRules : List ExecutableRule :=
  differentialGeometryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem differentialGeometryExecutableRules_length : differentialGeometryExecutableRules.length = 39 := by
  decide

/-! ## Computability Theory 公理包实例 -/

def computabilityTheoryTemplateNamesRaw : List String :=
  ["zero_function", "successor_function", "projection_function",
   "composition", "primitive_recursion", "minimization_operator",
   "universal_turing_machine", "kleene_T_predicate", "result_extraction",
   "kleene_normal_form", "smn_theorem", "kleene_recursion_theorem", "rice_theorem",
   "computable_set", "computably_enumerable_set", "halting_set_K", "complement_halting_set",
   "many_one_reducibility", "turing_reducibility", "turing_equivalence", "turing_degree", "turing_jump",
   "sigma_1_set", "pi_1_set", "sigma_n_set", "pi_n_set", "delta_n_set", "post_theorem",
   "cantor_pairing", "cantor_unpairing", "godel_numbering", "program_enumeration", "diagonalization",
   "oracle_turing_machine", "relative_computability", "finite_injury_priority", "infinite_injury_priority",
   "primitive_addition", "primitive_multiplication", "primitive_exponentiation", "primitive_factorial",
   "primitive_predecessor", "primitive_subtraction", "primitive_sign", "primitive_absolute_difference",
   "bounded_minimization", "bounded_existential", "bounded_universal",
   "ackermann_function", "busy_beaver_function", "kolmogorov_complexity", "martin_lof_randomness_test", "friedberg_muchnik_theorem",
   "computable_real_number", "computable_function_on_reals"]

def computabilityTheoryTemplates : List PackageTemplate :=
  computabilityTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "computability_theory" })

theorem computabilityTheoryTemplates_length : computabilityTheoryTemplates.length = 55 := by
  decide

def computabilityTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "halting_problem", reducesTo := "non_computable_set", dependencies := ["universal_turing_machine", "diagonalization", "godel_numbering"], externalRef := "https://en.wikipedia.org/wiki/Halting_problem", greenVerified := true },
    { name := "rice_theorem_undecidability", reducesTo := "halting_problem", dependencies := ["kleene_recursion_theorem", "program_enumeration", "halting_problem"], externalRef := "https://en.wikipedia.org/wiki/Rice%27s_theorem", greenVerified := true },
    { name := "totality_problem", reducesTo := "non_computable_set", dependencies := ["rice_theorem_undecidability", "sigma_n_set"], externalRef := "https://en.wikipedia.org/wiki/Total_function", greenVerified := true },
    { name := "program_equivalence_problem", reducesTo := "non_computable_set", dependencies := ["rice_theorem_undecidability", "program_enumeration"], externalRef := "https://en.wikipedia.org/wiki/Program_equivalence", greenVerified := true },
    { name := "post_correspondence_problem", reducesTo := "halting_problem", dependencies := ["universal_turing_machine", "halting_problem"], externalRef := "https://en.wikipedia.org/wiki/Post_correspondence_problem", greenVerified := true },
    { name := "hilberts_tenth_problem", reducesTo := "halting_problem", dependencies := ["minimization_operator", "primitive_recursion", "halting_problem"], externalRef := "https://en.wikipedia.org/wiki/Hilbert%27s_tenth_problem", greenVerified := true },
    { name := "word_problem_for_groups", reducesTo := "halting_problem", dependencies := ["universal_turing_machine", "halting_problem"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_for_groups", greenVerified := true },
    { name := "kolmogorov_complexity_exact", reducesTo := "non_computable_function", dependencies := ["diagonalization", "universal_turing_machine", "program_enumeration"], externalRef := "https://en.wikipedia.org/wiki/Kolmogorov_complexity", greenVerified := true },
    { name := "busy_beaver_values", reducesTo := "non_computable_function", dependencies := ["halting_problem", "universal_turing_machine"], externalRef := "https://en.wikipedia.org/wiki/Busy_beaver", greenVerified := true },
    { name := "entscheidungsproblem", reducesTo := "halting_problem", dependencies := ["halting_problem", "godel_numbering"], externalRef := "https://en.wikipedia.org/wiki/Entscheidungsproblem", greenVerified := true },
    { name := "tiling_problem", reducesTo := "halting_problem", dependencies := ["halting_problem", "post_correspondence_problem"], externalRef := "https://en.wikipedia.org/wiki/Wang_tile", greenVerified := true },
    { name := "mortal_matrix_problem", reducesTo := "halting_problem", dependencies := ["halting_problem"], externalRef := "https://en.wikipedia.org/wiki/Mortal_matrix_problem", greenVerified := true },
    { name := "posts_problem_uniform_solution", reducesTo := "non_uniform_construction", dependencies := ["finite_injury_priority", "turing_degree", "halting_set_K"], externalRef := "https://en.wikipedia.org/wiki/Post%27s_problem", greenVerified := true },
    { name := "zero_of_computable_function", reducesTo := "halting_problem", dependencies := ["rice_theorem_undecidability", "halting_problem"], externalRef := "https://en.wikipedia.org/wiki/Rice%27s_theorem", greenVerified := true } ]

theorem computabilityTheoryUnconstructibles_length : computabilityTheoryUnconstructibles.length = 14 := by
  decide

def computabilityTheoryPackage : AxiomPackageInstance :=
  { name := "computability_theory", version := "1.0.0", templates := computabilityTheoryTemplates,
    unconstructibles := computabilityTheoryUnconstructibles, bottomGeometry := "turing_machine_configuration_space",
    negationEncoding := "complement_in_natural_numbers", contradictionBehavior := "explosion_principle" }

def computabilityTheoryExecutableRules : List ExecutableRule :=
  computabilityTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem computabilityTheoryExecutableRules_length : computabilityTheoryExecutableRules.length = 55 := by
  decide

/-! ## Modal Logic 公理包实例 -/

def modalLogicTemplateNamesRaw : List String :=
  ["classical_tautology", "modus_ponens",
   "kripke_schema", "necessitation",
   "possibility_dual", "necessity_dual",
   "reflexivity_T",
   "transitivity_4",
   "symmetry_B", "euclidean_5",
   "seriality_D",
   "lob_axiom",
   "kripke_frame", "kripke_model", "satisfaction_at_world", "validity_in_frame",
   "modal_modus_tollens", "box_distributes_over_and", "diamond_monotonicity", "modal_negation",
   "knowledge_axiom", "positive_introspection", "negative_introspection",
   "always_operator", "eventually_operator", "next_operator", "until_operator"]

def modalLogicTemplates : List PackageTemplate :=
  modalLogicTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "modal_logic" })

theorem modalLogicTemplates_length : modalLogicTemplates.length = 27 := by
  decide

def modalLogicUnconstructibles : List UnconstructibleProblem :=
  [ { name := "modal_satisfiability_K", reducesTo := "PSPACE_complete", dependencies := ["classical_propositional_logic"], externalRef := "https://en.wikipedia.org/wiki/PSPACE-complete", greenVerified := false },
    { name := "modal_satisfiability_S4", reducesTo := "PSPACE_complete", dependencies := ["modal_satisfiability_K"], externalRef := "https://en.wikipedia.org/wiki/Modal_logic", greenVerified := false },
    { name := "modal_satisfiability_S5", reducesTo := "NP_complete", dependencies := ["classical_propositional_logic"], externalRef := "https://en.wikipedia.org/wiki/NP-completeness", greenVerified := false },
    { name := "modal_uniform_interpolation", reducesTo := "undecidable", dependencies := ["modal_logic"], externalRef := "https://en.wikipedia.org/wiki/Interpolation", greenVerified := false },
    { name := "modal_logic_with_propositional_quantifiers", reducesTo := "undecidable", dependencies := ["second_order_logic"], externalRef := "https://en.wikipedia.org/wiki/Second-order_logic", greenVerified := false },
    { name := "global_satisfiability_S4", reducesTo := "EXPTIME_complete", dependencies := ["modal_satisfiability_S4"], externalRef := "https://en.wikipedia.org/wiki/EXPTIME", greenVerified := false },
    { name := "modal_mu_calculus_model_checking", reducesTo := "NP_intersection_coNP", dependencies := ["modal_logic"], externalRef := "https://en.wikipedia.org/wiki/Modal_%CE%BC-calculus", greenVerified := false } ]

theorem modalLogicUnconstructibles_length : modalLogicUnconstructibles.length = 7 := by
  decide

def modalLogicPackage : AxiomPackageInstance :=
  { name := "modal_logic", version := "1.0.0", templates := modalLogicTemplates,
    unconstructibles := modalLogicUnconstructibles, bottomGeometry := "kripke_possible_worlds_semantics",
    negationEncoding := "classical_complement_with_modal_dual", contradictionBehavior := "explosion_principle" }

def modalLogicExecutableRules : List ExecutableRule :=
  modalLogicTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem modalLogicExecutableRules_length : modalLogicExecutableRules.length = 27 := by
  decide

/-! ## Universal Algebra 公理包实例 -/

def universalAlgebraTemplateNamesRaw : List String :=
  ["signature", "term_algebra", "substitution", "equational_satisfaction", "congruence", "quotient_algebra", "homomorphism", "subalgebra", "direct_product", "free_algebra",
   "homomorphic_image", "subalgebra_closure", "product_closure",
   "congruence_identity", "congruence_total", "congruence_meet", "congruence_join", "congruence_lattice", "factor_theorem",
   "first_isomorphism_theorem", "second_isomorphism_theorem", "third_isomorphism_theorem", "correspondence_theorem",
   "equational_class", "hsp_theorem", "free_algebra_universal_property", "subdirect_representation", "subdirectly_irreducible", "equational_basis",
   "malcev_term", "congruence_permutability", "congruence_modularity", "congruence_distributivity", "jonsson_terms", "day_terms",
   "equational_deduction", "term_rewriting", "confluence", "termination", "knuth_bendix_completion",
   "apply_operation", "build_term", "evaluate_term", "form_quotient", "form_homomorphism", "form_subalgebra", "form_product", "form_free_algebra",
   "congruence_generation", "kernel", "image", "isomorphism", "endomorphism", "automorphism", "subdirect_embedding", "ultrafilter_construction", "clone", "polynomial_clone", "variety_membership", "equational_consequence"]

def universalAlgebraTemplates : List PackageTemplate :=
  universalAlgebraTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "universal_algebra" })

theorem universalAlgebraTemplates_length : universalAlgebraTemplates.length = 60 := by
  decide

def universalAlgebraUnconstructibles : List UnconstructibleProblem :=
  [ { name := "word_problem_for_varieties", reducesTo := "undecidable", dependencies := ["equational_deduction", "term_rewriting", "signature", "equational_satisfaction"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_(mathematics)", greenVerified := true },
    { name := "equational_theory_equivalence", reducesTo := "undecidable", dependencies := ["equational_deduction", "equational_basis", "equational_satisfaction", "equational_class"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_(mathematics)", greenVerified := true },
    { name := "finite_basis_problem", reducesTo := "undecidable", dependencies := ["equational_basis", "equational_class", "hsp_theorem"], externalRef := "https://en.wikipedia.org/wiki/Universal_algebra", greenVerified := true },
    { name := "variety_equivalence", reducesTo := "undecidable", dependencies := ["equational_class", "hsp_theorem", "equational_basis"], externalRef := "https://en.wikipedia.org/wiki/Variety_(universal_algebra)", greenVerified := true },
    { name := "congruence_lattice_recognition", reducesTo := "undecidable", dependencies := ["congruence_lattice", "congruence_meet", "congruence_join"], externalRef := "https://en.wikipedia.org/wiki/Universal_algebra", greenVerified := false },
    { name := "free_algebra_finiteness", reducesTo := "undecidable", dependencies := ["free_algebra", "equational_deduction", "signature"], externalRef := "https://en.wikipedia.org/wiki/Universal_algebra", greenVerified := false },
    { name := "knuth_bendix_completion_termination", reducesTo := "undecidable", dependencies := ["knuth_bendix_completion", "confluence", "termination", "term_rewriting"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_(mathematics)", greenVerified := true },
    { name := "equational_unification", reducesTo := "undecidable", dependencies := ["equational_deduction", "substitution", "term_algebra", "equational_satisfaction"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_(mathematics)", greenVerified := true } ]

theorem universalAlgebraUnconstructibles_length : universalAlgebraUnconstructibles.length = 8 := by
  decide

def universalAlgebraPackage : AxiomPackageInstance :=
  { name := "universal_algebra", version := "1.0.0", templates := universalAlgebraTemplates,
    unconstructibles := universalAlgebraUnconstructibles, bottomGeometry := "universal_algebra_equational",
    negationEncoding := "equational_equality", contradictionBehavior := "explosion_principle" }

def universalAlgebraExecutableRules : List ExecutableRule :=
  universalAlgebraTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem universalAlgebraExecutableRules_length : universalAlgebraExecutableRules.length = 60 := by
  decide

/-! ## Combinatorics 公理包实例 -/

def combinatoricsTemplateNamesRaw : List String :=
  ["pigeonhole_principle", "inclusion_exclusion", "binomial_coefficient", "multinomial_coefficient", "stars_and_bars", "generating_function",
   "permutation", "combination", "permutation_with_repetition", "derangement", "stirling_number",
   "graph_vertex", "graph_edge", "graph_path", "graph_cycle", "graph_tree", "graph_connected", "graph_bipartite", "graph_planar",
   "graph_coloring", "graph_matching", "graph_flow", "eulerian_path", "hamiltonian_path", "graph_isomorphism",
   "ramsey_number", "ramsey_theorem", "schur_theorem", "van_der_waerden_theorem",
   "probabilistic_method", "markov_inequality", "chebyshev_inequality", "chernoff_bound", "lovasz_local_lemma",
   "latin_square", "design_theory_block", "error_correcting_code", "matroid", "poset_dilworth"]

def combinatoricsTemplates : List PackageTemplate :=
  combinatoricsTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "combinatorics" })

theorem combinatoricsTemplates_length : combinatoricsTemplates.length = 39 := by
  decide

def combinatoricsUnconstructibles : List UnconstructibleProblem :=
  [ { name := "graph_isomorphism_problem", reducesTo := "quasi_polynomial", dependencies := ["graph_vertex", "graph_edge", "graph_isomorphism"], externalRef := "https://en.wikipedia.org/wiki/Graph_isomorphism_problem", greenVerified := true },
    { name := "graph_coloring_decision", reducesTo := "np_complete", dependencies := ["graph_vertex", "graph_edge", "graph_coloring"], externalRef := "https://en.wikipedia.org/wiki/Graph_coloring", greenVerified := true },
    { name := "hamiltonian_cycle_decision", reducesTo := "np_complete", dependencies := ["graph_vertex", "graph_edge", "graph_path", "graph_cycle", "hamiltonian_path"], externalRef := "https://en.wikipedia.org/wiki/Hamiltonian_path_problem", greenVerified := true },
    { name := "subgraph_isomorphism", reducesTo := "np_complete", dependencies := ["graph_vertex", "graph_edge", "graph_isomorphism"], externalRef := "https://en.wikipedia.org/wiki/Subgraph_isomorphism_problem", greenVerified := true },
    { name := "ramsey_number_exact", reducesTo := "undecidable", dependencies := ["ramsey_number", "ramsey_theorem", "graph_coloring"], externalRef := "https://en.wikipedia.org/wiki/Ramsey%27s_theorem", greenVerified := true },
    { name := "permanent_computation", reducesTo := "sharp_p_hard", dependencies := ["permutation", "combination"], externalRef := "https://en.wikipedia.org/wiki/Permanent_(mathematics)", greenVerified := true },
    { name := "satisfiability_3sat", reducesTo := "np_complete", dependencies := ["pigeonhole_principle", "inclusion_exclusion"], externalRef := "https://en.wikipedia.org/wiki/Boolean_satisfiability_problem", greenVerified := true } ]

theorem combinatoricsUnconstructibles_length : combinatoricsUnconstructibles.length = 7 := by
  decide

def combinatoricsPackage : AxiomPackageInstance :=
  { name := "combinatorics", version := "1.0.0", templates := combinatoricsTemplates,
    unconstructibles := combinatoricsUnconstructibles, bottomGeometry := "finite_discrete_structures",
    negationEncoding := "classical_complement", contradictionBehavior := "explosion_principle" }

def combinatoricsExecutableRules : List ExecutableRule :=
  combinatoricsTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem combinatoricsExecutableRules_length : combinatoricsExecutableRules.length = 39 := by
  decide


end Instances
end Axioms
end Theory
end lvFormal
