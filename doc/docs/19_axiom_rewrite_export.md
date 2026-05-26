# 公理规则引擎、扩展重写策略与导出互操作 (Axiom Rule Engine, Extended Rewrite Strategy & Export Interop)

## 模块概述

本组模块构成 Lv-00 的推理引擎基础设施与生态互操作层。公理规则引擎提供可配置规则库与难度分级，扩展重写策略引擎支持四种经典归约策略，TikZ 导出与互操作模块实现多格式导出/导入与跨语言通信，关系模型层借鉴 Alloy 提供声明式关系逻辑，开放生态包管理与高效索引检索为系统提供可扩展性与性能保障。

## 核心设计原则

1. **规则可配置**：公理规则库支持运行时动态加载、难度分级与优先级调度
2. **策略可插拔**：innermost / outermost / parallel / e-graph 四种归约策略可独立切换与组合
3. **格式无关**：多格式导出（Coq / Lean / HTML / SVG / TikZ / GeoJSON）与多格式导入（GeoGebra / GeoJSON / SVG）实现生态互通
4. **结构指纹**：基于 FNV-1a 的约束图结构指纹支持增量变化检测与缓存失效
5. **声明式关系**：借鉴 Alloy 的关系逻辑模型，支持有限范围实例查找与逻辑公式求值

## 覆盖模块总览

| 模块 | 职责 |
|------|------|
| axiom_rule_engine.h | 公理规则引擎，可配置规则库 |
| axiom_grade.h | 公理分级系统，级进解锁 |
| rewrite_strategy.h | 扩展重写策略引擎 |
| tikz_export.h | TikZ 几何导出与渲染 |
| interop.h | 跨语言互操作与多格式导出/导入 |
| stream_context_util.h | 流式上下文注册与分发 |
| relation_model.h | 关系模型层（Alloy 风格） |
| ecosystem.h | 开放生态包管理 |
| graph_hash.h | 基于 FNV-1a 的约束图指纹 |
| fast_index.h | 高效索引与检索 |

## 1. axiom_rule_engine.h -- 公理规则引擎

### 设计概述

提供可配置的公理规则库，支持运行时动态加载规则、按难度分级筛选、按优先级调度执行。规则引擎是证明搜索的核心驱动，为上层推理模块提供统一的规则查询与匹配接口。

### 规则类型枚举（8 种）

```c
typedef enum {
    LV00_RULE_CONGRUENCE = 0,   /* 全等公理 —— 三角形/线段/角全等 */
    LV00_RULE_SIMILARITY,       /* 相似公理 —— 三角形相似判定 */
    LV00_RULE_PARALLEL,         /* 平行公理 —— 平行线判定与性质 */
    LV00_RULE_PERPENDICULAR,    /* 垂直公理 —— 垂直判定与性质 */
    LV00_RULE_ANGLE,            /* 角度公理 —— 角度计算与等量关系 */
    LV00_RULE_AREA,             /* 面积公理 —— 面积计算与等量关系 */
    LV00_RULE_CIRCLE,           /* 圆公理 —— 圆周角、切线、割线 */
    LV00_RULE_COORDINATE        /* 坐标公理 —— 解析几何方法 */
} Lv00RuleType;
```

### 规则优先级（5 级）

```c
typedef enum {
    LV00_PRIORITY_CRITICAL = 0,  /* 关键规则：必须优先尝试 */
    LV00_PRIORITY_HIGH,          /* 高优先级：高成功率的规则 */
    LV00_PRIORITY_NORMAL,        /* 普通优先级：默认级别 */
    LV00_PRIORITY_LOW,           /* 低优先级：消耗较大的规则 */
    LV00_PRIORITY_EXPLORATORY    /* 探索性规则：启发式搜索 */
} Lv00RulePriority;
```

### 规则结构

```c
typedef struct {
    Lv00RuleType type;           /* 规则类型 */
    Lv00RulePriority priority;   /* 优先级 */
    char *name;                  /* 规则名称 */
    char *description;           /* 规则描述 */
    int difficulty;              /* 难度等级（1-10） */
    bool enabled;                /* 是否启用 */
    double success_rate;         /* 历史成功率（0.0-1.0） */
    uint64_t invocation_count;   /* 调用次数 */
    uint64_t success_count;      /* 成功次数 */
    void *user_data;             /* 用户自定义数据 */
} Lv00Rule;
```

### 规则引擎上下文

```c
typedef struct {
    Lv00Rule **rules;            /* 规则数组 */
    int rule_count;
    int rule_capacity;
    int max_difficulty;          /* 当前允许的最大难度 */
    bool auto_tune;              /* 自动调优优先级 */
    uint64_t total_applications; /* 总应用次数 */
} Lv00RuleEngine;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建/销毁 | `lv00_rule_engine_create/destroy` | 生命周期管理 |
| 注册规则 | `lv00_rule_engine_register(engine, rule)` | 注册新规则 |
| 注销规则 | `lv00_rule_engine_unregister(engine, type)` | 按类型注销 |
| 按类型查询 | `lv00_rule_engine_get_rule(engine, type)` | 获取指定类型规则 |
| 按优先级获取 | `lv00_rule_engine_get_by_priority(engine, priority, out, count)` | 获取指定优先级规则列表 |
| 按难度筛选 | `lv00_rule_engine_filter_by_difficulty(engine, max_diff, out, count)` | 筛选难度范围内规则 |
| 应用规则 | `lv00_rule_engine_apply(engine, rule, context)` | 应用规则到当前上下文 |
| 设置难度上限 | `lv00_rule_engine_set_max_difficulty(engine, max)` | 限制可用规则难度 |
| 自动调优 | `lv00_rule_engine_auto_tune(engine)` | 根据历史成功率调整优先级 |
| 统计信息 | `lv00_rule_engine_get_stats(engine)` | 获取引擎统计 |

## 2. axiom_grade.h -- 公理分级系统

### 设计概述

提供公理的分级管理与证明风格选择机制。通过 Lv00AxiomGrade 四级枚举实现级进解锁：初学者仅接触基础公理，高级用户可使用全部公理集。同时支持四种证明风格选择，影响推理方向和策略。

### 公理等级枚举（4 级）

```c
typedef enum {
    LV00_GRADE_FOUNDATIONAL = 0,  /* 基础级：基本定义与公理 */
    LV00_GRADE_INTERMEDIATE,      /* 中级：常用定理与判定条件 */
    LV00_GRADE_ADVANCED,          /* 高级：复杂定理与组合方法 */
    LV00_GRADE_RESEARCH           /* 研究级：前沿方法与开放问题 */
} Lv00AxiomGrade;
```

### 证明风格枚举（4 种）

```c
typedef enum {
    LV00_PROOF_FORWARD = 0,   /* 正向证明：从已知条件出发推导结论 */
    LV00_PROOF_BACKWARD,      /* 反向证明：从目标出发回溯已知条件 */
    LV00_PROOF_BY_CONTRADICTION, /* 反证法：假设结论不成立推出矛盾 */
    LV00_PROOF_INDUCTION      /* 归纳法：数学归纳（有限归纳变体） */
} Lv00ProofStyle;
```

### 分级配置

```c
typedef struct {
    Lv00AxiomGrade current_grade;     /* 当前解锁等级 */
    Lv00ProofStyle default_style;     /* 默认证明风格 */
    bool auto_unlock;                 /* 自动解锁下一级 */
    int problems_solved;              /* 已解决问题数 */
    int unlock_threshold;             /* 解锁阈值（解决问题数） */
    char *grade_names[4];             /* 等级名称 */
    char *grade_descriptions[4];      /* 等级描述 */
} Lv00GradeConfig;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建配置 | `lv00_grade_config_create()` | 创建默认分级配置 |
| 设置等级 | `lv00_grade_config_set_grade(config, grade)` | 设置当前等级 |
| 获取等级 | `lv00_grade_config_get_grade(config)` | 获取当前等级 |
| 检查可用 | `lv00_grade_config_is_available(config, grade)` | 检查等级是否已解锁 |
| 设置风格 | `lv00_grade_config_set_style(config, style)` | 设置证明风格 |
| 获取风格 | `lv00_grade_config_get_style(config)` | 获取证明风格 |
| 记录解题 | `lv00_grade_config_record_solve(config)` | 记录解题并检查解锁 |
| 检查解锁 | `lv00_grade_config_check_unlock(config)` | 检查是否满足解锁条件 |
| 等级名称 | `lv00_grade_get_name(grade)` | 获取等级名称字符串 |
| 等级描述 | `lv00_grade_get_description(grade)` | 获取等级描述字符串 |

## 3. rewrite_strategy.h -- 扩展重写策略引擎

### 设计概述

提供四种经典归约策略的实现与统一接口，支持策略切换、组合和性能监控。重写策略引擎是表达式化简、等式推理和约束传播的核心驱动。

### 策略类型枚举

```c
typedef enum {
    LV00_STRATEGY_INNERMOST = 0,  /* 最内层优先：先归约最深的子项 */
    LV00_STRATEGY_OUTERMOST,      /* 最外层优先：先归约最浅的子项 */
    LV00_STRATEGY_PARALLEL,       /* 并行策略：同时归约所有可归约子项 */
    LV00_STRATEGY_EGRAPH          /* E-graph 策略：等类合并与提取 */
} Lv00RewriteStrategy;
```

### 重写规则

```c
typedef struct {
    char *name;                   /* 规则名称 */
    char *pattern;                /* 匹配模式（表达式字符串） */
    char *replacement;            /* 替换模板（表达式字符串） */
    int priority;                 /* 优先级 */
    bool enabled;                 /* 是否启用 */
    uint64_t applications;        /* 应用次数 */
} Lv00RewriteRule;
```

### 重写上下文

```c
typedef struct {
    Lv00RewriteStrategy strategy;     /* 当前策略 */
    Lv00RewriteRule **rules;          /* 规则数组 */
    int rule_count;
    int max_iterations;               /* 最大迭代次数 */
    int max_depth;                    /* 最大递归深度 */
    bool normalize_after;             /* 归约后规范化 */
    uint64_t total_rewrites;          /* 总重写次数 */
    uint64_t iterations_used;         /* 实际迭代次数 */
} Lv00RewriteContext;
```

### E-graph 专用结构

```c
typedef struct Lv00EGraph {
    Lv00EClass **classes;         /* 等价类数组 */
    int class_count;
    int class_capacity;
    Lv00ENode **nodes;            /* e-node 数组 */
    int node_count;
    int node_capacity;
    /* E-graph 操作 */
    int (*find)(struct Lv00EGraph *eg, int id);
    void (*merge)(struct Lv00EGraph *eg, int a, int b);
    void (*add_node)(struct Lv00EGraph *eg, ...);
    void *extract)(struct Lv00EGraph *eg, int id, double cost_fn);
} Lv00EGraph;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建上下文 | `lv00_rewrite_context_create(strategy)` | 创建指定策略的上下文 |
| 销毁上下文 | `lv00_rewrite_context_destroy(ctx)` | 释放资源 |
| 添加规则 | `lv00_rewrite_context_add_rule(ctx, rule)` | 注册重写规则 |
| 执行归约 | `lv00_rewrite(context, expr)` | 对表达式执行归约 |
| 策略切换 | `lv00_rewrite_context_set_strategy(ctx, strategy)` | 运行时切换策略 |
| E-graph 创建 | `lv00_egraph_create()` | 创建 E-graph |
| E-graph 合并 | `lv00_egraph_merge(eg, a, b)` | 合并两个等价类 |
| E-graph 提取 | `lv00_egraph_extract(eg, id, cost_fn)` | 从 E-graph 提取最优表达式 |
| 统计信息 | `lv00_rewrite_context_get_stats(ctx)` | 获取归约统计 |

## 4. tikz_export.h -- TikZ 几何导出与渲染

### 设计概述

将约束图中的几何对象和关系导出为 TikZ/PGF 代码，支持 28 种 TikZ 元素映射、trust_color 到 TikZ 样式的自动转换，以及基于 WASM 的服务端渲染后端。

### TikZ 元素类型（28 种）

```c
typedef enum {
    TIKZ_POINT = 0, TIKZ_LINE, TIKZ_SEGMENT, TIKZ_RAY,
    TIKZ_CIRCLE, TIKZ_ARC, TIKZ_ELLIPSE, TIKZ_TRIANGLE,
    TIKZ_RECTANGLE, TIKZ_POLYGON, TIKZ_VECTOR, TIKZ_ANGLE,
    TIKZ_ANGLE_MARK, TIKZ_RIGHT_ANGLE, TIKZ_GRID,
    TIKZ_AXIS, TIKZ_LABEL, TIKZ_NODE, TIKZ_COORDINATE,
    TIKZ_FILL, TIKZ_PATTERN, TIKZ_ARROW, TIKZ_DASHED,
    TIKZ_DOTTED, TIKZ_THICK, TIKZ_THIN, TIKZ_COLOR,
    TIKZ_TRANSFORM, TIKZ_CLIP
} TikZElementType;
```

### 信任颜色到 TikZ 样式映射

```c
typedef struct {
    double trust_level;           /* 信任度值 [0, 1] */
    char *tikz_color;             /* TikZ 颜色名 */
    char *tikz_line_style;        /* 线型（solid/dashed/dotted） */
    double line_width;            /* 线宽 */
    double opacity;               /* 透明度 */
} TikZTrustStyle;
```

信任度分级映射示例：

| 信任度范围 | TikZ 颜色 | 线型 | 含义 |
|-----------|-----------|------|------|
| [0.9, 1.0] | blue, solid | 实线 | 已证明 |
| [0.7, 0.9) | teal, solid | 实线 | 高置信度 |
| [0.5, 0.7) | orange, dashed | 虚线 | 中置信度 |
| [0.3, 0.5) | red!60, dashed | 虚线 | 低置信度 |
| [0.0, 0.3) | red, dotted | 点线 | 待验证 |

### 导出配置

```c
typedef struct {
    double scale;                 /* 缩放因子 */
    double x_offset, y_offset;    /* 偏移量 */
    bool show_grid;               /* 显示网格 */
    bool show_axis;               /* 显示坐标轴 */
    bool show_labels;             /* 显示标签 */
    bool show_trust_colors;       /* 显示信任颜色 */
    char *preamble;               /* TikZ 导言区代码 */
    char *document_class;         /* 文档类 */
    int precision;                /* 坐标精度 */
} TikZExportConfig;
```

### WASM 渲染后端

```c
typedef struct {
    bool wasm_available;          /* WASM 运行时是否可用 */
    char *wasm_path;              /* WASM 模块路径 */
    int canvas_width, canvas_height; /* 画布尺寸 */
    double dpi;                   /* 渲染 DPI */
    char *output_format;          /* 输出格式：png/svg/pdf */
} TikZWasmRenderer;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建配置 | `tikz_export_config_create()` | 创建默认导出配置 |
| 导出 TikZ | `tikz_export(graph, config)` | 约束图 -> TikZ 代码 |
| 导出完整文档 | `tikz_export_full(graph, config)` | 包含导言区的完整 LaTeX 文档 |
| 信任样式映射 | `tikz_trust_to_style(trust)` | 信任度 -> TikZ 样式 |
| WASM 渲染 | `tikz_render_wasm(code, renderer, out)` | WASM 后端渲染为图像 |
| 元素样式 | `tikz_element_style(element, trust)` | 元素 + 信任度 -> 样式字符串 |

## 5. interop.h -- 跨语言互操作

### 设计概述

提供 WebSocket 和 STDIO 两种服务器模式，支持多格式导出（Coq / Lean / HTML / SVG / TikZ / GeoJSON）和多格式导入（GeoGebra / GeoJSON / SVG），实现 Lv-00 与外部证明助手、几何工具和 Web 前端的互操作。

### 服务器模式

```c
typedef enum {
    INTEROP_SERVER_WEBSOCKET,  /* WebSocket 服务器（浏览器通信） */
    INTEROP_SERVER_STDIO       /* STDIO 服务器（命令行管道） */
} InteropServerMode;
```

### 导出格式

```c
typedef enum {
    INTEROP_EXPORT_COQ,        /* Coq 证明脚本 */
    INTEROP_EXPORT_LEAN,       /* Lean 4 证明脚本 */
    INTEROP_EXPORT_HTML,       /* HTML 交互式文档 */
    INTEROP_EXPORT_SVG,        /* SVG 矢量图形 */
    INTEROP_EXPORT_TIKZ,       /* TikZ/LaTeX 代码 */
    INTEROP_EXPORT_GEOJSON     /* GeoJSON 地理数据格式 */
} InteropExportFormat;
```

### 导入格式

```c
typedef enum {
    INTEROP_IMPORT_GEOGEBRA,   /* GeoGebra XML/JSON */
    INTEROP_IMPORT_GEOJSON,    /* GeoJSON */
    INTEROP_IMPORT_SVG         /* SVG 矢量图形 */
} InteropImportFormat;
```

### 互操作上下文

```c
typedef struct {
    InteropServerMode mode;         /* 服务器模式 */
    int port;                       /* WebSocket 端口 */
    char *host;                     /* 监听地址 */
    ConstraintGraph *graph;         /* 当前约束图 */
    void *server_handle;            /* 服务器句柄 */
    bool running;                   /* 运行状态 */
    /* 回调 */
    void (*on_message)(const char *msg, void *user_data);
    void (*on_error)(const char *error, void *user_data);
    void *user_data;
} InteropContext;
```

### 导入/导出结果

```c
typedef struct {
    bool success;
    char *output;                   /* 导出内容 */
    size_t output_size;
    char *content_type;             /* MIME 类型 */
    char error_message[256];
} InteropExportResult;

typedef struct {
    bool success;
    ConstraintGraph *graph;         /* 导入的约束图 */
    int imported_node_count;
    int imported_constraint_count;
    char error_message[256];
} InteropImportResult;
```

### 核心 API

| 类别 | 函数 | 说明 |
|------|------|------|
| 服务器管理 | `interop_server_create/start/stop/destroy` | 生命周期 |
| 导出 | `interop_export(ctx, format, graph)` | 约束图 -> 目标格式 |
| 导入 | `interop_import(ctx, format, data, size)` | 外部数据 -> 约束图 |
| 导出文件 | `interop_export_to_file(ctx, format, graph, path)` | 导出到文件 |
| 导入文件 | `interop_import_from_file(ctx, format, path)` | 从文件导入 |
| 消息发送 | `interop_send_message(ctx, msg)` | 发送消息到客户端 |
| 消息接收 | `interop_receive_message(ctx, timeout_ms)` | 接收客户端消息 |

## 6. stream_context_util.h -- 流式上下文注册与分发

### 设计概述

提供全局注册表机制，支持引擎核心状态的流式注册与一次性同步分发到所有订阅模块。确保各模块持有的上下文引用保持一致，避免状态不一致问题。

### 上下文注册表

```c
typedef struct {
    void *contexts[MAX_STREAM_CONTEXTS];  /* 上下文指针数组 */
    char *context_names[MAX_STREAM_CONTEXTS]; /* 上下文名称 */
    int context_count;
    /* 订阅者 */
    void (*subscribers[MAX_STREAM_SUBSCRIBERS])(void *context, void *user_data);
    void *subscriber_data[MAX_STREAM_SUBSCRIBERS];
    int subscriber_count;
    /* 同步状态 */
    bool dirty;                     /* 是否有未同步的更新 */
    uint64_t version;               /* 版本号（每次更新递增） */
} StreamContextRegistry;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 全局初始化 | `stream_context_init()` | 初始化全局注册表 |
| 注册上下文 | `stream_context_register(name, context)` | 注册上下文到全局表 |
| 注销上下文 | `stream_context_unregister(name)` | 从全局表移除 |
| 查找上下文 | `stream_context_find(name)` | 按名称查找上下文 |
| 订阅更新 | `stream_context_subscribe(callback, user_data)` | 注册订阅回调 |
| 取消订阅 | `stream_context_unsubscribe(callback)` | 取消订阅 |
| 同步分发 | `stream_context_sync_all()` | 将所有上下文同步到所有订阅者 |
| 标记脏 | `stream_context_mark_dirty()` | 标记有未同步更新 |
| 版本查询 | `stream_context_get_version()` | 获取当前版本号 |
| 销毁 | `stream_context_destroy()` | 销毁全局注册表 |

## 7. relation_model.h -- 关系模型层

### 设计概述

借鉴 Alloy 分析器的声明式关系逻辑模型，提供一阶关系逻辑的表达与求解能力。支持 13 种关系运算符和 12 种逻辑公式类型，可在有限范围内执行实例查找和公式求值。

### 关系运算符（13 种）

```c
typedef enum {
    REL_OP_JOIN = 0,         /* 关系连接（点号 . ） */
    REL_OP_TRANSPOSE,        /* 转置（波浪号 ~ ） */
    REL_OP_CLOSURE,          /* 传递闭包（星号 * ） */
    REL_OP_REFLEXIVE_CLOSURE,/* 自反传递闭包（^ ） */
    REL_OP_COMPOSITION,      /* 复合（分号 ; ） */
    REL_OP_INTERSECTION,     /* 交集（& ） */
    REL_OP_UNION,            /* 并集（\| ） */
    REL_OP_DIFFERENCE,       /* 差集（- ） */
    REL_OP_OVERRIDE,         /* 覆盖（++ ） */
    REL_OP_DOMAIN_RESTRICT,  /* 域限制（<: ） */
    REL_OP_RANGE_RESTRICT,   /* 值域限制（:> ） */
    REL_OP_PRODUCT,          /* 笛卡尔积（-> ） */
    REL_OP_TRANSPOSE_CLOSURE /* 转置闭包 */
} RelationOperator;
```

### 逻辑公式类型（12 种）

```c
typedef enum {
    REL_FORMULA_AND = 0,     /* 合取 */
    REL_FORMULA_OR,          /* 析取 */
    REL_FORMULA_NOT,         /* 否定 */
    REL_FORMULA_IMPLIES,     /* 蕴含 */
    REL_FORMULA_IFF,         /* 等价 */
    REL_FORMULA_FORALL,      /* 全称量化 */
    REL_FORMULA_EXISTS,      /* 存在量化 */
    REL_FORMULA_SOME,        /* 存在唯一 */
    REL_FORMULA_NO,          /* 不存在 */
    REL_FORMULA_ONE,         /* 恰好一个 */
    REL_FORMULA_LONE,        /* 至多一个 */
    REL_FORMULA_IN           /* 属于关系 */
} RelationFormulaType;
```

### 关系结构

```c
typedef struct {
    int arity;                    /* 关系元数 */
    int *domain_sizes;            /* 各域大小 */
    int domain_count;             /* 域数量 */
    bool *tuples;                 /* 元组存在性位图 */
    size_t tuple_count;           /* 元组总数 */
    char *name;                   /* 关系名称 */
} Lv00Relation;
```

### 关系模型上下文

```c
typedef struct {
    Lv00Relation **relations;     /* 关系数组 */
    int relation_count;
    char **sig_names;             /* 签名名称 */
    int *sig_sizes;               /* 签名大小 */
    int sig_count;                /* 签名数量 */
    int scope;                    /* 有限范围（实例查找上限） */
} Lv00RelationModel;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建模型 | `lv00_relation_model_create()` | 创建空关系模型 |
| 销毁模型 | `lv00_relation_model_destroy(model)` | 释放资源 |
| 添加签名 | `lv00_relation_model_add_sig(model, name, size)` | 添加类型签名 |
| 添加关系 | `lv00_relation_model_add_relation(model, name, arity, ...)` | 添加关系 |
| 关系运算 | `lv00_relation_apply_op(model, op, rel_a, rel_b)` | 执行关系运算 |
| 公式求值 | `lv00_relation_eval_formula(model, formula)` | 在有限范围内求值逻辑公式 |
| 实例查找 | `lv00_relation_find_instances(model, formula, max)` | 查找满足公式的实例 |
| 元组操作 | `lv00_relation_add_tuple/remove_tuple/contains_tuple` | 元组增删查 |

## 8. ecosystem.h -- 开放生态包管理

### 设计概述

提供 Lv-00 生态系统的包注册、版本管理、兼容性检查和一键部署能力。借鉴 npm / cargo 的包管理设计，结合 Docker 容器化部署，实现生态包的发现、安装和运行。

### 包信息

```c
typedef struct {
    char *name;                   /* 包名称 */
    char *version;                /* 版本号（语义化版本） */
    char *description;            /* 包描述 */
    char *author;                 /* 作者 */
    char *license;                /* 许可证 */
    char *repository;             /* 仓库地址 */
    char **dependencies;          /* 依赖列表 */
    int dependency_count;
    char **tags;                  /* 标签 */
    int tag_count;
    uint64_t download_count;      /* 下载次数 */
    char *checksum;               /* SHA256 校验和 */
} EcosystemPackage;
```

### 兼容性矩阵

```c
typedef struct {
    char *package_name;           /* 包名称 */
    char *min_core_version;       /* 最低核心版本 */
    char *max_core_version;       /* 最高核心版本 */
    char **tested_versions;       /* 已测试版本列表 */
    int tested_count;
    bool compatible;              /* 是否兼容 */
} CompatibilityEntry;
```

### 包注册表

```c
typedef struct {
    EcosystemPackage **packages;  /* 已注册包数组 */
    int package_count;
    CompatibilityEntry **compat_matrix; /* 兼容性矩阵 */
    int compat_count;
    char *registry_url;           /* 注册表 URL */
    char *cache_path;             /* 本地缓存路径 */
} EcosystemRegistry;
```

### Docker 一键体验

```c
typedef struct {
    char *image_name;             /* Docker 镜像名 */
    char *dockerfile_content;     /* Dockerfile 内容 */
    char **ports;                 /* 端口映射 */
    int port_count;
    char **volumes;               /* 卷映射 */
    int volume_count;
    char **env_vars;              /* 环境变量 */
    int env_count;
} DockerConfig;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 注册表管理 | `ecosystem_registry_create/destroy` | 生命周期 |
| 注册包 | `ecosystem_register_package(registry, pkg)` | 注册新包 |
| 查询包 | `ecosystem_query_package(registry, name)` | 按名称查询 |
| 搜索包 | `ecosystem_search_packages(registry, query, out, count)` | 按关键词搜索 |
| 安装包 | `ecosystem_install_package(registry, name, version)` | 安装指定版本 |
| 兼容性检查 | `ecosystem_check_compat(registry, name, core_version)` | 检查版本兼容性 |
| Docker 部署 | `ecosystem_docker_deploy(pkg, config)` | Docker 一键部署 |
| Docker 构建 | `ecosystem_docker_build(config, out_image)` | 构建 Docker 镜像 |

## 9. graph_hash.h -- 基于 FNV-1a 的约束图结构指纹

### 设计概述

基于 FNV-1a 哈希算法计算约束图的结构指纹，用于增量变化检测、缓存键生成和图等价性快速判定。结构指纹仅依赖图的拓扑结构和节点/约束类型，不受坐标值微扰影响。

### FNV-1a 常量

```c
#define FNV_1A_OFFSET_BASIS 0xcbf29ce484222325ULL
#define FNV_1A_PRIME        0x100000001b3ULL
```

### 图哈希结构

```c
typedef struct {
    uint64_t hash_value;          /* 最终哈希值 */
    uint64_t node_hash;           /* 节点类型哈希分量 */
    uint64_t edge_hash;           /* 边类型哈希分量 */
    uint64_t topology_hash;       /* 拓扑结构哈希分量 */
    int node_count;               /* 节点数量 */
    int edge_count;               /* 边数量 */
    bool is_valid;                /* 哈希是否有效 */
} GraphHash;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 计算哈希 | `graph_hash_compute(graph)` | 计算约束图的结构指纹 |
| 增量更新 | `graph_hash_update(hash, change)` | 根据变更增量更新哈希 |
| 比较哈希 | `graph_hash_equal(a, b)` | 比较两个图哈希是否相等 |
| 哈希字符串 | `graph_hash_to_string(hash)` | 哈希值转十六进制字符串 |
| 验证图 | `graph_hash_verify(graph, expected_hash)` | 验证图是否匹配预期哈希 |

### 哈希计算流程

```
1. 遍历所有节点，按类型和度数累积 node_hash
2. 遍历所有边（约束），按类型累积 edge_hash
3. 对邻接表排序后累积 topology_hash
4. 合并三分量：hash = FNV1a(node_hash || edge_hash || topology_hash)
```

## 10. fast_index.h -- 高效索引与检索

### 设计概述

提供五种高性能数据结构，覆盖通用键值查找、存在性检测、有序遍历、热点缓存和空间查询场景。所有数据结构支持可选的线程安全模式。

### 数据结构概览

| 数据结构 | 用途 | 平均复杂度 |
|----------|------|-----------|
| 哈希表 | 通用键值查找 | O(1) |
| 布隆过滤器 | 快速存在性检测 | O(k)，可能有假阳性 |
| 跳表 | 有序数据快速查找 | O(log n) |
| LRU 缓存 | 热点数据缓存 | O(1) |
| R 树 | 空间索引查询 | O(log n) |

### 哈希表（开放寻址 + Robin Hood）

```c
typedef struct {
    Lv00HashEntry *entries;  /* 条目数组 */
    size_t capacity;
    size_t count;
    size_t tombstones;       /* 墓碑数量 */
    bool thread_safe;
    void *mutex;
} Lv00HashTable;

/* 默认容量 64，最大负载因子 75% */
Lv00HashTable *lv00_hash_create(size_t initial_capacity, bool thread_safe);
void *lv00_hash_insert(Lv00HashTable *ht, uint64_t key, void *value);
void *lv00_hash_find(const Lv00HashTable *ht, uint64_t key);
void *lv00_hash_remove(Lv00HashTable *ht, uint64_t key);
void lv00_hash_foreach(const Lv00HashTable *ht, Lv00HashIterFunc func, void *user_data);
```

### 布隆过滤器

```c
typedef struct {
    uint8_t *bits;           /* 位数组 */
    size_t num_bits;
    size_t num_hashes;       /* 哈希函数数量 */
    size_t count;
} Lv00BloomFilter;

/* 默认误判率 1% */
Lv00BloomFilter *lv00_bloom_create(size_t expected_items, double error_rate);
void lv00_bloom_add(Lv00BloomFilter *bf, const void *data, size_t len);
void lv00_bloom_add_str(Lv00BloomFilter *bf, const char *str);
void lv00_bloom_add_int(Lv00BloomFilter *bf, int64_t value);
bool lv00_bloom_might_contain(const Lv00BloomFilter *bf, const void *data, size_t len);
double lv00_bloom_estimate_fp_rate(const Lv00BloomFilter *bf);
```

### 跳表

```c
typedef struct {
    Lv00SkipNode *header;
    int level;               /* 当前最大层数 */
    size_t count;
    bool thread_safe;
    void *mutex;
} Lv00SkipList;

/* 最大层数 32，概率因子 0.25 */
Lv00SkipList *lv00_skiplist_create(bool thread_safe);
void lv00_skiplist_insert(Lv00SkipList *sl, int64_t key, void *value);
void *lv00_skiplist_find(const Lv00SkipList *sl, int64_t key);
Lv00SkipNode *lv00_skiplist_lower_bound(const Lv00SkipList *sl, int64_t key);
Lv00SkipNode *lv00_skiplist_first(const Lv00SkipList *sl);
Lv00SkipNode *lv00_skiplist_last(const Lv00SkipList *sl);
```

### LRU 缓存

```c
typedef struct {
    Lv00LRUNode **hash_table;  /* 哈希表（O(1) 查找） */
    size_t hash_capacity;
    Lv00LRUNode *head;         /* 最近使用 */
    Lv00LRUNode *tail;         /* 最少使用 */
    size_t capacity;
    size_t count;
    uint64_t hits, misses;     /* 命中统计 */
} Lv00LRUCache;

/* 默认容量 256 */
Lv00LRUCache *lv00_lru_create(size_t capacity, bool thread_safe);
void *lv00_lru_get(Lv00LRUCache *lru, uint64_t key);
void *lv00_lru_put(Lv00LRUCache *lru, uint64_t key, void *value);
double lv00_lru_hit_rate(const Lv00LRUCache *lru);
```

### R 树（空间索引）

```c
typedef struct {
    Lv00RTreeNode *root;
    int max_entries;           /* 每节点最大条目数（默认 16） */
    int min_entries;
    size_t count;
    bool thread_safe;
    void *mutex;
} Lv00RTree;

typedef struct {
    double min_x, min_y;
    double max_x, max_y;
} Lv00BBox2D;

Lv00RTree *lv00_rtree_create(int max_entries, bool thread_safe);
uint64_t lv00_rtree_insert(Lv00RTree *rtree, const Lv00BBox2D *bbox, void *data);
void lv00_rtree_query(const Lv00RTree *rtree, const Lv00BBox2D *bbox,
                      Lv00SpatialQueryFunc callback, void *user_data);
void lv00_rtree_query_point(const Lv00RTree *rtree, double x, double y,
                            Lv00SpatialQueryFunc callback, void *user_data);
void lv00_rtree_nearest(const Lv00RTree *rtree, double x, double y, int k,
                        Lv00SpatialQueryFunc callback, void *user_data);
```

### 辅助函数

```c
double lv00_bbox_area(const Lv00BBox2D *bbox);
Lv00BBox2D lv00_bbox_merge(const Lv00BBox2D *a, const Lv00BBox2D *b);
double lv00_bbox_intersection_area(const Lv00BBox2D *a, const Lv00BBox2D *b);
bool lv00_bbox_intersects(const Lv00BBox2D *a, const Lv00BBox2D *b);
bool lv00_bbox_contains_point(const Lv00BBox2D *bbox, double x, double y);
double lv00_bbox_distance_to_point(const Lv00BBox2D *bbox, double x, double y);
```

### 性能统计

```c
typedef struct {
    uint64_t lookups;          /* 查找次数 */
    uint64_t inserts;          /* 插入次数 */
    uint64_t deletes;          /* 删除次数 */
    uint64_t cache_hits;       /* 缓存命中次数 */
    uint64_t cache_misses;     /* 缓存未命中次数 */
    uint64_t rebalances;       /* 重平衡次数 */
    uint64_t total_time_us;    /* 总耗时（微秒） */
} Lv00IndexStats;

void lv00_index_print_diag(void *stream);
```
