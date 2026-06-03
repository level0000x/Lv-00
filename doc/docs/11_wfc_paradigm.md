# Lv-00 WFC 范式注入：局部规则 → 全局结构

> **版本**: v1.0-draft
> **日期**: 2026-05-26
> **状态**: 规划阶段
> **依赖**: Lv-00 v3.5.0-academic 五层架构

---

## 0. 核心洞见

波函数坍缩（WFC）、非周期铺砌（Aperiodic Tiling）、准晶（Quasicrystal）与 Lv-00 的约束求解共享同一个数学本质：

> **有限的局部规则，通过排除非法状态，强迫出唯一的（或有限类的）全局结构。**

本文档规划三个核心模块的实现，将这一范式注入 Lv-00 的第 3-4 层。

---

## 1. 数学基础

### 1.1 形式框架

定义约束系统为一个元组 $\mathcal{S} = (V, C, \Sigma, \delta)$：

| 符号 | 含义 |
|------|------|
| $V$ | 节点集合（几何实体） |
| $C \subseteq V \times V \times T$ | 约束集合（$T$ = 约束类型） |
| $\Sigma(v)$ | 节点 $v$ 的**状态空间**（可能取值的集合） |
| $\delta$ | **传播函数**：$\delta(c, \sigma) \to \mathcal{P}(\Sigma)$，约束 $c$ 将参与者的状态空间收缩为子集 |

**全局约束**：$\Sigma_{\text{global}} = \bigcap_{c \in C} \delta(c, \Sigma)$

**合法解**：$\sigma^* \in \Sigma_{\text{global}}$ 且对所有 $v \in V$，$\sigma^*(v) \in \Sigma(v)$

**死路检测**：若存在 $v$ 使得 $\Sigma(v) = \emptyset$，则系统不可满足。

### 1.2 与非周期铺砌的同构

| 非周期铺砌 | Lv-00 |
|-----------|-------|
| 瓦片类型 | `GeomType` + 约束邻接规则 |
| 瓦片旋转等价类 | 符号坐标判等（有理数/代数数/超越数） |
| 匹配规则（matching rules） | `ConstraintType` + 邻接约束 |
| 非法拼法 → 死路 | `INCONSISTENT` 四态检测 |
| 合法拼法 → 全局结构 | 归一化 + 约束传播 → 唯一解 |
| 非周期性（无平移对称） | 解的唯一性（`SOLVER_STATUS_UNIQUE`） |

### 1.3 与 WFC 的同构

| WFC | Lv-00（规划后） |
|-----|----------------|
| 熵最小化选择 | Module A: 传播引擎的节点选择策略 |
| 约束传播（arc consistency） | Module A: 增量约束传播 |
| 坍缩（collapse） | Module A: 状态确定化 |
| 回溯/重启 | Module A: 死路恢复 |
| 瓦片邻接矩阵 | Module B: 等价类邻接表 |
| 超集（superposition） | Module B: 等价类状态空间 |

---

## 2. 现有架构分析

### 2.1 已有能力

| 能力 | 实现位置 | 状态 |
|------|---------|------|
| 约束图 + 节点/边 | `constraint_graph.c` | ✅ 成熟 |
| 归一化（点/线段/区域合并） | `normalization.c` | ✅ 成熟 |
| 四态相容检测 | `constraint_graph.c: graph_check_compatibility()` | ✅ 成熟 |
| 增量脏标记 | `constraint_graph.c: graph_mark_dirty/sync_nodes()` | ✅ v3.5.0 |
| 惰性约束废弃 | `constraint_graph.c: graph_deactivate_constraint()` | ✅ v3.5.0 |
| 符号坐标精确判等 | `symbolic_coord.c: symbolic_coord_compare()` | ✅ 五级渐进 |
| 并查集合并 | `normalization.c: uf_*` | ✅ path splitting + rank |
| 代数求解器 | `solver.c: solve_algebraic_system()` | ✅ Groebner 基 |
| 增量求解 | `solver.c: solver_incremental_solve()` | ✅ BFS 依赖传播 |
| CDCL SAT 内核 | `solver_core.c` | 🔶 框架预留，核心为桩 |
| 合一检查（三层匹配） | `unify.c` | ✅ 端口→约束→坐标 |
| 证明系统 + 信任颜色 | `proof.c` | ✅ 多策略 + 颜色传播 |
| 反证法作用域收束 | `proof.c: proof_apply_ex_falso_scoped()` | ✅ 防爆炸原理污染 |

### 2.2 缺失能力（本次规划填补）

| 缺失能力 | 对应模块 | 优先级 |
|---------|---------|--------|
| **动态约束传播**（arc consistency / AC-3 风格） | Module A | P0 |
| **熵最小化节点选择**（WFC 风格） | Module A | P0 |
| **死路检测 + 恢复**（backtrack/restart） | Module A | P0 |
| **等价类代数结构**（同余关系、商代数） | Module B | P1 |
| **语义等价类合并**（超越坐标判等） | Module B | P1 |
| **剪枝合法性元证明**（完备性论证） | Module C | P2 |

---

## 3. Module A：约束传播引擎（Constraint Propagation Engine）

### 3.1 设计目标

将 Lv-00 从"批量约束收集 → 全量求解"升级为"**增量约束传播 → 动态状态收缩 → 按需求解**"。

### 3.2 核心数据结构

```c
/* ========== propagation.h ========== */

typedef enum {
    PROP_STRATEGY_MIN_ENTROPY,    /* WFC: 选择状态空间最小的节点 */
    PROP_STRATEGY_MRVS,           /* MRV: 最少剩余值（约束满足术语） */
    PROP_STRATEGY_DEGREE,         /* Degree: 选择度数最高的节点 */
    PROP_STRATEGY_BFS,            /* BFS: 从最近修改处广度传播 */
    PROP_STRATEGY_TOPOLOGICAL     /* 拓扑序: 按依赖关系顺序 */
} PropagationStrategy;

typedef enum {
    PROP_RESULT_CONSISTENT,       /* 相容，可继续 */
    PROP_RESULT_CONTRADICTION,    /* 矛盾，需回溯 */
    PROP_RESULT_SATISFIED,        /* 所有节点已确定 */
    PROP_RESULT_STABLE,           /* 传播收敛，但仍有未确定节点 */
    PROP_RESULT_TIMEOUT           /* 超时 */
} PropagationResult;

/* 节点状态空间 */
typedef struct NodeStateSpace {
    int node_id;
    SymbolicCoord **possible_coords;  /* 可能坐标列表 */
    int coord_count;
    int capacity;
    bool is_collapsed;               /* 是否已坍缩为唯一值 */
    SymbolicCoord *collapsed_value;  /* 坍缩后的唯一值 */
} NodeStateSpace;

/* 传播上下文 */
typedef struct PropagationContext {
    ConstraintGraph *graph;           /* 关联约束图（只读引用） */
    NodeStateSpace *state_spaces;     /* 每个节点的状态空间 */
    int state_count;

    PropagationStrategy strategy;     /* 节点选择策略 */
    int max_iterations;               /* 最大传播轮次 */
    int max_backtracks;               /* 最大回溯次数 */

    /* 传播队列 */
    int *propagation_queue;           /* 待传播节点 ID */
    int queue_head, queue_tail;
    int queue_capacity;

    /* 回溯栈 */
    struct PropagationSnapshot *snapshot_stack;
    int snapshot_count;
    int snapshot_capacity;

    /* 统计 */
    int64_t propagation_steps;
    int64_t collapse_count;
    int64_t backtrack_count;
    int64_t prune_count;

    /* 流式事件 */
    StreamContext *stream_ctx;
} PropagationContext;
```

### 3.3 核心算法

#### 3.3.1 初始化状态空间

```c
/*
 * propagation_init_state_spaces()
 *
 * 为约束图中每个活跃节点初始化状态空间：
 * - GEOM_POINT: 基于当前坐标 + 邻域约束生成候选集
 * - GEOM_LINE_SEGMENT: 基于端点状态空间的笛卡尔积
 * - GEOM_REGION: 基于边界线段状态空间的组合
 *
 * 输入: ctx - 传播上下文（已关联约束图）
 * 输出: PROP_RESULT_CONSISTENT 或 PROP_RESULT_CONTRADICTION
 */
PropagationResult propagation_init_state_spaces(PropagationContext *ctx);
```

**候选坐标生成策略**：

对于 `GEOM_POINT` 节点 $v$：
1. 若 $v$ 已有精确坐标 → 状态空间 = `{当前坐标}`（已坍缩）
2. 若 $v$ 无坐标但有邻接约束 → 从邻接约束推导候选集：
   - `INCIDENCE`（点在线段上）：候选 = 线段参数化坐标
   - `INTERSECTION`（两线交点）：候选 = 交点坐标（代数求解）
   - `BETWEENNESS`（点在两点之间）：候选 = 线段上的参数化区间
3. 若 $v$ 完全无约束 → 状态空间 = `UNBOUNDED`（自由变量）

#### 3.3.2 AC-3 风格约束传播

```c
/*
 * propagation_arc_reduce()
 *
 * 对单个约束执行弧相容性检查：
 * 从参与者 A 的状态空间中移除所有"在 B 的当前状态下不可能"的值。
 *
 * 数学定义：
 *   对于约束 c = (A, B, type)，
 *   移除所有 a ∈ Σ(A) 使得 ¬∃ b ∈ Σ(B) : δ(c, (a, b)) 成立
 *
 * 输入: ctx, constraint_id
 * 输出: true = 状态空间被收缩, false = 无变化
 */
bool propagation_arc_reduce(PropagationContext *ctx, int constraint_id);

/*
 * propagation_run()
 *
 * AC-3 主循环：
 * 1. 将所有约束加入工作队列
 * 2. 取出约束，执行弧相容性检查
 * 3. 若某参与者状态空间收缩，将其所有邻接约束重新入队
 * 4. 若某参与者状态空间变空 → CONTRADICTION
 * 5. 队列为空 → 返回当前状态
 *
 * 输入: ctx
 * 输出: PropagationResult
 */
PropagationResult propagation_run(PropagationContext *ctx);
```

#### 3.3.3 熵最小化节点选择（WFC 核心）

```c
/*
 * propagation_select_node()
 *
 * WFC 风格节点选择：
 * 1. 计算每个未坍缩节点的"熵" H(v) = log₂|Σ(v)|
 * 2. 选择 H(v) 最小的节点（状态空间最小的）
 * 3. 若有多个节点熵相同，选择度数最高的（打破对称性）
 *
 * 输入: ctx
 * 输出: 节点 ID，或 -1（无候选）
 */
int propagation_select_node(PropagationContext *ctx);

/*
 * propagation_collapse()
 *
 * 坍缩：将选定节点的状态空间收缩为单一值。
 * 选择策略：从当前候选中按约束兼容性加权随机选择（或确定性选择第一个）。
 *
 * 输入: ctx, node_id
 * 输出: true = 成功坍缩, false = 状态空间为空
 */
bool propagation_collapse(PropagationContext *ctx, int node_id);
```

#### 3.3.4 完整 WFC 循环

```c
/*
 * propagation_wfc_solve()
 *
 * 完整的 WFC 求解循环：
 *
 *   loop:
 *     1. propagation_run()           -- AC-3 约束传播
 *     2. if CONTRADICTION → backtrack or restart
 *     3. if SATISFIED → return SUCCESS
 *     4. if STABLE → select_node() + collapse() → goto 1
 *
 * 输入: ctx
 * 输出: PropagationResult
 */
PropagationResult propagation_wfc_solve(PropagationContext *ctx);
```

#### 3.3.5 死路恢复

```c
/*
 * propagation_snapshot_save/restore()
 *
 * 在每次 collapse 前保存快照，矛盾时恢复。
 * 快照内容：所有 NodeStateSpace 的深拷贝。
 *
 * 策略：
 * - 顺序回溯（chronological backtracking）
 * - 非时序回溯（conflict-directed backjumping）：利用冲突分析跳过无关决策
 * - 重启（restart）：回溯次数超阈值时，从头开始（保留已学约束）
 */
PropagationSnapshot *propagation_snapshot_save(PropagationContext *ctx);
void propagation_snapshot_restore(PropagationContext *ctx, PropagationSnapshot *snap);
void propagation_snapshot_destroy(PropagationSnapshot *snap);
```

### 3.4 与现有模块的集成

```
engine_solve() 修改后的流水线：

  步骤 0: graph_normalize()              -- 归一化（不变）
  步骤 1: propagation_wfc_solve()        -- [新] WFC 约束传播
          ├─ 若 SATISFIED → 跳到步骤 4
          ├─ 若 CONTRADICTION → 报告矛盾
          └─ 若 STABLE → 继续步骤 2
  步骤 2: rewrite_with_rules()           -- 重写引擎（不变）
  步骤 3: run_solver_on_graph()          -- 代数求解器（处理 WFC 未确定的剩余变量）
  步骤 4: check_and_report_conflicts()   -- 冲突检查（不变）
  步骤 5: count_degrees_of_freedom()     -- 自由度更新（不变）
```

### 3.5 文件规划

| 文件 | 职责 |
|------|------|
| `core/include/lv00/propagation.h` | 传播引擎公共接口 |
| `core/src/core/propagation.c` | 传播引擎实现 |
| `test/c/test_propagation.c` | 传播引擎单元测试 |

### 3.6 测试用例

| 测试 ID | 描述 | 预期结果 |
|---------|------|---------|
| PROP-T01 | 三点共线，已知两点求第三点 | 一次坍缩直接确定 |
| PROP-T02 | 两圆交点（两个候选） | 状态空间 = {交点1, 交点2}，需选择 |
| PROP-T03 | 过约束三角形（三边给定） | 若矛盾 → CONTRADICTION |
| PROP-T04 | 欠约束四边形（仅两邻边） | STABLE，自由度 > 0 |
| PROP-T05 | 大规模网格（10×10 点阵） | WFC 传播收敛，性能 < 100ms |
| PROP-T06 | 死路 + 回溯恢复 | 回溯后找到合法解 |
| PROP-T07 | 重启策略 | 重启后收敛 |

---

## 4. Module B：等价类合并代数化（Algebraic Equivalence Class Merging）

### 4.1 设计目标

将当前的"坐标判等 → 并查集合并"推广为更一般的**代数等价关系**，使得合并操作本身成为可证明的数学结构。

### 4.2 数学定义

#### 4.2.1 等价关系

在节点集 $V$ 上定义等价关系 $\sim$，满足：
- **自反性**: $\forall v, v \sim v$
- **对称性**: $a \sim b \Rightarrow b \sim a$
- **传递性**: $a \sim b \land b \sim c \Rightarrow a \sim c$

当前 Lv-00 的并查集已满足这三条。但等价关系的**定义来源**需要推广。

#### 4.2.2 等价关系的来源分类

| 来源 | 数学描述 | 当前支持 | 规划支持 |
|------|---------|---------|---------|
| **坐标相等** | $\text{coord}(a) = \text{coord}(b)$（精确符号判等） | ✅ | ✅ |
| **约束推导** | 由约束链推导的隐式等价（如中点、对称点） | 🔶 部分 | ✅ |
| **代数共轭** | 同一极小多项式的不同实根（代数数等价） | ❌ | ✅ |
| **几何变换** | $b = T(a)$，$T$ 为合同变换（旋转/平移/反射） | ❌ | ✅ |
| **语义等价** | 满足相同约束模式的节点（约束图同构） | ❌ | 🔶 |

#### 4.2.3 商代数（Quotient Algebra）

定义商集 $V/{\sim} = \{[v] : v \in V\}$，其中 $[v] = \{u \in V : u \sim v\}$。

约束在商集上的自然诱导：
$$\bar{c} = \{([v_1], \ldots, [v_n], t) : \exists v_i' \in [v_i], (v_1', \ldots, v_n', t) \in C\}$$

**关键定理**：若 $\mathcal{S}$ 相容，则 $\mathcal{S}/{\sim}$ 也相容（等价合并保持相容性）。

### 4.3 核心数据结构

```c
/* ========== equiv_class.h ========== */

/* 等价来源类型 */
typedef enum {
    EQUIV_SOURCE_COORD_EQUAL,      /* 坐标精确相等 */
    EQUIV_SOURCE_CONSTRAINT_DERIVE, /* 约束推导 */
    EQUIV_SOURCE_ALGEBRAIC_CONJ,   /* 代数共轭 */
    EQUIV_SOURCE_GEOM_TRANSFORM,   /* 几何变换 */
    EQUIV_SOURCE_SEMANTIC_PATTERN  /* 语义模式匹配 */
} EquivSourceType;

/* 等价证明 */
typedef struct EquivProof {
    EquivSourceType source;
    int node_a_id;
    int node_b_id;
    int deriving_constraint_id;     /* 推导来源约束（若适用） */
    int proof_step_id;              /* 证明步骤引用 */
    TrustColor trust;               /* 信任颜色 */
} EquivProof;

/* 等价类 */
typedef struct EquivClass {
    int representative_id;          /* 代表节点 ID */
    int *member_ids;                /* 成员节点 ID */
    int member_count;
    int capacity;

    EquivProof *proofs;             /* 等价证明链 */
    int proof_count;
    int proof_capacity;

    TrustColor min_trust;           /* 类内最低信任颜色 */
} EquivClass;

/* 等价类管理器 */
typedef struct EquivClassManager {
    ConstraintGraph *graph;         /* 关联约束图 */

    EquivClass *classes;            /* 等价类数组 */
    int class_count;
    int class_capacity;

    /* 节点 → 类索引的 O(1) 映射 */
    int *node_to_class;             /* node_id → class_index */
    int node_to_class_capacity;

    /* 并查集（底层实现） */
    int *uf_parent;
    int *uf_rank;

    /* 等价关系来源追踪 */
    EquivProof *proof_log;
    int proof_log_count;
    int proof_log_capacity;
} EquivClassManager;
```

### 4.4 核心算法

#### 4.4.1 坐标等价（已有，封装）

```c
/*
 * equiv_merge_by_coord()
 *
 * 封装现有的 find_merge_candidates + apply_merges。
 * 为每对合并生成 EQUIV_SOURCE_COORD_EQUAL 证明。
 */
int equiv_merge_by_coord(EquivClassManager *mgr);
```

#### 4.4.2 约束推导等价

```c
/*
 * equiv_derive_from_constraints()
 *
 * 从约束图推导隐式等价：
 *
 * 规则 1 - 中点等价：
 *   若 B 是 A,C 的中点（BETWEENNESS + AB=BC），
 *   且 D 是 E,F 的中点，
 *   且 AC = EF（全等），
 *   则 B ~ D（若 A~E 且 C~F）
 *
 * 规则 2 - 对称等价：
 *   若 l 是 A,B 的对称轴（垂直平分线），
 *   且 C 关于 l 的对称点为 D，
 *   则 C ~ D（若 A~B）
 *
 * 规则 3 - 交点等价：
 *   若 l1 ∩ l2 = {P}，且 l1' ∩ l2' = {P'}，
 *   且 l1 ~ l1'，l2 ~ l2'，
 *   则 P ~ P'
 *
 * 输入: mgr
 * 输出: 新发现的等价对数量
 */
int equiv_derive_from_constraints(EquivClassManager *mgr);
```

#### 4.4.3 代数共轭等价

```c
/*
 * equiv_merge_algebraic_conjugates()
 *
 * 检测代数共轭等价：
 * 若两个节点的坐标是同一极小多项式的不同实根，
 * 则它们在代数意义上"等价"（可互换而不破坏约束）。
 *
 * 算法：
 * 1. 收集所有 ALGEBRAIC 类型坐标
 * 2. 按极小多项式哈希分组
 * 3. 组内比较极小多项式系数（精确 GMP 比较）
 * 4. 验证：交换共轭对后，所有涉及约束仍然成立
 *
 * 输入: mgr
 * 输出: 新发现的共轭等价对数量
 */
int equiv_merge_algebraic_conjugates(EquivClassManager *mgr);
```

#### 4.4.4 几何变换等价

```c
/*
 * equiv_merge_by_transform()
 *
 * 检测几何变换下的等价：
 * 若存在合同变换 T（旋转/平移/反射/组合）使得 T(A) = B，
 * 则 A ~ B。
 *
 * 算法：
 * 1. 对每对未等价节点 (A, B)，尝试求解变换 T
 *    - 两点确定平移：T(v) = v + (B - A)
 *    - 三点确定旋转+平移：求解刚体变换
 * 2. 验证 T 是否将 A 的所有邻域约束映射到 B 的邻域约束
 * 3. 若验证通过，合并等价类
 *
 * 输入: mgr
 * 输出: 新发现的变换等价对数量
 */
int equiv_merge_by_transform(EquivClassManager *mgr);
```

#### 4.4.5 合并合法性证明

```c
/*
 * equiv_prove_merge_valid()
 *
 * 证明合并操作的合法性：
 * 给定等价类合并 [a] ∪ [b] = [c]，
 * 证明合并后的约束系统仍然相容。
 *
 * 方法：
 * 1. 在合并后的商图上执行 graph_check_compatibility()
 * 2. 若结果为 CONSISTENT → 合法
 * 3. 若结果为 INCONSISTENT → 非法，回滚合并
 * 4. 生成证明步骤，记录到 ProofNavigator
 *
 * 输入: mgr, class_a_idx, class_b_idx
 * 输出: true = 合法, false = 非法
 */
bool equiv_prove_merge_valid(EquivClassManager *mgr,
                              int class_a_idx, int class_b_idx);
```

### 4.5 与现有归一化的关系

```
graph_normalize() 修改后的流水线：

  阶段 1: equiv_merge_by_coord()              -- 坐标等价（封装现有逻辑）
  阶段 2: equiv_derive_from_constraints()      -- [新] 约束推导等价
  阶段 3: equiv_merge_algebraic_conjugates()   -- [新] 代数共轭等价
  阶段 4: equiv_merge_by_transform()           -- [新] 几何变换等价
  阶段 5: merge_line_segments()                -- 线段合并（不变）
  阶段 6: merge_regions()                      -- 区域合并（不变）
  阶段 7: graph_topological_sort_stable()      -- 稳定化（不变）
```

### 4.6 文件规划

| 文件 | 职责 |
|------|------|
| `core/include/lv00/equiv_class.h` | 等价类管理器公共接口 |
| `core/src/core/equiv_class.c` | 等价类管理器实现 |
| `test/c/test_equiv_class.c` | 等价类单元测试 |

### 4.7 测试用例

| 测试 ID | 描述 | 预期结果 |
|---------|------|---------|
| EQC-T01 | 两点坐标相同 | 自动合并，生成 COORD_EQUAL 证明 |
| EQC-T02 | 中点等价推导 | 两个全等三角形的中点自动等价 |
| EQC-T03 | 代数共轭等价 | sqrt(2) 和 -sqrt(2) 被识别为共轭 |
| EQC-T04 | 旋转对称等价 | 正六边形的对称顶点自动等价 |
| EQC-T05 | 合并合法性证明 | 非法合并被拒绝，生成 INCONSISTENT 证明 |
| EQC-T06 | 传递性验证 | A~B, B~C → A,B,C 全部合并 |
| EQC-T07 | 幂等性 | 两次合并结果相同 |

---

## 5. Module C：剪枝合法性元证明（Pruning Validity Meta-Proof）

### 5.1 设计目标

在证明系统之上增加一个**元证明层**，证明"被排除的状态空间确实不包含合法解"。这是整个 WFC 范式的数学严格化基础。

### 5.2 数学定义

#### 5.2.1 剪枝操作

定义剪枝 $\pi$ 为一个三元组 $(v, R, \phi)$：
- $v$：被剪枝的节点
- $R \subset \Sigma(v)$：被移除的状态子集
- $\phi$：剪枝理由（证明）

**剪枝合法性条件**：$\pi$ 是合法的，当且仅当
$$\forall r \in R, \forall \sigma^* \in \Sigma_{\text{global}} : \sigma^*(v) \neq r$$

即：被移除的每个状态都不属于任何全局合法解。

#### 5.2.2 完备性定理

**定理**（剪枝完备性）：若剪枝序列 $\pi_1, \pi_2, \ldots, \pi_n$ 中每一步都是合法的，且最终 $\Sigma_{\text{global}} \neq \emptyset$，则 $\Sigma_{\text{global}}$ 中的所有解都是原问题的合法解。

**逆否命题**：若原问题存在合法解 $\sigma^*$，则不存在合法剪枝能移除 $\sigma^*$。

#### 5.2.3 证明策略

剪枝合法性的证明分为三个层次：

| 层次 | 策略 | 强度 | 适用场景 |
|------|------|------|---------|
| L1: 直接矛盾 | 证明 $r$ 与某约束直接矛盾 | 强 | 坐标不满足约束 |
| L2: 传播矛盾 | 证明 $r$ 通过约束传播导致某节点状态空间为空 | 中 | 间接矛盾 |
| L3: 代数排除 | 证明 $r$ 不满足多项式方程组的解集 | 强 | Groebner 基排除 |

### 5.3 核心数据结构

```c
/* ========== meta_proof.h ========== */

/* 剪枝操作 */
typedef struct PruningOperation {
    int node_id;
    SymbolicCoord **removed_states;  /* 被移除的状态 */
    int removed_count;

    enum {
        PRUNE_DIRECT_CONTRADICTION,  /* L1: 直接矛盾 */
        PRUNE_PROPAGATION_CONTRADICTION, /* L2: 传播矛盾 */
        PRUNE_ALGEBRAIC_EXCLUSION   /* L3: 代数排除 */
    } strategy;

    int conflicting_constraint_id;   /* L1: 矛盾约束 */
    int propagation_trace_id;        /* L2: 传播追踪 ID */
    mpz_poly_t *exclusion_polys;     /* L3: 排除多项式 */
    int poly_count;

    TrustColor trust;                /* 剪枝信任颜色 */
} PruningOperation;

/* 剪枝记录 */
typedef struct PruningRecord {
    PruningOperation *operations;
    int operation_count;
    int capacity;
    int64_t total_states_removed;
    int64_t total_states_remaining;
} PruningRecord;

/* 元证明上下文 */
typedef struct MetaProofContext {
    ProofNavigator *navigator;       /* 关联证明导航器 */
    ConstraintGraph *graph;          /* 关联约束图 */
    PropagationContext *prop_ctx;    /* 关联传播上下文（Module A） */
    EquivClassManager *equiv_mgr;    /* 关联等价类管理器（Module B） */

    PruningRecord *record;           /* 剪枝记录 */
    ProofStep **proof_steps;         /* 生成的证明步骤 */
    int proof_step_count;
} MetaProofContext;
```

### 5.4 核心算法

#### 5.4.1 L1: 直接矛盾证明

```c
/*
 * meta_prove_direct_contradiction()
 *
 * 证明状态 r 对节点 v 是非法的，因为 r 与某约束 c 直接矛盾。
 *
 * 方法：
 * 1. 将 r 代入约束 c 的代数表达式
 * 2. 若结果非零（使用 symbolic_coord_is_zero 精确判断）→ 矛盾
 * 3. 生成证明步骤：PRUNE_DIRECT_CONTRADICTION
 *
 * 示例：
 *   约束: INCIDENCE(P, L)，L 端点为 (0,0) 和 (1,0)
 *   候选: r = (0, 1)
 *   验证: (0-0)*(0-0) - (1-0)*(0-0) = 0 ✓（在直线上，不矛盾）
 *   候选: r = (0, 1)
 *   验证: (0-0)*(1-0) - (1-0)*(0-0) = 0 ✓（巧合在直线上）
 *   候选: r = (2, 1)
 *   验证: (2-0)*(1-0) - (1-0)*(0-0) = 2 ≠ 0 → 矛盾！
 */
bool meta_prove_direct_contradiction(MetaProofContext *ctx,
                                      int node_id,
                                      SymbolicCoord *candidate,
                                      int *out_conflicting_constraint_id);
```

#### 5.4.2 L2: 传播矛盾证明

```c
/*
 * meta_prove_propagation_contradiction()
 *
 * 证明状态 r 对节点 v 是非法的，因为选择 r 后约束传播导致矛盾。
 *
 * 方法：
 * 1. 临时将 v 坍缩为 r
 * 2. 运行约束传播（Module A）
 * 3. 若传播结果为 CONTRADICTION → r 非法
 * 4. 记录传播路径作为证明
 * 5. 恢复原始状态
 *
 * 输入: ctx, node_id, candidate
 * 输出: true = 证明 r 非法, false = r 可能合法
 */
bool meta_prove_propagation_contradiction(MetaProofContext *ctx,
                                           int node_id,
                                           SymbolicCoord *candidate);
```

#### 5.4.3 L3: 代数排除证明

```c
/*
 * meta_prove_algebraic_exclusion()
 *
 * 证明状态 r 对节点 v 是非法的，因为 r 不满足多项式方程组的解集。
 *
 * 方法：
 * 1. 从约束图提取多项式方程组 G（Groebner 基）
 * 2. 将 r 的坐标代入 G
 * 3. 若存在 g ∈ G 使得 g(r) ≠ 0 → r 不在解集中
 * 4. 使用 symbolic_coord 精确计算（避免浮点误差）
 *
 * 输入: ctx, node_id, candidate
 * 输出: true = 证明 r 非法, false = r 可能在解集中
 */
bool meta_prove_algebraic_exclusion(MetaProofContext *ctx,
                                     int node_id,
                                     SymbolicCoord *candidate);
```

#### 5.4.4 完备性验证

```c
/*
 * meta_prove_completeness()
 *
 * 验证整个剪枝序列的完备性：
 * 证明剪枝后的状态空间仍然包含原问题的所有合法解。
 *
 * 方法：
 * 1. 对每个被移除的状态 r，检查是否存在合法剪枝证明
 * 2. 若所有被移除状态都有合法证明 → 完备
 * 3. 若存在未被证明的移除 → 不完备（标记为 TRUST_AMBER）
 *
 * 输入: ctx
 * 输出: ProofColor（GREEN = 完备, AMBER = 部分完备, RED = 不完备）
 */
ProofColor meta_prove_completeness(MetaProofContext *ctx);
```

### 5.5 与证明系统的集成

```
ProofNavigator 扩展：

  现有证明步骤类型:
    ADD_NODE, ADD_CONSTRAINT, REWRITE, FUNCTION_APP,
    PACK_FUNCTION, NORMALIZATION, UNIFY, EX_FALSO, ORACLE

  [新增] 证明步骤类型:
    PRUNE_STATE,           -- 剪枝一个状态
    PRUNE_BATCH,           -- 批量剪枝
    PROPAGATION_STEP,      -- 传播步骤
    EQUIV_MERGE,           -- 等价类合并
    META_PROVE_DIRECT,     -- L1 直接矛盾证明
    META_PROVE_PROPAGATION, -- L2 传播矛盾证明
    META_PROVE_ALGEBRAIC,  -- L3 代数排除证明
    META_PROVE_COMPLETENESS -- 完备性验证
```

### 5.6 文件规划

| 文件 | 职责 |
|------|------|
| `core/include/lv00/meta_proof.h` | 元证明公共接口 |
| `core/src/core/meta_proof.c` | 元证明实现 |
| `test/c/test_meta_proof.c` | 元证明单元测试 |

### 5.7 测试用例

| 测试 ID | 描述 | 预期结果 |
|---------|------|---------|
| MPR-T01 | L1 直接矛盾：点不在直线上 | 正确排除，生成 GREEN 证明 |
| MPR-T02 | L2 传播矛盾：选择导致链式矛盾 | 正确排除，传播路径可追溯 |
| MPR-T03 | L3 代数排除：坐标不满足方程 | 正确排除，Groebner 基验证 |
| MPR-T04 | 完备性验证：所有剪枝合法 | GREEN 完备性证明 |
| MPR-T05 | 完备性验证：存在未证明剪枝 | AMBER 部分完备 |
| MPR-T06 | 与反证法协作 | 作用域收束 + 剪枝证明 |
| MPR-T07 | 大规模完备性（100 节点） | 性能 < 1s |

---

## 6. 实现计划

### 6.1 阶段划分

```
Phase 1: Module A — 约束传播引擎（P0）
  ├─ Week 1: propagation.h/c 数据结构 + 初始化
  ├─ Week 2: AC-3 传播核心 + 弧相容性
  ├─ Week 3: WFC 循环（选择 + 坍缩 + 回溯）
  └─ Week 4: 集成到 engine_solve + 测试

Phase 2: Module B — 等价类合并代数化（P1）
  ├─ Week 5: equiv_class.h/c 数据结构 + 坐标等价封装
  ├─ Week 6: 约束推导等价 + 代数共轭等价
  ├─ Week 7: 几何变换等价 + 合法性证明
  └─ Week 8: 集成到 graph_normalize + 测试

Phase 3: Module C — 剪枝合法性元证明（P2）
  ├─ Week 9: meta_proof.h/c 数据结构 + L1 直接矛盾
  ├─ Week 10: L2 传播矛盾 + L3 代数排除
  ├─ Week 11: 完备性验证 + 与证明系统集成
  └─ Week 12: 端到端集成测试 + 性能优化
```

### 6.2 依赖关系

```
Module A (传播引擎)
  ├── 依赖: constraint_graph.c (已有)
  ├── 依赖: symbolic_coord.c (已有)
  └── 被依赖: Module C (L2 传播矛盾证明)

Module B (等价类)
  ├── 依赖: normalization.c (已有)
  ├── 依赖: symbolic_coord.c (已有)
  └── 被依赖: Module C (完备性验证)

Module C (元证明)
  ├── 依赖: Module A (传播引擎)
  ├── 依赖: Module B (等价类)
  ├── 依赖: proof.c (已有)
  └── 依赖: solver.c (已有)
```

### 6.3 CMakeLists.txt 修改

```cmake
# 在 core/CMakeLists.txt 中添加
set(LV00_PROPAGATION_SOURCES
    src/core/propagation.c
)
set(LV00_EQUIV_CLASS_SOURCES
    src/core/equiv_class.c
)
set(LV00_META_PROOF_SOURCES
    src/core/meta_proof.c
)

# 头文件
set(LV00_PROPAGATION_HEADERS
    include/lv00/propagation.h
)
set(LV00_EQUIV_CLASS_HEADERS
    include/lv00/equiv_class.h
)
set(LV00_META_PROOF_HEADERS
    include/lv00/meta_proof.h
)
```

### 6.4 架构层级归属

```
Module A (传播引擎): 第 3 层（约束拓扑规约层）
  - 理由: 传播是约束图的拓扑操作，不涉及推理

Module B (等价类): 第 3 层（约束拓扑规约层）
  - 理由: 等价合并是归一化的推广，属于拓扑规约

Module C (元证明): 第 4 层（多策略自动推理层）
  - 理由: 剪枝合法性证明是推理行为
  - 特殊: 生成第 5 层的证明输出
```

---

## 7. 风险与缓解

| 风险 | 影响 | 缓解策略 |
|------|------|---------|
| 传播引擎性能（大规模图） | 求解变慢 | 增量传播 + 哈希预过滤 + 超时机制 |
| 代数共轭检测的计算复杂度 | 极小多项式比较开销大 | 先哈希预分组，组内精确比较 |
| 几何变换求解的数值稳定性 | 浮点误差导致误判 | 全程使用 SymbolicCoord 精确算术 |
| 元证明的完备性难以保证 | 部分剪枝无法证明 | 分级信任：GREEN(完备) / AMBER(部分) |
| 与现有代码的兼容性 | 回归错误 | 增量集成，每阶段完整测试 |

---

## 8. 成功标准

| 标准 | 度量方法 | 目标 |
|------|---------|------|
| 传播引擎正确性 | 单元测试通过率 | 100% (7/7) |
| 等价类正确性 | 单元测试通过率 | 100% (7/7) |
| 元证明正确性 | 单元测试通过率 | 100% (7/7) |
| 传播性能（100 节点） | 基准测试 | < 100ms |
| 等价类性能（100 节点） | 基准测试 | < 50ms |
| 元证明性能（100 节点） | 基准测试 | < 1s |
| 回归测试 | 现有测试套件 | 0 失败 |
| 幂等性 | 归一化幂等验证 | 通过 |
