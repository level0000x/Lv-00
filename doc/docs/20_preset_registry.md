# 20 预设函数块注册表（Preset Block Registry）

## 模块概述

预设函数块注册表为 Lv-00 的函数块系统（`func_block.h` / `func_block_preset.h` / `func_block_registry.h`）提供 60+ 个数学领域预设函数块的完整索引与加载机制。所有预设按数学领域分类注册，构成"分类 → 名称 → 元数据 → 模板实例"的完整查找链路。

覆盖头文件：

- `preset_core.h`：预设系统公共宏定义与核心注册入口 `preset_core_register()`。
- `preset_common.h`：公共宏重导出、空指针检查宏 `PRESET_CHECK_NULL`、注册辅助宏 `LV_DECLARE_PRESET_REGISTER` / `LV_PRESET_REGISTER`、安全字符串工具。
- `preset_blocks.h`：预设类型系统（`PresetType` / `PresetExtendedCategory`）、扩展注册表 API、45 个内置预设名称常量宏（中点、垂直平分线、外接圆、仿射变换等）。
- `preset_name_defs.h`：自动生成的全部预设名称宏定义（`PRESET_*`，覆盖代数/分析/拓扑/矩阵/数值/图论/群论/数论/概率/逻辑等数百个）。
- `preset_category.h`：`PresetCategory`（25 类）与 `PresetExtendedCategory`（29 类）中英文名映射的单一事实来源（条目宏）。
- 领域模块：`preset_algebraic.h`（16）、`preset_analysis.h`（49）、`preset_calculus.h`（18）、`preset_matrix.h`（28）、`preset_numerical.h`（24）、`preset_polygons.h`（16）、`preset_topology.h`（49），分别声明各领域的预设数量常量与注册入口（如 `preset_calculus_register()`）。

## 核心设计原则

1. **只读模板 + 实例化副本**：所有预设函数块都是只读模板，实例化时自动创建副本，保证共享模板不被修改。
2. **按数学领域分类**：预设按几何构造、代数、逻辑、分析、数论、群论、环论、域论、拓扑、线性代数、组合数学、复分析、概率统计、图论、微分几何、数值分析、优化理论、数理逻辑等类别组织，便于查找与管理。
3. **单一事实来源**：类别中英文名表只允许由 `preset_category.h` 的条目宏（`LV_PRESET_CATEGORY_ENTRY` / `LV_PRESET_EXTENDED_CATEGORY_ENTRY`）生成，禁止其他文件重复定义；中文名以 UI 查询侧（`func_block_preset_query.c`）为准。
4. **统一注册接口**：旧的分分类注册接口（`preset_blocks_register_construction` / `_algebraic` / `_logic`）统一合并为 `preset_blocks_register_by_category`，并提供带类型签名的 `preset_blocks_register_simple`；领域模块通过 `LV_DECLARE_PRESET_REGISTER` / `LV_PRESET_REGISTER` 宏消除注册样板代码。
5. **名称宏自动化**：`preset_name_defs.h` 自动生成全部 `PRESET_*` 名称宏，源码一律用宏而非裸字符串引用预设名，避免拼写漂移。

## 关键数据结构

```c
/* preset_blocks.h —— 预设输入/输出类型枚举（62 种，截选） */
typedef enum {
    PRESET_TYPE_POINT = 0,        /* 点 */
    PRESET_TYPE_LINE,             /* 直线 */
    PRESET_TYPE_CIRCLE,           /* 圆 */
    PRESET_TYPE_POLYGON,          /* 多边形 */
    PRESET_TYPE_SCALAR,           /* 标量 */
    PRESET_TYPE_VECTOR,           /* 向量 */
    PRESET_TYPE_MATRIX,           /* 矩阵 */
    PRESET_TYPE_BOOLEAN,          /* 布尔值 */
    PRESET_TYPE_POLYNOMIAL,       /* 多项式 */
    /* 代数结构：RING / IDEAL / FIELD / MODULE / ALGEBRA */
    /* 拓扑与分析：TOPOLOGY / MANIFOLD / DISTRIBUTION / GRAPH / TREE / INTEGRAL / SERIES */
    /* 数论：RESIDUE；逻辑：FORMULA / EXPRESSION / STRUCTURE / STRING */
    PRESET_TYPE_ANY,              /* 任意类型（多态） */
    PRESET_TYPE_COUNT             /* 类型总数（哨兵值） */
} PresetType;

/* preset_blocks.h —— 扩展类别枚举（29 类，截选） */
typedef enum {
    PRESET_EXT_BASIC_CONSTRUCTION = 0,  /* 基本几何构造 */
    PRESET_EXT_POLYGON,                 /* 多边形 */
    PRESET_EXT_CIRCLE,                  /* 圆相关构造 */
    PRESET_EXT_TRANSFORMATION_BASIC,    /* 基本变换 */
    PRESET_EXT_LINEAR_ALGEBRA,          /* 线性代数 */
    PRESET_EXT_ANALYSIS_INTEGRAL,       /* 积分 */
    PRESET_EXT_NUMBER_THEORY,           /* 数论 */
    PRESET_EXT_GROUP_THEORY,            /* 群论 */
    /* ... v10.0 补齐：PRESET_EXT_GRAPH_THEORY / NUMERICAL_ANALYSIS /
       OPTIMIZATION_THEORY / MATH_LOGIC */
    PRESET_EXT_CATEGORY_COUNT           /* 类别总数（哨兵值） */
} PresetExtendedCategory;

/* preset_blocks.h —— 预设详细元数据 */
typedef struct {
    const char *name;                    /* 预设名称（唯一键） */
    const char *description;             /* 中文描述 */
    const char *mathematical_definition; /* 数学定义（LaTeX 格式） */
    PresetExtendedCategory category;     /* 扩展类别 */
    int input_count;                     /* 输入端口数量 */
    int output_count;                    /* 输出端口数量 */
    bool has_selector;                   /* 是否需要多解选择器 */
    const char *preconditions;           /* 前置条件描述 */
    const char *example_usage;           /* 使用示例 */
} PresetBlockMetadata;

/* func_block_registry.h —— 基础类别枚举（25 类，由 preset_category.h 提供中英文名） */
typedef enum {
    PRESET_CATEGORY_CONSTRUCTION,   /* 几何构造 */
    PRESET_CATEGORY_ALGEBRAIC,      /* 代数运算 */
    PRESET_CATEGORY_LOGIC,          /* 逻辑推导 */
    PRESET_CATEGORY_ANALYSIS,       /* 数学分析 */
    PRESET_CATEGORY_NUMBER_THEORY,  /* 数论 */
    PRESET_CATEGORY_GROUP_THEORY,   /* 群论 */
    PRESET_CATEGORY_RING_THEORY,    /* 环论 */
    PRESET_CATEGORY_FIELD_THEORY,   /* 域论 */
    PRESET_CATEGORY_TOPOLOGY,       /* 拓扑学 */
    PRESET_CATEGORY_NUMERICAL,      /* 数值分析 */
    /* ... 含 LINEAR_ALGEBRA / COMBINATORICS / COMPLEX_ANALYSIS /
       PROBABILITY / GRAPH_THEORY / DIFFERENTIAL_GEOMETRY / OPTIMIZATION /
       MATH_LOGIC 等 */
    PRESET_CATEGORY_COUNT           /* 类别总数（哨兵值） */
} PresetCategory;

/* func_block_registry.h —— 预设属性位标志 */
typedef enum {
    PRESET_PROPERTY_NONE = 0,
    PRESET_PROPERTY_IDEMPOTENT = 1 << 0,  /* 幂等 */
    PRESET_PROPERTY_INVOLUTIVE = 1 << 1,  /* 对合 */
    PRESET_PROPERTY_COMMUTATIVE = 1 << 2, /* 交换 */
    PRESET_PROPERTY_ASSOCIATIVE = 1 << 3, /* 结合 */
    PRESET_PROPERTY_LINEAR = 1 << 4,      /* 线性 */
    PRESET_PROPERTY_CONTINUOUS = 1 << 5,  /* 连续 */
    PRESET_PROPERTY_DETERMINISTIC = 1 << 6, /* 确定性 */
    PRESET_PROPERTY_CONSTRUCTIVE = 1 << 7,  /* 构造性 */
    PRESET_PROPERTY_REVERSIBLE = 1 << 8     /* 可逆 */
} PresetProperty;
```

## 主要接口

### 注册与生命周期（`preset_core.h` / `preset_common.h` / `preset_blocks.h`）

| 接口 | 说明 |
|---|---|
| `bool preset_core_register(void)` / `preset_common_register(void)` / `preset_calculus_register(void)` | 各层注册入口 |
| `bool preset_blocks_init(void)` | 初始化扩展预设系统（须在 `func_block_registry_init()` 之后） |
| `void lv_preset_blocks_cleanup(void)` | 清理扩展预设系统（别名 `preset_blocks_cleanup`） |
| `bool preset_blocks_register_simple(const char *name, const char *description, PresetCategory category, const PresetType *input_types, int input_count, PresetType output_type, const char *mathematical_definition, const char *complexity, bool is_constructive, bool is_reversible)` | 统一预设注册（v5.0） |
| `bool preset_blocks_register_by_category(const char *name, const char *description, PresetExtendedCategory category, int input_count, int output_count)` | 通用分类注册（旧接口的合并） |

### 查询与统计

| 接口 | 说明 |
|---|---|
| `PresetBlockMetadata *preset_blocks_get_metadata(const char *name)` | 获取元数据副本（调用者负责释放） |
| `int preset_blocks_find_by_category(PresetExtendedCategory category, const char **out_names, int max_count)` | 按扩展类别查找 |
| `int preset_blocks_find_by_prefix(const char *prefix, const char **out_names, int max_count)` | 按名称前缀查找 |
| `int preset_blocks_find_by_keyword(const char *keyword, const char **out_names, int max_count)` | 按描述关键词查找 |
| `int preset_blocks_get_all_names(const char **out_names, int max_count)` | 获取全部预设名称 |
| `const char *preset_extended_category_to_string(PresetExtendedCategory cat)` | 扩展类别 → 中文名 |
| `void preset_blocks_get_stats(int *total_count, int *by_category)` / `preset_blocks_print_stats(void)` | 统计信息（总数与各类别数量） |

### 文档生成与工具

| 接口 | 说明 |
|---|---|
| `char *preset_blocks_generate_documentation(void)` | 生成全部预设的 Markdown 文档 |
| `char *preset_blocks_generate_single_doc(const char *name)` | 生成单个预设的 Markdown 文档 |
| `size_t lv_safe_strncpy(char *dest, const char *src, size_t dest_size)` / `int lv_safe_snprintf(...)` | 安全字符串工具 |
| `int preset_properties_to_string(PresetProperty properties, char *buffer, size_t buffer_size)` / `bool preset_properties_from_string(const char *str, PresetProperty *properties)` | 属性位标志与字符串互转 |
| 宏 `LV_DECLARE_PRESET_REGISTER(category)` / `LV_PRESET_REGISTER(...)` | 注册辅助宏，消除每个预设文件的重复注册结构 |
| 宏 `PRESET_CHECK_NULL(ptr, label)` | goto error 模式的空指针检查 |

### 各领域预设数量常量

| 常量 | 值 | 覆盖领域 |
|---|---|---|
| `CORE_PRESET_COUNT` | 1 | 核心预设 |
| `COMMON_PRESET_COUNT` | 1 | 公共预设 |
| `ALGEBRAIC_PRESET_COUNT` | 16 | 代数运算 |
| `ANALYSIS_PRESET_COUNT` | 49 | 数学分析 |
| `CALCULUS_PRESET_COUNT` | 18 | 微积分 |
| `MATRIX_PRESET_COUNT` | 28 | 矩阵运算 |
| `NUMERICAL_PRESET_COUNT` | 24 | 数值分析 |
| `POLYGONS_PRESET_COUNT` | 16 | 多边形 |
| `TOPOLOGY_PRESET_COUNT` | 49 | 拓扑学 |

## 工作流程

1. **加载**：`func_block_registry_init()` 初始化底层注册表 → `preset_blocks_init()` 初始化扩展系统 → 依次调用 `preset_core_register` / `preset_common_register` / `preset_calculus_register` 与各领域模块注册函数，通过 `preset_blocks_register_simple` 或 `preset_blocks_register_by_category` 写入注册表，全部注册为只读模板。
2. **索引**：注册表内部以名称（唯一键）建索引；`preset_name_defs.h` 的名称宏保证源码引用与注册键一致；类别中英文名经 `preset_category.h` 条目宏统一生成，供查询与 UI 展示。
3. **查询**：`preset_blocks_find_by_prefix` / `find_by_keyword` / `find_by_category` 按前缀、描述关键词、扩展类别三路检索名称列表；`get_metadata` 返回堆分配元数据副本（含 LaTeX 数学定义、前置条件、使用示例）。
4. **实例化**：预设模板经函数块系统实例化为可执行副本（多解预设如 `excircle`、`equilateral_triangle` 依赖 `has_selector` 多解选择器），随后接入 `07_func_block.md` 描述的确定性检查与组合子链路。
5. **文档化**：`preset_blocks_generate_documentation` / `generate_single_doc` 按类别组织输出 Markdown 文档，供 IDE 支持与用户参考；`preset_blocks_get_stats` 提供总数与分类统计用于调试与监控。

## 模块关系

| 本模块组件 | 关联文档/模块 | 关系说明 |
|---|---|---|
| `preset_blocks.h` / `preset_common.h` | [07_func_block.md](07_func_block.md) | 依赖 `func_block.h` / `func_block_preset.h` / `func_block_registry.h`；预设是函数块的只读模板，实例化与确定性机制见 07 |
| `preset_blocks.h`（内置常量） | [01_symbolic_coord.md](01_symbolic_coord.md) | 几何构造类预设（中点、外接圆等）操作 `GeomNode` 符号坐标对象 |
| `preset_category.h` | 本注册表自身 | 类别中英文名表的单一事实来源，供序列化/反序列化与 UI 查询共用 |
| `preset_numerical.h` / `preset_calculus.h` | [19_numerical_backends.md](19_numerical_backends.md) | `ode_rk4` / `ode_euler` / `numerical_gauss_quadrature` 等数值类预设调用数值后端内核 |
| `preset_numerical.h`（ODE 类） | [04_solver.md](04_solver.md) | 常微分方程预设与求解器共享算法骨架（RK4 / AB4 系数） |
| `preset_analysis.h` / `preset_topology.h` | [27_quantifier_logic.md](27_quantifier_logic.md)、[28_number_theory.md](28_number_theory.md) | 量词/数论类预设（`universal_quantifier`、`euler_totient` 等）复用逻辑与数论模块原语 |
| `preset_name_defs.h` | [33_gappa_verification.md](33_gappa_verification.md) | 预设名称宏体系与 Gappa 验证 DSL 的谓词命名保持一致，避免字符串漂移 |

## 版本历史

| 版本 | 日期 | 说明 |
|---|---|---|
| 1.0.0 | 2026-08-10 | 初稿：汇总预设注册表层级结构、类别体系、注册/查询/文档化接口与各领域预设数量 |
