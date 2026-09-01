# Lv-00 API 完整参考

> **版本**: 1.1.0  
> **最后更新**: 2026-07-23  
> **适用范围**: Lv-00 公共 API 完整参考

---

## 目录

0. [lv 语言解析器](#0-lv-语言解析器)
1. [核心 API](#1-核心-api)
2. [符号坐标系统](#2-符号坐标系统)
3. [约束图系统](#3-约束图系统)
4. [求解引擎](#4-求解引擎)
5. [证明系统](#5-证明系统)
6. [预设模块系统](#6-预设模块系统)
7. [函数块系统](#7-函数块系统)
8. [类型系统](#8-类型系统)
9. [流式输出系统](#9-流式输出系统)
10. [配置与内存管理](#10-配置与内存管理)
11. [错误处理](#11-错误处理)

---

## 0. lv 语言解析器

.v1.9.0 新增 — 解析 .lv 语言文件的完整管线：词法分析 → AST 构建 → 语法解析 → 语义分析 → 引擎加载。

### 0.1 词法分析器 (lv_lexer)

```c
#include "lv/lv_lexer.h"
```

**类型定义**:

| 类型 | 说明 |
|------|------|
| `LvTokenType` | 75 种 Token 类型（关键字/字面量/运算符/分隔符） |
| `LvSourceLoc` | 源码位置（行/列/偏移） |
| `LvToken` | 词法单元（类型 + 位置 + 源文本指针） |
| `LvLexer` | 词法分析器状态（不透明结构体） |

**核心函数**:

| 函数 | 说明 |
|------|------|
| `LvLexer *lv_lexer_create(const char *source, size_t source_len)` | 创建词法分析器 |
| `void lv_lexer_destroy(LvLexer *lexer)` | 销毁词法分析器 |
| `LvToken lv_lexer_next(LvLexer *lexer)` | 获取下一个 token |
| `LvToken lv_lexer_peek(LvLexer *lexer, int lookahead)` | 窥视后续 token（不消费） |
| `LvSourceLoc lv_lexer_get_loc(const LvLexer *lexer)` | 获取当前位置 |
| `const char *lv_token_type_name(LvTokenType type)` | token 类型 → 字符串 |
| `size_t lv_token_text(const LvToken *token, char *buf, size_t buf_size)` | 提取 token 文本 |

### 0.2 AST 节点 (lv_ast)

```c
#include "lv/lv_ast.h"
```

**类型定义**:

| 类型 | 说明 |
|------|------|
| `LvAstNodeType` | 28 种 AST 节点类型（程序/声明/语句/表达式） |
| `LvEntityType` | 12 种实体类型（Point/Line/Circle/.../Proof） |
| `LvAstNode` | AST 节点（tagged union，含子节点链表） |

**核心函数**:

| 函数 | 说明 |
|------|------|
| `LvAstNode *lv_ast_create(LvAstNodeType type, LvSourceLoc loc)` | 创建节点 |
| `LvAstNode *lv_ast_create_ident(LvSourceLoc loc, const char *name)` | 标识符节点 |
| `LvAstNode *lv_ast_create_int(LvSourceLoc loc, long long value)` | 整数节点 |
| `LvAstNode *lv_ast_create_rational(LvSourceLoc loc, long long num, long long den)` | 有理数节点 |
| `LvAstNode *lv_ast_create_decimal(LvSourceLoc loc, double value)` | 小数节点 |
| `LvAstNode *lv_ast_create_string(LvSourceLoc loc, const char *value)` | 字符串节点 |
| `LvAstNode *lv_ast_create_bool(LvSourceLoc loc, int value)` | 布尔节点 |
| `LvAstNode *lv_ast_create_call(LvSourceLoc loc, const char *func_name, LvAstNode *args)` | 函数调用节点 |
| `LvAstNode *lv_ast_create_binary(LvSourceLoc loc, const char *op, LvAstNode *left, LvAstNode *right)` | 二元运算节点 |
| `LvAstNode *lv_ast_create_unary(LvSourceLoc loc, const char *op, LvAstNode *operand)` | 一元运算节点 |
| `LvAstNode *lv_ast_create_compare(LvSourceLoc loc, const char *op, LvAstNode *left, LvAstNode *right)` | 比较运算节点 |
| `void lv_ast_append_child(LvAstNode *parent, LvAstNode *child)` | 追加子节点 |
| `void lv_ast_destroy(LvAstNode *node)` | 销毁 AST 树 |
| `void lv_ast_print(const LvAstNode *node, int indent)` | 调试打印 |
| `const char *lv_entity_type_name(LvEntityType type)` | 实体类型 → 字符串 |
| `LvEntityType lv_entity_type_from_token(LvTokenType tok)` | token → 实体类型 |

### 0.3 语法解析器 (lv_parser)

```c
#include "lv/lv_parser.h"
```

**类型定义**:

| 类型 | 说明 |
|------|------|
| `LvParser` | 递归下降解析器状态 |
| `LvParseError` | 解析错误（位置 + 消息） |
| `LvParseResult` | 解析结果（AST + 错误列表） |

**核心函数**:

| 函数 | 说明 |
|------|------|
| `LvParser *lv_parser_create(LvLexer *lexer)` | 创建解析器 |
| `void lv_parser_destroy(LvParser *parser)` | 销毁解析器 |
| `LvParseResult lv_parser_parse_program(LvParser *parser)` | 解析完整程序 |

### 0.4 语义分析 (lv_sema)

```c
#include "lv/lv_sema.h"
```

**类型定义**:

| 类型 | 说明 |
|------|------|
| `LvSemanticType` | 语义类型枚举（Point/Line/Scalar/Bool/Proposition 等） |
| `LvSemaContext` | 语义分析上下文（符号表 + 错误列表） |

**核心函数**:

| 函数 | 说明 |
|------|------|
| `LvSemaContext *lv_sema_create(void)` | 创建语义分析上下文 |
| `void lv_sema_destroy(LvSemaContext *ctx)` | 销毁语义分析上下文 |
| `bool lv_sema_analyze(LvSemaContext *ctx, LvAstNode *ast)` | 分析 AST（符号解析 + 类型检查） |
| `int lv_sema_error_count(const LvSemaContext *ctx)` | 获取错误数量 |
| `const char *lv_sema_error(const LvSemaContext *ctx, int index)` | 获取错误消息 |
| `const char *lv_semantic_type_name(LvSemanticType type)` | 语义类型 → 字符串 |

### 0.5 文件加载与引擎集成 (lv_loader)

```c
#include "lv/lv_loader.h"
```

**核心函数**:

| 函数 | 说明 |
|------|------|
| `LvParseResult lv_load_file(const char *filepath)` | 加载并解析 .lv 文件 |
| `bool lv_apply_parse_result(lvEngine *engine, const LvParseResult *result)` | 将解析结果应用到引擎 |

### 0.6 使用示例

```c
#include "lv/lv_lexer.h"
#include "lv/lv_parser.h"
#include "lv/lv_sema.h"
#include "lv/lv_loader.h"

/* 方式一：直接从文件加载 */
LvParseResult result = lv_load_file("example.lv");
if (result.error_count == 0) {
    /* 应用到引擎 */
    lvEngine *engine = lv_engine_create();
    lv_apply_parse_result(engine, &result);
    /* ... */
    lv_ast_destroy(result.ast);
    lv_engine_destroy(engine);
}

/* 方式二：分步执行 */
const char *src = "Point A, B, C; Constraint collinear(A,B,C); Prove true;";
LvLexer *lexer = lv_lexer_create(src, strlen(src));
LvParser *parser = lv_parser_create(lexer);
LvParseResult pr = lv_parser_parse_program(parser);
/* 语义分析 */
LvSemaContext *sema = lv_sema_create();
lv_sema_analyze(sema, pr.ast);
/* 清理 */
lv_sema_destroy(sema);
lv_parser_destroy(parser);
lv_lexer_destroy(lexer);
lv_ast_destroy(pr.ast);
```

## 1. 核心 API

### 1.1 系统生命周期

#### lv_context_create

```c
lvContext *lv_context_create(void);
```

创建 Lv-00 上下文。

**返回值**:
- 非 NULL - 创建成功
- NULL - 创建失败

**说明**: 必须在调用任何其他 Lv-00 API 之前调用。可以多次调用，后续调用若上下文有效则直接返回原上下文。

**示例**:
```c
lvContext *ctx = lv_context_create();
if (!ctx) {
    fprintf(stderr, "Lv-00 context creation failed\n");
    return EXIT_FAILURE;
}
```

---

#### lv_context_destroy

```c
void lv_context_destroy(lvContext *ctx);
```

销毁 Lv-00 上下文，释放所有资源。

**说明**: 调用后不应再使用任何 Lv-00 API。

**示例**:
```c
lv_context_destroy(ctx);
```

---

#### 上下文有效性检查

```c
/* 上下文有效性通过 ctx != NULL 判断 */
```

检查上下文是否有效。

**返回值**:
- `true` - 上下文有效（ctx != NULL）
- `false` - 上下文无效（ctx == NULL）

---

#### lv_context_create（替代旧版）

```c
lvContext *lv_context_create(void);
```

创建并初始化上下文实例。

**返回值**: 上下文指针，失败返回 NULL

**说明**: 上下文是 Lv-00 的核心工作单元，持有约束图、符号坐标、重写规则、证明树等所有状态。

**示例**:
```c
lvContext *ctx = lv_context_create();
if (!ctx) {
    fprintf(stderr, "Context creation failed\n");
    return EXIT_FAILURE;
}
```

---

#### lv_context_destroy（替代旧版）

```c
void lv_context_destroy(lvContext *ctx);
```

销毁上下文实例。

**参数**:
- `ctx` - 上下文指针（可为 NULL，此时函数无操作）

**说明**: 销毁上下文后，所有从该上下文获取的指针均失效。

---

### 1.2 版本信息

#### lv_get_version_string

```c
const char *lv_get_version_string(void);
```

获取版本字符串。

**返回值**: 版本字符串（如 "1.1.0"），静态常量，无需释放

---

#### lv_get_version_info

```c
bool lv_get_version_info(lvVersionInfo *info);
```

获取详细版本信息。

**参数**:
- `info` - 指向 lvVersionInfo 结构体的指针

**返回值**:
- `true` - 成功
- `false` - 失败（info 为 NULL 时）

**lvVersionInfo 结构**:
```c
typedef struct lvVersionInfo {
    int         major;          /* 主版本号 */
    int         minor;          /* 次版本号 */
    int         patch;          /* 补丁版本号 */
    const char *version_string; /* 完整版本字符串 */
    const char *platform;       /* 编译平台名称 */
    const char *compiler;       /* 编译器名称 */
    const char *arch;           /* 目标架构 */
    const char *build_date;     /* 构建日期 */
    const char *build_time;     /* 构建时间 */
} lvVersionInfo;
```

---

#### lv_check_version_compat

```c
bool lv_check_version_compat(void);
```

检查运行时版本与编译时头文件版本的兼容性。

**返回值**:
- `true` - 兼容（主版本号匹配）
- `false` - 不兼容

---

### 1.3 系统信息

#### lv_get_system_info

```c
int lv_get_system_info(char *buf, size_t size);
```

获取系统信息字符串。

**参数**:
- `buf` - 输出缓冲区
- `size` - 缓冲区大小（字节），建议至少 1024

**返回值**: 实际写入的字符数，缓冲区不足时返回所需大小

---

#### lv_health_check

```c
int lv_health_check(void);
```

检查系统健康状况。

**返回值**: 健康评分（0~100），未初始化时返回 0

---

## 2. 符号坐标系统

### 2.1 坐标创建

#### symbolic_coord_create_rational

```c
SymbolicCoord* symbolic_coord_create_rational(int64_t numer, uint64_t denom);
```

创建有理数坐标。

**参数**:
- `numer` - 分子（可为负数）
- `denom` - 分母（必须 > 0）

**返回值**: 符号坐标指针，失败返回 NULL

---

#### symbolic_coord_create_algebraic

```c
SymbolicCoord* symbolic_coord_create_algebraic(mpz_poly_t *poly, double left, double right);
```

创建代数坐标。

**参数**:
- `poly` - 最小多项式
- `left` - 隔离区间左边界
- `right` - 隔离区间右边界

---

#### symbolic_coord_create_quadratic

```c
SymbolicCoord* symbolic_coord_create_quadratic(Rational *a, Rational *b, unsigned int n);
```

创建二次扩张坐标（a + b*sqrt(n)）。

---

#### symbolic_coord_create_transcendental

```c
SymbolicCoord* symbolic_coord_create_transcendental(const char *name);
```

创建超越数坐标（如 "pi", "e"）。

---

### 2.2 坐标销毁

#### symbolic_coord_destroy

```c
void symbolic_coord_destroy(SymbolicCoord *coord);
```

销毁符号坐标。

---

### 2.3 算术运算

| 函数 | 说明 |
|------|------|
| `symbolic_coord_add(a, b)` | 加法 |
| `symbolic_coord_subtract(a, b)` | 减法 |
| `symbolic_coord_multiply(a, b)` | 乘法 |
| `symbolic_coord_divide(a, b)` | 除法 |
| `symbolic_coord_negate(coord)` | 取相反数 |
| `symbolic_coord_pow(base, exp)` | 幂运算 |
| `symbolic_coord_sqrt(coord)` | 平方根 |

**注意**: 所有运算返回新分配的符号坐标，调用者负责销毁。

---

### 2.4 比较与工具

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `symbolic_coord_compare(a, b)` | 比较 | 负/零/正 |
| `symbolic_coord_is_zero(coord)` | 是否为零 | bool |
| `symbolic_coord_is_positive(coord)` | 是否为正 | bool |
| `symbolic_coord_is_negative(coord)` | 是否为负 | bool |
| `symbolic_coord_copy(src)` | 深拷贝 | SymbolicCoord* |
| `symbolic_coord_to_double(coord)` | 转 double | double |
| `symbolic_coord_hash(coord)` | 哈希值 | uint64_t |

---

### 2.5 序列化

| 函数 | 说明 |
|------|------|
| `symbolic_coord_serialize(coord)` | 序列化为字符串 |
| `rational_serialize(r)` | 有理数序列化 |
| `rational_parse(str)` | 解析 "3/4" 或 "1.5" |

---

## 3. 约束图系统

### 3.1 几何构造

#### lv_add_point

```c
int lv_add_point(lvContext *ctx,
    int64_t x_num, uint64_t x_den,
    int64_t y_num, uint64_t y_den);
```

创建有理数坐标的点。

**参数**:
- `engine` - 引擎实例
- `x_num`, `x_den` - X 坐标分子/分母
- `y_num`, `y_den` - Y 坐标分子/分母

**返回值**: 新节点的 ID（>= 0），失败返回 -1

**示例**:
```c
int origin = lv_add_point(engine, 0, 1, 0, 1);   /* (0, 0) */
int p = lv_add_point(engine, 3, 2, 11, 4);       /* (1.5, 2.75) */
```

---

#### lv_add_point_i

```c
static inline int lv_add_point_i(lvContext *ctx, long long x, long long y);
```

添加整数坐标点（便捷函数）。

**说明**: 等价于 `lv_add_point(engine, x, 1, y, 1)`

---

#### lv_add_line_segment

```c
int lv_add_line_segment(lvContext *ctx, int point1_id, int point2_id);
```

创建连接两点的线段。

**参数**:
- `engine` - 引擎实例
- `point1_id` - 端点1节点ID
- `point2_id` - 端点2节点ID

**返回值**: 新线段的节点 ID（>= 0），失败返回 -1

---

#### lv_add_constraint_incidence

```c
bool lv_add_constraint_incidence(lvContext *ctx, int point_id, int line_id);
```

添加关联约束（点属于线段）。

**参数**:
- `engine` - 引擎实例
- `point_id` - 点节点ID
- `line_id` - 线段/区域节点ID

**返回值**:
- `true` - 成功
- `false` - 失败

---

### 3.2 约束图操作

#### lv_constraint_graph_create

```c
lvConstraintGraph *lv_constraint_graph_create(void);
```

创建空约束图。

---

#### lv_constraint_graph_destroy

```c
void lv_constraint_graph_destroy(lvConstraintGraph *graph);
```

销毁约束图。

---

#### lv_constraint_graph_add_node

```c
int lv_constraint_graph_add_node(lvConstraintGraph *graph, GeometryEntity *entity);
```

添加节点到约束图。

---

#### lv_constraint_graph_add_edge

```c
bool lv_constraint_graph_add_edge(lvConstraintGraph *graph, int node1, int node2, ConstraintType type);
```

添加边到约束图。

---

## 4. 求解引擎

### 4.1 归一化

#### lv_normalize

```c
NormalizationResult *lv_normalize(lvEngine *engine, bool scope_aware);
```

执行图归一化。

**参数**:
- `engine` - 引擎实例
- `scope_aware` - 是否考虑命名空间范围

**返回值**: 归一化结果，失败返回 NULL

**说明**: 调用者负责通过 `normalization_result_destroy` 释放返回值。

---

### 4.2 求解

#### lv_solve

```c
EngineSolveResult lv_solve(lvEngine *engine);
```

执行求解流水线（重写-求解协作）。

**参数**:
- `engine` - 引擎实例

**返回值**: 求解结果状态码

**EngineSolveResult 枚举**（见 `lv/engine.h`）:
```c
typedef enum {
    ENGINE_SOLVE_OK,      /* 求解成功 */
    ENGINE_SOLVE_CONFLICT,/* 约束矛盾 */
    ENGINE_SOLVE_TIMEOUT, /* 计算超时 */
    ENGINE_SOLVE_ERROR    /* 求解器错误 */
} EngineSolveResult;
```

---

### 4.3 高级求解

求解参数通过引擎配置 API 控制（如 `lv_config_set_int("rewrite.step_limit", ...)`、
`engine_set_rewrite_step_limit()` 等），无需单独的"带选项求解"入口：

```c
#include "lv/lv.h"

lvEngine *engine = lv_engine_create();
lv_config_set_int("rewrite.step_limit", 5000); /* 重写步数上限 */
lv_config_set_int("solver.timeout_ms", 60000); /* 求解超时 */
EngineSolveResult result = lv_solve(engine);
lv_engine_destroy(engine);
```

---

## 5. 证明系统

### 5.1 命题创建

命题（Proposition）表示一个可证明的数学断言。命题通过
`proposition_create()` 创建（见 `lv/proof.h`），并通过
`proposition_set_*` / `proposition_add_sub_proposition()` 系列函数
设置内容与结构；复合命题（合取、析取、蕴含等）通过
`PropositionType` 枚举区分：

```c
#include "lv/proof.h"

/* 创建一个原子命题 */
Proposition *p = proposition_create(1, PROPOSITION_TYPE_ATOMIC);

/* 创建合取命题 p1 ∧ p2 */
Proposition *p1 = proposition_create(2, PROPOSITION_TYPE_ATOMIC);
Proposition *p2 = proposition_create(3, PROPOSITION_TYPE_ATOMIC);
Proposition *conj = proposition_create(4, PROPOSITION_TYPE_CONJUNCTION);
proposition_add_sub_proposition(conj, p1);
proposition_add_sub_proposition(conj, p2);
```

**PropositionType 枚举**（节选，见 `lv/proof.h`）:
```c
typedef enum {
    PROPOSITION_TYPE_ATOMIC,      /* 原子命题 */
    PROPOSITION_TYPE_CONJUNCTION, /* 合取 ∧ */
    PROPOSITION_TYPE_DISJUNCTION, /* 析取 ∨ */
    PROPOSITION_TYPE_IMPLICATION, /* 蕴含 → */
    PROPOSITION_TYPE_NEGATION,    /* 否定 ¬ */
    PROPOSITION_TYPE_BOTTOM       /* 矛盾 ⊥ */
} PropositionType;
```

> 注：便捷证明场景下通常无需手工构造命题——直接使用 `lv_prove()`
> 传入 DSL 目标文本即可，解析器会自动生成命题与约束。

---

### 5.2 证明执行

#### lv_prove

```c
int lv_prove(lvContext *ctx, const char *goal);
```

执行自动证明。将 goal 文本解析为约束图，再调用引擎的重写-求解
流水线完成推理。

**参数**:
- `ctx` - 上下文实例（必须处于 IDLE 或 COMPLETE 状态）
- `goal` - 证明目标的 DSL 文本描述（可为 NULL，表示使用上下文中已有的约束图）

**返回值**:
- `0` - 证明成功
- `-1` - 参数无效（ctx 为 NULL）
- `-2` - 上下文状态不合法
- `-3` - 解析阶段失败
- `-4` - 推理阶段失败（矛盾、超时或熔断）

**示例**:
```c
#include "lv/lv.h"
#include "lv/lv_convenience.h"
#include <stdio.h>

lvContext *ctx = lv_context_create();
if (lv_prove(ctx, "triangle ABC is equilateral") == 0) {
    printf("证明成功\n");
}
lv_context_destroy(ctx);
```

> 需要针对性证明策略（反证法、Groebner 基等）时，可在 goal 文本中
> 声明策略关键字，或通过加载对应预设（见第 6 章）组合实现。

---

### 5.3 证明验证与统计

证明对象（`lvProofObject`，见 `lv/proof_compiler.h`）保存完整的证明链，
可用于机器复核：

#### lv_proof_object_is_valid

```c
bool lv_proof_object_is_valid(const lvProofObject *obj);
```

检查证明对象的结构是否有效。

---

#### lv_proof_object_verify

```c
bool lv_proof_object_verify(const lvProofObject *obj);
```

复核证明对象的证明链是否闭合（每一步的前提都得到满足）。

---

#### lv_proof_object_get_step_count

```c
int lv_proof_object_get_step_count(const lvProofObject *obj);
```

获取证明步骤数。

**示例**:
```c
#include "lv/lv.h"
#include "lv/proof_compiler.h"

lvProofObject *obj = lv_proof_object_create();
/* ... 填充证明步骤 ... */
if (lv_proof_object_verify(obj)) {
    printf("步骤数: %d\n", lv_proof_object_get_step_count(obj));
}
lv_proof_object_destroy(obj);
```

---

### 5.4 证明导出

证明对象可通过证明编译器（`lvProofCompiler`）导出为多种格式，
或直接写入文件：

#### lv_proof_export_to_file

```c
bool lv_proof_export_to_file(const lvProofObject *proof, const lvProofTrace *trace,
                             lvOutputFormat format, const char *filename);
```

将证明写入文件。

**lvOutputFormat 枚举**:
```c
typedef enum {
    OUTPUT_FORMAT_JSON,    /* JSON 格式 */
    OUTPUT_FORMAT_LATEX,   /* LaTeX 格式 */
    OUTPUT_FORMAT_TIKZ,    /* TikZ 几何图形 */
    OUTPUT_FORMAT_TEXT,    /* 纯文本格式 */
    OUTPUT_FORMAT_XML,     /* XML 格式 */
    OUTPUT_FORMAT_GRAPHVIZ /* Graphviz 格式 */
} lvOutputFormat;
```

**示例**:
```c
#include "lv/lv.h"
#include "lv/proof_compiler.h"

/* LaTeX 导出 */
lv_proof_export_to_file(obj, trace, OUTPUT_FORMAT_LATEX, "proof.tex");
/* JSON 导出 */
lv_proof_export_to_file(obj, trace, OUTPUT_FORMAT_JSON, "proof.json");
```

---

#### lv_proof_compiler_to_latex / lv_proof_compiler_to_json / lv_proof_compiler_to_tikz / lv_proof_compiler_to_text

```c
lvProofCompiler *lv_proof_compiler_create(const lvCompilerConfig *config);
char *lv_proof_compiler_to_latex(const lvProofObject *proof, const char *language);
char *lv_proof_compiler_to_json(const lvProofObject *proof, const lvProofTrace *trace);
char *lv_proof_compiler_to_tikz(const lvProofObject *proof);
char *lv_proof_compiler_to_text(const lvProofObject *proof, const char *language);
```

将证明对象编译为对应格式的字符串（调用者负责释放返回的字符串）。
Lean/Coq 脚本不在内置输出格式之列；需要时可通过
`lv_proof_compiler_to_text()` 导出为纯文本证明脚本后再转换。

---

## 6. 预设模块系统

### 6.1 预设加载

#### lv_preset_load

```c
int lv_preset_load(lvContext *ctx, const char *name);
```

加载预设模块，将指定名称的预设注册到上下文中。

**参数**:
- `ctx` - 上下文实例
- `name` - 预设名称（如 "euclidean_geometry"）

**返回值**:
- `0` - 成功
- `-1` - 参数无效（ctx 或 name 为 NULL）
- `-2` - 预设库未初始化
- `-3` - 指定名称的预设不存在
- `-4` - 内存分配失败

---

#### lv_preset_unload

```c
int lv_preset_unload(lvContext *ctx, const char *name);
```

卸载预设模块（从上下文中移除加载标记）。

**返回值**:
- `0` - 成功
- `-1` - 参数无效
- `-3` - 上下文中未找到该预设的加载记录

---

### 6.2 预设应用

#### lv_preset_apply

```c
int lv_preset_apply(lvContext *ctx, const char *name);
```

将指定预设实例化并应用到当前约束图（预设必须已通过
`lv_preset_load()` 加载）。

**参数**:
- `ctx` - 上下文实例（应处于 IDLE 或 PARSING 状态）
- `name` - 预设名称

**返回值**:
- `0` - 应用成功
- `-1` - 参数无效
- `-2` - 上下文状态不允许应用预设
- `-3` - 指定预设未加载
- `-4` - 实例化失败

**示例**:
```c
#include "lv/lv.h"
#include "lv/lv_convenience.h"

lvContext *ctx = lv_context_create();
lv_preset_load(ctx, "euclidean_geometry");
lv_preset_apply(ctx, "pythagorean_theorem");
if (lv_prove(ctx, "triangle ABC is right-angled at A") == 0) {
    printf("验证成功\n");
}
lv_preset_unload(ctx, "euclidean_geometry");
lv_context_destroy(ctx);
```

---

### 6.3 预设查询

预设库的注册与查询通过预设函数块系统公开接口完成
（见 `lv/func_block_preset.h`）：

```c
bool func_block_preset_library_init(void);   /* 初始化预设库 */
const PresetMetadata *func_block_preset_get_metadata(const char *preset_name); /* 查询预设元数据 */
int func_block_preset_count(void);           /* 预设总数 */
bool func_block_preset_exists(const char *preset_name); /* 判断预设是否存在 */
```

对外的高层预设操作入口为 `lv_preset_load()`、`lv_preset_apply()`
与 `lv_preset_unload()`。

---

## 7. 函数块系统

### 7.1 函数块创建

#### lv_fb_create

```c
FuncBlock *lv_fb_create(const char *name, int arity);
```

创建函数块。

**参数**:
- `name` - 函数块名称
- `arity` - 参数个数

**返回值**: 函数块指针

---

#### lv_fb_destroy

```c
void lv_fb_destroy(FuncBlock *fb);
```

销毁函数块。

---

### 7.2 函数块定义

#### lv_fb_define

```c
bool lv_fb_define(FuncBlock *fb, const char *definition);
```

定义函数块体。

**参数**:
- `fb` - 函数块
- `definition` - 定义字符串（Lv-00 DSL）

---

#### lv_fb_define_c

```c
bool lv_fb_define_c(FuncBlock *fb, lvFBFunction func);
```

使用 C 函数定义函数块。

---

### 7.3 函数块应用

#### lv_fb_apply

```c
GeometryEntity *lv_fb_apply(FuncBlock *fb, ...);
```

应用函数块。

**示例**:
```c
FuncBlock *midpoint = lv_fb_create("midpoint", 2);
lv_fb_define(midpoint, "return point((A.x+B.x)/2, (A.y+B.y)/2);");
Point *M = (Point *)lv_fb_apply(midpoint, A, B);
```

---

## 8. 类型系统

### 8.1 类型查询

#### lv_type_of

```c
lvType lv_type_of(const GeometryEntity *entity);
```

获取实体类型。

**lvType 枚举**:
```c
typedef enum {
    lv_TYPE_POINT,
    lv_TYPE_LINE,
    lv_TYPE_CIRCLE,
    lv_TYPE_SEGMENT,
    lv_TYPE_ANGLE,
    lv_TYPE_TRIANGLE,
    lv_TYPE_POLYGON,
    lv_TYPE_CONSTRAINT,
    lv_TYPE_PROPOSITION,
    lv_TYPE_PROOF
} lvType;
```

---

#### lv_type_check

```c
bool lv_type_check(const GeometryEntity *entity, lvType expected);
```

类型检查。

---

### 8.2 类型转换
> **K5 标注（2026-09-01）**：本章 lv_type_check / lv_type_cast_* 等 API **不存在**（虚构），真实类型 API 见 type_system.h / symbolic_coord.h。

#### lv_type_cast_point

```c
Point *lv_type_cast_point(GeometryEntity *entity);
```

转换为点类型。

---

#### lv_type_cast_line

```c
Line *lv_type_cast_line(GeometryEntity *entity);
```

转换为线类型。

---

## 9. 流式输出系统
> **K5 标注（2026-09-01）**：本章 lv_stream_* API **不存在**（虚构），真实流式系统见 stream.h（StreamContext / stream_* API）。

### 9.1 流创建

#### lv_stream_create

```c
lvStream *lv_stream_create(lvContext *ctx, StreamMode mode);
```

创建输出流。

**StreamMode 枚举**:
```c
typedef enum {
    STREAM_MODE_BUFFERED,   /* 缓冲模式 */
    STREAM_MODE_REALTIME    /* 实时模式 */
} StreamMode;
```

---

#### lv_stream_destroy

```c
void lv_stream_destroy(lvStream *stream);
```

销毁流。

---

### 9.2 事件处理

#### lv_stream_set_event_handler

```c
void lv_stream_set_event_handler(
    lvStream *stream,
    StreamEventType event_type,
    StreamEventHandler handler,
    void *user_data
);
```

设置事件处理器。

**StreamEventType 枚举**:
```c
typedef enum {
    STREAM_EVENT_PROOF_STEP,      /* 证明步骤 */
    STREAM_EVENT_CONSTRAINT_ADD,  /* 约束添加 */
    STREAM_EVENT_SOLVE_PROGRESS,  /* 求解进度 */
    STREAM_EVENT_ERROR            /* 错误 */
} StreamEventType;
```

---

## 10. 配置与内存管理

### 10.1 配置 API

| 函数 | 说明 |
|------|------|
| `lv_config_get_int(key, def)` | 获取整数配置 |
| `lv_config_get_bool(key, def)` | 获取布尔配置 |
| `lv_config_get_double(key, def)` | 获取浮点配置 |
| `lv_config_get_string(key, def)` | 获取字符串配置 |
| `lv_config_set_int(key, val)` | 设置整数配置 |
| `lv_config_set_bool(key, val)` | 设置布尔配置 |
| `lv_config_set_double(key, val)` | 设置浮点配置 |
| `lv_config_set_string(key, val)` | 设置字符串配置 |

**常用配置项**:
```
rewrite.step_limit       - 重写步数上限（默认 1000）
solver.timeout_ms        - 求解超时（毫秒，默认 30000）
solver.allow_approximation - 允许近似（默认 false）
memory.limit_bytes       - 内存上限（字节，默认 0 无限制）
log.level                - 日志级别（0-4，默认 2）
```

---

### 10.2 内存管理

#### lv_get_memory_stats

```c
void lv_get_memory_stats(MemoryStats *stats);
```

获取内存统计（见 `lv/lv_utils.h`）。

**MemoryStats 结构**:
```c
typedef struct {
    size_t total_allocated;  /* 总分配内存 */
    size_t total_freed;      /* 总释放内存 */
    size_t current_used;     /* 当前使用内存 */
    size_t peak_used;        /* 峰值使用内存 */
    size_t allocation_count; /* 分配次数 */
    size_t free_count;       /* 释放次数 */
} MemoryStats;
```

---

#### lv_reset_memory_stats

```c
void lv_reset_memory_stats(void);
```

重置内存统计。

---

#### lv_set_memory_limit

```c
void lv_set_memory_limit(size_t limit);
```

设置内存上限（字节，0 表示无限制）。

---

#### lv_get_memory_limit

```c
size_t lv_get_memory_limit(void);
```

获取内存上限。

---

## 11. 错误处理

### 11.1 错误码

| 错误码 | 值 | 含义 |
|--------|-----|------|
| `lv_OK` | 0 | 成功 |
| `lv_ERR_PARSE` | 1 | 输入解析失败 |
| `lv_ERR_MEMORY` | 2 | 内存不足 |
| `lv_ERR_INVALID_ARG` | 3 | 无效参数 |
| `lv_ERR_TIMEOUT` | 4 | 计算超时 |
| `lv_ERR_STATE` | 5 | 状态机违规 |
| `lv_ERR_OVERFLOW` | 6 | 数值溢出 |
| `lv_ERR_NOT_INIT` | 7 | 系统未初始化 |
| `lv_ERR_INCONSISTENT` | 8 | 约束矛盾 |
| `lv_ERR_UNDER_CONSTRAINED` | 9 | 欠约束 |
| `lv_ERR_OVER_CONSTRAINED` | 10 | 过约束 |

---

### 11.2 错误查询

#### lv_get_last_error_code

```c
lvErrorCode lv_get_last_error_code(void);
```

获取最后错误码。

---

#### lv_get_last_error_message

```c
const char *lv_get_last_error_message(void);
```

获取最后错误描述。

---

#### lv_clear_error

```c
void lv_clear_error(void);
```

清除错误状态。

---

### 11.3 日志控制

#### lv_set_log_level

```c
void lv_set_log_level(int level);
```

设置日志级别。

| 级别 | 含义 |
|------|------|
| 0 | 禁用所有日志 |
| 1 | 仅错误 |
| 2 | 错误 + 警告 |
| 3 | 错误 + 警告 + 信息 |
| 4 | 所有日志（含调试） |

---

#### lv_get_log_level

```c
int lv_get_log_level(void);
```

获取当前日志级别。

---

## 参考文档

- [架构手册](archived/ARCHITECTURE_MANUAL.md)
- [入门教程](TUTORIAL.md)
- [API 快速入门](API_QUICKSTART.md)
- [语言规范](lv_LANGUAGE_SPEC.md)
