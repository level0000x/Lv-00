# 32. 运行时监控与生态系统

## 32.1 模块概述

本文档描述 Lv-00 几何元语言系统中的运行时保护、日志监控、诊断报告、开放生态包管理与实验性魔法映射模块。这些模块不直接定义几何推理规则，但承担系统稳定性、可观测性、社区扩展与概念演示职责。

**覆盖头文件**：
- `runtime_guard.h` —— 运行时防护、递归/自旋/耗时写操作守卫
- `runtime_monitor.h` —— 结构化日志、性能监控、健康检查与事件追踪
- `ecosystem.h` —— 开放生态包管理系统
- `magic.h` —— 咒语编程模拟器与概念映射层

---

## 32.2 理论定位

Lv-00 作为构造—证明系统，必须在长时间推理、复杂约束传播和外部插件加载中保持可观测与可控。因此本模块承担四类职责：

1. **运行时安全边界**：防止递归过深、自旋失控、写操作阻塞和资源耗尽。
2. **可观测性**：结构化日志、性能计时、健康检查和事件追踪。
3. **生态扩展**：允许公理包、预设块、证明策略、导出格式和 DSL 扩展以包形式注册、发现和安装。
4. **概念映射与教育演示**：`magic.h` 将 Lv-00 的核心抽象映射为符文、魔法阵和咒语，用于交互演示和类比表达。

---

## 32.3 runtime_guard.h —— 运行时防护

### 32.3.1 防护对象

运行时防护模块主要约束以下风险：

- 递归深度超过上限；
- 线程长时间自旋；
- 写操作持续时间异常；
- 单次操作超时；
- 未配对 enter/leave 导致上下文泄露。

### 32.3.2 配置来源

相关阈值由 `config.h` 提供：

| 配置项 | 默认含义 |
|--------|----------|
| `lv_CONFIG_RUNTIME_GUARD_MAX_RECURSE` | 最大递归深度 |
| `lv_CONFIG_RUNTIME_GUARD_SPIN_ATTEMPTS` | 最大自旋尝试次数 |
| `lv_CONFIG_RUNTIME_GUARD_WRITE_WARN_US` | 写操作警告阈值（微秒） |

这些阈值用于在推理、归一化、重写、流式输出和插件调用中防止系统不可控膨胀。

### 32.3.3 典型用途

- 在递归证明搜索中防止无限递归；
- 在并发队列等待中监控自旋次数；
- 在文件、网络或流式输出中标记过慢写操作；
- 在长时间运行的求解任务中生成诊断记录。

---

## 32.4 runtime_monitor.h —— 日志、性能、健康与诊断

### 32.4.1 日志系统

日志级别：

```c
typedef enum {
    lv_LOG_LEVEL_TRACE = 0,
    lv_LOG_LEVEL_DEBUG = 1,
    lv_LOG_LEVEL_INFO  = 2,
    lv_LOG_LEVEL_WARN  = 3,
    lv_LOG_LEVEL_ERROR = 4,
    lv_LOG_LEVEL_FATAL = 5,
    lv_LOG_LEVEL_OFF   = 6
} lvLogLevel;
```

日志输出目标：

```c
typedef enum {
    LOG_TARGET_NONE = 0,
    LOG_TARGET_STDOUT = 1,
    LOG_TARGET_STDERR = 2,
    LOG_TARGET_FILE = 4,
    LOG_TARGET_CALLBACK = 8,
    LOG_TARGET_SYSLOG = 16
} lvLogTarget;
```

日志记录：

```c
typedef struct {
    lvLogLevel level;
    char tag[lv_LOG_TAG_MAX_LEN];
    char message[lv_LOG_MSG_MAX_LEN];
    char file[256];
    int line;
    char function[128];
    int64_t timestamp_ms;
    int thread_id;
} lvLogRecord;
```

核心 API：

```c
bool lv_log_init(const lvLogConfig *config);
void lv_log_shutdown(void);
void lv_log_set_level(lvLogLevel level);
void lv_log_set_targets(lvLogTarget targets);
bool lv_log_set_file(const char *path);
void lv_log_set_callback(lvLogCallback callback, void *user_data);
void lv_log_write(lvLogLevel level, const char *tag,
                    const char *file, int line, const char *function,
                    const char *fmt, ...);
```

便捷宏包括 `lv_LOG_TRACE`、`lv_LOG_DEBUG`、`lv_LOG_INFO`、`lv_LOG_WARN`、`lv_LOG_ERROR` 和 `lv_LOG_FATAL`。

### 32.4.2 性能计时与统计

计时器状态：

```c
typedef enum {
    TIMER_STOPPED,
    TIMER_RUNNING,
    TIMER_PAUSED
} lvTimerState;
```

计时器结构：

```c
typedef struct {
    char name[lv_METRIC_NAME_MAX_LEN];
    lvTimerState state;
    int64_t start_time_ns;
    int64_t elapsed_ns;
    int64_t total_ns;
    uint64_t call_count;
    int depth;
} lvTimer;
```

API：

```c
bool lv_perf_init(void);
void lv_perf_shutdown(void);
lvTimer *lv_timer_create(const char *name);
void lv_timer_destroy(lvTimer *timer);
void lv_timer_start(lvTimer *timer);
int64_t lv_timer_stop(lvTimer *timer);
void lv_timer_pause(lvTimer *timer);
void lv_timer_resume(lvTimer *timer);
int64_t lv_timer_elapsed_ms(const lvTimer *timer);
int64_t lv_timer_elapsed_ns(const lvTimer *timer);
```

性能统计：

```c
typedef struct {
    char name[lv_METRIC_NAME_MAX_LEN];
    uint64_t count;
    double min_val;
    double max_val;
    double sum;
    double sum_sq;
    double mean;
    double variance;
    double std_dev;
    double last_val;
    int64_t last_time_ns;
} lvPerfStats;
```

### 32.4.3 健康检查

```c
typedef enum {
    HEALTH_OK,
    HEALTH_WARNING,
    HEALTH_CRITICAL,
    HEALTH_UNKNOWN
} lvHealthStatus;
```

```c
typedef struct {
    char name[lv_METRIC_NAME_MAX_LEN];
    lvHealthStatus status;
    char message[256];
    double value;
    double threshold_warning;
    double threshold_critical;
} lvHealthCheck;
```

核心 API：

```c
bool lv_health_init(void);
void lv_health_shutdown(void);
lvHealthReport *lv_runtime_health_check(void);
void lv_health_report_destroy(lvHealthReport *report);
void lv_health_set_memory_thresholds(double warning_mb, double critical_mb);
void lv_health_set_cpu_thresholds(double warning_percent, double critical_percent);
```

### 32.4.4 诊断报告

```c
typedef struct {
    char version[64];
    char build_date[32];
    int64_t uptime_ms;

    uint64_t memory_total;
    uint64_t memory_peak;
    uint64_t alloc_count;
    uint64_t free_count;

    uint64_t proof_count;
    uint64_t solve_count;
    double avg_proof_time_ms;
    double avg_solve_time_ms;

    uint64_t error_count;
    uint64_t warning_count;
    char last_error[256];

    lvHealthStatus health;

    char os_info[256];
    char cpu_info[256];
    uint32_t cpu_cores;
    uint64_t total_memory_mb;
} lvDiagnostics;
```

API：

```c
lvDiagnostics *lv_diagnostics_generate(void);
void lv_diagnostics_destroy(lvDiagnostics *diag);
bool lv_diagnostics_write_file(const lvDiagnostics *diag, const char *path);
char *lv_diagnostics_to_json(const lvDiagnostics *diag);
```

### 32.4.5 事件追踪

事件类型覆盖证明、求解、节点与约束变动、错误、警告和自定义事件：

```c
typedef enum {
    EVENT_TYPE_PROOF_START,
    EVENT_TYPE_PROOF_END,
    EVENT_TYPE_SOLVE_START,
    EVENT_TYPE_SOLVE_END,
    EVENT_TYPE_CONSTRAINT_ADD,
    EVENT_TYPE_CONSTRAINT_DEL,
    EVENT_TYPE_NODE_CREATE,
    EVENT_TYPE_NODE_DESTROY,
    EVENT_TYPE_ERROR,
    EVENT_TYPE_WARNING,
    EVENT_TYPE_CUSTOM
} lvEventType;
```

API：

```c
bool lv_event_trace_init(uint32_t max_events);
void lv_event_trace_shutdown(void);
void lv_event_trace_record(lvEventType type, const char *name, const char *data);
int lv_event_trace_begin(lvEventType type, const char *name);
void lv_event_trace_end(int event_id, const char *data);
uint32_t lv_event_trace_get_all(lvEventRecord **out_events, uint32_t max_count);
void lv_event_trace_clear(void);
bool lv_event_trace_export_chrome(const char *path);
```

Chrome Tracing 导出可用于性能时间线分析。

---

## 32.5 ecosystem.h —— 开放生态包管理

### 32.5.1 设计来源

生态系统模块借鉴：

- OpenGeometry Group：开放几何公理包生态与社区注册；
- mai：Docker/Podman 一键运行体验；
- GAP PackageManager：数学软件包管理、依赖解析与兼容性矩阵。

### 32.5.2 生态实体类型

```c
typedef enum {
    ECO_ENTITY_AXIOM_PACKAGE = 0,
    ECO_ENTITY_PRESET_BLOCK = 1,
    ECO_ENTITY_PROOF_STRATEGY = 2,
    ECO_ENTITY_EXPORT_FORMAT = 3,
    ECO_ENTITY_WEB_COMPONENT = 4,
    ECO_ENTITY_DSL_EXTENSION = 5
} lvEcosystemEntity;
```

可注册对象不仅包括公理包，也包括预设函数块、证明策略、导出格式、Web 组件与 DSL 扩展。

### 32.5.3 生态包结构

```c
typedef struct lvEcoPackage {
    char package_id[lv_ECO_NAME_MAX];
    char name[lv_ECO_NAME_MAX];
    char version[lv_ECO_VERSION_MAX];
    char author[lv_ECO_AUTHOR_MAX];

    lvEcoLicense license_type;
    char custom_license_text[lv_ECO_DESC_MAX];

    char description[lv_ECO_DESC_MAX];
    char source_url[lv_ECO_URL_MAX];

    char dependencies[lv_ECO_MAX_DEPENDENCIES][lv_ECO_NAME_MAX];
    int dep_count;

    lvEcosystemEntity entity_type;

    uint64_t install_date;
    int star_count;
    bool verified;
    char min_lv_version[lv_ECO_MIN_VERSION_MAX];

    char install_path[lv_ECO_URL_MAX];
    bool is_installed;
} lvEcoPackage;
```

### 32.5.4 安装状态与兼容性

```c
typedef enum {
    ECO_INSTALL_OK = 0,
    ECO_INSTALL_NOT_FOUND = 1,
    ECO_INSTALL_VERSION_CONFLICT = 2,
    ECO_INSTALL_DEP_MISSING = 3,
    ECO_INSTALL_DISK_FULL = 4,
    ECO_INSTALL_NETWORK_ERROR = 5,
    ECO_INSTALL_ALREADY_INSTALLED = 6,
    ECO_INSTALL_CHECKSUM_MISMATCH = 7
} lvEcoInstallStatus;
```

```c
typedef enum {
    ECO_COMPAT_FULLY = 0,
    ECO_COMPAT_PARTIAL = 1,
    ECO_COMPAT_INCOMPATIBLE = 2,
    ECO_COMPAT_UNKNOWN = 3
} lvEcoCompatibilityLevel;
```

---

## 32.6 magic.h —— 概念映射与咒语编程模拟器

### 32.6.1 设计目标

`magic.h` 将 Lv-00 核心概念映射到魔法系统：

| Lv-00 概念 | 魔法概念 |
|------------|----------|
| 符号坐标 | 符文 Rune |
| 约束图 | 魔法阵 MagicArray |
| 函数块 | 咒语 Spell |
| 约束类型 | 元素/能量关系 |

该模块适合教学、演示、游戏化交互与可视化原型。

### 32.6.2 元素系统

```c
typedef enum {
    ELEMENT_FIRE,
    ELEMENT_WATER,
    ELEMENT_AIR,
    ELEMENT_EARTH,
    ELEMENT_ETHER,
    ELEMENT_NONE
} MagicElement;
```

元素反应：

```c
typedef enum {
    ELEMENT_REACTION_NONE,
    ELEMENT_REACTION_ENHANCE,
    ELEMENT_REACTION_WEAKEN,
    ELEMENT_REACTION_CONFLICT
} ElementReaction;
```

### 32.6.3 符文与魔法阵

```c
struct Rune {
    SymbolicCoord *coord;
    MagicElement element;
    char *name;
    char *symbol;
    int power_level;
};
```

```c
typedef enum {
    ARRAY_CONNECTION,
    ARRAY_ENHANCEMENT,
    ARRAY_CONFLICT,
    ARRAY_INTERSECTION,
    ARRAY_CONTAINMENT,
    ARRAY_BOUNDARY,
    ARRAY_CHANNEL,
    ARRAY_FOCUS
} ArrayConstraintType;
```

魔法阵本质上是约束图的包装层：符文对应节点，阵列约束对应边。

### 32.6.4 咒语系统

```c
typedef enum {
    SPELL_STAGE_MOLDING,
    SPELL_STAGE_PURIFYING,
    SPELL_STAGE_INFUSING,
    SPELL_STAGE_RELEASING
} SpellStage;
```

```c
typedef enum {
    SPELL_STATUS_IDLE,
    SPELL_STATUS_CASTING,
    SPELL_STATUS_SUCCESS,
    SPELL_STATUS_FAILED,
    SPELL_STATUS_BACKLASH
} SpellStatus;
```

咒语对应函数块：输入端口是施法参数，输出端口是施法效果，内部构造是符文序列和约束。

---

## 32.7 理论—代码对应关系

| 代码概念 | 理论/工程对应 | 说明 |
|----------|----------------|------|
| `runtime_guard` | 运行时安全不变量 | 防止递归、阻塞和自旋失控 |
| `lvLogRecord` | 结构化运行记录 | 可审计日志单元 |
| `lvTimer` | 操作时间度量 | 用于性能剖析 |
| `lvHealthReport` | 系统健康状态 | 聚合资源与异常状态 |
| `lvDiagnostics` | 诊断快照 | 系统状态一次性导出 |
| `lvEventRecord` | 事件追踪记录 | 可导出 Chrome Tracing |
| `lvEcoPackage` | 生态包元数据 | 支持依赖、版本与许可证 |
| `Rune` | 符号坐标映射 | 概念演示层 |
| `MagicArray` | 约束图映射 | 魔法阵即约束图 |
| `Spell` | 函数块映射 | 咒语即可复用构造块 |

---

## 32.8 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [23_core_infrastructure.md](23_core_infrastructure.md) | 核心配置、错误码、模块系统 |
| [30_performance_concurrency.md](30_performance_concurrency.md) | 性能、线程池、基准测试 |
| [31_stream_interop.md](31_stream_interop.md) | 流式事件与外部互操作 |
| [20_preset_registry.md](20_preset_registry.md) | 预设函数块注册表 |
| [41_axiom_rewrite_export.md](41_axiom_rewrite_export.md) | 公理包、重写与导出 |

---

## 32.9 版本历史

- **v5.0.0**
  - 补全文档化：运行时防护、监控、生态包管理与魔法映射系统。
  - 明确可观测性与生态扩展在系统架构中的位置。

- **v3.3.0**
  - 引入运行时日志、健康检查、生态注册表与概念映射模块。
