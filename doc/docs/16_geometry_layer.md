# 几何层 (Geometry Layer, Layer 3)

## 模块概述

几何层（Layer 3）是 Lv-00 的几何数据与判定基础，向上支撑求解器与证明引擎，向下依赖资源层。本层核心包括：约束图（`constraint_graph.h`，几何节点/约束/哈希索引/序列化/DOT 导出）、符号坐标（`symbolic_coord.h`，Rational/Algebraic/Quadratic/Transcendental 四类精确坐标）、规范化（`normalization.h`，节点合并/冗余消除/拓扑排序）、几何拓扑（`geo_topology.h`，单纯复形/欧拉示性数/边界）、AABB 树（`geo_aabb_tree.h`，2D/3D 空间加速结构）、几何谓词（`geo_predicate.h`，方向/线段/圆/多边形判定，四档精度模式）、几何规范（`geo_spec.h`，JSON 几何规范解析）以及高维投影（`high_dim.h`，多视图/保真度）。便捷聚合头文件 `geo_utils.h` 统一引用符号坐标、约束图与数值容差常量（`GEO_EPSILON` = 1e-12、`GEO_ANGLE_EPSILON` = 1e-10）。

## 核心设计原则

1. **符号精确优先**：坐标以 `SymbolicCoord`（GMP 有理数、代数数、二次扩域、超越数）存储；`symbolic_coord_are_collinear` 等符号判定与浮点谓词分属不同精度域，语义独立、不混用。
2. **图为中心**：`ConstraintGraph` 以邻接表 + 开放寻址哈希索引组织节点与约束，O(1) 按 ID 查找；`involving_index` 反向索引将热路径查询降为 O(度数)；`next_node_id`/`next_constraint_id` 为原子计数保证多线程安全。
3. **VTable 类型多态**：`GeomNodeVTable` 提供 alloc/free/clone/serialize/detect_conflict/hash/compare/fixup_refs/get_trust_coord_count，消除类型代码反模式。
4. **谓词自适应精度**：`lvPredicateMode` 四档模式（EXACT 区间算术 / APPROX 浮点 / ADAPTIVE 先浮点不确定再精确 / SYMBOLIC 回退精确）；`lvPredicateStats` 统计回退次数，全局模式可经 `lv_predicate_set_mode` 切换。
5. **数值容差单一来源**：`GEO_EPSILON`/`GEO_ANGLE_EPSILON` 统一引用 `lv_utils.h`/`config.h` 权威常量，禁止实现文件本地重定义数值。
6. **规范化幂等**：`graph_normalize` 支持作用域感知合并（`MergeConfirmCallback` 跨作用域确认），`normalization_verify_idempotency` 验证两次归一化结果一致；图哈希 + `RewriteHistory` 做循环检测。
7. **拓扑与几何分离**：`lvSimplicialComplex` 只管理顶点/边/三角形组合结构（欧拉示性数、连通分量、边界），几何判定由谓词层负责。
8. **空间索引可配置**：AABB 树经 `lvAABBTreeConfig`（`max_leaf_size`/`max_depth`/`use_sah`）配置，2D 与 3D 统一使用 3D 节点包围盒表示。

## 关键数据结构

```c
/* 符号坐标（symbolic_coord.h）：四类精确坐标 + 信任颜色 */
typedef enum { RATIONAL, ALGEBRAIC, QUADRATIC, TRANSCENDENTAL } CoordType;

struct SymbolicCoord {
    CoordType type;
    TrustColor trust;
    bool cache_valid;
    double cached_value;
    AlgebraicInfo *algebraic_info;   /* 代数共轭检测 */
    union {
        Rational *rational;          /* mpq_t */
        Algebraic *algebraic;        /* 最小多项式 + 区间 */
        Quadratic *quadratic;        /* a + b*sqrt(n) */
        Transcendental *transcendental;
    } data;
};

/* 约束图（constraint_graph.h）核心 */
struct ConstraintGraph {
    GeomNode **nodes; int node_count, node_capacity;
    Constraint **constraints; int constraint_count, constraint_capacity;
    _Atomic int next_node_id, next_constraint_id;    /* 多线程安全 */
    GeomNode **node_index; int node_index_capacity;  /* O(1) 哈希索引 */
    Constraint **constraint_index; int constraint_index_capacity;
    lvHashtable *involving_index; int constraints_version, involving_version;
    char *error_buffer, *serialize_buffer;
    struct lvContext *context;
    bool dirty;
};

/* 几何节点：union data 按 type 选用变体 */
struct GeomNode {
    int id;
    GeomType type;
    const GeomNodeVTable *vtable;
    SymbolicCoord **symbolic_coords;  /* 符号坐标 */
    int coord_count;
    TrustColor trust;
    bool is_active;                   /* 生命周期标记 */
    LightOrangeSubtype lo_subtype;
    char *numeric_assumption_declaration;
    double numeric_precision;         /* 近似值，勿用于精确相等 */
    int namespace_depth;
    int parent_block_id;
    union {
        Port *port;                    /* GEOM_PORT */
        struct { GeomNode **boundary_segments; int segment_count; } region;
        struct { int center_node_id; int radius_node_id; } circle;
        struct {
            GeomNode **internal_nodes;
            int *input_port_ids; int *output_port_ids;
            int internal_node_count, input_count, output_count;
            enum { UNVERIFIED, VERIFIED, NON_DETERMINISTIC,
                   PARTIALLY_VERIFIED } determinism_state;
        } func_block;                  /* GEOM_FUNCTION_BLOCK */
    } data;
};

/* 约束：参与者为节点 ID 数组 */
struct Constraint {
    int id;
    ConstraintType type;
    int *participants;
    int participant_count;
    int template_id;
    bool is_active;
    double numeric_value;              /* 角度等数值参数（度） */
    double satisfaction;               /* 0.0~1.0，用于概率推理 */
};

/* 几何拓扑（geo_topology.h）：单纯复形 */
typedef struct lvSimplicialComplex {
    int n_vertices;
    lvEdge *edges; size_t n_edges, edges_capacity;
    lvTriangle *triangles; size_t n_triangles, triangles_capacity;
    int dim;
    int *faces; int n_faces;
} lvSimplicialComplex;

/* AABB 树节点（geo_aabb_tree.h）：统一 3D 包围盒 */
typedef struct lvAABBNode {
    lvAABB3D bbox;
    int left, right;
    int primitive_id;
    int height;
    int leaf_start, leaf_count;
} lvAABBNode;

/* 几何谓词精度模式（geo_predicate.h） */
typedef enum { lv_PREDICATE_EXACT = 0, lv_PREDICATE_APPROX = 1,
               lv_PREDICATE_ADAPTIVE = 2, lv_PREDICATE_SYMBOLIC = 3 } lvPredicateMode;
typedef enum { lv_ORIENTATION_LEFT = -1, lv_ORIENTATION_COLLINEAR = 0,
               lv_ORIENTATION_COPLANAR = 0, lv_ORIENTATION_RIGHT = 1,
               lv_ORIENTATION_DEGENERATE = 2 } lvOrientation;

/* 高维投影（high_dim.h）管理块与保真度 */
typedef struct HighDimManager {
    lvDArray blocks;              /* HighDimAbstractBlock 数组 */
    int perspective_depth;
    int perspective_stack[HIGH_DIM_MAX_DEPTH];   /* 32 */
} HighDimManager;
```

## 主要接口

| 分组 | 函数 | 说明 |
| --- | --- | --- |
| 符号坐标 | `symbolic_coord_create_rational/quadratic/algebraic/transcendental`、`symbolic_coord_add/subtract/multiply/divide/negate/sqrt/pow`、`symbolic_coord_compare/equal/is_zero/is_positive/is_negative`、`symbolic_coord_are_collinear`、`symbolic_coord_to_double/serialize/hash` | 坐标构造、四则运算、符号判定与序列化 |
| 符号坐标管理 | `symbolic_coord_set_plan/get_plan/auto_degrade`、`symbolic_coord_is_amber`、`trust_color_combine`、`symbolic_coord_downgrade_to_amber`、电路系统 `circuit_*`/`check_digit_circuit` | 代数方案 A/B/C 降级、信任颜色传播、熔断保护 |
| 约束图 | `graph_create/copy/destroy`、`graph_add_point/line_segment/region/circle/port/function_block`、`graph_add_incidence/betweenness/intersection/containment/connection/angle`、`graph_get_node/get_constraint/find_constraints_involving`、`graph_remove_node/remove_constraint/deactivate_constraint`、`graph_detect_redundancy/detect_redundant_constraints/detect_conflicts/validate_region_closure/check_compatibility`、`graph_mark_dirty/sync_nodes` | 图构建、约束增删、O(1) 查询、冗余/冲突分析、生命周期 |
| 序列化/导出 | `graph_serialize_to_json/deserialize_from_json`、`graph_node_serialize_to_json`、`graph_export_dot/export_dot_file/export_dot_to_svg`、`graph_get_error/get_serialize_error` | JSON 序列化与 Graphviz DOT/SVG 导出 |
| 几何规范 | `lv_geo_spec_parse/lv_geo_spec_destroy` | JSON 几何规范解析（`lvGeoSpecPoint`/`lvGeoSpecPolygon`） |
| 几何工具 | `geo_distance_2d/3d`、`geo_norm_2d`、`geo_approx_equal`、`geo_point_on_segment`、`geo_signed_area_2x`、`geo_angle`、`geo_segments_intersect` | 基础距离/面积/角度/线段计算 |
| 规范化 | `graph_normalize`、`merge_line_segments`、`merge_regions`、`find_merge_candidates`、`apply_merges`、`graph_topological_sort_stable`、`normalization_verify_idempotency`、`normalization_set_merge_callback`、`rewrite_history_create/check_cycle/add` | 节点合并、冗余消除、拓扑排序、循环检测 |
| 几何拓扑 | `geo_simplicial_create/destroy/add_edge/add_triangle/euler_characteristic/boundary/connected_components`、`lv_euler_characteristic`、`lv_is_simplicial_complex` | 单纯复形构建、欧拉示性数、边界与连通分量 |
| AABB 树 | `lv_aabb_tree_default_config`、`lv_aabb2d_build/ray_query/nearest/range_query/point_query/root_bbox/stats`、`lv_aabb3d_build/ray_query/nearest/range_query/point_query/root_bbox`、`lv_aabb2d_empty/point/merge/is_valid/contains/intersects/area/center`、`lv_aabb3d_*`、旧版 `lv_aabb_tree_build/query` | 包围盒运算、2D/3D 树构建与射线/最近邻/范围/点查询 |
| 几何谓词 | `lv_orientation_2d/3d`、`lv_line_side`、`lv_segment_side`、`lv_same_side_of_line`、`lv_segments_intersect`、`lv_side_of_circle`、`lv_same_side_of_circle`、`lv_four_points_concyclic`、`lv_point_in_triangle`、`lv_polygon_is_convex`、`lv_point_in_convex_polygon`、`lv_point_in_polygon`、`lv_predicate_get_stats/reset_stats/set_mode/get_mode` | 方向/线段/圆/多边形谓词与统计；旧名 `lv_orient2d/orient3d/incircle` |
| 高维投影 | `high_dim_manager_create/destroy/init`、`high_dim_register_block/unregister_block/get_block`、`high_dim_add_projection_preset/create_default_preset/set_current_preset`、`high_dim_project_coordinates/apply_transform/create_rotation_transform/create_scale_transform`、`high_dim_calculate_fidelity/compute_fidelity_detailed/is_fidelity_below_threshold`、`high_dim_enter_block_perspective/exit`、`high_dim_create_multi_projection_view/manage_multi_views/export_views_json`、`high_dim_project_to_3d/full`、`high_dim_preset_serialize_json/deserialize_json` | 高维块管理、轴映射、多视图、保真度度量与 3D 投影 |
| 精确算术 | `lv_timestamp_now`、`lv_safe_pow/mul_impl/add_check_impl/sub_impl` | 溢出安全检查与高精度时间戳 |

## 工作流程

1. **构造建模**：解析几何规范（`lv_geo_spec_parse`）或 DSL 语句，经 `graph_add_point`/`graph_add_line_segment` 等添加节点，`graph_add_incidence`/`graph_add_angle` 等建立约束，节点坐标一律以 `symbolic_coord_create_*` 构造的符号坐标赋值。
2. **规范化**：`graph_normalize(graph, scope_aware)` 检测合并候选（`find_merge_candidates`），共线线段/重叠区域经 `merge_line_segments`/`merge_regions` 合并，跨作用域候选由 `MergeConfirmCallback` 确认；`graph_topological_sort_stable` 稳定排序，`rewrite_history_check_cycle` 防循环。
3. **相容性检查**：`graph_check_compatibility` 输出约束集合状态（一致/矛盾/欠约束/过约束），配合 `graph_detect_redundancy`/`graph_detect_conflicts` 定位冗余与冲突组。
4. **空间加速**：将线段/圆等图元包围盒批量交给 `lv_aabb2d_build`/`lv_aabb3d_build` 建树，求解与判定阶段经 `ray_query`/`nearest`/`range_query` 快速筛候选，减少几何谓词调用量。
5. **精确判定**：`lv_orientation_2d` 等谓词按全局 `lvPredicateMode`（默认 ADAPTIVE）执行：浮点快速路径不确定时回退区间算术精确路径，统计写入 `lvPredicateStats`。
6. **拓扑分析**：对判定结果构建 `lvSimplicialComplex`，`geo_simplicial_euler_characteristic` 验证网格拓扑，`geo_simplicial_boundary`/`connected_components` 做边界与连通性分析。
7. **高维可视化**：`high_dim_register_block` 注册高维抽象块，`high_dim_project_coordinates` 按预设投影到 2D/3D，`high_dim_compute_fidelity_detailed` 评估保真度（几何失真、MDS stress、拓扑保持）并生成调整建议。

## 模块关系

| 模块 | 关系说明 |
| --- | --- |
| [01_symbolic_coord.md](01_symbolic_coord.md) | 几何层的坐标基座：`GeomNode.symbolic_coords` 全部为 `SymbolicCoord`；`geo_utils.h` 聚合引用；`high_dim.h` 的投影输入亦为符号坐标 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图是几何层核心数据容器，节点/约束定义与哈希索引见该文档 |
| [03_normalization.md](03_normalization.md) | 规范化引擎对约束图执行节点合并、冗余消除与拓扑排序，保障合一/证明输入图的干净性 |
| [04_solver.md](04_solver.md) | 求解器读取约束图并调用几何谓词/工具完成数值求解与传播 |
| [06_unify.md](06_unify.md) | 合一验证以规范化后的约束图为输入，模式匹配依赖几何谓词判定的共线/共圆等性质 |
| [09_proof.md](09_proof.md) | 证明引擎的 `Proposition.pattern` 为约束图；`proof_unify` 可选 `normalize_first` 触发规范化 |
| [14_solver_backends.md](14_solver_backends.md) | 数值后端与 SMT 后端复用几何谓词的精确/近似模式与符号坐标运算 |
| [15_geometry_advanced.md](15_geometry_advanced.md) | 几何高级特征（相似、共圆、动态几何）构建于本层 AABB 树/谓词/拓扑之上 |

## 版本历史

- v3.6.0：约束图引入 `involving_index` 反向索引与版本化惰性重建，热路径线性扫描降为 O(度数)。
- v3.5.0：约束生命周期管理（`graph_deactivate_constraint`/`graph_mark_dirty`/`graph_sync_nodes`）；`SymbolicCoord` 增加代数共轭检测（`AlgebraicInfo`）。
- v3.4.1：`next_node_id`/`next_constraint_id` 原子化，多线程安全 ID 分配；`GRAPH_ATOMIC_*_ID_INCREMENT` 宏族。
- v1.1.0（geo_utils）：数值容差分级汇总，`GEO_EPSILON`/`GEO_ANGLE_EPSILON` 统一引用权威常量。
