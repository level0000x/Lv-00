# 约束图核心 (Constraint Graph Core)

## 模块概述

约束图是 Lv-00 系统的核心数据结构，以**邻接表 + 哈希索引**组织几何节点（点、线段、区域、圆、端口、函数块）与约束（关联、之间、相交、包含、连接、角度）的创建 / 删除 / 查询。它提供 O(1) 按 ID 查找、节点 → 约束反向索引、冗余检测与冲突分析、JSON 序列化与 Graphviz DOT 导出，并通过节点 VTable 实现类型多态。配套的 `graph_hash.h` 提供与节点顺序无关的图哈希指纹，`equiv_class.h` 基于并查集维护节点等价类合并与证明日志，`lv_ast.h` 定义语言层 AST，用于将声明 / 约束语句编译进约束图。

## 核心设计原则

1. **图为中心**：一切几何构造与约束关系均落为图中的 `GeomNode` / `Constraint`，求解、传播、证明均以图为输入。
2. **VTable 类型多态**：`GeomNodeVTable` 提供 alloc / free / clone / serialize / detect_conflict / hash / compare / fixup_refs / get_trust_coord_count，消除 switch/if-else 类型代码反模式。
3. **O(1) 哈希索引**：`node_index` / `constraint_index` 采用开放寻址 + 线性探测，容量恒为 2 的幂，`node_id % capacity` 定址；另有惰性重建的 `involving_index` 反向索引将热路径线性扫描降为 O(度数)。
4. **多线程安全 ID 分配**：`next_node_id` / `next_constraint_id` 为 `_Atomic int`，经 `GRAPH_ATOMIC_NODE_ID_INCREMENT` 原子递增。
5. **惰性废弃与脏标记**：删除节点/约束走 `is_active=false` 惰性废弃保留审计；`dirty` 标记配合 `graph_mark_dirty` / `graph_sync_nodes` 延迟同步节点属性。
6. **单一事实来源**：类型名称 / 别名 / DOT 形状统一由 `LV_GEOM_TYPE_ENTRY`、`LV_CONSTRAINT_TYPE_ENTRY`、`LV_GEOM_TYPE_X`、`LV_CONSTRAINT_TYPE_X` X-macro 生成，禁止散落重复表。
7. **图哈希与等价类**：`compute_complete_graph_hash` 生成顺序无关指纹用于去重/循环检测；`equiv_class.h` 的并查集 + 等价类证明日志支撑几何对象合并。

## 关键数据结构

```c
/* 几何节点类型：由 LV_GEOM_TYPE_X X-macro 生成 */
typedef enum { GEOM_POINT, GEOM_LINE_SEGMENT, GEOM_REGION, GEOM_CIRCLE,
               GEOM_PORT, GEOM_FUNCTION_BLOCK } GeomType;

/* 约束类型：由 LV_CONSTRAINT_TYPE_X X-macro 生成 */
typedef enum { INCIDENCE, BETWEENNESS, INTERSECTION, CONTAINMENT,
               CONNECTION, ANGLE } ConstraintType;

/* 端口：函数块输入/输出接口 */
struct Port {
    int id;
    PortType type;               /* PORT_INPUT / PORT_OUTPUT */
    int namespace_depth;
    int parent_block_id;
    bool is_formal_param;
    bool is_polymorphic;         /* 多态端口（如爆炸原理的输出） */
    TypeRegion *type_region;
    GeomNode *connected_to;
};

/* 几何节点：核心数据单元，union data 按 type 选用变体 */
struct GeomNode {
    int id;
    GeomType type;
    const GeomNodeVTable *vtable;
    SymbolicCoord **symbolic_coords;
    int coord_count;
    TrustColor trust;
    bool is_active;              /* true=活跃，false=已废弃 */
    LightOrangeSubtype lo_subtype;
    char *numeric_assumption_declaration;
    double numeric_precision;    /* 近似值，勿用于精确相等 */
    int namespace_depth;
    int parent_block_id;
    union {
        Port *port;              /* GEOM_PORT */
        struct { GeomNode **boundary_segments; int segment_count; } region;
        struct { int center_node_id; int radius_node_id; } circle;
        struct {
            GeomNode **internal_nodes;
            int *input_port_ids;
            int *output_port_ids;
            int internal_node_count, input_count, output_count;
            enum { UNVERIFIED, VERIFIED, NON_DETERMINISTIC,
                   PARTIALLY_VERIFIED } determinism_state;
        } func_block;            /* GEOM_FUNCTION_BLOCK */
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
    double numeric_value;        /* 角度等数值参数（度） */
    double satisfaction;         /* 0.0~1.0，用于概率推理 */
};

/* 约束图：节点/约束动态数组 + 哈希索引 + 反向索引 */
struct ConstraintGraph {
    GeomNode **nodes;  int node_count;  int node_capacity;
    Constraint **constraints;  int constraint_count;  int constraint_capacity;
    _Atomic int next_node_id;
    _Atomic int next_constraint_id;
    GeomNode **node_index;  int node_index_capacity;
    Constraint **constraint_index;  int constraint_index_capacity;
    char *error_buffer;      /* 256 字节堆分配 */
    char *serialize_buffer;  /* 256 字节堆分配 */
    struct lvContext *context;
    bool dirty;
    lvHashtable *involving_index;  /* node_id -> 约束下标列表（惰性） */
    int constraints_version;
    int involving_version;
};

/* 约束添加分派表条目（单一事实来源，graph_index.c 的 kConstraintAddOps） */
typedef struct {
    ConstraintType type;
    const char *name;            /* algebra_constrain 小写名，NULL=不暴露 */
    int min_participants;
    bool (*arity_ok)(int);
    AddConstraintResult (*fn)(ConstraintGraph *, const int *, int, double);
} ConstraintAddOps;

/* 图哈希（graph_hash.h）：整体哈希 + 各节点独立哈希 */
typedef struct GraphHash {
    uint64_t hash;
    int node_count;
    uint64_t *node_hashes;
} GraphHash;

/* 等价类证明与合并（equiv_class.h） */
typedef struct EquivProof {
    EquivSourceType source;      /* DIRECT/COORD_EQUAL/CONSTRAINT/TRANSFORM/CONJUGATE */
    int node_a_id; int node_b_id;
    int deriving_constraint_id;
    int proof_step_id;
    TrustColor trust;
} EquivProof;
typedef struct EquivClass {
    int representative_id;
    int member_count; int capacity;
    int *member_ids;
    EquivProof *proofs; int proof_count; int proof_capacity;
    TrustColor min_trust;
} EquivClass;
struct EquivClassManager {
    ConstraintGraph *graph;
    int *uf_parent; int *uf_rank; int uf_capacity;   /* 并查集 */
    EquivClass *classes; int class_count; int class_capacity;
    int *node_to_class; int node_to_class_capacity;
    EquivProof *proof_log; int proof_log_count; int proof_log_capacity;
    int total_merges, coord_merges, constraint_derives,
        algebraic_conjugates, transform_merges, rejected_merges;
    void *stream_ctx;
};

/* 语言层 AST 节点（lv_ast.h）：声明/约束语句的 tagged union */
struct LvAstNode {
    LvAstNodeType type;
    LvSourceLoc loc;
    LvAstNode *next;   /* 兄弟链表 */
    LvAstNode *child;  /* 首个子节点 */
    int child_count;
    union { struct { int entity_type; char *names; LvAstNode *value;
                     char *return_type; } decl; /* ... 其余变体 ... */ } data;
};
```

## 主要接口

| 分组 | 函数签名 | 说明 |
|------|----------|------|
| 生命周期 | `ConstraintGraph *graph_create(void)` | 创建空图 |
| 生命周期 | `ConstraintGraph *graph_copy(const ConstraintGraph*)` | 深拷贝（vtable->clone + fixup_refs） |
| 生命周期 | `void graph_destroy(ConstraintGraph*)` | 销毁 |
| 节点添加 | `AddNodeResult graph_add_point(ConstraintGraph*, SymbolicCoord*const*, int)` | 添加点 |
| 节点添加 | `AddNodeResult graph_add_line_segment(ConstraintGraph*, int, int)` | 添加线段 |
| 节点添加 | `AddNodeResult graph_add_region(ConstraintGraph*, const int*, int)` | 添加区域 |
| 节点添加 | `AddNodeResult graph_add_circle(ConstraintGraph*, int, int)` | 添加圆 |
| 节点添加 | `AddNodeResult graph_add_port(ConstraintGraph*, PortType, int, int)` | 添加端口 |
| 节点添加 | `AddNodeResult graph_add_function_block(ConstraintGraph*, ...)` | 添加函数块 |
| 约束添加 | `AddConstraintResult graph_add_incidence(graph, point_id, line_or_region_id)` | 关联 |
| 约束添加 | `AddConstraintResult graph_add_betweenness(graph, p1, p2, p3)` | 之间 |
| 约束添加 | `AddConstraintResult graph_add_intersection(graph, l1, l2, point)` | 相交 |
| 约束添加 | `AddConstraintResult graph_add_containment(graph, inner, outer)` | 包含 |
| 约束添加 | `AddConstraintResult graph_add_connection(graph, src_port, dst_port)` | 端口连接 |
| 约束添加 | `AddConstraintResult graph_add_angle(graph, l1, l2, double)` | 角度（度） |
| 移除 | `RemoveNodeResult graph_remove_node(graph, int)` | 移除节点 |
| 移除 | `RemoveConstraintResult graph_remove_constraint(graph, int)` | 移除约束 |
| 查询 | `GeomNode *graph_get_node(const ConstraintGraph*, int)` | O(1) 节点查询 |
| 查询 | `Constraint *graph_get_constraint(const ConstraintGraph*, int)` | O(1) 约束查询 |
| 查询 | `int graph_find_constraints_involving(graph, node_id, int*, int)` | 涉及约束（反向索引） |
| 相容性 | `bool graph_check_compatibility(graph, lvConstraintCompatibilityResult*)` | 相容性诊断 |
| 冗余/冲突 | `int *graph_detect_redundant_constraints(graph, int*)` | 冗余约束 ID 数组 |
| 冗余/冲突 | `int **graph_detect_conflicts(graph, int*, int**)` | 冲突组数组 |
| 冗余/冲突 | `int graph_detect_redundancy(graph, ConstraintType, const int*, int)` | 单约束冗余检查 |
| 区域校验 | `bool graph_validate_region_closure(graph, int)` | 区域闭合验证 |
| 序列化 | `char *graph_serialize_to_json(const ConstraintGraph*)` | 图 → JSON |
| 序列化 | `ConstraintGraph *graph_deserialize_from_json(const char*)` | JSON → 图 |
| 序列化 | `char *graph_node_serialize_to_json(const GeomNode*)` | 节点 → JSON |
| DOT 导出 | `char *graph_export_dot(graph, const DOTExportConfig*)` | 图 → DOT |
| DOT 导出 | `int graph_export_dot_to_svg(graph, const DOTExportConfig*, const char*)` | 图 → SVG |
| 生命周期 | `void graph_mark_dirty(ConstraintGraph*)` / `void graph_sync_nodes(...)` | 脏标记与同步 |
| 生命周期 | `int graph_deactivate_constraint(graph, int)` | 惰性废弃约束 |
| 跨边界 | `CrossBoundaryConstraint *find_cross_boundary_constraints(...)` | 函数块跨边界约束 |
| 错误 | `void graph_set_error(graph, const char *fmt, ...)` | 设置错误（lvContext 优先） |
| 索引 | `void graph_node_index_insert / graph_constraint_index_insert` | 哈希索引注册 |
| 索引 | `void graph_index_rebuild(ConstraintGraph*)` | 整图重建索引 |
| 名称映射 | `const char *lv_geom_type_name(int)` / `lv_constraint_type_name(...)` | 枚举 ↔ 字符串 |
| 图哈希 | `GraphHash *compute_complete_graph_hash(const ConstraintGraph*)` | 顺序无关指纹 |
| 图哈希 | `uint64_t compute_quick_graph_hash(const ConstraintGraph*)` | 轻量哈希（循环检测） |
| 等价类 | `EquivClassManager *equiv_manager_create(ConstraintGraph*)` | 管理器创建 |
| 等价类 | `EquivMergeResult equiv_merge_classes(mgr, a, b, source, cid, trust)` | 合并两节点类 |
| 等价类 | `int equiv_merge_all(EquivClassManager*)` | 全量合并（坐标/约束/共轭/变换） |
| 等价类 | `int equiv_find(const EquivClassManager*, int)` | 并查集查找 |
| 等价类 | `void equiv_get_statistics(mgr, int64_t*, int64_t*, ...)` | 合并统计 |

## 工作流程

1. **图创建**：`graph_create` 初始化动态数组、`_Atomic` ID 计数器与哈希索引，并分配每图级错误缓冲区。
2. **构建**：语言层经 `lv_ast.h` AST（如 `LV_AST_DECLARATION`、`LV_AST_CONSTRAINT_STMT`）解析后，调用 `graph_add_point` / `graph_add_line_segment` / `graph_add_incidence` 等填充图；ID 经 `GRAPH_ATOMIC_*_INCREMENT` 原子分配，节点 / 约束同时注册进哈希索引。
3. **属性同步**：每次约束变更后 `graph_mark_dirty`，求解前 `graph_sync_nodes` 刷新受影响节点属性。
4. **质量分析**：`graph_check_compatibility` 给出 CONSISTENT / INCONSISTENT / UNDER_CONSTRAINED / OVER_CONSTRAINED 诊断；`graph_detect_redundant_constraints` 与 `graph_detect_conflicts` 输出冗余/冲突 ID 集合。
5. **合并化简**：`equiv_manager_create` 建立并查集，`equiv_merge_by_coord` / `equiv_derive_from_constraints` / `equiv_merge_algebraic_conjugates` / `equiv_merge_by_transform` 分类合并，`equiv_merge_classes` 写入 `EquivProof` 证明日志，`equiv_prove_merge_valid` 校验合并有效性。
6. **持久化与可视化**：`graph_serialize_to_json` 落盘，`graph_deserialize_from_json` 经 `graph_add_node_with_id` / `graph_add_constraint_with_id` 恢复；`graph_export_dot` / `graph_export_dot_to_svg` 生成可视化。
7. **复用与校验**：`graph_copy` 深拷贝（`vtable->clone` + `fixup_refs` 重映射指针），`compute_complete_graph_hash` 校验两图等价。

## 模块关系

| 模块 | 依赖方向 | 关系说明 |
|------|----------|----------|
| [01_symbolic_coord.md](01_symbolic_coord.md) | 依赖 | 节点坐标类型 `SymbolicCoord` / 信任颜色定义于符号坐标系统 |
| [04_solver.md](04_solver.md) | 被依赖 | 求解器以约束图节点/约束为输入，消费 `graph_check_compatibility` |
| [06_unify.md](06_unify.md) | 被依赖 | 合一在节点与约束上执行，等价类管理器提供合并基础设施 |
| [07_func_block.md](07_func_block.md) | 依赖 | 函数块节点（GEOM_FUNCTION_BLOCK）经 `find_cross_boundary_constraints` 检测边界 |
| [24_constraint_propagation.md](24_constraint_propagation.md) | 被依赖 | 传播使用 `involving_index` 反向索引加速 |
| [14_solver_backends.md](14_solver_backends.md) | 被依赖 | 各后端从图取约束集合并回写结果 |
| [15_geometry_advanced.md](15_geometry_advanced.md) | 被依赖 | 高级几何模块扩展节点/约束语义 |
| [26_interactive_geometry.md](26_interactive_geometry.md) | 被依赖 | 交互编辑经节点增删与脏标记同步 |
| [31_stream_interop.md](31_stream_interop.md) | 依赖 | `graph_set_stream_context` 输出流式上下文 |

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0 | 2026-08 | 首版：六类节点/六类约束、O(1) 哈希索引、冗余/冲突检测、JSON 与 DOT 导出 |
| v3.4.1 | 2026-08 | 原子 ID 分配（`_Atomic` + `lv_ATOMIC_ADD`）；v3.4.0 统一 lvContext 错误系统 |
| v3.5.0 | 2026-08 | 约束惰性废弃与 dirty 同步；节点 → 约束反向索引（`involving_index`） |
| v3.6.0 | 2026-08 | 反向索引版本化重建，热路径查询降为 O(度数) |
