# 33. Gappa 浮点验证与解析安全

## 33.1 模块概述

本文档描述 Lv-00 几何元语言系统中的 Gappa 风格浮点证明 DSL、谓词传播引擎、解析器安全边界、路径类型系统与代数模式构造接口。这些模块共同负责浮点命题的形式化描述、误差谓词传播、输入安全、构造路径表达和链式代数几何建模。

**覆盖头文件**：
- `gappa_dsl.h` —— Gappa 风格浮点证明 DSL
- `gappa_propagate.h` —— Gappa 谓词正向/反向传播
- `parser_safety.h` —— 解析器输入安全校验与净化
- `path_type.h` —— 借鉴 HoTT/Arend 的路径类型系统
- `algebra_mode.h` —— 借鉴 build123d/CadQuery 的代数模式构造引擎

---

## 33.2 理论定位

Lv-00 的证明系统以精确构造为核心，但工程实现必须面对浮点程序、用户输入和交互式构造语言。本模块承担以下职责：

1. **浮点证明描述**：用 Gappa 风格 DSL 表达变量范围、舍入格式和误差目标。
2. **谓词传播**：通过区间传播与抽象解释推导新的误差谓词。
3. **解析安全**：在 DSL 和公式解析之前建立输入长度、token、AST 深度和字符安全边界。
4. **路径类型桥接**：将“构造即证明”与 HoTT 中“等式证明即路径”的思想对应起来。
5. **代数模式构造**：以无隐式状态的链式 API 记录构造历史和约束图。

---

## 33.3 gappa_dsl.h —— 浮点证明 DSL

### 33.3.1 舍入模式

```c
typedef enum {
    lv_ROUND_NE = 0,
    lv_ROUND_NA,
    lv_ROUND_ZR,
    lv_ROUND_DN,
    lv_ROUND_UP,
    lv_ROUND_COUNT
} lvGappaRounding;
```

这些模式对应 IEEE 754 舍入方向：最近偶数、最近远离零、向零、向负无穷、向正无穷。

### 33.3.2 浮点格式

```c
typedef struct {
    int precision_bits;
    int exponent_bits;
    lvGappaRounding rounding;
    char name[64];
} lvGappaFormat;
```

预定义格式包括：`binary16`、`binary32`、`binary64`、`binary128`。

```c
bool gappa_format_predefined(const char *name, lvGappaFormat *out);
```

### 33.3.3 谓词类型

```c
typedef enum {
    lv_PRED_BND = 0,
    lv_PRED_ABS,
    lv_PRED_REL,
    lv_PRED_LIN,
    lv_PRED_FIX,
    lv_PRED_FLT,
    lv_PRED_NZR,
    lv_PRED_EQL
} lvGappaPredType;
```

| 类型 | 语义 |
|------|------|
| BND | `x in [lo, hi]` |
| ABS | `|x| <= bound` |
| REL | `|x - y| / |y| <= bound` |
| LIN | 线性组合范围约束 |
| FIX | 固定精确值 |
| FLT | 浮点舍入值 |
| NZR | 非零性约束 |
| EQL | 等式约束 |

### 33.3.4 谓词与目标

```c
typedef struct {
    lvGappaPredType type;
    char expr_lhs[256];
    char expr_rhs[256];
    double bound_lo;
    double bound_hi;
    double bound_abs;
    double bound_rel;
    int is_hypothesis;
} lvGappaPredicate;
```

```c
typedef struct {
    lvGappaPredicate predicate;
    char description[256];
    int is_proven;
    char proof_method[128];
} lvGappaProofGoal;
```

### 33.3.5 解析与证明 API

```c
bool gappa_parse(const char *dsl_string,
                 lvGappaPredicate **hypotheses,
                 int *hyp_count,
                 lvGappaProofGoal **goals,
                 int *goal_count);
```

支持语法示例：

```text
x in [0, 1]
y in [-1, 1]
x in [0, 1] -> |x - 0.5| <= 0.5
```

证明接口：

```c
lvGappaProofResult gappa_prove(
    const lvGappaPredicate *hypotheses,
    int hyp_count,
    lvGappaProofGoal *goals,
    int goal_count,
    const lvGappaFormat *fmt);
```

证明结果：

```c
typedef struct {
    int success;
    int goals_total;
    int goals_proven;
    int goals_failed;
    char error_message[512];
    lvGappaProofGoal *goals;
} lvGappaProofResult;
```

---

## 33.4 gappa_propagate.h —— 谓词传播

### 33.4.1 谓词集合

```c
#define lv_PRED_SET_MAX_SIZE 256

typedef struct {
    lvGappaPredicate preds[lv_PRED_SET_MAX_SIZE];
    int count;
} lvGappaPredSet;
```

API：

```c
void gappa_pred_set_init(lvGappaPredSet *set);
bool gappa_pred_set_add(lvGappaPredSet *set, const lvGappaPredicate *pred);
int gappa_pred_set_find(lvGappaPredSet *set, const char *var_name, lvGappaPredicate *out);
const lvGappaPredicate *gappa_pred_set_get(const lvGappaPredSet *set, int index);
void gappa_pred_set_clear(lvGappaPredSet *set);
```

### 33.4.2 传播配置

```c
typedef struct {
    int max_iterations;
    int max_backward_depth;
    double contraction_eps;
    int enable_backward;
} lvGappaPropagateConfig;
```

默认配置由：

```c
lvGappaPropagateConfig gappa_propagate_config_default(void);
```

提供。

### 33.4.3 正向传播与反向传播

正向传播从已知假设推导新的区间谓词：

```c
int gappa_propagate(const lvGappaPredSet *input_set,
                    lvGappaPredSet *output_set,
                    const lvGappaPropagateConfig *config);
```

反向传播从目标倒推所需假设：

```c
int gappa_propagate_backward(const lvGappaPredicate *goal,
                             const lvGappaPredSet *known_facts,
                             lvGappaPredSet *output_set,
                             const lvGappaPropagateConfig *config);
```

### 33.4.4 重写规则

```c
typedef struct {
    char match_pattern[256];
    char replace_pattern[256];
    char description[128];
} lvGappaRewriteRule;
```

```c
bool gappa_register_rewrite_rules(const lvGappaRewriteRule *rules,
                                  int rule_count);
```

重写规则用于化简谓词表达式，使传播过程能够识别更多可推导事实。

---

## 33.5 parser_safety.h —— 解析器安全

### 33.5.1 安全边界常量

```c
#define lv_MAX_INPUT_LENGTH  65536
#define lv_MAX_TOKEN_LENGTH  512
#define lv_MAX_TOKEN_COUNT   10000
#define lv_MAX_AST_DEPTH     256
#define lv_MAX_AST_NODES     10000
```

这些限制防止超大输入、token 爆炸、深层嵌套 AST 和内存耗尽。

### 33.5.2 解析器安全错误码

```c
#define lv_ERROR_PARSER_NULL_INPUT        130
#define lv_ERROR_PARSER_EMPTY_INPUT       131
#define lv_ERROR_PARSER_INPUT_TOO_LONG    132
#define lv_ERROR_PARSER_ILLEGAL_CHARS     133
#define lv_ERROR_PARSER_TOO_MANY_TOKENS   134
#define lv_ERROR_PARSER_DEPTH_EXCEEDED    135
#define lv_ERROR_PARSER_NODE_LIMIT        136
#define lv_ERROR_PARSER_TOKEN_TOO_LONG    137
#define lv_ERROR_PARSER_POOL_EXHAUSTED    138
```

### 33.5.3 输入校验与净化

```c
lvErrorCode lv_input_validate(const char *input, size_t len);
size_t lv_input_sanitize(char *input, size_t max_len);
bool lv_char_is_safe_ctrl(unsigned char c);
```

净化操作包括：
- Unicode 空白标准化；
- 换行符统一；
- 非允许控制字符替换为空格；
- 移除嵌入 null 字节；
- 首尾空白处理。

### 33.5.4 AST 与 token 检查

```c
lvErrorCode lv_check_ast_depth(int depth);
lvErrorCode lv_check_ast_node_count(int count);
lvErrorCode lv_check_token_length(size_t len);
```

这些接口应在解析器递归下降、AST 构造和词法扫描阶段持续调用。

---

## 33.6 path_type.h —— 路径类型系统

### 33.6.1 理论来源

路径类型系统借鉴 Arend 与同伦类型论 HoTT：

```text
等式证明 = 路径
p : a = b
```

Lv-00 将几何构造步骤解释为路径，路径拼接对应构造组合，路径消去对应沿构造传输属性。

### 33.6.2 路径方向与类型

```c
typedef enum {
    DIRECTION_FORWARD = 0,
    DIRECTION_BACKWARD = 1
} lvPathDirection;
```

```c
typedef enum {
    PATH_IDENTITY = 0,
    PATH_CONSTRUCTION = 1,
    PATH_COMPOSITE = 2,
    PATH_INVERSE = 3,
    PATH_TRANSPORT = 4,
    PATH_EQUIVALENCE = 5
} lvPathType;
```

### 33.6.3 区间与路径结构

```c
struct lvInterval {
    int interval_id;
    double left;
    double right;
    bool is_degenerate;
    char *label;
};
```

```c
struct lvPath {
    int path_id;
    lvPathType type;
    lvPathDirection direction;
    int endpoint_a;
    int endpoint_b;
    int interval_id;
    char *label;
    ConstraintGraph *construction;
    double (*path_func)(double t, void *user_data);
    void *func_user_data;
    bool is_constant;
    int source_step_id;
    int64_t created_at_us;
};
```

### 33.6.4 关键 API

```c
lvPathSystem *path_system_create(int path_capacity, int interval_capacity);
void path_system_destroy(lvPathSystem *sys);
int path_create(lvPathSystem *sys, int endpoint_a, int endpoint_b,
                const char *label,
                double (*path_func)(double t, void *user_data),
                void *user_data, int source_step);
int path_create_identity(lvPathSystem *sys, int endpoint_a, const char *label);
int path_create_inverse(lvPathSystem *sys, int path_id);
int path_compose(lvPathSystem *sys, int path_id_p, int path_id_q, const char *label);
int path_transport(lvPathSystem *sys, int path_id, int source_type_id,
                   lvTransportMode mode, void **transported);
int path_to_equality(lvPathSystem *sys, int path_id, ConstraintGraph **out_equality);
int path_from_construction(lvPathSystem *sys, int step_index, const char *label);
int path_to_constraint_graph(lvPathSystem *sys, int path_id, ConstraintGraph **out_constraint);
```

---

## 33.7 algebra_mode.h —— 代数模式构造引擎

### 33.7.1 设计来源

该模块借鉴：
- build123d：代数模式、变换链、不可变构造对象；
- CadQuery：Fluent API 与 Selector DSL。

### 33.7.2 工作平面与变换

```c
typedef enum {
    PLANE_XY,
    PLANE_XZ,
    PLANE_YZ,
    PLANE_CUSTOM
} lvPlane;
```

```c
typedef enum {
    TRANSFORM_TRANSLATE,
    TRANSFORM_ROTATE,
    TRANSFORM_SCALE,
    TRANSFORM_MIRROR,
    TRANSFORM_PROJECT
} lvTransformOp;
```

### 33.7.3 选择器系统

```c
typedef enum {
    SELECTOR_ALL,
    SELECTOR_BY_DIRECTION,
    SELECTOR_BY_TAG,
    SELECTOR_BY_TYPE,
    SELECTOR_NEAREST,
    SELECTOR_LARGEST,
    SELECTOR_SMALLEST,
    SELECTOR_PARALLEL_TO,
    SELECTOR_PERPENDICULAR_TO,
    SELECTOR_AT_LOCATION,
    SELECTOR_BY_INDEX,
    SELECTOR_COMPOSITE
} lvSelectorType;
```

方向选择符：

```c
typedef enum {
    SEL_DIR_GREATER,
    SEL_DIR_LESS,
    SEL_DIR_PARALLEL
} lvSelectorDirOp;
```

示例语义：
- `>Z`：选择法向朝 +Z 的面；
- `<X`：选择法向朝 -X 的面；
- `|Z`：选择与 Z 轴平行的边。

### 33.7.4 代数操作状态

```c
typedef enum {
    ALGEBRA_OK,
    ALGEBRA_OVERCONSTRAINED,
    ALGEBRA_AMBIGUOUS,
    ALGEBRA_INFEASIBLE,
    ALGEBRA_DEGENERATE,
    ALGEBRA_OUT_OF_MEMORY,
    ALGEBRA_INVALID_ARGUMENT
} AlgebraOpResult;
```

### 33.7.5 代数几何体

```c
typedef struct AlgebraicGeom {
    ConstraintGraph *graph;
    int current_entity;
    int plane;
    double transform[16];
    bool has_transform;
    int *history;
    int history_count;
    int history_capacity;
    struct AlgebraicGeom **snapshots;
    int snapshot_count;
    int snapshot_capacity;
    int *redo_stack;
    int redo_count;
    int redo_capacity;
    int id;
    char *name;
} AlgebraicGeom;
```

该结构将构造历史、变换链、约束图和当前实体整合为一个不可变/半不可变的代数构造上下文。

---

## 33.8 理论—代码对应关系

| 代码概念 | 理论/工程对应 | 说明 |
|----------|----------------|------|
| `lvGappaFormat` | IEEE 754 浮点格式 | 精度、指数位与舍入模式 |
| `lvGappaPredicate` | 浮点误差谓词 | 范围、绝对误差、相对误差、等式 |
| `gappa_propagate` | 抽象解释正向传播 | 从假设推出新事实 |
| `gappa_propagate_backward` | 目标导向反向传播 | 从目标倒推所需假设 |
| `lv_input_validate` | 解析器安全前置条件 | 防止恶意输入与资源攻击 |
| `lvPath` | HoTT 路径类型 | 等式证明即路径 |
| `path_transport` | coe / 路径消去 | 沿路径传输属性 |
| `AlgebraicGeom` | 代数模式构造对象 | 构造即运算，操作记录历史 |
| `lvSelector` | Fluent Selector DSL | 子实体选择与组合筛选 |

---

## 33.9 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [39_numerical_analysis.md](39_numerical_analysis.md) | 浮点误差、区间算术与 Gappa/FPTaylor |
| [29_inequality_approximation.md](29_inequality_approximation.md) | Herbie/FPTaylor 与误差评估 |
| [40_formula_dsl_ga.md](40_formula_dsl_ga.md) | 公式 DSL、词法分析与表达式规范化 |
| [06_unify.md](06_unify.md) | 合一与等式检查 |
| [26_interactive_geometry.md](26_interactive_geometry.md) | 交互几何与构造过程 |

---

## 33.10 版本历史

- **v5.0.0**
  - 补全文档化：Gappa DSL、谓词传播、解析器安全、路径类型与代数模式构造。
  - 明确浮点证明、输入安全与构造路径之间的工程边界。

- **v3.4.0**
  - 引入 Gappa 风格 DSL 和谓词传播。

- **v3.3.0**
  - 引入路径类型系统和代数模式构造引擎。
