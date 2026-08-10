# 03 图规范化遍引擎（Normalization Pass Engine）

## 模块概述

图规范化遍引擎是 Lv-00 约束图（ConstraintGraph）的"清洗"层，负责消除构造中出现的重复与冗余几何对象，使下游求解器、合一器与汇合性验证基于最小化图工作。它提供三类合并遍（pass）：

- **点合并**：通过 `find_merge_candidates()` 检测符号坐标等价的作用域感知候选，再由 `apply_merges()` 批量应用；
- **线段合并**：`merge_line_segments()` 合并共线且共享端点的线段；
- **区域合并**：`merge_regions()` 合并重叠或相邻的边界区域。

幂等性保证由 `normalization_verify_idempotency()` 显式验证，配合 `RewriteHistory` 的图哈希历史进行循环检测，确保重复运行规范化不会无限改写。本模块的公共接口全部声明于 `core/include/lv/normalization.h`，内部复用于 `critical_pair.h` 关键对比较（归约结果先规范化再合一判等）。

运行时机与位置：规范化一般位于图编辑提交之后、代数求解（04_solver.md）与合一判定（06_unify.md）之前。它在约束图数据结构之上工作，不触碰底层符号算术（坐标的数值求值交由 symbolic_coord 层完成），因此既可离线整图运行，也可作为重写系统的单步算子被调度。三类合并遍按"点 → 线段 → 区域"的维度升序执行，低维合并产生的高维冗余会在后续遍中被吸收，避免重复处理。

与求解器的衔接：规范化是代数求解的预处理。冗余约束若直接进入求解器会虚增方程个数，甚至触发 `SOLVER_OVERCONSTRAINED` 的误判；因此规范化的输出直接决定求解的噪声底。`graph_topological_sort_stable()` 同时保证方程提取以稳定顺序遍历节点，使 `solver_extract_equations_full()` 生成的多项式在多次运行间逐位一致。反之，求解器报告 `SOLVER_OVERCONSTRAINED` 时也可提示规范化层"可能存在未合并的冗余"，两个模块在交互循环中互为反馈。

## 核心设计原则

1. **候选—应用分离**：`find_merge_candidates()` 只读扫描生成候选，`apply_merges()` 负责在图上施加变更并回填 `original_ids → representative_ids` 映射，避免"边扫描边改图"的顺序依赖。
2. **作用域感知**：节点携带 `namespace_depth` 与 `parent_block_id`（见 02_constraint_graph.md）。跨作用域合并一律经 `MergeConfirmCallback` 征询，回调返回 false 即拒绝，杜绝误并函数块内部/外部同名点。
3. **可审计的合并日志**：每次合并写入 `NormalizationLog`（`old_id/new_id/auto_merged`），自动合并与用户确认合并可区分，供回滚与审计。
4. **幂等即正确性**：规范化结果应使再跑一遍等价。等价判定基于确定性图哈希 `GraphHash`（graph_hash.h），`normalization_verify_idempotency()` 对同一图执行两遍规范化后比较哈希。
5. **终止性优先**：`RewriteHistory` 记录每一轮规范化后的图哈希，一旦哈希回到历史状态即判定循环并停止，防止震荡重写。
6. **流式可观测**：通过 `normalization_set_stream_context()` 注册 `StreamContext`，三类合并过程均发射规范化事件（置 NULL 可禁用）。
7. **最小侵入**：合并通过 ID 重映射（`representative_ids`）而非物理删除完成，约束对象的 `is_active` 惰性废弃机制（见 02_constraint_graph.md）保证被合并节点仍可被审计追踪，任何时刻均可由 `NormalizationLog` 重建合并前的结构。
8. **确定性**：候选生成的扫描顺序、哈希计算与拓扑排序均稳定，同一输入图在任何平台产生同一输出，这是幂等验证与汇合性比较成立的前提。

## 关键数据结构（C 代码块）

以下类型均来自 `normalization.h`：

```c
typedef struct NormalizationLogEntry {
    int old_id;       /* 被合并的节点 ID */
    int new_id;       /* 保留的代表节点 ID */
    bool auto_merged; /* true=自动合并，false=用户确认 */
} NormalizationLogEntry;

typedef struct NormalizationLog {
    lvDArray entries; /* 日志条目数组 */
} NormalizationLog;

typedef struct NormalizationResult {
    int *merged_node_ids;   /* 被合并节点 ID 数组 */
    int merged_count;
    int merged_capacity;    /* 预分配容量（边界检查用） */
    int *original_ids;      /* 合并前 ID 列表 */
    int *representative_ids;/* 合并后代表节点 ID */
    bool user_confirmed;    /* 是否发生用户确认的合并 */
    NormalizationLog *log;  /* 详细合并日志（结果拥有所有权） */
} NormalizationResult;

typedef struct NodeMergeCandidate {
    int node_a_id;          /* 候选节点 A */
    int node_b_id;          /* 候选节点 B */
    SymbolicCoord *coord_a; /* A 的符号坐标 */
    SymbolicCoord *coord_b; /* B 的符号坐标 */
    long long scope_a;      /* A 的作用域深度 */
    long long scope_b;      /* B 的作用域深度 */
} NodeMergeCandidate;

typedef struct RewriteHistory {
    GraphHash **history; /* 历史图哈希数组 */
    int count;
    int capacity;
} RewriteHistory;

typedef bool (*MergeConfirmCallback)(int node_a_id, int node_b_id,
                                     int scope_a_depth, int scope_b_depth,
                                     int parent_a, int parent_b, void *user_data);
```

各结构体在管线中的角色：

- `NodeMergeCandidate` 是"点合并"遍的中间产物。`scope_a/scope_b` 采用 `long long` 而非 `int`：一方面为深度嵌套（多层子图、递归模块展开）预留 64 位安全裕度；另一方面允许 `-1` 编码"无作用域/全局作用域"而不与真实深度冲突，且与 `ConstraintGraph` 内部作用域字段的宽度保持一致，避免跨平台截断风险。
- `NormalizationResult` 同时携带"谁被合并"（`merged_node_ids`）与"合并后留谁"（`representative_ids`）两套视图，并内置 `merged_capacity` 用于边界检查，防止外部调用者越界读数组。
- `RewriteHistory` 保存的是 `GraphHash*` 快照（graph_hash.h 提供的确定性图哈希，非直接保存图对象），因此循环检测的成本是 O(哈希) 而非 O(整图)。
- `MergeConfirmCallback` 的七个参数完整描述一次跨作用域合并的上下文：双方节点 ID、各自作用域深度与父块 ID，`user_data` 由 `normalization_set_merge_callback()` 原样透传，可用于实现"白名单/黑名单"策略。
- `NormalizationLogEntry.auto_merged` 是审计的关键区分位：自动合并由引擎内部判定（坐标等价/共线/区域重叠），用户确认合并则由 `MergeConfirmCallback` 批准。二者都写入同一 `NormalizationLog`，使"撤销到用户确认点"或"复现上次自动合并"成为可能。`NormalizationLog` 内部以 `lvDArray` 存储，容量由 `normalization_log_create(initial_capacity)` 预分配，避免热路径上的频繁 realloc。

## 主要接口（表格）

| 接口 | 签名 | 说明 |
| --- | --- | --- |
| `graph_normalize` | `NormalizationResult *(ConstraintGraph*, bool scope_aware)` | 入口：执行完整的规范化流程，返回合并结果（含日志），失败返回 NULL |
| `merge_line_segments` | `int (ConstraintGraph*, NormalizationLog*)` | 合并共线线段，返回合并数，出错返回 -1 |
| `merge_regions` | `int (ConstraintGraph*, NormalizationLog*)` | 合并重叠/相邻区域，返回合并数，出错返回 -1 |
| `find_merge_candidates` | `NodeMergeCandidate *(const ConstraintGraph*, int *out_count)` | 扫描全图生成点合并候选，调用者用 `merge_candidates_destroy` 释放 |
| `apply_merges` | `int (ConstraintGraph*, NodeMergeCandidate*, int, bool *user_confirmed)` | 批量应用候选；跨作用域合并触发 `MergeConfirmCallback` |
| `merge_candidates_destroy` | `void (NodeMergeCandidate*, int)` | 释放候选数组 |
| `normalization_verify_idempotency` | `bool (ConstraintGraph*)` | 两遍规范化并比较图哈希，验证幂等性 |
| `rewrite_history_create` | `RewriteHistory *(int capacity)` | 创建重写历史 |
| `rewrite_history_check_cycle` | `bool (const RewriteHistory*, const ConstraintGraph*)` | 当前图哈希已在历史中即判循环，返回 true |
| `rewrite_history_add` | `void (RewriteHistory*, ConstraintGraph*)` | 记录当前图的哈希快照 |
| `rewrite_history_destroy` | `void (RewriteHistory*)` | 销毁历史 |
| `graph_topological_sort_stable` | `void (ConstraintGraph*)` | 稳定的拓扑排序，供下游按依赖序访问 |
| `normalization_set_stream_context` | `void (StreamContext*)` | 设置/禁用（NULL）流式事件输出 |
| `normalization_set_merge_callback` | `void (MergeConfirmCallback, void *user_data)` | 注册跨作用域合并确认回调 |
| `normalization_get_merge_callback` | `MergeConfirmCallback (void)` | 读取当前确认回调 |
| `normalization_log_create/destroy/record` | — | 合并日志生命周期与单条记录写入 |

调用约定与所有权：

- `graph_normalize()` 返回的 `NormalizationResult` 由调用者持有，用 `normalization_result_destroy()` 释放；结果内的 `log` 由结果拥有所有权，无需单独销毁。
- `find_merge_candidates()` 返回的候选数组必须用 `merge_candidates_destroy(candidates, count)` 成对释放；`count` 必须与 `out_count` 一致。
- `merge_line_segments()`/`merge_regions()` 的 `log` 参数可为 NULL（不记录），返回值为执行的合并数，出错返回 -1。
- 三类合并遍均可在 `scope_aware=true` 与 `false` 两种模式下运行：`false` 模式仅做同作用域合并，速度快且无回调开销，适用于已隔离的子图（如函数块展开后的内部图）；`true` 模式启用跨作用域候选与 `MergeConfirmCallback` 确认。
- 幂等验证接口 `normalization_verify_idempotency()` 只读不修改图；它内部以整图 `GraphHash` 比较两次规范化结果，适用于测试与 CI 门禁。
- `graph_topological_sort_stable()` 就地调整 `graph->nodes` 数组的排列；持有旧指针快照的调用方需在排序后经 `graph_get_node()` 重新获取。排序不改变任何节点 ID，约束的参与者引用不受影响。

## 工作流程

一次典型调用由调用方发起 `graph_normalize(graph, scope_aware)`，内部按以下顺序编排各遍；单独调用各合并遍（`merge_line_segments`/`merge_regions`/`find_merge_candidates`+`apply_merges`）同样合法，适用于局部定点收敛场景：

1. **前置同步**：调用方先经 `graph_mark_dirty()`/`graph_sync_nodes()`（02_constraint_graph.md）同步节点属性，保证符号坐标最新。
2. **点合并**：`find_merge_candidates()` 以符号坐标等价（坐标判定语义见 01_symbolic_coord.md）与作用域深度生成候选；`apply_merges()` 应用，跨作用域候选经回调确认；节点 ID 经 `representative_ids` 重映射到关联约束的参与者。
3. **线段合并**：`merge_line_segments()` 将共享端点且共线的线段归并为单一代表线段，合并过程写入 `NormalizationLog`。
4. **区域合并**：`merge_regions()` 将边界重叠/相邻的区域合并，合并时校验 `graph_validate_region_closure()`（区域闭合性）。
5. **拓扑排序**：`graph_topological_sort_stable()` 输出稳定的访问顺序，为后续求解遍提供确定性的迭代次序。
6. **幂等验证**：`normalization_verify_idempotency()` 执行两遍规范化并比对 `GraphHash`；若不等则返回 false，提示上层回退或告警。
7. **终止守护**：长时间重写循环内用 `rewrite_history_check_cycle()` 检测哈希回环，命中即终止。

幂等性与终止性的关系：两者共同保证规范化是"良定义"的变换。幂等性（`normalization_verify_idempotency()`）断言规范化应用一次与应用多次产生同构图——这意味着规范化是一个投影算子（projection），第二次运行只会发现"无可合并项"并直接返回；终止性则由 `RewriteHistory` 守护——即便哈希比较因浮点/符号求值边界产生细微差异，一旦 `rewrite_history_check_cycle()` 检测到哈希重复即强制终止，从机制上排除无限重写。因此在引用本模块作为汇合性验证的一环（见 critical_pair.h 的使用方式）时，可以安全地把 `graph_normalize()` 视为对图的"规范化形"计算，而不必担心其对归约比较引入非确定性。

## 模块关系（表格）

依赖方向可概括为：**本模块向下仅依赖约束图与符号坐标两个底层领域模型（01/02），向上服务合一判定（06）、关键对引擎与求解器（04）**。它自身不产生任何几何/代数语义，只做结构等价归并，因此可被所有需要"图规范化形"的上层模块安全复用。该分层使规范化成为可独立测试的纯净变换：给定同一输入图，输出只取决于合并判定与哈希两个确定性环节。

| 相关模块 | 文档 | 关系说明 |
| --- | --- | --- |
| 符号坐标 | 01_symbolic_coord.md | `NodeMergeCandidate` 携带 `SymbolicCoord*`，点合并依据坐标等价性判定 |
| 约束图 | 02_constraint_graph.md | 本模块是约束图上的重写遍；依赖其 `GeomNode/Constraint` 结构、哈希索引与脏标记 |
| 合一系统 | 06_unify.md | 规范化的结果图是合一判等的输入，两者共同支撑构造等价性判定 |
| 关键对引擎 | critical_pair.h（头文件） | 关键对归约结果先经 `graph_normalize()` 规范化再合一比较，规范化是汇合性验证的极小可信基组件 |
| 深拷贝 | node_deep_copy.h（头文件） | 合并/重写时以 `node_deep_copy_geom_node()` 等接口创建独立副本，`fixup_refs` 修复内部引用 |
| 函数块 | 07_func_block.md | 作用域感知合并约束函数块内部/外部节点，避免跨块误并 |
| 求解器 | 04_solver.md | 规范化产出的最小化图进入代数求解；`graph_topological_sort_stable` 为求解迭代定序 |
| 稀疏后端 | 14_solver_backends.md | 规范化后节点/约束规模缩减，降低稀疏线性后端的装配成本 |
| 流式上下文 | 12_context_and_lifecycle.md | `normalization_set_stream_context()` 挂接全局 `StreamContext`，合并事件经流式通道对外可见，生命周期由上下文统一管理 |

## 版本历史

| 版本 | 变更说明 |
| --- | --- |
| v0.1 | 初稿：整理 normalization.h 公开接口（三类合并、候选—应用、幂等验证、重写历史循环检测），并记录与 01/02/06 及关键对引擎的依赖关系 |
| v0.2 | 补充：数据结构角色说明、调用约定与所有权、`scope_aware` 双模式语义、幂等性/终止性形式化关系、设计原则第 7/8 条 |
| v0.3 | 补充：与求解器的衔接（规范化作为求解预处理、拓扑定序保证方程逐位一致、过约束状态反向反馈） |

> 注：本表记录文档自身版本。代码模块的工程版本演进（如 `solver_dirty_set.h` 拆分对应的 v3.3.0+ 节点合并逻辑迁移）请参见各头文件注释与 git 历史。

> 文档编号体系：本文档为知识体系化 Wiki 的 03 号文档，编号与 01_symbolic_coord.md、02_constraint_graph.md、04_solver.md、06_unify.md 等对齐。
