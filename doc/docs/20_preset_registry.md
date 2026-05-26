# 20 预设函数块注册表与领域覆盖

## 1 模块概述

预设函数块系统（Preset Function Block System）是 Lv-00 的标准化数学函数块库，为理论数学研究提供覆盖广泛数学领域的可复用函数块模板。该系统以"注册-查找-实例化"为核心范式：所有内置预设在首次初始化时惰性加载至全局注册表，用户可通过名称查找并实例化为具体的函数块对象，亦可注册自定义预设以扩展系统能力。

预设系统覆盖从基础几何构造到高阶代数拓扑、从初等微积分到泛函分析的 16 大数学领域，共计 63 个预设头文件，构成 Lv-00 第四层（推理层）的核心知识基础。

### 1.1 架构层次

```
preset_core.h          -- 核心接口层（库生命周期、查询、实例化、组合、序列化）
preset_common.h        -- 公共定义层（宏、常量、工具函数、验证函数）
preset_blocks.h        -- 扩展类型系统与统一注册接口
preset_register_macros.h -- 注册宏（消除重复代码模式）
func_block_registry.h  -- 注册表实现（FNV-1a 哈希表、引用计数）
func_block_preset.h    -- 预设参数类型系统与元数据定义
func_block_preset_ops.h -- 高级操作（链式调用、批量实例化、搜索推荐）
func_block_utils.h     -- 内部工具函数
preset_*.h (63 files)  -- 各数学领域预设定义
```

### 1.2 设计原则

- **只读模板**：预设函数块为只读模板，实例化时自动创建深拷贝
- **惰性加载**：内置预设在首次调用 `func_block_registry_init()` 时创建
- **函数式组合**：所有高级操作不修改原始预设，支持链式调用
- **线程安全**：注册表操作受互斥锁保护，计数器使用原子操作
- **引用计数**：`PresetEntry` 包含引用计数字段（v3.4.1），防止悬空指针

---

## 2 核心头文件详解

### 2.1 func_block_preset.h -- 预设系统核心接口

**路径**：`core/include/lv00/func_block_preset.h`

本文件定义预设函数块系统的参数类型体系、元数据结构和实例化 API，是预设系统的类型基础。

#### 2.1.1 PresetParamType 枚举（16 种参数类型）

| 枚举值 | 含义 |
|--------|------|
| `PARAM_TYPE_POINT` | 点 |
| `PARAM_TYPE_LINE` | 直线（无限延伸） |
| `PARAM_TYPE_SEGMENT` | 线段 |
| `PARAM_TYPE_RAY` | 射线 |
| `PARAM_TYPE_CIRCLE` | 圆 |
| `PARAM_TYPE_ARC` | 圆弧 |
| `PARAM_TYPE_POLYGON` | 多边形 |
| `PARAM_TYPE_REGION` | 区域 |
| `PARAM_TYPE_ANGLE` | 角度 |
| `PARAM_TYPE_VECTOR` | 向量 |
| `PARAM_TYPE_SCALAR` | 标量（数值） |
| `PARAM_TYPE_BOOLEAN` | 布尔值 |
| `PARAM_TYPE_CURVE` | 曲线 |
| `PARAM_TYPE_SURFACE` | 曲面 |
| `PARAM_TYPE_ANY` | 任意类型（多态） |
| `PARAM_TYPE_VARIADIC` | 可变参数 |

#### 2.1.2 ParamConstraintType 枚举（8 种约束类型）

| 枚举值 | 含义 |
|--------|------|
| `PARAM_CONSTRAINT_NONE` | 无约束 |
| `PARAM_CONSTRAINT_NON_COLLINEAR` | 非共线 |
| `PARAM_CONSTRAINT_NON_COPLANAR` | 非共面 |
| `PARAM_CONSTRAINT_DISTINCT` | 互不相同 |
| `PARAM_CONSTRAINT_POSITIVE` | 正值 |
| `PARAM_CONSTRAINT_NON_ZERO` | 非零 |
| `PARAM_CONSTRAINT_UNIT` | 单位长度 |
| `PARAM_CONSTRAINT_IN_RANGE` | 在范围内 |

#### 2.1.3 PresetProperty 枚举（9 种位掩码属性）

| 位标志 | 值 | 含义 |
|--------|-----|------|
| `PRESET_PROPERTY_NONE` | 0 | 无特殊性质 |
| `PRESET_PROPERTY_IDEMPOTENT` | 1<<0 | 幂等性：f(f(x)) = f(x) |
| `PRESET_PROPERTY_INVOLUTIVE` | 1<<1 | 对合性：f(f(x)) = x |
| `PRESET_PROPERTY_COMMUTATIVE` | 1<<2 | 交换性：f(a,b) = f(b,a) |
| `PRESET_PROPERTY_ASSOCIATIVE` | 1<<3 | 结合性 |
| `PRESET_PROPERTY_LINEAR` | 1<<4 | 线性 |
| `PRESET_PROPERTY_CONTINUOUS` | 1<<5 | 连续性 |
| `PRESET_PROPERTY_DETERMINISTIC` | 1<<6 | 确定性 |
| `PRESET_PROPERTY_CONSTRUCTIVE` | 1<<7 | 构造性 |
| `PRESET_PROPERTY_REVERSIBLE` | 1<<8 | 可逆性 |

#### 2.1.4 PresetComplexity 枚举（7 种复杂度等级）

| 枚举值 | 含义 |
|--------|------|
| `COMPLEXITY_O1` | 常数时间 |
| `COMPLEXITY_OLOGN` | 对数时间 |
| `COMPLEXITY_ON` | 线性时间 |
| `COMPLEXITY_ONLOGN` | 线性对数 |
| `COMPLEXITY_ON2` | 平方时间 |
| `COMPLEXITY_ON3` | 立方时间 |
| `COMPLEXITY_UNKNOWN` | 未知 |

#### 2.1.5 PresetMetadata 结构

```c
typedef struct {
    const char *name;             /* 预设名称 */
    const char *description;      /* 描述 */
    const char *mathematical_def; /* 数学定义（LaTeX格式） */
    PresetCategory category;      /* 类别 */
    PresetProperty properties;    /* 数学性质位掩码 */
    PresetComplexity complexity;  /* 复杂度 */
    PresetParamDef *input_params; /* 输入参数数组 */
    int input_count;              /* 输入参数数量 */
    PresetParamDef *output_params;/* 输出参数数组 */
    int output_count;             /* 输出参数数量 */
    const char **preconditions;   /* 前置条件描述数组 */
    int precondition_count;       /* 前置条件数量 */
    const char **postconditions;  /* 后置条件描述数组 */
    int postcondition_count;      /* 后置条件数量 */
    const char **related_presets; /* 相关预设名称数组 */
    int related_count;            /* 相关预设数量 */
    int version_major;            /* 主版本 */
    int version_minor;            /* 次版本 */
    int version_patch;            /* 补丁版本 */
} PresetMetadata;
```

#### 2.1.6 实例化 API

| 函数 | 说明 |
|------|------|
| `func_block_preset_library_init()` | 初始化预设库（幂等） |
| `func_block_preset_library_cleanup()` | 清理预设库 |
| `func_block_preset_instantiate()` | 简化版实例化 |
| `func_block_preset_instantiate_ex()` | 完整版实例化（带选项和详情） |
| `func_block_preset_validate_types()` | 验证参数类型 |
| `func_block_preset_validate_constraints()` | 验证参数约束 |
| `func_block_preset_get_metadata()` | 获取预设元数据 |
| `func_block_preset_list()` | 列出可用预设（支持类别筛选） |
| `func_block_preset_exists()` | 检查预设是否存在 |
| `func_block_preset_count()` | 获取已注册预设总数 |
| `func_block_preset_compose()` | 函数复合 f . g |
| `func_block_preset_partial()` | 偏应用（固定部分参数） |
| `func_block_preset_get_inverse()` | 获取逆预设 |
| `func_block_preset_register_custom()` | 注册自定义预设 |
| `func_block_preset_generate_doc()` | 生成 Markdown 文档 |
| `func_block_preset_generate_index()` | 生成文档索引 |

---

### 2.2 func_block_preset_ops.h -- 高级操作接口

**路径**：`core/include/lv00/func_block_preset_ops.h`

本模块提供预设函数块的高级操作能力，所有操作均为函数式（不修改原始预设）。

#### 2.2.1 PresetComposeMode 枚举（5 种组合模式）

| 枚举值 | 含义 |
|--------|------|
| `PRESET_COMPOSE_SEQUENCE` | 顺序组合：f -> g |
| `PRESET_COMPOSE_PARALLEL` | 并行组合：f \| g |
| `PRESET_COMPOSE_FEEDBACK` | 反馈组合：f 的输出反馈到输入 |
| `PRESET_COMPOSE_BRANCH` | 分支组合：条件选择 f 或 g |
| `PRESET_COMPOSE_PIPE` | 管道组合：数据流管道 |

#### 2.2.2 功能分组

**链式调用**（`PresetChain` 不透明结构）：
- `preset_chain_create()` / `preset_chain_destroy()` -- 创建/销毁预设链
- `preset_chain_add()` -- 向链中添加预设（含输入映射）
- `preset_chain_execute()` -- 执行预设链

**批量操作**：
- `preset_batch_instantiate()` -- 批量实例化相同预设
- `preset_batch_apply()` -- 将预设批量应用到节点集合

**参数绑定**：
- `preset_create_bindings()` / `preset_bindings_free()` -- 创建/释放绑定数组
- `preset_partial_bind()` -- 应用部分绑定创建新预设

**验证与测试**：
- `preset_validate()` -- 验证预设有效性（`PresetValidationResult`）
- `preset_test_instantiation()` -- 使用测试参数验证实例化

**搜索与推荐**：
- `preset_search_by_signature()` -- 基于输入输出端口数量搜索
- `preset_recommend_related()` -- 基于类别推荐相关预设

**递归构造**：
- `preset_make_recursive()` -- 创建递归预设（用于迭代构造）

---

### 2.3 func_block_registry.h -- 注册系统

**路径**：`core/include/lv00/func_block_registry.h`

提供预设函数块的全局注册表，使用 FNV-1a 哈希表实现 O(1) 平均查找复杂度。

#### 2.3.1 PresetCategory 枚举（26 种类别）

| 枚举值 | 含义 |
|--------|------|
| `PRESET_CATEGORY_CONSTRUCTION` | 几何构造 |
| `PRESET_CATEGORY_MEASUREMENT` | 度量计算 |
| `PRESET_CATEGORY_TRANSFORMATION` | 几何变换 |
| `PRESET_CATEGORY_ALGEBRAIC` | 代数运算 |
| `PRESET_CATEGORY_LOGIC` | 逻辑推导 |
| `PRESET_CATEGORY_ANALYSIS` | 分析运算 |
| `PRESET_CATEGORY_NUMBER_THEORY` | 数论运算 |
| `PRESET_CATEGORY_GROUP_THEORY` | 群论运算 |
| `PRESET_CATEGORY_RING_THEORY` | 环论运算 |
| `PRESET_CATEGORY_FIELD_THEORY` | 域论运算 |
| `PRESET_CATEGORY_TOPOLOGY` | 拓扑构造 |
| `PRESET_CATEGORY_LINEAR_ALGEBRA` | 线性代数 |
| `PRESET_CATEGORY_COMBINATORICS` | 组合数学 |
| `PRESET_CATEGORY_COMPLEX_ANALYSIS` | 复分析 |
| `PRESET_CATEGORY_PROBABILITY` | 概率统计 |
| `PRESET_CATEGORY_GEOMETRY` | 几何（含三维/高级几何） |
| `PRESET_CATEGORY_ALGEBRA` | 代数（含线性代数/多项式） |
| `PRESET_CATEGORY_CATEGORY_THEORY` | 范畴论 |
| `PRESET_CATEGORY_SET_THEORY` | 集合论 |
| `PRESET_CATEGORY_CUSTOM` | 自定义/扩展类别 |
| `PRESET_CATEGORY_GRAPH_THEORY` | 图论 |
| `PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY` | 微分几何 |
| `PRESET_CATEGORY_NUMERICAL` | 数值分析 |
| `PRESET_CATEGORY_OPTIMIZATION` | 优化理论 |
| `PRESET_CATEGORY_MATH_LOGIC` | 数理逻辑 |
| `PRESET_CATEGORY_COUNT` | 类别总数（哨兵值） |

#### 2.3.2 PresetEntry 结构

```c
typedef struct {
    char *name;              /* 预设名称（唯一键） */
    char *description;       /* 描述 */
    PresetCategory category; /* 类别 */
    FuncBlock *template_fb;  /* 模板函数块（只读） */
    int ref_count;           /* 引用计数 (v3.4.1) */
} PresetEntry;
```

#### 2.3.3 FuncBlockRegistry 结构

```c
typedef struct {
    PresetEntry *entries; /* 条目数组 */
    int count;            /* 当前条目数 */
    int capacity;         /* 数组容量 */
    bool initialized;     /* 是否已初始化内置预设 */
} FuncBlockRegistry;
```

#### 2.3.4 核心 API

| 函数 | 说明 |
|------|------|
| `func_block_registry_init()` | 初始化注册表并惰性加载内置预设 |
| `func_block_registry_cleanup()` | 清理注册表 |
| `func_block_register()` | 注册自定义预设 |
| `func_block_registry_lookup()` | 按名称查找并返回深拷贝 |
| `func_block_registry_find()` | 按名称查找条目（不创建副本） |
| `func_block_registry_find_by_category()` | 按类别查找 |
| `func_block_registry_get_count()` | 获取条目总数 |
| `func_block_registry_unregister()` | 注销指定预设 |
| `preset_category_to_string()` | 类别枚举转中文字符串 |
| `preset_category_from_string()` | 字符串解析为类别枚举 |

---

### 2.4 func_block_utils.h -- 内部工具函数

**路径**：`core/include/lv00/func_block_utils.h`

提供函数块系统内部使用的整数数组操作工具函数。

| 函数 | 说明 |
|------|------|
| `is_id_in_array(id, array, count)` | 线性扫描检查 ID 是否存在于数组中 |
| `dup_int_array(src, count)` | 深拷贝整数数组（分配新内存） |
| `lv00_int_array_merge(a, a_count, b, b_count, out_count)` | 合并两个整数数组（a 在前，b 在后） |

向后兼容别名：`merge_int_arrays` -> `lv00_int_array_merge`。

---

### 2.5 preset_core.h -- 核心接口层

**路径**：`core/include/lv00/preset_core.h`

预设系统的主头文件，定义核心数据结构和完整接口（v5.0.0）。

#### 2.5.1 不透明句柄类型

| 类型 | 说明 |
|------|------|
| `PresetLibraryHandle` | 预设库句柄 |
| `PresetEntryHandle` | 预设条目句柄 |
| `PresetInstanceHandle` | 预设实例句柄 |

#### 2.5.2 核心数据结构

- **PresetVersion**：版本信息（major, minor, patch, build_info）
- **PresetStatistics**：统计信息（总数、内置数、自定义数、活跃数、各类别数量）
- **PresetQueryCriteria**：高级查询条件（名称模式、类别、属性、复杂度、端口数量等）
- **PresetQueryResult**：查询结果（名称数组、结果数、总匹配数）
- **PresetInstantiateOptions**：实例化选项（类型验证、约束验证、自动连接、缓存、超时）
- **PresetExecutionContext**：执行上下文（用户数据、进度回调、取消回调、精度要求）
- **PresetComposition**：组合描述（预设名称数组、组合模式、参数映射）

#### 2.5.3 功能分组

| 分组 | 主要函数 |
|------|----------|
| 库生命周期 | `preset_library_init`, `preset_library_shutdown`, `preset_library_is_initialized`, `preset_library_get_version`, `preset_library_get_statistics`, `preset_library_reset` |
| 注册与注销 | `preset_register_builtin`, `preset_register_custom`, `preset_unregister`, `preset_register_batch` |
| 查询与检索 | `preset_find`, `preset_get_metadata`, `preset_query`, `preset_list_by_category`, `preset_list_all`, `preset_exists`, `preset_is_builtin` |
| 实例化 | `preset_instantiate`, `preset_instantiate_batch`, `preset_instance_destroy`, `preset_instance_get_func_block`, `preset_instance_get_outputs` |
| 执行与验证 | `preset_instance_execute`, `preset_instance_validate` |
| 组合与绑定 | `preset_compose`, `preset_bind_parameter` |
| 文档生成 | `preset_generate_documentation`, `preset_generate_library_documentation`, `preset_get_usage_example` |
| 序列化 | `preset_serialize`, `preset_deserialize`, `preset_export_to_file`, `preset_import_from_file` |
| 错误处理 | `preset_get_last_error`, `preset_clear_error`, `preset_set_error_callback`, `preset_release` |

---

### 2.6 preset_common.h -- 公共定义与工具

**路径**：`core/include/lv00/preset_common.h`

所有预设模块共享的公共类型定义、常量、宏和工具函数。

#### 2.6.1 版本与容量常量

| 宏 | 值 | 说明 |
|----|-----|------|
| `PRESET_SYSTEM_VERSION_MAJOR` | 5 | 主版本号 |
| `PRESET_SYSTEM_VERSION_MINOR` | 0 | 次版本号 |
| `PRESET_SYSTEM_VERSION_PATCH` | 0 | 修订版本号 |
| `PRESET_MAX_COUNT` | 1024 | 最大预设数量 |
| `PRESET_MAX_PARAMS` | 32 | 最大参数数量 |
| `PRESET_MAX_INPUTS` | 16 | 最大输入数量 |
| `PRESET_MAX_OUTPUTS` | 8 | 最大输出数量 |
| `PRESET_BUFFER_SIZE` | 8192 | 字符串缓冲区大小 |
| `PRESET_MAX_NAME_LENGTH` | 256 | 名称最大长度 |
| `PRESET_MAX_DESC_LENGTH` | 1024 | 描述最大长度 |
| `PRESET_ID_OFFSET` | 60000 | 预设 ID 起始偏移 |

#### 2.6.2 宏分类

- **线程安全计数器**：`PresetAtomicCounter`、`PRESET_ATOMIC_INC/DEC/READ`（Windows 使用 `InterlockedIncrement`，其他平台使用 `_Atomic`）
- **安全字符串操作**：`PRESET_SAFE_STRCPY`、`PRESET_SAFE_STRCAT`、`PRESET_SAFE_SNPRINTF`
- **内存管理**：`PRESET_SAFE_MALLOC`、`PRESET_SAFE_CALLOC`、`PRESET_SAFE_REALLOC`、`PRESET_SAFE_FREE`
- **错误处理**：`PRESET_CHECK`、`PRESET_CHECK_NULL`、`PRESET_CHECK_STRING`、`PRESET_CHECK_INDEX`、`PRESET_CHECK_RANGE`
- **预设注册**：`PRESET_REGISTER`、`PRESET_REGISTER_EX`、`PRESET_REGISTER_CAT`、`PRESET_REGISTER_CAT_COUNTED`、`PRESET_REGISTER_BEGIN/END`
- **元数据定义**：`PRESET_METADATA_DEFINE`
- **调试日志**：`PRESET_DEBUG_LOG`、`PRESET_TRACE_LOG`、`PRESET_ERROR_LOG`、`PRESET_WARN_LOG`、`PRESET_INFO_LOG`
- **模块模板**：`LV00_DEFINE_PRESET_REGISTER_WRAPPER`、`LV00_DEFINE_PRESET_MODULE`

#### 2.6.3 工具函数

**验证函数**：
- `preset_validate_name()` / `preset_validate_description()` -- 名称和描述格式验证
- `preset_validate_type_combination()` -- 类型组合有效性验证
- `preset_validate_input_count()` / `preset_validate_output_count()` -- 参数数量边界检查
- `preset_validate_input_types()` / `preset_validate_output_type()` -- 类型数组验证
- `preset_validate_metadata()` -- 元数据完整性验证
- `preset_validate_math_definition()` -- LaTeX 数学定义格式验证
- `preset_validate_complexity()` -- 复杂度描述格式验证

**类型分类函数**：
- `preset_type_is_basic()` / `preset_type_is_algebraic()` / `preset_type_is_analytic()` / `preset_type_is_topological()` -- 类型领域归属判断
- `preset_type_get_domain()` -- 获取类型的类别归属字符串
- `preset_types_compatible()` -- 类型兼容性检查

**工具函数**：
- `lv00_safe_strncpy()` / `lv00_safe_strncat()` / `lv00_safe_snprintf()` -- 安全字符串操作
- `lv00_hash_int_array()` / `lv00_int_arrays_equal()` / `lv00_dup_int_array()` -- 整数数组工具
- `preset_compute_signature_hash()` -- 预设签名哈希
- `preset_module_get_names()` -- 通用名称列表获取

---

### 2.7 preset_blocks.h -- 扩展类型系统与注册接口

**路径**：`core/include/lv00/preset_blocks.h`

扩展基础函数块注册系统，提供统一的预设类型系统和注册接口。

#### 2.7.1 PresetType 枚举（扩展类型系统）

涵盖几何、代数、逻辑、分析、拓扑、数论、度量等领域的 60 余种类型，包括：

- **几何类型**：POINT, LINE, LINE_SEGMENT, RAY, CIRCLE, POLYGON, ANGLE, REGION, PATH, SURFACE, SPACE
- **代数类型**：SCALAR, VECTOR, MATRIX, INTEGER, POLYNOMIAL, SET, FUNCTION, TUPLE, LIST, SEQUENCE
- **代数结构类型**：GROUP, GROUP_ELEMENT, SUBGROUP, HOMOMORPHISM, RING, IDEAL, FIELD, MODULE, ALGEBRA, COSET, EXTENSION, AUTOMORPHISM, PERMUTATION
- **分析类型**：LIMIT, DERIVATIVE, INTEGRAL, SERIES, COMPLEX, LIMIT_EXPRESSION, EQUATION
- **拓扑类型**：TOPOLOGY, MANIFOLD, OPEN_SET, CLOSED_SET
- **逻辑类型**：BOOLEAN, FORMULA, EXPRESSION, STRUCTURE, STRING
- **概率统计类型**：DISTRIBUTION, PROBABILITY
- **图论类型**：GRAPH, TREE
- **数论类型**：PRIME, RESIDUE
- **度量类型**：DISTANCE, AREA, LENGTH, CURVATURE
- **通用类型**：ANY, COUNT（哨兵值）

#### 2.7.2 PresetExtendedCategory 枚举

在 `PresetCategory` 基础上提供更细粒度的扩展分类，共 27 种（含哨兵值），涵盖基本构造、高级构造、多边形、圆、基本变换、高级变换、度量、三角函数、坐标运算、基础代数、高级代数、线性代数、多项式、命题逻辑、谓词逻辑、证明策略、极限、微分、积分、拓扑、微分几何、数论、群论、分析学、组合数学、图论、数值分析、优化理论、数理逻辑等。

#### 2.7.3 统一注册接口

```c
bool preset_blocks_register_simple(
    const char *name, const char *description,
    PresetCategory category,
    const PresetType *input_types, int input_count,
    PresetType output_type,
    const char *mathematical_definition,
    const char *complexity,
    bool is_constructive, bool is_reversible);
```

所有预设模块统一使用此接口进行注册。

#### 2.7.4 内置预设常量

定义了 30 余个常用预设的名称常量，包括：
- **基础几何构造**：midpoint, perpendicular_bisector, angle_bisector, parallel_line, perpendicular_line, line_intersection, reflection
- **圆相关**：circle_by_center_radius, circle_by_three_points, tangent_line, circumcircle, incircle, excircle
- **多边形**：equilateral_triangle, square, regular_polygon, triangle_centroid, triangle_orthocenter, triangle_circumcenter, triangle_incenter
- **几何变换**：translation, rotation, homothety, reflection_transform, affine_transform, inversion
- **度量计算**：distance, angle_measure, area, perimeter
- **三角函数**：sine, cosine, tangent, arctangent
- **代数运算**：vector_add, vector_scale, vector_dot, vector_cross, polynomial_roots, linear_solve
- **逻辑推导**：contradiction_detector, implication_chain, equivalence, universal_quantifier, existential_quantifier

---

### 2.8 preset_register_macros.h -- 注册宏

**路径**：`core/include/lv00/preset_register_macros.h`

提供统一的宏定义以简化预设注册流程，消除重复代码模式。

| 宏 | 说明 |
|----|------|
| `LV00_PRESET_REGISTER_BEGIN(category, category_name)` | 开始预设注册块 |
| `LV00_PRESET_ENTRY(name, display_name, desc, cat)` | 注册单个预设条目 |
| `LV00_PRESET_REGISTER_END()` | 结束预设注册块 |

使用示例：
```c
LV00_PRESET_REGISTER_BEGIN(PRESET_CATEGORY_CONSTRUCTION, "几何构造")
LV00_PRESET_ENTRY("midpoint", "中点", "构造两点A,B的中点", PRESET_CATEGORY_CONSTRUCTION)
LV00_PRESET_ENTRY("circumcircle", "外接圆", "过三角形三顶点的圆", PRESET_CATEGORY_CONSTRUCTION)
LV00_PRESET_REGISTER_END()
```

---

## 3 领域覆盖总表

以下表格列出全部 63 个预设头文件及其覆盖的数学领域。文件路径统一省略前缀 `core/include/lv00/` 和 `core/src/layer4_reasoning/preset/`。

### 3.1 几何（7 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 1 | `preset_basic_geometry.h` | 基础几何：点、线、圆的基本构造与性质 |
| 2 | `preset_advanced_geometry.h` | 高级几何：高级几何构造与定理 |
| 3 | `preset_geometry_3d.h` | 三维几何：空间中的点、线、面、体 |
| 4 | `preset_polygons.h` | 多边形：正多边形、三角形特殊点 |
| 5 | `preset_transformations.h` | 几何变换：平移、旋转、反射、位似、仿射、反演 |
| 6 | `preset_measurements.h` | 度量：距离、角度、面积、周长 |
| 7 | `preset_trigonometry.h` | 三角学：三角函数与反三角函数 |

### 3.2 代数（7 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 8 | `preset_algebraic.h` | 一般代数：代数运算与代数结构 |
| 9 | `preset_polynomial.h` | 多项式：多项式运算、求根、因式分解 |
| 10 | `preset_matrix.h` | 矩阵：矩阵运算、行列式、特征值 |
| 11 | `preset_linear_algebra.h` | 线性代数：向量空间、线性映射、线性方程组 |
| 12 | `preset_group_theory.h` | 群论：群、子群、同态、正规子群 |
| 13 | `preset_ring_theory.h` | 环论：环、理想、商环、多项式环 |
| 14 | `preset_field_theory.h` | 域论：域扩张、伽罗瓦理论、有限域 |

### 3.3 分析（8 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 15 | `preset_analysis.h` | 数学分析：极限、连续、收敛 |
| 16 | `preset_complex_analysis.h` | 复分析：解析函数、柯西积分、留数 |
| 17 | `preset_special_functions.h` | 特殊函数：Gamma、Beta、Bessel、超几何 |
| 18 | `preset_integral_transforms.h` | 积分变换：Fourier、Laplace、Z 变换 |
| 19 | `preset_differential_equations.h` | 微分方程：ODE、PDE、边值问题 |
| 20 | `preset_difference_equations.h` | 差分方程：递推关系、离散动力系统 |
| 21 | `preset_numerical.h` | 数值方法：数值逼近、插值、拟合 |
| 22 | `preset_numerical_analysis.h` | 数值分析：误差分析、稳定性、收敛性 |

### 3.4 拓扑与几何（7 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 23 | `preset_topology.h` | 拓扑学：拓扑空间、连续映射、紧致性 |
| 24 | `preset_algebraic_topology.h` | 代数拓扑：基本群、同调群、上同调 |
| 25 | `preset_algebraic_topology_adv.h` | 高等代数拓扑：谱序列、K 理论 |
| 26 | `preset_differential_geometry.h` | 微分几何：流形、联络、曲率 |
| 27 | `preset_differential_geometry_adv.h` | 高等微分几何：黎曼几何、度量张量 |
| 28 | `preset_algebraic_geometry.h` | 代数几何：概形、层、簇 |
| 29 | `preset_arithmetic_geometry.h` | 算术几何：Diophantine 方程、椭圆曲线 |

### 3.5 逻辑与集合（4 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 30 | `preset_set_theory.h` | 集合论：集合运算、势、序数、基数 |
| 31 | `preset_math_logic.h` | 数理逻辑（基础）：命题逻辑、一阶逻辑 |
| 32 | `preset_mathematical_logic.h` | 数理逻辑（系统）：形式系统、可判定性 |
| 33 | `preset_logic_advanced.h` | 高级逻辑：模态逻辑、直觉主义逻辑 |

### 3.6 数论（1 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 34 | `preset_number_theory.h` | 数论：整除性、素数、同余、二次剩余 |

### 3.7 概率统计（4 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 35 | `preset_probability.h` | 概率论：概率空间、随机变量、期望 |
| 36 | `preset_probability_statistics.h` | 概率统计：分布、估计、假设检验 |
| 37 | `preset_statistics.h` | 统计学：抽样、回归、方差分析 |
| 38 | `preset_stochastic_processes.h` | 随机过程：Markov 链、Brown 运动、鞅 |

### 3.8 信息与编码（2 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 39 | `preset_information_theory.h` | 信息论：熵、互信息、信道容量 |
| 40 | `preset_coding_theory.h` | 编码理论：线性码、循环码、纠错码 |

### 3.9 组合与图论（2 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 41 | `preset_combinatorics.h` | 组合数学：排列组合、生成函数、Pólya 计数 |
| 42 | `preset_graph_theory.h` | 图论：图、树、网络流、着色 |

### 3.10 范畴与表示（3 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 43 | `preset_category_theory.h` | 范畴论：范畴、函子、自然变换 |
| 44 | `preset_category_theory_adv.h` | 高等范畴论：伴随函子、极限、余极限 |
| 45 | `preset_representation_theory.h` | 表示论：群表示、特征标、不可约表示 |

### 3.11 同调与泛函（5 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 46 | `preset_homological_algebra.h` | 同调代数：链复形、同调、导出函子 |
| 47 | `preset_functional_analysis.h` | 泛函分析：Banach 空间、Hilbert 空间 |
| 48 | `preset_functional_analysis_adv.h` | 高等泛函分析：算子理论、谱理论 |
| 49 | `preset_measure_theory.h` | 测度论：测度、积分、Radon-Nikodym |
| 50 | `preset_lie_theory_advanced.h` | 高等 Lie 理论：Lie 代数、表示、分类 |

### 3.12 优化与动力（3 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 51 | `preset_optimization.h` | 优化理论：线性规划、凸优化、对偶 |
| 52 | `preset_dynamical_systems.h` | 动力系统：相空间、稳定性、分岔 |
| 53 | `preset_game_theory.h` | 博弈论：Nash 均衡、合作博弈、演化博弈 |

### 3.13 序与格（2 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 54 | `preset_lattice_theory.h` | 格论：格、模格、分配格、Boole 代数 |
| 55 | `preset_order_theory.h` | 序理论：偏序集、格、完备格 |

### 3.14 数学物理（1 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 56 | `preset_mathematical_physics.h` | 数学物理：经典力学、量子力学数学基础 |

### 3.15 基础数学（5 个文件）

| 序号 | 头文件 | 覆盖领域 |
|------|--------|----------|
| 57 | `preset_basic_math.h` | 基础数学：算术运算、基本函数 |
| 58 | `preset_blocks.h` | 预设块系统：扩展类型系统与统一注册接口 |
| 59 | `preset_common.h` | 公共定义：宏、常量、工具函数 |
| 60 | `preset_calculus.h` | 微积分：极限、微分、积分、级数展开 |
| 61 | `preset_manager.c` | 预设管理器实现：组合、注册、生命周期管理 |

### 3.16 其他（2 个文件）

| 序号 | 头文件 | 说明 |
|------|--------|------|
| 62 | `preset_advanced_geometry.h` | 已归入几何类（3.1 第 2 项），此处为跨目录副本（`src/layer4_reasoning/preset/`） |
| 63 | `preset_geometry_3d.h` | 已归入几何类（3.1 第 3 项），此处为跨目录副本（`src/layer4_reasoning/preset/`） |

> **注**：`preset_advanced_geometry.h` 和 `preset_geometry_3d.h` 同时存在于 `core/include/lv00/` 和 `core/src/layer4_reasoning/preset/` 两个目录中，后者为源码目录中的副本。

---

## 4 实现文件说明

### 4.1 preset_calculus.h

微积分预设函数块头文件，提供理论数学研究中常用的微积分运算预设，包括：

- **极限运算**：数列极限、函数极限、左/右极限、无穷极限、不定式极限
- **微分运算**：导数定义、幂函数导数、链式法则、乘积法则、商法则、隐函数求导、参数方程求导、偏导数
- **积分运算**：不定积分、定积分、换元积分法、分部积分法、部分分式积分、三角积分、反常积分、曲线积分
- **级数展开**：Taylor 级数、Maclaurin 级数、Fourier 级数、幂级数
- **多元微积分**：梯度、散度、旋度、Laplace 算子

所属类别：`PRESET_CATEGORY_ANALYSIS`。

### 4.2 preset_manager.c

预设管理器的核心实现文件（路径：`core/src/layer4_reasoning/func_block/preset_manager.c`），负责：

- 预设的组合操作实现（`preset_compose`）
- 预设的注册与生命周期管理
- 预设库的全局状态维护

### 4.3 preset_register_helper.h

预设注册辅助头文件（路径：`core/src/layer4_reasoning/preset/preset_register_helper.h`），为源码目录中的预设模块提供注册辅助功能。

---

## 5 统计摘要

| 指标 | 数值 |
|------|------|
| 预设头文件总数 | 63 |
| 覆盖数学领域 | 16 大类 |
| PresetCategory 枚举值 | 26 种（含哨兵值） |
| PresetParamType 枚举值 | 16 种 |
| ParamConstraintType 枚举值 | 8 种 |
| PresetProperty 位标志 | 9 种 |
| PresetComplexity 等级 | 7 种 |
| PresetComposeMode 模式 | 5 种 |
| PresetType 类型 | 60 余种 |
| PresetExtendedCategory 扩展类别 | 27 种（含哨兵值） |
| 预设系统版本 | 5.0.0 |
| 最大预设容量 | 1024 |
| 查找算法 | FNV-1a 哈希表（O(1) 平均） |
