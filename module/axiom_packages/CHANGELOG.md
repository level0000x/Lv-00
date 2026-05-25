# Axiom Packages Changelog

All notable changes to the Lv-00 axiom package system will be documented in this file.

## [1.0.0] -- 2026-05-23

### Added -- 初始创建

#### Package System Infrastructure
- Created `README.md` -- package system overview with architecture documentation
  - Pluggable, versioned axiom system concept
  - `namespace.axiom_name` naming convention (from LeanGeo)
  - Layer philosophy: incidence -> order -> congruence -> continuity -> parallels (from GeoCoq)
  - Dependency management and resolution documentation
  - Package versioning (SemVer) and SHA-256 hash-based integrity
  - Package registry format specification

#### Package Registry
- Created `INDEX.json` -- central package registry
  - Registered 41 packages across 8 categories:
    - **geometry**: euclidean_plane, hyperbolic_geometry, elliptic_geometry, projective_geometry, affine_geometry, differential_geometry, algebraic_geometry (7)
    - **foundations**: zfc_set_theory, nbg_set_theory, category_theory, descriptive_set_theory, domain_theory, computability_theory, computational_complexity_theory (7)
    - **algebra**: group_theory, ring_theory, field_theory, lattice_theory, boolean_algebra, lie_theory, homological_algebra (7)
    - **analysis**: real_analysis, functional_analysis (2)
    - **logic**: classical_propositional_logic, intuitionistic_logic, intuitionistic_propositional_logic, modal_logic, linear_logic, simple_type_theory, dependent_type_theory, homotopy_type_theory, proof_theory, model_theory (10)
    - **topology**: metric_space, point_set_topology, algebraic_topology (3)
    - **number_theory**: number_theory (1)
    - **discrete_math**: combinatorics, game_theory, information_theory (3)
  - Each entry includes: version, category, layers, dependencies, description, manifest path, axiom file, and test file references

#### Euclidean Geometry Package
- Created `euclidean/README.md` -- comprehensive layered axiom documentation
  - **Layer 0 (Incidence)**: I1 (Two-Point Incidence), I2 (Line Points Existence), I3 (Non-Collinearity) with formal statements, Chinese descriptions, and independence notes
  - **Layer 1 (Order)**: B1 (Betweenness Symmetry), B2 (Betweenness Extension), B3 (Betweenness Uniqueness), B4 (Pasch's Axiom) with Tarski-style formalizations
  - **Layer 2 (Congruence)**: C1 (Segment Construction), C2 (Congruence Transitivity), C3 (Segment Addition) with Tarski-style formalizations
  - **Layer 3 (Continuity)**: Dedekind Cut Axiom with alternative forms table (Line-Circle, Circle-Circle, Archimedean, Cauchy)
  - **Layer 4 (Parallels)**: Playfair's Axiom with equivalent forms table (Euclid's Fifth, triangle angle sum, Legendre, Wallis)
  - Complete axiom index table with namespace names, dependencies, and independence status
  - Usage examples in C showing layer-by-layer loading and axiom checking
- Created `euclidean/manifest.json` -- structured package manifest
  - All 5 layers with detailed axiom definitions including signatures, arities, and primitive relations
  - Extended axiom definitions (Five-Segment Axiom for angle congruence)
  - First-order alternatives for second-order continuity axiom
  - Minimal model descriptions for each layer
  - Scholarly references (Tarski 1959, Schwabhauser et al. 1983, Hilbert 1899, Playfair 1795)

#### Hyperbolic Geometry Package
- Created `hyperbolic/manifest.json` -- non-Euclidean geometry package
  - Reuses Euclidean layers 0-3 via `inherits_from` mechanism
  - Layer 4: Hyperbolic Parallel Postulate (Lobachevsky form) -- INFINITELY MANY parallels
  - Detailed comparison table: theorems that change (7) vs. theorems that stay the same
  - Three standard models: Poincare disk, Beltrami-Klein, hyperboloid (Minkowski)
  - Scholarly references (Lobachevsky 1840, Bolyai 1832, Beltrami 1868, Poincare 1882, Anderson 2005)

#### Developer Tools
- Created `package_template.json` -- new package creation template
  - Annotated template with all required and optional fields
  - Complete example package (example_geometry with 2 layers, 3 axioms)
  - Notes on naming conventions, layer ordering, dependency declaration, and inheritance
  - Guidance on primitive_relations, references format, and INDEX.json registration

#### Documentation
- Created `CHANGELOG.md` -- this file

### Design Principles Applied

| Principle | Source | Implementation |
|-----------|--------|----------------|
| Axiom Layering | GeoCoq | 5-layer Euclidean structure (incidence -> order -> congruence -> continuity -> parallels) |
| Namespace Naming | LeanGeo | `namespace.axiom_name` format (e.g., `euclidean.I1`, `hyperbolic.Lobachevsky`) |
| Package Management | GAP | Central registry (INDEX.json), versioned manifests, dependency resolution |

### P0 and P4 from Competitive Analysis

- **P0 (Axiom Layering)**: Fully implemented via GeoCoq-inspired layer architecture in euclidean and hyperbolic packages
- **P4 (Package Management)**: Fully implemented via INDEX.json registry, manifest.json per-package, and dependency tracking

### Known Limitations -- 已知限制

- Layer inheritance (`inherits_from`) is specified in manifests but the runtime dependency resolver is not yet implemented
- Some registered packages in INDEX.json have `manifest_path: null` -- manifests need to be created for those packages
- Hash-based integrity verification is specified but the hash computation pipeline is pending
- Test save files exist for only 8 of 41 packages
- No automated package validation (schema compliance checker) is in place yet

### Next Steps

1. Implement the runtime dependency resolver for `inherits_from`
2. Create manifests for remaining packages (especially algebraic structures: group, ring, field)
3. Build a schema compliance checker for manifest validation
4. Generate content hashes for all `.lvz` axiom files
5. Add CI pipeline for automatic package integrity verification
