# 06. 合一检查（Unification Check）

## 模块概述

合一检查模块负责验证"构造图是否满足命题图定义的结构模式"，是 Lv-00 将几何命题判定为证明/反例的判定环节。它提供三种递进强度的合一算法（基础版、坐标增强版、哈希预过滤版），并提供端口、约束、坐标三个维度的精细化匹配函数与详细失败报告。

- **端口匹配**：命题图端口到构造图端口的类型、命名空间深度、TypeRegion 匹配；
- **约束匹配**：约束类型、参与者数量与参与者 ID 的逐约束核对；
- **坐标判等**：符号坐标的结构化相等判断（`symbolic_coord_compare` 语义），支持以哈希分组加速；
- **配套能力**：命题等价声明（存储为双向重写规则）、多态命题实例化、简化命题/证明快速通道。

**覆盖头文件**：
- `unify.h` —— 合一检查核心：状态枚举、精细匹配、失败报告、等价声明、实例化
- `lambda_unify.h` —— λ-项句法合一（Martelli-Montanari）与 Miller 模式合一
- `prop_formula_ops.h` —— 命题公式 VTable（equal / hash / is_descendant）
- `expr_canon.h` / `expr_canonical.h` —— 符号表达式规范表示（坐标判等支撑）

## 核心设计原则

1. **由粗到细的分层判定**：先结构（端口）后内容（约束）再数值（坐标），任一阶段失败即返回对应 `UnifyStatus`，避免无效深层比较。
2. **失败可定位**：`MismatchReason` 枚举 + `UnifyFailureInfo`（含 `failed_node_id`/`failed_constraint_id`/`failed_port_index`）精确报告不匹配位置与原因。
3. **坐标判等收敛语义**：所有坐标判等收敛到 `symbolic_coord_compare`，NULL 安全、与 `symbolic_coord_hash` 保持一致（hash 相等是判等前提）。
4. **哈希预过滤加速**：以 `symbolic_coord_hash()` 对节点分组，只比较同组节点，将匹配复杂度从全对全降低到组内比较。
5. **等价声明透明化**：命题等价作为双向重写规则存储，合一前自动应用，对调用方无感。
6. **多态支持**：类型变量经 `unify_instantiate_proposition` 实例化为具体 `TypeRegion` 后再合一，实现命题模板复用。

## 关键数据结构

### 状态与失败报告

```c
typedef enum {
    UNIFY_STATUS_OK,                  /* 合一成功 */
    UNIFY_STATUS_PORT_TYPE_MISMATCH,  /* 端口类型不匹配 */
    UNIFY_STATUS_CONSTRAINT_MISMATCH, /* 约束不匹配 */
    UNIFY_STATUS_COORD_MISMATCH,      /* 坐标不匹配 */
    UNIFY_STATUS_STRUCTURE_MISMATCH,  /* 结构不匹配 */
    UNIFY_STATUS_SCOPE_MISMATCH,      /* 作用域不匹配 */
    UNIFY_STATUS_FAILED               /* 合一失败 */
} UnifyStatus;

typedef enum {
    PORT_TYPE_MISMATCH,                    /* 端口类型不匹配 */
    PORT_NAMESPACE_MISMATCH,               /* 端口命名空间深度不匹配 */
    PORT_TYPE_REGION_MISMATCH,             /* 端口类型区域不匹配 */
    CONSTRAINT_TYPE_MISMATCH,              /* 约束类型不匹配 */
    CONSTRAINT_PARTICIPANT_COUNT_MISMATCH, /* 参与者数量不匹配 */
    CONSTRAINT_PARTICIPANT_ID_MISMATCH,    /* 参与者 ID 不匹配 */
    COORD_VALUE_MISMATCH,                  /* 坐标值不匹配 */
    COORD_TYPE_MISMATCH                    /* 坐标类型不匹配 */
} MismatchReason;

typedef struct {
    UnifyStatus status;             /* 失败类型 */
    int failed_constraint_id;       /* 失败的约束 ID（-1 表示无） */
    int failed_node_id;             /* 失败的节点 ID（-1 表示无） */
    int failed_port_index;          /* 失败的端口索引（-1 表示无） */
    char *description;              /* 人类可读描述（堆分配，由 destroy 释放） */
    MismatchReason mismatch_reason; /* 不匹配的具体原因 */
    char reason_detail[256];        /* 内嵌固定缓冲：不匹配原因明细 */
} UnifyFailureInfo;
```

### 简化命题/证明快速通道

```c
typedef struct SimpleProposition {
    int id;
    char *name;
    ConstraintGraph *pattern;
    int *input_port_ids;
    int *output_port_ids;
    int input_count;
    int output_count;
} SimpleProposition;

typedef struct SimpleProof {
    int id;
    SimpleProposition *proposition;
    ConstraintGraph *construction;
    bool normalized;
    bool passed;
} SimpleProof;
```

### λ-合一与命题公式操作（相关支撑）

```c
typedef enum {
    LAMBDA_UNIFY_OK,           /* 合一成功 */
    LAMBDA_UNIFY_FAIL,         /* 合一失败（无法匹配） */
    LAMBDA_UNIFY_OCCURS_CHECK, /* 变量出现在自身中 */
    LAMBDA_UNIFY_ERROR         /* 内部错误（如最大深度超限） */
} LambdaUnifyStatus;

typedef struct LambdaSubstitution {
    int index;                         /* De Bruijn 索引 */
    struct LvLambdaTerm *replacement;  /* 替换项 */
    struct LambdaSubstitution *next;
} LambdaSubstitution;

typedef struct {
    PropFormulaEqualFn equal;         /* 公式相等判定 */
    PropFormulaHashFn hash;           /* 公式哈希 */
    PropFormulaIsDescendantFn is_descendant; /* 公式子式判定 */
} PropFormulaOps;
```

### 坐标判等的规范表示（`expr_canon.h` / `expr_canonical.h`）

```c
typedef struct {
    lvRational *coeff;   /* 单项式系数 */
    int *exponents;      /* 各变量指数数组 */
    int var_count;
} lvExprTerm;

typedef struct {
    lvExprTerm *terms;   /* 稀疏多项式项数组（总次数降序、同次字典序） */
    int term_count;
    int term_capacity;
    int var_count;
    char **var_names;
    bool canonicalized;  /* 是否已规范化 */
} lvExprCanonical;
```

## 主要接口

### 完整合一入口

| 接口 | 签名要点 | 说明 |
|------|----------|------|
| `unify_construction_with_proposition` | `(construction, proposition)` | 基础版：端口类型 + 约束匹配 |
| `unify_construction_with_proposition_coord` | `(construction, proposition)` | 坐标增强版：额外验证参与者符号坐标相等 |
| `unify_construction_with_proposition_hash_filtered` | `(construction, proposition)` | 哈希预过滤版：`symbolic_coord_hash` 分组加速 |
| `unify_construction_with_proposition_detailed` | `(construction, pattern, &out_failure)` | 失败时填充 `UnifyFailureInfo` 详细报告 |
| `unify_status_reason_zh` | `(status)` | 状态的中文原因（单一事实来源，如"约束类型不匹配"） |

### 精细化匹配与坐标判等

| 接口 | 签名要点 | 说明 |
|------|----------|------|
| `unify_match_ports` | `(construction, proposition, &bindings, max)` | 独立端口匹配，返回端口对数；绑定缓冲需 ≥ 命题端口数 × 2 |
| `unify_match_constraints` | `(construction, proposition, &bindings)` | 独立约束匹配，返回约束对数 |
| `unify_coords_equal` | `(a, b)` | 两个 `GeomNode` 的符号坐标数组逐槽位判等 |
| `unify_match_coords` | `(c1, c2)` | 两个 `SymbolicCoord` 结构判等（0 = 相等） |

### 等价声明与多态实例化

| 接口 | 签名要点 | 说明 |
|------|----------|------|
| `unify_declare_proposition_equivalence` | `(prop_a_id, prop_b_id, transformation_rule)` | 声明命题等价（存储为双向重写规则） |
| `unify_find_equivalent_proposition` | `(prop_id, &ids, max_count)` | 查找等价命题 |
| `unify_clear_equivalences` | `(void)` | 清除所有等价声明 |
| `unify_equivalence_storage_init` | `(void)` | 初始化等价声明存储（每线程独立实例） |
| `lv_unify_equivalence_storage_cleanup` | `(void)` | 清理存储（兼容别名 `unify_equivalence_storage_cleanup`） |
| `unify_equivalence_count` | `(void)` | 当前等价声明数量 |
| `unify_instantiate_proposition` | `(proposition, type_var_node_id, concrete_type, &out)` | 类型变量 → 具体 `TypeRegion` 实例化 |

### 简化通道与 λ-合一

| 接口 | 签名要点 | 说明 |
|------|----------|------|
| `simple_proposition_create` / `simple_proposition_destroy` | `(name, in_ids, in_count, out_ids, out_count)` | 简化命题生命周期 |
| `simple_proof_create` / `simple_proof_destroy` | `(prop, construction)` | 简化证明生命周期 |
| `simple_proof_check` / `simple_proof_normalize` | `(proof)` | 快速检查 / 规范化 |
| `lambda_unify` | `(t1, t2, &subs, max_depth)` | 句法合一（Martelli-Montanari，处理 VAR/ABS/APP） |
| `lambda_pattern_unify` | `(t1, t2, &subs, max_depth)` | Miller 模式合一（Imitation/Projection 规则） |
| `lambda_is_pattern` | `(term)` | 模式形式判定（高阶变量参数须为不同 bound 变量） |
| `lambda_unify_apply_to_graph` | `(graph, subs, binder_depth)` | 将替换实例化进约束图（事务式提交与回滚） |
| `unify_set_stream_context` | `(ctx)` | 绑定合一检查器流式输出上下文 |

## 工作流程

1. **预备**：可选调用 `unify_declare_proposition_equivalence` 登记等价；多态命题先 `unify_instantiate_proposition` 实例化。
2. **哈希分组**：哈希预过滤版按 `symbolic_coord_hash` 对节点分组，仅在同组候选间匹配。
3. **端口匹配**：`unify_match_ports` 核对端口类型（`PortType`）、命名空间深度（`namespace_depth`）与 `TypeRegion`，失败 → `UNIFY_STATUS_PORT_TYPE_MISMATCH`。
4. **约束匹配**：`unify_match_constraints` 核对约束类型、参与者数量与参与者 ID，失败 → `UNIFY_STATUS_CONSTRAINT_MISMATCH`。
5. **坐标判等**：坐标增强版经 `unify_coords_equal` / `unify_match_coords` 判等符号坐标，失败 → `UNIFY_STATUS_COORD_MISMATCH`。
6. **报告与提交**：失败时经 detailed 入口输出 `UnifyFailureInfo`（`unify_failure_info_destroy` 释放）；等价声明在合一前自动应用；`lambda_unify_apply_to_graph` 以事务方式将 λ-替换实例化到图中。

## 模块关系

| 模块 | 关系 | 说明 |
|------|------|------|
| [02_constraint_graph.md](02_constraint_graph.md) | 输入载体 | 合一的双方（构造图 / 命题模式图）均为 `ConstraintGraph` |
| [03_normalization.md](03_normalization.md) | 前置 | `simple_proof_normalize` 与坐标规范表示（`lvExprCanonical`）保证判等基准一致 |
| [04_solver.md](04_solver.md) | 下游 | 合一通过后构造图进入求解器验证；坐标哈希分组复用求解器坐标设施 |
| [05_rewrite.md](05_rewrite.md) | 等价联动 | 命题等价以双向重写规则存储，替换逻辑复用重写引擎 |
| [09_proof.md](09_proof.md) | 上层 | `UnifyStatus` 直接决定命题证明的通过/失败状态 |
| [11_wfc_paradigm.md](11_wfc_paradigm.md) | 互补 | WFC 传播负责约束求解，合一负责"命题模式"的匹配判定 |
| [23_core_infrastructure.md](23_core_infrastructure.md) | 基础设施 | `SymbolicCoord`/`TypeRegion`/`PropFormula` 类型族归属核心设施 |

## 版本历史

| 版本 | 变更 |
|------|------|
| v1.0 | 基础合一 `unify_construction_with_proposition`、端口/约束匹配 |
| v1.1 | 坐标增强版、`unify_coords_equal`/`unify_match_coords` 坐标判等 |
| v1.2 | 哈希预过滤加速、`UnifyFailureInfo` 详细失败报告 |
| v1.3 | 命题等价声明（双向重写规则存储）、多态实例化 `unify_instantiate_proposition` |
| v1.4 | 简化命题/证明快速通道、等价声明存储线程化（`unify_equivalence_storage_init`） |
| v1.5 | 引入 λ-合一（`lambda_unify` / `lambda_pattern_unify`）并支持实例化入图 |
