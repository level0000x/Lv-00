# Lv-00 十层架构优化方案（含完整补全清单）

> **版本**: 5.0.0  
> **日期**: 2026-06-05  
> **状态**: 完整方案（含性能、安全、规范与发布设计）

---

## 1. 任务重述

### 1.1 目标
将 Lv-00 项目中所有未纳入前5层框架的代码进行**合理化下沉**，使后5层变薄、职责更清晰。同时**完整列出所有需要补全的接口、机制、工具和文档**，确保架构可落地执行。

### 1.2 约束
- **前5层目录结构不可变**：`core/src/layer1-5/` 和 `core/include/lv00/layer1-5/` 保持现状
- **允许在前5层内新增文件**：可以在现有目录中新增扩展接口和绑定代码
- **追求清晰分层**：不刻意追求后5层最薄，但确保每个模块归属合理

### 1.3 成功标准
- 每个模块都有明确的"下沉/保留"决策及理由
- 所有C层缺失的扩展接口都有定义和实现计划
- 层间通信契约、测试策略、CI/CD适配都有明确方案
- 提供可执行的迁移和补全路线图

---

## 2. 核心设计思想

### 2.1 分层原则

```
前5层（C核心 + 核心绑定）        后5层（纯语言工具 + 纯UI + 纯生态）
+---------------------------+    +---------------------------+
| C核心算法（liblv00）       |    |                           |
| - 解析/公理/约束/推理/输出 |    |  第6层：纯交互可视化        |
+---------------------------+    |  - React/tkinter前端       |
            |                    |  - 公式渲染器               |
+-----------v---------------+    |  - WebSocket服务           |
| 核心绑定层（新增）         |    |                           |
| - Python ctypes绑定        |    |  第7层：纯语言绑定运行时    |
| - 核心类型Python封装       |    |  - PyEuclid语法糖          |
| - 引擎/算法Python接口      |    |  - DSL语法工具             |
+-----------+---------------+    |  - 预设函数块库            |
            |                    |  - 异步流适配器             |
+-----------v---------------+    |                           |
| 后5层调用入口              |    |  第8层：智能辅助            |
| - 纯Python算法/工具        |    |  - LLM编程辅助             |
| - UI事件处理               |    |  - 知识库/提示词引擎        |
+---------------------------+    |                           |
                                 |  第9层：监控运维            |
                                 |  - 并发监控                |
                                 |  - Web仪表盘               |
                                 |                           |
                                 |  第10层：形式化与生态       |
                                 |  - Lean形式化验证          |
                                 |  - 示例库/文档/工具         |
                                 +---------------------------+
```

### 2.2 下沉判断标准

| 标准 | 下沉到前5层 | 留在后5层 |
|------|------------|----------|
| **是否直接持有C指针** | 是 | 否 |
| **是否直接调用C函数** | 是 | 否 |
| **是否是C结构体的语言镜像** | 是 | 否 |
| **算法是否完全在Python中实现** | 否 | 是 |
| **是否纯UI/前端代码** | 否 | 是 |
| **是否纯基础设施（WebSocket/asyncio）** | 否 | 是 |

---

## 3. 模块下沉决策表

### 3.1 应下沉到前5层的模块

| 当前位置 | 模块 | 下沉位置 | 下沉理由 |
|----------|------|----------|----------|
| `module/python/lv00/_ctypes_binding.py` | C库ctypes绑定 | `core/src/python_binding/_ctypes_binding.py` | C头文件的Python镜像，FFI层 |
| `module/python/lv00/core.py` (C委托部分) | 核心类型封装 | `core/src/python_binding/native_types.py` | SymbolicCoord/Graph等C结构体包装 |
| `module/python/lv00/engine.py` (Engine类) | 引擎封装 | `core/src/python_binding/engine.py` | 直接调用C引擎函数 |
| `module/python/lv00/groebner_engine.py` | Groebner引擎 | `core/src/python_binding/groebner.py` | C Groebner引擎的Python绑定 |
| `module/python/lv00/sparse_la.py` | 稀疏线性代数 | `core/src/python_binding/sparse_la.py` | C稀疏LA的Python绑定 |
| `module/python/lv00/type_system.py` | 类型系统 | `core/src/python_binding/type_system.py` | C类型系统的Python绑定 |
| `module/python/lv00/func_block.py` | 函数块系统 | `core/src/python_binding/func_block.py` | C函数块系统的Python绑定 |
| `module/python/lv00/formula.py` (C委托) | 公式节点 | `core/src/python_binding/formula_native.py` | FormulaNode的C指针管理 |
| `module/python/lv00/proof_extras.py` (C委托) | 证明系统 | `core/src/python_binding/proof_native.py` | 证明导航/多策略/导出 |
| `module/python/lv00/stream_bridge.py` (C操作) | 流式上下文 | `core/src/python_binding/stream_native.py` | C流式上下文操作 |
| `module/python/lv00/interactive_geo.py` (C核心) | 交互几何核心 | `core/src/python_binding/interactive_geo_native.py` | 约束维护/奇点检测/随机化检查 |

### 3.2 应留在后5层的模块

| 当前位置 | 模块 | 保留位置 | 保留理由 |
|----------|------|----------|----------|
| `module/python/lv00/py_euclid_style.py` | PyEuclid风格API | `layer7_binding/python/highlevel/` | 纯Python语法糖 |
| `module/python/lv00/high_dim.py` | 高维几何 | `layer7_binding/python/highlevel/` | 纯Python算法 |
| `module/python/lv00/preset_*.py` | 预设函数块 | `layer7_binding/python/presets/` | 纯Python扩展库 |
| `module/python/lv00/math_presets.py` | 数学预设 | `layer7_binding/python/presets/` | 纯Python数学库 |
| `module/python/lv00/dsl*.py` | DSL工具 | `layer7_binding/python/dsl/` | 纯Python语法分析 |
| `module/python/lv00/async_stream.py` | 异步流式 | `layer7_binding/python/streaming/` | asyncio适配器 |
| `module/python/lv00/ws_server.py` | WebSocket服务 | `layer6_interactive/websocket/` | 纯基础设施 |
| `module/python/lv00/interactive_geo.py` (UI) | tkinter GUI | `layer6_interactive/python_gui/` | 纯UI代码 |
| `module/python/lv00/formula.py` (渲染) | 公式渲染 | `layer6_interactive/renderers/` | SVG/LaTeX/ASCII渲染 |
| `module/python/lv00/engine.py` (辅助) | 引擎辅助 | `layer7_binding/python/helpers/` | 重试/安全求解等 |
| `web/gui/src/` | React前端 | `layer6_interactive/web_gui/` | 纯前端代码 |
| `module/llm_coding_assistant/` | LLM辅助 | `layer8_ai_assistant/` | AI工具 |
| `module/concurrent_monitor/` | 并发监控 | `layer9_monitoring/` | 运维工具 |
| `formal/`, `lv00-formal/` | Lean形式化 | `layer10_ecosystem/formal_verification/` | 形式化验证 |
| `examples/`, `doc/`, `tool/` | 示例/文档/工具 | `layer10_ecosystem/` | 生态内容 |

---

## 4. 需要补全的内容清单

### 4.1 C层扩展接口（高优先级）

#### 4.1.1 插件系统头文件

**当前状态**：`examples/plugin_example/sample_plugin.c` 引用了 `lv00/plugin_system.h`，但该头文件**不存在**。

**需要创建**：

```c
// core/include/lv00/plugin_system.h
#ifndef LV00_PLUGIN_SYSTEM_H
#define LV00_PLUGIN_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* 插件事件类型 */
typedef enum {
    LV00_PLUGIN_EVENT_LOAD = 0,
    LV00_PLUGIN_EVENT_UNLOAD,
    LV00_PLUGIN_EVENT_ACTIVATE,
    LV00_PLUGIN_EVENT_DEACTIVATE,
    LV00_PLUGIN_EVENT_MESSAGE,
} Lv00PluginEventType;

/* 插件事件 */
typedef struct {
    Lv00PluginEventType type;
    const char* message;
    void* data;
    size_t data_size;
} Lv00PluginEvent;

/* 插件配置 */
typedef struct Lv00PluginConfig Lv00PluginConfig;

/* 插件接口 */
typedef struct {
    char name[64];
    int version;
    const char* description;
    const char* author;
    const char* license;
    void* vtable;  /* 虚函数表，用于扩展 */
} Lv00PluginInterface;

/* 插件上下文 */
typedef struct {
    void* plugin_handle;
    Lv00PluginConfig* config;
    void* user_data;
    void* engine_ctx;
} Lv00PluginContext;

/* 插件生命周期回调 */
typedef int (*lv00_plugin_on_load_fn)(Lv00PluginContext* ctx);
typedef int (*lv00_plugin_on_unload_fn)(Lv00PluginContext* ctx);
typedef int (*lv00_plugin_on_activate_fn)(Lv00PluginContext* ctx);
typedef int (*lv00_plugin_on_deactivate_fn)(Lv00PluginContext* ctx);
typedef int (*lv00_plugin_on_configure_fn)(Lv00PluginContext* ctx, const Lv00PluginConfig* config);
typedef int (*lv00_plugin_on_event_fn)(Lv00PluginContext* ctx, const Lv00PluginEvent* event);

/* 插件描述符 */
typedef struct {
    const char* name;
    int version;
    lv00_plugin_on_load_fn on_load;
    lv00_plugin_on_unload_fn on_unload;
    lv00_plugin_on_activate_fn on_activate;
    lv00_plugin_on_deactivate_fn on_deactivate;
    lv00_plugin_on_configure_fn on_configure;
    lv00_plugin_on_event_fn on_event;
} Lv00PluginDescriptor;

/* 插件系统 API */
typedef struct Lv00PluginSystem Lv00PluginSystem;
typedef struct Lv00Plugin Lv00Plugin;

Lv00PluginSystem* lv00_plugin_system_create(void* engine_ctx);
void lv00_plugin_system_destroy(Lv00PluginSystem* system);
void lv00_plugin_system_init(Lv00PluginSystem* system);
void lv00_plugin_system_add_search_path(Lv00PluginSystem* system, const char* path);

Lv00Plugin* lv00_plugin_load(Lv00PluginSystem* system, const char* path);
bool lv00_plugin_activate(Lv00Plugin* plugin);
bool lv00_plugin_deactivate(Lv00Plugin* plugin);
void lv00_plugin_unload(Lv00PluginSystem* system, Lv00Plugin* plugin);

bool lv00_plugin_register_interface(Lv00Plugin* plugin, Lv00PluginInterface* interface);
Lv00PluginInterface* lv00_plugin_query_interface(Lv00PluginSystem* system, const char* name, int version);

/* 配置 API */
Lv00PluginConfig* lv00_plugin_config_create(void);
void lv00_plugin_config_destroy(Lv00PluginConfig* config);
bool lv00_plugin_config_set(Lv00PluginConfig* config, const char* key, const char* value);
const char* lv00_plugin_config_get(Lv00PluginConfig* config, const char* key, const char* default_value);

/* 入口宏 */
#define LV00_PLUGIN_DECLARE(name) const char* lv00_plugin_name = name;
#define LV00_PLUGIN_ENTRY() \
    __attribute__((visibility("default"))) \
    const Lv00PluginDescriptor* lv00_plugin_get_descriptor(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PLUGIN_SYSTEM_H */
```

**需要实现**：
- `core/src/shared/plugin_system.c`：插件系统核心实现
- 动态库加载器（跨平台：Linux dlopen、Windows LoadLibrary、macOS dlopen）
- 插件生命周期管理（加载、初始化、激活、停用、卸载）
- 插件依赖解析和加载顺序控制
- 插件隔离和沙箱机制（可选）

#### 4.1.2 自定义函数注册接口

**当前状态**：`examples/custom_syntax_extension.c` 引用了 `func_block_register_custom`，但该函数**不存在**。

**需要创建**：

```c
// core/include/lv00/func_block_custom.h
#ifndef LV00_FUNC_BLOCK_CUSTOM_H
#define LV00_FUNC_BLOCK_CUSTOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* 前向声明 */
struct ConstraintGraph;
struct GeomNode;

/* 自定义函数元数据 */
typedef struct {
    const char* name;
    const char* description;
    const char* category;
    int min_inputs;
    int max_inputs;
    int output_count;
    const char** input_types;   /* 输入类型名称数组 */
    const char** output_types;  /* 输出类型名称数组 */
    const char** param_names;   /* 参数名称数组 */
} CustomFunctionMeta;

/* 自定义函数回调签名 */
typedef bool (*CustomFunctionCallback)(
    struct ConstraintGraph* graph,
    const int* input_node_ids,
    int input_count,
    int** output_node_ids,
    int* output_count,
    void* user_data
);

/* 自定义函数注册信息 */
typedef struct {
    CustomFunctionMeta meta;
    CustomFunctionCallback callback;
    void* user_data;
    void (*free_user_data)(void*);
} CustomFunctionRegistration;

/* 自定义函数管理 API */
bool lv00_func_block_register_custom(const CustomFunctionRegistration* reg);
bool lv00_func_block_unregister_custom(const char* name);
bool lv00_func_block_is_custom_registered(const char* name);
const CustomFunctionMeta* lv00_func_block_get_custom_meta(const char* name);

/* 批量注册 */
typedef struct {
    CustomFunctionRegistration* registrations;
    size_t count;
} CustomFunctionRegistry;

bool lv00_func_block_register_custom_batch(const CustomFunctionRegistry* registry);
bool lv00_func_block_unregister_custom_batch(const char** names, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FUNC_BLOCK_CUSTOM_H */
```

**需要实现**：
- `core/src/layer3_constraint/func_block_custom.c`：自定义函数注册表管理
- 函数名到回调的哈希映射
- 输入/输出类型验证
- 线程安全的注册/注销操作

#### 4.1.3 函数块模板系统

**当前状态**：`examples/custom_syntax_extension.c` 引用了函数块模板API，但**无实现**。

**需要创建**：

```c
// core/include/lv00/func_block_template.h
#ifndef LV00_FUNC_BLOCK_TEMPLATE_H
#define LV00_FUNC_BLOCK_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* 参数描述 */
typedef struct {
    char name[64];
    char type[64];
    char default_value[256];
    bool required;
    const char* description;
} FuncBlockTemplateParam;

/* 函数块模板 */
typedef struct FuncBlockTemplate FuncBlockTemplate;

/* 模板创建与销毁 */
FuncBlockTemplate* lv00_fb_template_create(const char* name, const char* description);
void lv00_fb_template_destroy(FuncBlockTemplate* tmpl);

/* 参数管理 */
bool lv00_fb_template_add_param(FuncBlockTemplate* tmpl, const FuncBlockTemplateParam* param);
bool lv00_fb_template_set_script(FuncBlockTemplate* tmpl, const char* script);
bool lv00_fb_template_set_version(FuncBlockTemplate* tmpl, const char* version);
bool lv00_fb_template_add_dependency(FuncBlockTemplate* tmpl, const char* dep_name);

/* 注册与查询 */
bool lv00_fb_template_register(FuncBlockTemplate* tmpl);
FuncBlockTemplate* lv00_fb_template_query(const char* name);
bool lv00_fb_template_unregister(const char* name);

/* 实例化 */
typedef struct {
    int* input_node_ids;
    int input_count;
    const char** param_values;
    int param_count;
} FuncBlockInstantiationArgs;

int lv00_fb_template_instantiate(
    const char* template_name,
    struct ConstraintGraph* graph,
    const FuncBlockInstantiationArgs* args
);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FUNC_BLOCK_TEMPLATE_H */
```

**需要实现**：
- `core/src/layer3_constraint/func_block_template.c`：模板注册表、脚本解析、实例化逻辑
- 模板脚本语言解析器（或嵌入Lua/Python）
- 依赖解析和拓扑排序

#### 4.1.4 Python嵌入桥接接口

**需要创建**（全新接口）：

```c
// core/include/lv00/python_embed.h
#ifndef LV00_PYTHON_EMBED_H
#define LV00_PYTHON_EMBED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* Python对象生命周期管理 */
typedef void* (*lv00_py_incref_fn)(void* obj);
typedef void (*lv00_py_decref_fn)(void* obj);
typedef const char* (*lv00_py_str_fn)(void* obj);

/* 注册Python运行时钩子 */
bool lv00_py_register_runtime_hooks(
    lv00_py_incref_fn incref,
    lv00_py_decref_fn decref,
    lv00_py_str_fn to_string
);

/* 注册Python函数作为几何构造器 */
typedef int (*lv00_py_constructor_fn)(
    void* py_func,              /* Python可调用对象 */
    struct ConstraintGraph* graph,
    const int* input_ids,
    int input_count,
    int** output_ids,
    int* output_count
);

bool lv00_py_register_constructor(
    const char* name,
    lv00_py_constructor_fn wrapper,
    void* py_func
);

/* 注册Python求解器 */
typedef bool (*lv00_py_solver_fn)(
    void* py_func,
    struct ConstraintGraph* graph,
    double timeout_seconds,
    int* status
);

bool lv00_py_register_solver(
    const char* name,
    lv00_py_solver_fn wrapper,
    void* py_func
);

/* 注册Python重写规则 */
typedef bool (*lv00_py_rewrite_fn)(
    void* py_func,
    struct ConstraintGraph* graph,
    int node_id,
    int** replacement_ids,
    int* replacement_count
);

bool lv00_py_register_rewrite_rule(
    const char* name,
    lv00_py_rewrite_fn wrapper,
    void* py_func
);

/* 从Python加载模块 */
int lv00_py_load_module(struct LV00Engine* engine, const char* python_module_path);

/* Python异常转换 */
typedef struct {
    int error_code;
    char message[1024];
    char traceback[4096];
} Lv00PythonError;

bool lv00_py_get_last_error(Lv00PythonError* out_error);
void lv00_py_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PYTHON_EMBED_H */
```

**需要实现**：
- `core/src/shared/python_embed.c`：Python解释器嵌入、GIL管理、异常转换
- Python C-API的封装和错误处理
- 线程安全的Python调用机制

#### 4.1.5 DSL编译器扩展接口

**当前状态**：`examples/custom_syntax_extension.c` 引用了DSL扩展API，但**无实现**。

**需要创建**：

```c
// core/include/lv00/dsl_extension.h
#ifndef LV00_DSL_EXTENSION_H
#define LV00_DSL_EXTENSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* DSL版本 */
typedef struct {
    int major;
    int minor;
    int patch;
} DslVersion;

/* 解析钩子 */
typedef bool (*DslParseHook)(
    const char* source,
    size_t source_len,
    void* ast_out,
    void* user_data
);

/* 代码生成钩子 */
typedef bool (*DslCodegenHook)(
    void* ast,
    char** output,
    size_t* output_len,
    void* user_data
);

/* DSL扩展注册 */
typedef struct {
    const char* name;
    const char* version;
    DslParseHook parse_hook;
    DslCodegenHook codegen_hook;
    void* user_data;
} DslExtensionRegistration;

bool lv00_dsl_register_extension(const DslExtensionRegistration* reg);
bool lv00_dsl_unregister_extension(const char* name);

/* 版本控制 */
bool lv00_dsl_version_parse(const char* version_str, DslVersion* out_version);
bool lv00_dsl_version_compare(const DslVersion* a, const DslVersion* b, int* out_result);
bool lv00_dsl_syntax_transform(
    const char* source,
    const DslVersion* from_version,
    const DslVersion* to_version,
    char** out_transformed
);

#ifdef __cplusplus
}
#endif

#endif /* LV00_DSL_EXTENSION_H */
```

**需要实现**：
- `core/src/layer1_parser/dsl_extension.c`：扩展注册表、版本解析、语法转换
- DSL版本迁移工具

#### 4.1.6 错误消息系统

**当前状态**：`examples/custom_syntax_extension.c` 引用了错误消息API，但**无实现**。

**需要创建**：

```c
// core/include/lv00/error_messages.h
#ifndef LV00_ERROR_MESSAGES_H
#define LV00_ERROR_MESSAGES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* 错误类别 */
typedef enum {
    LV00_ERR_CATEGORY_SYNTAX = 0,
    LV00_ERR_CATEGORY_TYPE,
    LV00_ERR_CATEGORY_CONSTRAINT,
    LV00_ERR_CATEGORY_REASONING,
    LV00_ERR_CATEGORY_RUNTIME,
    LV00_ERR_CATEGORY_PLUGIN,
    LV00_ERR_CATEGORY_COUNT
} Lv00ErrorCategory;

/* 错误消息 */
typedef struct {
    int code;
    Lv00ErrorCategory category;
    const char* message;        /* 英文 */
    const char* message_cn;     /* 中文 */
    const char* suggestion;     /* 修复建议 */
    const char* documentation;  /* 相关文档链接 */
} Lv00ErrorMessage;

/* 错误消息查询 */
const Lv00ErrorMessage* lv00_get_error_message(int error_code);
const char* lv00_error_category_name(Lv00ErrorCategory category);
const char* lv00_error_category_name_cn(Lv00ErrorCategory category);

/* 错误消息注册（允许插件扩展） */
typedef struct {
    int code;
    Lv00ErrorCategory category;
    const char* message;
    const char* message_cn;
    const char* suggestion;
} Lv00ErrorMessageRegistration;

bool lv00_register_error_message(const Lv00ErrorMessageRegistration* reg);
bool lv00_unregister_error_message(int code);

/* 格式化错误输出 */
int lv00_format_error(char* buffer, size_t buffer_size, int error_code, const char* context);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ERROR_MESSAGES_H */
```

**需要实现**：
- `core/src/shared/error_messages.c`：错误消息注册表、查询、格式化
- 多语言支持框架

### 4.2 Python绑定层补全

#### 4.2.1 核心绑定模块结构

**需要创建**：`core/src/python_binding/` 目录及以下文件：

| 文件 | 职责 | 来源 |
|------|------|------|
| `__init__.py` | 包入口，导出公共API | 新建 |
| `_ctypes_binding.py` | C库ctypes绑定（从module迁移） | 迁移 |
| `native_types.py` | SymbolicCoord/Graph/GeomNode等C类型封装（从core.py迁移） | 拆分迁移 |
| `engine.py` | Engine类C委托方法（从engine.py迁移） | 拆分迁移 |
| `groebner.py` | Groebner引擎绑定（从groebner_engine.py迁移） | 迁移 |
| `sparse_la.py` | 稀疏LA绑定（从sparse_la.py迁移） | 迁移 |
| `type_system.py` | 类型系统绑定（从type_system.py迁移） | 迁移 |
| `func_block.py` | 函数块系统绑定（从func_block.py迁移） | 迁移 |
| `formula_native.py` | FormulaNode C委托（从formula.py拆分） | 拆分迁移 |
| `proof_native.py` | 证明系统C委托（从proof_extras.py拆分） | 拆分迁移 |
| `stream_native.py` | 流式C操作（从stream_bridge.py拆分） | 拆分迁移 |
| `interactive_geo_native.py` | 交互几何C核心（从interactive_geo.py拆分） | 拆分迁移 |
| `utils_native.py` | C指针验证工具（从utils拆分） | 拆分迁移 |

#### 4.2.2 绑定层需要新增的代码

| 组件 | 说明 | 优先级 |
|------|------|--------|
| `PyEngine` 类 | 对C Engine的Pythonic封装 | 高 |
| `PyConstraintGraph` 类 | 对C ConstraintGraph的Pythonic封装 | 高 |
| `PySymbolicCoord` 类 | 对C SymbolicCoord的Pythonic封装 | 高 |
| 异常转换层 | C错误码 → Python异常 | 高 |
| 内存管理适配器 | Python GC ↔ C内存池 | 高 |
| 回调函数适配器 | Python callable ↔ C回调 | 中 |
| 迭代器适配器 | C数组 ↔ Python迭代器 | 中 |
| 上下文管理器 | `with`语句支持 | 低 |

### 4.3 后5层需要补全的内容

#### 4.3.1 第6层：交互与可视化层

| 缺失内容 | 说明 | 优先级 |
|----------|------|--------|
| `layer6_interactive/web_gui/` | React前端迁移（从`web/gui/`） | 高 |
| `layer6_interactive/python_gui/` | tkinter GUI迁移（从`interactive_geo.py`拆分） | 中 |
| `layer6_interactive/renderers/` | 公式渲染器（SVG/LaTeX/ASCII，从`formula.py`拆分） | 中 |
| `layer6_interactive/websocket/` | WebSocket服务器（从`ws_server.py`迁移） | 中 |
| `layer6_interactive/penrose_renderer/` | Penrose风格渲染器 | 低 |
| 画布-核心同步协议 | 定义UI状态与核心状态的同步契约 | 高 |
| 交互事件规范 | 定义鼠标/键盘/触摸事件到核心操作的映射 | 高 |

#### 4.3.2 第7层：语言绑定与运行时层

| 缺失内容 | 说明 | 优先级 |
|----------|------|--------|
| `layer7_binding/python/highlevel/` | PyEuclid风格API（`py_euclid_style.py`） | 高 |
| `layer7_binding/python/highlevel/high_dim.py` | 高维几何（`high_dim.py`） | 低 |
| `layer7_binding/python/presets/` | 预设函数块库（`preset_*.py`） | 高 |
| `layer7_binding/python/dsl/` | DSL语法工具（`dsl*.py`） | 中 |
| `layer7_binding/python/streaming/` | 异步流适配（`async_stream.py`、`stream_bridge.py` Python侧） | 中 |
| `layer7_binding/python/helpers/` | 引擎辅助（`engine.py`辅助方法） | 中 |
| `layer7_binding/js_ts/` | JavaScript/TypeScript绑定（从`web/gui/src/engine/`迁移） | 高 |
| `layer7_binding/wasm/` | WASM绑定 | 中 |
| 包入口重构 | `lv00/__init__.py`重新设计，支持惰性加载 | 高 |
| 类型存根文件 | `.pyi`文件，支持IDE类型提示 | 中 |

#### 4.3.3 第8层：智能辅助层

| 缺失内容 | 说明 | 优先级 |
|----------|------|--------|
| `layer8_ai_assistant/core/ai_engine.py` | AI引擎核心（从`module/llm_coding_assistant/core/`迁移） | 高 |
| `layer8_ai_assistant/core/code_analyzer.py` | 代码分析器（从`module/llm_coding_assistant/core/`迁移） | 高 |
| `layer8_ai_assistant/api_server/` | FastAPI服务（从`module/llm_coding_assistant/api_server.py`迁移） | 高 |
| `layer8_ai_assistant/knowledge_base/` | 领域知识库（从`module/llm_coding_assistant/lv00_knowledge.py`迁移） | 中 |
| `layer8_ai_assistant/templates/` | 代码模板（从`module/llm_coding_assistant/templates.py`迁移） | 中 |
| AI-核心通信协议 | 定义LLM与第7层绑定的通信契约 | 高 |
| 提示词版本管理 | 提示词模板版本控制和A/B测试 | 低 |

#### 4.3.4 第9层：运行时监控与运维层

| 缺失内容 | 说明 | 优先级 |
|----------|------|--------|
| `layer9_monitoring/core/` | 监控引擎（从`module/concurrent_monitor/core/`迁移） | 高 |
| `layer9_monitoring/web_dashboard/` | Web仪表盘（从`module/concurrent_monitor/web/`迁移） | 中 |
| `layer9_monitoring/cli/` | CLI工具（从`module/concurrent_monitor/cli/`迁移） | 中 |
| `layer9_monitoring/tests/` | 监控测试（从`module/concurrent_monitor/tests/`迁移） | 中 |
| 监控数据采集协议 | 定义性能指标、日志、事件的采集格式 | 高 |
| 告警规则引擎 | 可配置的告警条件和通知渠道 | 低 |

#### 4.3.5 第10层：形式化验证与生态层

| 缺失内容 | 说明 | 优先级 |
|----------|------|--------|
| `layer10_ecosystem/formal_verification/` | Lean形式化（从`formal/`、`lv00-formal/`迁移） | 中 |
| `layer10_ecosystem/examples/` | 示例库（从`examples/`迁移） | 高 |
| `layer10_ecosystem/axiom_packages/` | 公理包库（从`module/axiom_packages/`迁移） | 高 |
| `layer10_ecosystem/docs/` | 技术文档（从`doc/`迁移） | 高 |
| `layer10_ecosystem/tools/` | 生态工具（从`tool/`迁移） | 中 |
| 形式化-核心等价性证明 | 证明Lean定义与C核心语义等价 | 低 |
| 文档自动生成流水线 | 从代码注释生成API文档 | 中 |

### 4.4 架构层面需要补全的内容

#### 4.4.1 层间通信契约

| 契约 | 说明 | 优先级 |
|------|------|--------|
| 第5层→第6层数据契约 | Proof Object → 可视化模型的转换规范 | 高 |
| 第6层→第4层控制契约 | UI操作 → 推理请求的映射规范 | 高 |
| 第7层→第1-5层绑定契约 | Python调用C的参数/返回值规范 | 高 |
| 第7层→第6层事件契约 | 流式事件 → UI更新的推送规范 | 中 |
| 第8层→第7层AI契约 | LLM请求/响应格式规范 | 中 |
| 第9层→第7层监控契约 | 性能指标采集格式规范 | 低 |

#### 4.4.2 测试策略

| 测试类型 | 说明 | 优先级 |
|----------|------|--------|
| 层间依赖测试 | 验证无跨层违规依赖 | 高 |
| 绑定层兼容性测试 | 验证Python绑定与C核心版本兼容 | 高 |
| 插件系统测试 | 验证插件加载/激活/停用/卸载生命周期 | 高 |
| 流式事件测试 | 验证异步事件流的正确性和性能 | 中 |
| AI辅助测试 | 验证LLM生成代码的正确性 | 中 |
| 监控告警测试 | 验证监控数据采集和告警触发 | 低 |
| 形式化等价性测试 | 验证Lean定义与C核心语义一致 | 低 |

#### 4.4.3 CI/CD适配

| 适配项 | 说明 | 优先级 |
|--------|------|--------|
| GitHub Actions工作流更新 | 适配新目录结构的构建和测试流程 | 高 |
| Python包构建配置 | `setup.py`/`pyproject.toml`适配新结构 | 高 |
| 多平台构建支持 | Windows/Linux/macOS的CI构建 | 高 |
| 绑定层版本同步 | C核心与Python绑定的版本锁定机制 | 中 |
| 插件签名验证 | CI中验证插件包的完整性和签名 | 低 |

#### 4.4.4 文档补全

| 文档 | 说明 | 优先级 |
|------|------|--------|
| `ARCHITECTURE_MANUAL.md`更新 | 补充第6-10层完整定义 | 高 |
| `PYTHON_BINDING_GUIDE.md` | Python绑定开发指南 | 高 |
| `PLUGIN_DEVELOPMENT_GUIDE.md` | 插件开发指南 | 高 |
| `LAYER_BOUNDARY_SPEC.md` | 层间边界和通信契约规范 | 高 |
| `MIGRATION_GUIDE.md` | 从旧结构迁移到新结构的指南 | 中 |
| `API_REFERENCE.md`更新 | 补充新扩展接口的API文档 | 中 |

---

## 5. 优化后的完整目录结构

```
Lv-00/
├── core/                                    # 前5层 + Shared（目录不变）
│   ├── include/lv00/
│   │   ├── shared/                          # Shared 公共基础层
│   │   │   ├── error_codes.h
│   │   │   ├── memory_pool.h
│   │   │   ├── runtime_guard.h
│   │   │   ├── context.h
│   │   │   ├── cross_platform.h
│   │   │   ├── plugin_system.h              # [新增] 插件系统
│   │   │   ├── python_embed.h               # [新增] Python嵌入桥接
│   │   │   └── error_messages.h             # [新增] 错误消息系统
│   │   ├── layer1_parser/
│   │   │   ├── lexer.h
│   │   │   ├── parser.h
│   │   │   ├── ast.h
│   │   │   ├── typed_ir.h
│   │   │   └── dsl_extension.h              # [新增] DSL扩展接口
│   │   ├── layer2_axiom/
│   │   │   ├── geometry_ontology.h
│   │   │   ├── metric_relations.h
│   │   │   └── euclidean_axioms.h
│   │   ├── layer3_constraint/
│   │   │   ├── constraint_graph.h
│   │   │   ├── normalization.h
│   │   │   ├── func_block_custom.h          # [新增] 自定义函数注册
│   │   │   └── func_block_template.h        # [新增] 函数块模板
│   │   ├── layer4_reasoning/
│   │   │   ├── proof.h
│   │   │   ├── solver.h
│   │   │   └── reasoning_strategy.h
│   │   ├── layer5_output/
│   │   │   ├── proof_formatting.h
│   │   │   ├── cross_language_export.h
│   │   │   └── visualization.h
│   │   └── python_binding/                  # [新增] Python绑定头文件
│   │       ├── lv00_python.h
│   │       └── native_objects.h
│   └── src/
│       ├── shared/
│       │   ├── error_codes.c
│       │   ├── memory_pool.c
│       │   ├── plugin_system.c              # [新增] 插件系统实现
│       │   ├── python_embed.c               # [新增] Python嵌入实现
│       │   └── error_messages.c             # [新增] 错误消息实现
│       ├── layer1_parser/
│       ├── layer2_axiom/
│       ├── layer3_constraint/
│       │   ├── func_block_custom.c          # [新增] 自定义函数实现
│       │   └── func_block_template.c        # [新增] 函数块模板实现
│       ├── layer4_reasoning/
│       ├── layer5_output/
│       └── python_binding/                  # [新增] Python绑定实现
│           ├── __init__.py
│           ├── _ctypes_binding.py           # [迁移]
│           ├── native_types.py              # [拆分迁移]
│           ├── engine.py                    # [拆分迁移]
│           ├── groebner.py                  # [迁移]
│           ├── sparse_la.py                 # [迁移]
│           ├── type_system.py               # [迁移]
│           ├── func_block.py                # [迁移]
│           ├── formula_native.py            # [拆分迁移]
│           ├── proof_native.py              # [拆分迁移]
│           ├── stream_native.py             # [拆分迁移]
│           ├── interactive_geo_native.py    # [拆分迁移]
│           └── utils_native.py              # [拆分迁移]
│
├── layer6_interactive/                      # 第6层：交互与可视化
│   ├── web_gui/                             # [迁移] React前端
│   │   ├── src/
│   │   ├── package.json
│   │   └── vite.config.ts
│   ├── python_gui/                          # [拆分迁移] tkinter GUI
│   │   └── interactive_geo_ui.py
│   ├── renderers/                           # [拆分迁移] 公式渲染
│   │   ├── svg_renderer.py
│   │   ├── latex_renderer.py
│   │   ├── ascii_renderer.py
│   │   └── unicode_renderer.py
│   └── websocket/                           # [迁移] WebSocket服务
│       └── ws_server.py
│
├── layer7_binding/                          # 第7层：语言绑定与运行时
│   └── python/
│       ├── __init__.py                      # [重构] 包入口
│       ├── highlevel/                       # [迁移] 高层API
│       │   ├── py_euclid_style.py
│       │   └── high_dim.py
│       ├── presets/                         # [迁移] 预设库
│       │   ├── preset_basic.py
│       │   ├── preset_analysis.py
│       │   ├── preset_algebra.py
│       │   ├── preset_topology.py
│       │   ├── preset_func_blocks.py
│       │   └── math_presets.py
│       ├── dsl/                             # [迁移] DSL工具
│       │   ├── dsl.py
│       │   ├── dsl_wrappers.py
│       │   └── dsl_context.py
│       ├── streaming/                       # [拆分迁移] 流式适配
│       │   ├── async_stream.py
│       │   └── stream_bridge.py             # Python侧队列管理
│       └── helpers/                         # [拆分迁移] 辅助工具
│           └── engine_helpers.py
│
├── layer8_ai_assistant/                     # 第8层：智能辅助
│   ├── core/                                # [迁移] AI核心
│   │   ├── ai_engine.py
│   │   └── code_analyzer.py
│   ├── api_server/                          # [迁移] API服务
│   │   └── api_server.py
│   ├── knowledge_base/                      # [迁移] 知识库
│   │   └── lv00_knowledge.py
│   └── templates/                           # [迁移] 代码模板
│       └── templates.py
│
├── layer9_monitoring/                       # 第9层：监控运维
│   ├── core/                                # [迁移] 监控核心
│   │   ├── engine.py
│   │   ├── events.py
│   │   ├── models.py
│   │   └── config.py
│   ├── web_dashboard/                       # [迁移] Web仪表盘
│   │   ├── dashboard.py
│   │   ├── routes.py
│   │   └── templates.py
│   ├── cli/                                 # [迁移] CLI工具
│   │   └── monitor.py
│   └── tests/                               # [迁移] 监控测试
│       └── ...
│
├── layer10_ecosystem/                       # 第10层：形式化与生态
│   ├── formal_verification/                 # [迁移] Lean形式化
│   │   ├── formal/                          # 原formal/
│   │   └── lv00_formal/                     # 原lv00-formal/
│   ├── examples/                            # [迁移] 示例库
│   │   ├── library/
│   │   ├── templates/
│   │   └── plugin_example/
│   ├── axiom_packages/                      # [迁移] 公理包
│   │   └── ...
│   ├── docs/                                # [迁移] 技术文档
│   │   ├── docs/
│   │   └── generate_version_doc.js
│   └── tools/                               # [迁移] 生态工具
│       └── scripts/
│
├── tests/                                   # 核心测试套件
├── cmake/                                   # CMake配置
└── .github/                                 # CI/CD配置
    └── workflows/
```

---

## 6. 迁移与补全路线图

### 阶段1：C层扩展接口实现（Week 1-3）

**目标**：实现所有缺失的C层扩展接口

| 周次 | 任务 | 产出 |
|------|------|------|
| Week 1 | 实现插件系统 | `plugin_system.h` + `plugin_system.c` |
| Week 1 | 实现错误消息系统 | `error_messages.h` + `error_messages.c` |
| Week 2 | 实现自定义函数注册 | `func_block_custom.h` + `func_block_custom.c` |
| Week 2 | 实现函数块模板系统 | `func_block_template.h` + `func_block_template.c` |
| Week 3 | 实现Python嵌入桥接 | `python_embed.h` + `python_embed.c` |
| Week 3 | 实现DSL扩展接口 | `dsl_extension.h` + `dsl_extension.c` |

### 阶段2：Python绑定下沉（Week 4-5）

**目标**：将核心绑定代码迁移到`core/src/python_binding/`

| 周次 | 任务 | 产出 |
|------|------|------|
| Week 4 | 迁移基础绑定 | `_ctypes_binding.py`, `native_types.py` |
| Week 4 | 迁移引擎绑定 | `engine.py`, `groebner.py`, `sparse_la.py` |
| Week 5 | 迁移系统绑定 | `type_system.py`, `func_block.py` |
| Week 5 | 拆分混合模块 | `formula_native.py`, `proof_native.py`, `stream_native.py` |

### 阶段3：后5层重组（Week 6-7）

**目标**：按新架构重组后5层代码

| 周次 | 任务 | 产出 |
|------|------|------|
| Week 6 | 重组第6-7层 | `layer6_interactive/`, `layer7_binding/` |
| Week 7 | 重组第8-10层 | `layer8_ai_assistant/`, `layer9_monitoring/`, `layer10_ecosystem/` |

### 阶段4：架构补全（Week 8-9）

**目标**：补全所有架构层面内容

| 周次 | 任务 | 产出 |
|------|------|------|
| Week 8 | 层间通信契约 | `LAYER_BOUNDARY_SPEC.md` |
| Week 8 | 测试策略实施 | 层间依赖测试、绑定兼容性测试 |
| Week 9 | CI/CD适配 | 更新GitHub Actions、Python包配置 |
| Week 9 | 文档补全 | 更新架构手册、开发指南、API参考 |

### 阶段5：验证与发布（Week 10）

**目标**：全面验证和文档发布

| 任务 | 验收标准 |
|------|----------|
| 编译验证 | 所有目标平台编译无错误 |
| 依赖检查 | 静态分析确认无跨层违规 |
| 测试验证 | 所有现有测试通过 + 新增测试通过 |
| 文档验证 | 架构文档反映完整十层结构 |
| CI/CD验证 | 持续集成流水线运行正常 |

---

## 7. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| C层扩展接口设计不当 | 后期大规模重构 | 先设计评审，再实现；提供兼容性层 |
| Python绑定下沉导致兼容性问题 | 现有用户代码失效 | 保留旧导入路径的兼容性shim |
| 混合模块拆分引入bug | 功能退化 | 拆分前增加测试覆盖；拆分后对比测试 |
| 插件系统安全漏洞 | 恶意代码执行 | 插件签名验证、沙箱机制、权限控制 |
| 多平台构建失败 | 发布延迟 | CI中覆盖Windows/Linux/macOS |
| 文档滞后 | 开发者困惑 | 文档与代码同步更新，文档即代码 |

---

## 8. 附录

### 8.1 模块归属速查表

| 模块 | 当前位置 | 目标位置 | 操作 |
|------|----------|----------|------|
| C库ctypes绑定 | `module/python/lv00/_ctypes_binding.py` | `core/src/python_binding/` | 迁移 |
| 核心类型封装 | `module/python/lv00/core.py` (部分) | `core/src/python_binding/native_types.py` | 拆分迁移 |
| Engine类 | `module/python/lv00/engine.py` (部分) | `core/src/python_binding/engine.py` | 拆分迁移 |
| Groebner引擎 | `module/python/lv00/groebner_engine.py` | `core/src/python_binding/groebner.py` | 迁移 |
| 稀疏LA | `module/python/lv00/sparse_la.py` | `core/src/python_binding/sparse_la.py` | 迁移 |
| 类型系统 | `module/python/lv00/type_system.py` | `core/src/python_binding/type_system.py` | 迁移 |
| 函数块 | `module/python/lv00/func_block.py` | `core/src/python_binding/func_block.py` | 迁移 |
| 公式C委托 | `module/python/lv00/formula.py` (部分) | `core/src/python_binding/formula_native.py` | 拆分迁移 |
| 证明C委托 | `module/python/lv00/proof_extras.py` (部分) | `core/src/python_binding/proof_native.py` | 拆分迁移 |
| 流式C操作 | `module/python/lv00/stream_bridge.py` (部分) | `core/src/python_binding/stream_native.py` | 拆分迁移 |
| 交互几何核心 | `module/python/lv00/interactive_geo.py` (部分) | `core/src/python_binding/interactive_geo_native.py` | 拆分迁移 |
| PyEuclid API | `module/python/lv00/py_euclid_style.py` | `layer7_binding/python/highlevel/` | 迁移 |
| 高维几何 | `module/python/lv00/high_dim.py` | `layer7_binding/python/highlevel/` | 迁移 |
| 预设库 | `module/python/lv00/preset_*.py` | `layer7_binding/python/presets/` | 迁移 |
| DSL工具 | `module/python/lv00/dsl*.py` | `layer7_binding/python/dsl/` | 迁移 |
| 异步流 | `module/python/lv00/async_stream.py` | `layer7_binding/python/streaming/` | 迁移 |
| WebSocket | `module/python/lv00/ws_server.py` | `layer6_interactive/websocket/` | 迁移 |
| tkinter GUI | `module/python/lv00/interactive_geo.py` (部分) | `layer6_interactive/python_gui/` | 拆分迁移 |
| 公式渲染 | `module/python/lv00/formula.py` (部分) | `layer6_interactive/renderers/` | 拆分迁移 |
| React前端 | `web/gui/src/` | `layer6_interactive/web_gui/` | 迁移 |
| LLM辅助 | `module/llm_coding_assistant/` | `layer8_ai_assistant/` | 迁移 |
| 并发监控 | `module/concurrent_monitor/` | `layer9_monitoring/` | 迁移 |
| Lean形式化 | `formal/`, `lv00-formal/` | `layer10_ecosystem/formal_verification/` | 迁移 |
| 示例库 | `examples/` | `layer10_ecosystem/examples/` | 迁移 |
| 公理包 | `module/axiom_packages/` | `layer10_ecosystem/axiom_packages/` | 迁移 |
| 技术文档 | `doc/` | `layer10_ecosystem/docs/` | 迁移 |
| 生态工具 | `tool/` | `layer10_ecosystem/tools/` | 迁移 |

### 8.2 需要新建的C头文件清单

| 头文件 | 路径 | 说明 |
|--------|------|------|
| `plugin_system.h` | `core/include/lv00/plugin_system.h` | 插件系统 |
| `python_embed.h` | `core/include/lv00/python_embed.h` | Python嵌入桥接 |
| `error_messages.h` | `core/include/lv00/error_messages.h` | 错误消息系统 |
| `func_block_custom.h` | `core/include/lv00/func_block_custom.h` | 自定义函数注册 |
| `func_block_template.h` | `core/include/lv00/func_block_template.h` | 函数块模板 |
| `dsl_extension.h` | `core/include/lv00/dsl_extension.h` | DSL扩展接口 |

### 8.3 需要新建的C实现文件清单

| 实现文件 | 路径 | 说明 |
|----------|------|------|
| `plugin_system.c` | `core/src/shared/plugin_system.c` | 插件系统实现 |
| `python_embed.c` | `core/src/shared/python_embed.c` | Python嵌入实现 |
| `error_messages.c` | `core/src/shared/error_messages.c` | 错误消息实现 |
| `func_block_custom.c` | `core/src/layer3_constraint/func_block_custom.c` | 自定义函数实现 |
| `func_block_template.c` | `core/src/layer3_constraint/func_block_template.c` | 函数块模板实现 |
| `dsl_extension.c` | `core/src/layer1_parser/dsl_extension.c` | DSL扩展实现 |

---

## 9. C层重写评估清单

> 本章标记所有**需要在C层面重写/补全**的代码，包括：Python中应下沉到C的实现、C核心中缺失的头文件和实现文件、以及示例代码引用的不存在的API。

### 9.1 Python代码需要C重写的部分

#### 9.1.1 P0 — 性能关键路径 + 精度丢失（必须重写）

| # | 模块 | 代码位置 | 功能 | 重写理由 | C实现位置 | 工作量 |
|---|------|---------|------|----------|----------|--------|
| R1 | `core.py` | L876-896 | `Point.distance_to()` | 高频调用，每次5+次FFI跨界开销；与C层`point_distance_sq`平行实现 | `layer3_geometry/geometry_csg.c` | 1人天 |
| R2 | `core.py` | L898-911 | `Point.mid_point()` | 高频调用，每次6+次FFI；反复创建`SymbolicCoord.from_rational(2)`常量 | `layer3_geometry/geometry_csg.c` | 0.5人天 |
| R3 | `core.py` | L957-975 | `Point.is_collinear_with()` | 约束验证核心操作；与C层共线检测可能不一致 | `layer3_geometry/geometry_csg.c` | 0.5人天 |
| R4 | `core.py` | L1329-1355 | `Graph` Python侧状态追踪 | **最高风险**：Python侧`_points`列表与C层节点管理完全独立，Engine直接操作时不会同步 | `layer3_geometry/constraint_graph.c` | 5人天 |
| R5 | `dsl_context.py` | L511-538 | `G._intersect_lines()` | **精度丢失**：将SymbolicCoord转为float，完全丢失符号精度 | `layer3_geometry/geometry_csg.c` | 1.5人天 |
| R6 | `dsl_context.py` | L540-580 | `G._intersect_circles()` | **精度丢失**：同上，且与`CircleWrapper.intersect_line()`精度策略不一致 | `layer3_geometry/geometry_csg.c` | 1.5人天 |
| R7 | `dsl_wrappers.py` | L738-886 | `TriangleWrapper`几何方法集（area/circumcenter/orthocenter/incenter等8个方法） | **精度丢失**：全部使用float+math.sqrt；与`preset_analysis.py`的精确计算重复 | `layer3_geometry/geometry_csg.c` | 3人天 |

**P0小计：约12人天**

#### 9.1.2 P1 — 数据一致性 + 架构耦合（应该重写）

| # | 模块 | 代码位置 | 功能 | 重写理由 | C实现位置 | 工作量 |
|---|------|---------|------|----------|----------|--------|
| R8 | `core.py` | L1058-1178 | `LineSegment`几何方法集（length/midpoint/direction/parallel/perpendicular等8个方法） | 高频调用；叉积/点积零值判断在Python和C可能使用不同精度阈值 | `layer3_geometry/geometry_csg.c` | 2人天 |
| R9 | `core.py` | L913-955 | `Point.translate()`/`Point.reflect_over_point()` | 与`preset_algebra.py`的`create_reflection/create_translation`多层平行实现 | `layer3_geometry/geometry_csg.c` | 1人天 |
| R10 | `engine.py` | L578-613 | `Engine.unify_detailed()` | 注释说明C函数已移除，Python侧模拟诊断丢失精度；硬编码中文字符串 | `layer4_reasoning/proof_engine_enhanced.c` | 2人天 |
| R11 | `constraints.py` | L67-309 | `Constraint`类型系统（from_dict反序列化 + 约束子类层次） | Python侧约束类型与C层约束枚举是两套独立系统，映射关系隐式无编译时保证 | `layer3_geometry/constraint_graph.c` | 7人天 |
| R12 | `dsl_context.py` | L597-625 | `G.angle()` | 精度丢失（math.acos浮点运算） | `layer3_geometry/geometry_csg.c` | 0.5人天 |
| R13 | `stream_bridge.py` | L264-306 | `EngineBridge._handle_event()` | C层已有`stream_event_to_json()`但Python未使用，手动构建字典产生不必要开销 | 直接使用已有C函数 | 0.5人天 |
| R14 | `preset_*.py` | 多处 | 预设函数块实现（create_midpoint/create_circumcenter等） | 与C层`preset_blocks.c`重复；Python侧`FuncBlockSpec`注册表和C侧预设注册表无同步 | `layer4_reasoning/func_block/preset_common.c` | 5人天 |
| R15 | `preset_*.py` | 多处 | `SymbolicCoord`常量反复创建（from_rational(2)/from_rational(3)等） | 每次调用触发C内存分配 | `layer3_geometry/symbolic_coord.c`新增常量池 | 1人天 |

**P1小计：约19人天**

#### 9.1.3 P2 — 优化改进（可以重写）

| # | 模块 | 代码位置 | 功能 | 重写理由 | C实现位置 | 工作量 |
|---|------|---------|------|----------|----------|--------|
| R16 | `dsl_algebra.py` | L92-112 | `Transform._transform_point()` | 变换链中每个对象独立应用，复杂图形可能数百次浮点运算 | `layer3_geometry/geometry_csg.c` | 1人天 |
| R17 | `engine.py` | L819-863 | `Engine.solve_with_retry()`/`Engine.safe_solve()` | Python侧重试每次跨越FFI边界；C层可实现更高效的指数退避 | `layer4_reasoning/engine.c` | 1人天 |
| R18 | `proof_extras.py` | L331-386 | `ProofMultiStrategy.pipeline()`/`try_all()`策略编排 | Python组装c_int数组传C，策略组合场景产生不必要FFI开销 | `layer4_reasoning/proof_multi_strategy.c` | 2人天 |

**P2小计：约4人天**

#### 9.1.4 P3 — 低优先级（暂不重写）

| # | 模块 | 代码位置 | 功能 | 不重写理由 |
|---|------|---------|------|-----------|
| R19 | `dsl_algebra.py` | L133-144 | `Transform._compose()` | 纯参数加乘，性能影响可忽略 |
| R20 | `proof_extras.py` | L538-559 | `proof_minimal_verify()`字符串数组 | I/O密集型，Python足够 |
| — | `ws_server.py` | 全部 | WebSocket服务器 | 网络I/O，asyncio足够 |
| — | `async_stream.py` | 全部 | 异步流式适配 | asyncio足够 |
| — | `normalization.py` | 全部 | 规范化结果查询 | 性能不敏感 |

### 9.2 C核心缺失的头文件（需要创建）

#### 9.2.1 P0 — 致命级（阻止编译）

| # | 头文件 | 被引用次数 | 引用者 | 说明 | 工作量 |
|---|--------|-----------|--------|------|--------|
| H1 | `lv00_internal.h` | **40+个文件** | 几乎所有层源文件 | 内部数据结构、常量、工具宏。**最大单一阻塞点** | 5人天 |
| H2 | `stream_context_util.h` | 10个文件 | layer1/3/4/5多个源文件 | 流上下文工具 | 1人天 |
| H3 | `solver_core.h` | 2个文件 | `solver_core.c`, `sat_encoding.c` | 求解器核心 | 0.5人天 |
| H4 | `constraint_graph_safe.h` | 1个文件 | `layer3_geometry/constraint_graph.c` | 安全操作辅助 | 0.5人天 |
| H5 | `prop_verifier.h` | 2个文件 | `layer4_reasoning/engine.c`, `stream_context_util.c` | 命题验证器 | 1人天 |
| H6 | `parser_safety.h` | 1个文件 | `layer1_parser/formula_parser.c` | 解析器安全 | 0.5人天 |

**P0小计：约8.5人天**

#### 9.2.2 P1 — 高优先级（API不完整）

| # | 头文件 | 引用者 | 说明 | 工作量 |
|---|--------|--------|------|--------|
| H7 | `smt_backend.h` | `smt_backend_impl.c` | SMT后端接口 | 1人天 |
| H8 | `conflict_detector.h` | `conflict_detector.c` | 冲突检测器 | 0.5人天 |
| H9 | `relation_model.h` | `relation_model.c` | 关系模型 | 1人天 |
| H10 | `probabilistic_constraint.h` | `probabilistic_constraint.c` | 概率约束 | 0.5人天 |
| H11 | `sat_encoding.h` | `sat_encoding.c` | SAT编码 | 0.5人天 |
| H12 | `geo_visual.h` | `layer5_output/geo_visual.c` | 几何可视化 | 1人天 |
| H13 | `geometry_config.h` + `geo_utils.h` | `conflict_detector.c` | 几何配置和工具 | 0.5人天 |

**P1小计：约5人天**

### 9.3 C核心缺失的实现文件（需要创建）

#### 9.3.1 P0 — 致命级（Python绑定断裂）

| # | 实现文件 | 对应头文件 | 缺失函数数 | 说明 | 工作量 |
|---|---------|-----------|-----------|------|--------|
| I1 | `recursion.c` | `recursion.h`（680行声明） | **17+个函数** | `recursion.h`已声明完整API但**无任何.c实现**。包括measure_system_create/destroy、recursion_context_create/destroy/enter/exit等 | 8人天 |

**关键缺失函数清单**：
- `measure_system_create()` / `measure_system_destroy()`
- `measure_create_symbolic()` / `measure_destroy()`
- `measure_system_add()` / `measure_system_set_default()`
- `measure_compute_value()` / `measure_compute_value_symbolic()`
- `measure_compare()`
- `recursion_context_create()` / `recursion_context_destroy()`
- `recursion_context_enter()` / `recursion_context_exit()`
- `recursion_context_get_depth()` / `recursion_context_reset()`
- `recursion_check_mutual()` / `recursion_run_builtin_tests()`

#### 9.3.2 P1 — 高优先级（API命名不匹配）

| # | 问题 | 说明 | 工作量 |
|---|------|------|--------|
| I2 | `selector_create` vs `selector_block_create` | ctypes注册了`selector_create`但C侧实际名为`selector_block_create`；`selector_apply` vs `selector_block_evaluate`语义也不一致 | 0.5人天 |
| I3 | Python绑定中标记"已移除"的函数 | `unify_detailed`、`rewrite_create_rule`等函数在Python绑定中被注释为已移除，但C层可能仍需要 | 1人天 |

### 9.4 示例代码引用的不存在的API

| # | 引用位置 | 引用的API | 当前状态 | 需要补全 | 工作量 |
|---|---------|----------|----------|----------|--------|
| E1 | `custom_syntax_extension.c` | `func_block_register_custom()` | 不存在 | 见4.1.2节 | 3人天 |
| E2 | `custom_syntax_extension.c` | `FuncBlockTemplate`全套API | 不存在 | 见4.1.3节 | 5人天 |
| E3 | `custom_syntax_extension.c` | `DslVersion`/`dsl_version_extract()`/`dsl_syntax_transform()` | 不存在 | 见4.1.5节 | 3人天 |
| E4 | `custom_syntax_extension.c` | `Lv00ErrorMessage`/`lv00_get_error_message()` | 不存在 | 见4.1.6节 | 2人天 |
| E5 | `custom_syntax_extension.c` | `#include "error_messages_cn.h"` | 不存在 | 合并到`error_messages.h` | 0.5人天 |
| E6 | `sample_plugin.c` | `lv00/plugin_system.h` | **已存在**（426行），但需验证实现完整性 | 验证 | 1人天 |

### 9.5 C核心架构发现

#### 9.5.1 实际目录结构 vs 文档描述

| 文档描述 | 实际目录 | 状态 |
|----------|----------|------|
| `layer2_axiom/` | `layer2_resource/` | **命名不一致** |
| `layer3_constraint/` | `layer3_geometry/` | **命名不一致** |
| `shared/`（独立目录） | 功能散布在`layer2_resource/`中 | **未独立** |
| 5层架构 | 实际已有10层（layer6_visual ~ layer10_interop） | **文档严重滞后** |
| `layer_validation.h`定义6层验证 | 实际已有10层 | **验证规则不完整** |

#### 9.5.2 各层文件完整性

| 目录 | 文件数 | 完整性评估 |
|------|--------|-----------|
| `layer1_parser/` | 4个.c | **基本完整** |
| `layer2_resource/` | 11个.c | **较完整**（含shared功能） |
| `layer3_geometry/` | 8个.c/.h | **较完整** |
| `layer4_reasoning/` | 25个.c | **实现丰富但头文件缺失严重** |
| `layer5_output/` | 5个.c+子目录 | **部分完整** |
| `layer6_visual/` | 17个.c | **较完整** |
| `layer7_orchestration/` | 1个.c | **骨架** |
| `layer8_meta_verify/` | 1个.c | **骨架** |
| `layer9_application/` | 1个.c | **骨架** |
| `layer10_interop/` | 4个.c | **部分完整** |

### 9.6 C重写总工作量汇总

| 优先级 | 类别 | 工作量 |
|--------|------|--------|
| **P0** | Python→C重写（R1-R7） | 12人天 |
| **P0** | 缺失头文件创建（H1-H6） | 8.5人天 |
| **P0** | 缺失实现文件（I1） | 8人天 |
| **P0小计** | | **28.5人天** |
| **P1** | Python→C重写（R8-R15） | 19人天 |
| **P1** | 缺失头文件创建（H7-H13） | 5人天 |
| **P1** | API对齐（I2-I3） | 1.5人天 |
| **P1小计** | | **25.5人天** |
| **P2** | Python→C重写（R16-R18） | 4人天 |
| **P2** | 示例API实现（E1-E5） | 13.5人天 |
| **P2小计** | | **17.5人天** |
| **P3** | 示例验证（E6）+ 架构文档更新 | 4人天 |
| **总计** | | **约75.5人天** |

---

## 10. 更新后的完整路线图

### 阶段0：C核心编译修复（Week 1-3）— 【新增】

> **前置条件**：必须先修复C核心的编译问题，后续所有工作都依赖于此。

| 周次 | 任务 | 产出 | 优先级 |
|------|------|------|--------|
| Week 1 | 创建`lv00_internal.h` | 从40+源文件逆向工程出内部结构体、常量、宏 | P0 |
| Week 1 | 创建6个缺失的内部头文件 | `stream_context_util.h`, `solver_core.h`, `constraint_graph_safe.h`, `prop_verifier.h`, `parser_safety.h` + 修复`lv00_internal.h` | P0 |
| Week 2 | 创建`recursion.c`实现 | 17+个函数的完整实现 | P0 |
| Week 2 | 创建7个缺失的模块头文件 | `smt_backend.h`, `conflict_detector.h`等 | P1 |
| Week 3 | 修复selector API命名不匹配 | `selector_create`适配层 | P1 |
| Week 3 | 验证C核心可编译 | 所有平台编译通过 | P0 |

### 阶段1：C层扩展接口实现（Week 4-6）

| 周次 | 任务 | 产出 |
|------|------|------|
| Week 4 | 实现插件系统 | `plugin_system.h` + `plugin_system.c` |
| Week 4 | 实现错误消息系统 | `error_messages.h` + `error_messages.c` |
| Week 5 | 实现自定义函数注册 | `func_block_custom.h` + `func_block_custom.c` |
| Week 5 | 实现函数块模板系统 | `func_block_template.h` + `func_block_template.c` |
| Week 6 | 实现Python嵌入桥接 | `python_embed.h` + `python_embed.c` |
| Week 6 | 实现DSL扩展接口 | `dsl_extension.h` + `dsl_extension.c` |

### 阶段2：Python代码C重写（Week 7-9）— 【新增】

> 将Python中性能关键和精度丢失的代码重写为C实现。

| 周次 | 任务 | 产出 |
|------|------|------|
| Week 7 | P0重写：Graph双状态消除 + 基础几何方法C化 | R4(5d) + R1-R3(2d) |
| Week 8 | P0重写：交点/三角形符号精度恢复 | R5-R7(6d) |
| Week 9 | P1重写：LineSegment方法集 + 约束类型统一 | R8(2d) + R11(7d) |

### 阶段3：Python绑定下沉（Week 10-11）

| 周次 | 任务 | 产出 |
|------|------|------|
| Week 10 | 迁移基础绑定 | `_ctypes_binding.py`, `native_types.py`, `engine.py` |
| Week 10 | 迁移引擎绑定 | `groebner.py`, `sparse_la.py`, `type_system.py` |
| Week 11 | 拆分混合模块 | `formula_native.py`, `proof_native.py`, `stream_native.py` |

### 阶段4：后5层重组（Week 12-13）

| 周次 | 任务 | 产出 |
|------|------|------|
| Week 12 | 重组第6-7层 | `layer6_interactive/`, `layer7_binding/` |
| Week 13 | 重组第8-10层 | `layer8_ai_assistant/`, `layer9_monitoring/`, `layer10_ecosystem/` |

### 阶段5：架构补全（Week 14-15）

| 周次 | 任务 | 产出 |
|------|------|------|
| Week 14 | 层间通信契约 + 测试策略 | `LAYER_BOUNDARY_SPEC.md` |
| Week 14 | CI/CD适配 | 更新GitHub Actions、Python包配置 |
| Week 15 | 文档补全 | 更新架构手册（10层）、开发指南、API参考 |

### 阶段6：验证与发布（Week 16）

| 任务 | 验收标准 |
|------|----------|
| 编译验证 | 所有目标平台编译无错误 |
| 依赖检查 | 静态分析确认无跨层违规 |
| 测试验证 | 所有现有测试通过 + 新增测试通过 |
| 文档验证 | 架构文档反映完整十层结构 |
| CI/CD验证 | 持续集成流水线运行正常 |

---

## 11. 更新后的风险与缓解

| 风险 | 影响 | 严重程度 | 缓解措施 |
|------|------|----------|----------|
| **`lv00_internal.h`逆向工程困难** | 40+文件无法编译，项目完全阻塞 | **致命** | 从引用文件中逐步提取所需定义；先创建最小可用版本 |
| **`recursion.c`实现复杂** | Python绑定断裂，测度系统不可用 | **高** | 参考`recursion.h`的声明和Python绑定的使用模式 |
| **Graph双状态重写引入回归** | 现有用户代码依赖Python侧状态追踪 | **高** | 提供兼容性API；分阶段迁移 |
| **精度重写改变计算结果** | 浮点→符号精度可能改变部分边界情况的行为 | **中** | 增加回归测试；提供精度配置选项 |
| C层扩展接口设计不当 | 后期大规模重构 | 中 | 先设计评审，再实现；提供兼容性层 |
| Python绑定下沉导致兼容性问题 | 现有用户代码失效 | 中 | 保留旧导入路径的兼容性shim |
| 文档与实际10层结构不一致 | 开发者困惑 | 中 | 优先更新`ARCHITECTURE_MANUAL.md` |

---

## 12. C重写详细实现方案

> 本章为第9章标记的每个C重写项提供详细的接口设计、数据结构和算法思路。

### 12.1 H1 — lv00_internal.h 设计

**设计目标**：为40+源文件提供统一的内部数据结构、常量和工具宏。

**接口设计**：
```c
// core/include/lv00/lv00_internal.h
#ifndef LV00_INTERNAL_H
#define LV00_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ===== 内部常量 ===== */
#define LV00_MAX_NODES_PER_GRAPH 65536
#define LV00_MAX_CONSTRAINTS_PER_GRAPH 16384
#define LV00_MAX_SYMBOLIC_DEPTH 256
#define LV00_DEFAULT_SOLVER_TIMEOUT 30.0
#define LV00_MAX_PLUGIN_COUNT 64
#define LV00_MAX_RECURSION_DEPTH 1024

/* ===== 内部数据结构 ===== */

/* 内存池统计 */
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t peak_usage;
    size_t block_count;
    size_t large_block_count;
} Lv00MemPoolStats;

/* 图节点内部表示 */
typedef struct Lv00GeomNodeInternal {
    int id;
    int type;  /* Lv00GeomType */
    int status; /* Lv00NodeStatus */
    void* symbolic_data;  /* SymbolicCoord* 或复合结构 */
    void* concrete_data;  /* 浮点近似值缓存 */
    struct Lv00GeomNodeInternal** dependencies;
    int dependency_count;
    int ref_count;
} Lv00GeomNodeInternal;

/* 约束图内部表示 */
typedef struct {
    Lv00GeomNodeInternal* nodes;
    int node_count;
    int node_capacity;
    void* constraints;  /* Lv00ConstraintInternal* */
    int constraint_count;
    void* solver_state;
    void* measure_system;
    void* recursion_context;
    Lv00MemPoolStats* mem_stats;
} Lv00ConstraintGraphInternal;

/* 引擎内部状态 */
typedef struct {
    Lv00ConstraintGraphInternal* active_graph;
    void* proof_context;
    void* strategy_context;
    void* plugin_system;
    void* error_context;
    double solver_timeout;
    int max_recursion_depth;
    bool auto_normalize;
} Lv00EngineInternal;

/* ===== 内部工具宏 ===== */
#define LV00_INTERNAL(graph) ((Lv00ConstraintGraphInternal*)(graph)->_internal)
#define LV00_ENGINE_INTERNAL(engine) ((Lv00EngineInternal*)(engine)->_internal)
#define LV00_RETURN_IF_NULL(ptr) do { if (!(ptr)) return LV00_ERROR_NULL_POINTER; } while(0)
#define LV00_RETURN_IF_OOM(ptr) do { if (!(ptr)) return LV00_ERROR_OUT_OF_MEMORY; } while(0)

/* ===== 内部API（仅core内部使用） ===== */
Lv00GeomNodeInternal* lv00_internal_get_node(Lv00ConstraintGraphInternal* graph, int node_id);
int lv00_internal_add_node(Lv00ConstraintGraphInternal* graph, int type, void* data);
bool lv00_internal_remove_node(Lv00ConstraintGraphInternal* graph, int node_id);
bool lv00_internal_update_dependency(Lv00GeomNodeInternal* node, int dep_id, bool add);

#ifdef __cplusplus
}
#endif

#endif /* LV00_INTERNAL_H */
```

**实现要点**：
- 所有现有源文件中的内部结构体定义迁移到此头文件
- 使用`_internal`指针实现不透明指针模式，保持公共API不变
- 提供内部节点操作API，供`recursion.c`等实现使用

**工作量**：5人天（含40+文件适配）

---

### 12.2 I1 — recursion.c 实现

**设计目标**：实现`recursion.h`中声明的17+个函数，提供测度系统和递归上下文管理。

**数据结构**：
```c
/* 测度值 */
typedef struct {
    enum { MEASURE_SYMBOLIC, MEASURE_NUMERIC, MEASURE_UNDEFINED } type;
    union {
        SymbolicCoord* symbolic;
        double numeric;
    } value;
} Lv00MeasureValue;

/* 测度定义 */
typedef struct {
    char name[64];
    int id;
    Lv00MeasureValue default_value;
    Lv00MeasureValue current_value;
    bool is_computed;
} Lv00Measure;

/* 测度系统 */
typedef struct {
    Lv00Measure* measures;
    int measure_count;
    int measure_capacity;
    Lv00Measure* default_measure;
    void* computation_cache;
} Lv00MeasureSystem;

/* 递归上下文 */
typedef struct {
    int* call_stack;      /* 节点ID栈 */
    int stack_depth;
    int stack_capacity;
    int max_depth;
    void* depth_counters; /* 每节点深度计数 */
    bool in_recursion;
} Lv00RecursionContext;
```

**核心算法**：

```c
/* 递归检测算法 */
bool recursion_check_mutual(Lv00RecursionContext* ctx, int node_id) {
    /* 检查当前调用栈中是否已存在node_id */
    for (int i = 0; i < ctx->stack_depth; i++) {
        if (ctx->call_stack[i] == node_id) {
            return true;  /* 发现循环依赖 */
        }
    }
    return false;
}

/* 测度值符号计算 */
bool measure_compute_value_symbolic(
    Lv00MeasureSystem* sys,
    int measure_id,
    SymbolicCoord** out_value
) {
    Lv00Measure* m = find_measure(sys, measure_id);
    if (!m) return false;
    
    if (m->is_computed) {
        *out_value = symbolic_coord_clone(m->current_value.value.symbolic);
        return true;
    }
    
    /* 根据测度类型执行符号计算 */
    switch (measure_type(m)) {
        case MEASURE_LENGTH:
            return compute_length_symbolic(m, out_value);
        case MEASURE_ANGLE:
            return compute_angle_symbolic(m, out_value);
        case MEASURE_AREA:
            return compute_area_symbolic(m, out_value);
        /* ... */
    }
    return false;
}
```

**实现文件**：`core/src/layer4_reasoning/recursion.c`（约800-1000行）

**工作量**：8人天

---

### 12.3 R1-R3 — Point基础几何方法

**接口设计**：
```c
/* core/include/lv00/geometry_ops.h（新增） */

/* 两点距离平方（符号精度） */
bool lv00_point_distance_sq(
    const SymbolicCoord* a,
    const SymbolicCoord* b,
    SymbolicCoord** out_result
);

/* 中点计算（符号精度） */
bool lv00_point_midpoint(
    const SymbolicCoord* a,
    const SymbolicCoord* b,
    SymbolicCoord** out_result
);

/* 共线性检测 */
bool lv00_point_is_collinear(
    const SymbolicCoord* a,
    const SymbolicCoord* b,
    const SymbolicCoord* c,
    bool* out_result,
    double tolerance
);
```

**算法思路**：
- `distance_sq`: 使用SymbolicCoord的减法+点积运算，避免开方保持精度
- `midpoint`: `(a + b) / 2`，利用SymbolicCoord的加法和标量除法
- `is_collinear`: 计算叉积 `(b-a) × (c-a)`，检查是否为零（符号精度）

**Python绑定更新**：
```python
# core/src/python_binding/native_types.py
class Point:
    def distance_to(self, other):
        result = SymbolicCoord()
        if not lib.lv00_point_distance_sq(self._coord, other._coord, byref(result._ptr)):
            raise GeometryError("Failed to compute distance")
        return result
    
    def mid_point(self, other):
        result = SymbolicCoord()
        lib.lv00_point_midpoint(self._coord, other._coord, byref(result._ptr))
        return Point(result)
    
    def is_collinear_with(self, b, c):
        result = c_bool()
        lib.lv00_point_is_collinear(self._coord, b._coord, c._coord, byref(result), 1e-10)
        return result.value
```

**工作量**：R1(1d) + R2(0.5d) + R3(0.5d) = 2人天

---

### 12.4 R4 — Graph双状态消除

**问题分析**：
- Python侧`Graph._points`列表与C层节点管理完全独立
- Engine直接操作C层节点时，Python侧状态不会同步
- 导致内存泄漏、悬空指针、状态不一致

**解决方案**：

```c
/* C层新增：节点变更通知机制 */
typedef void (*Lv00NodeChangeCallback)(
    int graph_id,
    int node_id,
    int change_type,  /* ADD/REMOVE/UPDATE */
    void* user_data
);

bool lv00_graph_register_change_callback(
    Lv00ConstraintGraph* graph,
    Lv00NodeChangeCallback callback,
    void* user_data
);
```

```python
# Python层：惰性查询替代状态缓存
class Graph:
    def __init__(self, c_graph_ptr):
        self._c_graph = c_graph_ptr
        self._change_callback = None
        # 移除 self._points = []
    
    @property
    def points(self):
        """惰性从C层查询所有点节点"""
        count = c_int()
        nodes_ptr = POINTER(c_int)()
        lib.lv00_graph_get_nodes_by_type(
            self._c_graph, NODE_TYPE_POINT, 
            byref(nodes_ptr), byref(count)
        )
        return [Point._from_id(nodes_ptr[i]) for i in range(count.value)]
    
    def add_point(self, x, y):
        """直接调用C层，不维护Python侧缓存"""
        node_id = lib.lv00_graph_add_point(self._c_graph, x, y)
        return Point._from_id(node_id)
```

**兼容性处理**：
- 保留`Graph._points`属性但改为惰性查询
- 提供`Graph._legacy_mode`标志用于旧代码过渡

**工作量**：5人天

---

### 12.5 R5-R6 — 直线/圆交点计算（符号精度）

**接口设计**：
```c
/* 直线-直线交点（符号精度） */
bool lv00_intersect_lines(
    const SymbolicCoord* p1, const SymbolicCoord* d1,  /* 点+方向 */
    const SymbolicCoord* p2, const SymbolicCoord* d2,
    SymbolicCoord** out_intersection,
    bool* out_parallel
);

/* 圆-圆交点（符号精度） */
bool lv00_intersect_circles(
    const SymbolicCoord* c1, const SymbolicCoord* r1,  /* 圆心+半径平方 */
    const SymbolicCoord* c2, const SymbolicCoord* r2,
    SymbolicCoord** out_p1,
    SymbolicCoord** out_p2,
    int* out_count  /* 0/1/2 */
);

/* 直线-圆交点（符号精度） */
bool lv00_intersect_line_circle(
    const SymbolicCoord* line_p, const SymbolicCoord* line_d,
    const SymbolicCoord* circle_c, const SymbolicCoord* circle_r2,
    SymbolicCoord** out_p1,
    SymbolicCoord** out_p2,
    int* out_count
);
```

**算法思路**：
- 直线交点：解线性方程组，使用SymbolicCoord的加减乘除
- 圆-圆交点：利用根式表达，保持符号精度（不转为float）
- 所有中间计算使用SymbolicCoord，仅在最终渲染时转为浮点

**工作量**：R5(1.5d) + R6(1.5d) = 3人天

---

### 12.6 R7 — TriangleWrapper几何方法集

**接口设计**：
```c
/* 三角形面积（符号精度 - 海伦公式或叉积） */
bool lv00_triangle_area(
    const SymbolicCoord* a,
    const SymbolicCoord* b,
    const SymbolicCoord* c,
    SymbolicCoord** out_area
);

/* 外心（符号精度） */
bool lv00_triangle_circumcenter(
    const SymbolicCoord* a, const SymbolicCoord* b, const SymbolicCoord* c,
    SymbolicCoord** out_center
);

/* 垂心 */
bool lv00_triangle_orthocenter(...);

/* 内心 */
bool lv00_triangle_incenter(...);

/* 重心 */
bool lv00_triangle_centroid(...);

/* 九点圆心 */
bool lv00_triangle_nine_point_center(...);

/* 旁心 */
bool lv00_triangle_excenter(...);

/* 内切圆半径 */
bool lv00_triangle_inradius(...);

/* 外接圆半径 */
bool lv00_triangle_circumradius(...);
```

**算法要点**：
- 所有方法使用符号精度计算
- 与`preset_analysis.py`中的实现统一，消除重复
- 利用重心坐标系简化计算

**工作量**：3人天

---

### 12.7 R8 — LineSegment方法集

**接口设计**：
```c
bool lv00_segment_length_sq(const SymbolicCoord* a, const SymbolicCoord* b, SymbolicCoord** out);
bool lv00_segment_midpoint(const SymbolicCoord* a, const SymbolicCoord* b, SymbolicCoord** out);
bool lv00_segment_direction(const SymbolicCoord* a, const SymbolicCoord* b, SymbolicCoord** out);
bool lv00_segment_is_parallel(const SymbolicCoord* a1, const SymbolicCoord* a2,
                               const SymbolicCoord* b1, const SymbolicCoord* b2,
                               bool* out, double tolerance);
bool lv00_segment_is_perpendicular(...);
bool lv00_segment_intersection(...);
bool lv00_segment_contains_point(...);
bool lv00_segment_distance_to_point(...);
```

**工作量**：2人天

---

### 12.8 R11 — Constraint类型系统统一

**问题分析**：
- Python侧`Constraint`子类层次（67-309行）与C层约束枚举独立
- 映射关系隐式，无编译时保证
- 序列化/反序列化容易出错

**解决方案**：

```c
/* C层：统一的约束类型系统 */
typedef enum {
    LV00_CONSTRAINT_EQUALITY = 0,
    LV00_CONSTRAINT_INEQUALITY,
    LV00_CONSTRAINT_PARALLEL,
    LV00_CONSTRAINT_PERPENDICULAR,
    LV00_CONSTRAINT_COLLINEAR,
    LV00_CONSTRAINT_CONCENTRIC,
    LV00_CONSTRAINT_TANGENT,
    LV00_CONSTRAINT_ANGLE,
    LV00_CONSTRAINT_DISTANCE,
    LV00_CONSTRAINT_RATIO,
    /* ... */
    LV00_CONSTRAINT_COUNT
} Lv00ConstraintType;

/* 约束元数据 */
typedef struct {
    Lv00ConstraintType type;
    const char* name;
    const char* python_class_name;
    int min_nodes;
    int max_nodes;
    bool requires_parameters;
    const char* parameter_schema;  /* JSON schema */
} Lv00ConstraintMeta;

/* 约束注册表 */
const Lv00ConstraintMeta* lv00_constraint_get_meta(Lv00ConstraintType type);
Lv00ConstraintType lv00_constraint_type_from_name(const char* name);
Lv00ConstraintType lv00_constraint_type_from_python_class(const char* class_name);

/* 约束序列化 */
bool lv00_constraint_to_json(const Lv00Constraint* constraint, char** out_json);
bool lv00_constraint_from_json(const char* json, Lv00Constraint** out_constraint);
```

```python
# Python层：自动从C元数据生成类
class ConstraintMeta(type):
    """自动从C层约束元数据创建Python类"""
    def __new__(mcs, name, bases, namespace):
        # 查询C层元数据
        c_meta = lib.lv00_constraint_type_from_python_class(name.encode())
        namespace['_c_type'] = c_meta
        return super().__new__(mcs, name, bases, namespace)

class Constraint(metaclass=ConstraintMeta):
    @classmethod
    def from_dict(cls, data):
        """使用C层JSON解析"""
        c_constraint = c_void_p()
        json_str = json.dumps(data).encode()
        if not lib.lv00_constraint_from_json(json_str, byref(c_constraint)):
            raise ConstraintError(f"Invalid constraint: {data}")
        return cls._from_c(c_constraint)
    
    def to_dict(self):
        """使用C层JSON序列化"""
        json_ptr = c_char_p()
        lib.lv00_constraint_to_json(self._c_constraint, byref(json_ptr))
        return json.loads(json_ptr.value.decode())
```

**工作量**：7人天

---

### 12.9 R14 — 预设函数块下沉

**接口设计**：
```c
/* core/include/lv00/preset_blocks.h（新增） */

/* 预设函数块注册 */
typedef struct {
    const char* name;
    const char* category;
    const char* description;
    int (*min_inputs)();
    int (*max_inputs)();
    bool (*execute)(Lv00ConstraintGraph* graph, const int* inputs, int input_count, int** outputs, int* output_count);
} Lv00PresetBlockDef;

bool lv00_preset_register(const Lv00PresetBlockDef* def);
bool lv00_preset_unregister(const char* name);
const Lv00PresetBlockDef* lv00_preset_get(const char* name);

/* 常用预设 */
bool lv00_preset_create_midpoint(Lv00ConstraintGraph* graph, int p1, int p2, int* out_midpoint);
bool lv00_preset_create_circumcenter(Lv00ConstraintGraph* graph, int a, int b, int c, int* out_center);
bool lv00_preset_create_centroid(Lv00ConstraintGraph* graph, int a, int b, int c, int* out_centroid);
bool lv00_preset_create_orthocenter(...);
bool lv00_preset_create_incenter(...);
bool lv00_preset_create_reflection(Lv00ConstraintGraph* graph, int point, int mirror, int* out_reflection);
bool lv00_preset_create_translation(Lv00ConstraintGraph* graph, int point, int vector, int* out_translated);
/* ... */
```

**统一策略**：
- 将Python侧`preset_basic.py`、`preset_analysis.py`、`preset_algebra.py`、`preset_topology.py`中的实现迁移到C
- Python侧保留薄包装，直接调用C预设函数
- 消除`FuncBlockSpec`注册表和C侧预设注册表的双轨制

**工作量**：5人天

---

### 12.10 R15 — SymbolicCoord常量池

**问题**：每次调用`from_rational(2)`、`from_rational(3)`等都会触发C内存分配。

**解决方案**：
```c
/* core/include/lv00/symbolic_coord.h 扩展 */

/* 常用常量预分配 */
extern SymbolicCoord* LV00_SYM_ZERO;
extern SymbolicCoord* LV00_SYM_ONE;
extern SymbolicCoord* LV00_SYM_TWO;
extern SymbolicCoord* LV00_SYM_THREE;
extern SymbolicCoord* LV00_SYM_HALF;
extern SymbolicCoord* LV00_SYM_NEG_ONE;
extern SymbolicCoord* LV00_SYM_SQRT2;  /* √2 */
extern SymbolicCoord* LV00_SYM_SQRT3;  /* √3 */
extern SymbolicCoord* LV00_SYM_PI;     /* π（符号表示）*/

/* 初始化常量池 */
void lv00_symbolic_coord_init_constants(void);
void lv00_symbolic_coord_free_constants(void);

/* 快速创建（使用常量池） */
SymbolicCoord* lv00_symbolic_coord_from_int_fast(int value);  /* 小整数直接用池 */
```

```python
# Python绑定更新
class SymbolicCoord:
    # 类级别常量，避免重复创建
    ZERO = None
    ONE = None
    TWO = None
    HALF = None
    
    @classmethod
    def _init_constants(cls):
        cls.ZERO = cls._from_ptr(lib.LV00_SYM_ZERO)
        cls.ONE = cls._from_ptr(lib.LV00_SYM_ONE)
        cls.TWO = cls._from_ptr(lib.LV00_SYM_TWO)
        cls.HALF = cls._from_ptr(lib.LV00_SYM_HALF)
    
    @classmethod
    def from_rational(cls, num, den=1):
        # 小整数优化
        if den == 1:
            if num == 0: return cls.ZERO
            if num == 1: return cls.ONE
            if num == 2: return cls.TWO
            if num == -1: return cls._from_ptr(lib.LV00_SYM_NEG_ONE)
        return cls._from_ptr(lib.symbolic_coord_from_rational(num, den))
```

**工作量**：1人天

---

### 12.11 其他重写项简要方案

| # | 重写项 | 简要方案 | 工作量 |
|---|--------|---------|--------|
| R9 | Point.translate/reflect | 复用R14预设函数 | 1人天 |
| R10 | Engine.unify_detailed | 在C层实现带诊断的统一化 | 2人天 |
| R12 | G.angle() | 符号精度反余弦（利用SymbolicCoord的acos） | 0.5人天 |
| R13 | EngineBridge._handle_event | 直接使用C层`stream_event_to_json()` | 0.5人天 |
| R16 | Transform._transform_point | 矩阵-向量乘法C化 | 1人天 |
| R17 | Engine.solve_with_retry | C层指数退避重试 | 1人天 |
| R18 | ProofMultiStrategy | C层策略管道 | 2人天 |

---

## 13. 架构层面详细设计

> 本章补充层间通信契约、测试策略、CI/CD配置的详细设计。

### 13.1 层间通信契约

#### 13.1.1 契约设计原则

1. **单向依赖**：信息只能从低层向高层流动，控制只能从高层向低层流动
2. **版本兼容**：契约版本与C核心版本锁定
3. **序列化中立**：契约定义不依赖特定序列化格式
4. **错误传播**：所有契约都定义错误码和回退行为

#### 13.1.2 第5层→第6层数据契约（Proof Object → 可视化模型）

**数据流**：`layer5_output` 生成的证明对象 → `layer6_interactive` 的可视化组件

**契约定义**：
```protobuf
// layer6_interactive/proto/proof_visual.proto
syntax = "proto3";

package lv00.layer6;

message ProofVisualModel {
  string proof_id = 1;
  int32 version = 2;

  message Step {
    int32 step_number = 1;
    string step_type = 2;  // "axiom", "inference", "substitution", "contradiction"
    string description = 3;
    repeated string premises = 4;
    string conclusion = 5;

    message VisualElement {
      string element_id = 1;
      string element_type = 2;  // "point", "line", "circle", "angle", "triangle"
      map<string, string> properties = 3;
      bool is_highlighted = 4;
      string highlight_reason = 5;
    }
    repeated VisualElement visual_elements = 6;

    message Animation {
      string animation_type = 1;  // "appear", "transform", "fade", "connect"
      int32 duration_ms = 2;
      map<string, string> params = 3;
    }
    repeated Animation animations = 7;
  }
  repeated Step steps = 3;

  message GeometryState {
    repeated Point points = 1;
    repeated Line lines = 2;
    repeated Circle circles = 3;
  }
  GeometryState initial_state = 4;
  GeometryState final_state = 5;
}

message Point {
  string id = 1;
  string label = 2;
  double x = 3;
  double y = 4;
  string symbolic_x = 5;
  string symbolic_y = 6;
}

message Line {
  string id = 1;
  string label = 2;
  string point1_id = 3;
  string point2_id = 4;
  string equation = 5;
}

message Circle {
  string id = 1;
  string label = 2;
  string center_id = 3;
  double radius = 4;
  string symbolic_radius = 5;
}
```

**Python绑定**：
```python
# core/src/python_binding/proof_visual_model.py
class ProofVisualModel:
    """Pythonic包装器，将C层Proof对象转换为可视化模型"""

    @classmethod
    def from_c_proof(cls, c_proof_ptr):
        """从C层Proof指针构建可视化模型"""
        model = cls()
        # 遍历C层证明步骤，转换为VisualModel
        step_count = lib.lv00_proof_get_step_count(c_proof_ptr)
        for i in range(step_count):
            c_step = lib.lv00_proof_get_step(c_proof_ptr, i)
            model.steps.append(VisualStep.from_c(c_step))
        return model

    def to_json(self) -> str:
        """序列化为JSON供前端使用"""
        return json.dumps(self._to_dict())

    def to_proto(self) -> bytes:
        """序列化为Protobuf（高性能场景）"""
        # 使用生成的protobuf类
        pass
```

#### 13.1.3 第6层→第4层控制契约（UI操作 → 推理请求）

**数据流**：用户交互事件 → 推理引擎请求

**契约定义**：
```python
# layer6_interactive/proto/interaction_reasoning.py

from dataclasses import dataclass
from enum import Enum, auto
from typing import Optional, List, Dict, Any

class InteractionType(Enum):
    ADD_POINT = auto()           # 添加点
    ADD_LINE = auto()            # 添加线
    ADD_CIRCLE = auto()          # 添加圆
    ADD_CONSTRAINT = auto()      # 添加约束
    DELETE_ELEMENT = auto()      # 删除元素
    MOVE_POINT = auto()          # 拖动点
    APPLY_AXIOM = auto()         # 应用公理
    REQUEST_PROOF = auto()       # 请求证明
    REQUEST_VERIFY = auto()      # 请求验证
    UNDO = auto()                # 撤销
    REDO = auto()                # 重做

@dataclass
class InteractionEvent:
    event_id: str                # UUID
    timestamp: float             # Unix timestamp
    interaction_type: InteractionType
    target_element_id: Optional[str]
    parameters: Dict[str, Any]   # 类型特定的参数

    # 坐标参数（用于ADD_POINT, MOVE_POINT）
    x: Optional[float] = None
    y: Optional[float] = None

    # 约束参数（用于ADD_CONSTRAINT）
    constraint_type: Optional[str] = None
    source_elements: Optional[List[str]] = None

    # 公理参数（用于APPLY_AXIOM）
    axiom_name: Optional[str] = None
    axiom_parameters: Optional[Dict[str, Any]] = None

@dataclass
class ReasoningRequest:
    request_id: str
    event: InteractionEvent
    context: Dict[str, Any]      # 当前图状态快照
    priority: int = 0            # 优先级（用于异步处理）
    timeout_ms: int = 5000       # 超时

@dataclass
class ReasoningResponse:
    request_id: str
    success: bool
    result_type: str             # "proof", "verification", "state_update", "error"
    result_data: Dict[str, Any]
    affected_elements: List[str] # 受影响的元素ID
    error_code: Optional[int] = None
    error_message: Optional[str] = None
```

**异步处理流程**：
```
UI事件 → InteractionEvent → 消息队列 → 推理工作线程 → C层引擎 → ReasoningResponse → UI更新
```

#### 13.1.4 第7层→第1-5层绑定契约

**契约定义**：Python调用C的参数/返回值规范

```c
// core/include/lv00/python_binding/lv00_python.h

/* Python绑定契约版本 */
#define LV00_PYTHON_BINDING_VERSION_MAJOR 1
#define LV00_PYTHON_BINDING_VERSION_MINOR 0
#define LV00_PYTHON_BINDING_VERSION_PATCH 0

/* 参数传递规范 */
typedef struct {
    int type;           /* LV00_PY_ARG_INT, LV00_PY_ARG_FLOAT, LV00_PY_ARG_STRING, etc. */
    union {
        int64_t i;
        double f;
        const char* s;
        void* p;
    } value;
} Lv00PythonArg;

/* 返回值规范 */
typedef struct {
    int type;
    int error_code;
    union {
        int64_t i;
        double f;
        char* s;        /* 调用方负责释放 */
        void* p;
    } value;
} Lv00PythonResult;

/* 批量调用接口（减少FFI开销） */
typedef struct {
    const char* function_name;
    Lv00PythonArg* args;
    int arg_count;
} Lv00PythonCall;

bool lv00_python_batch_call(
    Lv00Engine* engine,
    Lv00PythonCall* calls,
    int call_count,
    Lv00PythonResult** out_results
);
```

**Python侧**：
```python
# core/src/python_binding/batch_call.py
class BatchCallOptimizer:
    """批量FFI调用优化器"""

    def __init__(self, engine):
        self.engine = engine
        self._pending_calls = []

    def call(self, func_name, *args):
        """缓存调用，不立即执行"""
        self._pending_calls.append((func_name, args))
        if len(self._pending_calls) >= 10:
            self.flush()

    def flush(self):
        """执行所有缓存的调用"""
        if not self._pending_calls:
            return
        # 一次性传递所有调用到C层
        results = lib.lv00_python_batch_call(
            self.engine._ptr,
            self._pending_calls
        )
        self._pending_calls = []
        return results
```

#### 13.1.5 第7层→第6层事件契约

**流式事件推送规范**：
```python
# layer7_binding/python/streaming/event_protocol.py

from dataclasses import dataclass
from enum import Enum
from typing import Optional, Dict, Any
import json

class StreamEventType(Enum):
    PROOF_STEP = "proof_step"           # 证明步骤完成
    CONSTRAINT_ADDED = "constraint_added" # 约束添加
    CONSTRAINT_VIOLATED = "constraint_violated" # 约束违反
    NODE_CREATED = "node_created"       # 节点创建
    NODE_UPDATED = "node_updated"       # 节点更新
    NODE_DELETED = "node_deleted"       # 节点删除
    SOLVER_PROGRESS = "solver_progress" # 求解器进度
    ERROR = "error"                     # 错误
    COMPLETE = "complete"               # 操作完成

@dataclass
class StreamEvent:
    event_type: StreamEventType
    timestamp: float
    sequence_number: int
    payload: Dict[str, Any]

    def to_json(self) -> str:
        return json.dumps({
            "type": self.event_type.value,
            "timestamp": self.timestamp,
            "seq": self.sequence_number,
            "payload": self.payload
        })

    @classmethod
    def from_json(cls, json_str: str) -> "StreamEvent":
        data = json.loads(json_str)
        return cls(
            event_type=StreamEventType(data["type"]),
            timestamp=data["timestamp"],
            sequence_number=data["seq"],
            payload=data["payload"]
        )

# WebSocket推送格式
class WebSocketEventEncoder:
    @staticmethod
    def encode(event: StreamEvent) -> str:
        return f"data: {event.to_json()}\n\n"  # SSE格式
```

#### 13.1.6 第8层→第7层AI契约

**LLM请求/响应格式**：
```python
# layer8_ai_assistant/proto/ai_protocol.py

from dataclasses import dataclass
from typing import List, Optional, Dict, Any

@dataclass
class LLMRequest:
    request_id: str
    prompt_type: str  # "code_generation", "proof_assist", "explanation", "debug"
    context: Dict[str, Any]

    # 代码生成上下文
    current_code: Optional[str] = None
    target_functionality: Optional[str] = None

    # 证明辅助上下文
    current_proof_state: Optional[Dict] = None
    target_theorem: Optional[str] = None

    # 约束
    max_tokens: int = 2048
    temperature: float = 0.7

@dataclass
class LLMResponse:
    request_id: str
    success: bool

    # 生成的代码
    generated_code: Optional[str] = None
    code_language: Optional[str] = None

    # 证明建议
    suggested_steps: Optional[List[str]] = None

    # 解释
    explanation: Optional[str] = None

    # 元数据
    model_used: str = ""
    tokens_used: int = 0
    error: Optional[str] = None
```

### 13.2 测试策略

#### 13.2.1 测试金字塔

```
                    /\
                   /  \
                  / E2E\      端到端测试（5%）
                 /------\
                /Integration\  集成测试（15%）
               /--------------\
              /   Unit Tests   \ 单元测试（80%）
             /------------------\
```

#### 13.2.2 层间依赖测试

**目标**：验证无跨层违规依赖

**测试工具**：`scripts/check_layer_deps.py`

```python
# tests/architecture/test_layer_dependencies.py

import ast
import os
from pathlib import Path
import pytest

LAYER_DIRS = {
    1: "core/src/layer1_parser",
    2: "core/src/layer2_resource",
    3: "core/src/layer3_geometry",
    4: "core/src/layer4_reasoning",
    5: "core/src/layer5_output",
    6: "layer6_interactive",
    7: "layer7_binding",
    8: "layer8_ai_assistant",
    9: "layer9_monitoring",
    10: "layer10_ecosystem",
}

class TestLayerDependencies:
    """验证层间依赖规则"""

    def test_no_backward_dependencies(self):
        """第N层不能依赖第N+1层或更高层"""
        for layer_num, layer_dir in LAYER_DIRS.items():
            for py_file in Path(layer_dir).rglob("*.py"):
                imports = self._extract_imports(py_file)
                for imp in imports:
                    target_layer = self._resolve_layer(imp)
                    if target_layer and target_layer > layer_num:
                        pytest.fail(
                            f"Layer {layer_num} file {py_file} "
                            f"imports from Layer {target_layer}: {imp}"
                        )

    def test_c_layer_isolation(self):
        """C核心层（1-5）不能导入Python模块"""
        for layer_num in range(1, 6):
            layer_dir = LAYER_DIRS[layer_num]
            for c_file in Path(layer_dir).rglob("*.c"):
                content = c_file.read_text()
                # C文件不应包含Python.h（除python_binding目录）
                if "Python.h" in content and "python_binding" not in str(c_file):
                    pytest.fail(
                        f"C file {c_file} includes Python.h outside python_binding"
                    )

    def _extract_imports(self, py_file: Path) -> List[str]:
        tree = ast.parse(py_file.read_text())
        imports = []
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                for alias in node.names:
                    imports.append(alias.name)
            elif isinstance(node, ast.ImportFrom):
                imports.append(node.module)
        return imports
```

#### 13.2.3 绑定层兼容性测试

```python
# tests/python_binding/test_compatibility.py

import pytest
import ctypes
from lv00 import __version__ as py_version
from lv00._ctypes_binding import lib, get_binding_version

class TestBindingCompatibility:
    """验证Python绑定与C核心版本兼容"""

    def test_version_match(self):
        """Python绑定版本必须与C核心版本匹配"""
        c_version = get_binding_version()
        assert c_version == py_version, (
            f"Version mismatch: Python={py_version}, C={c_version}"
        )

    def test_all_functions_exported(self):
        """C库必须导出Python绑定所需的所有函数"""
        required_functions = [
            "lv00_engine_create",
            "lv00_engine_destroy",
            "lv00_graph_create",
            "lv00_graph_add_point",
            "lv00_graph_add_line",
            "lv00_proof_create",
            "lv00_solver_solve",
            # ... 完整列表
        ]
        for func_name in required_functions:
            assert hasattr(lib, func_name), f"Missing C function: {func_name}"

    def test_struct_layout_compatibility(self):
        """C结构体布局必须与Python ctypes定义一致"""
        # 使用C测试程序输出结构体布局，与Python定义对比
        pass

    def test_error_code_consistency(self):
        """C错误码必须与Python异常映射一致"""
        from lv00.exceptions import ERROR_CODE_MAP
        c_error_count = lib.lv00_error_code_count()
        assert len(ERROR_CODE_MAP) == c_error_count
```

#### 13.2.4 插件系统测试

```python
# tests/plugin/test_plugin_lifecycle.py

import pytest
import tempfile
import os
from lv00.plugin import PluginSystem

class TestPluginLifecycle:
    """验证插件加载/激活/停用/卸载生命周期"""

    def test_load_valid_plugin(self):
        """加载有效插件"""
        system = PluginSystem()
        plugin = system.load("tests/fixtures/valid_plugin.so")
        assert plugin is not None
        assert plugin.name == "test_plugin"

    def test_activate_deactivate(self):
        """激活和停用插件"""
        system = PluginSystem()
        plugin = system.load("tests/fixtures/valid_plugin.so")
        assert system.activate(plugin)
        assert plugin.is_active
        assert system.deactivate(plugin)
        assert not plugin.is_active

    def test_unload_removes_plugin(self):
        """卸载后插件不可再用"""
        system = PluginSystem()
        plugin = system.load("tests/fixtures/valid_plugin.so")
        system.unload(plugin)
        assert plugin not in system.loaded_plugins

    def test_plugin_isolation(self):
        """插件错误不影响系统"""
        system = PluginSystem()
        bad_plugin = system.load("tests/fixtures/crashing_plugin.so")
        # 即使插件崩溃，系统仍应稳定
        assert system.is_healthy
```

### 13.3 CI/CD配置

#### 13.3.1 GitHub Actions工作流

```yaml
# .github/workflows/ci.yml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build-c:
    name: Build C Core
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        compiler: [gcc, clang]
        exclude:
          - os: windows-latest
            compiler: clang

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies (Ubuntu)
        if: matrix.os == 'ubuntu-latest'
        run: sudo apt-get update && sudo apt-get install -y cmake ninja-build

      - name: Install dependencies (macOS)
        if: matrix.os == 'macos-latest'
        run: brew install cmake ninja

      - name: Configure
        run: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

      - name: Build
        run: cmake --build build --parallel

      - name: Test C Core
        run: ctest --test-dir build --output-on-failure

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: liblv00-${{ matrix.os }}-${{ matrix.compiler }}
          path: build/liblv00.*

  build-python:
    name: Build Python Package
    runs-on: ubuntu-latest
    needs: build-c

    steps:
      - uses: actions/checkout@v4

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Download C library
        uses: actions/download-artifact@v4
        with:
          name: liblv00-ubuntu-latest-gcc
          path: lv00/lib/

      - name: Install dependencies
        run: |
          python -m pip install --upgrade pip
          pip install -e ".[dev]"

      - name: Run Python tests
        run: pytest tests/ -v --cov=lv00 --cov-report=xml

      - name: Check layer dependencies
        run: python scripts/check_layer_deps.py

      - name: Upload coverage
        uses: codecov/codecov-action@v3
        with:
          files: ./coverage.xml

  test-binding-compatibility:
    name: Binding Compatibility
    runs-on: ubuntu-latest
    needs: [build-c, build-python]

    steps:
      - uses: actions/checkout@v4

      - name: Download artifacts
        uses: actions/download-artifact@v4
        with:
          path: artifacts/

      - name: Test cross-version compatibility
        run: |
          python scripts/test_binding_compat.py \
            --c-lib artifacts/liblv00-ubuntu-latest-gcc/liblv00.so \
            --python-package .

  layer-dependency-check:
    name: Layer Dependency Check
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Check layer dependencies
        run: |
          pip install networkx
          python scripts/check_layer_deps.py --strict

      - name: Generate dependency graph
        run: python scripts/generate_dep_graph.py --output dep_graph.svg

      - name: Upload dependency graph
        uses: actions/upload-artifact@v4
        with:
          name: dependency-graph
          path: dep_graph.svg
```

#### 13.3.2 Python包配置

```toml
# pyproject.toml
[build-system]
requires = ["setuptools>=61.0", "wheel", "cmake>=3.20"]
build-backend = "setuptools.build_meta"

[project]
name = "lv00"
version = "0.5.0"
description = "Lv-00: A formalized geometric reasoning engine"
readme = "README.md"
license = {text = "MIT"}
authors = [
    {name = "Lv-00 Team", email = "team@lv00.dev"}
]
classifiers = [
    "Development Status :: 3 - Alpha",
    "Intended Audience :: Science/Research",
    "License :: OSI Approved :: MIT License",
    "Programming Language :: Python :: 3",
    "Programming Language :: Python :: 3.9",
    "Programming Language :: Python :: 3.10",
    "Programming Language :: Python :: 3.11",
    "Programming Language :: Python :: 3.12",
    "Programming Language :: C",
    "Topic :: Scientific/Engineering :: Mathematics",
]
requires-python = ">=3.9"
dependencies = [
    "numpy>=1.24.0",
    "sympy>=1.12",
    "typing-extensions>=4.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=7.0",
    "pytest-cov>=4.0",
    "pytest-asyncio>=0.21",
    "black>=23.0",
    "mypy>=1.0",
    "ruff>=0.1",
]
ui = [
    "tkinter",
    "websocket-server>=0.6",
]
ai = [
    "openai>=1.0",
    "anthropic>=0.8",
]
monitoring = [
    "prometheus-client>=0.17",
    "grafana-api>=1.0",
]
all = ["lv00[dev,ui,ai,monitoring]"]

[project.urls]
Homepage = "https://lv00.dev"
Documentation = "https://docs.lv00.dev"
Repository = "https://github.com/lv00/lv00"
Issues = "https://github.com/lv00/lv00/issues"

[tool.setuptools.packages.find]
where = ["layer7_binding/python", "core/src/python_binding"]

[tool.setuptools.package-data]
lv00 = ["lib/*.so", "lib/*.dll", "lib/*.dylib"]

# 版本锁定机制
[tool.lv00]
c-core-version = "0.5.0"
binding-version = "0.5.0"
min-c-core-version = "0.4.0"

[tool.black]
line-length = 100
target-version = ['py39']

[tool.mypy]
python_version = "3.9"
warn_return_any = true
warn_unused_configs = true
disallow_untyped_defs = true

[tool.pytest.ini_options]
testpaths = ["tests"]
python_files = ["test_*.py"]
python_classes = ["Test*"]
python_functions = ["test_*"]
addopts = "-v --tb=short"
```

#### 13.3.3 CMakeLists.txt更新

```cmake
# core/CMakeLists.txt（关键部分）

cmake_minimum_required(VERSION 3.20)
project(lv00_core VERSION 0.5.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 版本信息
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lv00/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/lv00/version.h"
)

# 源文件收集
file(GLOB_RECURSE LV00_SOURCES
    "src/shared/*.c"
    "src/layer1_parser/*.c"
    "src/layer2_resource/*.c"
    "src/layer3_geometry/*.c"
    "src/layer4_reasoning/*.c"
    "src/layer5_output/*.c"
)

# Python绑定源文件（可选）
option(LV00_BUILD_PYTHON_BINDING "Build Python binding" ON)
if(LV00_BUILD_PYTHON_BINDING)
    file(GLOB PYTHON_BINDING_SOURCES "src/python_binding/*.c")
    list(APPEND LV00_SOURCES ${PYTHON_BINDING_SOURCES})
    find_package(Python3 COMPONENTS Development REQUIRED)
endif()

# 共享库
add_library(lv00 SHARED ${LV00_SOURCES})
target_include_directories(lv00 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
)

# Python绑定包含
if(LV00_BUILD_PYTHON_BINDING)
    target_include_directories(lv00 PRIVATE ${Python3_INCLUDE_DIRS})
    target_link_libraries(lv00 PRIVATE ${Python3_LIBRARIES})
endif()

# 平台特定
if(WIN32)
    target_compile_definitions(lv00 PRIVATE LV00_BUILD_DLL)
    target_compile_options(lv00 PRIVATE /W4)
elseif(APPLE)
    target_compile_options(lv00 PRIVATE -Wall -Wextra -Wpedantic)
else()
    target_compile_options(lv00 PRIVATE -Wall -Wextra -Wpedantic -fPIC)
endif()

# 测试
enable_testing()
add_subdirectory(tests)

# 安装
install(TARGETS lv00
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/lv00 DESTINATION include)
```

#### 13.3.4 版本锁定机制

```python
# core/src/python_binding/version_check.py

import ctypes
import warnings
from packaging import version

# 绑定版本（与C核心同步）
BINDING_VERSION = "0.5.0"
MIN_C_CORE_VERSION = "0.4.0"

def check_version_compatibility(lib):
    """检查Python绑定与C核心版本兼容"""
    try:
        c_major = lib.lv00_version_major()
        c_minor = lib.lv00_version_minor()
        c_patch = lib.lv00_version_patch()
        c_version = f"{c_major}.{c_minor}.{c_patch}"
    except AttributeError:
        raise RuntimeError(
            "C library does not export version functions. "
            "Please rebuild with version support."
        )

    c_ver = version.parse(c_version)
    binding_ver = version.parse(BINDING_VERSION)
    min_ver = version.parse(MIN_C_CORE_VERSION)

    if c_ver < min_ver:
        raise RuntimeError(
            f"C core version {c_version} is too old. "
            f"Minimum required: {MIN_C_CORE_VERSION}. "
            f"Please upgrade the C library."
        )

    if c_ver.major != binding_ver.major:
        raise RuntimeError(
            f"Major version mismatch: C={c_version}, Python={BINDING_VERSION}. "
            f"Major versions must match."
        )

    if c_ver < binding_ver:
        warnings.warn(
            f"C core version {c_version} is older than Python binding {BINDING_VERSION}. "
            f"Some features may be unavailable."
        )

    return True

# 在库加载时自动检查
lib = load_c_library()
check_version_compatibility(lib)
```

---

## 14. 文档与工具链设计

> 本章定义开发指南、迁移指南、自动化工具等文档和工具链的详细设计。

### 14.1 开发指南

#### 14.1.1 Python绑定开发指南

**目标读者**：为Lv-00贡献Python绑定代码的开发者

**核心目录结构**：
```
core/src/python_binding/
├── __init__.py              # 包入口，导出公共API
├── _ctypes_binding.py       # C库ctypes绑定（底层）
├── native_types.py          # C类型Pythonic封装
├── engine.py                # Engine类绑定
├── batch_call.py            # 批量FFI调用优化
├── version_check.py         # 版本兼容性检查
└── exceptions.py            # 异常定义和C错误码映射
```

**开发规范**：

1. **C函数包装模式**：
```python
# 标准包装模板
from ctypes import c_int, c_double, c_void_p, byref, POINTER
from .exceptions import check_error, GeometryError

def _wrap_c_function(func_name, arg_types, restype):
    """通用C函数包装器"""
    func = getattr(lib, func_name)
    func.argtypes = arg_types
    func.restype = restype
    return func

# 示例：包装 lv00_graph_add_point
lv00_graph_add_point = _wrap_c_function(
    "lv00_graph_add_point",
    [c_void_p, c_double, c_double],  # graph, x, y
    c_int  # 返回node_id
)

class Graph:
    def add_point(self, x: float, y: float) -> "Point":
        """添加点到图中"""
        node_id = lv00_graph_add_point(self._ptr, x, y)
        if node_id < 0:
            raise check_error(node_id)  # 将负值错误码转为异常
        return Point._from_id(self, node_id)
```

2. **内存管理规则**：
   - C分配的内存必须由C释放（`lv00_free()`）
   - Python对象持有C指针时，在`__del__`中释放
   - 使用`weakref`避免循环引用导致的内存泄漏
   - 所有C指针包装类必须继承`CObjectBase`

```python
class CObjectBase:
    """C指针包装基类"""
    _lib = lib  # ctypes库引用
    
    def __init__(self, ptr: c_void_p):
        self._ptr = ptr
        self._valid = True
    
    def __del__(self):
        if self._valid and self._ptr:
            self._free()
    
    def _free(self):
        """子类重写此方法释放C资源"""
        raise NotImplementedError
    
    def _check_valid(self):
        if not self._valid:
            raise RuntimeError("Object has been freed")
```

3. **错误处理模式**：
```python
# exceptions.py
class LV00Error(Exception):
    """基础异常"""
    def __init__(self, code: int, message: str):
        self.code = code
        self.message = message
        super().__init__(f"[{code}] {message}")

class GeometryError(LV00Error):
    """几何操作错误"""
    pass

class SolverError(LV00Error):
    """求解器错误"""
    pass

# C错误码 → Python异常映射
ERROR_CODE_MAP = {
    -1: (GeometryError, "Invalid geometry operation"),
    -2: (SolverError, "Solver failed to converge"),
    -3: (LV00Error, "Out of memory"),
    # ...
}

def check_error(code: int):
    """将C错误码转为Python异常"""
    if code >= 0:
        return code  # 成功
    exc_class, default_msg = ERROR_CODE_MAP.get(code, (LV00Error, f"Unknown error: {code}"))
    # 尝试从C层获取详细错误信息
    error_ptr = c_char_p()
    if hasattr(lib, 'lv00_get_last_error'):
        lib.lv00_get_last_error(byref(error_ptr))
        msg = error_ptr.value.decode() if error_ptr.value else default_msg
    else:
        msg = default_msg
    raise exc_class(code, msg)
```

#### 14.1.2 插件开发指南

**目标读者**：开发Lv-00插件的第三方开发者

**插件结构**：
```c
// my_plugin.c
#include <lv00/plugin_system.h>
#include <lv00/func_block_custom.h>

static int my_plugin_on_load(Lv00PluginContext* ctx) {
    // 注册自定义函数
    CustomFunctionRegistration reg = {
        .meta = {
            .name = "my_custom_op",
            .description = "My custom geometric operation",
            .min_inputs = 2,
            .max_inputs = 2,
            .output_count = 1,
        },
        .callback = my_custom_op_callback,
    };
    lv00_func_block_register_custom(&reg);
    return 0;
}

static int my_plugin_on_unload(Lv00PluginContext* ctx) {
    lv00_func_block_unregister_custom("my_custom_op");
    return 0;
}

// 插件入口
LV00_PLUGIN_ENTRY() {
    static Lv00PluginDescriptor desc = {
        .name = "my_plugin",
        .version = 1,
        .on_load = my_plugin_on_load,
        .on_unload = my_plugin_on_unload,
    };
    return &desc;
}
```

**编译插件**：
```bash
# Linux/macOS
gcc -shared -fPIC -o my_plugin.so my_plugin.c \
    -I/path/to/lv00/include \
    -L/path/to/lv00/lib -llv00

# Windows
cl /LD my_plugin.c /I C:\lv00\include \
   /link C:\lv00\lib\lv00.lib /OUT:my_plugin.dll
```

**Python插件（使用Python嵌入桥接）**：
```python
# my_python_plugin.py
from lv00.plugin import PythonPluginBase

class MyPlugin(PythonPluginBase):
    name = "my_python_plugin"
    version = 1
    
    def on_load(self, ctx):
        # 注册Python函数作为几何构造器
        ctx.register_constructor("my_construct", self.my_construct)
        return True
    
    def my_construct(self, graph, inputs):
        # 使用Python实现几何构造
        p1, p2 = inputs
        midpoint = graph.add_point((p1.x + p2.x) / 2, (p1.y + p2.y) / 2)
        return [midpoint]
```

### 14.2 迁移指南

#### 14.2.1 从旧结构迁移到新结构

**迁移步骤**：

1. **备份现有代码**：
```bash
git checkout -b migration/ten-layer
git tag pre-ten-layer-backup
```

2. **创建新目录结构**：
```bash
mkdir -p layer6_interactive/{web_gui,python_gui,renderers,websocket}
mkdir -p layer7_binding/python/{highlevel,presets,dsl,streaming,helpers}
mkdir -p layer8_ai_assistant/{core,api_server,knowledge_base,templates}
mkdir -p layer9_monitoring/{core,web_dashboard,cli,tests}
mkdir -p layer10_ecosystem/{formal_verification,examples,axiom_packages,docs,tools}
mkdir -p core/src/python_binding
```

3. **迁移文件（使用git mv保留历史）**：
```bash
# Python绑定迁移
git mv module/python/lv00/_ctypes_binding.py core/src/python_binding/
git mv module/python/lv00/groebner_engine.py core/src/python_binding/groebner.py
git mv module/python/lv00/sparse_la.py core/src/python_binding/

# 后5层迁移
git mv web/gui/src layer6_interactive/web_gui/
git mv module/python/lv00/ws_server.py layer6_interactive/websocket/
git mv module/llm_coding_assistant layer8_ai_assistant/
git mv module/concurrent_monitor layer9_monitoring/
```

4. **更新导入路径**：
```python
# 兼容性shim（保留旧导入路径6个月）
# module/python/lv00/__init__.py
import warnings
warnings.warn(
    "Importing from 'module.python.lv00' is deprecated. "
    "Use 'lv00' instead.",
    DeprecationWarning,
    stacklevel=2
)

from core.src.python_binding import *  # 重定向到新路径
```

5. **更新构建配置**：
```toml
# pyproject.toml 更新
[tool.setuptools.packages.find]
where = ["layer7_binding/python", "core/src/python_binding"]
```

#### 14.2.2 兼容性Shim设计

**目标**：确保现有用户代码在迁移期间继续工作

```python
# layer7_binding/python/compat/shim.py
"""兼容性shim，将旧导入重定向到新位置"""

import sys
import importlib
from typing import Any

# 旧模块名 → 新模块名映射
MODULE_REDIRECTS = {
    "module.python.lv00.core": "lv00.core",
    "module.python.lv00.engine": "lv00.engine",
    "module.python.lv00.formula": "lv00.formula",
    "module.python.lv00.proof_extras": "lv00.proof_extras",
    "module.python.lv00.stream_bridge": "lv00.streaming",
    "module.python.lv00.interactive_geo": "lv00.interactive",
    "module.python.lv00.py_euclid_style": "lv00.highlevel",
    "module.python.lv00.preset_basic": "lv00.presets.basic",
    "module.python.lv00.preset_analysis": "lv00.presets.analysis",
    "module.python.lv00.dsl": "lv00.dsl",
}

class CompatibilityImporter:
    """兼容性导入器"""
    
    def find_module(self, fullname: str, path: str = None) -> Any:
        if fullname in MODULE_REDIRECTS:
            return self
        return None
    
    def load_module(self, fullname: str) -> Any:
        new_name = MODULE_REDIRECTS[fullname]
        module = importlib.import_module(new_name)
        sys.modules[fullname] = module  # 注册旧名
        return module

# 注册兼容性导入器
sys.meta_path.insert(0, CompatibilityImporter())
```

**弃用时间表**：

| 阶段 | 时间 | 行为 |
|------|------|------|
| 阶段1 | 0-3个月 | 静默重定向，记录使用日志 |
| 阶段2 | 3-6个月 | 发出DeprecationWarning |
| 阶段3 | 6-9个月 | 发出FutureWarning |
| 阶段4 | 9-12个月 | 移除shim，抛出ImportError |

### 14.3 自动化工具

#### 14.3.1 层依赖检查工具

```python
#!/usr/bin/env python3
# scripts/check_layer_deps.py

"""检查层间依赖违规"""

import ast
import os
import sys
from pathlib import Path
from collections import defaultdict
import argparse

# 层定义
LAYERS = {
    "core/src/shared": 0,
    "core/src/layer1_parser": 1,
    "core/src/layer2_resource": 2,
    "core/src/layer3_geometry": 3,
    "core/src/layer4_reasoning": 4,
    "core/src/layer5_output": 5,
    "core/src/python_binding": 5,  # 绑定层与第5层同级
    "layer6_interactive": 6,
    "layer7_binding": 7,
    "layer8_ai_assistant": 8,
    "layer9_monitoring": 9,
    "layer10_ecosystem": 10,
}

def get_layer(path: str) -> int:
    """获取文件所属层"""
    for prefix, layer in sorted(LAYERS.items(), key=lambda x: -len(x[0])):
        if prefix in path:
            return layer
    return -1

def extract_imports(file_path: Path) -> list:
    """提取Python文件中的所有导入"""
    try:
        tree = ast.parse(file_path.read_text(encoding="utf-8"))
    except SyntaxError:
        return []
    
    imports = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                imports.append(alias.name)
        elif isinstance(node, ast.ImportFrom):
            if node.module:
                imports.append(node.module)
    return imports

def check_dependencies(root_dir: str, strict: bool = False) -> list:
    """检查依赖违规"""
    violations = []
    root = Path(root_dir)
    
    for py_file in root.rglob("*.py"):
        layer = get_layer(str(py_file))
        if layer < 0:
            continue
        
        imports = extract_imports(py_file)
        for imp in imports:
            # 解析导入目标层
            imp_path = imp.replace(".", "/")
            target_layer = get_layer(imp_path)
            if target_layer < 0:
                continue
            
            # 检查反向依赖
            if target_layer > layer:
                violations.append({
                    "file": str(py_file),
                    "layer": layer,
                    "import": imp,
                    "target_layer": target_layer,
                })
    
    return violations

def main():
    parser = argparse.ArgumentParser(description="Check layer dependencies")
    parser.add_argument("--root", default=".", help="Project root directory")
    parser.add_argument("--strict", action="store_true", help="Fail on any violation")
    parser.add_argument("--output", help="Output file for violations")
    args = parser.parse_args()
    
    violations = check_dependencies(args.root, args.strict)
    
    if violations:
        print(f"Found {len(violations)} dependency violations:")
        for v in violations:
            print(f"  Layer {v['layer']} file imports Layer {v['target_layer']}: {v['file']} -> {v['import']}")
        
        if args.output:
            import json
            Path(args.output).write_text(json.dumps(violations, indent=2))
        
        if args.strict:
            sys.exit(1)
    else:
        print("No dependency violations found.")

if __name__ == "__main__":
    main()
```

#### 14.3.2 绑定代码生成工具

```python
#!/usr/bin/env python3
# scripts/generate_binding.py

"""从C头文件自动生成Python ctypes绑定代码"""

import re
import argparse
from pathlib import Path
from typing import List, Dict

# C类型 → ctypes类型映射
CTYPE_MAP = {
    "int": "c_int",
    "int32_t": "c_int32",
    "int64_t": "c_int64",
    "uint32_t": "c_uint32",
    "uint64_t": "c_uint64",
    "size_t": "c_size_t",
    "float": "c_float",
    "double": "c_double",
    "bool": "c_bool",
    "char*": "c_char_p",
    "const char*": "c_char_p",
    "void*": "c_void_p",
    "void": "None",
}

class CParser:
    """简单C头文件解析器"""
    
    def __init__(self, content: str):
        self.content = content
    
    def parse_functions(self) -> List[Dict]:
        """解析函数声明"""
        # 匹配函数声明：返回类型 函数名(参数列表);
        pattern = r'(\w+[\w\s*]+)\s+(\w+)\s*\(([^)]*)\)\s*;'
        functions = []
        for match in re.finditer(pattern, self.content):
            return_type = match.group(1).strip()
            func_name = match.group(2)
            params = match.group(3)
            functions.append({
                "return_type": return_type,
                "name": func_name,
                "params": self._parse_params(params),
            })
        return functions
    
    def _parse_params(self, params_str: str) -> List[Dict]:
        """解析参数列表"""
        params = []
        for param in params_str.split(','):
            param = param.strip()
            if not param or param == 'void':
                continue
            # 提取类型和参数名
            parts = param.rsplit(' ', 1)
            if len(parts) == 2:
                param_type, param_name = parts
            else:
                param_type = parts[0]
                param_name = "arg"
            params.append({
                "type": param_type.strip(),
                "name": param_name.strip().replace('*', ''),
            })
        return params

def generate_python_binding(header_file: Path, output_file: Path):
    """生成Python绑定代码"""
    parser = CParser(header_file.read_text())
    functions = parser.parse_functions()
    
    lines = [
        '# Auto-generated from {}'.format(header_file.name),
        'from ctypes import *',
        '',
        '# Function prototypes',
    ]
    
    for func in functions:
        # 转换返回类型
        restype = CTYPE_MAP.get(func["return_type"], "c_void_p")
        
        # 转换参数类型
        argtypes = []
        for param in func["params"]:
            ctype = CTYPE_MAP.get(param["type"], "c_void_p")
            argtypes.append(ctype)
        
        lines.append(f'# {func["return_type"]} {func["name"]}(...)')
        lines.append(f'lib.{func["name"]}.argtypes = [{", ".join(argtypes)}]')
        lines.append(f'lib.{func["name"]}.restype = {restype}')
        lines.append('')
    
    output_file.write_text('\n'.join(lines))
    print(f"Generated: {output_file}")

def main():
    parser = argparse.ArgumentParser(description="Generate Python bindings from C headers")
    parser.add_argument("headers", nargs="+", help="C header files")
    parser.add_argument("--output-dir", default="core/src/python_binding/generated", help="Output directory")
    args = parser.parse_args()
    
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    for header in args.headers:
        header_path = Path(header)
        output = output_dir / f"{header_path.stem}_binding.py"
        generate_python_binding(header_path, output)

if __name__ == "__main__":
    main()
```

#### 14.3.3 文档生成工具

```python
#!/usr/bin/env python3
# scripts/generate_docs.py

"""从代码注释生成API文档"""

import ast
import re
from pathlib import Path
from typing import List, Dict
import json

class DocGenerator:
    """文档生成器"""
    
    def __init__(self, source_dir: str):
        self.source_dir = Path(source_dir)
        self.docs = {"modules": []}
    
    def generate(self) -> Dict:
        """生成完整文档"""
        for py_file in self.source_dir.rglob("*.py"):
            module_doc = self._parse_module(py_file)
            if module_doc:
                self.docs["modules"].append(module_doc)
        return self.docs
    
    def _parse_module(self, py_file: Path) -> Dict:
        """解析Python模块"""
        try:
            tree = ast.parse(py_file.read_text())
        except SyntaxError:
            return None
        
        module_name = str(py_file.relative_to(self.source_dir)).replace("/", ".").replace(".py", "")
        
        classes = []
        functions = []
        
        for node in ast.walk(tree):
            if isinstance(node, ast.ClassDef):
                classes.append(self._parse_class(node))
            elif isinstance(node, ast.FunctionDef):
                functions.append(self._parse_function(node))
        
        return {
            "name": module_name,
            "docstring": ast.get_docstring(tree),
            "classes": classes,
            "functions": functions,
        }
    
    def _parse_class(self, node: ast.ClassDef) -> Dict:
        """解析类定义"""
        methods = []
        for item in node.body:
            if isinstance(item, ast.FunctionDef):
                methods.append(self._parse_function(item))
        
        return {
            "name": node.name,
            "docstring": ast.get_docstring(node),
            "bases": [base.id for base in node.bases if isinstance(base, ast.Name)],
            "methods": methods,
        }
    
    def _parse_function(self, node: ast.FunctionDef) -> Dict:
        """解析函数定义"""
        return {
            "name": node.name,
            "docstring": ast.get_docstring(node),
            "args": [arg.arg for arg in node.args.args],
            "lineno": node.lineno,
        }
    
    def to_markdown(self) -> str:
        """生成Markdown文档"""
        lines = ["# Lv-00 API Reference\n"]
        
        for module in self.docs["modules"]:
            lines.append(f"## {module['name']}\n")
            if module["docstring"]:
                lines.append(f"{module['docstring']}\n")
            
            for cls in module["classes"]:
                lines.append(f"### class {cls['name']}\n")
                if cls["docstring"]:
                    lines.append(f"{cls['docstring']}\n")
                
                for method in cls["methods"]:
                    args = ", ".join(method["args"])
                    lines.append(f"#### `{method['name']}({args})`\n")
                    if method["docstring"]:
                        lines.append(f"{method['docstring']}\n")
            
            for func in module["functions"]:
                args = ", ".join(func["args"])
                lines.append(f"### `{func['name']}({args})`\n")
                if func["docstring"]:
                    lines.append(f"{func['docstring']}\n")
        
        return "\n".join(lines)

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate API documentation")
    parser.add_argument("--source", default=".", help="Source directory")
    parser.add_argument("--output", default="docs/API_REFERENCE.md", help="Output file")
    parser.add_argument("--format", choices=["markdown", "json"], default="markdown", help="Output format")
    args = parser.parse_args()
    
    generator = DocGenerator(args.source)
    docs = generator.generate()
    
    if args.format == "markdown":
        output = generator.to_markdown()
    else:
        output = json.dumps(docs, indent=2)
    
    Path(args.output).write_text(output, encoding="utf-8")
    print(f"Documentation generated: {args.output}")

if __name__ == "__main__":
    main()
```

#### 14.3.4 迁移辅助工具

```python
#!/usr/bin/env python3
# scripts/migrate_module.py

"""辅助模块迁移的自动化工具"""

import shutil
from pathlib import Path
import argparse
import re

# 迁移规则
MIGRATION_RULES = {
    # 源路径 → 目标路径
    "module/python/lv00/_ctypes_binding.py": "core/src/python_binding/_ctypes_binding.py",
    "module/python/lv00/groebner_engine.py": "core/src/python_binding/groebner.py",
    "module/python/lv00/sparse_la.py": "core/src/python_binding/sparse_la.py",
    "module/python/lv00/type_system.py": "core/src/python_binding/type_system.py",
    "module/python/lv00/func_block.py": "core/src/python_binding/func_block.py",
    "web/gui/src": "layer6_interactive/web_gui/src",
    "module/python/lv00/ws_server.py": "layer6_interactive/websocket/ws_server.py",
    "module/llm_coding_assistant": "layer8_ai_assistant",
    "module/concurrent_monitor": "layer9_monitoring",
    "formal": "layer10_ecosystem/formal_verification/formal",
    "lv00-formal": "layer10_ecosystem/formal_verification/lv00_formal",
    "examples": "layer10_ecosystem/examples",
    "module/axiom_packages": "layer10_ecosystem/axiom_packages",
    "doc": "layer10_ecosystem/docs",
    "tool": "layer10_ecosystem/tools",
}

def migrate_file(src: Path, dst: Path, dry_run: bool = True):
    """迁移单个文件"""
    if dry_run:
        print(f"[DRY RUN] Would move: {src} -> {dst}")
        return
    
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(str(src), str(dst))
    print(f"Moved: {src} -> {dst}")

def update_imports(directory: Path, old_prefix: str, new_prefix: str):
    """更新目录中所有Python文件的导入语句"""
    for py_file in directory.rglob("*.py"):
        content = py_file.read_text()
        # 替换导入路径
        updated = re.sub(
            rf'from\s+{re.escape(old_prefix)}',
            f'from {new_prefix}',
            content
        )
        updated = re.sub(
            rf'import\s+{re.escape(old_prefix)}',
            f'import {new_prefix}',
            updated
        )
        if updated != content:
            py_file.write_text(updated)
            print(f"Updated imports: {py_file}")

def main():
    parser = argparse.ArgumentParser(description="Migrate modules to new structure")
    parser.add_argument("--root", default=".", help="Project root")
    parser.add_argument("--execute", action="store_true", help="Actually execute migrations")
    args = parser.parse_args()
    
    root = Path(args.root)
    
    for src_rel, dst_rel in MIGRATION_RULES.items():
        src = root / src_rel
        dst = root / dst_rel
        
        if not src.exists():
            print(f"Skip (not found): {src}")
            continue
        
        migrate_file(src, dst, dry_run=not args.execute)
    
    if args.execute:
        # 更新导入路径
        update_imports(root / "core/src/python_binding", "module.python", "lv00")
        update_imports(root / "layer7_binding", "module.python", "lv00")

if __name__ == "__main__":
    main()
```

### 14.4 文档清单

| 文档 | 路径 | 优先级 | 状态 |
|------|------|--------|------|
| 架构手册（10层） | `doc/ARCHITECTURE_MANUAL.md` | P0 | 需更新 |
| Python绑定开发指南 | `doc/PYTHON_BINDING_GUIDE.md` | P0 | 本章14.1.1 |
| 插件开发指南 | `doc/PLUGIN_DEVELOPMENT_GUIDE.md` | P0 | 本章14.1.2 |
| 层间边界规范 | `doc/LAYER_BOUNDARY_SPEC.md` | P0 | 第13章 |
| 迁移指南 | `doc/MIGRATION_GUIDE.md` | P1 | 本章14.2 |
| API参考 | `doc/API_REFERENCE.md` | P1 | 由工具生成 |
| 测试指南 | `doc/TESTING_GUIDE.md` | P1 | 第13章 |
| CI/CD配置说明 | `doc/CI_CD_GUIDE.md` | P2 | 第13章 |

---

## 15. 性能优化策略

> 本章定义 Lv-00 十层架构的性能优化策略，涵盖 FFI 调用优化、符号计算缓存、内存管理、求解器加速、流式处理和性能基准测试。

### 15.1 FFI调用优化

**目标**：减少 Python→C 的 FFI 跨界调用次数，降低调用开销。

**量化目标**：FFI 调用次数减少 80%

#### 15.1.1 批量调用接口

```c
/* core/include/lv00/python_binding/lv00_python.h 扩展 */

/* 批量调用参数 */
typedef struct {
    const char* function_name;
    Lv00PythonArg* args;
    int arg_count;
} Lv00PythonCall;

/* 批量调用结果 */
typedef struct {
    int call_index;
    Lv00PythonResult result;
} Lv00PythonCallResult;

/* 批量调用接口 */
bool lv00_python_batch_call(
    Lv00Engine* engine,
    Lv00PythonCall* calls,
    int call_count,
    Lv00PythonCallResult** out_results
);
```

#### 15.1.2 批量调用优化器

```python
# core/src/python_binding/batch_call.py

from typing import List, Tuple, Any, Optional
from . import lib
import ctypes

class BatchCallOptimizer:
    """批量FFI调用优化器 —— 缓存多个调用，一次性传递到C层。"""

    def __init__(self, engine, flush_threshold: int = 10):
        self._engine = engine
        self._pending: List[Tuple[str, list]] = []
        self._flush_threshold = flush_threshold

    def call(self, func_name: str, *args) -> None:
        """缓存调用，达到阈值时自动flush。"""
        self._pending.append((func_name, list(args)))
        if len(self._pending) >= self._flush_threshold:
            self.flush()

    def flush(self) -> Optional[List[Any]]:
        """执行所有缓存的调用，返回结果列表。"""
        if not self._pending:
            return None

        # 构建C层批量调用结构
        calls = (Lv00PythonCall * len(self._pending))()
        for i, (name, args) in enumerate(self._pending):
            calls[i].function_name = name.encode()
            calls[i].args = self._pack_args(args)
            calls[i].arg_count = len(args)

        # 一次性传递到C层
        results_ptr = ctypes.POINTER(Lv00PythonCallResult)()
        count = ctypes.c_int()
        lib.lv00_python_batch_call(
            self._engine._ptr, calls, len(self._pending),
            ctypes.byref(results_ptr), ctypes.byref(count)
        )

        self._pending = []
        return [results_ptr[i].result for i in range(count.value)]

    def _pack_args(self, args: list) -> ctypes.Array:
        """将Python参数打包为C结构体数组。"""
        packed = (Lv00PythonArg * len(args))()
        for i, arg in enumerate(args):
            if isinstance(arg, int):
                packed[i].type = LV00_PY_ARG_INT
                packed[i].value.i = arg
            elif isinstance(arg, float):
                packed[i].type = LV00_PY_ARG_FLOAT
                packed[i].value.f = arg
            elif isinstance(arg, str):
                packed[i].type = LV00_PY_ARG_STRING
                packed[i].value.s = arg.encode()
            else:
                packed[i].type = LV00_PY_ARG_POINTER
                packed[i].value.p = arg._ptr if hasattr(arg, '_ptr') else id(arg)
        return packed
```

#### 15.1.3 FFI调用频率监控

```python
# core/src/python_binding/ffi_monitor.py

import time
import threading
from collections import defaultdict
from typing import Dict, List

class FFICallMonitor:
    """FFI调用频率监控器 —— 检测热点函数。"""

    def __init__(self, sample_interval: float = 1.0):
        self._counts: Dict[str, int] = defaultdict(int)
        self._lock = threading.Lock()
        self._sample_interval = sample_interval
        self._running = False

    def record_call(self, func_name: str) -> None:
        """记录一次FFI调用。"""
        with self._lock:
            self._counts[func_name] += 1

    def get_hotspots(self, top_n: int = 10) -> List[Tuple[str, int]]:
        """获取调用频率最高的N个函数。"""
        with self._lock:
            sorted_funcs = sorted(
                self._counts.items(), key=lambda x: x[1], reverse=True
            )
            return sorted_funcs[:top_n]

    def reset(self) -> None:
        """重置计数器。"""
        with self._lock:
            self._counts.clear()

    def start_periodic_report(self) -> None:
        """启动周期性报告线程。"""
        self._running = True
        def _report_loop():
            while self._running:
                time.sleep(self._sample_interval)
                hotspots = self.get_hotspots(5)
                if hotspots:
                    print(f"[FFI Monitor] Top 5: {hotspots}")
                self.reset()
        t = threading.Thread(target=_report_loop, daemon=True)
        t.start()
```

---

### 15.2 符号计算缓存

**目标**：缓存 SymbolicCoord 的计算结果，避免重复计算。

**量化目标**：重复计算减少 90%

#### 15.2.1 LRU缓存策略

```c
/* core/include/lv00/symbolic_cache.h（新增） */

#ifndef LV00_SYMBOLIC_CACHE_H
#define LV00_SYMBOLIC_CACHE_H

#include <stdbool.h>
#include <stddef.h>

/* 缓存条目 */
typedef struct {
    unsigned int key_hash;       /* 输入参数的哈希 */
    SymbolicCoord* result;      /* 缓存的计算结果 */
    unsigned int access_count;  /* 访问计数（用于LRU淘汰） */
    bool is_valid;              /* 是否有效 */
} Lv00CacheEntry;

/* LRU缓存 */
typedef struct {
    Lv00CacheEntry* entries;
    int capacity;
    int count;
    unsigned int hits;
    unsigned int misses;
} Lv00SymbolicCache;

/* 缓存操作 */
Lv00SymbolicCache* lv00_cache_create(int capacity);
void lv00_cache_destroy(Lv00SymbolicCache* cache);

/* 查询缓存（命中返回缓存的SymbolicCoord，未命中返回NULL） */
SymbolicCoord* lv00_cache_lookup(
    Lv00SymbolicCache* cache,
    const char* operation,
    const SymbolicCoord** inputs,
    int input_count
);

/* 插入缓存 */
void lv00_cache_insert(
    Lv00SymbolicCache* cache,
    const char* operation,
    const SymbolicCoord** inputs,
    int input_count,
    SymbolicCoord* result
);

/* 使缓存失效（图变更时调用） */
void lv00_cache_invalidate(Lv00SymbolicCache* cache);
void lv00_cache_invalidate_by_node(Lv00SymbolicCache* cache, int node_id);

/* 缓存统计 */
double lv00_cache_hit_rate(const Lv00SymbolicCache* cache);

#endif /* LV00_SYMBOLIC_CACHE_H */
```

#### 15.2.2 缓存失效策略

```c
/* 图变更时自动清除关联缓存 */
void lv00_graph_on_node_changed(
    Lv00ConstraintGraph* graph,
    int node_id,
    int change_type  /* ADD/REMOVE/UPDATE */
) {
    /* 通知所有缓存清除与该节点相关的条目 */
    if (graph->symbolic_cache) {
        lv00_cache_invalidate_by_node(graph->symbolic_cache, node_id);
    }
    /* 通知下游节点清除缓存（级联失效） */
    int* dependents = lv00_graph_get_dependents(graph, node_id);
    for (int i = 0; dependents[i] >= 0; i++) {
        lv00_cache_invalidate_by_node(graph->symbolic_cache, dependents[i]);
    }
}
```

---

### 15.3 内存管理优化

**目标**：通过分层内存池减少内存分配次数和峰值内存。

**量化目标**：内存分配次数减少 60%，峰值内存减少 30%

#### 15.3.1 分层内存池设计

```c
/* core/include/lv00/memory_pool_enhanced.h（新增） */

/* 内存池层级 */
typedef enum {
    LV00_POOL_TINY = 0,    /* 小对象池：< 64字节（SymbolicCoord等） */
    LV00_POOL_SMALL,       /* 中对象池：64-512字节（节点、约束等） */
    LV00_POOL_LARGE,        /* 大对象池：512-4096字节（矩阵、方程组等） */
    LV00_POOL_HUGE,         /* 超大对象池：> 4096字节（AST、图结构等） */
    LV00_POOL_TEMP,         /* 临时对象池：函数调用期间的临时分配 */
} Lv00PoolTier;

/* 分层内存池 */
typedef struct {
    Lv00ObjectPool* tiny_pool;
    Lv00ObjectPool* small_pool;
    Lv00ObjectPool* large_pool;
    Lv00ObjectPool* huge_pool;
    Lv00LinearAllocator* temp_allocator;  /* 临时分配器（栈式释放） */
    Lv00MemPoolStats stats;               /* 全局统计 */
} Lv00TieredMemoryPool;

/* 创建分层内存池 */
Lv00TieredMemoryPool* lv00_tiered_pool_create(
    size_t tiny_size,    /* 小对象池总大小（字节） */
    size_t small_size,
    size_t large_size,
    size_t huge_size
);

/* 按大小自动选择池层级的分配 */
void* lv00_tiered_alloc(Lv00TieredMemoryPool* pool, size_t size);
void lv00_tiered_free(Lv00TieredMemoryPool* pool, void* ptr, size_t size);

/* 临时分配（函数返回时自动释放） */
void* lv00_temp_alloc(Lv00TieredMemoryPool* pool, size_t size);
void lv00_temp_reset(Lv00TieredMemoryPool* pool);  /* 重置临时池 */
```

#### 15.3.2 内存池统计与泄漏检测

```c
/* 运行时内存统计（与第12章 Lv00MemPoolStats 一致） */
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t peak_usage;
    size_t allocation_count;
    size_t free_count;
    size_t pool_tiny_usage;
    size_t pool_small_usage;
    size_t pool_large_usage;
    size_t pool_huge_usage;
    size_t pool_temp_usage;
} Lv00MemPoolStatsDetailed;

Lv00MemPoolStatsDetailed lv00_pool_get_stats(Lv00TieredMemoryPool* pool);

/* 泄漏检测：检查 total_allocated != total_freed */
bool lv00_pool_check_leak(Lv00TieredMemoryPool* pool);
```

---

### 15.4 求解器性能优化

**目标**：通过增量求解和并行策略加速约束求解。

**量化目标**：平均求解时间减少 50%

#### 15.4.1 增量求解

```c
/* core/include/lv00/solver_incremental.h（新增） */

/* 增量求解上下文 */
typedef struct {
    Lv00ConstraintGraph* graph;
    void* last_solution;       /* 上次求解结果缓存 */
    int* changed_nodes;        /* 自上次求解后变更的节点 */
    int changed_count;
    bool is_valid;              /* 缓存是否有效 */
} Lv00IncrementalSolver;

/* 增量求解（仅求解变更部分） */
bool lv00_solve_incremental(
    Lv00IncrementalSolver* solver,
    Lv00ProofResult* out_result
);

/* 标记图变更（使增量缓存失效） */
void lv00_incremental_solver_invalidate(Lv00IncrementalSolver* solver);
```

#### 15.4.2 并行求解策略

```c
/* 独立约束子图并行求解 */
typedef struct {
    int subgraph_id;
    int* node_ids;
    int node_count;
    Lv00ProofResult partial_result;
} Lv00SubgraphTask;

/* 将约束图分解为独立子图 */
int lv00_graph_decompose(
    const Lv00ConstraintGraph* graph,
    Lv00SubgraphTask** out_tasks,
    int* out_task_count
);

/* 并行求解多个子图 */
bool lv00_solve_parallel(
    Lv00SubgraphTask* tasks,
    int task_count,
    int max_threads,
    Lv00ProofResult* out_result
);
```

---

### 15.5 流式处理优化

**目标**：通过事件批处理和背压控制降低事件处理延迟。

**量化目标**：事件处理延迟降低 70%

#### 15.5.1 事件批处理

```python
# core/src/python_binding/stream_batch.py

import asyncio
import time
from typing import List, Dict, Any
from .stream_native import StreamEvent

class EventBatcher:
    """事件批处理器 —— 合并高频小事件，减少序列化和推送开销。"""

    def __init__(
        self,
        max_batch_size: int = 50,
        max_wait_ms: float = 100.0,
        merge_threshold: int = 10
    ):
        self._pending: List[StreamEvent] = []
        self._max_batch_size = max_batch_size
        self._max_wait_ms = max_wait_ms
        self._merge_threshold = merge_threshold
        self._last_flush = time.time()

    def add_event(self, event: StreamEvent) -> bool:
        """添加事件到批处理队列。达到阈值时返回True表示应flush。"""
        self._pending.append(event)
        # 检查是否应flush
        if (len(self._pending) >= self._max_batch_size or
            (time.time() - self._last_flush) * 1000 >= self._max_wait_ms):
            return True
        return False

    def flush(self) -> List[StreamEvent]:
        """取出所有待处理事件（合并可合并的事件）。"""
        events = self._merge_events(self._pending)
        self._pending = []
        self._last_flush = time.time()
        return events

    def _merge_events(self, events: List[StreamEvent]) -> List[StreamEvent]:
        """合并同类型事件（如多个NODE_UPDATED合并为一个）。"""
        if len(events) <= self._merge_threshold:
            return events
        merged = {}
        for event in events:
            key = (event.event_type, event.payload.get("node_id"))
            if key in merged:
                # 保留最新的事件
                if event.timestamp > merged[key].timestamp:
                    merged[key] = event
            else:
                merged[key] = event
        return list(merged.values())
```

#### 15.5.2 背压控制

```python
class BackpressureController:
    """背压控制器 —— 消费者速度跟不上时降级处理。"""

    def __init__(self, max_queue_size: int = 1000, drop_policy: str = "oldest"):
        self._max_queue_size = max_queue_size
        self._drop_policy = drop_policy  # "oldest" | "newest" | "throttle"
        self._queue_size = 0
        self._is_throttled = False

    def before_push(self) -> bool:
        """推送前检查。返回False表示应丢弃。"""
        if self._queue_size >= self._max_queue_size:
            self._is_throttled = True
            return self._drop_policy != "newest"
        self._is_throttled = False
        return True

    def after_pop(self) -> None:
        """消费后更新状态。"""
        if self._queue_size < self._max_queue_size * 0.7:
            self._is_throttled = False
```

---

### 15.6 性能基准测试

#### 15.6.1 基准测试框架

```python
# tests/benchmarks/bench_runner.py

import time
import statistics
from dataclasses import dataclass, field
from typing import List, Callable, Dict, Any

@dataclass
class BenchResult:
    """基准测试结果"""
    name: str
    iterations: int
    times_ms: List[float] = field(default_factory=list)

    @property
    def mean_ms(self) -> float:
        return statistics.mean(self.times_ms)

    @property
    def std_ms(self) -> float:
        return statistics.stdev(self.times_ms) if len(self.times_ms) > 1 else 0.0

    @property
    def p50_ms(self) -> float:
        return statistics.median(self.times_ms)

    @property
    def p99_ms(self) -> float:
        sorted_times = sorted(self.times_ms)
        idx = int(len(sorted_times) * 0.99)
        return sorted_times[min(idx, len(sorted_times) - 1)]

class BenchRunner:
    """基准测试运行器"""

    def __init__(self, warmup: int = 100, iterations: int = 1000):
        self._warmup = warmup
        self._iterations = iterations
        self._results: List[BenchResult] = []

    def bench(self, name: str, func: Callable, *args) -> BenchResult:
        """运行单个基准测试。"""
        result = BenchResult(name=name, iterations=self._iterations)

        # 预热
        for _ in range(self._warmup):
            func(*args)

        # 正式测试
        for _ in range(self._iterations):
            start = time.perf_counter()
            func(*args)
            elapsed = (time.perf_counter() - start) * 1000
            result.times_ms.append(elapsed)

        self._results.append(result)
        return result

    def report(self) -> str:
        """生成基准测试报告。"""
        lines = [f"{'Benchmark':<40} {'Mean':>10} {'Std':>10} {'P50':>10} {'P99':>10}"]
        lines.append("-" * 80)
        for r in self._results:
            lines.append(
                f"{r.name:<40} {r.mean_ms:>10.3f} {r.std_ms:>10.3f} "
                f"{r.p50_ms:>10.3f} {r.p99_ms:>10.3f}"
            )
        return "\n".join(lines)
```

#### 15.6.2 关键路径基准

| 基准名称 | 测量内容 | 目标 |
|----------|---------|------|
| `bench_point_distance` | 两点距离计算（符号精度） | < 1us |
| `bench_line_intersection` | 直线交点计算 | < 5us |
| `bench_triangle_circumcenter` | 三角形外心计算 | < 10us |
| `bench_graph_add_100_nodes` | 添加100个节点 | < 1ms |
| `bench_solve_simple` | 简单命题求解 | < 100ms |
| `bench_solve_complex` | 复杂命题求解 | < 10s |
| `bench_proof_export` | 证明导出为TikZ | < 50ms |
| `bench_batch_call_10` | 10个FFI批量调用 | < 100us |
| `bench_cache_hit` | 缓存命中查询 | < 0.1us |
| `bench_event_batch_flush` | 50个事件批处理flush | < 1ms |

#### 15.6.3 CI集成

```yaml
# .github/workflows/benchmark.yml（补充到第13章CI配置中）
name: Performance Benchmark

on:
  pull_request:
    paths:
      - 'core/**'
      - 'module/python/**'

jobs:
  benchmark:
    name: Run Benchmarks
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build C Core
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
      - name: Run Benchmarks
        run: |
          python tests/benchmarks/run_all.py --output benchmark_results.json
      - name: Compare with Baseline
        run: |
          python tests/benchmarks/compare.py \
            --baseline benchmark_baseline.json \
            --current benchmark_results.json \
            --threshold 0.1
      - name: Upload Results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-results
          path: benchmark_results.json
```

---

## 16. 安全策略

> 本章定义 Lv-00 十层架构的安全策略，涵盖插件安全、输入验证、内存安全、网络安全、AI辅助安全和安全审计。

### 16.1 插件安全

#### 16.1.1 插件签名验证

```c
/* core/include/lv00/plugin_security.h（新增） */

typedef enum {
    LV00_SIG_OK = 0,
    LV00_SIG_NO_SIGNATURE,
    LV00_SIG_INVALID_FORMAT,
    LV00_SIG_HASH_MISMATCH,
    LV00_SIG_KEY_UNTRUSTED,
    LV00_SIG_EXPIRED,
    LV00_SIG_INTERNAL_ERROR
} Lv00SignatureResult;

/* 验证插件签名 */
Lv00SignatureResult lv00_plugin_verify_signature(
    const char* plugin_path,
    const char* manifest_path
);

/* 添加可信公钥 */
bool lv00_plugin_add_trusted_key(
    const char* public_key_pem,
    const char* key_id
);

/* 设置签名验证策略 */
void lv00_plugin_set_enforcement(bool enforce);
```

#### 16.1.2 插件沙箱机制

```c
typedef struct {
    uint32_t cpu_time_limit_seconds;
    size_t max_rss_bytes;
    const char** allowed_paths;
    size_t allowed_path_count;
    bool allow_network;
    bool allow_fork;
    int max_open_fds;
    int max_threads;
} Lv00SandboxConfig;

/* 默认只读沙箱 */
static inline Lv00SandboxConfig lv00_sandbox_readonly(void) {
    Lv00SandboxConfig cfg = {0};
    cfg.cpu_time_limit_seconds = 30;
    cfg.max_rss_bytes = 64 * 1024 * 1024;
    cfg.allow_network = false;
    cfg.allow_fork = false;
    cfg.max_open_fds = 16;
    cfg.max_threads = 0;
    return cfg;
}

/* 应用沙箱配置 */
bool lv00_sandbox_apply(const Lv00SandboxConfig* config);
bool lv00_sandbox_check(const Lv00SandboxConfig* config, char* violation, size_t len);
```

#### 16.1.3 插件权限模型

```c
typedef enum {
    LV00_PERM_READONLY = 0,
    LV00_PERM_CONSTRUCTION = 1,
    LV00_PERM_FULL = 2
} Lv00PermissionLevel;

/* 权限检查宏 */
#define LV00_REQUIRE_PERMISSION(plugin, required_level, retval) do { \
    if ((plugin) == NULL) return (retval); \
    Lv00PermissionLevel _current = lv00_plugin_get_permission(plugin); \
    if (_current < (required_level)) { \
        lv00_audit_log(plugin, LV00_AUDIT_PERMISSION_DENIED, \
            "权限不足: 需要 %s, 当前 %s", \
            lv00_perm_level_str(required_level), \
            lv00_perm_level_str(_current)); \
        return (retval); \
    } \
} while(0)
```

#### 16.1.4 插件审计日志

```c
typedef enum {
    LV00_AUDIT_PLUGIN_LOAD = 0,
    LV00_AUDIT_PLUGIN_UNLOAD,
    LV00_AUDIT_PERMISSION_DENIED,
    LV00_AUDIT_RESOURCE_VIOLATION,
    LV00_AUDIT_SIGNATURE_FAILURE,
    LV00_AUDIT_API_CALL,
    LV00_AUDIT_SANDBOX_VIOLATION,
} Lv00AuditEventType;

/* 审计日志格式：[ISO8601] [AUDIT] plugin="<name>" event=<type> msg="<message>" */
void lv00_audit_log(const Lv00Plugin* plugin, Lv00AuditEventType type, const char* fmt, ...);
```

---

### 16.2 输入验证

#### 16.2.1 DSL注入检测

```c
/* 预定义注入检测模式 */
static const Lv00InjectionPattern INJECTION_PATTERNS[] = {
    { ";rm",     "可能的命令注入",     2 },
    { "|sh",     "可能的管道注入",     2 },
    { "../",     "路径遍历尝试",       2 },
    { "%n",      "格式化字符串攻击",    2 },
    { "#include","可疑的预处理指令",     2 },
    { "#define", "可疑的预处理指令",     2 },
};

/* DSL安全检查（组合检查） */
Lv00ErrorCode lv00_dsl_security_check(
    const char* input, size_t len,
    char* error, size_t err_len
);
```

#### 16.2.2 几何参数边界检查

```c
#define LV00_COORD_MAX        1e15
#define LV00_COORD_MIN        1e-10
#define LV00_DISTANCE_EPSILON 1e-12

/* 安全坐标检查 */
#define LV00_CHECK_COORD(x, retval) do { \
    double _v = (x); \
    if (isnan(_v) || isinf(_v) || fabs(_v) > LV00_COORD_MAX) return (retval); \
} while(0)

/* 安全除法 */
#define LV00_SAFE_DIV(num, den, default) \
    (fabs(den) < LV00_DISTANCE_EPSILON ? (default) : ((num) / (den)))

/* 三角形参数验证 */
static inline bool lv00_validate_triangle(double a, double b, double c) {
    if (a <= 0 || b <= 0 || c <= 0) return false;
    if (a + b <= c + LV00_DISTANCE_EPSILON) return false;
    if (a + c <= b + LV00_DISTANCE_EPSILON) return false;
    if (b + c <= a + LV00_DISTANCE_EPSILON) return false;
    return true;
}
```

#### 16.2.3 约束图深度限制

```c
#define LV00_PROPAGATION_MAX_DEPTH  1024
#define LV00_PROOF_SEARCH_MAX_DEPTH 4096
#define LV00_NODE_MAX_CONSTRAINTS   64
#define LV00_GRAPH_MAX_NODES        100000

/* 深度守卫 */
typedef struct {
    int propagation_depth;
    int proof_search_depth;
    uint64_t total_steps;
} Lv00DepthGuard;

#define LV00_DEPTH_ENTER(guard, field, max_depth, retval) do { \
    (guard)->field##_depth++; \
    if ((guard)->field##_depth > (max_depth)) return (retval); \
    (guard)->total_steps++; \
} while(0)

#define LV00_DEPTH_LEAVE(guard, field) do { \
    (guard)->field##_depth--; \
} while(0)
```

---

### 16.3 内存安全

#### 16.3.1 安全字符串操作

```c
/* 替代 strcpy */
#define LV00_STRCPY(dst, size, src) do { \
    if ((size) > 0) { strncpy((dst), (src), (size) - 1); (dst)[(size) - 1] = '\0'; } \
} while(0)

/* 替代 strcat */
#define LV00_STRCAT(dst, size, src) do { \
    size_t _cl = strlen(dst); \
    size_t _rem = (size) > _cl ? (size) - _cl - 1 : 0; \
    if (_rem > 0) strncat((dst), (src), _rem); \
} while(0)

/* 替代 sprintf */
static inline int lv00_safe_snprintf(char* buf, size_t size, const char* fmt, ...) {
    if (size == 0) return 0;
    va_list args; va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    buf[size - 1] = '\0';
    return (ret < 0) ? 0 : ret;
}
```

#### 16.3.2 引用计数与安全释放

```c
#define LV00_REFCOUNT_HEADER \
    volatile int32_t _ref_count; \
    void (*_on_zero_ref)(void *self)

#define LV00_REFCOUNT_INIT(obj, destructor) do { \
    (obj)->_ref_count = 1; \
    (obj)->_on_zero_ref = (destructor); \
} while(0)

/* 安全释放（释放后置NULL） */
#define LV00_SAFE_FREE(ptr) do { \
    if ((ptr)) { lv00_free(ptr); (ptr) = NULL; } \
} while(0)
```

#### 16.3.3 内存泄漏检测

```c
typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    int freed;
} Lv00AllocRecord;

/* 获取泄漏快照 */
Lv00LeakSnapshot lv00_leak_detector_snapshot(void);

/* 打印泄漏报告 */
void lv00_leak_detector_report(const Lv00LeakSnapshot* snapshot);

/* 断言无泄漏（用于测试退出时） */
int lv00_leak_detector_assert_clean(void);
```

---

### 16.4 网络安全

#### 16.4.1 WebSocket认证

```python
class WSSecurityMiddleware:
    """WebSocket安全中间件 —— Token认证 + 速率限制 + 方法授权。"""

    def __init__(self, config):
        self.config = config
        self._ip_connections: Dict[str, int] = {}
        self._identities: Dict[int, ClientIdentity] = {}
        self._message_timestamps: Dict[int, list] = {}

    async def authenticate(self, websocket, client_id):
        """握手认证：等待客户端发送auth消息，验证JWT。"""
        if not self.config.auth_enabled:
            return ClientIdentity("dev_user", "admin", "", time.time(), "127.0.0.1")
        try:
            msg = await asyncio.wait_for(websocket.recv(), timeout=5.0)
        except asyncio.TimeoutError:
            await websocket.close(code=4001, reason="Authentication timeout")
            return None
        # 验证JWT...
```

#### 16.4.2 CORS策略

```python
def configure_cors(app: FastAPI, environment: str = "production"):
    if environment == "development":
        app.add_middleware(CORSMiddleware,
            allow_origins=["http://localhost:3000", "http://localhost:5173"],
            allow_methods=["GET", "POST", "OPTIONS"])
    else:
        origins = os.environ.get("LV00_CORS_ORIGINS", "").split(",")
        if not origins or "*" in origins:
            raise ValueError("生产环境禁止使用CORS通配符")
        app.add_middleware(CORSMiddleware, allow_origins=origins)
```

#### 16.4.3 速率限制

```python
class SlidingWindowRateLimiter:
    """滑动窗口速率限制器。"""
    def __init__(self, max_requests: int = 100, window_seconds: float = 60.0):
        self._windows: Dict[str, list] = defaultdict(list)

    async def allow(self, key: str) -> bool:
        now = time.time()
        cutoff = now - self._window_seconds
        timestamps = self._windows[key]
        while timestamps and timestamps[0] <= cutoff:
            timestamps.pop(0)
        if len(timestamps) >= self._max_requests:
            return False
        timestamps.append(now)
        return True
```

---

### 16.5 AI辅助安全

#### 16.5.1 LLM代码沙箱执行

```python
class SafeCodeValidator(ast.NodeVisitor):
    """AST安全检查器 —— 禁止import/exec/eval/open等危险操作。"""
    ALLOWED_IMPORTS = {"math", "cmath", "fractions", "decimal"}
    FORBIDDEN_ATTRS = {"__import__", "exec", "eval", "compile", "open", "input"}

    def visit_Import(self, node):
        for alias in node.names:
            if alias.name not in self.ALLOWED_IMPORTS:
                self.errors.append(f"禁止导入模块: {alias.name}")
```

#### 16.5.2 提示词注入防护

```python
INJECTION_PATTERNS = [
    r"ignore\s+(all\s+)?previous\s+instructions",
    r"you\s+are\s+now\s+a",
    r"system\s*:\s*",
    r"reveal\s+(your|the|system)\s+(prompt|instructions)",
]

def detect_prompt_injection(user_input: str) -> Optional[str]:
    for pattern in _COMPILED_PATTERNS:
        match = pattern.search(user_input)
        if match:
            return f"检测到提示词注入模式: '{match.group()}'"
    return None
```

---

### 16.6 安全审计清单

| 编号 | 检查项 | 通过标准 |
|------|--------|---------|
| S-01 | AddressSanitizer全量测试 | 零error |
| S-02 | 内存泄漏检测 | outstanding_count=0 |
| S-03 | cppcheck静态分析 | 零error |
| S-05 | 插件签名验证 | 已签名通过、篡改拒绝 |
| S-06 | 插件沙箱资源限制 | CPU/内存超限被终止 |
| S-08 | DSL注入检测 | 已知攻击模式全部拦截 |
| S-11 | WebSocket认证 | 无token连接被拒绝 |
| S-14 | AI代码沙箱 | 危险代码被拒绝 |
| S-17 | 硬编码凭证扫描 | 零发现 |

**漏洞响应SLA**：

| 严重性 | 响应时间 | 修复时间 |
|--------|---------|---------|
| Critical | 4小时 | 48小时 |
| High | 24小时 | 7天 |
| Medium | 72小时 | 30天 |
| Low | 1周 | 90天 |

---

## 17. 外部依赖、代码规范与发布流程

> 本章定义 Lv-00 的外部依赖管理、全栈代码规范、代码审查流程和发布流程。

### 17.1 外部依赖矩阵

#### 17.1.1 C核心依赖

| 依赖项 | 最低版本 | 用途 | 必需 |
|--------|---------|------|------|
| CMake | >= 3.15 | 构建系统 | 是 |
| GCC/Clang/MSVC | >= 11/14/2019 | 编译器 | 是 |
| GMP | >= 6.2 | 多精度算术 | 是 |
| Python3-dev | >= 3.10 | ctypes绑定 | 是 |
| Ninja | >= 1.10 | 构建加速 | 否 |

#### 17.1.2 Python依赖

| 依赖项 | 最低版本 | 用途 | 必需 |
|--------|---------|------|------|
| numpy | >= 1.24.0 | 数值计算 | 是 |
| sympy | >= 1.12 | 符号代数 | 是 |
| pytest | >= 7.0 | 测试框架 | 开发 |
| openai | >= 1.0 | OpenAI API | 可选 |
| websocket-server | >= 0.6 | WebSocket | 可选 |

#### 17.1.3 前端依赖

| 依赖项 | 最低版本 | 用途 | 必需 |
|--------|---------|------|------|
| react | >= 18.0 | UI框架 | 是 |
| vite | >= 5.0 | 构建工具 | 是 |
| typescript | >= 5.0 | 类型系统 | 是 |

#### 17.1.4 依赖安全扫描

```yaml
# .github/workflows/security.yml
name: Dependency Security Scan
on:
  schedule:
    - cron: '0 2 * * 1'  # 每周一
jobs:
  python-scan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: pip install pip-audit
      - run: pip-audit -r requirements-lock.txt --strict
  frontend-scan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: cd web && npm ci && npm audit --audit-level=high
  secret-scan:
    runs-on: ubuntu-latest
    steps:
      - uses: gitleaks/gitleaks-action@v2
```

---

### 17.2 C代码规范

#### 17.2.1 命名约定

| 类别 | 规范 | 示例 |
|------|------|------|
| 公共函数 | `lv00_` 前缀 + 小写下划线 | `lv00_graph_add_node()` |
| 类型/结构体 | `Lv00` 前缀 + PascalCase | `Lv00ConstraintGraph` |
| 枚举值 | `LV00_` 前缀 + 全大写 | `LV00_ERROR_INVALID_ARG` |
| 宏常量 | `LV00_` 前缀 + 全大写 | `LV00_MAX_TOKEN_COUNT` |
| 内部函数 | `_lv00_` 前缀 | `_lv00_graph_validate_edge()` |

#### 17.2.2 错误处理模式

- 返回值约定：`int` 类型，`LV00_OK`（0）表示成功
- 错误码分层：Layer N 错误码范围 `N*100 ~ N*100+99`
- 所有公共函数必须检查 NULL 入参
- 出错时必须释放已分配的中间资源

#### 17.2.3 内存管理规则

- 统一使用 `lv00_malloc()` / `lv00_free()`
- 禁止混用裸 `malloc`/`free`
- 释放后必须置 NULL
- `_create`/`_alloc` 后缀表示调用者获得所有权
- `_get` 后缀表示不转移所有权

#### 17.2.4 代码注释规范（Doxygen风格）

```c
/**
 * @brief 向约束图添加新节点
 *
 * @param[in]  ctx    引擎上下文（不可为NULL）
 * @param[in]  type   节点类型
 * @param[out] out_id 输出新节点的ID
 * @return LV00_OK 成功
 * @return LV00_ERROR_NULL_ARG ctx或out_id为NULL
 *
 * @par 线程安全
 * 调用者需持有ctx的写锁。
 */
```

---

### 17.3 Python代码规范

#### 17.3.1 命名约定（PEP 8扩展）

| 类别 | 规范 | 示例 |
|------|------|------|
| 模块名 | 小写下划线 | `lv00.binding.engine` |
| 类名 | PascalCase | `GeometrySession` |
| 函数/方法 | 小写下划线 | `add_point()` |
| 常量 | 全大写下划线 | `MAX_RECURSION_DEPTH` |
| ctypes绑定 | 双下划线前缀 | `__lv00_parser_tokenize` |

#### 17.3.2 类型注解要求

所有公共API必须包含完整类型注解：

```python
def add_point(self, x: float, y: float, name: str = "") -> int:
    """添加几何点到约束图。"""
    ...

def solve(
    self, strategy: str = "auto",
    timeout_ms: int = 30000,
    callback: Optional[Callable[[str], None]] = None,
) -> SolveResult:
    """执行约束求解。"""
    ...
```

#### 17.3.3 测试规范

| 要求 | 标准 |
|------|------|
| 测试框架 | pytest |
| 测试命名 | `test_<行为描述>` |
| 正常路径 | 每个公共方法至少1个 |
| 异常路径 | 每个公共方法至少1个 |
| Mock外部依赖 | 不依赖真实网络 |
| 覆盖率目标 | 新增代码 >= 80% |

---

### 17.4 前端代码规范

| 规则 | 说明 |
|------|------|
| 函数组件 | 统一使用函数组件 + Hooks |
| TypeScript严格模式 | 全部strict选项开启 |
| Props类型 | 使用 `interface` 定义 |
| 状态管理 | `useState`（本地）/ `useContext`（共享） |
| 测试ID | 关键元素添加 `data-testid` |

---

### 17.5 代码审查流程

#### 17.5.1 PR模板

```markdown
## 变更类型
- [ ] feat / fix / refactor / perf / docs / test / chore

## 影响层级
- [ ] Layer 1-5 / 绑定层 / Layer 6-10

## 层依赖合规
- [ ] 未引入跨层反向依赖

## 测试
- [ ] C测试通过 / Python测试通过 / 层依赖检查通过
- [ ] 新增代码覆盖率 >= 80%

## 安全审查
- [ ] 无新安全风险 / 无硬编码凭证
```

#### 17.5.2 审查人要求

| 审查类型 | 审查人要求 | 适用场景 |
|---------|-----------|---------|
| 标准审查 | 1名项目成员 | 常规修复 |
| 架构审查 | 至少1名架构审查人 | 层间接口变更 |
| 安全审查 | 至少1名安全审查人 | 输入解析/认证 |
| 性能审查 | 至少1名性能审查人 | 热点路径优化 |

#### 17.5.3 自动化审查

| 检查项 | 工具 | 阻断合并 |
|--------|------|---------|
| C编译+格式 | CMake + clang-format | 是 |
| C静态分析 | cppcheck | 是 |
| Python Lint+格式 | ruff + black | 是 |
| Python类型 | mypy --strict | 是 |
| 层依赖 | check_layer_deps.py | 是 |
| TypeScript | tsc --noEmit | 是 |

---

### 17.6 发布流程

#### 17.6.1 语义化版本规则

| 版本段 | 递增条件 | 示例 |
|--------|---------|------|
| MAJOR | 破坏性API变更 | 5.0.0 -> 6.0.0 |
| MINOR | 新增功能（向后兼容） | 5.0.0 -> 5.1.0 |
| PATCH | Bug修复 | 5.0.0 -> 5.0.1 |

#### 17.6.2 版本号同步清单

版本号必须在以下位置完全一致：
- `CMakeLists.txt` -> `project(lv00 VERSION X.Y.Z)`
- `core/include/lv00/lv00.h` -> `LV00_VERSION_*` 宏
- `pyproject.toml` -> `version = "X.Y.Z"`
- `package.json` -> `"version": "X.Y.Z"`
- `CHANGELOG.md` -> `## [X.Y.Z]`

```python
# scripts/verify_version_sync.py
def main():
    versions = {
        "CMakeLists.txt": extract_cmake_version(root),
        "lv00.h": extract_lv00_h_version(root),
        "pyproject.toml": extract_pyproject_version(root),
        "package.json": extract_package_json_version(root),
    }
    # 检查所有版本号一致...
```

#### 17.6.3 发布检查清单

```
[ ] 1. 编译验证（Linux/macOS/Windows，零警告）
[ ] 2. 测试通过（ctest + pytest + npm test）
[ ] 3. 文档更新（CHANGELOG + API文档 + 迁移指南）
[ ] 4. 版本号同步（verify_version_sync.py 通过）
[ ] 5. 安全检查（无硬编码凭证，依赖扫描通过）
[ ] 6. 变更日志（所有变更已记录）
[ ] 7. 发布产物（C库 + Python wheel + 前端构建）
```

#### 17.6.4 变更日志格式（Keep a Changelog）

```markdown
## [5.1.0] - 2026-06-XX

### 新增 (Added)
- **功能简述**：详细描述

### 修复 (Fixed)
- **Bug修复简述**：影响范围

### 安全 (Security)
- **安全修复简述**：漏洞描述
```

#### 17.6.5 发布自动化

```yaml
# .github/workflows/release.yml
on:
  push:
    tags: ['v*']
jobs:
  verify:
    runs-on: ubuntu-latest
    steps:
      - run: python scripts/verify_version_sync.py
      - run: grep -q "\[$TAG\]" CHANGELOG.md
  build-c:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
    steps:
      - run: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
  publish-python:
    needs: [verify, build-c]
    steps:
      - uses: pypa/gh-action-pypi-publish@release/v1
  github-release:
    needs: [build-c, publish-python]
    steps:
      - uses: softprops/action-gh-release@v2
```

#### 17.6.6 紧急补丁流程

```
1. 创建 hotfix/X.Y.Z 分支
2. 实施最小化修复 + 针对性测试
3. 更新版本号 + CHANGELOG
4. 快速审查（1名审查人）
5. 合并后立即打标签发布
6. 同步到dev分支
7. 评估是否需要安全公告
```

---

**文档状态**: 已完成（v5.0.0，含性能、安全、规范与发布设计）
**下一步**: 按阶段0开始执行C核心编译修复
