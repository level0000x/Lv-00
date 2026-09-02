import lvFormal.Theory.Axioms.Instances_Core
open lvFormal.Theory.Axioms.Instances
open lvFormal.Theory.Axioms.RuleTemplate

namespace lvFormal
namespace Theory
namespace Axioms
namespace Instances

/-! ## Affine Geometry 公理包实例 -/

def affineGeometryTemplateNamesRaw : List String :=
  ["line_through_two_points", "line_has_two_points", "existence_of_triangle", "existence_of_affine_frame_3d",
   "parallel_through_point", "parallelism_reflexive", "parallelism_transitive", "parallelism_no_intersection",
   "point_subtraction", "point_translation", "vector_addition_associative", "vector_addition_commutative", "zero_vector_exists", "vector_negation", "scalar_multiplication", "scalar_distributivity_vectors", "scalar_distributivity_scalars", "scalar_multiplication_associative", "scalar_unit",
   "subtraction_identity", "subtraction_chain", "translation_subtraction_compat", "subtraction_translation_compat",
   "affine_combination_two", "midpoint", "centroid_three_points", "general_barycenter", "affine_combination_associative", "affine_combination_commutative", "affine_combination_idempotent",
   "affine_map_preserves_combination", "translation_map", "affine_map_decomposition", "affine_map_preserves_parallelism", "affine_map_preserves_ratio", "affine_map_preserves_midpoint", "affine_map_preserves_centroid", "affine_map_preserves_barycenter",
   "desargues_theorem_affine",
   "pappus_theorem_affine",
   "affine_subspace_check", "affine_span_two_points", "affine_span_three_points", "affine_subspace_dimension", "affine_subspaces_parallel",
   "simple_ratio", "thales_intercept_theorem", "ceva_theorem", "menelaus_theorem",
   "parallelogram_fourth_vertex", "parallelogram_law", "parallelogram_diagonal_bisect", "affine_map_preserves_parallelogram",
   "projective_completion", "line_at_infinity", "affine_patch_from_projective"]

def affineGeometryTemplates : List PackageTemplate :=
  affineGeometryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "affine_geometry" })

theorem affineGeometryTemplates_length : affineGeometryTemplates.length = 56 := by
  decide

def affineGeometryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "perpendicular_bisector", reducesTo := "orthogonality requires metric structure", dependencies := ["affine_geometry", "euclidean_plane"], externalRef := "https://en.wikipedia.org/wiki/Affine_geometry", greenVerified := true },
    { name := "angle_trisection", reducesTo := "requires metric structure and solving cubic equations", dependencies := ["affine_geometry", "euclidean_plane"], externalRef := "https://en.wikipedia.org/wiki/Angle_trisection", greenVerified := true },
    { name := "circle_construction", reducesTo := "circles require metric distance notion absent in affine geometry", dependencies := ["affine_geometry", "euclidean_plane"], externalRef := "https://en.wikipedia.org/wiki/Affine_geometry", greenVerified := true },
    { name := "finite_affine_plane_non_prime_power", reducesTo := "existence of finite affine planes of non-prime-power order is an open problem in combinatorics; equivalent to existence of finite projective planes", dependencies := ["affine_geometry", "combinatorics", "projective_geometry"], externalRef := "https://en.wikipedia.org/wiki/Affine_plane_(incidence_geometry)", greenVerified := false },
    { name := "non_desarguesian_classification", reducesTo := "classification of non-Desarguesian affine planes is wildly open; only partial results known", dependencies := ["affine_geometry", "projective_geometry"], externalRef := "https://en.wikipedia.org/wiki/Non-Desarguesian_plane", greenVerified := false },
    { name := "metric_recovery_from_affine", reducesTo := "an affine space admits infinitely many inequivalent metric structures; no canonical choice without additional data", dependencies := ["affine_geometry", "euclidean_plane"], externalRef := "https://en.wikipedia.org/wiki/Affine_space", greenVerified := true },
    { name := "area_computation", reducesTo := "area requires a notion of determinant or metric; only ratios of areas on parallel lines are affine invariants", dependencies := ["affine_geometry", "euclidean_plane"], externalRef := "https://en.wikipedia.org/wiki/Affine_geometry", greenVerified := true } ]

theorem affineGeometryUnconstructibles_length : affineGeometryUnconstructibles.length = 7 := by
  decide

def affineGeometryPackage : AxiomPackageInstance :=
  { name := "affine_geometry", version := "1.0.0", templates := affineGeometryTemplates,
    unconstructibles := affineGeometryUnconstructibles, bottomGeometry := "affine_space",
    negationEncoding := "classical_material_implication", contradictionBehavior := "explosion_principle" }

def affineGeometryExecutableRules : List ExecutableRule :=
  affineGeometryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem affineGeometryExecutableRules_length : affineGeometryExecutableRules.length = 56 := by
  decide

/-! ## Algebraic Topology 公理包实例 -/

def algebraicTopologyTemplateNamesRaw : List String :=
  ["fundamental_group", "fundamental_groupoid", "covering_space", "universal_cover", "monodromy", "van_kampen_theorem",
   "simplicial_complex", "simplicial_homology", "chain_complex", "boundary_operator", "homology_group", "euler_characteristic", "betti_number",
   "singular_homology", "singular_cohomology", "mayer_vietoris_sequence", "excision_theorem", "universal_coefficient_theorem", "kuenneth_formula",
   "cup_product", "cohomology_ring", "poincare_duality", "de_rham_cohomology", "sheaf_cohomology",
   "homotopy_group", "hurewicz_theorem", "whitehead_theorem", "fibration", "cofibration", "long_exact_sequence_fibration",
   "lefschetz_fixed_point", "lefschetz_number", "intersection_theory", "poincare_lemma",
   "vector_bundle", "k_group", "bott_periodicity", "atiyah_singer_index"]

def algebraicTopologyTemplates : List PackageTemplate :=
  algebraicTopologyTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "algebraic_topology" })

theorem algebraicTopologyTemplates_length : algebraicTopologyTemplates.length = 38 := by
  decide

def algebraicTopologyUnconstructibles : List UnconstructibleProblem :=
  [ { name := "homotopy_group_computation", reducesTo := "undecidable", dependencies := ["homotopy_group", "fibration", "long_exact_sequence_fibration"], externalRef := "https://en.wikipedia.org/wiki/Homotopy_groups_of_spheres", greenVerified := true },
    { name := "homology_isomorphism_problem", reducesTo := "undecidable", dependencies := ["homology_group", "singular_homology", "simplicial_homology"], externalRef := "https://en.wikipedia.org/wiki/Homology_(mathematics)", greenVerified := true },
    { name := "knot_classification", reducesTo := "undecidable", dependencies := ["fundamental_group", "homology_group", "covering_space"], externalRef := "https://en.wikipedia.org/wiki/Knot_theory", greenVerified := true },
    { name := "homeomorphism_problem_manifolds", reducesTo := "undecidable", dependencies := ["homology_group", "homotopy_group", "poincare_duality"], externalRef := "https://en.wikipedia.org/wiki/Homeomorphism", greenVerified := true },
    { name := "simple_homotopy_equivalence", reducesTo := "undecidable", dependencies := ["homotopy_group", "homology_group", "whitehead_theorem"], externalRef := "https://en.wikipedia.org/wiki/Simple-homotopy_equivalence", greenVerified := true },
    { name := "group_presentation_triviality", reducesTo := "undecidable", dependencies := ["fundamental_group", "van_kampen_theorem", "covering_space"], externalRef := "https://en.wikipedia.org/wiki/Word_problem_for_groups", greenVerified := true },
    { name := "manifold_triangulation", reducesTo := "undecidable", dependencies := ["simplicial_complex", "simplicial_homology", "homology_group"], externalRef := "https://en.wikipedia.org/wiki/Triangulation_(topology)", greenVerified := true } ]

theorem algebraicTopologyUnconstructibles_length : algebraicTopologyUnconstructibles.length = 7 := by
  decide

def algebraicTopologyPackage : AxiomPackageInstance :=
  { name := "algebraic_topology", version := "1.0.0", templates := algebraicTopologyTemplates,
    unconstructibles := algebraicTopologyUnconstructibles, bottomGeometry := "topological_spaces_with_algebraic_invariants",
    negationEncoding := "abelian_group_complement", contradictionBehavior := "explosion_principle" }

def algebraicTopologyExecutableRules : List ExecutableRule :=
  algebraicTopologyTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem algebraicTopologyExecutableRules_length : algebraicTopologyExecutableRules.length = 38 := by
  decide

/-! ## Elliptic Geometry 公理包实例 -/

def ellipticGeometryTemplateNamesRaw : List String :=
  ["line_through_two_points", "line_has_two_points", "existence_of_triangle", "any_two_lines_intersect",
   "separation_relation", "separation_symmetry", "separation_transitivity", "separation_extension", "bounded_segment_transport",
   "segment_congruence_reflexive", "segment_congruence_transitive",
   "angle_transport", "angle_congruence_properties", "SAS_congruence",
   "no_parallel_lines", "projective_incidence_property", "absolute_polar_line", "absolute_pole",
   "elliptic_distance", "triangle_angle_excess", "polar_triangle", "similarity_implies_congruence",
   "elliptic_archimedes_axiom", "elliptic_line_completeness", "elliptic_area",
   "spherical_model", "projective_model", "gnomonic_projection",
   "perpendicular_from_point", "elliptic_midpoint_pair"]

def ellipticGeometryTemplates : List PackageTemplate :=
  ellipticGeometryTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "elliptic_geometry" })

theorem ellipticGeometryTemplates_length : ellipticGeometryTemplates.length = 30 := by
  decide

def ellipticGeometryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "squaring_the_circle_elliptic", reducesTo := "transcendental_number", dependencies := ["elliptic_distance", "elliptic_area", "elliptic_line_completeness"], externalRef := "https://en.wikipedia.org/wiki/Squaring_the_circle", greenVerified := true },
    { name := "angle_trisection_elliptic", reducesTo := "cubic_equation_solving", dependencies := ["angle_transport", "SAS_congruence", "bounded_segment_transport"], externalRef := "https://en.wikipedia.org/wiki/Angle_trisection", greenVerified := true },
    { name := "doubling_the_cube_elliptic", reducesTo := "cube_root_of_two", dependencies := ["bounded_segment_transport", "elliptic_distance"], externalRef := "https://en.wikipedia.org/wiki/Doubling_the_cube", greenVerified := true },
    { name := "regular_heptagon_elliptic", reducesTo := "cubic_equation_solving", dependencies := ["bounded_segment_transport", "angle_transport", "elliptic_distance"], externalRef := "https://en.wikipedia.org/wiki/Constructible_polygon", greenVerified := true },
    { name := "constructible_length_characterization", reducesTo := "algebraic_number_theory", dependencies := ["elliptic_distance", "elliptic_line_completeness", "bounded_segment_transport"], externalRef := "https://en.wikipedia.org/wiki/Constructible_number", greenVerified := true },
    { name := "triangle_similarity_without_congruence", reducesTo := "compactness_of_elliptic_space", dependencies := ["similarity_implies_congruence", "SAS_congruence", "triangle_angle_excess"], externalRef := "https://en.wikipedia.org/wiki/Elliptic_geometry", greenVerified := true } ]

theorem ellipticGeometryUnconstructibles_length : ellipticGeometryUnconstructibles.length = 6 := by
  decide

def ellipticGeometryPackage : AxiomPackageInstance :=
  { name := "elliptic_geometry", version := "1.0.0", templates := ellipticGeometryTemplates,
    unconstructibles := ellipticGeometryUnconstructibles, bottomGeometry := "elliptic_plane_RP2",
    negationEncoding := "classical_material_implication", contradictionBehavior := "explosion_principle" }

def ellipticGeometryExecutableRules : List ExecutableRule :=
  ellipticGeometryTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem ellipticGeometryExecutableRules_length : ellipticGeometryExecutableRules.length = 30 := by
  decide

/-! ## Metric Space 公理包实例 -/

def metricSpaceTemplateNamesRaw : List String :=
  ["metric_non_negativity", "metric_symmetry", "triangle_inequality", "identity_of_indiscernibles",
   "open_ball", "closed_ball", "sphere", "point_set_distance", "set_set_distance", "diameter",
   "metric_open_set", "metric_closed_set", "interior", "closure", "boundary", "hausdorff_separation",
   "sequence_convergence", "cauchy_sequence", "completeness", "cauchy_completion", "banach_fixed_point", "baire_category_theorem",
   "pointwise_continuity", "uniform_continuity", "lipschitz_continuity", "contraction_map", "isometry", "uniform_extension_to_completion",
   "bounded_set", "totally_bounded", "sequential_compactness", "compact_equals_complete_totally_bounded", "lebesgue_number_lemma", "arzela_ascoli",
   "connected_set", "path_connected_set", "connected_component",
   "product_metric_linf", "product_metric_l2", "product_metric_l1", "quotient_metric", "hausdorff_distance", "subspace_metric",
   "discrete_metric", "euclidean_metric_Rn", "sup_metric", "weighted_metric"]

def metricSpaceTemplates : List PackageTemplate :=
  metricSpaceTemplateNamesRaw.map (fun n => { name := n, paramCount := 2, group := "metric_space" })

theorem metricSpaceTemplates_length : metricSpaceTemplates.length = 47 := by
  decide

def metricSpaceUnconstructibles : List UnconstructibleProblem :=
  [ { name := "isometric_embedding_into_l2", reducesTo := "gram_matrix_positive_semi_definiteness", dependencies := ["metric_non_negativity", "triangle_inequality", "identity_of_indiscernibles"], externalRef := "https://en.wikipedia.org/wiki/Metric_space#Embeddings", greenVerified := true },
    { name := "separable_metric_space_classification", reducesTo := "uncountable_isometry_classes", dependencies := ["metric_non_negativity", "completeness", "hausdorff_separation"], externalRef := "https://en.wikipedia.org/wiki/Separable_space", greenVerified := true },
    { name := "urysohn_universal_space_existence", reducesTo := "requires_axiom_of_choice", dependencies := ["completeness", "triangle_inequality", "isometry"], externalRef := "https://en.wikipedia.org/wiki/Urysohn_universal_space", greenVerified := true },
    { name := "finite_metric_space_isometry", reducesTo := "graph_isomorphism", dependencies := ["metric_non_negativity", "metric_symmetry", "triangle_inequality", "identity_of_indiscernibles"], externalRef := "https://en.wikipedia.org/wiki/Graph_isomorphism_problem", greenVerified := true },
    { name := "finite_metric_embedding_into_Rn", reducesTo := "NP_hard_optimization", dependencies := ["euclidean_metric_Rn", "triangle_inequality", "identity_of_indiscernibles"], externalRef := "https://en.wikipedia.org/wiki/Embedding#Metric_space_embeddings", greenVerified := true },
    { name := "hausdorff_distance_computability", reducesTo := "non_computable_in_computable_analysis", dependencies := ["hausdorff_distance", "closure", "completeness"], externalRef := "https://en.wikipedia.org/wiki/Hausdorff_distance", greenVerified := true },
    { name := "baire_category_without_choice", reducesTo := "requires_dependent_choice", dependencies := ["baire_category_theorem", "completeness", "cauchy_sequence"], externalRef := "https://en.wikipedia.org/wiki/Baire_category_theorem", greenVerified := true },
    { name := "general_metrizability", reducesTo := "nagata_smirnov_conditions", dependencies := ["metric_open_set", "hausdorff_separation", "triangle_inequality"], externalRef := "https://en.wikipedia.org/wiki/Metrization_theorem", greenVerified := true } ]

theorem metricSpaceUnconstructibles_length : metricSpaceUnconstructibles.length = 8 := by
  decide

def metricSpacePackage : AxiomPackageInstance :=
  { name := "metric_space", version := "1.0.0", templates := metricSpaceTemplates,
    unconstructibles := metricSpaceUnconstructibles, bottomGeometry := "metric_space_general",
    negationEncoding := "classical_distance_negation", contradictionBehavior := "explosion_principle" }

def metricSpaceExecutableRules : List ExecutableRule :=
  metricSpaceTemplates.mapIdx fun i t => templateToExecutableRule i t

theorem metricSpaceExecutableRules_length : metricSpaceExecutableRules.length = 47 := by
  decide


end Instances
end Axioms
end Theory
end lvFormal
