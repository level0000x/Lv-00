# Polyscope Structure+Quantity 可视化架构借鉴设计

> **借鉴项目**：Polyscope（github.com/nmwsharp/polyscope）
> **核心借鉴点**：Structure+Quantity 分离范式、不接管程序控制流（非"全应用"模式）、数据适配器模板、头文件+CMake轻量集成
> **分类**：P3 中优先级 / 几何数据可视化架构
> **日期**：2026-05-24

---

## 1. 概述

Polyscope 是由 Nicholas Sharp 等人在卡内基梅隆大学开发的轻量级 C++ 几何数据可视化库。其核心设计理念是**Structure+Quantity 分离**：将几何"载体"（Structure，如 SurfaceMesh、PointCloud、CurveNetwork）与附着在其上的"数据"（Quantity，如标量函数、向量场、颜色映射）解耦为两个正交的概念，由库自动管理可视化管线、色彩映射和交互。这一范式与 MATLAB、ParaView 等传统的"全应用"模式形成鲜明对比——前者将程序控制流交给可视化工具，后者允许研究者在自己熟悉的主程序流程中嵌入可视化。

Polyscope 的三大核心设计特征对 Lv-00 的几何可视化子系统有直接借鉴价值。第一，**不接管控制流**——`polyscope::show()` 仅仅是弹出一个窗口并在内部运行一个帧循环，程序的主控制流仍然由调用者掌握。这意味着用户可以在自己的几何算法代码中穿插可视化检查点，而不需要将算法"适配"到可视化工具的框架中。第二，**数据适配器模板**——通过 C++ 模板自动适配 Eigen 矩阵、`std::vector`、原始数组等多种数据容器，使用户无需手动转换数据格式。第三，**头文件 + CMake 轻量集成**——无需复杂的构建配置，`#include "polyscope/polyscope.h"` 即可使用全部功能。

对 Lv-00 而言，Polyscope 的 Structure+Quantity 范式精确对应着 Lv-00 Web GUI 几何可视化面板中"几何对象渲染层（约束图节点）+ 色彩标注层（约束条件/证明状态/类型颜色）"的分层架构。此外，Polyscope 的轻量级设计为 Lv-00 的 Web 前端提供了"不绑架用户交互流"的重要设计原则——可视化窗口应该是一个透明的观察器，而非一个全屏的编辑环境。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 Structure+Quantity 分离范式

Polyscope 的核心架构将几何可视化分为两个正交层次：

```
Structure（几何载体）              Quantity（附着数据）
  ├─ SurfaceMesh              ←    ├─ ScalarQuantity（标量函数）
  │  顶点(_V) + 面(_F)       ←    │   ├─ 顶点值、面值、边值
  │                           ←    │   └─ 自动色彩映射
  ├─ PointCloud               ←    ├─ VectorQuantity（向量场）
  │  点坐标(_P)               ←    │   ├─ 3D 向量（法向量、梯度）
  │                           ←    │   └─ 自动箭头/锥体渲染
  ├─ CurveNetwork             ←    ├─ ColorQuantity（颜色标注）
  │  节点(_V) + 边(_E)        ←    │   ├─ RGB/RGBA 直接颜色
  │                           ←    │   └─ 表面着色
  └─ VolumeMesh（实验性）     ←    └─ ParameterizationQuantity（参数化）
      体网格                          ├─ UV 贴图坐标
                                     └─ 纹理映射
```

**关键特性**：
- Structure 注册后获得唯一句柄（如 `ps_surface`），后续 Quantity 通过此句柄与之关联
- 同一个 Structure 可以附着任意数量的 Quantity（如同时显示平均曲率 + 高斯曲率 + 法向量）
- Structure 和 Quantity 各自独立管理生命周期，Quantity 可以在运行时添加/移除，不影响 Structure 的渲染
- 色彩映射（Colormap）、等值线（Isolines）、光照（Shading）均由 Quantity 自动管理

### 2.2 Lv-00 几何可视化 → Structure+Quantity 映射

Lv-00 Web GUI 的几何可视化层可以精确映射 Polyscope 的 Structure+Quantity 范式：

| Polyscope 概念 | Lv-00 Web GUI 映射 | 说明 |
|:---|:---|:---|
| SurfaceMesh | `ConstraintGraph` 中的几何节点（点、线段、圆、三角形） | Lv-00 的几何对象作为"Structure"渲染 |
| PointCloud | 离散点集（自由点、交点、构造点） | 点的坐标 + 类型标注 |
| CurveNetwork | 约束边网络（线段 + 弧 + 辅助线） | 图形骨架的拓扑渲染 |
| ScalarQuantity | 约束满足度颜色映射（红色=冲突，绿色=满足，黄色=未判定） | 类似有限元分析的应力热力图 |
| VectorQuantity | 约束梯度方向指示（力的方向、位移向量） | 指示约束传播方向 |
| ColorQuantity | 类型颜色标注（type_color：点=蓝，线段=绿，圆=橙，依赖类型=紫） | Lv-00 的类型系统颜色到 Polyscope 的颜色映射 |
| Colormap | `ConstraintHeatMap` 组件 | 约束条件满足度的自动色彩映射 |
| Isolines | 等长线/等角线标注 | 距离/角度约束的可视化等值线 |
| `ps_mesh->addVertexScalarQuantity()` | `geometry_view.add_constraint_heatmap(node_id, values)` | 将约束满足度数据附着到几何对象 |

### 2.3 不接管控制流的设计模式

Polyscope 的核心设计原则是"不接管程序控制流"——与 MATLAB/ParaView 的"全应用"范式形成根本差异：

```
MATLAB/ParaView 的"全应用"模式：
  ┌─────────────────────────────────────┐
  │  MATLAB/ParaView 主窗口             │
  │  ┌───────────────────────────────┐  │
  │  │  算法必须以插件/脚本形式嵌入  │  │
  │  │  可视化工具控制程序生命周期   │  │
  │  │  用户交互流被可视化工具绑架   │  │
  │  └───────────────────────────────┘  │
  └─────────────────────────────────────┘

Polyscope 的"观察器"模式：
  ┌──────────────────────┐     ┌──────────────────────┐
  │  用户的主程序         │     │  Polyscope 窗口      │
  │  my_algorithm()       │     │  ┌────────────────┐  │
  │  compute_geometry()   │────→│  │  透明观察器     │  │
  │  polyscope::show()    │ ←──│  │  帧循环独立运行 │  │
  │  continue_algorithm() │     │  └────────────────┘  │
  │  程序继续执行         │     │  窗口可随时关闭      │
  └──────────────────────┘     └──────────────────────┘
```

**Lv-00 中的"不绑架交互流"原则**：Lv-00 Web GUI 的可视化面板应该表现为一个**可嵌入式观察器**，而非全屏编辑器。具体策略：

- Lv-00 证明面板（主交互流）与 Lv-00 几何可视化面板（观察器）并行存在
- 几何可视化面板可独立弹出/嵌入/关闭，不影响证明面板的编辑状态
- 可视化更新由证明步骤的提交事件自动触发（`on_commit_step` 钩子）
- 用户可以随时关闭可视化窗口继续证明工作

```c
/**
 * @brief Lv-00 可视化观察器模式 —— 借鉴 Polyscope 不接管控制流
 *
 * Lv-00 几何可视化窗口是独立的观察器组件，
 * 不绑架用户的主要证明交互流。
 */
typedef struct {
    /** 可视化窗口状态 */
    bool is_open;                   /**< 窗口是否打开 */
    bool is_docked;                 /**< 窗口是否坞接到主面板 */

    /** 更新策略 */
    enum {
        VIZ_UPDATE_MANUAL,          /**< 手动触发更新（用户点击 Refresh） */
        VIZ_UPDATE_ON_COMMIT,       /**< 每次提交步骤自动更新（推荐） */
        VIZ_UPDATE_ON_CHANGE,       /**< 约束图每次变化都更新（性能开销大） */
        VIZ_UPDATE_CONTINUOUS       /**< 连续帧循环更新（动画/演示模式） */
    } update_strategy;

    /** 渲染配置 */
    struct {
        bool show_ghost_objects;    /**< 是否渲染 Ghost 几何对象 */
        float point_size;           /**< 点渲染大小 */
        float edge_width;           /**< 边渲染宽度 */
        bool wireframe_only;        /**< 是否仅线框渲染 */
    } render_config;

    /** 程序控制流独立性标志 */
    bool control_flow_independent;  /**< 可视化不接管主程序控制流 */
} Lv00VisualizationObserver;
```

### 2.4 数据适配器模板

Polyscope 通过 C++ 模板自动适配多种数据容器类型，用户无需手动转换：

```cpp
// Polyscope 自动适配多种数据格式
Eigen::MatrixXd V;          // 顶点坐标矩阵
Eigen::MatrixXi F;          // 面索引矩阵
std::vector<double> vals;   // 标量值
double *raw_array;          // 原始数组

// 统一接口，无需关心底层容器
auto ps_mesh = polyscope::registerSurfaceMesh("my_mesh", V, F);
ps_mesh->addVertexScalarQuantity("scalar", vals);  // 自动适配 std::vector
ps_mesh->addVertexScalarQuantity("raw", raw_array, n_verts); // 自动适配原始数组
```

Lv-00 中对应的数据适配层：

```c
/**
 * @brief Lv-00 几何数据适配器 —— 借鉴 Polyscope 数据适配器模板
 *
 * 将 Lv-00 的符号坐标（有理数/代数数）适配为可视化渲染管线的
 * 浮点坐标，同时保留符号→数值的映射关系。
 *
 * 关键挑战：Lv-00 使用有理数和代数数作为精确坐标，
 * Polyscope 使用 float/double 作为渲染坐标。
 * 适配器负责在两者之间进行安全转换。
 */
typedef struct {
    /** 数据源类型 */
    enum {
        ADAPTER_SRC_RATIONAL,       /**< Lv-00 有理数坐标 */
        ADAPTER_SRC_ALGEBRAIC,      /**< Lv-00 代数数坐标 */
        ADAPTER_SRC_SYMBOLIC,       /**< 纯符号坐标（含自由变量） */
        ADAPTER_SRC_FLOAT,          /**< 已数值化的坐标 */
        ADAPTER_SRC_HYBRID          /**< 混合类型（部分符号+部分数值） */
    } source_type;

    /** 数值化策略 */
    enum {
        NUMERIZE_EXACT_FLOAT,       /**< 精确转双精度（有理数→分子/分母除法） */
        NUMERIZE_INTERVAL,          /**< 区间逼近（代数数→上下界中值） */
        NUMERIZE_SYMBOLIC_EVAL,     /**< 符号代入求值（代入已知变量值） */
        NUMERIZE_NEWTON_REFINE      /**< Newton 迭代精化（高精度浮点逼近） */
    } numerization_strategy;

    /** 精度配置 */
    int float_precision_bits;       /**< 浮点精度（32=单精度，64=双精度，128=四倍精度） */
    double epsilon_tolerance;       /**< 数值化容差 */
    bool warn_on_precision_loss;    /**< 精度损失警告 */

    /** 符号→数值映射表（用于反向查找：从渲染坐标回到约束图节点） */
    struct {
        void *node_id;              /**< 约束图节点 ID */
        double x, y, z;             /**< 数值化坐标 */
        double error_bound;         /**< 误差界 */
    } *mapping_table;
    int mapping_count;
} Lv00GeometryAdapter;
```

### 2.5 Structure+Quantity 分层渲染代码示例

```c
/**
 * @brief Lv-00 几何可视化中实现 Structure+Quantity 分层渲染
 *
 * 借鉴 Polyscope 的 registerStructure + addQuantity 模式，
 * Lv-00 的渲染器将几何对象和约束数据分为两层：
 *
 * Layer 1 (Structure): 几何对象本体（点、线段、三角形、圆）
 * Layer 2 (Quantity):  附着数据（约束满足度、类型颜色、证明状态）
 */

/**
 * @brief Layer 1：注册几何 Structure
 *
 * 对应 Polyscope 的 registerSurfaceMesh/registerPointCloud。
 * 每个约束图节点映射为独立的几何 Structure。
 */
typedef struct {
    const char *structure_name;        /**< Structure 唯一名称 */
    ConstraintNodeType node_type;      /**< 几何类型（POINT/SEGMENT/CIRCLE...） */
    double *vertices;                  /**< 数值化后的顶点坐标（N*3） */
    int vertex_count;                  /**< 顶点数量 */
    int *indices;                      /**< 拓扑索引（边/面连接关系） */
    int index_count;                   /**< 索引数量 */
    void *constraint_node_ref;         /**< 反向引用：约束图节点指针 */
} Lv00GeometryStructure;

/**
 * @brief Layer 2.1：约束满足度 Quantity（标量函数）
 *
 * 对应 Polyscope 的 ScalarQuantity。
 * 将每条约束的满足程度编码为 [0,1] 标量值，自动生成热力图色彩映射。
 */
typedef struct {
    const char *quantity_name;         /**< Quantity 名称 */
    Lv00GeometryStructure *parent;     /**< 附着的 Structure */
    double *constraint_satisfaction;   /**< 约束满足度数组 [0,1]：
                                        *   1.0 = 完全满足（绿色）
                                        *   0.5 = 未知/未验证（黄色）
                                        *   0.0 = 冲突/违反（红色） */
    int value_count;                   /**< 值数量（通常=Structure.vertex_count） */
    char *colormap_name;               /**< 色彩映射方案（"viridis"/"RdYlGn"/"coolwarm"） */
    bool show_colorbar;                /**< 是否显示色标 */
    double isoline_spacing;            /**< 等值线间距（0=不显示等值线） */
} Lv00ConstraintSatisfactionQuantity;

/**
 * @brief Layer 2.2：类型颜色 Quantity（颜色标注）
 *
 * 对应 Polyscope 的 ColorQuantity。
 * 按 Lv-00 类型系统着色每个几何对象。
 */
typedef struct {
    const char *quantity_name;
    Lv00GeometryStructure *parent;
    uint8_t *type_colors;              /**< RGBA 颜色数组：
                                        *   点    → 蓝色  (0x3366CCFF)
                                        *   线段  → 绿色  (0x33CC66FF)
                                        *   圆    → 橙色  (0xCC8833FF)
                                        *   依赖类型 → 紫色 (0x9933CCFF)
                                        *   Ghost → 半透明灰 (0x88888880) */
    int color_count;
} Lv00TypeColorQuantity;

/**
 * @brief Layer 2.3：证明状态标注 Quantity（向量场 + 标注）
 *
 * 对应 Polyscope 的 VectorQuantity。
 * 用箭头和标注指示约束的证明状态和传播方向。
 */
typedef struct {
    const char *quantity_name;
    Lv00GeometryStructure *parent;
    double *status_vectors;            /**< 状态向量（方向+长度编码证明状态）：
                                        *   向上箭头 ↑ = 已证明
                                        *   水平箭头 → = 证明中
                                        *   向下箭头 ↓ = 被阻塞
                                        *   NULL箭头   = 未开始 */
    int vector_count;
    char **status_labels;              /**< 状态文字标注（"proved"/"pending"/"blocked"） */
    bool show_arrows;                  /**< 是否显示方向箭头 */
} Lv00ProofStatusQuantity;
```

---

## 3. 实现方案

### 3.1 第一阶段：基础 Structure 渲染层（P3-1）

- [ ] 实现 `Lv00GeometryAdapter` 符号→数值坐标转换
  - 有理数坐标精确转双精度（`mpq_get_d`）
  - 代数数坐标区间逼近（`arb_get_interval`）
  - 符号坐标代入求值（自由变量→数值映射）
- [ ] 实现 `Lv00GeometryStructure` 注册系统
  - 点 Structure（PointCloud 渲染）
  - 线段 Structure（CurveNetwork 渲染）
  - 三角形 Structure（SurfaceMesh 渲染）
  - 圆 Structure（参数曲线渲染，N 段逼近）
- [ ] 实现基础 WebGL 渲染器（基于 Three.js/Babylon.js）
- [ ] 编写符号→数值转换的精度测试

### 3.2 第二阶段：Quantity 约束数据叠加层（P3-2）

- [ ] 实现 `Lv00ConstraintSatisfactionQuantity` 约束满足度热力图
  - 约束满足度 [0,1] → viridis/RdYlGn 色彩映射
  - 自动色彩映射表生成
  - 等值线渲染（isoline）
  - 色标（colorbar）组件
- [ ] 实现 `Lv00TypeColorQuantity` 类型颜色标注
  - Lv-00 类型系统颜色方案集成
  - Ghost 对象半透明渲染
- [ ] 实现 `Lv00ProofStatusQuantity` 证明状态标注
  - 箭头方向 + 长度编码证明状态
  - 状态标签文字渲染
- [ ] 实现 Structure-Quantity 关联的生命周期管理

### 3.3 第三阶段：可视化观察器模式（P3-3）

- [ ] 实现 `Lv00VisualizationObserver` 组件
  - 弹出式窗口 / 坞接式面板 切换
  - 不绑架主证明交互流的独立控件
- [ ] 实现多种更新策略
  - 手动更新（Refresh 按钮）
  - 提交时自动更新（`on_commit_step` 钩子）
  - 连续帧循环（动画/演示模式）
- [ ] 实现交互式元素
  - 点击几何对象 → 跳转到对应证明步骤
  - 悬停显示约束详情 tooltip
  - 拖拽旋转/缩放/平移 3D 视图
- [ ] 实现渲染质量与性能的平衡配置

### 3.4 第四阶段：轻量集成与构建（P3-4）

- [ ] 在 Lv-00 Web 前端中集成可视化渲染器
  - 选择 Three.js 或 Babylon.js 作为 WebGL 渲染引擎
  - 实现 Lv-00 数据格式到渲染器顶点格式的转换桥
- [ ] 实现头文件风格的模块化接口
  - `#include "lv00/visualization/geometry_view.h"`
  - 无需复杂构建配置，通过 ESM import 引入
- [ ] 实现渲染器独立于证明引擎的松耦合架构
  - 渲染器不依赖证明引擎的任何内部状态
  - 仅通过 `ConstraintGraphSnapshot` 快照接口获取数据
- [ ] 编写可视化组件的集成测试和视觉回归测试

---

## 4. 设计决策与权衡

### 4.1 符号坐标 → 浮点数转换的精度策略

这是 Lv-00 与 Polyscope 最根本的差异：Lv-00 使用符号精确坐标（有理数/代数数），而 Polyscope 使用 IEEE 754 浮点数。对于可视化这一用途，完全的符号精度既不必要也不可行（GPU 不支持符号运算），因此需要设计保守的数值化策略：

| 源类型 | 转换策略 | 精度 | 适用场景 |
|:---|:---|:---|:---|
| 有理数（分数） | 分子 / 分母 → double | 精确（在双精度范围内） | 整数坐标、简单分数坐标 |
| 代数数（根式） | 区间逼近取中值 | ~15 位有效数字 | sqrt(2)、黄金比例等非有理数 |
| 纯符号（含自由变量） | 代入默认值/上次值 | 取决于代入值 | 含未定参数的多态构造 |
| 大整数/高精度 | 四倍精度（long double）或任意精度→截断 | 可配置 | 大规模几何问题 |

**转换管线设计**：

```
Lv-00 符号坐标
    ↓
[Lv00GeometryAdapter.numerize()]
    ├─ 有理数 → mpq_get_d() → double
    ├─ 代数数 → arb_get_interval() → [lo, hi] → (lo+hi)/2 → double
    └─ 符号   → sym_substitute(known_vars) → double
    ↓
浮点坐标数组（double[N*3]）
    ↓
[WebGL 渲染器]
    ├─ Three.js BufferGeometry
    └─ GPU 光栅化
    ↓
屏幕像素
```

**误差管理**：所有数值化操作记录的误差界存储在 `Lv00GeometryAdapter.mapping_table[].error_bound` 中，当用户点击渲染对象时，系统使用误差界进行反向查找，确定对应的是哪个约束图节点。

### 4.2 不接管控制流 vs Web 环境的适配

Polyscope 在桌面环境中通过独立 OpenGL 窗口实现不接管控制流。Lv-00 的 Web 环境对此提出独特挑战：

- **桌面 Polyscope**：`polyscope::show()` 启动独立窗口 + 帧循环，主程序继续运行
- **Web Lv-00**：浏览器单线程环境下，可视化渲染由 `requestAnimationFrame` 驱动，与 React/Angular 事件循环共享主线程

解决方案：Lv-00 的可视化渲染器在 Web Worker 中运行 WebGL 计算（通过 OffscreenCanvas），确保不阻塞主线程的证明交互。主线程仅通过 `postMessage` 发送渲染更新指令。

### 4.3 与已有 JSXGraph 借鉴的关系

Lv-00 已有 `jsxgraph_interactive_geometry.md` 参考文档，JSXGraph 提供 2D 交互式几何的精确数位。Polyscope 与 JSXGraph 在 Lv-00 中分工：

| 特性 | JSXGraph（2D 精确几何） | Polyscope（3D 可视化） |
|:---|:---|:---|
| 坐标类型 | 符号精确坐标（Convex Hull 方法） | 浮点近似坐标 |
| 维度 | 2D 平面几何 | 2D/3D 网格几何 |
| 渲染引擎 | Canvas/SVG（2D） | WebGL（3D GPU） |
| 交互模式 | 主动编辑（鼠标拖拽构造点） | 被动观察（旋转/缩放查看） |
| Lv-00 角色 | 几何构造编辑器（前台交互） | 约束分析可视化器（后台分析） |

两者互补：JSXGraph 负责用户在构造几何时的精确交互，Polyscope 负责证明过程中约束状态的宏观可视化。

---

## 5. 补充：Polyscope 的轻量集成模式

### 5.1 头文件 + CMake 集成

Polyscope 的集成极其简单，仅需头文件和 CMake：

```cmake
# CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
  polyscope
  GIT_REPOSITORY https://github.com/nmwsharp/polyscope.git
)
FetchContent_MakeAvailable(polyscope)
target_link_libraries(my_app PUBLIC polyscope)
```

```cpp
// 使用：一个头文件搞定全部功能
#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

polyscope::init();
auto ps_mesh = polyscope::registerSurfaceMesh("mesh", V, F);
ps_mesh->addVertexScalarQuantity("curvature", curv_data);
polyscope::show();
```

Lv-00 对应的 Web 模块集成风格：

```javascript
// Lv-00 几何可视化模块：ESM 导入，零配置
import { Lv00GeometryView } from '@lv00/geometry-view';

const view = new Lv00GeometryView('#viz-container', {
  updateStrategy: 'onCommit',
  renderQuality: 'high',
  backgroundColor: '#1a1a2e'
});

// 注册几何 Structure
view.registerPointCloud('free_points', points);
view.registerCurveNetwork('constraint_edges', edges);

// 叠加约束满足度 Quantity
view.addScalarQuantity('free_points', 'constraint_heat', satisfactionData, {
  colormap: 'RdYlGn',
  valueRange: [0, 1]
});

// 可视化窗口独立运行，不绑架主交互流
view.show();  // 非阻塞，仅弹出/嵌入窗口
```

### 5.2 渲染表面的材质与光照

Polyscope 支持多种材质和光照模式，Lv-00 可视化中的对应配置：

```c
/**
 * @brief Lv-00 几何渲染材质配置 —— 借鉴 Polyscope 材质系统
 */
typedef struct {
    /** 材质类型 */
    enum {
        MATERIAL_WAX,            /**< 蜡质（柔和高光，适合展示曲面） */
        MATERIAL_FLAT,           /**< 平坦（无高光，适合展示颜色数据） */
        MATERIAL_MUD,            /**< 泥质（粗糙表面，适合展示标量函数） */
        MATERIAL_SMOOTH,         /**< 光滑（Blinn-Phong 高光，适合展示法向量） */
        MATERIAL_GHOST           /**< 鬼影材质（半透明+发光边缘，适合 Ghost 几何） */
    } material_type;

    /** 光照配置 */
    struct {
        float ambient_intensity;  /**< 环境光强度 [0,1] */
        float diffuse_intensity;  /**< 漫反射强度 [0,1] */
        float specular_intensity; /**< 高光强度 [0,1] */
    } lighting;

    /** 透明度 */
    float opacity;               /**< 全局透明度 [0,1] */
    bool backface_culling;       /**< 是否背面剔除 */
} Lv00RenderMaterial;
```

---

## 6. 参考资源

- Polyscope 官方仓库：https://github.com/nmwsharp/polyscope
- Polyscope 官方文档：https://polyscope.run/
- Nicholas Sharp et al., "Polyscope: A Rapid-Prototyping 3D Viewer for Scientific Computing", 2022
- Polyscope 源码中的 Structure/Quantity 架构：`include/polyscope/structure.h` 和 `include/polyscope/quantity.h`
- 与 Lv-00 相关的已有借鉴文档：
  - `jsxgraph_interactive_geometry.md` —— 2D 交互式几何构造（与 Polyscope 互补）
  - `libigl_header_only_api.md` —— libigl 的几何数据处理 API 参考
  - `libfive_frep_modeling.md` —— F-Rep 函数表示的渲染策略参考
  - `mermaidjs_diagram_rendering.md` —— Web 前端图表渲染架构参考
