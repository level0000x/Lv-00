# 多后端求解器体系 (Multi-Backend Solver Architecture)

## 模块概述

Lv-00 采用多后端求解策略，将几何约束的求解需求分发至不同的形式化引擎。在当前实现中，GROEBNER 后端为真实可用的求解实现（基于 Buchberger 算法与理想成员判定），其余后端（Z3、cvc5、Singular、ATP 等）为桩实现或框架定义，预留了完整的接口与编码管线，待后续按需链接外部求解库。

整体架构参考 polymake 的多后端设计哲学：通过统一的抽象接口屏蔽底层求解器差异，由引擎调度器（engine_scheduler.h）完成自动路由与分发。求解后端覆盖 SAT、SMT、ATP、BDD、模型计数等多个层次，形成从命题逻辑到一阶逻辑、从精确求解到近似计数的完整求解栈。

## 核心设计原则

1. **GROEBNER 为默认后端**：度数不超过 2 的多项式系统使用内置 Gröbner 基方法求解，无需外部依赖
2. **接口统一**：所有后端通过 `SMTSolver` 不透明句柄和工厂函数注册，上层代码与具体后端解耦
3. **增量求解**：支持 `add/solve/assume` 模式的增量约束构建，与 CaDiCaL 的增量接口对齐
4. **自动路由**：基于约束图特征向量（变量数、非线性度、量词存在性）自动选择最佳后端
5. **回退链**：首选后端不可用或求解失败时，自动尝试次选后端

## 数据类型定义

### 求解器后端类型枚举

```c
typedef enum {
    GROEBNER = 0, /**< Gröbner 基方法（度数<=2，内置实现） */
    SMT_Z3,       /**< Z3 SMT 求解器（Microsoft Research） */
    SMT_CVC5,     /**< cvc5 SMT 求解器（Stanford/Waterloo） */
    SMT_SINGULAR, /**< Singular 代数系统（含 SMT 接口） */
    COUNT         /**< 后端类型计数（用于数组大小） */
} SolverBackendType;
```

### ATP 后端类型枚举

```c
typedef enum {
    ATP_BACKEND_VAMPIRE = 0, /**< Vampire — superposition calculus，CASC 冠军 */
    ATP_BACKEND_EPROVER = 1, /**< E Prover — 高性能模块化 ATP */
    ATP_BACKEND_IPROVER = 2, /**< iProver — Inst-Gen，量词友好 */
    ATP_BACKEND_CUSTOM = 3,  /**< 自定义后端 */
    ATP_BACKEND_COUNT        /**< 后端总数 */
} ATPBackendType;
```

### BDD 变量类型枚举

```c
typedef enum {
    BDD_BOOLEAN = 0, /**< 布尔变量（0/1） */
    BDD_INT_BIT = 1, /**< 整数位变量（bit-blast 中的某一位） */
    BDD_ENUM = 2     /**< 枚举类型变量（多值编码为多位） */
} BDDVarType;
```

## 1. solver_core.h —— CDCL SAT 求解器核心

### 设计借鉴

借鉴 CaDiCaL（Armin Biere）的 CDCL SAT 求解器极简内核设计，约 15k 行 C++，聚焦核心正确性。核心特性包括冲突驱动子句学习、增量求解接口（add/assume/solve/get）、双监视文字快速单元传播。

### CDCL 状态机

CDCL 求解器内部维护 10 状态有限状态机，每个状态对应求解流程的一个阶段：

```c
typedef enum {
    CDCL_IDLE = 0,        /**< 空闲：等待 solve() 调用 */
    CDCL_PROPAGATING = 1, /**< 传播：执行单元传播（BCP） */
    CDCL_CONFLICT = 2,    /**< 冲突：检测到冲突子句 */
    CDCL_ANALYZING = 3,   /**< 分析：分析冲突生成学习子句 */
    CDCL_BACKJUMPING = 4, /**< 回跳：非时序回溯到决策层 */
    CDCL_LEARNING = 5,    /**< 学习：将学习子句加入子句库 */
    CDCL_DECIDING = 6,    /**< 决策：做出新的变量赋值决策 */
    CDCL_RESTARTING = 7,  /**< 重启：放弃当前决策栈重启搜索 */
    CDCL_SATISFIED = 8,   /**< 满足：所有变量已赋值且无冲突 */
    CDCL_UNSAT = 9        /**< 不可满足：推导出空子句 */
} CDCLState;
```

### CDCL 上下文

```c
typedef struct CDCLContext {
    CDCLState state;

    /* 变量赋值 */
    int *assigns;     /**< 赋值数组（0 = 未赋值） */
    int *levels;      /**< 每个变量的决策层级 */
    int *reasons;     /**< 每个赋值的归因子句索引 */
    int var_count;

    /* 决策栈 */
    int *trail;         /**< 赋值路径（文字序列） */
    int *trail_lim;     /**< 每个决策层级在 trail 中的起始位置 */
    int decision_level;

    /* 子句数据库 */
    int **clauses;
    int orig_clause_count;
    int learn_clause_count;

    /* 监视文字（双监视文字传播，桩实现） */
    int **watches;
    int *watch_sizes;

    /* 冲突分析 */
    int *conflict_clause;
    int backtrack_level;

    /* 统计 */
    int64_t propagations, conflicts, decisions, restarts;
} CDCLContext;
```

### 求解结果

```c
typedef enum {
    LV00_SOLVER_SAT = 10,   /**< 可满足 */
    LV00_SOLVER_UNSAT = 20, /**< 不可满足 */
    LV00_SOLVER_UNKNOWN = 0 /**< 未知（资源耗尽） */
} Lv00SolverResult;
```

### 核心操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建 | `lv00_solver_create()` | 创建求解器实例 |
| 添加约束 | `lv00_solver_add_constraint()` | 添加 CNF 子句 |
| 求解 | `lv00_solver_solve()` | 执行 CDCL 搜索 |
| 假设求解 | `lv00_solver_solve_under_assumptions()` | 在假设文字下求解 |
| 冲突查询 | `lv00_solver_failed_assumption()` | 查询导致 UNSAT 的假设 |
| 代数协同 | `lv00_solver_solve_algebraic()` | SAT 无解时回退 Gröbner 基 |
| 克隆 | `lv00_solver_clone()` | 导出求解器状态副本 |

### 双监视文字传播

双监视文字（two-watched-literals）是 CaDiCaL 的核心传播优化：每个子句仅监视两个文字，当其中一个文字被赋值为假时，检查另一个监视文字的状态。若已为真则子句已满足，否则搜索新的监视文字或触发单元传播。当前实现为桩实现，预留了完整的 `watches` 数据结构。

## 2. smt_backend.h —— SMT 后端抽象层

### 设计借鉴

参考 polymake 的多后端架构，统一 Z3、cvc5、Singular 等外部求解引擎的调用接口，通过 SMT-LIB2 标准格式实现与各后端的互操作。

### 编码管线

```
约束图 → SMT-LIB2 编码 → 后端原生表示 → 求解 → 结果解码 → SMTSolverResult
```

四步管线对应四个核心函数：

1. `smtencode_constraint_graph_to_smtlib2()` —— 约束图到 SMT-LIB2
2. `smtsolver_encode()` —— SMT-LIB2 到后端原生表示
3. `smtsolver_check()` —— 执行求解
4. `smtsolver_decode_result()` —— 解析结果

### SMT 逻辑理论

```c
typedef enum {
    SMT_LOGIC_QF_NRA = 0, /**< 无量词非线性实数算术 */
    SMT_LOGIC_QF_LRA,     /**< 无量词线性实数算术 */
    SMT_LOGIC_QF_NIA,     /**< 无量词非线性整数算术 */
    SMT_LOGIC_QF_LIA,     /**< 无量词线性整数算术 */
    SMT_LOGIC_QF_UFLRA,   /**< 未解释函数 + 线性实数算术 */
    SMT_LOGIC_QF_UFNRA,   /**< 未解释函数 + 非线性实数算术 */
    SMT_LOGIC_QF_BV,      /**< 无量词位向量 */
    SMT_LOGIC_AUTO        /**< 自动检测最合适的逻辑 */
} SMTLogic;
```

### GROEBNER 后端

GROEBNER 后端为 Lv-00 的默认和遗留后端，提供真实的求解能力：

- **Buchberger 算法**：计算多项式理想 Gröbner 基
- **理想成员判定**：判定给定多项式是否属于理想
- **适用范围**：度数不超过 2 的多项式系统（希尔伯特平面几何的尺规可构造片段）

### Z3/cvc5/Singular 后端

当前为桩实现。接口定义完整，包括工厂函数注册、SMT-LIB2 编码/解码、模型生成、UNSAT 核心提取等。Z3 和 cvc5 通过外部进程或动态库链接调用，Singular 通过其 SMT 接口桥接。

### 求解器配置

```c
typedef struct SMTSolverConfig {
    int64_t timeout_ms;       /**< 求解超时（毫秒） */
    int64_t memory_limit_mb;  /**< 内存上限（MB） */
    SMTLogic logic;           /**< 逻辑理论片段 */
    bool produce_models;      /**< 生成模型 */
    bool produce_unsat_cores; /**< 生成 UNSAT 核心 */
    bool produce_proofs;      /**< 生成证明对象 */
    bool incremental;         /**< 增量求解模式 */
    int random_seed;          /**< 随机种子 */
    void *custom_config;      /**< 后端特定扩展配置 */
} SMTSolverConfig;
```

### 后端注册表

```c
typedef struct SMTBackendEntry {
    SolverBackendType type;
    bool available;
    SMTSolverCreateFunc create_func;
    int priority;
    const char *description;
} SMTBackendEntry;

typedef struct SMTBackendRegistry {
    SMTBackendEntry entries[SMT_BACKEND_REGISTRY_CAPACITY]; // 16
    int count;
} SMTBackendRegistry;
```

## 3. smt_bitvector.h —— 固定位宽位向量算术

### 设计借鉴

借鉴 STP（层次化编码与字级缓存）和 Boolector（Lambert 变换）的位向量理论实现，提供 SMT-LIB QF_BV 理论所需的位级操作。

### 内部表示

基于 `uint64_t` 字数组的任意固定位宽位向量：

```c
typedef struct Lv00BitVector {
    uint64_t *words; /**< 64 位字数组 */
    int width;       /**< 总位宽（必须 > 0） */
} Lv00BitVector;
```

位布局：`words[0]` 持有 bits [63:0]，`words[1]` 持有 bits [127:64]，以此类推。最高有效字的超出 `width` 部分恒为零。

### 核心操作

| 类别 | 操作 | 函数 |
|------|------|------|
| 生命周期 | 创建/销毁/从整数构造 | `bv_create()`, `bv_destroy()`, `bv_from_int()` |
| 位运算 | NOT/AND/OR/XOR | `bv_not()`, `bv_and()`, `bv_or()`, `bv_xor()` |
| 移位 | 逻辑左移/右移 | `bv_shift_left()`, `bv_shift_right()` |
| 提取/拼接 | 位段提取/拼接 | `bv_extract()`, `bv_concat()` |
| 算术 | 模加/模乘/取反 | `bv_add()`, `bv_mul()`, `bv_neg()` |
| 比较 | 无符号等/无符号小于/有符号小于 | `bv_eq()`, `bv_ult()`, `bv_slt()` |

所有算术运算在模 2^width 下执行（wrapping 语义），位向量以无符号二进制补码解释，除非显式标记为有符号比较。

## 4. smt_theory_combiner.h —— SMT 理论组合调度器

### 设计借鉴

借鉴 Alt-Ergo 的 CDCL(T) 理论组合架构和 Yices2 的 EF-solving 方法，提供基于优先级的串行理论分发机制。

### 支持的理论

```c
typedef enum {
    THEORY_QF_LIA = 0,    /**< 线性整数算术 */
    THEORY_QF_LRA = 1,    /**< 线性实数算术 */
    THEORY_QF_BV = 2,     /**< 位向量 */
    THEORY_QF_AUFNIA = 3, /**< 数组+未解释函数+非线性整数算术 */
    THEORY_QF_AX = 4,     /**< 数组+外延性 */
    THEORY_COUNT = 5
} Lv00TheoryId;
```

### 组合策略

理论组合器维护一个按优先级排序的理论求解器列表，对给定的约束集依次尝试每个启用的理论，直到获得确定性结果（SAT/UNSAT）：

```c
typedef struct Lv00TheoryCombiner {
    Lv00TheoryEntry *entries;
    int entry_count;
    double timeout_ms;  /**< 每个理论的超时时间 */
} Lv00TheoryCombiner;
```

分发规则：优先级数值越低越先尝试。若所有理论均超时，返回最后一个结果并标记 `timeout=true`。

## 5. smt_trigger_engine.h —— 量词实例化引擎

### 设计借鉴

借鉴 Yices2 的多模式触发器、Z3 的 E-matching 子项共享与缓存、CVC5 的触发器选择启发式与实例化限制。

### E-matching 机制

当量化公式包含模式（触发器）时，引擎监视基项（ground term），在匹配到合适的替换时实例化量化公式。

### 触发器结构

```c
typedef struct Lv00Trigger {
    int pattern_ids[LV00_TRIGGER_MAX_PATTERNS]; /**< 子模式 ID 数组 */
    int pattern_size;
    double weight;  /**< 选择权重（越低越优先） */
} Lv00Trigger;
```

### 实例缓存

```c
typedef struct Lv00InstanceEntry {
    int quantifier_id;    /**< 量化公式 ID */
    uint64_t binding_hash; /**< 替换的哈希值 */
} Lv00InstanceEntry;
```

实例缓存通过 `(quantifier_id, binding_hash)` 二元组去重，避免重复实例化。

### 核心操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 添加模式 | `trigger_engine_add_pattern()` | 注册触发器模式 |
| 查找匹配 | `trigger_engine_find_matches()` | 对给定基项扫描所有触发器 |
| 清除缓存 | `trigger_engine_clear_cache()` | 允许重新考虑已生成的实例 |
| 统计 | `trigger_engine_get_instantiation_count()` | 获取总实例化次数 |

## 6. atp_backend.h —— 一阶逻辑 ATP 后端

### 设计借鉴

借鉴 Vampire（superposition calculus + 策略调度）、E Prover（子句评估启发式）、iProver（Inst-Gen 量词友好实例化）的 FOL ATP 架构。ATP 后端与 SMT 后端互补：SMT 处理算术/非线性约束，ATP 处理纯逻辑推导和一阶量词推理。

### 输入格式

```c
typedef enum {
    ATP_FORMAT_TPTP_FOF = 0, /**< TPTP FOF（一阶公式） */
    ATP_FORMAT_TPTP_CNF = 1, /**< TPTP CNF（子句范式） */
    ATP_FORMAT_TPTP_TFF = 2, /**< TPTP TFF（带类型的一阶公式） */
    ATP_FORMAT_SMTLIB2 = 3   /**< SMT-LIB2 */
} ATPInputFormat;
```

### 约束图到 TPTP 编码

编码映射规则：

| 约束图元素 | TPTP 表示 |
|-----------|-----------|
| 节点 | 常量符号（point_node_id, line_node_id, ...） |
| GeomType | 一元谓词（is_point(X), is_line_segment(X), ...） |
| 约束 | 二元/三元谓词（incident(P, L), between(A, B, C), ...） |
| 公理 | TPTP axiom 子句 |

### ATP 后端注册表

与 SMT 后端注册表模式一致，通过工厂函数和优先级注册：

```c
typedef struct {
    ATPBackendType type;
    bool available;
    ATPBackendCreateFunc create;
    int priority;
    const char *description;
} ATPBackendEntry;
```

### 自动路由决策

```
1. 含量化公式 + Vampire/E 可用 → ATP 优先
2. 纯逻辑约束（无非线性算术）→ ATP 优先于 SMT
3. 含非线性算术 → SMT 优先
4. 混合约束 → 同时尝试 ATP 和 SMT，返回最先成功的结果
```

## 7. bdd_encoding.h —— BDD/ADD 编码

### 设计借鉴

借鉴 CUDD（Colorado University Decision Diagram）的 BDD 库架构，提供约束图的布尔化编码和符号化操作。

### BDD 节点

```c
typedef struct BDDNode {
    int var_id;           /**< 决策变量 ID（终端节点为 -1） */
    struct BDDNode *low;  /**< 变量=0 时的子图 */
    struct BDDNode *high; /**< 变量=1 时的子图 */
    uint64_t ref_count;   /**< 引用计数 */
    bool complemented;    /**< 补边（CUDD 风格） */
} BDDNode;
```

### BDD 管理器

```c
typedef struct BDDManager {
    BDDNode *true_node, *false_node;  /**< 终端节点 */
    BDDNode **unique_table;           /**< 唯一表（哈希去重） */
    int *var_order;                   /**< 变量序数组 */
    int var_count;
    uint64_t node_count;
} BDDManager;
```

### 布尔运算

所有运算通过递归 ITE（If-Then-Else）算法实现：

```
ite(F, G, H) = (F ∧ G) ∨ (¬F ∧ H)
```

支持的操作：`bdd_and()`, `bdd_or()`, `bdd_not()`, `bdd_ite()`, `bdd_xor()`, `bdd_nand()`。

### ADD 代数运算

ADD（Algebraic Decision Diagram）叶子为实数，支持：`add_add()`, `add_sub()`, `add_mul()`, `add_div()`, `add_max()`, `add_min()`。

### 变量序优化

使用 Sifting 算法重排 BDD 变量序：遍历每个变量，将其移动到使 BDD 节点数最少的位置。

### 应用场景

- 约束图到 BDD 编码 → 符号化模型计数
- BDD 到 CNF 转换（Tseitin 变换）→ 供 SAT 求解器使用
- 坐标 bit-blasting（IEEE 754 位表示）

当前实现为桩实现，预留了完整的接口。

## 8. sat_encoding.h —— SAT 编码管线

### 设计借鉴

借鉴 Alloy Kodkod 的关系逻辑到 SAT 编码管道：关系公式 → 布尔约束 → CNF 子句的三层翻译管线，基于 MiniSat 的高效增量求解。

### 编码映射

将几何约束图的关系视图映射到 SAT 变量空间：

- 每个 (节点_i, 节点_j) 对映射为布尔变量
- 几何约束编码为 CNF 子句

### 变量映射

```c
typedef struct SatVarEntry {
    int var_id;      /**< SAT 变量 ID（>= 1） */
    int arity;       /**< 元组的元数 */
    int atom_ids[8]; /**< 元组中的原子 ID */
} SatVarEntry;
```

### 约束编码规则

| 约束类型 | 编码函数 | 编码规则 |
|----------|----------|----------|
| 共线性 | `sat_encode_collinearity()` | 方向向量成比例 |
| 平行性 | `sat_encode_parallelism()` | 方向向量叉积为零 |
| 垂直性 | `sat_encode_perpendicularity()` | 方向向量点积为零 |
| 距离相等 | `sat_encode_distance_eq()` | 距离公式等式 |
| 角度相等 | `sat_encode_angle_eq()` | 角度公式等式 |
| 包含关系 | `sat_encode_containment()` | 点在区域内 |

### DIMACS 导出

```c
bool sat_encoding_export_dimacs(const SatEncoding *enc, const char *filepath);
```

将编码结果导出为标准 DIMACS CNF 格式，供外部 SAT 求解器验证或调试。

## 9. approx_counter.h —— 近似模型计数

### 设计借鉴

借鉴 ApproxMC（基于 XOR 哈希的近似 #SAT 求解器）和 UniGen（近似均匀采样器），提供带 PAC（Probably Approximately Correct）保证的约束图模型计数能力。

### PAC 保证

计数结果满足：

```
Pr[|total_count - true_count| <= epsilon * true_count] >= 1 - delta
```

### PAC 配置

```c
typedef struct {
    double epsilon;    /**< 相对误差（如 0.1 表示 +/-10%） */
    double delta;      /**< 置信度下界（如 0.99 表示 99%） */
    int seed;          /**< 随机种子 */
    bool sparse_xor;   /**< 稀疏 XOR 哈希 */
    int num_hashes;    /**< 哈希函数数量（0 = 自动选择） */
} PacConfig;
```

### 计数公式

```
total_count = cell_sol_count * 2^hash_count
```

### 核心操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 模型计数 | `approx_count_solutions()` | PAC 保证的 #SAT |
| 投影计数 | `approx_count_projected()` | 仅统计指定变量的不同赋值 |
| 构造性判断 | `is_approximately_constructible()` | total_count > 0 则近似可构造 |
| CNF 编码 | `approx_count_to_sat()` | 约束图到 DIMACS CNF |

当前实现为桩实现，预留了完整的 PAC 框架接口。

## 10. engine_scheduler.h —— 多引擎调度框架

### 设计借鉴

参考 polymake 的多后端架构，实现后端注册、自动路由与分发求解的中央调度层。调度器本身不实现求解逻辑，纯粹是路由和编排层。

### 常量定义

```c
#define SCHEDULER_MAX_ROUTING_RULES 32      /**< 最大路由规则数 */
#define SCHEDULER_MAX_BACKEND_INSTANCES 8   /**< 最大后端实例数 */
#define SCHEDULER_MAX_FALLBACK_DEPTH 4      /**< 回退链最大深度 */
```

### 图特征分析

```c
typedef struct GraphFeatures {
    int total_nodes, total_constraints;
    int variable_nodes, fixed_nodes;
    int nonlinear_constraints;
    double nonlinear_ratio;
    bool has_quantifier_like;
    bool has_boolean_variables;
    int estimated_degree_max;
    int64_t analysis_time_us;
} GraphFeatures;
```

分析复杂度 O(N + C)（N = 节点数，C = 约束数），为轻量级操作。

### 路由规则

```c
typedef struct RoutingRule {
    char name[64];
    int priority;                     /**< 数值越低越优先 */
    bool enabled;
    RouteCondition conditions[4];
    int condition_count;
    RouteCombineMode combine_mode;    /**< AND / OR */
    SolverBackendType target_backend;
} RoutingRule;
```

### 预置标准路由规则

| 规则名 | 条件 | 目标后端 | 优先级 |
|--------|------|----------|--------|
| quantifier-cvc5 | 含类量词约束 + cvc5 可用 | SMT_CVC5 | 0 |
| nonlinear-smt | 非线性占比 >= 0.3 + 有 SMT 可用 | 最佳 SMT | 10 |
| small-groebner | 变量数 < 50 | GROEBNER | 20 |
| large-smt | 变量数 >= 50 + 非线性 > 0 | 最佳 SMT | 30 |
| default-groebner | 无条件 | GROEBNER | 100 |

### 调度器工作流

```
1. scheduler_analyze_graph()     —— 提取图特征
2. scheduler_select_backend()    —— 基于特征自动选后端
3. scheduler_solve()             —— 创建求解器并求解
4. 结果转换                       —— SMTSolverResult → GroebnerResult
```

### 回退策略

当首选后端不可用或求解失败时，按回退链依次尝试。默认回退链为 `[GROEBNER]`（深度 1），可通过 `scheduler_set_fallback_policy()` 自定义。

### 结果向后兼容

```c
GroebnerResult *scheduler_convert_smt_to_groebner(
    const SMTSolverResult *smt_result,
    const ConstraintGraph *graph
);
```

将 SMT 求解结果转换为 Gröbner 兼容格式，支持从 Gröbner 到 SMT 后端的渐进式迁移。

## 实现文件

- **头文件**：`include/lv00/solver_core.h`, `include/lv00/smt_backend.h`, `include/lv00/smt_bitvector.h`, `include/lv00/smt_theory_combiner.h`, `include/lv00/smt_trigger_engine.h`, `include/lv00/atp_backend.h`, `include/lv00/bdd_encoding.h`, `include/lv00/sat_encoding.h`, `include/lv00/approx_counter.h`, `include/lv00/engine_scheduler.h`
- **源文件**：`src/layer4_reasoning/` 和 `src/layer3_geometry/` 下的对应 .c 文件

## 依赖

- GMP 库（solver_core.h 中的 Gröbner 基计算）
- 可选外部求解器：Z3, cvc5, Singular, Vampire, E Prover, iProver
- 可选：CUDD（BDD 编码）、ApproxMC（近似计数）

## 测试要点

1. CDCL 状态机各状态转换的正确性
2. SMT-LIB2 编码的约束图覆盖度
3. 位向量算术的边界条件（溢出、符号扩展）
4. 理论组合器的优先级分发正确性
5. E-matching 触发器的模式匹配与缓存去重
6. TPTP 编码的几何约束映射完整性
7. BDD 变量序优化前后的节点数对比
8. SAT 编码的 DIMACS 导出可被外部求解器验证
9. PAC 计数的置信度与误差界验证
10. 调度器路由规则的匹配与回退行为
