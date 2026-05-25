# Lv-00 编码规范 (Coding Standard)

## 1. 命名规范

### 1.1 类型 (Types)

所有自定义类型使用 **PascalCase** 并以 **Lv00** 为前缀。

```c
// 正确
typedef struct Lv00Engine Lv00Engine;
typedef enum { ... } ConstraintGraph;
typedef struct { ... } GroebnerResult;

// 错误
typedef struct lv00_engine lv00_engine;  // 不使用 snake_case
typedef struct { ... } engine;           // 缺少 Lv00 前缀
```

### 1.2 函数 (Functions)

所有公共 API 函数使用 **snake_case** 并以 **lv00_** 为前缀。

```c
// 正确
Lv00Engine *lv00_engine_create(void);
void lv00_engine_destroy(Lv00Engine *engine);
int lv00_graph_node_count(const ConstraintGraph *graph);

// 错误
Lv00Engine *EngineCreate(void);          // 不使用 PascalCase
void engine_destroy(Lv00Engine *e);      // 缺少 lv00_ 前缀
```

对于模块内部函数（如 solver、graph 等），模块前缀可替代 lv00_ 前缀：

```c
// 模块内部函数可使用模块名作为前缀
SolverStatus solve_algebraic_system(...);   // solver 模块
AddNodeResult graph_add_point(...);          // graph 模块
```

### 1.3 宏 (Macros)

所有宏使用 **UPPER_SNAKE_CASE** 并以 **LV00_** 为前缀。

```c
// 正确
#define LV00_OK              0
#define LV00_MAX_VAR_ID      100000
#define LV00_ERROR_NULL_PTR  3

// 错误
#define max_var_id 100000       // 不使用小写
#define MAX_VAR_ID  100000      // 缺少 LV00_ 前缀
```

### 1.4 全局变量 (Global Variables)

全局变量使用 **g_** 前缀。

```c
// 正确
static StreamContext *g_stream_ctx;
static int g_error_count;

// 错误
static StreamContext *stream_ctx;       // 缺少 g_ 前缀
static StreamContext *gStreamContext;   // 不使用 camelCase
```

### 1.5 静态变量 (Static Variables)

文件作用域静态变量使用 **s_** 前缀。

```c
// 正确
static int s_overflow_count;
static MergeConfirmCallback s_merge_callback;

// 错误
static int overflow_count;              // 缺少 s_ 前缀
```

### 1.6 枚举值 (Enum Values)

枚举值使用 **UPPER_SNAKE_CASE** 并尽量以模块名称为前缀。

```c
// 正确
typedef enum {
    SOLVER_OK,
    SOLVER_UNIQUE,
    SOLVER_MULTIPLE
} SolverStatus;

typedef enum {
    LV00_OK = 0,
    LV00_ERROR_UNKNOWN = 1
} Lv00ErrorCode;

// 错误
typedef enum {
    solverOk,               // 不使用 camelCase
    solver_unique           // 不使用 snake_case
} SolverStatus;
```

### 1.7 结构体成员 (Struct Members)

结构体成员使用 **snake_case**，不加前缀。

```c
// 正确
struct ConstraintGraph {
    GeomNode **nodes;
    int node_count;
    int node_capacity;
};

// 错误
struct ConstraintGraph {
    GeomNode **m_Nodes;        // 不使用匈牙利命名法
    int NodeCount;             // 不使用 PascalCase
};
```

### 1.8 缩写规则

缩写名称至少包含 3 个字符，除非属于标准数学符号。

| 缩写 | 全称 | 说明 |
|------|------|------|
| `coords` | coordinates | 3 个字符以上，合法 |
| `num` | numerator | 3 个字符，合法 |
| `ctx` | context | 3 个字符，合法 |
| `pi` | pi | 标准数学符号，例外允许 |
| `e` | e (自然常数) | 标准数学符号，例外允许 |
| `dt` | delta | **不合法**，应使用 `delta` |
| `n` | number | **不合法**（非标准数学符号），应使用 `count` 或 `num` |
| `max` | maximum | 3 个字符，合法 |
| `min` | minimum | 3 个字符，合法 |

标准数学符号例外列表：`pi`, `e`, `i`（虚数单位）, `x`, `y`, `z`（坐标轴）, `dx`, `dy`（微积分）。

## 2. 文件组织

### 2.1 头文件保护

所有头文件必须使用 `#ifndef` / `#define` / `#endif` 模式：

```c
#ifndef LV00_MODULE_NAME_H
#define LV00_MODULE_NAME_H

/* 头文件内容 */

#endif /* LV00_MODULE_NAME_H */
```

保护宏命名规则：`LV00_` + 文件名（大写） + `_H`。

### 2.2 包含顺序

```c
/* 1. 对应的本地头文件 */
#include "lv00/module.h"

/* 2. 系统头文件 */
#include <stdbool.h>
#include <stdint.h>

/* 3. 项目内部头文件 */
#include "lv00/constraint_graph.h"
#include "lv00/error_codes.h"
```

### 2.3 目录结构

```
include/lv00/    - 公共 API 头文件
src/             - 实现源文件
tests/           - 测试代码
docs/            - 文档
```

## 3. 代码风格

### 3.1 缩进与格式

- 使用 4 个空格缩进（不使用 Tab）
- 行宽不超过 100 个字符
- 使用 `.clang-format` 配置文件统一格式化

### 3.2 注释

使用 Doxygen 风格的文档注释：

```c
/**
 * @brief 简要描述函数功能
 *
 * @param[in]  param1  参数描述
 * @param[out] param2  输出参数描述
 * @return 返回值描述
 */
```

### 3.3 const 正确性

- 所有指向只读数据的指针参数必须使用 `const`
- 不修改对象状态的成员函数应标记为作用于 `const` 对象

```c
// 正确：graph 参数只读
int graph_get_node_count(const ConstraintGraph *graph);

// 正确：输入数组只读
bool func_block_set_input_ports(FuncBlock *fb, const int *port_ids, int count);
```

## 4. 配置管理

所有可调参数集中在 `include/lv00/config.h` 中定义，使用 `LV00_CONFIG_` 前缀。

```c
#define LV00_CONFIG_MAX_VAR_ID           100000
#define LV00_CONFIG_DEFAULT_ERROR_BUF    256
```

严禁在源文件中使用魔数（magic numbers）。

## 5. 错误处理

所有公共 API 函数返回 `Lv00Status` 类型（定义在 `include/lv00/status_codes.h`）。

```c
typedef int Lv00Status;
#define LV00_OK                 0
#define LV00_ERR_MEMORY         1
#define LV00_ERR_INVALID_ARG    2
```

## 6. 参考

- 现有命名约定文档: `docs/NAMING_CONVENTION.md`
- 架构设计文档: `docs/ARCHITECTURE_v3.3.md`
- 本项目编码标准基于上述约定制定，补充了静态变量前缀、缩写规则和 const 正确性要求。
