# 27. 量词与关系逻辑

## 27.1 模块概述

本文档描述 Lv-00 几何元语言系统的量词系统和关系模型层。量词系统提供全称量词（∀）、存在量词（∃）和唯一存在量词（∃!）的形式化处理，支持量词实例化/泛化操作和有限域上的量词消去。关系模型层借鉴 Alloy 的"关系即一切"统一建模范式，将几何约束图重新解释为关系模型。

**覆盖头文件**：
- `quantifier.h` —— 量词系统（∀, ∃, ∃!）
- `relation_model.h` —— 关系模型层（借鉴 Alloy）

---

## 27.2 quantifier.h —— 量词系统

### 27.2.1 设计定位

提供全称量词、存在量词和唯一存在量词的形式化处理：
- 量词实例化/泛化操作
- 有限域上的量词消去
- 与约束图的双向映射
- 三值逻辑真值评估
- 遵循构造性/BHK 解释语义

### 27.2.2 量词类型

| 量词 | 枚举值 | 符号 | 含义 |
|------|--------|------|------|
| 全称量词 | `lv_FORALL` | ∀ | "对所有...都成立" |
| 存在量词 | `lv_EXISTS` | ∃ | "存在一个...使得..." |
| 唯一存在 | `lv_EXISTS_UNIQUE` | ∃! | "存在唯一一个...使得..." |

### 27.2.3 核心数据结构

#### 量化域（lvDomain）

```c
struct lvDomain {
    int id;
    char *domain_name;         // 域名（如 "R", "Triangle", "Point"）
    
    // 有限枚举域
    int *domain_elements;      // 域元素列表（节点ID数组）
    int element_count;
    
    // 约束图子图域
    ConstraintGraph *subgraph;
    
    bool is_finite;
    int estimated_cardinality; // 估计基数（-1 = 未知/无限）
};
```

**支持三种域定义方式**：
- 有限枚举：`domain_elements` 中列出所有元素
- 约束图子图：`subgraph` 指定一个约束图子图作为域
- 命名域：`domain_name` 引用预定义的域（如实数域 R）

#### 量化表达式（lvQuantifiedExpr）

```c
struct lvQuantifiedExpr {
    int id;
    lvQuantifier quantifier;  // 量词类型
    char *variable_name;        // 绑定变量名
    int variable_node_id;       // 绑定变量对应的约束图节点ID
    lvDomain *domain;         // 量化域
    
    struct Proposition *body_proposition;  // 体命题
    
    int *instantiated_ids;      // 已实例化的变量ID列表
    int instantiated_count;
    
    lvTruthValue cached_truth;  // 缓存的真值（三值逻辑）
    bool truth_cache_valid;
};
```

**结构**：QUANTIFIER variable ∈ domain . body_proposition

**示例**：∀p ∈ Points . collinear(p, A, B) → onSegment(p, AB)

### 27.2.4 量词操作

#### 全称量词实例化（∀-消去）

```c
lvQuantResult lv_quantifier_instantiate(
    const lvQuantifiedExpr *expr,  // ∀...
    int instance_id,                  // 要代入的实例节点ID
    lvQuantifiedResult *out_result
);
```

**规则**：从 ∀x.P(x) 推导出 P(t)，其中 t 在域中。

**示例**：
```
从 "∀p∈Points.collinear(p,A,B)→onSegment(p,AB)"
和 t=Midpoint(A,B) 推导出
"collinear(Midpoint(A,B),A,B)→onSegment(Midpoint(A,B),AB)"
```

#### 全称量词泛化（∀-引入）

```c
lvQuantResult lv_quantifier_generalize(
    const lvQuantifiedExpr *expr,
    lvQuantifiedResult *out_result
);
```

**规则**：从 P(x) 对任意 x∈D 成立推导出 ∀x.P(x)。

**前提**：x 不能在前提集中自由出现（特征变量条件）。

#### 存在量词引入（∃I）

```c
lvQuantResult lv_quant_exists_introduce(
    lvQuantifiedExpr *expr,  // 量词为 ∃
    int witness_id,             // 目击者节点ID
    lvQuantifiedResult *out_result
);
```

**规则**：从 P(t) 推导出 ∃x.P(x)，其中 t 在域中。

#### 存在量词消去（∃E）

```c
lvQuantResult lv_quant_exists_eliminate(
    const lvQuantifiedExpr *exists_expr,
    struct Proposition *target_prop,  // 目标命题 Q
    lvQuantifiedResult *out_result
);
```

**规则**：从 ∃x.P(x) 和 ∀y.(P(y)→Q) 推导出 Q（其中 y 不在 Q 中自由）。

### 27.2.5 有限域量词消去

#### 全称量词消去

```c
lvQuantResult lv_quant_eliminate_forall_finite(
    const lvQuantifiedExpr *expr,
    lvQuantifiedResult *out_result
);
```

**展开规则**：在有限域 D = {d₁, ..., dₙ} 上，
```
∀x∈D.P(x) → P(d₁) ∧ P(d₂) ∧ ... ∧ P(dₙ)
```

#### 存在量词消去

```c
lvQuantResult lv_quant_eliminate_exists_finite(
    const lvQuantifiedExpr *expr,
    lvQuantifiedResult *out_result
);
```

**展开规则**：
```
∃x∈D.P(x) → P(d₁) ∨ P(d₂) ∨ ... ∨ P(dₙ)
```

#### 唯一存在量词消去

```c
lvQuantResult lv_quant_eliminate_exists_unique_finite(
    const lvQuantifiedExpr *expr,
    lvQuantifiedResult *out_result
);
```

**展开规则**：
```
∃!x.P(x) → (P(d₁) ∧ ¬P(d₂) ∧ ... ∧ ¬P(dₙ)) ∨ (¬P(d₁) ∧ P(d₂) ∧ ... ∧ ¬P(dₙ)) ∨ ...
```
即恰好一个元素满足 P。

### 27.2.6 辅助 API

```c
// 域管理
lvDomain *lv_quant_domain_create(int id, const char *domain_name);
lvDomain *lv_quant_domain_create_finite(int id, const int *elements, int count);
bool lv_quant_domain_add_element(lvDomain *domain, int element);
bool lv_quant_domain_contains(const lvDomain *domain, int element);
int lv_quant_domain_size(const lvDomain *domain);
void lv_quant_domain_destroy(lvDomain *domain);

// 量化表达式
lvQuantifiedExpr *lv_quant_expr_create(int id, lvQuantifier quantifier,
    const char *variable_name, int variable_node_id,
    lvDomain *domain, struct Proposition *body_prop);
void lv_quant_expr_destroy(lvQuantifiedExpr *expr);
lvTruthValue lv_quant_expr_evaluate(lvQuantifiedExpr *expr);

// 检查与统计
bool lv_quant_is_eliminable(const lvQuantifiedExpr *expr);
int lv_quant_count_satisfying(const lvQuantifiedExpr *expr);
```

---

## 27.3 relation_model.h —— 关系模型层

### 27.3.1 设计借鉴来源

**Alloy (alloytools.org)** — Daniel Jackson 的关系逻辑建模语言：
- 所有数据都是关系元组（Atom × ... × Atom → Bool）
- 有限范围实例查找（bounded exhaustive search）
- 关系组合算子（join/product/transpose/closure）

### 27.3.2 核心设计理念

将几何约束图重新解释为关系模型：
- **点/线/区域**是关系原子（Atom）
- **约束边**是关系上的逻辑公式（n 元关系）
- **查询** = 关系表达式求值
- **验证** = 有限范围反例查找

### 27.3.3 关系原子

```c
typedef enum {
    REL_ATOM_POINT,      // 点原子
    REL_ATOM_LINE,       // 线原子
    REL_ATOM_REGION,     // 区域原子
    REL_ATOM_PORT,       // 端口原子
    REL_ATOM_FUNC_BLOCK  // 函数块原子
} RelAtomType;

typedef struct RelAtom {
    int atom_id;        // 原子唯一标识符
    RelAtomType type;   // 原子类型
    char *label;        // 原子标签
    int graph_node_id;  // 对应的约束图节点ID
} RelAtom;
```

### 27.3.4 关系

```c
typedef struct Relation {
    char *name;
    int arity;                     // 关系元数（1~n）
    RelSignature *domains[8];      // 各列的定义域签名
    int **tuples;                  // 元组数组
    int tuple_count;
} Relation;
```

**借鉴 Alloy 的核心理念：一切皆为关系**
- 标量是 1 元关系
- 集合是 1 元关系
- 二元关系是边的集合

### 27.3.5 关系运算符

| 运算符 | 枚举值 | 符号 | 说明 |
|--------|--------|------|------|
| 并集 | `REL_OP_UNION` | R + S | 关系并集 |
| 交集 | `REL_OP_INTERSECTION` | R & S | 关系交集 |
| 差集 | `REL_OP_DIFFERENCE` | R - S | 关系差集 |
| 连接 | `REL_OP_JOIN` | R.S | 关系连接 |
| 笛卡尔积 | `REL_OP_PRODUCT` | R -> S | 笛卡尔积 |
| 转置 | `REL_OP_TRANSPOSE` | ~R | 交换列顺序 |
| 传递闭包 | `REL_OP_TRANSITIVE_CLOSURE` | ^R | R⁺ |
| 自反传递闭包 | `REL_OP_REFL_TRANS_CLOSURE` | *R | R* |
| 恒等 | `REL_OP_IDENTITY` | iden | 恒等关系 |
| 补集 | `REL_OP_COMPLEMENT` | !R | 关系补集 |
| 域约束 | `REL_OP_RESTRICT_DOMAIN` | S <: R | 域约束 |
| 值域约束 | `REL_OP_RESTRICT_RANGE` | R :> S | 值域约束 |
| 覆盖 | `REL_OP_OVERRIDE` | R ++ S | 关系覆盖 |

### 27.3.6 关系公式

```c
typedef enum {
    REL_FORMULA_FORALL,   // 全称量词：all x: S | F
    REL_FORMULA_EXISTS,   // 存在量词：some x: S | F
    REL_FORMULA_NO,       // 不存在：no R
    REL_FORMULA_SOME,     // 非空：some R
    REL_FORMULA_LONE,     // 最多一个：lone R
    REL_FORMULA_ONE,      // 恰好一个：one R
    REL_FORMULA_EQ,       // 关系相等：R = S
    REL_FORMULA_SUBSET,   // 子集：R in S
    REL_FORMULA_AND,      // 合取：F && G
    REL_FORMULA_OR,       // 析取：F || G
    REL_FORMULA_NOT,      // 否定：!F
    REL_FORMULA_IMPLIES   // 蕴含：F => G
} RelFormulaType;
```

### 27.3.7 关系模型

```c
typedef struct RelModel {
    RelSignature **sigs;
    int sig_count;
    
    Relation **relations;
    int relation_count;
    
    RelFormula **facts;       // 事实（恒真约束）
    int fact_count;
    
    RelFormula **assertions;  // 断言（待验证）
    int assertion_count;
    
    // 有限范围配置
    int max_point_count;
    int max_line_count;
    int max_region_count;
    int max_func_block_count;
} RelModel;
```

### 27.3.8 有限范围配置

```c
typedef struct SmallScopeConfig {
    int scope_point;   // Point 的实例上限
    int scope_line;    // Line 的实例上限
    int scope_region;  // Region 的实例上限
    int scope_block;   // FuncBlock 的实例上限
    int bitwidth;      // 整数位宽
} SmallScopeConfig;
```

**默认配置**：point=8, line=8, region=4, block=2, bitwidth=4

### 27.3.9 核心 API

#### 关系运算

```c
Relation *rel_union(const Relation *a, const Relation *b);           // R + S
Relation *rel_intersection(const Relation *a, const Relation *b);    // R & S
Relation *rel_difference(const Relation *a, const Relation *b);      // R - S
Relation *rel_join(const Relation *a, const Relation *b);            // R.S
Relation *rel_product(const Relation *a, const Relation *b);         // R -> S
Relation *rel_transpose(const Relation *r);                          // ~R
Relation *rel_transitive_closure(const Relation *r);                 // ^R
Relation *rel_reflexive_transitive_closure(const Relation *r);       // *R
```

#### 模型构建

```c
// 从约束图构建关系模型
RelModel *relation_model_from_graph(const ConstraintGraph *graph);
void relation_model_destroy(RelModel *model);
bool relation_model_add_fact(RelModel *model, RelFormula *formula);
bool relation_model_add_assertion(RelModel *model, RelFormula *formula);
```

**转换规则**：
- 每种 GeomType 对应一个 RelSignature
- 每个节点对应一个 RelAtom
- 每个约束边对应关系元组
- 推导出的几何不变式作为事实（fact）

#### 可满足性检查

```c
bool relation_check_satisfiability(RelModel *model, const SmallScopeConfig *scope);
RelInstance *relation_find_instance(RelModel *model, const SmallScopeConfig *scope, bool assertions);
void relation_instance_destroy(RelInstance *inst);
```

**借鉴 Alloy 的 bounded exhaustive checking**：
限制每种 sig 的最大实例数，在小范围内穷举所有可能。

#### 求值

```c
Relation *relation_evaluate_expr(const RelModel *model, const RelInstance *inst, const RelExpr *expr);
bool relation_evaluate_formula(const RelModel *model, const RelInstance *inst, const RelFormula *formula);
```

#### 导出

```c
char *relation_model_export_alloy(const RelModel *model);
char *relation_instance_export_xml(const RelInstance *inst);
```

---

## 27.4 代码-理论对应关系

| 代码概念 | 理论对应 | 文档位置 |
|----------|----------|----------|
| `lvQuantifiedExpr` | 量化命题公式 | 本文档 27.2.3 |
| `lv_quantifier_instantiate()` | ∀-消去规则 | 本文档 27.2.4 |
| `lv_quantifier_generalize()` | ∀-引入规则 | 本文档 27.2.4 |
| `lv_quant_eliminate_*_finite()` | 有限域量词消去 | 本文档 27.2.5 |
| `RelAtom` | Alloy sig 实例 | 本文档 27.3.3 |
| `Relation` | n 元关系 | 本文档 27.3.4 |
| `rel_join()` | 关系连接运算 | 本文档 27.3.9 |
| `rel_transitive_closure()` | 传递闭包 R⁺ | 本文档 27.3.9 |
| `SmallScopeConfig` | 有限范围检查 | 本文档 27.3.8 |

---

## 27.5 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [16_logic_verification.md](16_logic_verification.md) | 逻辑验证、三值逻辑 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图核心 |
| [09_proof.md](09_proof.md) | 命题与证明系统 |
| [26_interactive_geometry.md](26_interactive_geometry.md) | 交互式几何 |

---

## 27.6 版本历史

- **v5.0.0** (当前)
  - 量词系统（∀, ∃, ∃!）
  - 量词实例化/泛化操作
  - 有限域量词消去
  - 关系模型层（借鉴 Alloy）
  - 关系运算（join/closure/transpose）
  - 有限范围可满足性检查

- **v3.3.0**
  - 基础逻辑检查
  - 三值逻辑支持
