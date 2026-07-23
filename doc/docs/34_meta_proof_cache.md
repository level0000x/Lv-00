# 34. 元证明与推理缓存

## 34.1 模块概述

本文档描述 Lv-00 几何元语言系统中的剪枝合法性元证明、命题逻辑验证器、推理结果缓存和节点深拷贝公共接口。该组模块处于证明系统、WFC 约束传播和工程复用层之间，用于保证剪枝操作的数学合法性、命题验证的构造性、证明搜索的性能以及节点复制的一致所有权语义。

**覆盖头文件**：
- `meta_proof.h` —— 剪枝合法性元证明，WFC 范式数学严格化
- `prop_verifier.h` —— 自然演绎风格命题逻辑验证器与 BHK 几何构造桥接
- `reasoning_cache.h` —— 基于开放寻址哈希表的推理结果缓存
- `node_deep_copy.h` —— 几何节点、端口和符号坐标深拷贝公共接口

---

## 34.2 理论定位

Lv-00 使用 WFC 风格的状态空间削减、约束传播和多策略证明搜索。若某个状态被剪枝，系统必须说明该剪枝不会排除真实合法解。因此需要在普通证明系统之上增加一层元证明：证明“被排除的候选状态确实不可能属于任何全局解”。

本模块的核心职责为：

1. **剪枝合法性证明**：证明每个被删除状态都不是全局合法解的一部分。
2. **完备性报告**：聚合剪枝证明状态，形成整体可信颜色。
3. **命题构造性验证**：以自然演绎和 BHK 解释检查命题是否具有几何构造证物。
4. **推理缓存**：避免重复证明搜索与中间结果重复计算。
5. **深拷贝规范化**：统一节点、端口、符号坐标的复制所有权语义。

---

## 34.3 meta_proof.h —— 剪枝合法性元证明

### 34.3.1 数学基础

剪枝操作可形式化为：

```text
π = (v, R, φ)
```

其中：
- `v` 是被剪枝的节点；
- `R ⊂ Σ(v)` 是被移除的状态子集；
- `φ` 是用于证明剪枝合法性的理由或策略。

合法性条件：

```text
∀ r ∈ R, ∀ σ* ∈ Σ_global : σ*(v) ≠ r
```

即每个被移除状态都不可能出现在任何全局合法解中。

完备性定理：

```text
若每步剪枝合法且 Σ_global ≠ ∅，
则剪枝后保留的全局解仍然是原问题的合法解。
```

### 34.3.2 剪枝策略

```c
typedef enum {
    PRUNE_DIRECT_CONTRADICTION,
    PRUNE_PROPAGATION_CONTRADICTION,
    PRUNE_ALGEBRAIC_EXCLUSION
} PruneStrategy;
```

| 策略 | 层级 | 说明 |
|------|------|------|
| `PRUNE_DIRECT_CONTRADICTION` | L1 | 候选状态与某个约束直接矛盾 |
| `PRUNE_PROPAGATION_CONTRADICTION` | L2 | 选择候选后传播导致矛盾 |
| `PRUNE_ALGEBRAIC_EXCLUSION` | L3 | 候选不满足多项式方程组解集 |

### 34.3.3 元证明结果

```c
typedef enum {
    META_PROVE_VALID,
    META_PROVE_INVALID,
    META_PROVE_INCONCLUSIVE,
    META_PROVE_TIMEOUT
} MetaProofResult;
```

- `VALID`：剪枝合法，已证明；
- `INVALID`：剪枝非法，候选可能是合法解；
- `INCONCLUSIVE`：当前证明能力不足；
- `TIMEOUT`：证明超时。

### 34.3.4 剪枝记录

```c
typedef struct PruningOperation {
    int node_id;
    SymbolicCoord **removed_states;
    int removed_count;

    PruneStrategy strategy;
    TrustColor trust;

    int conflicting_constraint_id;

    int propagation_steps;
    int *propagation_trace;
    int propagation_trace_count;

    int poly_violation_count;
} PruningOperation;
```

```c
typedef struct PruningRecord {
    PruningOperation *operations;
    int operation_count;
    int capacity;
    int64_t total_states_removed;
    int64_t total_states_remaining;
} PruningRecord;
```

### 34.3.5 完备性报告

```c
typedef struct CompletenessReport {
    int total_prunings;
    int proven_prunings;
    int unproven_prunings;
    int invalid_prunings;
    TrustColor overall_color;
    char summary[256];
} CompletenessReport;
```

该报告将剪枝合法性状态映射到整体可信颜色。若存在非法剪枝，则整体可信度必须下降；若存在未证明剪枝，则不能标为完全绿色路径。

### 34.3.6 元证明上下文

```c
typedef struct MetaProofContext {
    ConstraintGraph *graph;
    PropagationContext *prop_ctx;
    EquivClassManager *equiv_mgr;
    ProofNavigator *navigator;

    PruningRecord *record;

    int max_propagation_steps;
    int timeout_ms;
    bool enable_l1;
    bool enable_l2;
    bool enable_l3;

    int64_t l1_proofs;
    int64_t l2_proofs;
    int64_t l3_proofs;
    int64_t inconclusive_count;

    StreamContext *stream_ctx;
} MetaProofContext;
```

### 34.3.7 核心 API

生命周期：

```c
MetaProofContext *meta_proof_context_create(ConstraintGraph *graph,
                                             PropagationContext *prop_ctx);
void meta_proof_context_destroy(MetaProofContext *ctx);
```

三层证明策略：

```c
MetaProofResult meta_prove_direct_contradiction(MetaProofContext *ctx,
                                                int node_id,
                                                const SymbolicCoord *candidate,
                                                int *out_conflicting_constraint);

MetaProofResult meta_prove_propagation_contradiction(MetaProofContext *ctx,
                                                     int node_id,
                                                     const SymbolicCoord *candidate);

MetaProofResult meta_prove_algebraic_exclusion(MetaProofContext *ctx,
                                                int node_id,
                                                const SymbolicCoord *candidate);
```

自动策略选择：

```c
MetaProofResult meta_prove_pruning(MetaProofContext *ctx,
                                    int node_id,
                                    const SymbolicCoord *candidate);
```

该函数按 L1 → L2 → L3 优先级尝试，返回第一个成功证明结果。

完备性验证：

```c
CompletenessReport *meta_prove_completeness(MetaProofContext *ctx);
void meta_proof_completeness_report_destroy(CompletenessReport *report);
```

记录与配置：

```c
void meta_proof_record_pruning(MetaProofContext *ctx,
                               int node_id,
                               SymbolicCoord **removed,
                               int count,
                               PruneStrategy strategy,
                               TrustColor trust);

const PruningRecord *meta_proof_get_record(const MetaProofContext *ctx);
void meta_proof_set_navigator(MetaProofContext *ctx, ProofNavigator *navigator);
void meta_proof_set_equiv_manager(MetaProofContext *ctx, EquivClassManager *mgr);
void meta_proof_set_stream_context(MetaProofContext *ctx, StreamContext *stream_ctx);
void meta_proof_set_strategy_enabled(MetaProofContext *ctx,
                                      PruneStrategy strategy,
                                      bool enable);
void meta_proof_set_max_propagation_steps(MetaProofContext *ctx, int max_steps);
void meta_proof_set_timeout(MetaProofContext *ctx, int timeout_ms);
```

统计：

```c
void meta_proof_get_statistics(const MetaProofContext *ctx,
                               int64_t *out_l1,
                               int64_t *out_l2,
                               int64_t *out_l3,
                               int64_t *out_inconclusive);
```

---

## 34.4 prop_verifier.h —— 命题逻辑验证器

### 34.4.1 设计目标

命题验证器是 Lv-00 自举目标的一部分：在几何构造系统内部验证命题逻辑证明搜索，并通过 BHK 解释桥接到几何构造。

支持公式：

```c
typedef enum {
    PROP_ATOM,
    PROP_CONJUNCTION,
    PROP_DISJUNCTION,
    PROP_IMPLICATION,
    PROP_NEGATION,
    PROP_BOTTOM,
    PROP_TRUE
} PropFormulaType;
```

### 34.4.2 命题公式 AST

```c
struct PropFormula {
    PropFormulaType type;
    union {
        struct { char name[64]; } atom;
        struct { PropFormula *left, *right; } binary;
        struct { PropFormula *operand; } unary;
    } data;
};
```

公式不可变使用，创建后通过复制/销毁接口管理生命周期。

### 34.4.3 验证结果与配置

```c
typedef enum {
    PV_VERIFY_PROVEN,
    PV_VERIFY_DISPROVEN,
    PV_VERIFY_FAILED,
    PV_VERIFY_INVALID_INPUT,
    PV_VERIFY_TIMEOUT,
    PV_VERIFY_ERROR
} PropVerifyResult;
```

```c
typedef struct {
    PropVerifyResult result;
    int steps_used;
    int max_steps;
    char error_message[256];
    char construction_summary[512];
} VerifyDetail;
```

```c
typedef struct {
    int max_steps;
    bool use_intuitionistic;
    bool enable_ex_falso;
    int timeout_ms;
} VerifierConfig;
```

默认配置：

```c
#define VERIFIER_CONFIG_DEFAULT {10000, true, false, 30000}
```

### 34.4.4 核心验证

```c
VerifyDetail prop_verifier_verify(const PropFormula **premises,
                                  int premise_count,
                                  const PropFormula *goal,
                                  const VerifierConfig *config);
```

该接口验证 sequent：

```text
premises ⊢ goal
```

验证方式为自然演绎风格的向后链接证明搜索。在直觉主义模式下，不使用 RAA（归谬法）。仅在 `enable_ex_falso` 为 true 时允许从 `⊥` 推出任意命题。

### 34.4.5 BHK 几何构造桥接

BHK 解释：

| 命题形式 | 构造证物 |
|----------|----------|
| A ∧ B | 一对证物 `(a, b)` |
| A ∨ B | 一个证物及其左右来源标记 |
| A → B | 将 A 的证物转换为 B 的证物的构造 |
| ¬A | 将 A 的证物转换为 ⊥ 的构造 |
| ⊥ | 不存在证物 |

结构：

```c
typedef struct {
    bool verified;
    char bhk_interpretation[512];
    char geometric_mapping[512];
    int missing_constructions;
    char **missing_descriptions;
    int missing_count;
} BHKVerificationResult;
```

API：

```c
BHKVerificationResult prop_verifier_bhk_verify(
    const PropFormula **premises,
    int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config);
```

### 34.4.6 信任颜色桥接

```c
int prop_verifier_apply_trust_colors(ConstraintGraph *graph,
                                     const PropFormula **premises,
                                     int premise_count,
                                     const PropFormula *goal,
                                     const VerifierConfig *config,
                                     BHKVerificationResult *out_result);
```

映射规则：

| 验证状态 | 几何构造缺失 | 信任颜色 |
|----------|--------------|----------|
| 已证明 | 0 | GREEN |
| 已证明 | ≤2 | YELLOW |
| 已证明 | ≥3 | AMBER |
| 搜索耗尽未证伪 | 任意 | BLUE |
| 已证伪 | 任意 | RED |
| 超时/错误 | 任意 | BLUE |

### 34.4.7 不可构造性分析与等价检查

```c
InconstructibilityAnalysis prop_verifier_analyze_inconstructibility(
    const PropFormula **premises,
    int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config);
```

```c
bool prop_verifier_check_equivalence(const PropFormula *a,
                                     const PropFormula *b,
                                     const VerifierConfig *config);

bool prop_verifier_check_tautology(const PropFormula *f,
                                   const VerifierConfig *config);
```

---

## 34.5 reasoning_cache.h —— 推理结果缓存

### 34.5.1 设计目标

推理结果缓存用于避免证明搜索过程中重复计算相同命题与规则集组合的推理结果。

特点：
- 开放寻址哈希表；
- 线性探测解决冲突；
- 固定容量；
- 满容量时替换最早条目，近似 LRU；
- 线程不安全，调用者需自行同步；
- 内置命中/未命中统计。

### 34.5.2 生命周期与操作

```c
lvReasoningCache *lv_reasoning_cache_create(size_t capacity);
void lv_reasoning_cache_destroy(lvReasoningCache *cache);
```

```c
bool lv_reasoning_cache_has(lvReasoningCache *cache, uint64_t key);
void lv_reasoning_cache_put(lvReasoningCache *cache, uint64_t key, int result);
int lv_reasoning_cache_get(lvReasoningCache *cache, uint64_t key);
void lv_reasoning_cache_clear(lvReasoningCache *cache);
```

默认容量为 4096；实际内部容量取不小于指定容量的最小 2 的幂。

### 34.5.3 统计接口

```c
void lv_reasoning_cache_get_stats(const lvReasoningCache *cache,
                                     size_t *hits,
                                     size_t *misses,
                                     size_t *size);
```

统计信息可用于判断证明搜索是否存在重复子问题，并辅助调节缓存容量。

---

## 34.6 node_deep_copy.h —— 节点深拷贝公共接口

### 34.6.1 设计目标

该模块统一 `engine.c`、`proof.c`、`rewrite.c` 中曾经重复存在的节点复制逻辑，明确所有权语义，避免浅拷贝导致悬空指针或重复释放。

### 34.6.2 所有权语义

- `type_region` 执行浅拷贝，所有权由 TypeSystem 统一管理；
- `connected_to` 指针置为 NULL，调用者需通过 ID 映射更新连接关系；
- `symbolic_coords` 执行深拷贝，所有权归新节点所有。

### 34.6.3 API

```c
Port *node_deep_copy_port(const Port *orig);
GeomNode *node_deep_copy_geom_node(const GeomNode *orig, const int *id_map);
SymbolicCoord *node_deep_copy_symbolic_coord(const SymbolicCoord *orig);
```

`id_map` 用于旧节点 ID 到新节点 ID 的映射。若为 NULL，则只复制节点本体，不修复连接关系。

---

## 34.7 理论—代码对应关系

| 代码概念 | 理论/工程对应 | 说明 |
|----------|----------------|------|
| `PruningOperation` | 剪枝操作 π = (v,R,φ) | 记录被移除状态和证明策略 |
| `MetaProofContext` | 元证明上下文 | 关联图、传播、等价类和证明导航器 |
| `meta_prove_pruning` | 自动剪枝合法性证明 | 按 L1→L2→L3 尝试证明 |
| `CompletenessReport` | 剪枝完备性报告 | 聚合所有剪枝合法性 |
| `PropFormula` | 命题逻辑 AST | 原子、合取、析取、蕴涵、否定 |
| `prop_verifier_verify` | 自然演绎证明搜索 | 验证 premises ⊢ goal |
| `BHKVerificationResult` | 构造性证物检查 | 映射命题证明到几何构造 |
| `lvReasoningCache` | 重复子问题缓存 | 哈希键到推理结果 |
| `node_deep_copy_geom_node` | 结构复制规范 | 消除重复实现和所有权歧义 |

---

## 34.8 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [09_proof.md](09_proof.md) | 证明系统核心 |
| [13_proof_engine_enhanced.md](13_proof_engine_enhanced.md) | 增强证明引擎、会话、评分与规则 |
| [24_constraint_propagation.md](24_constraint_propagation.md) | WFC 传播与等价类系统 |
| [16_logic_verification.md](16_logic_verification.md) | 逻辑验证与命题验证 |
| [22_proof_export_trace_widget.md](22_proof_export_trace_widget.md) | 证明导出与追踪 |
| [31_stream_interop.md](31_stream_interop.md) | 流式事件输出 |

---

## 34.9 版本历史

- **v5.0.0**
  - 补全文档化：剪枝合法性元证明、命题逻辑验证器、推理缓存与节点深拷贝。
  - 明确 WFC 剪枝完备性与 BHK 构造验证之间的关系。

- **v3.3.0**
  - 引入命题逻辑验证器、元证明层与推理缓存基础接口。
