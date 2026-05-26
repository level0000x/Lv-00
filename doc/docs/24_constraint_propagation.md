# 24. 约束传播与等价类系统

## 24.1 模块概述

本文档描述 Lv-00 几何元语言系统的约束传播引擎、等价类管理和图哈希机制。这些模块构成了约束求解的核心，实现了从局部约束到全局推理的自动化过程。

**覆盖头文件**：
- `propagation.h` —— WFC 风格约束传播引擎
- `equiv_class.h` —— 等价类管理器
- `graph_hash.h` —— 图结构哈希
- `probabilistic_constraint.h` —— 概率约束与 PCTL 评估

---

## 24.2 propagation.h —— 约束传播引擎

### 24.2.1 设计定位

约束传播引擎采用 **WFC（Wave Function Collapse）** 范式，将"局部规则排除错误路径"的思想注入 Lv-00 约束求解：
- **AC-3 弧相容性**：经典的约束传播算法
- **熵最小化节点选择**：WFC 风格的状态空间坍缩
- **快照/回溯机制**：死路恢复与搜索空间探索

### 24.2.2 数学基础

约束系统形式化定义：
```
S = (V, C, Σ, δ)

其中：
- V: 变量（节点）集合
- C: 约束集合
- Σ: 状态空间（每个变量的可能取值）
- δ: C × Σ → P(Σ) 传播函数

全局约束：Σ_global = ∩_{c∈C} δ(c, Σ)
合法解：σ* ∈ Σ_global
```

### 24.2.3 核心数据结构

#### 节点状态空间（NodeStateSpace）

```c
typedef struct NodeStateSpace {
    int node_id;                       // 关联节点 ID
    SymbolicCoord **possible_coords;   // 可能坐标列表
    int *coord_dims;                   // 每个候选坐标的维度
    int coord_count;                   // 候选坐标数量
    int capacity;                      // 预分配容量
    bool is_collapsed;                 // 是否已坍缩
    SymbolicCoord *collapsed_value;    // 坍缩后的唯一值
    bool is_unbounded;                 // 是否为无界自由变量
} NodeStateSpace;
```

#### 传播上下文（PropagationContext）

```c
typedef struct PropagationContext {
    ConstraintGraph *graph;           // 关联约束图
    NodeStateSpace *state_spaces;     // 每个节点的状态空间
    int state_count;
    
    PropagationStrategy strategy;     // 节点选择策略
    CollapseStrategy collapse_strategy; // 坍缩策略
    int max_iterations;               // 最大传播轮次
    int max_backtracks;               // 最大回溯次数
    
    // 传播队列（环形缓冲区）
    int *propagation_queue;
    int queue_head, queue_tail;
    int queue_capacity, queue_size;
    
    // 回溯栈
    PropagationSnapshot **snapshot_stack;
    int snapshot_count, snapshot_capacity;
    
    // 统计
    int64_t propagation_steps;        // 传播步数
    int64_t collapse_count;           // 坍缩次数
    int64_t backtrack_count;          // 回溯次数
    int64_t prune_count;              // 剪枝次数
} PropagationContext;
```

### 24.2.4 传播策略

| 策略 | 枚举值 | 说明 |
|------|--------|------|
| 熵最小化 | `PROP_STRATEGY_MIN_ENTROPY` | WFC: 选择状态空间最小的节点 |
| 最少剩余值 | `PROP_STRATEGY_MRVS` | MRV: 最少剩余值启发式 |
| 度数优先 | `PROP_STRATEGY_DEGREE` | 选择度数（邻接约束数）最高的节点 |
| BFS | `PROP_STRATEGY_BFS` | 从最近修改处广度优先传播 |
| 拓扑序 | `PROP_STRATEGY_TOPOLOGICAL` | 按依赖关系顺序选择 |

### 24.2.5 传播结果

```c
typedef enum {
    PROP_RESULT_CONSISTENT,       // 相容，可继续传播
    PROP_RESULT_CONTRADICTION,    // 矛盾，某节点状态空间为空
    PROP_RESULT_SATISFIED,        // 所有节点已坍缩为唯一值
    PROP_RESULT_STABLE,           // 传播收敛，但仍有未确定节点
    PROP_RESULT_TIMEOUT           // 超时
} PropagationResult;
```

### 24.2.6 核心 API

#### 生命周期管理

```c
PropagationContext *propagation_context_create(ConstraintGraph *graph);
void propagation_context_destroy(PropagationContext *ctx);
```

#### 状态空间初始化

```c
PropagationResult propagation_init_state_spaces(PropagationContext *ctx);
NodeStateSpace *propagation_get_state_space(PropagationContext *ctx, int node_id);
```

**初始化规则**：
- 已有精确坐标的节点 → 状态空间 = {当前坐标}（已坍缩）
- 有邻接约束但无坐标的节点 → 从约束推导候选集
- 完全自由的节点 → 标记为 unbounded

#### AC-3 约束传播

```c
bool propagation_arc_reduce(PropagationContext *ctx, int constraint_id);
PropagationResult propagation_run(PropagationContext *ctx);
```

**AC-3 算法流程**：
1. 将所有约束加入工作队列
2. 取出约束，执行弧相容性检查
3. 若某参与者状态空间收缩，将其所有邻接约束重新入队
4. 若某参与者状态空间变空 → CONTRADICTION
5. 队列为空 → 返回当前状态

**弧相容性数学定义**：
```
对于约束 c = (A, B, type)：
移除所有 a ∈ Σ(A) 使得 ¬∃ b ∈ Σ(B) : δ(c, (a, b)) 成立
```

#### WFC 节点选择与坍缩

```c
int propagation_select_node(PropagationContext *ctx);
double propagation_compute_entropy(const NodeStateSpace *state);
bool propagation_collapse(PropagationContext *ctx, int node_id);
```

**熵计算**：
```
H(v) = log₂|Σ(v)|

其中：
- |Σ(v)| 是节点 v 的状态空间大小
- unbounded 节点返回 PROP_ENTROPY_UNBOUNDED (-1.0)
```

**节点选择策略**：
1. 计算每个未坍缩节点的熵 H(v)
2. 选择 H(v) 最小的节点（状态空间最小）
3. 若有多个节点熵相同，选择度数最高的（打破对称性）

#### 完整 WFC 求解循环

```c
PropagationResult propagation_wfc_solve(PropagationContext *ctx);
```

**算法流程**：
```
loop:
    1. propagation_run()           // AC-3 约束传播
    2. if CONTRADICTION → backtrack or restart
    3. if SATISFIED → return SUCCESS
    4. if STABLE → select_node() + collapse() → goto 1
```

#### 快照与回溯

```c
PropagationSnapshot *propagation_snapshot_save(PropagationContext *ctx);
void propagation_snapshot_restore(PropagationContext *ctx, PropagationSnapshot *snap);
void propagation_snapshot_destroy(PropagationSnapshot *snap);
```

**快照内容**：
- 所有节点状态空间的深拷贝
- 传播步数、坍缩次数、回溯次数、剪枝次数
- 本次决策的节点 ID 和选择的坐标索引

### 24.2.7 配置常量

| 常量 | 默认值 | 说明 |
|------|--------|------|
| `PROP_DEFAULT_MAX_ITERATIONS` | 10000 | 默认最大迭代轮次 |
| `PROP_DEFAULT_MAX_BACKTRACKS` | 1000 | 默认最大回溯次数 |
| `PROP_DEFAULT_QUEUE_CAPACITY` | 256 | 传播队列默认容量 |
| `PROP_DEFAULT_SNAPSHOT_CAPACITY` | 64 | 快照栈默认容量 |
| `PROP_DEFAULT_STATE_CAPACITY` | 8 | 节点状态空间默认初始容量 |
| `PROP_ENTROPY_UNBOUNDED` | -1.0 | 无穷大熵标记 |
| `PROP_WFC_MAX_COLLABORATION_ITERATIONS` | 10000 | WFC 循环最大协作迭代次数 |

---

## 24.3 equiv_class.h —— 等价类管理器

### 24.3.1 设计定位

等价类管理器将 Lv-00 的"坐标判等 → 并查集合并"推广为更一般的**代数等价关系**，支持：
- 约束推导等价
- 代数共轭等价
- 几何变换等价
- 每步合并生成可追溯的证明

### 24.3.2 数学基础

**等价关系** ~ 满足：
- **自反性**：∀v ∈ V, v ~ v
- **对称性**：v₁ ~ v₂ → v₂ ~ v₁
- **传递性**：v₁ ~ v₂ ∧ v₂ ~ v₃ → v₁ ~ v₃

**商集**：V/~ = {[v] : v ∈ V}

**关键定理**：若 S 相容，则 S/~ 也相容

### 24.3.3 等价来源分类

| 来源类型 | 枚举值 | 说明 |
|----------|--------|------|
| 坐标精确相等 | `EQUIV_SOURCE_COORD_EQUAL` | 坐标数值相等 |
| 约束链推导 | `EQUIV_SOURCE_CONSTRAINT_DERIVE` | 从约束推导的隐式等价 |
| 代数共轭 | `EQUIV_SOURCE_ALGEBRAIC_CONJ` | 同一极小多项式的不同实根 |
| 几何变换 | `EQUIV_SOURCE_GEOM_TRANSFORM` | 合同变换下的等价 |
| 语义模式匹配 | `EQUIV_SOURCE_SEMANTIC_PATTERN` | 语义层面的等价识别 |

### 24.3.4 核心数据结构

#### 等价证明（EquivProof）

```c
typedef struct EquivProof {
    EquivSourceType source;         // 等价来源类型
    int node_a_id;                  // 节点 A ID
    int node_b_id;                  // 节点 B ID
    int deriving_constraint_id;     // 推导来源约束 ID
    int proof_step_id;              // 证明步骤引用
    TrustColor trust;               // 信任颜色
} EquivProof;
```

#### 等价类（EquivClass）

```c
typedef struct EquivClass {
    int representative_id;          // 代表节点 ID（最小 ID）
    int *member_ids;                // 成员节点 ID 数组
    int member_count;               // 成员数量
    int capacity;                   // 预分配容量
    
    EquivProof *proofs;             // 等价证明链
    int proof_count, proof_capacity;
    
    TrustColor min_trust;           // 类内最低信任颜色
} EquivClass;
```

#### 等价类管理器（EquivClassManager）

```c
typedef struct EquivClassManager {
    ConstraintGraph *graph;         // 关联约束图
    
    EquivClass *classes;            // 等价类数组
    int class_count, class_capacity;
    
    int *node_to_class;             // node_id → class_index 映射
    int node_to_class_capacity;
    
    // 并查集（底层实现）
    int *uf_parent;                 // 父节点数组
    int *uf_rank;                   // 秩数组
    int uf_capacity;
    
    // 等价证明日志
    EquivProof *proof_log;
    int proof_log_count, proof_log_capacity;
    
    // 统计
    int64_t total_merges;
    int64_t coord_merges;
    int64_t constraint_derives;
    int64_t algebraic_conjugates;
    int64_t transform_merges;
    int64_t rejected_merges;
} EquivClassManager;
```

### 24.3.5 等价合并策略

#### 坐标等价合并

```c
int equiv_merge_by_coord(EquivClassManager *mgr);
```

封装现有的坐标判等逻辑，为每对坐标相等的节点生成 `EQUIV_SOURCE_COORD_EQUAL` 证明。

#### 约束推导等价

```c
int equiv_derive_from_constraints(EquivClassManager *mgr);
```

**推导规则**：

**规则 1 - 中点等价**：
```
若 B 是 A,C 的中点，D 是 E,F 的中点，且 AC ≅ EF，则 B ~ D
```

**规则 2 - 对称等价**：
```
若 l 是 A,B 的对称轴，C 关于 l 的对称点为 D，则 C ~ D
```

**规则 3 - 交点等价**：
```
若 l₁ ∩ l₂ = {P}，l₁' ∩ l₂' = {P'}，且 l₁ ~ l₁'，l₂ ~ l₂'，则 P ~ P'
```

#### 代数共轭等价

```c
int equiv_merge_algebraic_conjugates(EquivClassManager *mgr);
```

**算法流程**：
1. 收集所有 ALGEBRAIC 类型坐标
2. 按极小多项式哈希分组
3. 组内比较极小多项式系数（精确 GMP 比较）
4. 验证交换共轭对后约束仍然成立

**数学原理**：若两个节点的坐标是同一极小多项式的不同实根，则它们在代数意义上"等价"（可互换而不破坏约束）。

#### 几何变换等价

```c
int equiv_merge_by_transform(EquivClassManager *mgr);
```

检测合同变换下的等价：若存在旋转/平移/反射/组合变换 T 使得 T(A) = B，且 T 保持所有邻域约束，则 A ~ B。

#### 全部合并

```c
int equiv_merge_all(EquivClassManager *mgr);
```

按顺序执行：
1. `equiv_merge_by_coord`
2. `equiv_derive_from_constraints`
3. `equiv_merge_algebraic_conjugates`
4. `equiv_merge_by_transform`

### 24.3.6 查询接口

```c
const EquivClass *equiv_get_class(const EquivClassManager *mgr, int node_id);
int equiv_find(const EquivClassManager *mgr, int node_id);
bool equiv_are_equivalent(const EquivClassManager *mgr, int node_a, int node_b);
int equiv_class_count(const EquivClassManager *mgr);
```

### 24.3.7 合法性证明

```c
bool equiv_prove_merge_valid(EquivClassManager *mgr, int class_a_idx, int class_b_idx);
```

验证合并两个等价类后约束系统仍然相容。

---

## 24.4 graph_hash.h —— 图结构哈希

### 24.4.1 设计定位

基于 **FNV-1a** 的约束图结构指纹，提供：
- 紧凑、与节点顺序无关的结构哈希
- 快速比较图的拓扑等价性
- 重写循环检测

### 24.4.2 核心数据结构

```c
typedef struct GraphHash {
    uint64_t hash;         // 整个图的聚合 FNV-1a 哈希值
    uint64_t *node_hashes; // 每个节点的哈希值
    int node_count;        // 图中节点数量
} GraphHash;
```

### 24.4.3 核心 API

```c
GraphHash *compute_complete_graph_hash(const ConstraintGraph *graph);
bool graph_hash_equal(const GraphHash *a, const GraphHash *b);
void graph_hash_destroy(GraphHash *hash);
```

### 24.4.4 应用场景

| 场景 | 用途 |
|------|------|
| 重写循环检测 | 检测图结构是否进入之前的状态 |
| 规范化终止判断 | 判断归一化是否达到不动点 |
| 缓存键 | 基于图结构缓存求解结果 |
| 等价性快速判断 | 先比较哈希，再详细比较 |

---

## 24.5 probabilistic_constraint.h —— 概率约束

### 24.5.1 设计定位

借鉴 **PRISM** 概率模型检测框架，为 Lv-00 提供：
- 概率分布约束
- PCTL（概率计算树逻辑）公式评估
- 概率推理能力

### 24.5.2 概率分布类型

```c
typedef enum {
    PROB_DIST_UNIFORM,   // 均匀分布 U(a, b)
    PROB_DIST_NORMAL,    // 正态分布 N(μ, σ²)
    PROB_DIST_DISCRETE,  // 离散分布
    PROB_DIST_BETA,      // Beta 分布 Beta(α, β)
    PROB_DIST_CUSTOM     // 自定义分布
} ProbDistType;
```

### 24.5.3 概率约束节点

```c
typedef struct {
    int base_node_id;              // 基础节点 ID
    ProbDistribution *coord_dist;  // 坐标概率分布
    bool is_soft;                  // 是否为软约束
    double probability;            // 约束成立概率 [0,1]
    char *pctl_formula;            // PCTL 公式字符串
} ProbConstraintNode;
```

### 24.5.4 PCTL 公式

```c
typedef enum {
    PCTL_PROB_BOUND,      // P~p [ φ ] — 概率边界
    PCTL_NEXT,            // X φ — 下一状态满足 φ
    PCTL_UNTIL,           // φ U ψ — φ 一直成立直到 ψ
    PCTL_EVENTUALLY,      // F φ — 最终满足 φ
    PCTL_ALWAYS,          // G φ — 总是满足 φ
    PCTL_STEADY_STATE     // S~p [ φ ] — 稳态概率
} PCTLFormulaType;
```

### 24.5.5 核心 API

#### 概率分布操作

```c
ProbDistribution *prob_dist_create(ProbDistType type, double *params, int param_count);
void prob_dist_destroy(ProbDistribution *dist);
double prob_dist_pdf(ProbDistribution *dist, double x);
double prob_dist_cdf(ProbDistribution *dist, double x);
int prob_dist_sample(ProbDistribution *dist, int n_samples, double **out_samples);
```

#### PCTL 评估

```c
bool pctl_evaluate(const ConstraintGraph *graph, const PCTLFormula *formula, 
                   double *out_probability);
bool pctl_check_constructibility(const ConstraintGraph *graph, double confidence);
```

**构造性检查算法**：
1. 对概率分布进行 Monte Carlo 采样（默认 N=1000 次）
2. 统计满足约束的有效构造比例
3. 返回 proportion ≥ confidence

#### 概率推理

```c
bool prob_constraint_infer(const ConstraintGraph *graph, int target_var,
                           ProbConstraintNode **constraints, int n,
                           double *out_conf);
```

使用贝叶斯网络风格的信念传播推断目标变量的置信度。

---

## 24.6 代码-理论对应关系

| 代码概念 | 理论对应 | 文档位置 |
|----------|----------|----------|
| `PropagationContext` | 约束系统 S = (V, C, Σ, δ) | 本文档 24.2.3 |
| `propagation_run()` | AC-3 弧相容性算法 | 本文档 24.2.6 |
| `propagation_compute_entropy()` | 信息熵 H(v) = log₂\|Σ(v)\| | 本文档 24.2.6 |
| `EquivClass` | 等价类 [v] ∈ V/~ | 本文档 24.3.4 |
| `uf_parent` / `uf_rank` | 并查集（Union-Find） | 本文档 24.3.4 |
| `GraphHash` | 图同构不变量 | 本文档 24.4 |
| `PCTLFormula` | 概率计算树逻辑 | 本文档 24.5.4 |
| `prob_constraint_infer()` | 贝叶斯信念传播 | 本文档 24.5.5 |

---

## 24.7 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图核心数据结构 |
| [03_normalization.md](03_normalization.md) | 图规范化遍引擎 |
| [04_solver.md](04_solver.md) | 符号代数求解器 |
| [11_wfc_paradigm.md](11_wfc_paradigm.md) | WFC 范式设计原理 |
| [01_symbolic_coord.md](01_symbolic_coord.md) | 符号坐标系统 |

---

## 24.8 版本历史

- **v3.5.0** (当前)
  - 概率约束与 PCTL 评估
  - 代数共轭等价检测
  - 几何变换等价检测

- **v3.4.0**
  - WFC 风格约束传播引擎
  - 快照/回溯机制
  - 熵最小化节点选择

- **v3.3.0**
  - 等价类管理器
  - 并查集实现
  - 图结构哈希（FNV-1a）
