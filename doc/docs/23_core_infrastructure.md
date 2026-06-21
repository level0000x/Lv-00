# 23. 核心基础设施与配置系统

## 23.1 模块概述

本文档描述 Lv-00 几何元语言系统的核心基础设施模块，包括系统入口、配置管理、错误处理、调试工具和跨平台抽象层。这些模块构成了整个系统的基石，为上层功能提供统一的接口和保障。

**覆盖头文件**：
- `lv00.h` —— 公共 API 主入口
- `lv00_internal.h` —— 内部实现接口
- `lv00_utils.h` —— 通用工具函数
- `lv00_numeric.h` —— 数值类型工具
- `config.h` —— 集中化配置系统
- `error_codes.h` —— 统一错误码体系
- `status_codes.h` —— 状态码定义
- `debug.h` —— 调试与日志工具
- `cross_platform.h` —— 跨平台兼容层
- `module.h` —— 模块系统
- `memory_pool.h` —— 内存池管理
- `mini_kernel.h` —— 最小内核接口、轻量初始化与核心能力裁剪

---

## 23.2 lv00.h —— 公共 API 主入口

### 23.2.1 设计定位

`lv00.h` 是 Lv-00 系统的唯一公共 API 入口，遵循**最小暴露原则**：
- 仅暴露对外接口，隐藏所有实现细节
- 内部数据结构定义在 `lv00_internal.h`，不应被外部直接引用
- 所有公共函数通过 `LV00_PUBLIC_API` 宏声明，支持共享库（DLL/SO）构建

### 23.2.2 版本管理

```c
#define LV00_VERSION_MAJOR 5
#define LV00_VERSION_MINOR 0
#define LV00_VERSION_PATCH 0
#define LV00_VERSION_STRING "5.0.0"
```

**版本 API**：
| 函数 | 功能 |
|------|------|
| `lv00_version_major()` | 获取主版本号（编译期内联） |
| `lv00_version_minor()` | 获取次版本号（编译期内联） |
| `lv00_version_patch()` | 获取补丁版本号（编译期内联） |
| `lv00_get_version_string()` | 获取版本字符串 |
| `lv00_get_version_info()` | 获取完整版本信息（含平台、编译器、架构） |
| `lv00_check_version_compat()` | 检查运行时与编译时版本兼容性 |

**版本兼容性规则**：
- 主版本号不匹配 → 返回 false（不兼容）
- 次版本号不同 → 警告但允许运行（向后兼容的 API 添加）

### 23.2.3 系统生命周期

```c
// 初始化与清理
bool lv00_init(void);           // 初始化所有子系统
void lv00_cleanup(void);        // 释放所有全局资源
bool lv00_is_initialized(void); // 检查初始化状态

// 系统信息
int  lv00_get_system_info(char *buf, size_t size);  // 获取系统信息
int  lv00_health_check(void);                       // 健康评分（0~100）
```

**初始化顺序**：
1. 内存管理系统
2. 错误码系统
3. 调试基础设施
4. 其他子系统

**线程安全**：`lv00_context_create()` 应在主线程调用，在创建工作线程之前完成。

### 23.2.4 引擎生命周期（便捷 API）

```c
LV00Context *lv00_engine_create(void);   // 创建上下文实例
void lv00_context_destroy(LV00Context *ctx);  // 销毁上下文
```

引擎是 Lv-00 的核心工作单元，持有：
- 约束图（Constraint Graph）
- 符号坐标系统
- 重写规则集
- 证明树

### 23.2.5 几何构造便捷 API

```c
// 创建点（有理数坐标）
int lv00_add_point(LV00Context *ctx,
    int64_t x_num, uint64_t x_den,
    int64_t y_num, uint64_t y_den);

// 创建点（整数坐标，内联优化）
static inline int lv00_add_point_i(LV00Context *ctx, long long x, long long y);

// 创建线段
int lv00_add_line_segment(LV00Context *ctx, int point1_id, int point2_id);

// 添加关联约束
bool lv00_add_constraint_incidence(LV00Context *ctx, int point_id, int line_id);
```

### 23.2.6 推理与求解

```c
// 图归一化
NormalizationResult *lv00_normalize(LV00Context *ctx, bool scope_aware);

// 完整求解流水线
EngineSolveResult lv00_solve(LV00Context *ctx);
```

**求解流水线**：归一化 → 重写 → 约束求解 → 验证

### 23.2.7 跨平台宏定义

| 宏 | 功能 | 平台差异处理 |
|----|------|-------------|
| `LV00_PUBLIC_API` | 共享库导出/导入控制 | Windows: `__declspec(dllexport/dllimport)`；GCC/Clang: `__attribute__((visibility))` |
| `LV00_THREAD_LOCAL` | 线程局部存储 | MSVC: `__declspec(thread)`；GCC/Clang: `__thread`；C11: `_Thread_local` |
| `LV00_DEPRECATED(msg)` | 废弃标记 | GCC/Clang: `__attribute__((deprecated))`；MSVC: `__declspec(deprecated)` |
| `LV00_PATH_SEPARATOR` | 路径分隔符 | Windows: `\\`；Unix: `/` |
| `LV00_LOCALTIME` | 线程安全 localtime | Windows: `localtime_s`；Unix: `localtime_r` |

---

## 23.3 config.h —— 集中化配置系统

### 23.3.1 设计原则

`config.h` 是 Lv-00 系统的**单一事实来源**（Single Source of Truth），集中管理所有可调参数：
- 消除代码中的"魔数"
- 统一命名规范（`LV00_CONFIG_` 前缀）
- 向后兼容（保留旧宏名作为别名）

### 23.3.2 配置分类

#### 约束图与求解器限制

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `LV00_CONFIG_SOLVER_MAX_VAR_ID` | 100000 | 变量 ID 上限（防稀疏 ID 导致 OOM） |
| `LV00_CONFIG_MAX_MODULE_DEPTH` | 32 | 模块最大嵌套深度 |
| `LV00_CONFIG_GRAPH_ERROR_BUFFER_SIZE` | 256 | 错误缓冲区大小（字节） |
| `LV00_CONFIG_INITIAL_ARRAY_CAPACITY` | 8 | 动态数组初始容量 |
| `LV00_CONFIG_INITIAL_HASH_INDEX_CAPACITY` | 64 | 哈希索引表初始容量（2的幂） |

#### 重写引擎阈值

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `LV00_CONFIG_DEFAULT_REWRITE_LIMIT` | 1000 | 默认重写步数上限 |
| `LV00_CONFIG_WL_ITERATIONS` | 3 | WL 图核迭代次数 |
| `LV00_CONFIG_WL_HISTORY_SIZE` | 64 | WL 图哈希历史缓冲区大小 |

#### 流式输出阈值

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `LV00_CONFIG_STREAM_ASYNC_QUEUE_CAPACITY` | 1024 | 异步事件队列容量 |
| `LV00_CONFIG_STREAM_JSON_BUFFER_SIZE` | 4096 | JSON 序列化缓冲区大小 |
| `LV00_CONFIG_STREAM_DEFAULT_THROTTLE_MS` | 50 | 默认节流间隔（毫秒） |

#### 数值精度

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `LV00_CONFIG_BIT_CUTOFF_THRESHOLD` | 1000000 | 位数熔断阈值（A→B 计划切换） |
| `LV00_CONFIG_MAX_PRECISION_BITS` | 100 | 代数数默认精度位数 |

#### 解析器安全限制

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `LV00_CONFIG_PARSER_MAX_INPUT_LENGTH` | 1048576 (1 MiB) | 最大输入长度 |
| `LV00_CONFIG_PARSER_MAX_TOKENS` | 100000 | 最大 token 数量 |
| `LV00_CONFIG_PARSER_MAX_AST_DEPTH` | 256 | 最大 AST 深度 |
| `LV00_CONFIG_PARSER_MAX_AST_NODES` | 500000 | 最大 AST 节点数 |

#### 运行时防护阈值

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `LV00_CONFIG_RUNTIME_GUARD_MAX_RECURSE` | 128 | 最大递归深度 |
| `LV00_CONFIG_RUNTIME_GUARD_SPIN_ATTEMPTS` | 1024 | 自旋尝试次数 |
| `LV00_CONFIG_RUNTIME_GUARD_WRITE_WARN_US` | 10000 | 写操作警告阈值（微秒） |

### 23.3.3 配置访问 API（lv00.h）

```c
// 获取配置
int         lv00_config_get_int(const char *key, int default_val);
bool        lv00_config_get_bool(const char *key, bool default_val);
double      lv00_config_get_double(const char *key, double default_val);
const char *lv00_config_get_string(const char *key, const char *default_val);

// 设置配置
bool lv00_config_set_int(const char *key, int value);
bool lv00_config_set_bool(const char *key, bool value);
bool lv00_config_set_double(const char *key, double value);
bool lv00_config_set_string(const char *key, const char *value);
```

**配置键名格式**：`模块.参数`，如 `"rewrite.step_limit"`、`"solver.timeout_ms"`

---

## 23.4 error_codes.h —— 统一错误码体系

### 23.4.1 错误码分层设计

| 范围 | 类别 |
|------|------|
| 0 | 成功（`LV00_OK`） |
| 1-99 | 通用系统错误 |
| 100-199 | 内存与资源错误 |
| 130-139 | 解析器安全错误 |
| 200-299 | 约束图相关错误 |
| 300-399 | 符号坐标相关错误 |
| 400-499 | 求解器相关错误 |
| 500-599 | 重写引擎相关错误 |
| 600-699 | 合一检查相关错误 |
| 700-799 | 函数块相关错误 |
| 800-899 | 类型系统相关错误 |
| 900-999 | 证明系统相关错误 |

### 23.4.2 核心错误码

**通用系统错误**：
- `LV00_ERROR_UNKNOWN` —— 未知错误
- `LV00_ERROR_INVALID_PARAM` —— 无效参数
- `LV00_ERROR_NULL_POINTER` —— 空指针
- `LV00_ERROR_NOT_INITIALIZED` —— 未初始化
- `LV00_ERROR_TIMEOUT` —— 操作超时
- `LV00_ERROR_CANCELLED` —— 操作被取消

**约束图错误**：
- `LV00_ERROR_NODE_CONFLICT` —— 节点冲突
- `LV00_ERROR_CONSTRAINT_CONFLICT` —— 约束冲突
- `LV00_ERROR_CYCLIC_DEPENDENCY` —— 循环依赖

**证明系统错误**：
- `LV00_ERROR_PROOF_INVALID` —— 无效证明
- `LV00_ERROR_PROOF_VERIFICATION_FAILED` —— 证明验证失败
- `LV00_ERROR_CIRCUIT_OPEN` —— 熔断器已跳闸

### 23.4.3 错误信息 API

```c
// 获取错误信息
const char *lv00_error_string(Lv00ErrorCode code);      // 错误描述
const char *lv00_error_name(Lv00ErrorCode code);        // 错误名称（如 "LV00_OK"）
const char *lv00_error_category(Lv00ErrorCode code);    // 错误类别

// 错误码反向查找
Lv00ErrorCode lv00_error_code_from_string(const char *name);

// 辅助判断
static inline bool lv00_is_success(Lv00ErrorCode code);
static inline bool lv00_is_error(Lv00ErrorCode code);
```

### 23.4.4 线程局部错误状态

```c
Lv00ErrorCode lv00_get_last_error_code(void);                    // 获取最后错误码
const char   *lv00_get_last_error_message(void);                 // 获取错误信息
int           lv00_get_error_description(char *buf, size_t buf_size);  // 完整描述

void lv00_set_error(Lv00ErrorCode code, const char *format, ...);      // 设置错误
void lv00_set_error_ctx(Lv00ErrorCode code, const char *file, int line, 
                        const char *func, const char *format, ...);   // 带上下文
void lv00_clear_error(void);                                       // 清除错误
```

### 23.4.5 便捷错误处理宏

```c
// 空指针检查
LV00_CHECK_NULL(ptr, ret)
LV00_CHECK_NULL_VOID(ptr)

// 条件检查
LV00_CHECK(cond, err_code, ret, msg)

// 内存分配检查
LV00_CHECK_ALLOC(ptr, ret)

// 索引范围检查
LV00_CHECK_INDEX(idx, max, ret)

// 数值范围检查
LV00_CHECK_RANGE(val, min, max, ret)

// 错误传播
LV00_PROPAGATE_ERROR(code)
LV00_TRY(expr, label)

// 设置错误并返回
LV00_ERROR_SET(code, fmt, ...)
LV00_ERROR_RETURN(err_code, ret, fmt, ...)
```

**使用示例**：
```c
LV00_CHECK_NULL(ctx, NULL);
LV00_CHECK(index >= 0 && index < count, LV00_ERROR_INVALID_PARAM, -1, "索引越界");
LV00_CHECK_ALLOC(new_node, NULL);
LV00_PROPAGATE_ERROR(graph_add_constraint(graph, type, participants, n));
```

---

## 23.5 debug.h —— 调试与日志工具

### 23.5.1 日志级别

| 级别 | 值 | 说明 |
|------|-----|------|
| `LV00_LOG_OFF` | 0 | 禁用所有日志 |
| `LV00_LOG_ERROR` | 1 | 仅错误 |
| `LV00_LOG_WARN` | 2 | 错误 + 警告 |
| `LV00_LOG_INFO` | 3 | 错误 + 警告 + 信息 |
| `LV00_LOG_DEBUG` | 4 | 所有日志（含调试） |

### 23.5.2 日志 API

```c
void lv00_set_log_level(int level);    // 设置日志级别
int  lv00_get_log_level(void);         // 获取当前日志级别

// 日志宏（根据级别自动过滤）
LV00_LOG_ERROR(fmt, ...)
LV00_LOG_WARN(fmt, ...)
LV00_LOG_INFO(fmt, ...)
LV00_LOG_DEBUG(fmt, ...)
```

### 23.5.3 断言控制

```c
void lv00_set_assertions_enabled(bool enabled);  // 启用/禁用断言
bool lv00_are_assertions_enabled(void);          // 检查断言状态
```

**建议**：发布构建中禁用断言以获得最佳性能。

### 23.5.4 调试工具

```c
// 打印约束图结构（用于调试）
void lv00_debug_print_graph(const ConstraintGraph *graph);

// 打印符号坐标
void lv00_debug_print_coord(const SymbolicCoord *coord);

// 打印证明树
void lv00_debug_print_proof(const ProofTree *proof);

// 性能分析
void lv00_debug_profile_start(const char *name);
void lv00_debug_profile_end(const char *name);
```

---

## 23.6 cross_platform.h —— 跨平台兼容层

### 23.6.1 平台检测宏

```c
// 操作系统检测
#ifdef _WIN32
    // Windows 平台
#elif defined(__APPLE__)
    // macOS 平台
#elif defined(__linux__)
    // Linux 平台
#endif

// 编译器检测
#ifdef _MSC_VER
    // Microsoft Visual C++
#elif defined(__GNUC__)
    // GCC
#elif defined(__clang__)
    // Clang
#endif

// 架构检测
#ifdef _WIN64
    // 64-bit Windows
#elif defined(__x86_64__)
    // x86-64
#elif defined(__aarch64__)
    // ARM64
#endif
```

### 23.6.2 类型系统统一

```c
// 固定宽度整数（C99 标准，但某些旧编译器需要兼容）
#include <stdint.h>

// 布尔类型（C99 标准）
#include <stdbool.h>

// size_t 和 NULL
#include <stddef.h>
```

### 23.6.3 编译器特性适配

| 特性 | GCC/Clang | MSVC |
|------|-----------|------|
| 内联函数 | `static inline` | `static __inline` |
| 强制内联 | `__attribute__((always_inline))` | `__forceinline` |
| 分支预测 | `__builtin_expect` | 无直接等价 |
| 对齐 | `__attribute__((aligned(n)))` | `__declspec(align(n))` |
| 打包结构 | `__attribute__((packed))` | `#pragma pack` |

---

## 23.7 module.h —— 模块系统

### 23.7.1 模块注册

```c
typedef struct Lv00Module {
    const char *name;           // 模块名称
    const char *version;        // 模块版本
    int         priority;       // 初始化优先级（越小越早）
    
    bool (*init)(void);         // 初始化函数
    void (*cleanup)(void);      // 清理函数
    
    struct Lv00Module *next;    // 链表指针
} Lv00Module;

// 模块注册
bool lv00_module_register(Lv00Module *module);

// 模块查找
Lv00Module *lv00_module_find(const char *name);

// 按优先级初始化所有模块
bool lv00_module_init_all(void);

// 清理所有模块（逆序）
void lv00_module_cleanup_all(void);
```

### 23.7.2 模块依赖声明

```c
#define LV00_MODULE_DEPENDS_ON(mod_name) \
    extern bool lv00_module_##mod_name##_loaded;

#define LV00_MODULE_ENSURE(mod_name) \
    do { \
        if (!lv00_module_##mod_name##_loaded) { \
            LV00_ERROR_RETURN(LV00_ERROR_NOT_INITIALIZED, false, \
                              "依赖模块 " #mod_name " 未加载"); \
        } \
    } while (0)
```

---

## 23.8 memory_pool.h —— 内存池管理

### 23.8.1 内存统计

```c
typedef struct Lv00MemoryStats {
    size_t current_bytes;   // 当前分配字节数
    size_t peak_bytes;      // 峰值分配字节数
    size_t total_allocs;    // 总分配次数
    size_t total_frees;     // 总释放次数
    size_t current_objects; // 当前活跃对象数
} Lv00MemoryStats;

// 获取内存统计
bool lv00_get_memory_stats_ex(Lv00MemoryStats *stats);
```

### 23.8.2 内存限制

```c
void   lv00_set_memory_limit_ex(size_t limit_bytes);  // 设置内存上限（0=无限制）
size_t lv00_get_memory_limit_ex(void);                // 获取内存上限
```

达到上限后，新分配请求将失败并返回 `LV00_ERROR_OUT_OF_MEMORY`。

### 23.8.3 内存池配置（config.h）

| 配置项 | 对象大小 | 说明 |
|--------|----------|------|
| `LV00_CONFIG_POOL_CONSTRAINT_NODE_SIZE` | 128 | 约束节点池 |
| `LV00_CONFIG_POOL_CONSTRAINT_SIZE` | 96 | 约束池 |
| `LV00_CONFIG_POOL_SYMBOLIC_COORD_SIZE` | 64 | 符号坐标池 |
| `LV00_CONFIG_POOL_PROOF_STEP_SIZE` | 128 | 证明步骤池 |

---

## 23.9 代码-理论对应关系

| 代码概念 | 理论对应 | 文档位置 |
|----------|----------|----------|
| `lv00_context_create()` / `lv00_context_destroy(ctx)` | 系统初始态与终止态 | 本文档 23.2.3 |
| `LV00_PUBLIC_API` | 接口契约的形式化边界 | 本文档 23.2.7 |
| `Lv00ErrorCode` 分层 | 错误分类的代数结构 | 本文档 23.4.1 |
| `LV00_CONFIG_*` 常量 | 系统参数空间 | 本文档 23.3 |
| 内存池管理 | 资源约束的可预测性 | 本文档 23.8 |
| 跨平台抽象 | 实现无关的语义保持 | 本文档 23.6 |

---

## 23.10 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [12_context_and_lifecycle.md](12_context_and_lifecycle.md) | 上下文管理、熔断器、运行时守卫 |
| [14_memory_management.md](14_memory_management.md) | 内存池详细设计 |
| [01_symbolic_coord.md](01_symbolic_coord.md) | 符号坐标系统 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图核心 |
| [ARCHITECTURE_v3.3.md](ARCHITECTURE_v3.3.md) | 五层架构总览 |

---

## 23.11 版本历史

- **v3.5.0** (当前)
  - 统一版本号管理
  - 完善跨平台宏定义
  - 集中化配置系统

- **v3.4.0**
  - 引入 `LV00_PUBLIC_API` 共享库支持
  - 线程局部错误状态

- **v3.3.0**
  - 创建 `config.h` 集中化配置
  - 错误码分层体系
