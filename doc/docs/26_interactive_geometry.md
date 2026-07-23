# 26. 交互式几何与事件系统

## 26.1 模块概述

本文档描述 Lv-00 几何元语言系统的交互式几何模块和几何事件检测机制。交互几何借鉴 Cinderella 和 Dr. Geo 的设计哲学，提供直观的拖拽式几何构造体验；事件检测借鉴 SUNDIALS 的 Rootfinding 机制，实现几何演化过程中的精确事件定位。

**覆盖头文件**：
- `interactive_geo.h` —— 交互几何系统（借鉴 Cinderella + Dr. Geo）
- `geo_event_detect.h` —— 几何事件检测（借鉴 SUNDIALS）
- `geo_topology.h` —— 几何拓扑（单纯复形）
- `geo_invariant_type.h` —— 几何不变量类型

---

## 26.2 interactive_geo.h —— 交互几何系统

### 26.2.1 设计借鉴来源

**Cinderella (cinderella.de, 1998-)**：
- **Randomized Theorem Checking**：通过大量随机采样验证几何定理，提供概率性真值判定
- **Continuity Tracking**：在用户拖拽几何对象时，自动检测奇异配置并维护构造语境
- **投影几何核心**：基于齐次坐标的投影几何作为底层模型

**Dr. Geo (gnu.org/software/dr-geo, 1996-)**：
- **几何构造即代码生成**：用户画图的同时自动生成 Smalltalk 脚本
- **构造/脚本双向同步**：修改脚本自动更新图形，反之亦然

### 26.2.2 交互模式

| 模式 | 枚举值 | 功能 |
|------|--------|------|
| 点模式 | `GEO_MODE_POINT` | 点击画布创建新点 |
| 线模式 | `GEO_MODE_LINE` | 拖拽创建直线 |
| 圆模式 | `GEO_MODE_CIRCLE` | 点击圆心后拖拽确定半径 |
| 线段模式 | `GEO_MODE_SEGMENT` | 点击两端点创建线段 |
| 选择模式 | `GEO_MODE_SELECT` | 点击选中/取消选中几何对象 |
| 拖拽模式 | `GEO_MODE_DRAG` | 自由拖拽移动几何对象 |
| 构造模式 | `GEO_MODE_CONSTRUCT` | 通过预设构造规则创建几何体 |
| 测量模式 | `GEO_MODE_MEASURE` | 点击显示距离/角度/面积 |
| 证明模式 | `GEO_MODE_PROVE` | 选择几何体并启动自动证明 |

### 26.2.3 核心数据结构

#### 画布状态（lvGeoCanvasState）

```c
typedef struct lvGeoCanvasState {
    // 几何对象
    int *active_object_ids;   // 所有活跃几何对象 ID 列表
    int active_object_count;
    
    // 拖拽状态
    int drag_target_id;       // 当前被拖拽的对象 ID（-1 = 无拖拽）
    double drag_start_x, drag_start_y;
    double drag_current_x, drag_current_y;
    
    // 选中状态
    int *selected_ids;
    int selected_count;
    int primary_selected_id;  // 主选中对象 ID
    
    // 当前构造模式
    InteractiveGeoMode current_mode;
    int construction_partials[4];  // 构造模式部分结果
    int construction_partial_count;
    
    // 视口变换
    double viewport_matrix[3][3];
    double zoom_level;
    double viewport_offset_x, viewport_offset_y;
    
    // 元数据
    bool grid_visible;
    bool snap_to_grid;
    double grid_spacing;
    bool modified;
} lvGeoCanvasState;
```

#### 随机化定理验证（lvRandomizedCheck）

借鉴 Cinderella 的随机化定理验证：

```c
typedef struct lvRandomizedCheck {
    int sample_count;      // 随机采样次数（默认 10000）
    double tolerance;      // 数值容差（默认 1e-9）
    
    int passed_samples;    // 通过的样本数
    int failed_samples;    // 失败的样本数
    
    bool is_probabilistically_true;  // 概率真值判定
    double confidence_level;         // 置信水平 [0.0, 1.0]
    
    double *failed_sample_params;    // 失败样本的参数数组
    double elapsed_time_ms;          // 验证耗时
} lvRandomizedCheck;
```

**算法流程**：
1. 生成 `sample_count` 组随机配置（遵守当前约束）
2. 在每组配置下评估定理条件
3. 统计通过率，返回概率真值判定
4. 若 `confidence_level >= lv_GEO_HIGH_CONFIDENCE (0.9999)`，判定为高概率成立

#### 几何脚本绑定（lvGeoScriptBinding）

借鉴 Dr. Geo 的代码生成哲学：

```c
typedef struct lvGeoScriptBinding {
    // 对象→脚本映射表
    int *object_ids;
    char **script_snippets;  // 对应脚本代码片段
    int binding_count;
    
    // 脚本语言配置
    ScriptLanguage current_language;  // lv_DSL / PYTHON / LUA
    bool auto_generate;
    
    // 脚本缓冲区
    char full_script[lv_GEO_SCRIPT_BUFFER_SIZE];
    int script_length;
} lvGeoScriptBinding;
```

**支持脚本语言**：
- `SCRIPT_LANG_lv_DSL` —— Lv-00 原生 DSL
- `SCRIPT_LANG_PYTHON` —— Python 脚本
- `SCRIPT_LANG_LUA` —— Lua 脚本

#### 连续性跟踪器（lvContinuityTracker）

借鉴 Cinderella 的连续性机制：

```c
typedef struct lvContinuityTracker {
    double *last_config;      // 上次配置的参数向量
    
    // 奇异检测
    bool parallel_lines_detected;  // 平行线异常相交
    bool degenerate_triangle;      // 三角形退化（三点共线）
    bool zero_denominator;         // 分母为零
    bool near_singular;            // 接近奇异预警
    
    // 配置分类
    ConfigClassification current_config;   // NORMAL / SINGULAR / DEGENERATE
    ConfigClassification previous_config;
    
    double singular_threshold;
    double degenerate_threshold;
} lvContinuityTracker;
```

#### 约束保持器（lvConstraintMaintainer）

```c
typedef struct lvConstraintMaintainer {
    int *constraint_ids;
    int *constraint_subjects;
    int constraint_count;
    
    int *affected_objects;    // 当前受影响的对象
    int affected_count;
    
    void *solver_handle;
    bool use_projective_method;  // 使用投影几何方法
    double convergence_epsilon;
    int max_iterations;
} lvConstraintMaintainer;
```

### 26.2.4 核心 API

#### 生命周期

```c
lvInteractiveGeo *interactive_geo_init(lvContext *ctx_handle);
void interactive_geo_destroy(lvInteractiveGeo *geo);
```

#### 模式管理

```c
void interactive_geo_set_mode(lvInteractiveGeo *geo, InteractiveGeoMode mode);
InteractiveGeoMode interactive_geo_get_mode(const lvInteractiveGeo *geo);
```

#### 选择管理

```c
int interactive_geo_select(lvInteractiveGeo *geo, int object_id);
void interactive_geo_deselect(lvInteractiveGeo *geo, int object_id);
```

#### 拖拽交互

```c
int interactive_geo_drag_start(lvInteractiveGeo *geo, int object_id, double x, double y);
ConstraintMaintainStatus interactive_geo_drag_move(lvInteractiveGeo *geo, double x, double y);
ConstraintMaintainStatus interactive_geo_drag_end(lvInteractiveGeo *geo, double x, double y);
```

**约束维护流程**：
1. 识别被移动对象影响的约束
2. 构建影响链（BFS 遍历约束图）
3. 使用投影几何方法迭代求解新位置
4. 检测奇异配置并回退（如需要）

#### 随机化定理验证

```c
RandomizedCheckResult interactive_geo_randomized_check(
    lvInteractiveGeo *geo,
    int sample_count,           // 0 = 使用默认值 10000
    double tolerance,           // 0 = 使用默认值 1e-9
    const char *theorem_expr,   // Lv-00 DSL 格式
    lvRandomizedCheck *result
);
```

#### 构造脚本生成

```c
int interactive_geo_generate_script(
    lvInteractiveGeo *geo,
    ScriptLanguage language,
    char **output
);
```

#### 状态导入/导出

```c
char *interactive_geo_export_state(const lvInteractiveGeo *geo);
int interactive_geo_import_state(lvInteractiveGeo *geo, const char *json);
```

#### 快照/恢复

```c
int interactive_geo_snapshot(lvInteractiveGeo *geo);
int interactive_geo_restore(lvInteractiveGeo *geo, int snapshot_index);
```

### 26.2.5 配置常量

| 常量 | 默认值 | 说明 |
|------|--------|------|
| `lv_GEO_MAX_OBJECTS` | 1024 | 最大同时活跃几何对象数量 |
| `lv_GEO_MAX_CONSTRAINTS` | 2048 | 最大约束数量 |
| `lv_GEO_MAX_DRAG_CHAIN` | 64 | 最大拖拽影响链深度 |
| `lv_GEO_MAX_SNAPSHOTS` | 32 | 快照历史最大保留数量 |
| `lv_GEO_SCRIPT_BUFFER_SIZE` | 65536 | 构造脚本缓冲区大小 |
| `lv_GEO_DEFAULT_SAMPLE_COUNT` | 10000 | 随机化验证默认采样次数 |
| `lv_GEO_DEFAULT_TOLERANCE` | 1e-9 | 随机化验证默认容差 |
| `lv_GEO_HIGH_CONFIDENCE` | 0.9999 | 随机化验证高置信度阈值 |

---

## 26.3 geo_event_detect.h —— 几何事件检测

### 26.3.1 设计借鉴来源

**SUNDIALS CVODE/CVODES (github.com/LLNL/sundials)**：
- **RootsFinding 模块**：在 ODE 演化中检测事件函数过零
- **与演化引擎松耦合**：通过注册回调函数工作
- **多种求根方法**：Brent / Illinois / Bisection

### 26.3.2 事件类型

| 类型 | 枚举值 | 说明 |
|------|--------|------|
| 交点事件 | `lv_EVENT_INTERSECTION` | 两条曲线相交 |
| 接触事件 | `lv_EVENT_CONTACT` | 几何体发生接触 |
| 穿越事件 | `lv_EVENT_CROSSING` | 点/线穿越某个边界 |
| 阈值事件 | `lv_EVENT_THRESHOLD` | 某个量超过阈值 |
| 周期性事件 | `lv_EVENT_PERIODIC` | 在固定时间点触发 |
| 自定义事件 | `lv_EVENT_CUSTOM` | 用户定义事件 |

### 26.3.3 方向过滤

| 方向 | 值 | 说明 |
|------|-----|------|
| 负向穿越 | -1 | 仅检测从正到负的穿越 |
| 正向穿越 | 1 | 仅检测从负到正的穿越 |
| 双向穿越 | 2 | 检测双向穿越 |
| 任意变化 | 3 | 检测任意符号变化（含触碰） |

### 26.3.4 求根方法

| 方法 | 枚举值 | 特点 |
|------|--------|------|
| Brent 法 | `lv_ROOTFIND_BRENT` | 结合二分、割线和逆二次插值（默认推荐） |
| Illinois 法 | `lv_ROOTFIND_ILLINOIS` | 改进试位法（SUNDIALS 默认） |
| 二分法 | `lv_ROOTFIND_BISECTION` | 最稳健，但收敛较慢（备选） |

### 26.3.5 核心数据结构

#### 事件条目（lvEventEntry）

```c
typedef struct lvEventEntry {
    int event_id;           // 事件唯一 ID
    lvEventType type;     // 事件类型
    lvEventFunc func;     // 事件函数 g(t, param)
    int direction;          // 方向过滤
    bool enabled;           // 是否活跃
    bool terminal;          // 是否为终止事件
    lvEventCallback callback;  // 事件处理回调
} lvEventEntry;
```

#### 事件检测器（lvEventDetector）

```c
typedef struct lvEventDetector {
    lvEventEntry events[GEO_EVENT_MAX_EVENTS];
    int num_events;
    
    lvRootfindMethod root_method;
    double root_tol;
    int max_root_iters;
    
    double t_prev;
    double g_prev[GEO_EVENT_MAX_EVENTS];  // 上一步的事件函数值
    
    void *user_data;
} lvEventDetector;
```

### 26.3.6 核心 API

#### 生命周期

```c
lvEventDetector *geo_event_detector_create(void);
void geo_event_detector_destroy(lvEventDetector *detector);
```

#### 事件注册

```c
int geo_event_register(
    lvEventDetector *detector,
    int event_id,
    lvEventType type,
    lvEventFunc func,        // 事件函数 g(t, param)
    int direction,             // 方向过滤
    bool terminal,             // 是否为终止事件
    lvEventCallback callback // 事件处理回调
);
```

#### 事件检测

```c
lvEventResult geo_event_detect(
    lvEventDetector *detector,
    double t_prev, const double *param_prev,
    double t_curr, const double *param_curr,
    int dim,
    int *event_id,      // 输出：触发的事件 ID
    double *t_event     // 输出：事件发生的精确参数值
);
```

**检测流程**：
1. 检查 `t_prev` 和 `t_curr` 处各事件函数值是否有符号变化
2. 对满足方向条件的事件，通过求根方法精确定位
3. 触发对应回调

#### 精确求根定位

```c
int geo_event_root_locate(
    lvEventDetector *detector,
    int event_id,
    const double *param_a, const double *param_b,
    int dim,
    double a, double b,      // 区间 [a, b]
    double ga, double gb,    // g(a), g(b) 的值
    double *root             // 输出：根的精确位置
);
```

### 26.3.7 配置常量

| 常量 | 默认值 | 说明 |
|------|--------|------|
| `GEO_EVENT_MAX_EVENTS` | 32 | 最大同时注册的事件数量 |
| `GEO_EVENT_MAX_DEPTH` | 8 | 最大检测器嵌套深度 |
| `GEO_EVENT_MAX_ROOT_ITERS` | 100 | 事件定位最大迭代次数 |
| `GEO_EVENT_DEFAULT_TOL` | 1e-12 | 事件定位默认容差 |

---

## 26.4 geo_topology.h —— 几何拓扑

### 26.4.1 设计定位

提供单纯复形（simplicial complex）的数据结构和算法，用于计算拓扑不变量：
- 借鉴 GUDHI（计算拓扑）
- 借鉴 GeometryCentral（离散微分几何）
- 借鉴 Polyscope（结构/量可视化）

### 26.4.2 单纯复形表示

```c
typedef struct lvEdge {
    int v0, v1;  // 顶点索引（v0 < v1）
} lvEdge;

typedef struct lvTriangle {
    int v0, v1, v2;  // 顶点索引（v0 < v1 < v2）
} lvTriangle;

typedef struct lvSimplicialComplex {
    int n_vertices;           // 顶点数（0-单形）
    lvEdge *edges;          // 边数组（1-单形）
    size_t n_edges;
    lvTriangle *triangles;  // 三角形数组（2-单形）
    size_t n_triangles;
} lvSimplicialComplex;
```

### 26.4.3 核心 API

#### 创建与销毁

```c
lvSimplicialComplex *geo_simplicial_create(int n_vertices);
void geo_simplicial_destroy(lvSimplicialComplex *sc);
```

#### 添加单形

```c
bool geo_simplicial_add_edge(lvSimplicialComplex *sc, int v0, int v1);
bool geo_simplicial_add_triangle(lvSimplicialComplex *sc, int v0, int v1, int v2);
```

#### 拓扑不变量

```c
int geo_simplicial_euler_characteristic(const lvSimplicialComplex *sc);
```

**欧拉示性数**：χ = V - E + F

#### 边界算子

```c
lvBoundary *geo_simplicial_boundary(const lvSimplicialComplex *sc, const lvTriangle *tri);
```

三角形 (v0, v1, v2) 的边界是三边：(v0, v1), (v1, v2), (v0, v2)

#### 连通分量

```c
int geo_simplicial_connected_components(const lvSimplicialComplex *sc);
```

使用并查集（Union-Find）算法计算 1-骨架的连通分量数。

---

## 26.5 代码-理论对应关系

| 代码概念 | 理论对应 | 文档位置 |
|----------|----------|----------|
| `lvRandomizedCheck` | 概率性定理验证 | 本文档 26.2.3 |
| `lvContinuityTracker` | 投影几何连续性 | 本文档 26.2.3 |
| `lvConstraintMaintainer` | 实时约束求解 | 本文档 26.2.3 |
| `lvEventDetector` | 事件函数过零检测 | 本文档 26.3.5 |
| `geo_event_root_brent()` | Brent 求根算法 | 本文档 26.3.6 |
| `lvSimplicialComplex` | 单纯复形 | 本文档 26.4.2 |
| `geo_simplicial_euler_characteristic()` | 欧拉示性数 χ = V - E + F | 本文档 26.4.3 |

---

## 26.6 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [15_geometry_advanced.md](15_geometry_advanced.md) | 高级几何模块 |
| [21_euclidean_geometry.md](21_euclidean_geometry.md) | 欧氏几何公理包 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图核心 |
| [03_normalization.md](03_normalization.md) | 图规范化 |
| [24_constraint_propagation.md](24_constraint_propagation.md) | 约束传播与等价类 |

---

## 26.7 版本历史

- **v5.0.0** (当前)
  - 交互几何系统（借鉴 Cinderella + Dr. Geo）
  - 随机化定理验证
  - 连续性跟踪与约束维护
  - 几何事件检测（借鉴 SUNDIALS）
  - 单纯复形拓扑计算

- **v3.3.0**
  - 基础几何类型定义
  - 几何变换与压缩
