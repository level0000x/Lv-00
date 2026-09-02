import lvFormal.Theory.Axioms.Instances_Core
open lvFormal.Theory.Axioms.Instances
open lvFormal.Theory.Axioms.RuleTemplate

namespace lvFormal
namespace Theory
namespace Axioms
namespace Instances

/-! ## Real Analysis 公理包实例 -/

def realAnalysisTemplateNamesRaw : List String :=
  ["ordered_field", "additive_associativity", "multiplicative_associativity", "distributivity", "order_compatibility", "archimedean_property", "dedekind_completeness", "least_upper_bound",
   "convergent_sequence", "cauchy_sequence", "monotone_convergence", "bolzano_weierstrass", "sequential_compactness", "limit_superior", "limit_inferior",
   "continuous_function", "uniform_continuity", "intermediate_value_theorem", "extreme_value_theorem", "lipschitz_continuity", "uniform_convergence",
   "derivative", "riemann_integral", "fundamental_theorem_of_calculus", "chain_rule", "mean_value_theorem", "taylor_expansion",
   "sigma_algebra", "measurable_set", "measure", "lebesgue_measure", "outer_measure", "caratheodory_extension", "measurable_function",
   "lebesgue_integral", "monotone_convergence_theorem", "dominated_convergence_theorem", "fatou_lemma", "fubini_theorem",
   "lp_space", "holder_inequality", "minkowski_inequality", "l2_hilbert_space"]

def realAnalysisTemplates : List PackageTemplate :=
  realAnalysisTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "real_analysis" })

theorem realAnalysisTemplates_length : realAnalysisTemplates.length = 43 := by
  decide

def realAnalysisUnconstructibles : List UnconstructibleProblem :=
  [ { name := "banach_tarski_paradox", reducesTo := "ac_non_constructive", dependencies := ["lebesgue_measure", "sigma_algebra", "measurable_set"], externalRef := "https://en.wikipedia.org/wiki/Banach%E2%80%93Tarski_paradox", greenVerified := true },
    { name := "vitali_set_non_measurable", reducesTo := "ac_non_constructive", dependencies := ["lebesgue_measure", "sigma_algebra", "measurable_set"], externalRef := "https://en.wikipedia.org/wiki/Vitali_set", greenVerified := true },
    { name := "lebesgue_measure_borel", reducesTo := "undecidable", dependencies := ["lebesgue_measure", "sigma_algebra", "measurable_set", "outer_measure"], externalRef := "https://en.wikipedia.org/wiki/Lebesgue_measure", greenVerified := true },
    { name := "riemann_integrability_characterization", reducesTo := "undecidable", dependencies := ["riemann_integral", "lebesgue_measure", "measurable_function"], externalRef := "https://en.wikipedia.org/wiki/Riemann_integral", greenVerified := true },
    { name := "improper_integral_convergence", reducesTo := "undecidable", dependencies := ["riemann_integral", "lebesgue_integral", "convergent_sequence"], externalRef := "https://en.wikipedia.org/wiki/Improper_integral", greenVerified := true },
    { name := "function_space_separability", reducesTo := "undecidable", dependencies := ["lp_space", "l2_hilbert_space", "measurable_function"], externalRef := "https://en.wikipedia.org/wiki/Lp_space", greenVerified := true },
    { name := "distribution_generalized_function", reducesTo := "undecidable", dependencies := ["measurable_function", "lebesgue_integral", "l2_hilbert_space"], externalRef := "https://en.wikipedia.org/wiki/Distribution_(mathematics)", greenVerified := true } ]

theorem realAnalysisUnconstructibles_length : realAnalysisUnconstructibles.length = 7 := by
  decide

def realAnalysisPackage : AxiomPackageInstance :=
  { name := "real_analysis", version := "1.0.0", templates := realAnalysisTemplates,
    unconstructibles := realAnalysisUnconstructibles, bottomGeometry := "real_number_line_dedekind_complete",
    negationEncoding := "classical_complement_in_measure_space", contradictionBehavior := "explosion_principle" }

def realAnalysisExecutableRules : List ExecutableRule :=
  realAnalysisTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem realAnalysisExecutableRules_length : realAnalysisExecutableRules.length = 43 := by
  decide

/-! ## Functional Analysis 公理包实例 -/

def functionalAnalysisTemplateNamesRaw : List String :=
  ["normed_vector_space", "banach_space", "dual_space", "hahn_banach_theorem", "separation_theorem",
   "inner_product_space", "hilbert_space", "orthonormal_basis", "projection_theorem", "riesz_representation", "parseval_identity",
   "bounded_linear_operator", "operator_norm", "operator_adjoint", "compact_operator", "self_adjoint_operator", "unitary_operator",
   "spectrum_of_operator", "spectral_radius", "resolvent_set", "spectral_theorem", "eigenvalue_problem", "spectral_decomposition",
   "banach_algebra", "c_star_algebra", "von_neumann_algebra", "gel_fand_naimark", "functional_calculus",
   "sobolev_space", "weak_derivative", "trace_theorem", "embedding_theorem",
   "test_function_space", "distribution_theory", "fourier_transform", "convolution", "singularity_theorem"]

def functionalAnalysisTemplates : List PackageTemplate :=
  functionalAnalysisTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "functional_analysis" })

theorem functionalAnalysisTemplates_length : functionalAnalysisTemplates.length = 37 := by
  decide

def functionalAnalysisUnconstructibles : List UnconstructibleProblem :=
  [ { name := "invariant_subspace_problem", reducesTo := "open_problem", dependencies := ["bounded_linear_operator", "hilbert_space", "banach_space"], externalRef := "https://en.wikipedia.org/wiki/Invariant_subspace_problem", greenVerified := true },
    { name := "approximation_property", reducesTo := "undecidable", dependencies := ["banach_space", "bounded_linear_operator", "operator_norm"], externalRef := "https://en.wikipedia.org/wiki/Aproximation_property", greenVerified := true },
    { name := "komornik_loreti_constant", reducesTo := "open_problem", dependencies := ["banach_space", "fourier_transform", "operator_norm"], externalRef := "https://en.wikipedia.org/wiki/Komornik%E2%80%93Loreti_constant", greenVerified := true },
    { name := "boundedness_of_singular_integrals", reducesTo := "undecidable", dependencies := ["operator_norm", "bounded_linear_operator", "lp_space"], externalRef := "https://en.wikipedia.org/wiki/Singular_integral_operators", greenVerified := true },
    { name := "spectral_theorem_self_adjoint", reducesTo := "open_problem", dependencies := ["spectral_theorem", "self_adjoint_operator", "spectral_decomposition"], externalRef := "https://en.wikipedia.org/wiki/Spectral_theorem", greenVerified := true },
    { name := "existence_of_complement", reducesTo := "undecidable", dependencies := ["banach_space", "projection_theorem", "dual_space"], externalRef := "https://en.wikipedia.org/wiki/Complemented_subspace", greenVerified := true },
    { name := "continuous_function_algebra", reducesTo := "undecidable", dependencies := ["c_star_algebra", "banach_algebra", "functional_calculus"], externalRef := "https://en.wikipedia.org/wiki/Commutative_C*-algebra", greenVerified := true } ]

theorem functionalAnalysisUnconstructibles_length : functionalAnalysisUnconstructibles.length = 7 := by
  decide

def functionalAnalysisPackage : AxiomPackageInstance :=
  { name := "functional_analysis", version := "1.0.0", templates := functionalAnalysisTemplates,
    unconstructibles := functionalAnalysisUnconstructibles, bottomGeometry := "infinite_dimensional_banach_hilbert_spaces",
    negationEncoding := "operator_norm_complement", contradictionBehavior := "explosion_principle" }

def functionalAnalysisExecutableRules : List ExecutableRule :=
  functionalAnalysisTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem functionalAnalysisExecutableRules_length : functionalAnalysisExecutableRules.length = 37 := by
  decide

/-! ## Probability Theory 公理包实例 -/

def probabilityTheoryTemplateNamesRaw : List String :=
  ["kolmogorov_non_negativity", "kolmogorov_unit_measure", "kolmogorov_sigma_additivity",
   "prob_empty_set_zero", "prob_bounds", "prob_complement_rule", "prob_monotonicity", "prob_union_two", "boole_inequality", "bonferroni_inequality", "prob_continuity_below", "prob_continuity_above", "inclusion_exclusion", "partition_formula", "prob_finite_additivity",
   "conditional_prob_def", "multiplication_rule", "chain_rule", "law_total_probability", "bayes_theorem", "bayes_theorem_partition",
   "independence_pairwise", "independence_mutual", "independence_conditional", "independence_complement", "independence_trivial", "independence_pairwise_not_mutual", "independence_sigma_algebras", "independence_random_variables",
   "random_variable_def", "distribution_function", "random_variable_discrete", "random_variable_continuous", "pmf_def", "pdf_def", "indicator_random_variable", "random_variable_function", "joint_distribution", "marginal_distribution", "random_variable_transformation", "quantile_function",
   "expected_value_def", "expected_value_linearity", "expected_value_indicator", "expected_value_nonnegativity", "expected_value_monotonicity", "variance_def", "variance_linear_transform", "covariance_def", "correlation_coefficient", "cauchy_schwarz_inequality",
   "convergence_almost_sure", "convergence_probability", "convergence_Lp", "convergence_distribution", "monotone_convergence", "dominated_convergence", "fatou_lemma", "convergence_hierarchy",
   "weak_law_large_numbers", "strong_law_large_numbers", "kolmogorov_three_series", "borel_cantelli",
   "central_limit_theorem", "lindeberg_feller_clt", "berry_esseen_theorem", "characteristic_function_convergence",
   "distribution_bernoulli", "distribution_binomial", "distribution_poisson", "distribution_geometric", "distribution_uniform_continuous", "distribution_exponential", "distribution_normal", "distribution_gamma", "distribution_beta", "distribution_chi_squared",
   "stochastic_process_def", "filtration_def", "martingale_def", "markov_property", "stopping_time_def", "optional_stopping_theorem",
   "characteristic_function_def", "characteristic_function_properties", "characteristic_function_uniqueness", "characteristic_function_inversion"]

def probabilityTheoryTemplates : List PackageTemplate :=
  probabilityTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "probability_theory" })

theorem probabilityTheoryTemplates_length : probabilityTheoryTemplates.length = 87 := by
  decide

def probabilityTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "non_measurable_set_existence", reducesTo := "axiom_of_choice", dependencies := ["zfc_set_theory", "measure_theory"], externalRef := "https://en.wikipedia.org/wiki/Non-measurable_set", greenVerified := true },
    { name := "vitali_set_non_measurable", reducesTo := "non_measurable_set_existence", dependencies := ["axiom_of_choice"], externalRef := "https://en.wikipedia.org/wiki/Vitali_set", greenVerified := true },
    { name := "banach_tarski_paradox", reducesTo := "non_measurable_set_existence", dependencies := ["axiom_of_choice"], externalRef := "https://en.wikipedia.org/wiki/Banach%E2%80%93Tarski_paradox", greenVerified := true },
    { name := "solovay_all_sets_measurable", reducesTo := "inaccessible_cardinal_existence", dependencies := ["zfc_set_theory"], externalRef := "https://en.wikipedia.org/wiki/Solovay_model", greenVerified := true },
    { name := "slln_without_sigma_additivity", reducesTo := "kolmogorov_sigma_additivity", dependencies := ["kolmogorov_sigma_additivity"], externalRef := "https://en.wikipedia.org/wiki/Law_of_large_numbers", greenVerified := true },
    { name := "exact_continuous_simulation", reducesTo := "computational_limits", dependencies := [], externalRef := "https://en.wikipedia.org/wiki/Pseudorandom_number_generator", greenVerified := true },
    { name := "exact_probability_computation", reducesTo := "computational_complexity", dependencies := ["computational_complexity_theory"], externalRef := "https://en.wikipedia.org/wiki/%E2%99%FP", greenVerified := true },
    { name := "regular_conditional_probability_general", reducesTo := "measure_theory_limitations", dependencies := ["measure_theory"], externalRef := "https://en.wikipedia.org/wiki/Regular_conditional_probability", greenVerified := true } ]

theorem probabilityTheoryUnconstructibles_length : probabilityTheoryUnconstructibles.length = 8 := by
  decide

def probabilityTheoryPackage : AxiomPackageInstance :=
  { name := "probability_theory", version := "1.0.0", templates := probabilityTheoryTemplates,
    unconstructibles := probabilityTheoryUnconstructibles, bottomGeometry := "probability_space",
    negationEncoding := "event_complement", contradictionBehavior := "explosion_principle" }

def probabilityTheoryExecutableRules : List ExecutableRule :=
  probabilityTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem probabilityTheoryExecutableRules_length : probabilityTheoryExecutableRules.length = 87 := by
  decide

/-! ## Algebraic Geometry 公理包实例 -/

def algebraicGeometryTemplateNamesRaw : List String :=
  ["affine_space", "algebraic_set", "zariski_topology", "coordinate_ring", "hilbert_nullstellensatz", "ideal_variety_correspondence",
   "projective_space", "projective_variety", "homogeneous_coordinates", "serre_duality", "projective_normality",
   "scheme_theory", "affine_scheme", "locally_ringed_space", "separated_morphism", "proper_morphism", "morphism_of_schemes",
   "sheaf_of_rings", "presheaf", "sheafification", "cohomology_group", "serre_criterion", "leray_cover",
   "krull_dimension", "codimension", "dimension_theorem", "transcendence_degree", "regular_local_ring",
   "algebraic_curve", "elliptic_curve_scheme", "abelian_variety", "grassmannian", "flag_variety",
   "noetherian_ring", "artinian_ring", "integral_extension", "noether_normalization", "zariski_main_theorem"]

def algebraicGeometryTemplates : List PackageTemplate :=
  algebraicGeometryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "algebraic_geometry" })

theorem algebraicGeometryTemplates_length : algebraicGeometryTemplates.length = 38 := by
  decide

def algebraicGeometryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "hartshorne_conjecture", reducesTo := "open_problem", dependencies := ["projective_variety", "hilbert_nullstellensatz", "coordinate_ring"], externalRef := "https://en.wikipedia.org/wiki/Hartshorne_conjecture", greenVerified := true },
    { name := "minimal_model_program", reducesTo := "open_problem", dependencies := ["morphism_of_schemes", "proper_morphism", "separated_morphism", "cohomology_group"], externalRef := "https://en.wikipedia.org/wiki/Minimal_model_program", greenVerified := true },
    { name := "resolution_of_singularities", reducesTo := "proven_hard", dependencies := ["morphism_of_schemes", "proper_morphism", "projective_variety", "cohomology_group"], externalRef := "https://en.wikipedia.org/wiki/Resolution_of_singularities", greenVerified := true },
    { name := "cohomology_ring_computation", reducesTo := "undecidable", dependencies := ["cohomology_group", "sheaf_of_rings", "serre_duality"], externalRef := "https://en.wikipedia.org/wiki/Cohomology_ring", greenVerified := true },
    { name := "rational_point_existence", reducesTo := "undecidable", dependencies := ["algebraic_set", "coordinate_ring", "affine_space"], externalRef := "https://en.wikipedia.org/wiki/Rational_point", greenVerified := true },
    { name := "hilbert_sixteenth_problem", reducesTo := "open_problem", dependencies := ["algebraic_curve", "projective_variety", "cohomology_group"], externalRef := "https://en.wikipedia.org/wiki/Hilbert%27s_sixteenth_problem", greenVerified := true } ]

theorem algebraicGeometryUnconstructibles_length : algebraicGeometryUnconstructibles.length = 6 := by
  decide

def algebraicGeometryPackage : AxiomPackageInstance :=
  { name := "algebraic_geometry", version := "1.0.0", templates := algebraicGeometryTemplates,
    unconstructibles := algebraicGeometryUnconstructibles, bottomGeometry := "polynomial_equation_solutions_in_affine_projective_space",
    negationEncoding := "sheaf_stalk_complement", contradictionBehavior := "explosion_principle" }

def algebraicGeometryExecutableRules : List ExecutableRule :=
  algebraicGeometryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem algebraicGeometryExecutableRules_length : algebraicGeometryExecutableRules.length = 38 := by
  decide

/-! ## Information Theory 公理包实例 -/

def informationTheoryTemplateNamesRaw : List String :=
  ["entropy_non_negativity", "entropy_continuity", "entropy_maximum_uniform", "entropy_additivity", "entropy_expansibility",
   "shannon_entropy", "binary_entropy", "entropy_deterministic_zero", "entropy_concavity", "entropy_schur_concavity", "entropy_grouping_property", "fano_inequality", "data_processing_inequality",
   "joint_entropy", "conditional_entropy", "entropy_chain_rule", "joint_entropy_bounds", "conditional_entropy_non_negativity", "conditioning_reduces_entropy", "independence_entropy_equivalence",
   "mutual_information", "mutual_information_symmetry", "mutual_information_non_negativity", "mutual_information_independence", "mutual_information_kl_divergence", "conditional_mutual_information", "mutual_information_chain_rule", "interaction_information",
   "kl_divergence", "kl_divergence_non_negativity", "kl_divergence_asymmetry", "kl_divergence_convexity", "cross_entropy", "cross_entropy_lower_bound", "jensen_shannon_divergence",
   "source_coding_theorem", "kraft_inequality", "optimal_code_length", "huffman_optimality", "shannon_fano_coding", "arithmetic_coding", "asymptotic_equipartition_property", "typical_set",
   "noisy_channel_coding_theorem", "channel_capacity", "channel_capacity_converse", "bsc_capacity", "bec_capacity", "shannon_hartley_theorem", "awgn_channel_capacity", "feedback_no_capacity_increase", "fano_channel_coding",
   "rate_distortion_function", "rate_distortion_theorem", "rate_distortion_convexity", "distortion_rate_function", "shannon_lower_bound", "binary_rate_distortion", "gaussian_rate_distortion",
   "differential_entropy", "differential_entropy_can_be_negative", "gaussian_maximizes_differential_entropy", "gaussian_differential_entropy", "joint_differential_entropy", "conditional_differential_entropy", "continuous_mutual_information",
   "discrete_memoryless_channel", "binary_symmetric_channel", "binary_erasure_channel", "z_channel", "awgn_channel", "broadcast_channel", "multiple_access_channel", "relay_channel",
   "network_coding", "max_flow_min_cut_information", "slepian_wolf_coding", "wyner_ziv_coding", "gelfand_pinsker_theorem",
   "kolmogorov_complexity", "kolmogorov_uncomputability", "kolmogorov_incompressibility", "kolmogorov_entropy_relation", "algorithmic_mutual_information", "solomonoff_induction",
   "compute_entropy", "compute_joint_entropy", "compute_conditional_entropy", "compute_mutual_information", "compute_kl_divergence", "compute_cross_entropy", "compute_channel_capacity", "compute_rate_distortion", "compute_differential_entropy", "construct_typical_set", "construct_huffman_code", "construct_channel_code",
   "entropy_power_inequality", "log_sum_inequality", "shearers_lemma", "hans_inequality", "mrs_gerbers_lemma", "csiszar_sum_identity", "viterbi_decoding", "bcjr_decoding", "blahut_arimoto_algorithm"]

def informationTheoryTemplates : List PackageTemplate :=
  informationTheoryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "information_theory" })

theorem informationTheoryTemplates_length : informationTheoryTemplates.length = 106 := by
  decide

def informationTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "kolmogorov_complexity_computation", reducesTo := "halting_problem", dependencies := ["turing_machine_universality", "program_termination"], externalRef := "https://en.wikipedia.org/wiki/Kolmogorov_complexity#Uncomputability", greenVerified := true },
    { name := "channel_capacity_general_channel", reducesTo := "non_convex_optimization", dependencies := ["channel_model_specification"], externalRef := "https://en.wikipedia.org/wiki/Channel_capacity", greenVerified := true },
    { name := "optimal_prefix_code_construction", reducesTo := "NP-hard_optimization", dependencies := ["source_distribution"], externalRef := "https://en.wikipedia.org/wiki/Source_coding", greenVerified := true },
    { name := "rate_distortion_function_computation", reducesTo := "NP-hard_optimization", dependencies := ["source_distribution", "distortion_measure"], externalRef := "https://en.wikipedia.org/wiki/Rate%E2%80%93distortion_theory", greenVerified := true },
    { name := "minimum_entropy_decoding", reducesTo := "NP-hard_optimization", dependencies := ["channel_model", "received_signal"], externalRef := "https://en.wikipedia.org/wiki/Maximum_likelihood", greenVerified := false },
    { name := "network_coding_capacity_general", reducesTo := "undecidable", dependencies := ["network_topology", "channel_models"], externalRef := "https://en.wikipedia.org/wiki/Network_coding", greenVerified := true },
    { name := "information_theoretic_security_verification", reducesTo := "undecidable", dependencies := ["cryptographic_protocol", "adversary_model"], externalRef := "https://en.wikipedia.org/wiki/Information_theoretic_security", greenVerified := false },
    { name := "solomonoff_prior_approximation", reducesTo := "kolmogorov_complexity_computation", dependencies := ["universal_turing_machine"], externalRef := "https://en.wikipedia.org/wiki/Solomonoff%27s_theory_of_inductive_inference", greenVerified := true } ]

theorem informationTheoryUnconstructibles_length : informationTheoryUnconstructibles.length = 8 := by
  decide

def informationTheoryPackage : AxiomPackageInstance :=
  { name := "information_theory", version := "1.0.0", templates := informationTheoryTemplates,
    unconstructibles := informationTheoryUnconstructibles, bottomGeometry := "probability_space_measure_theory",
    negationEncoding := "classical_measure_theoretic", contradictionBehavior := "explosion_principle" }

def informationTheoryExecutableRules : List ExecutableRule :=
  informationTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem informationTheoryExecutableRules_length : informationTheoryExecutableRules.length = 106 := by
  decide

/-! ## Linear Algebra 公理包实例 -/

def linearAlgebraTemplateNamesRaw : List String :=
  ["vector_addition_associativity", "vector_addition_commutativity", "vector_additive_identity", "vector_additive_inverse", "scalar_multiplication_compatibility", "scalar_identity", "scalar_distributivity_vector", "scalar_distributivity_field",
   "zero_scalar_annihilates", "zero_vector_annihilates", "negation_via_scalar", "zero_divisor_property", "vector_subtraction", "scalar_subtraction",
   "subspace_test", "span", "linear_independence", "linear_dependence", "quotient_space", "direct_sum",
   "basis", "dimension", "finite_dimensional", "infinite_dimensional", "dimension_theorem", "rank_nullity_theorem", "basis_extension", "steinitz_exchange",
   "linear_map", "linear_map_composition", "kernel", "image", "injectivity_criterion", "surjectivity_criterion", "isomorphism", "hom_space", "endomorphism_ring", "general_linear_group",
   "matrix", "matrix_addition", "matrix_multiplication", "scalar_matrix_multiplication", "matrix_transpose", "identity_matrix", "zero_matrix", "matrix_inverse", "matrix_trace", "matrix_rank",
   "determinant", "determinant_multiplicative", "determinant_transpose", "cofactor_expansion", "cramers_rule", "adjugate_matrix",
   "eigenvalue", "eigenvector", "characteristic_polynomial", "eigenspace", "algebraic_multiplicity", "geometric_multiplicity", "diagonalizability", "cayley_hamilton",
   "inner_product", "conjugate_symmetry", "inner_product_linearity", "positive_definiteness", "norm_from_inner_product", "orthogonality", "orthogonal_complement", "gram_schmidt",
   "dual_space", "dual_basis", "double_dual", "annihilator", "tensor_product", "tensor_universal_property", "exterior_product", "symmetric_product",
   "jordan_normal_form", "rational_canonical_form", "singular_value_decomposition", "qr_decomposition", "lu_decomposition", "spectral_theorem",
   "direct_product", "external_direct_sum", "induced_quotient_map", "pullback", "pushout", "multilinear_map"]

def linearAlgebraTemplates : List PackageTemplate :=
  linearAlgebraTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "linear_algebra" })

theorem linearAlgebraTemplates_length : linearAlgebraTemplates.length = 90 := by
  decide

def linearAlgebraUnconstructibles : List UnconstructibleProblem :=
  [ { name := "matrix_mortality_problem", reducesTo := "undecidable", dependencies := ["matrix_multiplication", "zero_matrix", "matrix"], externalRef := "https://en.wikipedia.org/wiki/Mortality_problem", greenVerified := true },
    { name := "matrix_semigroup_membership", reducesTo := "undecidable", dependencies := ["matrix_multiplication", "matrix", "identity_matrix"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_in_matrix_semigroups", greenVerified := true },
    { name := "matrix_semigroup_equality", reducesTo := "undecidable", dependencies := ["matrix_multiplication", "matrix"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_in_matrix_semigroups", greenVerified := true },
    { name := "matrix_nilpotency_problem", reducesTo := "undecidable", dependencies := ["matrix_multiplication", "zero_matrix", "matrix", "matrix_mortality_problem"], externalRef := "https://en.wikipedia.org/wiki/Mortality_problem", greenVerified := true },
    { name := "basis_existence_infinite_dimensional", reducesTo := "requires_axiom_of_choice", dependencies := ["basis", "infinite_dimensional", "basis_extension"], externalRef := "https://en.wikipedia.org/wiki/Basis_(linear_algebra)", greenVerified := true },
    { name := "vector_space_isomorphism_problem", reducesTo := "undecidable", dependencies := ["isomorphism", "dimension", "basis", "linear_map"], externalRef := "https://en.wikipedia.org/wiki/Vector_space", greenVerified := true },
    { name := "tensor_rank_problem", reducesTo := "np_hard", dependencies := ["tensor_product", "multilinear_map"], externalRef := "https://en.wikipedia.org/wiki/Tensor_rank", greenVerified := true },
    { name := "eigenvalue_sensitivity_nonnormal", reducesTo := "numerically_unstable", dependencies := ["eigenvalue", "characteristic_polynomial", "algebraic_multiplicity", "geometric_multiplicity"], externalRef := "https://en.wikipedia.org/wiki/Eigenvalues_and_eigenvectors", greenVerified := true } ]

theorem linearAlgebraUnconstructibles_length : linearAlgebraUnconstructibles.length = 8 := by
  decide

def linearAlgebraPackage : AxiomPackageInstance :=
  { name := "linear_algebra", version := "1.0.0", templates := linearAlgebraTemplates,
    unconstructibles := linearAlgebraUnconstructibles, bottomGeometry := "vector_space_over_field",
    negationEncoding := "classical_equality", contradictionBehavior := "explosion_principle" }

def linearAlgebraExecutableRules : List ExecutableRule :=
  linearAlgebraTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem linearAlgebraExecutableRules_length : linearAlgebraExecutableRules.length = 90 := by
  decide


end Instances
end Axioms
end Theory
end lvFormal
