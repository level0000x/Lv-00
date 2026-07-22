import Lv00Formal.Theory.Axioms.Instances_Core
open Lv00Formal.Theory.Axioms.Instances
open Lv00Formal.Theory.Axioms.RuleTemplate

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace Instances

/-! ## Euclidean Plane 公理包实例

以下内容对照 `euclidean_plane.lvz` 与 `test_axiom_euclidean_plane.c`：
- 22 个模板（16 核心公理 + 6 导出构造器）；
- 6 个不可构造问题（全部 green_verified=true）；
- bottom_geometry = euclidean_plane；
- negation_encoding = classical_material_implication；
- contradiction_behavior = explosion_principle。
-/

/-- Euclidean Plane 包中的 22 个模板。 -/
def euclideanPlaneTemplates : List PackageTemplate :=
  [ -- Group I: Incidence Axioms
    { name := "line_through_two_points", paramCount := 2, group := "incidence" },
    { name := "line_has_two_points", paramCount := 1, group := "incidence" },
    { name := "existence_of_triangle", paramCount := 0, group := "incidence" },
    -- Group II: Order Axioms
    { name := "betweenness_symmetry", paramCount := 3, group := "order" },
    { name := "extend_segment", paramCount := 2, group := "order" },
    { name := "betweenness_uniqueness", paramCount := 3, group := "order" },
    { name := "pasch_axiom", paramCount := 4, group := "order" },
    -- Group III: Congruence Axioms
    { name := "segment_transport", paramCount := 4, group := "congruence" },
    { name := "segment_congruence_reflexive", paramCount := 2, group := "congruence" },
    { name := "segment_congruence_transitive", paramCount := 6, group := "congruence" },
    { name := "angle_transport", paramCount := 5, group := "congruence" },
    { name := "angle_congruence_properties", paramCount := 6, group := "congruence" },
    { name := "SAS_congruence", paramCount := 6, group := "congruence" },
    -- Group IV: Parallel Axiom
    { name := "unique_parallel", paramCount := 2, group := "parallel" },
    -- Group V: Continuity Axioms
    { name := "archimedes_axiom", paramCount := 4, group := "continuity" },
    { name := "line_completeness", paramCount := 0, group := "continuity" },
    -- Group VI: Derived Constructors
    { name := "midpoint", paramCount := 2, group := "derived_constructor" },
    { name := "perpendicular_bisector", paramCount := 2, group := "derived_constructor" },
    { name := "perpendicular_from_point", paramCount := 2, group := "derived_constructor" },
    { name := "angle_bisector", paramCount := 3, group := "derived_constructor" },
    { name := "circle_by_center_radius", paramCount := 2, group := "derived_constructor" },
    { name := "line_circle_intersection", paramCount := 3, group := "derived_constructor" }
  ]

/-- C 测试中的 EXPECTED_TEMPLATE_COUNT = 22。 -/
theorem euclideanPlaneTemplates_length : euclideanPlaneTemplates.length = 22 := by
  decide

/-- Euclidean Plane 包中的 6 个不可构造问题。 -/
def euclideanPlaneUnconstructibles : List UnconstructibleProblem :=
  [ { name := "angle_trisection", reducesTo := "cubic_equation_solving",
      dependencies := ["angle_bisector", "circle_by_center_radius", "line_circle_intersection"],
      externalRef := "https://en.wikipedia.org/wiki/Angle_trisection", greenVerified := true },
    { name := "doubling_the_cube", reducesTo := "cube_root_of_two",
      dependencies := ["circle_by_center_radius", "line_circle_intersection", "midpoint"],
      externalRef := "https://en.wikipedia.org/wiki/Doubling_the_cube", greenVerified := true },
    { name := "squaring_the_circle", reducesTo := "pi_transcendence",
      dependencies := ["circle_by_center_radius", "line_circle_intersection", "perpendicular_from_point"],
      externalRef := "https://en.wikipedia.org/wiki/Squaring_the_circle", greenVerified := true },
    { name := "general_quintic_by_radicals", reducesTo := "abel_ruffini_theorem",
      dependencies := ["circle_by_center_radius", "line_circle_intersection"],
      externalRef := "https://en.wikipedia.org/wiki/Abel%E2%80%93Ruffini_theorem", greenVerified := true },
    { name := "construction_of_regular_heptagon", reducesTo := "cubic_equation_solving",
      dependencies := ["circle_by_center_radius", "angle_bisector"],
      externalRef := "https://en.wikipedia.org/wiki/Heptagon", greenVerified := true },
    { name := "circle_squaring_straightedge", reducesTo := "lindemann_weierstrass_theorem",
      dependencies := ["circle_by_center_radius", "line_circle_intersection"],
      externalRef := "https://en.wikipedia.org/wiki/Lindemann%E2%80%93Weierstrass_theorem", greenVerified := true }
  ]

/-- C 测试中的 EXPECTED_UNCONSTRUCTIBLE_COUNT = 6。 -/
theorem euclideanPlaneUnconstructibles_length : euclideanPlaneUnconstructibles.length = 6 := by
  decide

/-- Euclidean Plane 公理包实例。 -/
def euclideanPlanePackage : AxiomPackageInstance :=
  { name := "euclidean_plane",
    version := "1.0.0",
    templates := euclideanPlaneTemplates,
    unconstructibles := euclideanPlaneUnconstructibles,
    bottomGeometry := "euclidean_plane",
    negationEncoding := "classical_material_implication",
    contradictionBehavior := "explosion_principle" }

/-- Euclidean Plane 逻辑框架字段与 C 测试一致。 -/
theorem euclideanPlane_logical_framework :
    euclideanPlanePackage.bottomGeometry = "euclidean_plane" ∧
    euclideanPlanePackage.negationEncoding = "classical_material_implication" ∧
    euclideanPlanePackage.contradictionBehavior = "explosion_principle" := by
  decide

/-- Euclidean Plane 全部 6 个不可构造问题均标记为 green_verified=true。 -/
theorem euclideanPlaneUnconstructibles_green_verified :
    ∀ u ∈ euclideanPlaneUnconstructibles, u.greenVerified = true := by
  decide


/-- 由全部 Euclidean Plane 模板生成的规则实例。 -/
def euclideanPlaneExecutableRules : List ExecutableRule :=
  euclideanPlaneTemplates.mapIdx fun i t => templateToExecutableRule i t

/-- 生成的 Euclidean Plane 规则实例数量与模板数量一致。 -/
theorem euclideanPlaneExecutableRules_length : euclideanPlaneExecutableRules.length = 22 := by
  decide

/-! ## Category Theory 公理包实例

以下内容对照 `category_theory.lvz` 与 `test_axiom_category_theory.c`：
- 60 个模板（11 组：核心原语、公理约束、态射分类、泛对象、极限、函子、自然变换、伴随、特殊范畴、等价与性质、Yoneda 引理）；
- 7 个不可构造问题（全部 green_verified=true）；
- bottom_geometry = directed_multigraph_with_composition；
- negation_encoding = categorical_subobject_complement；
- contradiction_behavior = explosion_principle。
-/

/-- Category Theory 包中的 60 个模板。 -/
def categoryTheoryTemplates : List PackageTemplate :=
  [ -- Group I: Core Primitives & Construction (5)
    { name := "object", paramCount := 0, group := "core_primitive" },
    { name := "morphism", paramCount := 2, group := "core_primitive" },
    { name := "composability", paramCount := 2, group := "core_primitive" },
    { name := "composition", paramCount := 3, group := "core_primitive" },
    { name := "identity_morphism", paramCount := 1, group := "core_primitive" },
    -- Group II: Axiom Constraints (3)
    { name := "associativity", paramCount := 3, group := "axiom_constraint" },
    { name := "left_identity", paramCount := 2, group := "axiom_constraint" },
    { name := "right_identity", paramCount := 2, group := "axiom_constraint" },
    -- Group III: Morphism Classifications (7)
    { name := "monomorphism", paramCount := 3, group := "morphism_classification" },
    { name := "epimorphism", paramCount := 3, group := "morphism_classification" },
    { name := "isomorphism", paramCount := 2, group := "morphism_classification" },
    { name := "endomorphism", paramCount := 1, group := "morphism_classification" },
    { name := "automorphism", paramCount := 2, group := "morphism_classification" },
    { name := "section", paramCount := 2, group := "morphism_classification" },
    { name := "retraction", paramCount := 2, group := "morphism_classification" },
    -- Group IV: Universal Objects (3)
    { name := "initial_object", paramCount := 0, group := "universal_object" },
    { name := "terminal_object", paramCount := 0, group := "universal_object" },
    { name := "zero_object", paramCount := 0, group := "universal_object" },
    -- Group V: Limits & Universal Constructions (11)
    { name := "binary_product", paramCount := 2, group := "limit" },
    { name := "product_projection_left", paramCount := 2, group := "limit" },
    { name := "product_projection_right", paramCount := 2, group := "limit" },
    { name := "binary_coproduct", paramCount := 2, group := "limit" },
    { name := "coproduct_injection_left", paramCount := 2, group := "limit" },
    { name := "coproduct_injection_right", paramCount := 2, group := "limit" },
    { name := "equalizer", paramCount := 2, group := "limit" },
    { name := "coequalizer", paramCount := 2, group := "limit" },
    { name := "pullback", paramCount := 2, group := "limit" },
    { name := "pushout", paramCount := 2, group := "limit" },
    { name := "exponential_object", paramCount := 2, group := "limit" },
    -- Group VI: Functors (8)
    { name := "functor_object_map", paramCount := 2, group := "functor" },
    { name := "functor_morphism_map", paramCount := 2, group := "functor" },
    { name := "functor_preserves_composition", paramCount := 3, group := "functor" },
    { name := "functor_preserves_identity", paramCount := 1, group := "functor" },
    { name := "identity_functor", paramCount := 1, group := "functor" },
    { name := "functor_composition", paramCount := 3, group := "functor" },
    { name := "contravariant_functor", paramCount := 2, group := "functor" },
    { name := "forgetful_functor", paramCount := 1, group := "functor" },
    -- Group VII: Natural Transformations (6)
    { name := "natural_transformation", paramCount := 2, group := "natural_transformation" },
    { name := "natural_transformation_component", paramCount := 3, group := "natural_transformation" },
    { name := "naturality_square", paramCount := 4, group := "natural_transformation" },
    { name := "vertical_composition", paramCount := 2, group := "natural_transformation" },
    { name := "horizontal_composition", paramCount := 2, group := "natural_transformation" },
    { name := "natural_isomorphism", paramCount := 2, group := "natural_transformation" },
    -- Group VIII: Adjunctions (4)
    { name := "adjunction", paramCount := 2, group := "adjunction" },
    { name := "unit_of_adjunction", paramCount := 2, group := "adjunction" },
    { name := "counit_of_adjunction", paramCount := 2, group := "adjunction" },
    { name := "triangle_identities", paramCount := 4, group := "adjunction" },
    -- Group IX: Special Categories & Duality (6)
    { name := "opposite_category", paramCount := 1, group := "special_category" },
    { name := "product_category", paramCount := 2, group := "special_category" },
    { name := "slice_category", paramCount := 2, group := "special_category" },
    { name := "coslice_category", paramCount := 2, group := "special_category" },
    { name := "arrow_category", paramCount := 1, group := "special_category" },
    { name := "monoidal_category", paramCount := 1, group := "special_category" },
    -- Group X: Category Equivalence & Properties (4)
    { name := "equivalence_of_categories", paramCount := 2, group := "equivalence_property" },
    { name := "skeleton", paramCount := 1, group := "equivalence_property" },
    { name := "full_subcategory", paramCount := 2, group := "equivalence_property" },
    { name := "commutative_diagram", paramCount := 2, group := "equivalence_property" },
    -- Group XI: Yoneda Lemma & Representables (3)
    { name := "hom_functor", paramCount := 1, group := "yoneda" },
    { name := "representable_functor", paramCount := 2, group := "yoneda" },
    { name := "yoneda_embedding", paramCount := 1, group := "yoneda" }
  ]

/-- Category Theory 模板数量。 -/
theorem categoryTheoryTemplates_length : categoryTheoryTemplates.length = 60 := by
  decide

/-- Category Theory 包中的 7 个不可构造问题。 -/
def categoryTheoryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "word_problem_for_fp_categories", reducesTo := "undecidable",
      dependencies := ["composition", "associativity", "identity_morphism", "morphism"],
      externalRef := "https://en.wikipedia.org/wiki/Word_problem_for_groups", greenVerified := true },
    { name := "equality_of_morphisms_fpc", reducesTo := "word_problem_for_fp_categories",
      dependencies := ["composition", "associativity", "morphism", "commutative_diagram"],
      externalRef := "https://ncatlab.org/nlab/show/finitely+presented+category", greenVerified := true },
    { name := "isomorphism_of_fp_categories", reducesTo := "undecidable",
      dependencies := ["morphism", "composition", "functor_object_map", "functor_morphism_map", "equivalence_of_categories"],
      externalRef := "https://en.wikipedia.org/wiki/Category_theory", greenVerified := true },
    { name := "existence_of_limit_in_fpc", reducesTo := "undecidable",
      dependencies := ["binary_product", "equalizer", "pullback", "morphism", "composition"],
      externalRef := "https://ncatlab.org/nlab/show/limit", greenVerified := true },
    { name := "is_category_equivalent_to_poset", reducesTo := "undecidable",
      dependencies := ["morphism", "isomorphism", "equivalence_of_categories", "skeleton"],
      externalRef := "https://ncatlab.org/nlab/show/poset", greenVerified := true },
    { name := "finite_model_property_for_fpc", reducesTo := "undecidable",
      dependencies := ["object", "morphism", "composition", "functor_object_map"],
      externalRef := "https://ncatlab.org/nlab/show/finitely+presented+category", greenVerified := true },
    { name := "functor_equivalence_in_fpc", reducesTo := "undecidable",
      dependencies := ["functor_object_map", "functor_morphism_map", "functor_preserves_composition", "equivalence_of_categories", "natural_isomorphism"],
      externalRef := "https://en.wikipedia.org/wiki/Functor", greenVerified := true }
  ]

/-- Category Theory 不可构造问题数量。 -/
theorem categoryTheoryUnconstructibles_length : categoryTheoryUnconstructibles.length = 7 := by
  decide

/-- Category Theory 公理包实例。 -/
def categoryTheoryPackage : AxiomPackageInstance :=
  { name := "category_theory",
    version := "1.0.0",
    templates := categoryTheoryTemplates,
    unconstructibles := categoryTheoryUnconstructibles,
    bottomGeometry := "directed_multigraph_with_composition",
    negationEncoding := "categorical_subobject_complement",
    contradictionBehavior := "explosion_principle" }

/-- Category Theory 逻辑框架字段与 C 测试一致。 -/
theorem categoryTheory_logical_framework :
    categoryTheoryPackage.bottomGeometry = "directed_multigraph_with_composition" ∧
    categoryTheoryPackage.negationEncoding = "categorical_subobject_complement" ∧
    categoryTheoryPackage.contradictionBehavior = "explosion_principle" := by
  decide

/-- 由全部 Category Theory 模板生成的规则实例。 -/
def categoryTheoryExecutableRules : List ExecutableRule :=
  categoryTheoryTemplates.mapIdx fun i t => templateToExecutableRule i t

/-- 生成的 Category Theory 规则实例数量与模板数量一致。 -/
theorem categoryTheoryExecutableRules_length : categoryTheoryExecutableRules.length = 60 := by
  decide

/-! ## Hyperbolic Geometry 公理包实例

以下内容对照 `hyperbolic_geometry.lvz` 与 `test_axiom_hyperbolic_geometry.c`：
- 29 个模板（7 组：关联、顺序、全等、双曲平行、双曲特有、连续性与度量、双曲模型构造）；
- 6 个不可构造问题（全部 green_verified=true）；
- bottom_geometry = hyperbolic_plane；
- negation_encoding = classical_material_implication；
- contradiction_behavior = explosion_principle。
-/

/-- Hyperbolic Geometry 包中的 29 个模板。 -/
def hyperbolicGeometryTemplates : List PackageTemplate :=
  [ -- Group I: Incidence Axioms (same as Euclidean)
    { name := "line_through_two_points", paramCount := 2, group := "incidence" },
    { name := "line_has_two_points", paramCount := 1, group := "incidence" },
    { name := "existence_of_triangle", paramCount := 0, group := "incidence" },
    -- Group II: Betweenness (Order) Axioms (same as Euclidean)
    { name := "betweenness_symmetry", paramCount := 3, group := "order" },
    { name := "extend_segment", paramCount := 2, group := "order" },
    { name := "betweenness_uniqueness", paramCount := 3, group := "order" },
    { name := "pasch_axiom", paramCount := 4, group := "order" },
    -- Group III: Congruence Axioms (same as Euclidean)
    { name := "segment_transport", paramCount := 4, group := "congruence" },
    { name := "segment_congruence_reflexive", paramCount := 2, group := "congruence" },
    { name := "segment_congruence_transitive", paramCount := 6, group := "congruence" },
    { name := "angle_transport", paramCount := 5, group := "congruence" },
    { name := "angle_congruence_properties", paramCount := 6, group := "congruence" },
    { name := "SAS_congruence", paramCount := 6, group := "congruence" },
    -- Group IV: Hyperbolic Parallel Axiom
    { name := "hyperbolic_parallel_existence", paramCount := 2, group := "hyperbolic_parallel" },
    { name := "parallel_through_point_not_unique", paramCount := 2, group := "hyperbolic_parallel" },
    { name := "limiting_parallel_ray", paramCount := 3, group := "hyperbolic_parallel" },
    -- Group V: Hyperbolic-Specific Constructions
    { name := "angle_of_parallelism", paramCount := 2, group := "hyperbolic_specific" },
    { name := "common_perpendicular", paramCount := 2, group := "hyperbolic_specific" },
    { name := "ultraparallel_line", paramCount := 2, group := "hyperbolic_specific" },
    { name := "asymptotic_triangle", paramCount := 3, group := "hyperbolic_specific" },
    { name := "saccheri_quadrilateral", paramCount := 2, group := "hyperbolic_specific" },
    { name := "lambert_quadrilateral", paramCount := 3, group := "hyperbolic_specific" },
    -- Group VI: Continuity & Metric
    { name := "archimedes_axiom", paramCount := 4, group := "continuity_metric" },
    { name := "line_completeness", paramCount := 0, group := "continuity_metric" },
    { name := "hyperbolic_distance", paramCount := 2, group := "continuity_metric" },
    -- Group VII: Hyperbolic Model Constructions
    { name := "poincare_disk_model", paramCount := 2, group := "hyperbolic_model" },
    { name := "poincare_halfplane_model", paramCount := 2, group := "hyperbolic_model" },
    { name := "klein_model", paramCount := 2, group := "hyperbolic_model" },
    { name := "hyperbolic_isometry", paramCount := 2, group := "hyperbolic_model" }
  ]

/-- Hyperbolic Geometry 模板数量。 -/
theorem hyperbolicGeometryTemplates_length : hyperbolicGeometryTemplates.length = 29 := by
  decide

/-- Hyperbolic Geometry 包中的 6 个不可构造问题。 -/
def hyperbolicGeometryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "squaring_the_circle_hyperbolic", reducesTo := "transcendental_number",
      dependencies := ["hyperbolic_distance", "line_completeness", "angle_of_parallelism"],
      externalRef := "https://en.wikipedia.org/wiki/Squaring_the_circle", greenVerified := true },
    { name := "angle_trisection_hyperbolic", reducesTo := "cubic_equation_solving",
      dependencies := ["segment_transport", "angle_transport", "SAS_congruence"],
      externalRef := "https://en.wikipedia.org/wiki/Angle_trisection", greenVerified := true },
    { name := "doubling_the_cube_hyperbolic", reducesTo := "cube_root_of_two",
      dependencies := ["segment_transport", "hyperbolic_distance"],
      externalRef := "https://en.wikipedia.org/wiki/Doubling_the_cube", greenVerified := true },
    { name := "regular_polygon_hyperbolic", reducesTo := "algebraic_equation",
      dependencies := ["segment_transport", "angle_transport", "hyperbolic_distance"],
      externalRef := "https://en.wikipedia.org/wiki/Constructible_polygon", greenVerified := true },
    { name := "area_of_triangle_trisection", reducesTo := "transcendental",
      dependencies := ["hyperbolic_distance", "angle_of_parallelism", "SAS_congruence"],
      externalRef := "https://en.wikipedia.org/wiki/Hyperbolic_triangle", greenVerified := true },
    { name := "constructible_angle_characterization", reducesTo := "algebraic_number_theory",
      dependencies := ["angle_of_parallelism", "hyperbolic_distance", "line_completeness"],
      externalRef := "https://en.wikipedia.org/wiki/Angle_of_parallelism", greenVerified := true }
  ]

/-- Hyperbolic Geometry 不可构造问题数量。 -/
theorem hyperbolicGeometryUnconstructibles_length : hyperbolicGeometryUnconstructibles.length = 6 := by
  decide

/-- Hyperbolic Geometry 公理包实例。 -/
def hyperbolicGeometryPackage : AxiomPackageInstance :=
  { name := "hyperbolic_geometry",
    version := "1.0.0",
    templates := hyperbolicGeometryTemplates,
    unconstructibles := hyperbolicGeometryUnconstructibles,
    bottomGeometry := "hyperbolic_plane",
    negationEncoding := "classical_material_implication",
    contradictionBehavior := "explosion_principle" }

/-- Hyperbolic Geometry 逻辑框架字段与 C 测试一致。 -/
theorem hyperbolicGeometry_logical_framework :
    hyperbolicGeometryPackage.bottomGeometry = "hyperbolic_plane" ∧
    hyperbolicGeometryPackage.negationEncoding = "classical_material_implication" ∧
    hyperbolicGeometryPackage.contradictionBehavior = "explosion_principle" := by
  decide

/-- 由全部 Hyperbolic Geometry 模板生成的规则实例。 -/
def hyperbolicGeometryExecutableRules : List ExecutableRule :=
  hyperbolicGeometryTemplates.mapIdx fun i t => templateToExecutableRule i t

/-- 生成的 Hyperbolic Geometry 规则实例数量与模板数量一致。 -/
theorem hyperbolicGeometryExecutableRules_length : hyperbolicGeometryExecutableRules.length = 29 := by
  decide

/-! ## Projective Geometry 公理包实例

以下内容对照 `projective_geometry.lvz` 与 `test_axiom_projective_geometry.c`：
- 38 个模板（10 组：关联、维度与扩展、Desargues、Pappus、基本构造器、射影变换、圆锥曲线、坐标域构造、高维射影空间、导出定理）；
- 7 个不可构造问题（全部 green_verified=true）；
- bottom_geometry = projective_plane_incidence；
- negation_encoding = classical_equality；
- contradiction_behavior = explosion_principle。

注意：部分不可构造问题的依赖引用了外部理论（如 combinatorial_design_theory），
这些依赖不在本包模板表中，对应 C 测试中 `test_dependency_validation` 的预期行为。
-/

/-- Projective Geometry 包中的 38 个模板。 -/
def projectiveGeometryTemplates : List PackageTemplate :=
  [ -- Group I: Incidence Axioms (3)
    { name := "join_of_two_points", paramCount := 2, group := "incidence" },
    { name := "meet_of_two_lines", paramCount := 2, group := "incidence" },
    { name := "existence_of_triangle_projective", paramCount := 0, group := "incidence" },
    -- Group II: Dimension and Extension Axioms (4)
    { name := "existence_of_complete_quadrangle", paramCount := 0, group := "dimension" },
    { name := "line_has_three_points", paramCount := 1, group := "dimension" },
    { name := "point_has_three_lines", paramCount := 1, group := "dimension" },
    { name := "veblen_axiom", paramCount := 4, group := "dimension" },
    -- Group III: Desargues' Theorem (1)
    { name := "desargues_theorem", paramCount := 7, group := "desargues" },
    -- Group IV: Pappus's Hexagon Theorem (1)
    { name := "pappus_hexagon_theorem", paramCount := 6, group := "pappus" },
    -- Group V: Fundamental Constructors (8)
    { name := "line_through_point_meeting_line", paramCount := 2, group := "constructor" },
    { name := "harmonic_conjugate", paramCount := 3, group := "constructor" },
    { name := "intersection_of_line_with_line", paramCount := 3, group := "constructor" },
    { name := "cross_ratio", paramCount := 4, group := "constructor" },
    { name := "perspectivity", paramCount := 3, group := "constructor" },
    { name := "diagonal_triangle_of_quadrangle", paramCount := 4, group := "constructor" },
    { name := "dual_configuration", paramCount := 1, group := "constructor" },
    { name := "pole_polar_construction", paramCount := 2, group := "constructor" },
    -- Group VI: Projective Transformations (5)
    { name := "projectivity", paramCount := 2, group := "transformation" },
    { name := "fundamental_theorem_uniqueness", paramCount := 6, group := "transformation" },
    { name := "collineation", paramCount := 1, group := "transformation" },
    { name := "correlation", paramCount := 1, group := "transformation" },
    { name := "elation", paramCount := 2, group := "transformation" },
    -- Group VII: Conic Sections (4)
    { name := "conic_through_five_points", paramCount := 5, group := "conic" },
    { name := "tangent_to_conic", paramCount := 2, group := "conic" },
    { name := "pascal_theorem", paramCount := 6, group := "conic" },
    { name := "brianchon_theorem", paramCount := 6, group := "conic" },
    -- Group VIII: Coordinate Field Construction (4)
    { name := "field_addition_geometric", paramCount := 4, group := "coordinate_field" },
    { name := "field_multiplication_geometric", paramCount := 4, group := "coordinate_field" },
    { name := "field_additive_inverse", paramCount := 2, group := "coordinate_field" },
    { name := "field_multiplicative_inverse", paramCount := 3, group := "coordinate_field" },
    -- Group IX: Higher-Dimensional Projective Space (3)
    { name := "join_point_line_to_plane", paramCount := 2, group := "higher_dimensional" },
    { name := "meet_of_two_planes", paramCount := 2, group := "higher_dimensional" },
    { name := "desargues_provable_in_3d", paramCount := 0, group := "higher_dimensional" },
    -- Group X: Derived Theorems (5)
    { name := "dual_desargues_theorem", paramCount := 7, group := "derived_theorem" },
    { name := "harmonic_conjugate_uniqueness", paramCount := 3, group := "derived_theorem" },
    { name := "complete_quadrilateral_theorem", paramCount := 4, group := "derived_theorem" },
    { name := "projectivity_uniqueness", paramCount := 6, group := "derived_theorem" },
    { name := "cross_ratio_invariance", paramCount := 8, group := "derived_theorem" }
  ]

/-- Projective Geometry 模板数量。 -/
theorem projectiveGeometryTemplates_length : projectiveGeometryTemplates.length = 38 := by
  decide

/-- Projective Geometry 包中的 7 个不可构造问题。

注意：部分依赖引用了外部理论（如 combinatorial_design_theory、bruck_ryser_chowla_theorem、
field_theory、coordinate_field_construction），这些名称不在本包模板表中。 -/
def projectiveGeometryUnconstructibles : List UnconstructibleProblem :=
  [ { name := "existence_of_finite_projective_plane_non_prime_power", reducesTo := "open_problem",
      dependencies := ["combinatorial_design_theory", "finite_field_theory", "bruck_ryser_chowla_theorem"],
      externalRef := "https://en.wikipedia.org/wiki/Projective_plane#Finite_projective_planes", greenVerified := true },
    { name := "classification_of_finite_projective_planes", reducesTo := "wildly_open",
      dependencies := ["non_desarguesian_plane_construction", "translation_plane_theory"],
      externalRef := "https://en.wikipedia.org/wiki/Projective_plane#Classification", greenVerified := true },
    { name := "coordinate_field_of_non_desarguesian_plane", reducesTo := "non_associative_algebra",
      dependencies := ["desargues_theorem_failure", "alternative_division_ring"],
      externalRef := "https://en.wikipedia.org/wiki/Non-Desarguesian_plane", greenVerified := true },
    { name := "constructing_midpoint_with_straightedge_only", reducesTo := "metric_construction",
      dependencies := ["harmonic_conjugate", "projective_to_euclidean_specialization"],
      externalRef := "https://en.wikipedia.org/wiki/Straightedge_and_compass_construction", greenVerified := true },
    { name := "trisection_of_angle_projective", reducesTo := "cubic_equation_solving",
      dependencies := ["cross_ratio", "field_theory"],
      externalRef := "https://en.wikipedia.org/wiki/Angle_trisection", greenVerified := true },
    { name := "pappus_implies_field_commutativity", reducesTo := "algebraic_equivalence",
      dependencies := ["desargues_theorem", "coordinate_field_construction", "pappus_hexagon_theorem"],
      externalRef := "https://en.wikipedia.org/wiki/Pappus%27s_hexagon_theorem#Relationship_to_the_axioms_of_projective_geometry", greenVerified := true },
    { name := "order_of_largest_unknown_finite_projective_plane", reducesTo := "open_problem",
      dependencies := ["bruck_ryser_chowla_theorem", "computational_search"],
      externalRef := "https://en.wikipedia.org/wiki/Projective_plane#Finite_projective_planes", greenVerified := true }
  ]

/-- Projective Geometry 不可构造问题数量。 -/
theorem projectiveGeometryUnconstructibles_length : projectiveGeometryUnconstructibles.length = 7 := by
  decide

/-- Projective Geometry 公理包实例。 -/
def projectiveGeometryPackage : AxiomPackageInstance :=
  { name := "projective_geometry",
    version := "1.0.0",
    templates := projectiveGeometryTemplates,
    unconstructibles := projectiveGeometryUnconstructibles,
    bottomGeometry := "projective_plane_incidence",
    negationEncoding := "classical_equality",
    contradictionBehavior := "explosion_principle" }

/-- 由全部 Projective Geometry 模板生成的规则实例。 -/
def projectiveGeometryExecutableRules : List ExecutableRule :=
  projectiveGeometryTemplates.mapIdx fun i t => templateToExecutableRule i t

/-- 生成的 Projective Geometry 规则实例数量与模板数量一致。 -/
theorem projectiveGeometryExecutableRules_length : projectiveGeometryExecutableRules.length = 38 := by
  decide


end Instances
end Axioms
end Theory
end Lv00Formal
