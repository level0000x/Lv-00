# Lv-00 API 完整参考

> **版本**: 1.1.0  
> **最后更新**: 2026-06-27  
> **适用范围**: Lv-00 公共 API 完整参考

---

## 目录

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
NormalizationResult *lv_normalize(lvContext *ctx, bool scope_aware);
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
EngineSolveResult lv_solve(lvContext *ctx);
```

执行求解流水线。

**参数**:
- `engine` - 引擎实例

**返回值**: 求解结果状态码

**EngineSolveResult 枚举**:
```c
typedef enum {
    lv_SOLVE_SUCCESS = 0,        /* 求解成功 */
    lv_SOLVE_TIMEOUT,            /* 计算超时 */
    lv_SOLVE_INCONSISTENT,       /* 约束矛盾 */
    lv_SOLVE_UNDER_CONSTRAINED,  /* 欠约束 */
    lv_SOLVE_OVER_CONSTRAINED,   /* 过约束 */
    lv_SOLVE_MEMORY_ERROR,       /* 内存不足 */
    lv_SOLVE_INTERNAL_ERROR      /* 内部错误 */
} EngineSolveResult;
```

---

### 4.3 高级求解

#### lv_solve_with_options

```c
EngineSolveResult lv_solve_with_options(
    lvContext *ctx,
    const SolverOptions *options
);
```

使用自定义选项求解。

**SolverOptions 结构**:
```c
typedef struct SolverOptions {
    uint32_t timeout_ms;           /* 超时时间（毫秒），0 表示无限制 */
    uint32_t max_iterations;       /* 最大迭代次数 */
    bool enable_groebner;          /* 启用 Groebner 基 */
    bool enable_smt;               /* 启用 SMT 后端 */
    bool enable_atp;               /* 启用 ATP */
    uint8_t log_level;             /* 日志级别 */
} SolverOptions;
```

---

## 5. 证明系统

### 5.1 命题创建

#### lv_proposition_eq

```c
Proposition *lv_proposition_eq(Expr *lhs, Expr *rhs);
```

创建相等命题（lhs = rhs）。

---

#### lv_proposition_lt

```c
Proposition *lv_proposition_lt(Expr *lhs, Expr *rhs);
```

创建小于命题（lhs < rhs）。

---

#### lv_proposition_and

```c
Proposition *lv_proposition_and(Proposition *p1, Proposition *p2);
```

创建合取命题（p1 ∧ p2）。

---

#### lv_proposition_or

```c
Proposition *lv_proposition_or(Proposition *p1, Proposition *p2);
```

创建析取命题（p1 ∨ p2）。

---

#### lv_proposition_not

```c
Proposition *lv_proposition_not(Proposition *p);
```

创建否定命题（¬p）。

---

#### lv_proposition_implies

```c
Proposition *lv_proposition_implies(Proposition *premise, Proposition *conclusion);
```

创建蕴含命题（premise → conclusion）。

---

### 5.2 证明执行

#### lv_prove

```c
Proof *lv_prove(lvContext *ctx, Proposition *goal);
```

执行自动证明。

**参数**:
- `engine` - 引擎实例
- `goal` - 证明目标

**返回值**: 证明对象，失败返回 NULL

---

#### lv_prove_with_strategy

```c
Proof *lv_prove_with_strategy(
    lvContext *ctx,
    Proposition *goal,
    ProofStrategy strategy
);
```

使用指定策略证明。

**ProofStrategy 枚举**:
```c
typedef enum {
    PROOF_STRATEGY_AUTO,           /* 自动选择 */
    PROOF_STRATEGY_FORWARD,        /* 正向演绎 */
    PROOF_STRATEGY_BACKWARD,       /* 反向溯源 */
    PROOF_STRATEGY_CONTRADICTION,  /* 反证法 */
    PROOF_STRATEGY_GROEBNER,       /* Groebner 基 */
    PROOF_STRATEGY_SMT             /* SMT 求解 */
} ProofStrategy;
```

---

### 5.3 证明验证

#### lv_proof_valid

```c
bool lv_proof_valid(const Proof *proof);
```

检查证明是否有效。

---

#### lv_proof_get_step_count

```c
size_t lv_proof_get_step_count(const Proof *proof);
```

获取证明步骤数。

---

### 5.4 证明导出

#### lv_proof_export_lean

```c
bool lv_proof_export_lean(const Proof *proof, const char *filename);
```

导出为 Lean 证明脚本。

---

#### lv_proof_export_coq

```c
bool lv_proof_export_coq(const Proof *proof, const char *filename);
```

导出为 Coq 证明脚本。

---

#### lv_proof_export_latex

```c
bool lv_proof_export_latex(const Proof *proof, const char *filename);
```

导出为 LaTeX 文档。

---

#### lv_proof_export_tikz

```c
bool lv_proof_export_tikz(const Proof *proof, const char *filename);
```

导出为 TikZ 几何图形。

---

#### lv_proof_export_json

```c
char *lv_proof_export_json(const Proof *proof);
```

导出为 JSON 字符串（调用者负责释放）。

---

## 6. 预设模块系统

### 6.1 预设加载

#### lv_preset_load

```c
bool lv_preset_load(lvContext *ctx, const char *preset_name);
```

加载预设模块。

**参数**:
- `engine` - 引擎实例
- `preset_name` - 预设名称（如 "euclidean_geometry"）

**返回值**:
- `true` - 成功
- `false` - 失败

---

#### lv_preset_unload

```c
bool lv_preset_unload(lvContext *ctx, const char *preset_name);
```

卸载预设模块。

---

### 6.2 预设应用

#### lv_preset_apply

```c
Proposition *lv_preset_apply(
    lvContext *ctx,
    const char *theorem_name,
    ...
);
```

应用预设定理。

**参数**:
- `engine` - 引擎实例
- `theorem_name` - 定理名称
- `...` - 变参，几何实体参数

**返回值**: 命题对象

**示例**:
```c
Proposition *prop = lv_preset_apply(
    engine, "pythagorean_theorem", A, B, C
);
```

---

### 6.3 预设查询

#### lv_preset_list

```c
char **lv_preset_list(size_t *count);
```

获取可用预设列表。

**参数**:
- `count` - 输出预设数量

**返回值**: 预设名称数组（调用者负责释放）

---

#### lv_preset_get_info

```c
PresetInfo *lv_preset_get_info(const char *preset_name);
```

获取预设信息。

**PresetInfo 结构**:
```c
typedef struct PresetInfo {
    char *name;
    char *description;
    char *version;
    char **dependencies;
    size_t dependency_count;
} PresetInfo;
```

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

#### lv_get_memory_stats_ex

```c
bool lv_get_memory_stats_ex(lvMemoryStats *stats);
```

获取内存统计。

**lvMemoryStats 结构**:
```c
typedef struct lvMemoryStats {
    size_t current_bytes;   /* 当前分配量 */
    size_t peak_bytes;      /* 峰值分配量 */
    size_t alloc_count;     /* 分配次数 */
    size_t free_count;      /* 释放次数 */
} lvMemoryStats;
```

---

#### lv_set_memory_limit_ex

```c
void lv_set_memory_limit_ex(size_t limit_bytes);
```

设置内存上限。

---

#### lv_get_memory_limit_ex

```c
size_t lv_get_memory_limit_ex(void);
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

#### lv_get_last_error

```c
lvErrorCode lv_get_last_error(void);
```

获取最后错误码。

---

#### lv_get_last_error_string

```c
const char *lv_get_last_error_string(void);
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

- [架构手册](ARCHITECTURE_MANUAL.md)
- [入门教程](TUTORIAL.md)
- [API 快速入门](API_QUICKSTART.md)
- [语言规范](lv_LANGUAGE_SPEC.md)
