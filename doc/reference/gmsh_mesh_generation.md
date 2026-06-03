# Gmsh 三维有限元网格生成器参考文档

> 面向 Lv-00 几何可视化与网格生成模块的 Gmsh 技术调研与借鉴方案

---

## 1. 项目概述

### 1.1 简介

[Gmsh](https://gitlab.onelab.info/gmsh/gmsh) 是由 Christophe Geuzaine 和 Jean-François Remacle 开发的开源三维有限元网格生成器，内置 CAD 引擎和后处理器。Gmsh 项目始于 1996 年，至今已持续发展近 30 年，是全球工程仿真领域最广泛使用的开源网格生成工具之一。

Gmsh 的独特之处在于它集成了四个通常需要不同软件完成的功能：

1. **几何建模**：通过内置的 CAD 引擎（基于 OpenCASCADE 或自带几何内核）构造参数化的几何体
2. **网格生成**：对几何模型自动生成 1D（线段）、2D（三角形/四边形）、3D（四面体/六面体）网格
3. **自适应细化**：基于误差估计的 h-refinement（网格单元细分）策略
4. **后处理**：对求解结果进行可视化渲染和数据分析

Gmsh 围绕四个核心模块构建：

```
┌─────────────────────────────────────────────────┐
│                  Gmsh 系统架构                     │
├───────────────┬──────────────┬───────────────────┤
│   Geometry    │    Mesh      │    Post-          │
│   (几何建模)   │  (网格生成)   │  Processing       │
│               │              │  (后处理)          │
├───────────────┴──────────────┴───────────────────┤
│                Gmsh API / .geo 脚本解析器          │
├──────────────────────────────────────────────────┤
│       OpenCASCADE (可选)  /  自带几何内核          │
├──────────────────────────────────────────────────┤
│       OpenGL / FLTK (GUI 渲染)                    │
└──────────────────────────────────────────────────┘
```

### 1.2 Gmsh 脚本语言

Gmsh 提供了一种声明式的几何脚本语言（.geo 文件），通过整数标签和简洁语法定义几何模型。核心语法模式：`Point(1) = {0, 0, 0}; Line(1) = {1, 2}; Curve Loop(1) = {1,2,3,4}; Plane Surface(1) = {1};`。每个几何实体获得唯一标签，后续操作通过标签引用建立拓扑关系。声明顺序隐含构造依赖，但实际求值是惰性的——只有需要显示或网格化时才计算几何。

### 1.3 技术栈

| 层次 | 语言 | 说明 |
|------|------|------|
| 核心引擎 | C++ | 约 500,000+ 行，包含几何内核、网格生成算法、求解器接口 |
| 脚本解析器 | C++（Flex/Bison） | 解析 .geo 脚本语言，生成几何模型 |
| 几何内核 | OpenCASCADE / 自带内核 | 可选链接：OpenCASCADE 提供更强的曲面建模能力 |
| GUI | FLTK + OpenGL | 轻量级跨平台 GUI，3D 渲染 |
| C API | C 包装层 | `gmsh.h` 和 `gmshc.h`，可供 C/C++/Python 调用 |
| Python API | `gmsh.py`（SWIG 生成） | 通过 SWIG 从 C++ 头文件自动生成 |
| Julia API | `Gmsh.jl` | Julia 语言的 Gmsh 绑定 |

### 1.4 许可证

Gmsh 采用 **GNU General Public License 2.0（GPL 2.0）**。这意味着：
- 修改和分发 Gmsh 源码需以 GPL 兼容许可证开源
- 链接 Gmsh 库的衍生程序也需开源

对于 Lv-00 而言，GPL 2.0 的限制意味着不能将 Gmsh 源码直接链接到 Lv-00 闭源内核中。但是，Gmsh 的设计理念、API 架构和脚本语言语法可以作为设计参考，独立实现。

### 1.5 关键统计

| 指标 | 数值 |
|------|------|
| C++ 源码行数 | 500,000+ |
| 支持的网格类型 | 1D（线段）、2D（三角形、四边形）、3D（四面体、六面体、棱柱、金字塔） |
| 支持的几何格式 | STEP、IGES、BREP、STL、OFF、OBJ、PLY、VTK 等 20+ 种 |
| 网格生成算法 | Delaunay、Frontal、HXT、MeshAdapt 等 |
| Python API 函数数 | 800+（几乎覆盖所有 Gmsh 功能） |

---

## 2. 核心借鉴点

### 2.1 声明式几何构造语法

**Gmsh 做法**

Gmsh .geo 脚本语言的核心是一种**声明式**语法——用户"声明"几何元素的存在和它们之间的关系，而非编写"如何构造"的逐步指令。每个几何实体获得一个唯一的整数标签（tag），后续操作通过引用标签来建立拓扑关系：

```
Point(1) = {0, 0, 0};     // 声明：点 1 存在于 (0,0,0)
Line(1) = {1, 2};         // 声明：线 1 连接点 1 和点 2
Curve Loop(1) = {1,2,3};  // 声明：回路 1 由线 1、2、3 按序围成
```

标签体系（从 1 开始的自增整数）构成 Gmsh 脚本的"名字空间"，每个几何操作都通过标签引用已存在的实体。声明的顺序隐含了构造的依赖关系，但实际求值是惰性的——只有需要显示或网格化时才真正计算几何。

**Lv-00 对应关系**

Lv-00 DSL 的几何构造语法可以借鉴 Gmsh 的声明式风格。不同的是，Lv-00 的几何元素需要区分"给定的"和"构造的"，并且涉及符号坐标而非纯数值：

| Gmsh 语法 | Lv-00 DSL 对应 |
|-----------|---------------|
| `Point(tag) = {x, y, z}` | `given Point A(x, y)` 或 `construct Point P from ...` |
| `Line(tag) = {p1, p2}` | `construct Segment s between(A, B)` |
| `Circle(tag) = {start, center, end}` | `construct Circle c by_arc(P_start, O, P_end)` |
| `Curve Loop(tag) = {lines[]}` | `construct Polygon tri vertices(A, B, C)` |
| `Plane Surface(tag) = {loop}` | `construct Region interior of(tri)` |
| `BooleanDifference{A}{B}` | 几何体的集合差（不在 DSL 中直接表达，而在约束图中用不等式约束实现） |

### 2.2 几何到网格的自动离散化管道

**Gmsh 做法**

Gmsh 最核心的设计是**几何到网格的自动转换管道**。用户只需定义几何模型（点、线、面），Gmsh 自动将其离散化为有限元网格：

```
几何模型 (.geo)              中间层                  网格输出
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Point(1)     │     │ 参数曲线采样  │     │ 节点坐标      │
│ Line(1)      │────→│ 边界恢复      │────→│ 单元连通性    │
│ Surface(1)   │     │ 网格生成器    │     │ .msh 文件     │
│ Volume(1)    │     │ 质量优化      │     │              │
└──────────────┘     └──────────────┘     └──────────────┘
                            │
                    ┌───────┴───────┐
                    │ 自适应细化     │
                    │ (h-refinement) │
                    └───────────────┘
```

转换管道的核心步骤：
1. **参数曲线/曲面采样**：将连续的几何实体在参数空间采样为一组离散点
2. **边界网格生成**：先在 1D 边、2D 面上生成网格
3. **体积网格生成**：基于边界网格，在体积内部生成四面体/六面体网格
4. **网格质量优化**：通过节点重定位、边翻转、面交换等操作改善单元质量
5. **边界恢复**：确保最终网格的单元面与原始几何边界吻合

**Lv-00 对应关系**

Lv-00 的几何构造以符号形式存在（`SymbolicCoord` + `ConstraintGraph`），这本质上是"符号几何"。借鉴 Gmsh 的离散化管道，Lv-00 可以实现符号几何到数值网格的可视化路径：

| Gmsh 管道阶段 | Lv-00 对应 | 实现说明 |
|--------------|-------------|---------|
| 参数曲线采样 | 符号坐标求值 → 数值坐标 | `symbolic_coord_to_double()` 将符号值转为双精度浮点 |
| 边界恢复 | 几何约束的数值验证 | 验证离散点的共线性、角度等近似满足几何约束 |
| 网格生成器 | 简单多边形三角剖分 | 对 Lv-00 几何体使用 Delaunay 三角剖分生成可视化网格 |
| 质量优化 | Laplacian 平滑 | 对可视化网格做简单平滑以改善显示效果 |
| .msh 输出 | SVG/Canvas 渲染 | 输出为前端可渲染的图形格式而非 .msh |

```
Lv-00 符号几何 → 数值网格的可视化管道：

  ConstraintGraph         符号求值 + 三角剖分           可视化
  ┌──────────────┐     ┌───────────────────┐     ┌──────────────┐
  │ 给定点坐标    │     │ 1. 符号→数值求值    │     │ SVG 路径     │
  │ 约束关系      │────→│ 2. 约束一致性检查   │────→│ Canvas 渲染  │
  │ 构造层级      │     │ 3. 多边形三角剖分   │     │ LaTeX TikZ   │
  │              │     │ 4. 网格平滑         │     │             │
  └──────────────┘     └───────────────────┘     └──────────────┘
```

### 2.3 OpenCASCADE 几何内核集成模式

**Gmsh 做法**

Gmsh 支持两种几何内核模式：
1. **内置内核（built-in）**：Gmsh 自带的轻量级几何引擎，支持点、线、样条、平面、拉伸体等基本几何体
2. **OpenCASCADE 内核**：链接外部 OpenCASCADE 库，获得工业级 CAD 能力（NURBS 曲面、布尔运算、倒角等）

两种内核通过统一的 `GModel` 抽象层切换，上层代码无需感知底层使用哪个内核：

```cpp
// Gmsh 几何内核抽象层（简化）
class GModel {
    std::vector<GVertex*> vertices;
    std::vector<GEdge*>   edges;
    std::vector<GFace*>   faces;
    std::vector<GRegion*> regions;
public:
    // 统一接口，内部根据内核类型分发
    virtual bool addVertex(int tag, double x, double y, double z);
    virtual bool addEdge(int tag, int p1, int p2);
    virtual bool addFace(int tag, const std::vector<int>& edgeTags);
    // ...
};
```

**Lv-00 对应关系**

Lv-00 在处理复杂的几何约束（如 NURBS 曲线、隐式曲面）时，可能超出自身有理数多项式系统的表达能力。借鉴 Gmsh 的外核集成模式，Lv-00 可以设计一个几何库抽象层，将超出能力范围的几何运算委托给外部库：

| Gmsh 内核模式 | Lv-00 几何后端抽象 |
|--------------|-------------------|
| 内置内核（built-in） | 内置符号几何引擎（`SymbolicCoord` + 有理数多项式） |
| OpenCASCADE 内核 | 外部数值几何库（CGAL / OpenCASCADE） |
| `GModel` 抽象层 | `Lv00GeometryBackend` 虚函数表 |
| `addVertex/addEdge/addFace` | `lv00_geom_create_point/line/region` 统一 API |
| 运行时切换 | 构造函数参数选择后端 |

```c
// Lv-00 几何后端抽象层设计
typedef enum {
    LV00_GEOM_BUILTIN,       // 内置符号几何引擎
    LV00_GEOM_CGAL,          // CGAL 精确计算几何库
    LV00_GEOM_OPENCASCADE,   // OpenCASCADE 工业 CAD 内核
} Lv00GeometryBackendType;

typedef struct Lv00GeometryBackend {
    Lv00GeometryBackendType type;
    const char* name;
    Lv00RetCode (*create_point)(struct Lv00GeometryBackend* g,
                                 int tag, double x, double y, double z);
    Lv00RetCode (*create_segment)(struct Lv00GeometryBackend* g,
                                   int tag, int p1_tag, int p2_tag);
    Lv00RetCode (*create_circle_by_arc)(struct Lv00GeometryBackend* g,
                                         int tag, int start, int center, int end);
    Lv00RetCode (*create_polygon)(struct Lv00GeometryBackend* g,
                                   int tag, const int* vertex_tags, int n);
    Lv00RetCode (*boolean_union)(struct Lv00GeometryBackend* g,
                                  int result_tag, const int* a_tags, int na);
    Lv00RetCode (*boolean_intersection)(struct Lv00GeometryBackend* g,
                                         int result_tag, const int* a_tags, int na);
    Lv00RetCode (*tessellate)(struct Lv00GeometryBackend* g,
                               int tag, double** vertices, int* n_vert,
                               int** triangles, int* n_tri);
    void        (*destroy)(struct Lv00GeometryBackend* g);
} Lv00GeometryBackend;
```

### 2.4 几何实体标签体系

**Gmsh 做法**

Gmsh 使用统一的整数标签（tag）系统标识所有几何实体。标签维度从 0 到 3 分别对应：

| 维度 | 实体类型 | 标签范围示例 | Gmsh 命令示例 |
|------|---------|-------------|--------------|
| 0 | Point（点） | 1, 2, 3, ... | `Point(1) = {0, 0, 0}` |
| 1 | Line / Spline / Circle（线） | 1, 2, 3, ... | `Line(1) = {1, 2}` |
| 2 | Surface（面） | 1, 2, 3, ... | `Plane Surface(1) = {1}` |
| 3 | Volume（体） | 1, 2, 3, ... | `Volume(1) = {1}` |

跨维度引用形成了几何的层级拓扑结构：Volume 由 Surface 包围，Surface 由 Curve Loop 界定，Curve Loop 由 Line 首尾相连，Line 由 Point 定义端点。

**Lv-00 对应关系**

Lv-00 的 `GeomNode` 体系可以借鉴 Gmsh 的维度层次设计，为不同类型的几何元素建立清晰的类型分层：

| Gmsh 标签维度 | Lv-00 GeomNode 类型 | C 类型定义 | 说明 |
|--------------|--------------------|-----------|------|
| Point (0D) | `GEOM_NODE_POINT` | `GeomPoint { SymbolicCoord x, y; }` | 符号坐标点 |
| Line (1D) | `GEOM_NODE_SEGMENT` | `GeomSegment { GeomPoint *a, *b; }` | 线段 |
| Line (1D) | `GEOM_NODE_CIRCLE` | `GeomCircle { GeomPoint *center; SymbolicCoord radius; }` | 圆 |
| Surface (2D) | `GEOM_NODE_POLYGON` | `GeomPolygon { GeomPoint** vertices; int n; }` | 多边形 |
| Surface (2D) | `GEOM_NODE_REGION` | `GeomRegion { GeomNode** boundary; int n; }` | 任意闭合区域 |
| Volume (3D) | `GEOM_NODE_MESH` | `GeomMesh { GeomNode** faces; int n; }` | 三维网格 |

```c
/**
 * @brief Lv-00 几何实体类型层次（借鉴 Gmsh 标签体系）
 *
 * 文件: include/lv00/geom_node.h
 */

typedef enum {
    GEOM_DIM_0D = 0,  /* 点维度 */
    GEOM_DIM_1D = 1,  /* 线维度 */
    GEOM_DIM_2D = 2,  /* 面维度 */
    GEOM_DIM_3D = 3,  /* 体维度 */
} GeomDimension;

typedef enum {
    /* 0D */
    GEOM_NODE_POINT,          /* 自由点 */
    GEOM_NODE_POINT_ON_CURVE, /* 曲线上的点 */

    /* 1D */
    GEOM_NODE_SEGMENT,        /* 线段 */
    GEOM_NODE_RAY,            /* 射线 */
    GEOM_NODE_LINE,           /* 无限直线 */
    GEOM_NODE_CIRCLE,         /* 圆 */
    GEOM_NODE_ARC,            /* 圆弧 */

    /* 2D */
    GEOM_NODE_POLYGON,        /* 多边形 */
    GEOM_NODE_REGION,         /* 任意闭合区域 */

    /* 3D（Lv-00 当前主要关注 2D，预留 3D 接口） */
    GEOM_NODE_MESH,           /* 三维网格容器 */
} GeomNodeType;

typedef struct GeomNode {
    int            tag;        /* 唯一标签（借鉴 Gmsh） */
    GeomNodeType   type;
    GeomDimension  dim;
    const char*    name;       /* 可选的语义名称 */
    union {
        struct { SymbolicCoord x, y; } point;
        struct { int p1_tag, p2_tag; } segment;
        struct { int center_tag; SymbolicCoord radius; } circle;
        struct { int* vertex_tags; int n; } polygon;
        struct { int* boundary_tags; int n; } region;
        /* ... */
    } data;
} GeomNode;
```

### 2.5 物理组的语义标注

**Gmsh 做法**

Gmsh 区分了两种"组"概念：
- **几何实体**：Point(1)、Line(2) 等，定义几何的拓扑结构
- **物理组**（Physical Group）：Physical Point、Physical Line、Physical Surface、Physical Volume，对几何实体进行**语义标注**

物理组的作用是将几何实体按物理/工程意义分组——例如标注"入口"、"出口"、"壁面"、"发热面"等边界条件：

```
// .geo 脚本中的物理组
Physical Surface("inlet", 101)  = {1, 2, 3};    // "入口"面
Physical Surface("outlet", 102) = {4, 5};       // "出口"面
Physical Surface("wall", 103)   = {6, 7, 8, 9}; // "壁面"
Physical Point("sensor", 201)   = {10, 11};     // "传感器"位置
```

在导出网格时，物理组信息被保留，使得后续的有限元求解器可以为不同物理组指派不同的边界条件。

**Lv-00 对应关系**

Lv-00 的约束图节点可以借鉴物理组的语义标注思想。在几何证明问题中，不同类型的约束节点服务于不同的证明目的：

| Gmsh 物理组概念 | Lv-00 语义组概念 | 作用 |
|----------------|-----------------|------|
| Physical Point | GivenPoint（给定点） / ConstructedPoint（构造点） | 区分已知坐标与待求坐标 |
| Physical Line | Hypothesis（前提线段） / Goal（目标线段） | 区分已知条件与待证明关系 |
| Physical Surface | AxiomRegion（公理区域） / GoalRegion（目标区域） | 区分公理几何体与待求解几何体 |
| 边界条件标注 | ProofHint（证明提示组） | 将辅助构造与证明策略关联 |
| 材料属性标注 | CoordinateGroup（坐标群组） | 将共享坐标系变换的点归为一组 |

```c
/**
 * @brief Lv-00 语义组（借鉴 Gmsh 物理组概念）
 *
 * 将约束图节点按证明语义分组标注，
 * 便于证明引擎区分给定/构造/目标等不同角色。
 *
 * 文件: include/lv00/semantic_group.h
 */

typedef enum {
    SEMGROUP_GIVEN,           /* 给定元素（已知条件） */
    SEMGROUP_CONSTRUCTED,     /* 构造元素（辅助线/点） */
    SEMGROUP_GOAL,            /* 待证明目标 */
    SEMGROUP_AXIOM,           /* 公理/定理引用 */
    SEMGROUP_DEDUCED,         /* 已推导出的中间结论 */
    SEMGROUP_HINT,            /* 证明策略提示 */
} SemanticGroupType;

typedef struct Lv00SemanticGroup {
    int              tag;         /* 组标签 */
    const char*      name;        /* 语义名称，如 "triangle_ABC" */
    SemanticGroupType type;
    int*             node_tags;   /* 包含的 GeomNode 标签列表 */
    int              n_nodes;
    int              dimension;   /* 组内元素的最高维度 */
} Lv00SemanticGroup;

/* 将语义组信息附加到约束图 */
Lv00RetCode lv00_semgroup_attach_to_graph(
    Lv00SemanticGroup* group,
    Lv00ConstraintGraph* graph);
```

### 2.6 自适应网格细化

**Gmsh 做法**

Gmsh 的 h-refinement（h-自适应细化）机制允许基于误差估计自动在误差较大的区域加密网格，在已足够精确的区域保持粗网格。这套机制的数学基础是后验误差估计（posteriori error estimation）：

```
自适应网格细化的循环：

  ┌─────────────────────────────────────┐
  │ 1. 在当前网格上求解 PDE / 评估指标   │
  ├─────────────────────────────────────┤
  │ 2. 计算每个单元的局部误差估计 η_e    │
  ├─────────────────────────────────────┤
  │ 3. 如果 Σ η_e < 全局容忍度 → 完成   │
  │    否则 → 标记 η_e 最大的单元       │
  ├─────────────────────────────────────┤
  │ 4. 对被标记的单元执行边分裂 / 面细分 │
  │    (1D: 分裂边, 2D: 细分三角形,     │
  │     3D: 分裂四面体)                  │
  ├─────────────────────────────────────┤
  │ 5. 返回步骤 1                       │
  └─────────────────────────────────────┘
```

**Lv-00 对应关系**

Lv-00 的几何精度自适应提升可以借鉴 Gmsh 的自适应细化思想。在几何求解中，"误差"对应的是约束违反度或数值精度不足：

| Gmsh 自适应 | Lv-00 几何精度自适应 |
|-----------|-------------------|
| 求解 PDE 评估 | 评估几何约束在当前候选解处的违反度 |
| 局部误差估计 η_e | 每个几何构造点的坐标精度估计（如 Grobner 基多项式的余式范数） |
| 标记最大误差单元 | 标记精度最差的构造点 / 约束 |
| 边分裂（1D） | 在线段上增加采样点以提高曲线的多边形逼近精度 |
| 三角形细分（2D） | 在约束冲突区域增加辅助构造以细化局部几何 |
| 全局容忍度 | A 计划（精确）vs B 计划（近似）的切換阈值 |

```c
/**
 * @brief Lv-00 自适应精度提升（借鉴 Gmsh h-refinement）
 *
 * 当几何约束的数值验证超出容忍度时，
 * 在误差最大的区域添加局部精细化构造。
 *
 * 文件: src/geometry_refinement.c
 */

typedef struct Lv00GeometryRefiner {
    Lv00ConstraintGraph* graph;
    double               global_tol;     /* 全局精度容忍度 */
    int                  max_refinements;/* 最大细化轮数 */
    int                  current_round;
} Lv00GeometryRefiner;

/**
 * 一轮自适应细化
 *
 * 1. 评估所有约束的当前违反度
 * 2. 找出违反度最大的构造点
 * 3. 该点附近增加辅助构造（如局部放大的辅助线）
 * 4. 重新求解局部子系统
 * 5. 重复直到全局精度满足或达到最大轮数
 */
Lv00RetCode lv00_geom_refine(Lv00GeometryRefiner* refiner,
                              Lv00RefineResult* result);

/**
 * 评估给定构造点的局部精度
 *
 * 通过求解包含该点的所有约束构成的局部子系统的 Grobner 基，
 * 计算该点坐标的最大可能变动范围。
 */
double lv00_geom_point_accuracy(const Lv00GeometryRefiner* refiner,
                                 int point_tag);
```

### 2.7 核心借鉴点对照总表

| 序号 | Gmsh 概念 | Gmsh 实现位置 | Lv-00 对应模块 | Lv-00 借鉴方式 |
|------|----------|--------------|---------------|---------------|
| 1 | 声明式 .geo 脚本语法 | `Parser.l`、`Parser.y`（Flex/Bison） | Lv-00 几何 DSL | 声明式语法 + 整数标签引用体系 |
| 2 | 几何→网格离散化管道 | `mesh/` 目录（meshGFace、meshGRegion） | 符号几何→可视化网格管道 | 符号求值→三角剖分→渲染 |
| 3 | OpenCASCADE 集成 | `GModelIO_OCC`、`gmshOCC` 命名空间 | 几何后端抽象层 | 内置符号引擎 + 外部数值内核可切换 |
| 4 | 整数标签体系（0D-3D） | `GModel`、`GEntity` 类层次 | `GeomNode` 类型层次 | Point/Segment/Polygon/Region/Mesh 分层 |
| 5 | 物理组语义标注 | `PhysicalGroup` 管理 | `SemanticGroup` 语义组 | Given/Constructed/Goal/Hint 分类标注 |
| 6 | h-refinement 自适应 | `meshGEdge`、`meshGFace` 细化算法 | 几何精度自适应提升 | 约束违反度驱动局部辅助构造添加 |

---

## 3. Lv-00 映射方案

### 3.1 架构概览

Lv-00 通过借鉴 Gmsh 的架构模式，构建从符号几何到可视化渲染的完整路径：

```
.lvgeo DSL ──→ 语义组标注 (Given/Constructed/Goal/Hint) ──→ 外部几何后端 (CGAL/OpenCASCADE, 可选)
                              │
                              ▼
  GeomNodeManager: Point(0D) | Segment(1D) | Polygon(2D) | Region(2D)
                              │
                              ▼
  离散化管道: 符号坐标求值 → 约束一致性检查 → Delaunay三角剖分 → 自适应精度提升
                              │
                              ▼
  渲染输出层: SVG / Canvas 2D / WebGL 网格
```

### 3.2 声明式几何 DSL 的语法设计

借鉴 Gmsh .geo 脚本的声明式风格，为 Lv-00 设计几何描述语法。核心思想：用 `given` 声明已知元素，用 `construct` 声明辅助构造，用 `group` 进行语义标注，用 `goal` 声明证明目标。

```
// 示例：Lv-00 三角形重心定理的 .lvgeo 描述
given Point A = (0, 0); given Point B = (10, 0); given Point C = (6, 8)
construct Point M_BC = midpoint of BC
construct Segment med_A from(A) to(M_BC)
group "triangle_ABC" as Given contains(A, B, C)
group "medians" as Constructed contains(med_A, med_B, med_C)
goal "concurrent_medians" { statement: concurrent(med_A, med_B, med_C) }
```

对应的解析器核心接口（递归下降手写解析器，避免依赖 Flex/Bison）：

```c
typedef struct Lv00DSLParser {
    const char* source; int pos;
    Lv00GeometryDSL* dsl;   /* 输出：解析结果 */
} Lv00DSLParser;
Lv00RetCode lv00_dsl_parse(Lv00DSLParser* parser);
Lv00RetCode lv00_dsl_parse_file(Lv00DSLParser* parser, const char* path);
```

### 3.3 符号几何到可视化网格的离散化

借鉴 Gmsh 的几何→网格管道，实现 Lv-00 的符号→数值可视化。核心数据结构 `Lv00RenderMesh`（顶点坐标数组 + 三角形索引 + 线段索引 + 顶点标签），处理流程为：(1)遍历所有 GeomNode 并做符号→数值求值；(2)对面类型用 Delaunay 三角剖分；(3)对线类型生成线段列表；(4)可选 Laplacian 平滑；(5)收集标签。

```c
Lv00RetCode lv00_geom_to_render_mesh(const GeomNode** nodes, int n_nodes,
                                      Lv00RenderMesh* output);
Lv00RetCode lv00_verify_render_constraints(const Lv00RenderMesh* mesh,
    const Lv00ConstraintGraph* graph, double tolerance);
Lv00RetCode lv00_delaunay_triangulate(const double* vertices, int n,
                                       Lv00RenderMesh* output);
```

### 3.4 GeomNode 类型层次与标签管理器

借鉴 Gmsh 的整数标签管理体系，为 Lv-00 设计几何实体标签系统。核心结构 `Lv00GeomNodeManager`：按维度独立维护标签→实体映射表（HashMap），每维度独立自增标签，并通过名称→标签反向索引支持按名查找。

```c
int lv00_geom_register_point(Lv00GeomNodeManager* mgr,
    const char* name, SymbolicCoord x, SymbolicCoord y);
GeomNode* lv00_geom_get_by_tag(Lv00GeomNodeManager* mgr, int tag);
GeomNode* lv00_geom_get_by_name(Lv00GeomNodeManager* mgr, const char* name);
```

### 3.5 渲染输出层的实现

借鉴 Gmsh 的多格式导出能力，为 Lv-00 设计多种渲染输出格式。支持 SVG（矢量图含标签和交互式锚点）、LaTeX TikZ（`\draw`/`\node` 命令）、Canvas 2D JSON 指令（供前端消费）。

```c
typedef enum { LV00_RENDER_SVG, LV00_RENDER_CANVAS,
               LV00_RENDER_LATEX_TIKZ, LV00_RENDER_JSON } Lv00RenderFormat;
Lv00RetCode lv00_render_export(const Lv00RenderMesh* mesh,
                                Lv00RenderFormat format, const char* path);
Lv00RetCode lv00_render_svg(const Lv00RenderMesh* mesh, FILE* output);
Lv00RetCode lv00_render_tikz(const Lv00RenderMesh* mesh, FILE* output);
```

---

## 4. 实现路线图

### 4.1 分阶段规划

| 阶段 | 名称 | 预计工期 | 产出物 | 依赖 |
|------|------|----------|--------|------|
| Phase 1 | GeomNode 类型体系 + 标签管理器 | 2-3 周 | `geom_node.h/c`、`geom_node_manager.c` | 无 |
| Phase 2 | 几何 DSL 解析器 | 3-4 周 | `geometry_dsl_parser.c`、.lvgeo 语法规范 | Phase 1 |
| Phase 3 | 符号→渲染网格离散化管道 | 3-4 周 | `geom_to_render.c`、Delaunay 三角剖分 | Phase 1 |
| Phase 4 | 渲染输出 + 自适应精度提升 | 2-3 周 | `render_output.c`、`geometry_refinement.c` | Phase 3 |

### 4.2 Phase 1：GeomNode 类型体系 + 标签管理器

**目标**：建立完整的几何实体类型层次和标签管理体系。

**任务清单**：

1. 定义 `GeomNodeType` 枚举和 `GeomDimension` 枚举（`include/lv00/geom_node.h`）
2. 定义 `GeomNode` 联合体结构体（支持 Point/Segment/Circle/Polygon/Region）
3. 实现 `Lv00GeomNodeManager` 标签管理器（`src/geom_node_manager.c`）
4. 实现 `lv00_geom_register_*()` 系列注册函数
5. 实现 `lv00_geom_get_by_tag/name()` 查询函数
6. 编写单元测试：注册 10+ 几何实体并验证标签唯一性

**关键数据结构**：

```c
/* 每个维度的标签空间独立，但标签编号模式统一 */
#define LV00_TAG_POINT_MIN    1000   /* 点标签从 1000 开始 */
#define LV00_TAG_SEGMENT_MIN  2000   /* 线段标签从 2000 开始 */
#define LV00_TAG_CIRCLE_MIN   3000
#define LV00_TAG_POLYGON_MIN  4000
#define LV00_TAG_REGION_MIN   5000
```

**验收标准**：
- 能正确注册、查询、删除各种类型的几何实体
- 标签生成器确保同一维度内标签不重复

### 4.3 Phase 2：几何 DSL 解析器

**目标**：实现 .lvgeo 文件的解析，生成 GeomNode 列表和 SemanticGroup 列表。

**任务清单**：

1. 编写 .lvgeo 语法规范文档（EBNF 定义）
2. 实现词法分析器（`lv00_dsl_lexer`）
3. 实现递归下降语法分析器（`lv00_dsl_parse`）
4. 支持 `given` / `construct` / `group` / `goal` / `visualization` 五个顶级声明
5. 支持表达式：中点、垂足、交点、角平分线等几何构造
6. 实现 `lv00_dsl_parse_file()` 文件解析入口
7. 编写解析器的单元测试和若干个 .lvgeo 示例文件

**验收标准**：
- 能正确解析三角形重心定理的完整 .lvgeo 描述
- 错误位置的语法错误能给出有意义的报错信息（行号+期望的 token）

### 4.4 Phase 3：符号→渲染网格离散化管道

**目标**：实现从符号几何体到可渲染三角形网格的完整转换。

**任务清单**：

1. 实现 `lv00_geom_to_render_mesh()` 主函数
2. 集成 Bowyer-Watson 算法实现 Delaunay 三角剖分
3. 实现约束一致性检查（`lv00_verify_render_constraints()`）
4. 实现 Laplacian 平滑（改善三角形质量）
5. 对不同的几何体类型（多边形、圆、圆弧）实现专门的离散化策略
6. 编写集成测试：三角形、五边形、圆的离散化质量

**验收标准**：
- 三角形/四边形/圆的离散化三角形网格质量（最小角度 > 20 度）
- 点标签在离散化后能正确定位

### 4.5 Phase 4：渲染输出 + 自适应精度提升

**目标**：实现多格式渲染输出和基于约束违反度的自适应精度提升。

**任务清单**：

1. 实现 SVG 渲染输出（`lv00_render_svg()`）
2. 实现 LaTeX TikZ 渲染输出（`lv00_render_tikz()`）
3. 实现 Canvas 2D JSON 指令输出（供前端使用）
4. 实现 `lv00_geom_refine()` 自适应精度提升
5. 实现 `lv00_geom_point_accuracy()` 精度评估
6. 对复杂几何图形（星形、嵌套图形）编写渲染测试

**验收标准**：
- SVG 输出能在标准浏览器中正确显示
- TikZ 输出能在 LaTeX 文档中编译通过
- 自适应精度的三角形逼近误差 < 1e-6

---

## 5. 附录

### 5.1 关键资源

| 资源 | 链接 |
|------|------|
| Gmsh 官方网站 | https://gmsh.info |
| Gmsh GitLab 仓库 | https://gitlab.onelab.info/gmsh/gmsh |
| Gmsh 参考手册 | https://gmsh.info/doc/texinfo/gmsh.html |
| Gmsh Python API 教程 | https://gmsh.info/doc/texinfo/gmsh.html#Tutorial |
| Gmsh 论文（Geuzaine & Remacle, 2009） | International Journal for Numerical Methods in Engineering, 79(11): 1309-1331 |
| OpenCASCADE 官网 | https://www.opencascade.com |
| Bowyer-Watson Delaunay 算法 | 计算几何经典文献 |
| Triangle 网格生成库 | https://www.cs.cmu.edu/~quake/triangle.html |
| Gmsh 自适应细化相关论文 | "Efficient mesh optimization schemes based on Optimal Delaunay Triangulations" |

### 5.2 Gmsh 与同类型工具对比

| 工具 | 类型 | 语言 | 许可证 | 脚本语言 | 3D 支持 | API 语言 |
|------|------|------|--------|---------|--------|---------|
| Gmsh | 网格生成器 + CAD | C++ | GPL 2.0 | 内置 .geo | 是 | C/C++/Python/Julia |
| Triangle | 2D Delaunay 三角剖分 | C | 免费（非商业） | 命令行 | 否 | C |
| TetGen | 3D Delaunay 四面体剖分 | C++ | AGPL 3.0 | 命令行 | 是 | C++ |
| Netgen | 网格生成器 + CAD | C++ | LGPL 2.1 | 内置 | 是 | C++/Python |
| CGAL | 计算几何算法库 | C++ | GPL 3.0 / 商业 | 无（C++ 模板库） | 是 | C++/Python |
| OpenMesh | 网格数据结构 | C++ | BSD 3-Clause | 无 | 仅表面 | C++/Python |

### 5.3 Gmsh 核心 API 速查（Python）

```python
import gmsh

gmsh.initialize()

# ---------- 几何 ----------
p1 = gmsh.model.geo.addPoint(0, 0, 0)
p2 = gmsh.model.geo.addPoint(1, 0, 0)
l1 = gmsh.model.geo.addLine(p1, p2)
cl = gmsh.model.geo.addCurveLoop([l1, l2, l3])
s1 = gmsh.model.geo.addPlaneSurface([cl])
gmsh.model.geo.synchronize()

# ---------- 物理组 ----------
gmsh.model.addPhysicalGroup(2, [s1], tag=101)
gmsh.model.setPhysicalName(2, 101, "my_surface")

# ---------- 网格 ----------
gmsh.model.mesh.generate(2)  # 生成 2D 网格

# ---------- 导出 ----------
gmsh.write("output.msh")
gmsh.write("output.stl")

# ---------- 自适应 ----------
gmsh.model.mesh.refine()     # 全局均匀细化
gmsh.model.mesh.setOrder(2)  # 提升为二次单元

gmsh.finalize()
```

### 5.4 术语对照表

| 英文术语 | 中文翻译 | 首次出现章节 |
|----------|----------|-------------|
| Mesh Generation | 网格生成 | 1.1 |
| Finite Element Method (FEM) | 有限元法 | 1.1 |
| CAD Engine | CAD 引擎 | 1.1 |
| Post-Processing | 后处理 | 1.1 |
| Declarative Syntax | 声明式语法 | 2.1 |
| Curve Loop | 曲线回路 | 1.2 |
| Tessellation / Triangulation | 三角剖分 | 3.3 |
| Delaunay Triangulation | Delaunay 三角剖分 | 3.3 |
| h-refinement | h-自适应细化 | 2.6 |
| Physical Group | 物理组 | 2.5 |
| Boundary Recovery | 边界恢复 | 2.2 |
| Laplacian Smoothing | Laplacian 平滑 | 3.3 |
| OpenCASCADE | OpenCASCADE 几何内核 | 2.3 |
| Posteriori Error Estimation | 后验误差估计 | 2.6 |
| GPL (GNU General Public License) | GNU 通用公共许可证 | 1.4 |
| NURBS | 非均匀有理 B 样条 | 2.3 |
| Semantic Annotation | 语义标注 | 2.5 |

---

> 文档版本: v1.0 | 最后更新: 2026-05-24 | 作者: Lv-00 开发团队
