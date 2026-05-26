# 高级几何分析模块 (Advanced Geometry Analysis Modules)

## 模块概述

高级几何分析模块扩展了基础约束图之外的几何分析能力，覆盖几何类型定义、几何配置、几何工具、变换推理、数据压缩、高维表示、交互几何、演化引擎、事件检测、不变量类型、构造规约和拓扑操作等多个维度。对应头文件包括 `geometry_types.h`、`geometry_config.h`、`geo_utils.h`、`geometry_transform.h`、`geometry_compress.h`、`high_dim.h`、`interactive_geo.h`、`geom_evol.h`、`geo_event_detect.h`、`geo_invariant_type.h`、`geo_spec.h` 与 `geo_topology.h`。这些模块共同构成 Lv-00 几何系统的上层能力层，为用户提供从静态几何分析到动态几何演化、从精确符号计算到交互式探索的完整工具集。

## 核心设计原则

1. **符号精确优先**：变换推理和不变量计算在符号层面精确执行，数值近似仅用于显示和演化
2. **借鉴成熟系统**：各模块分别借鉴 Cinderella、Dr. Geo、SUNDIALS、TLA+、GUDHI 等成熟系统的设计
3. **可扩展性**：通过注册机制和回调接口支持用户自定义扩展
4. **信任着色**：几何不变量携带信任颜色，反映计算的可信度
5. **连续性保证**：交互几何系统通过投影几何方法确保拖拽操作的连续性

## 1. geometry_types.h —— 几何类型定义

### 设计借鉴

借鉴 SymPy Geometry 的 GeometryEntity 继承层次和 clifford 的 flat array 多向量存储策略，提供清晰的几何实体类型层次和紧凑的 SIMD 友好存储。

### 几何实体类型层次

```
GeomEntity (基类)
  ├── PointEntity        (0 维)
  ├── LinearEntity       (1 维)
  │     ├── LineEntity
  │     ├── SegmentEntity
  │     └── RayEntity
  ├── CircularEntity     (1 维曲线)
  │     └── CircleEntity
  └── RegionEntity       (2 维区域)
        ├── PolygonEntity
        └── TriangleEntity
```

### 几何实体基类

```c
typedef struct GeomEntity {
    GeomEntityKind kind; /**< 实体类型 */
    int dimension;       /**< 维度（0/1/2） */
    TrustColor trust;    /**< 信任颜色 */
    struct {
        double x_min, y_min, x_max, y_max;
    } bounding_box;      /**< 轴对齐包围盒 */
    int graph_node_id;   /**< 关联的约束图节点 ID */
    char *name;
} GeomEntity;
```

### 点实体

```c
typedef struct PointEntity {
    GeomEntity base;
    SymbolicCoord *x, *y;    /**< 二维便捷访问 */
    int dimension;            /**< 坐标维度 */
    SymbolicCoord **coords;   /**< 通用坐标数组 */
} PointEntity;
```

字段冗余设计：`x/y` 向后兼容早期 API，`coords` 支持任意维度。修改任一端必须同步更新另一端。

### 三角形实体

```c
typedef struct TriangleEntity {
    PolygonEntity base;
    PointEntity *_centroid;      /**< 重心缓存 */
    PointEntity *_circumcenter;  /**< 外心缓存 */
    PointEntity *_incenter;      /**< 内心缓存 */
    PointEntity *_orthocenter;   /**< 垂心缓存 */
} TriangleEntity;
```

### 扁平数组存储

借鉴 clifford 的连续内存布局，将几何实体的所有数值分量存储在连续的 `double` 数组中，便于 SIMD 批处理：

```c
typedef struct FlatStorage {
    double *components;  /**< 扁平分量数组 */
    int component_count;
    bool *is_symbolic;   /**< 符号标记 */
    int alignment;       /**< 内存对齐（32/64 字节） */
} FlatStorage;
```

### CSG 操作符

借鉴 OpenSCAD 的 CSG 操作树，支持布尔并集、差集、交集、线性/旋转拉伸、凸包和 Minkowski 和等构造实体几何操作。

## 2. geometry_transform.h —— 几何变换推理系统

### 设计借鉴

提供旋转、轴对称、平移三种基本几何变换的符号推理，支持变换矩阵的符号计算、约束保持性验证、变换序列复合和不动点分析。

### 变换类型

```c
typedef enum {
    TRANSFORM_IDENTITY,     /**< 恒等变换 */
    TRANSFORM_TRANSLATION,  /**< 平移 */
    TRANSFORM_ROTATION,     /**< 旋转 */
    TRANSFORM_REFLECTION,   /**< 轴对称 */
    TRANSFORM_SCALING,      /**< 缩放 */
    TRANSFORM_GLUING,       /**< 粘合（复合变换） */
    TRANSFORM_INVERSION     /**< 反演 */
} Lv00TransformType;
```

### 变换矩阵

使用 GMP 有理数的 3x3 齐次坐标矩阵表示仿射变换：

```c
typedef struct {
    mpq_t a, b, tx;     /**< 第一行 */
    mpq_t c, d, ty;     /**< 第二行 */
} Lv00TransformMatrix;
```

### 变换参数

每种变换类型有独立的参数结构：

- **平移**：`(dx, dy)` 有理数位移向量
- **旋转**：旋转中心 `(cx, cy)` + 角度 `numerator * pi / denominator`（特殊角度标记优化）
- **轴对称**：轴上两点 `(ax, ay), (bx, by)` 或直线方程 `ax + by + c = 0`
- **缩放**：缩放中心 `(cx, cy)` + 缩放因子
- **反演**：反演中心 `(cx, cy)` + 反演圆半径

### 变换性质分析

| 性质 | 函数 | 说明 |
|------|------|------|
| 等距性 | `lv00_transform_is_isometry()` | 是否保持距离不变 |
| 保向性 | `lv00_transform_is_orientation_preserving()` | 是否保持方向 |
| 不动点 | `lv00_transform_find_fixed_point()` | 求解 T(x) = x |
| 阶数 | `lv00_transform_order()` | 使 T^n = I 的最小正整数 n |
| 逆变换 | `lv00_transform_inverse()` | 计算逆变换 |
| 复合 | `lv00_transform_compose()` | 计算 t2 ∘ t1 |

### 变换群

```c
typedef struct Lv00TransformGroup {
    Lv00Transform **generators;  /**< 生成元数组 */
    uint32_t generator_count;
    char *group_name;            /**< 群名称（如 "D4", "C3"） */
    uint32_t order;              /**< 群阶（有限群）或 0（无限群） */
    bool is_abelian;             /**< 是否为阿贝尔群 */
} Lv00TransformGroup;
```

支持常见变换群的预设创建（`lv00_transform_group_create_preset()`），以及群元素枚举和成员判定。

### 约束保持性验证

```c
typedef enum {
    CONSTRAINT_PRESERVED,       /**< 约束完全保持 */
    CONSTRAINT_TRANSFORMED,     /**< 约束被变换为另一约束 */
    CONSTRAINT_BROKEN,          /**< 约束被破坏 */
    CONSTRAINT_CHECK_FAILED     /**< 检查失败 */
} Lv00ConstraintPreservation;
```

### 对称性识别

`lv00_transform_identify_symmetries()` 分析约束图所描述的几何图形，识别其所有对称变换。

## 3. geometry_compress.h —— Draco 风格几何数据压缩

### 设计借鉴

借鉴 Google Draco 的几何压缩管线，将约束图中的几何节点坐标和拓扑关系进行高压缩比编码。

### Edgebreaker CLERS 拓扑编码

Edgebreaker 算法（J. Rossignac, 1999）将三角网格的边遍历过程编码为五种符号：

```c
typedef enum {
    EDGEBREAKER_C = 0, /**< 新顶点闭合三角形 */
    EDGEBREAKER_L = 1, /**< 左边界顶点 */
    EDGEBREAKER_E = 2, /**< 边结束，不生成新三角形 */
    EDGEBREAKER_R = 3, /**< 右边界顶点 */
    EDGEBREAKER_S = 4  /**< 分割操作 */
} EdgebreakerMode;
```

### 预测编码

```c
typedef enum {
    PREDICT_NONE = 0,                /**< 无预测 */
    PREDICT_PARALLELOGRAM = 1,       /**< 平行四边形预测 */
    PREDICT_MULTI_PARALLELOGRAM = 2, /**< 多阶平行四边形预测 */
    PREDICT_DELTA = 3                /**< 差分预测 */
} PredictionMode;
```

平行四边形预测公式：`prediction = v0 + v1 - v_opposite`，仅编码残差 `residual = v2 - prediction`。

### 熵编码

```c
typedef enum {
    ENTROPY_RANS = 0,       /**< rANS 非对称数字系统 */
    ENTROPY_ARITHMETIC = 1, /**< 算术编码 */
    ENTROPY_HUFFMAN = 2     /**< Huffman 前缀树编码 */
} EntropyCoding;
```

### 压缩管线

完整压缩管线：

```
约束图 → 坐标提取 → 预测编码 → Edgebreaker 拓扑编码 → 熵编码 → 二进制 buffer
```

### .lvzd 二进制格式

```
[magic:4B "LVZD"] [version_major:2B] [version_minor:2B]
[original_size:8B] [compressed_size:8B]
[compressed_data:compressed_size bytes]
```

所有多字节整数使用小端序。通过 `compress_write_lvzd()` 和 `compress_read_lvzd()` 进行文件 I/O。

## 4. high_dim.h —— 高维结构表示与交互

### 设计概述

实现四维及以上数学对象的表示和投影机制。高维对象通过端口抽象块承载，二维矩形编码仅为投影视图之一。

### 常量定义

```c
#define HIGH_DIM_MAX_DIMENSIONS 16          /**< 最大维度数 */
#define HIGH_DIM_MAX_PROJECTION_PRESETS 8   /**< 最大投影预设数量 */
#define HIGH_DIM_MAX_DEPTH 32               /**< 语义缩放最大透视深度 */
```

### 维度映射

```c
typedef enum {
    HIGH_DIM_MAP_TO_X = 0, /**< 映射到 X 轴 */
    HIGH_DIM_MAP_TO_Y,     /**< 映射到 Y 轴 */
    HIGH_DIM_MAP_FOLD,     /**< 折叠（不显示） */
    HIGH_DIM_MAP_DISCARD   /**< 丢弃 */
} HighDimMappingType;
```

### 投影预设

```c
typedef struct HighDimProjectionPreset {
    char name[64];
    int dimension_count;
    int mapping_count;
    HighDimAxisMapping mappings[HIGH_DIM_MAX_DIMENSIONS];
    HighDimTransform2D transform;  /**< 2D 变换矩阵 */
    bool is_default;
} HighDimProjectionPreset;
```

### 4D 到 3D 投影

支持四种投影模式：

| 模式 | 说明 |
|------|------|
| 透视投影 (0) | 以第 4 维作为深度，远小近大 |
| 正交投影 (1) | 保留选定维度，高维加权折叠 |
| 旋转投影 (2) | SO(4) 旋转后正交投影到 3D |
| 立体投影 (3) | S^3 到 R^3 的球极投影 |

### 保真度计算（五层度量）

综合五层加权度量计算投影保真度：

| 层次 | 度量 | 权重（5D 以下） |
|------|------|-----------------|
| 第一层 | 维度可见性比例 | 0.20 |
| 第二层 | 约束类型敏感度加权保留率 | 0.45 |
| 第三层 | 几何失真（角度+面积） | 0.20 |
| 第四层 | 拓扑保持 | 0.15 |
| 第五层 | MDS Stress（仅 5D+） | 0.15 |

### 语义缩放

支持进入高维块内部透视（`high_dim_enter_block_perspective()`），维护深度栈（最大 32 层），以及直接跳转到指定缩放层级。

### 多投影视图

支持为同一高维块创建多个不同投影视图，视图间联动高亮。

## 5. interactive_geo.h —— 交互几何系统

### 设计借鉴

借鉴 Cinderella（随机化定理验证、连续运动保持构造一致性、投影几何核心）和 Dr. Geo（几何构造即代码生成、构造/脚本双向同步）。

### 交互模式

```c
typedef enum {
    GEO_MODE_POINT = 0,     /**< 点模式 */
    GEO_MODE_LINE = 1,      /**< 线模式 */
    GEO_MODE_CIRCLE = 2,    /**< 圆模式 */
    GEO_MODE_SEGMENT = 3,   /**< 线段模式 */
    GEO_MODE_SELECT = 4,    /**< 选择模式 */
    GEO_MODE_DRAG = 5,      /**< 拖拽模式 */
    GEO_MODE_CONSTRUCT = 6, /**< 构造模式 */
    GEO_MODE_MEASURE = 7,   /**< 测量模式 */
    GEO_MODE_PROVE = 8      /**< 证明模式 */
} InteractiveGeoMode;
```

### 随机化定理验证

借鉴 Cinderella 的 Randomized Theorem Checking：

1. 生成大量随机配置（默认 10000 次采样）
2. 在每种配置下验证定理条件
3. 统计通过率，给出概率真值判定

```c
typedef struct Lv00RandomizedCheck {
    int sample_count;
    double tolerance;
    int passed_samples, failed_samples;
    bool is_probabilistically_true;
    double confidence_level;
} Lv00RandomizedCheck;
```

验证结果分类：

```c
typedef enum {
    RAND_CHECK_PASSED = 0,                /**< 所有样本通过 */
    RAND_CHECK_FAILED = 1,                /**< 至少一个样本失败 */
    RAND_CHECK_INCONCLUSIVE = 2,          /**< 无法判定 */
    RAND_CHECK_PROBABILISTICALLY_TRUE = 3 /**< 高概率成立 */
} RandomizedCheckResult;
```

### 连续性跟踪

借鉴 Cinderella 的 Continuity 机制，使用投影几何（齐次坐标）作为底层数学模型，确保拖拽操作不发生"跳跃"。

```c
typedef struct Lv00ContinuityTracker {
    double *last_config;
    bool parallel_lines_detected;  /**< 平行线异常相交 */
    bool degenerate_triangle;      /**< 三角形退化 */
    bool zero_denominator;         /**< 分母为零 */
    bool near_singular;            /**< 接近奇异预警 */
    ConfigClassification current_config;
} Lv00ContinuityTracker;
```

### 约束实时维护

当用户拖拽点时，实时计算所有受影响的约束并更新位置：

1. 识别被移动对象影响的约束
2. 构建影响链（BFS 遍历约束图）
3. 使用投影几何方法迭代求解新位置
4. 检测奇异配置并回退

### 构造脚本生成

借鉴 Dr. Geo 的代码生成哲学，支持三种目标语言：

```c
typedef enum {
    SCRIPT_LANG_LV00_DSL = 0, /**< Lv-00 原生 DSL */
    SCRIPT_LANG_PYTHON = 1,   /**< Python */
    SCRIPT_LANG_LUA = 2       /**< Lua */
} ScriptLanguage;
```

### 快照系统

维护环形缓冲区（最多 32 个快照），支持撤销/重做功能。

## 6. geom_evol.h —— 几何演化引擎

### 设计借鉴

借鉴 SUNDIALS CVODE 的自适应步长 ODE 求解器，将几何约束演化为 ODE 系统在约束流形上演化。

### 演化方法

```c
typedef enum {
    LV00_EVOL_EULER = 0, /**< 显式 Euler 法（一阶） */
    LV00_EVOL_RK4 = 1,   /**< 经典四阶 Runge-Kutta 法 */
    LV00_EVOL_ADAMS = 2, /**< Adams-Bashforth-Moulton 预测-校正法 */
    LV00_EVOL_BDF = 3    /**< 后向差分公式 BDF（刚性） */
} Lv00EvolMethod;
```

### PI 步长控制器

借鉴 CVODE 的 PI（Proportional-Integral）控制器动态调整步长：

```
h_new = h * safety * (1/error)^(k_I + k_P)
```

```c
typedef struct Lv00GeomEvolPI {
    double safety_factor;  /**< 安全因子（典型值 0.9） */
    double growth_factor;  /**< 最大增长因子（典型值 2.5） */
    double bias_factor;    /**< P 分量权重（典型值 0.6） */
    double min_scale;      /**< 最小缩放因子（典型值 0.2） */
    double max_scale;      /**< 最大缩放因子（典型值 5.0） */
    double error_prev;     /**< 上一步误差估计 */
} Lv00GeomEvolPI;
```

### 演化引擎主结构

```c
struct Lv00GeomEvol {
    int dim;
    Lv00EvolMethod method;
    Lv00EvolStatus status;

    double t;                                        /**< 当前时间 */
    double param[GEOEVOL_MAX_PARAM_DIM];             /**< 当前参数向量 */
    double dparam[GEOEVOL_MAX_PARAM_DIM];            /**< 参数导数 */

    double step_size, step_size_min, step_size_max;
    double rel_tol, abs_tol;

    Lv00GeomEvolPI pi;
    Lv00GeomEvolRHSFunc rhs_func;
    Lv00GeomEvolPostStepFunc post_step;
    Lv00GeomEvolRootFunc root_func;
    Lv00GeomEvolStats stats;
};
```

### 误差控制

混合相对误差与绝对误差：

```
error_weight[i] = rel_tol * |param[i]| + abs_tol
```

### 统计信息

```c
typedef struct Lv00GeomEvolStats {
    int64_t num_steps;             /**< 累计步数 */
    int64_t num_rhs_evals;         /**< RHS 函数求值次数 */
    int64_t num_error_fails;       /**< 误差测试失败次数 */
    int64_t num_convergence_fails; /**< 收敛失败次数 */
    int64_t num_root_events;       /**< 事件触发次数 */
    double last_step_size;
    double last_error_est;
} Lv00GeomEvolStats;
```

## 7. geo_event_detect.h —— 几何事件检测器

### 设计借鉴

借鉴 SUNDIALS CVODE/CVODES 的 RootsFinding 模块，与演化引擎松耦合，通过注册回调函数工作。

### 事件类型

```c
typedef enum {
    LV00_EVENT_INTERSECTION = 0, /**< 交点事件 */
    LV00_EVENT_CONTACT = 1,      /**< 接触事件 */
    LV00_EVENT_CROSSING = 2,     /**< 穿越事件 */
    LV00_EVENT_THRESHOLD = 3,    /**< 阈值事件 */
    LV00_EVENT_PERIODIC = 4,     /**< 周期性事件 */
    LV00_EVENT_CUSTOM = 99       /**< 自定义事件 */
} Lv00EventType;
```

### 求根方法

```c
typedef enum {
    LV00_ROOTFIND_BRENT = 0,    /**< Brent 法（默认推荐） */
    LV00_ROOTFIND_ILLINOIS = 1, /**< Illinois 法（SUNDIALS 默认） */
    LV00_ROOTFIND_BISECTION = 2 /**< 二分法（最稳健） */
} Lv00RootfindMethod;
```

### 事件检测流程

1. 检查 `t_prev` 和 `t_curr` 处各事件函数值是否有符号变化
2. 对满足方向条件的事件，通过求根方法精确定位
3. 触发对应回调

### 方向过滤

```c
#define GEO_EVENT_DIR_NEGATIVE -1  /**< 仅检测负向穿越 */
#define GEO_EVENT_DIR_POSITIVE 1   /**< 仅检测正向穿越 */
#define GEO_EVENT_DIR_BOTH 2       /**< 检测双向穿越 */
#define GEO_EVENT_DIR_ANY 3        /**< 检测任意符号变化 */
```

### 事件检测器

```c
struct Lv00EventDetector {
    Lv00EventEntry events[GEO_EVENT_MAX_EVENTS]; // 32
    int num_events;
    Lv00RootfindMethod root_method;
    double root_tol;
    double t_prev;
    double g_prev[GEO_EVENT_MAX_EVENTS];
    void *user_data;
};
```

### Brent 法实现

内联提供了完整的 Brent 法实现，结合二分、割线和逆二次插值，在保证收敛性的同时提供超线性收敛速度。

## 8. geo_invariant_type.h —— 几何不变量类型

### 设计概述

几何不变量是在特定变换类（等距、相似、仿射等）下保持不变的几何性质。在 Lv-00 系统中，不变量携带信任颜色，反映计算的可信度。

### 不变量种类

提供 14 种基本几何不变量：

```c
typedef enum GeoInvariantKind {
    GEO_INV_DISTANCE,           /**< 欧几里得距离 */
    GEO_INV_ANGLE,              /**< 角度 */
    GEO_INV_AREA,               /**< 面积 */
    GEO_INV_VOLUME,             /**< 体积 */
    GEO_INV_CROSS_RATIO,        /**< 投影交比 */
    GEO_INV_CURVATURE,          /**< 曲线曲率 */
    GEO_INV_TORSION,            /**< 空间曲线挠率 */
    GEO_INV_PERIMETER,          /**< 周长 */
    GEO_INV_DIHEDRAL_ANGLE,     /**< 二面角 */
    GEO_INV_SOLID_ANGLE,        /**< 立体角 */
    GEO_INV_BARYCENTER,         /**< 重心 */
    GEO_INV_MOMENT_OF_INERTIA,  /**< 转动惯量 */
    GEO_INV_PARALLELISM,        /**< 平行关系 */
    GEO_INV_ORTHOGONALITY       /**< 垂直关系 */
} GeoInvariantKind;
```

### 不变量结构

```c
typedef struct GeoInvariant {
    GeoInvariantKind kind;
    char *name;           /**< 人类可读名称 */
    double value;         /**< 数值 */
    double trust;         /**< 信任级别（0.0..1.0） */
    int *entity_ids;      /**< 涉及的几何实体 ID */
    int entity_count;
    char *metadata;       /**< JSON 格式元数据 */
} GeoInvariant;
```

### 信任着色

信任级别为 0.0 到 1.0 的连续值，与符号坐标系统的 TrustColor 枚举对应：

- 1.0 = TRUST_GREEN（全构造）
- 0.8 = TRUST_BLUE（待完成证明义务）
- 0.6 = TRUST_YELLOW（条件性不可构造）
- 0.4 = TRUST_ORANGE（非构造性 oracle）
- 0.2 = TRUST_AMBER（数值假设）

### 类型关联

`geo_invariant_attach_to_type()` 将不变量关联到类型区域，使类型系统能够跟踪和验证几何性质。

## 9. geo_spec.h —— 几何构造规约层

### 设计借鉴

借鉴 TLA+（Leslie Lamport）的 Init/Next/Invariant 三段式框架，将几何构造建模为状态变迁系统。

### 三段式规约

```
Spec == Init /\ [][Next]_vars /\ Invariant
```

- **Init**：初始几何体声明（点、线、圆的基本配置）
- **Next**：构造步骤（作垂线、作平行线、作交点等）
- **Invariant**：不变式（构造过程中必须恒为真的命题）

### 构造步骤类型

```c
typedef enum {
    GEO_STEP_POINT,         /**< 定义点 */
    GEO_STEP_LINE,          /**< 定义线 */
    GEO_STEP_CIRCLE,        /**< 定义圆 */
    GEO_STEP_INTERSECTION,  /**< 求交点 */
    GEO_STEP_PERPENDICULAR, /**< 作垂线 */
    GEO_STEP_PARALLEL,      /**< 作平行线 */
    GEO_STEP_MIDPOINT,      /**< 求中点 */
    GEO_STEP_BISECTOR,      /**< 作角平分线 */
    GEO_STEP_MEASURE,       /**< 测量 */
    GEO_STEP_CONSTRAINT,    /**< 施加约束 */
    GEO_STEP_UNDO           /**< 撤销 */
} GeoStepType;
```

### 构造规约

```c
typedef struct GeoConstructionSpec {
    ConstraintGraph *initial;   /**< 初始几何配置 */
    GeoStep *steps;             /**< 构造步骤列表 */
    int step_count;
    char **invariants;          /**< 不变式表达式字符串数组 */
    int invariant_count;
} GeoConstructionSpec;
```

### 状态空间搜索

借鉴 TLC 模型检查器的穷举状态搜索：

```c
typedef struct StateSpaceExplorer {
    GeoConstructionState **queue;     /**< BFS/DFS 队列 */
    uint64_t *seen_fingerprints;      /**< 已见指纹集合 */
    int total_states_explored;
    int max_depth;
    GeoSearchStrategy strategy;       /**< BFS 或 DFS */
} StateSpaceExplorer;
```

### 模型检查

`geo_model_check()` 从 Init 出发，枚举所有可能的 Next 步骤，在每个状态下检查所有不变式。若发现违反，生成反例（从 Init 到违规状态的完整路径）。

### TLA+ 导出

`geo_spec_export_tlaplus()` 将构造规约导出为可被 TLC 模型检查器直接验证的 TLA+ 模块。

## 10. geo_topology.h —— 几何拓扑模块

### 设计借鉴

借鉴 GUDHI（计算拓扑）、GeometryCentral（离散微分几何）和 Polyscope（结构可视化）。

### 单纯复形

```c
typedef struct Lv00SimplicialComplex {
    int n_vertices;        /**< 顶点数（0-单纯形） */
    Lv00Edge *edges;       /**< 边数组（1-单纯形） */
    size_t n_edges;
    Lv00Triangle *triangles; /**< 三角形数组（2-单纯形） */
    size_t n_triangles;
} Lv00SimplicialComplex;
```

### 边与三角形

```c
typedef struct Lv00Edge {
    int v0, v1;  /**< 顶点索引，v0 < v1 */
} Lv00Edge;

typedef struct Lv00Triangle {
    int v0, v1, v2;  /**< 顶点索引，v0 < v1 < v2 */
} Lv00Triangle;
```

### 核心操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建/销毁 | `geo_simplicial_create()` / `geo_simplicial_destroy()` | 生命周期管理 |
| 添加边 | `geo_simplicial_add_edge()` | 添加 1-单纯形 |
| 添加三角形 | `geo_simplicial_add_triangle()` | 添加 2-单纯形 |
| Euler 特征数 | `geo_simplicial_euler_characteristic()` | 计算 V - E + F |
| 边界算子 | `geo_simplicial_boundary()` | 三角形 → 三条边 |
| 连通分量 | `geo_simplicial_connected_components()` | Union-Find 算法 |

### Euler 特征数

```
chi = V - E + F
```

其中 V = 顶点数，E = 边数，F = 三角形数。

### 边界算子

三角形 (v0, v1, v2) 的边界为三条边：(v0, v1), (v1, v2), (v0, v2)。

```c
typedef struct Lv00Boundary {
    Lv00Edge *edges;
    size_t n_edges;
    int *vertices;
    size_t n_vertices;
} Lv00Boundary;
```

### 连通分量

使用 Union-Find（不相交集合）算法在 1-骨架（顶点+边构成的图）上计算连通分量数。

## 实现文件

- **头文件**：`include/lv00/geometry_types.h`, `include/lv00/geometry_transform.h`, `include/lv00/geometry_compress.h`, `include/lv00/high_dim.h`, `include/lv00/interactive_geo.h`, `include/lv00/geom_evol.h`, `include/lv00/geo_event_detect.h`, `include/lv00/geo_invariant_type.h`, `include/lv00/geo_spec.h`, `include/lv00/geo_topology.h`
- **源文件**：`src/layer3_geometry/` 下的对应 .c 文件

## 依赖

- GMP 库（geometry_transform.h 中的有理数变换矩阵）
- SymbolicCoord 系统（geometry_types.h 中的符号坐标）
- ConstraintGraph（多个模块的约束图交互）
- 可选：SUNDIALS（geom_evol.h 和 geo_event_detect.h 的参考实现）

## 测试要点

1. 几何实体类型层次的创建与销毁
2. 扁平数组存储的批量变换正确性
3. 变换矩阵的符号计算精度
4. 变换群生成元的正确性
5. Edgebreaker CLERS 编码/解码的一致性
6. 高维投影的保真度计算
7. 交互几何的拖拽约束维护
8. 随机化定理验证的统计正确性
9. 演化引擎的 PI 步长控制稳定性
10. 事件检测的求根精度
11. 不变量的信任着色一致性
12. 模型检查器的反例生成
13. 单纯复形的 Euler 特征数与连通分量
