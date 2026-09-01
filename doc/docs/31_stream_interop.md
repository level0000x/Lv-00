# 31. 流处理与互操作系统

## 31.1 模块概述

本文档描述 Lv-00 几何元语言系统中的实时流式输出、流上下文分发与外部互操作模块。该组模块负责将引擎内部的求解、归一化、重写、证明、函数块与错误事件实时传递给前端、外部工具和定理证明系统。

**覆盖头文件**：
- `stream.h` —— 引擎事件回调、实时状态推送与 JSON 序列化
- `stream_context_util.h` —— 流式上下文注册与分发工具
- `interop.h` —— WebSocket/stdio/pipe 互操作、导入导出与定理交换

---

## 31.2 理论定位

Lv-00 的证明与构造过程并非一次性黑箱计算，而是由多个可观察步骤组成。流处理与互操作系统承担以下职责：

1. **过程可视化**：把引擎内部状态转化为可播放、可追踪的事件序列。
2. **证明可审计**：将证明步骤、颜色更新、依赖变化等内容实时输出。
3. **外部系统连接**：与 Web 前端、Coq/Lean、GeoGebra、GeoJSON、TikZ 等格式交换数据。
4. **模块低耦合**：通过流上下文注册分发机制避免每个模块硬编码引擎依赖。

---

## 31.3 stream.h —— 实时事件流

### 31.3.1 发射模式

```c
typedef enum {
    STREAM_EMIT_IMMEDIATE = 0,
    STREAM_EMIT_BUFFERED,
    STREAM_EMIT_THROTTLED,
    STREAM_EMIT_LAZY
} StreamEmitMode;
```

| 模式 | 语义 | 适用场景 |
|------|------|----------|
| IMMEDIATE | 立即同步发射 | 调试、小规模推理 |
| BUFFERED | 事件入队，手动 flush | 批量步骤输出 |
| THROTTLED | 按时间间隔批量刷新 | Web 前端动画 |
| LAZY | 消费者拉取时分发 | 惰性审计、按需查看 |

### 31.3.2 事件类型体系

`StreamEventType` 覆盖引擎全生命周期，包括：

- 引擎事件：`ENGINE_START`, `ENGINE_DONE`, `ENGINE_PAUSED`
- 归一化事件：`NORMALIZE_START`, `NORMALIZE_MERGE`, `NORMALIZE_DONE`
- 重写事件：`REWRITE_START`, `REWRITE_MATCH_FOUND`, `REWRITE_APPLIED`, `REWRITE_ROLLBACK`
- 求解事件：`SOLVE_START`, `SOLVE_EQUATION_EXTRACTED`, `SOLVE_GROEBNER_STEP`, `SOLVE_VARIABLE_RESOLVED`
- 证明事件：`PROOF_STEP_ADDED`, `PROOF_STEP_APPLIED`, `PROOF_UNIFY`, `PROOF_COLOR_UPDATE`
- 函数块事件：打包、实例化、部分应用、确定性检查、捕获避免
- 预设事件：注册、查找、实例化、验证、模块加载
- 错误事件：冲突、熔断、错误、警告
- 信息事件：进度、图快照

事件类型通过 64 位掩码过滤：

```c
#define STREAM_FILTER_ALL  ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#define STREAM_FILTER_NONE ((uint64_t)0x0000000000000000ULL)
#define STREAM_EVENT_MASK(type) ((uint64_t)1ULL << (uint64_t)(type))
```

### 31.3.3 流式事件结构

```c
typedef struct StreamEvent {
    StreamEventType type;
    int64_t timestamp_ms;

    int step_number;
    int total_steps;
    int node_id;
    int constraint_id;
    int rule_id;
    int proof_step_id;

    double progress;
    double numeric_value;
    const char *description;
    const char *json_payload;
} StreamEvent;
```

不同事件类型只填充相关字段。未使用字段应置为 0、NULL 或 -1。

### 31.3.4 典型用途

- 前端实时显示归一化过程中的节点合并；
- 动画呈现证明步骤增长；
- 在冲突检测时立即推送错误；
- 输出图快照供 Web 端同步；
- 将证明颜色系统变化推送给可视化组件。

---

## 31.4 stream_context_util.h —— 流上下文分发

### 31.4.1 设计目标

不同模块需要访问当前 `StreamContext`，但不应直接依赖引擎对象。该工具提供注册/分发机制，使引擎只需调用一次分发函数即可同步所有模块。

### 31.4.2 Setter 回调

```c
typedef void (*StreamContextSetter)(StreamContext *ctx);
```

注册与分发：

```c
void stream_context_register_setter(StreamContextSetter setter);
void stream_context_dispatch_all(void *ctx);
void stream_context_register_builtins(void);
```

### 31.4.3 模块宏

```c
#define lv_DECLARE_STREAM_CTX(prefix) \
    static lv_THREAD_LOCAL StreamContext *prefix##_stream_ctx = NULL; \
    void prefix##_set_stream_context(StreamContext *ctx) { \
        prefix##_stream_ctx = ctx; \
    }
```

```c
#define LV_STREAM_CTX_DECLARE(prefix) \
    stream_context_register_setter(prefix##_set_stream_context)
```

使用原则：
- 只在 `.c` 文件中声明模块级上下文；
- 避免在多个编译单元中使用相同 prefix 生成多个 static 副本；
- 跨编译单元共享时应使用 extern 声明。

---

## 31.5 interop.h —— 外部互操作

### 31.5.1 接口类型

```c
typedef enum {
    INTEROP_INTERFACE_STDIO = 0,
    INTEROP_INTERFACE_WEBSOCKET,
    INTEROP_INTERFACE_PIPE
} InteropInterfaceType;
```

- STDIO：命令行/脚本接口；
- WebSocket：浏览器前端实时交互；
- PIPE：进程间管道通信。

### 31.5.2 导出格式

```c
typedef enum {
    INTEROP_EXPORT_COQ = 0,
    INTEROP_EXPORT_LEAN,
    INTEROP_EXPORT_HTML,
    INTEROP_EXPORT_SVG,
    INTEROP_EXPORT_PDF,
    INTEROP_EXPORT_TIKZ,
    INTEROP_EXPORT_GEOJSON,
    INTEROP_EXPORT_CANONICAL,
    INTEROP_EXPORT_ISABELLE,
    INTEROP_EXPORT_HOL_LIGHT
} InteropExportFormat;
```

导出格式覆盖定理证明器、可视化格式和规范表示。

### 31.5.3 导入格式

```c
typedef enum {
    INTEROP_IMPORT_GEOGEBRA = 0,
    INTEROP_IMPORT_GEOJSON,
    INTEROP_IMPORT_SVG
} InteropImportFormat;
```

### 31.5.4 命令类型

```c
typedef enum {
    INTEROP_CMD_ADD_NODE = 0,
    INTEROP_CMD_REMOVE_NODE,
    INTEROP_CMD_GET_NODE,
    INTEROP_CMD_ADD_CONSTRAINT,
    INTEROP_CMD_REMOVE_CONSTRAINT,
    INTEROP_CMD_GET_CONSTRAINT,
    INTEROP_CMD_PACK_FUNCTION,
    INTEROP_CMD_INSTANTIATE,
    INTEROP_CMD_SOLVE,
    INTEROP_CMD_REWRITE,
    INTEROP_CMD_UNIFY,
    INTEROP_CMD_GET_GRAPH,
    INTEROP_CMD_EXPORT_GRAPH,
    INTEROP_CMD_GET_STATUS,
    INTEROP_CMD_PING,
    INTEROP_CMD_SHUTDOWN,
    INTEROP_CMD_STREAM_START,
    INTEROP_CMD_STREAM_STOP,
    INTEROP_CMD_STREAM_FILTER,
    INTEROP_CMD_STREAM_STATS,
    INTEROP_CMD_STREAM_FLUSH
} InteropCommandType;
```

### 31.5.5 命令与响应

```c
typedef struct {
    InteropCommandType type;
    char command_name[256];
    char params[INTEROP_MAX_PARAMS][256];
    int param_count;
    int request_id;
} InteropCommand;
```

```c
typedef struct {
    int request_id;
    int status_code;
    char data[INTEROP_RESP_BUFFER_SIZE];
    size_t data_len;
} InteropResponse;
```

### 31.5.6 服务器状态

```c
typedef struct {
    InteropInterfaceType type;
    int port;
    bool running;
    void *internal_data;

    bool stream_enabled;
    int stream_callback_id;
    uint64_t stream_filter_mask;
    long stream_events_sent;
} InteropServer;
```

互操作服务器不仅处理请求/响应命令，也可转发流式事件。

### 31.5.7 导出与导入配置

```c
typedef struct {
    InteropExportFormat format;
    char output_path[INTEROP_MAX_PATH_LEN];
    bool include_proofs;
    bool include_metadata;
    bool pretty_print;
    int compression_level;
} InteropExportConfig;
```

```c
typedef struct {
    InteropImportFormat format;
    char input_path[INTEROP_MAX_PATH_LEN];
    bool preserve_ids;
    bool validate_geometry;
} InteropImportConfig;
```

### 31.5.8 定理交换上下文

```c
typedef struct {
    char trust_base_name[64];
    char trust_base_version[32];
    char *exported_calls;
    size_t calls_len;
} InteropTheoremContext;
```

该结构用于记录导出到 Coq、Lean、Isabelle/HOL 或 HOL Light 时的信任基名称、版本与调用序列。

---

## 31.6 理论—代码对应关系

| 代码概念 | 理论/工程对应 | 说明 |
|----------|----------------|------|
| `StreamEvent` | 推理过程事件 | 将内部步骤外化为可观察对象 |
| `StreamEmitMode` | 事件调度策略 | 控制实时性、吞吐量与延迟 |
| `STREAM_EVENT_MASK` | 事件类型筛选 | 用位掩码选择可观察子过程 |
| `StreamContextSetter` | 依赖反转接口 | 模块不直接依赖引擎 |
| `InteropCommand` | 外部命令语言 | 将外部请求映射为引擎操作 |
| `InteropExportFormat` | 证明/图形导出目标 | 支持定理证明器和可视化格式 |
| `InteropTheoremContext` | 信任基记录 | 记录导出证明的依赖边界 |

---

## 31.7 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [18_output_layer.md](18_output_layer.md) | 输出层总体设计 |
| [22_proof_export_trace_widget.md](22_proof_export_trace_widget.md) | 证明导出、追踪与可视化组件 |
| [25_engine_scheduler.md](25_engine_scheduler.md) | 引擎状态与求解流程 |
| [09_proof.md](09_proof.md) | 证明系统核心 |
| [23_core_infrastructure.md](23_core_infrastructure.md) | 跨平台与公共 API 定义 |

---

## 31.8 版本历史

- **v5.0.0**
  - 补全文档化：流式事件、上下文分发与互操作命令/格式。
  - 明确流事件作为证明过程外化机制的角色。

- **v3.3.0**
  - 引入实时事件推送、外部互操作与定理交换接口。
