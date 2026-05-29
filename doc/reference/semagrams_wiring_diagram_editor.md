# Lv-00 参考落地设计文档：Semagrams.jl 接线图编辑器

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: Semagrams.jl (github.com/AlgebraicJulia/Semagrams.jl) —— 基于范畴论的图形化接线图编辑器
> **目标**: 借鉴 Semagrams.jl 的拖拽式接线图编辑范式、类型检查的连接端口和多层嵌套图结构，映射到 Lv-00 的 Web GUI 几何构造面板与 `func_block.h` 嵌套块系统

---

## 目录

1. [Semagrams.jl 项目概述与 Lv-00 借鉴动机](#1-semagramsjl-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点一：拖拽式接线图编辑范式](#2-核心借鉴要点一拖拽式接线图编辑范式)
3. [核心借鉴要点二：类型检查的端口连接](#3-核心借鉴要点二类型检查的端口连接)
4. [核心借鉴要点三：多层嵌套图结构](#4-核心借鉴要点三多层嵌套图结构)
5. [Lv-00 映射方案：Web GUI 构造面板设计](#5-lv-00-映射方案web-gui-构造面板设计)
6. [FuncBlock 嵌套块系统映射](#6-funcblock-嵌套块系统映射)
7. [实现路线图](#7-实现路线图)
8. [关键映射表](#8-关键映射表)

---

## 1. Semagrams.jl 项目概述与 Lv-00 借鉴动机

### 1.1 Semagrams.jl 是什么

Semagrams.jl 是 AlgebraicJulia 组织开发的一个**基于范畴论的图形化接线图编辑器**。它将抽象数学中的范畴论概念（对象、态射、交换图）转化为可视化的拖拽式编辑体验。用户可以通过拖拽"盒子"（代表态射、函数、变换）和连接"线缆"（代表对象、类型、状态）来构造复杂的层次化图示。

```
Semagrams.jl 核心概念:
─────────────────────────────────────────────────
盒子 (Box)      →  Morphism / Function / Transformation
                   有输入端口（input ports）和输出端口（output ports）
线缆 (Wire)      →  Object / Type / State
                   连接一个盒子的输出端口到另一个盒子的输入端口
端口 (Port)      →  类型注解（typed annotation）
                   端口有类型标签，只有类型兼容的端口才能连接
嵌套 (Nesting)   →  盒子内部可包含另一个完整的接线图
                   支持任意深度的层次嵌套
```

### 1.2 Lv-00 借鉴动机

Lv-00 的几何构造本质上是一个"接线图"——点、线、圆等实体（对象）通过约束（线缆）相互连接。每个几何构造块（如三角形、中垂线、角平分线）可以视为一个"态射"——输入若干几何对象，输出新的几何对象。Semagrams.jl 的类型化拖拽编辑为 Lv-00 的 Web GUI 构造面板提供了天然的交互范式：

| 借鉴方向 | Semagrams.jl 特性 | Lv-00 现有基础 | 差距 |
|:---|:---|:---|:---|
| **拖拽式构造** | 盒子拖入画布 + 端口连接 | `FuncBlock` 函数块注册表 | 缺可视化拖拽构造界面 |
| **类型检查连接** | 端口类型不兼容则自动拒绝连接 | `type_check_port_compatibility()` | 缺 Web 前端实时反馈 |
| **多层嵌套** | 盒子内部可包含子图 | `FuncBlock` 可嵌套调用 | 缺嵌套的可视化表示 |
| **实时验证** | 连接创建时立即类型检查 | `ConstraintGraph` 的合一检查 | 缺前端到后端的实时验证管道 |

### 1.3 核心概念对照

```
Semagrams.jl                                  Lv-00
────────────────────────────────────────────────────────────
Box (态射盒子)                           →    FuncBlock (函数块)
Input Port (输入端口)                     →    FuncBlock input ports
Output Port (输出端口)                    →    FuncBlock output ports
Wire (线缆连接)                           →    Constraint (约束边)
Port Type (端口类型)                      →    TypeRegion (类型区域)
Nested Diagram (嵌套图示)                 →    Nested FuncBlock (嵌套函数块)
Petri Net / Wiring Diagram               →    ConstraintGraph (约束图)
Drag-and-drop canvas                     →    Web GUI construction panel
```

---

## 2. 核心借鉴要点一：拖拽式接线图编辑范式

### 2.1 Semagrams.jl 的盒子+线缆范式

Semagrams.jl 的工作流非常简单直观：

```
用户操作流程:
  1. 从工具箱中拖出一个"盒子"（如 matrix_multiply）
     盒子上有标记类型的端口:
        [matrix_multiply]
         in_A: Matrix(m,n)  ─────┐
         in_B: Matrix(n,p)  ───┐ │
                                │ │
         out:  Matrix(m,p)  ←──┘ │ (两个输入端口，一个输出端口)

  2. 从工具箱中拖出另一个盒子（如 matrix_transpose）
        [matrix_transpose]
         in:   Matrix(m,n)  ─────┐
         out:  Matrix(n,m)  ←────┘

  3. 用线缆连接: matrix_transpose.out → matrix_multiply.in_B
     系统自动检查: out 类型 (Matrix(n,m)) 与 in_B 类型 (Matrix(n,p))
     → 只有当 n=n 且 m=p 时，连接才被允许

  4. 可以在盒子内部双击展开，看到嵌套的子接线图
```

### 2.2 Lv-00 的等价拖拽式几何构造

借鉴 Semagrams.jl 的盒子+线缆范式，Lv-00 的 Web GUI 构造面板可以设计为**几何工具箱 → 画布拖拽 → 约束连接**的工作流：

```
Lv-00 Web GUI 构造面板（借鉴 Semagrams.jl）:

┌─ 工具箱 ────────────────────────────────────────┐
│  [点]    [线段]    [圆]    [三角形]    [角]     │
│  [中点]  [垂线]    [平行线] [角平分线]  [交点]  │
│  ────────────────────────────────────────       │
│  用户自定义 FuncBlock:                          │
│  [my_median] [my_circumcenter] [my_euler_circle]│
└─────────────────────────────────────────────────┘

用户操作:
  1. 从工具箱拖出 [点] 盒子到画布 → 创建自由点 A
     端口: out: Point (TypeRegion = GEOM_POINT)

  2. 从工具箱拖出 [点] → 自由点 B
  3. 从工具箱拖出 [点] → 自由点 C
  4. 从工具箱拖出 [三角形] 盒子
     端口:
       in_A: Point ──── 连接 A.out
       in_B: Point ──── 连接 B.out
       in_C: Point ──── 连接 C.out
       out: Triangle ── 输出三角形 ABC

  5. 从工具箱拖出 [中点] 盒子
     端口:
       in_segment: Segment ──── 需要用户选择 BC 边
       out_point: Point ─────── 输出中点 M

  6. 实时类型检查:
     尝试连接 A.out → [中点].in_segment
     → 类型不兼容 (Point ≠ Segment)
     → 端口高亮红色，拒绝连接
```

### 2.3 几何工具箱的盒子定义

```c
/**
 * @brief 几何工具箱盒子定义 —— 借鉴 Semagrams.jl 的 Box 概念
 *
 * 每个 GeomToolboxItem 对应一个可拖拽的几何构造工具。
 * 它定义了构造需要的输入端口类型和产生的输出端口类型。
 *
 * Semagrams.jl 等价: Box(in_ports=[Port(:A, Matrix)], out_ports=[Port(:out, Matrix)])
 */
typedef struct {
    char *tool_name;                    /* 工具名（如 "midpoint", "triangle"） */
    char *display_label;                /* GUI 显示标签（中文：如 "中点", "三角形"） */
    char *icon_path;                    /* 工具箱图标路径 */

    /* 输入端口定义 */
    GeomToolPort *input_ports;
    int input_port_count;

    /* 输出端口定义 */
    GeomToolPort *output_ports;
    int output_port_count;

    /* 关联的 FuncBlock ID（后端实现） */
    int func_block_id;
} GeomToolboxItem;

typedef struct {
    char *port_name;                    /* 端口名（如 "point_A", "segment_input"） */
    int required_type_region;           /* 要求的 TypeRegion */
    char *type_label;                   /* GUI 类型标签（如 "Point", "Segment"） */
} GeomToolPort;
```

---

## 3. 核心借鉴要点二：类型检查的端口连接

### 3.1 Semagrams.jl 的实时类型验证

Semagrams.jl 在用户拖拽连接时**实时进行类型检查**——不兼容的连接被自动拒绝。这一机制直接映射到 Lv-00 的 `type_check_port_compatibility()` 函数：

```
Semagrams.jl 连接创建流程:              Lv-00 等价:
──────────────────────────────          ─────────────────
用户拖拽线缆 from port_A to port_B       Web GUI 触发 connect(port_A, port_B)
    │                                        │
    ▼                                        ▼
检查 port_A.type 与 port_B.type          type_check_port_compatibility(
    │                                     ts, node_A_id, port_A_idx,
    ▼                                     node_B_id, port_B_idx)
类型兼容?                                 │
  ├─ YES → 连接建立 (绿线)                ├─ 返回 true → 创建约束边
  └─ NO  → 端口闪烁红色 + 提示             └─ 返回 false → Web 高亮红色
```

### 3.2 Lv-00 的实时类型验证流

```c
/**
 * @brief Lv-00 Web GUI 端口连接类型验证 —— 借鉴 Semagrams.jl 的实时检查
 *
 * 当用户在 Web 前端拖拽连接两个端口时，
 * 前端通过 WebSocket 向 Lv-00 后端发送连接请求，
 * 后端调用此函数进行类型兼容性检查。
 *
 * Semagrams.jl 等价: 线缆拖拽时自动检查端口类型
 */
typedef enum {
    PORT_CONNECT_OK,            /* 类型兼容，连接允许 */
    PORT_CONNECT_TYPE_MISMATCH, /* 类型不兼容 */
    PORT_CONNECT_CYCLE_DETECTED,/* 连接会产生循环 */
    PORT_CONNECT_ALREADY_EXISTS,/* 同类型约束已存在 */
    PORT_CONNECT_CATEGORY_FAIL  /* 范畴约束不满足 */
} PortConnectStatus;

/**
 * @brief 端口连接验证 —— 后端核心
 *
 * 验证步骤（按序）:
 *   1. 检查源端口和目标端口是否都处于空闲状态
 *   2. 检查 源端口输出类型 ⊆ 目标端口输入类型的 TypeRegion
 *   3. 检查连接不会在约束图中产生非法循环
 *   4. 如果目标端口已有连接，检查是否允许多对一（取决于几何语义）
 *   5. 执行范畴约束检查（如 MetricSpace 操作不能用于 ProjectiveSpace 对象）
 *
 * 返回详细的连接状态，前端据此显示绿/红/黄反馈。
 */
PortConnectStatus web_port_validate_connection(
    ConstraintGraph *graph,
    int source_node_id, int source_port_idx,
    int target_node_id, int target_port_idx,
    char **out_error_message);
```

### 3.3 前端连接反馈设计

| 验证结果 | 视觉反馈 | 含义 |
|:---|:---|:---|
| `PORT_CONNECT_OK` | 绿色虚线 + "连接成功"提示 | 类型兼容，连接已建立 |
| `PORT_CONNECT_TYPE_MISMATCH` | 端口红色闪烁 + "类型不兼容: Point vs Segment" | 用户需选择正确类型的端口 |
| `PORT_CONNECT_CYCLE_DETECTED` | 红色警示图标 + "连接形成循环，违反构造顺序" | 几何构造必须是无环的 |
| `PORT_CONNECT_ALREADY_EXISTS` | 黄色提示 + "约束已存在，合并中..." | 合一引擎自动合并重复约束 |
| `PORT_CONNECT_CATEGORY_FAIL` | 橙色警告 + "此操作在 ProjectiveSpace 中未定义" | 范畴约束不满足 |

---

## 4. 核心借鉴要点三：多层嵌套图结构

### 4.1 Semagrams.jl 的嵌套图示

Semagrams.jl 最强大的特性之一是**嵌套**——一个盒子内部可以包含另一个完整的接线图，而且嵌套可以无限递归：

```
外层图 (main):
  ┌──────────────────────────────────────┐
  │  [预处理] ──→ [核心算法] ──→ [输出]   │
  │                 │                     │
  │  双击展开 [核心算法] ↓               │
  │  ┌──────────────────────────────┐    │
  │  │ [核心算法] 内部接线图 (嵌套)  │    │
  │  │                              │    │
  │  │  [排序] ──→ [过滤] ──→ [聚合] │    │
  │  │    │          │         │     │    │
  │  │  双击 [排序] 展开 ↓         │    │
  │  │  ┌──────────────────┐      │    │
  │  │  │ [排序] 内部       │      │    │
  │  │  │  [比较]→[交换]    │      │    │
  │  │  └──────────────────┘      │    │
  │  └──────────────────────────────┘    │
  └──────────────────────────────────────┘
```

### 4.2 Lv-00 的嵌套几何构造

在 Lv-00 中，几何构造自然具有嵌套关系。借鉴 Semagrams.jl 的嵌套图示：

```
外层构造 (三角形):
  ┌────────────────────────────────────────┐
  │  triangle(A, B, C)                      │
  │    │                                    │
  │  双击展开 triangle ↓                    │
  │  ┌──────────────────────────────────┐  │
  │  │ [triangle] 内部构造 (嵌套)        │  │
  │  │                                   │  │
  │  │  A ──segment── B                  │  │
  │  │  │              │                 │  │
  │  │  segment      segment             │  │
  │  │  │              │                 │  │
  │  │  └──── C ──────┘                 │  │
  │  │  双击展开 segment(A,B) ↓          │  │
  │  │  ┌──────────────────────────┐    │  │
  │  │  │ [segment] 内部            │    │  │
  │  │  │  in: Point A, Point B    │    │  │
  │  │  │  constraint: A ≠ B       │    │  │
  │  │  │  out: Segment s          │    │  │
  │  │  └──────────────────────────┘    │  │
  │  └──────────────────────────────────┘  │
  └────────────────────────────────────────┘

更深层嵌套:
  └── [triangle] 可能被更高层的 [quadrilateral] 使用
      └── [quadrilateral] = union([triangle], [triangle])
```

### 4.3 FuncBlock 嵌套的 C 数据结构

```c
/**
 * @brief FuncBlock 嵌套图节点 —— 借鉴 Semagrams.jl 的嵌套图示
 *
 * 每个 FuncBlock 本身是一个完整的 Lv-00 约束图（子图），
 * 可以包含任意数量的子 FuncBlock。
 *
 * Semagrams.jl 等价: Box 内部包含一个完整的 WiringDiagram
 */
typedef struct FuncBlockNesting {
    int func_block_id;                  /* 当前 FuncBlock 的 ID */

    /* 父 FuncBlock（如果为 -1，表示这是顶层块） */
    int parent_func_block_id;

    /* 子 FuncBlock 列表 */
    int *child_func_block_ids;
    int child_count;

    /* 该 FuncBlock 的端口与父图端口的映射关系 */
    PortMapping *port_mappings;         /* 外层端口 ↔ 内层端口的映射 */
    int mapping_count;

    /* 该 FuncBlock 内部的约束图（独立子图） */
    ConstraintGraph *internal_graph;

    /* 嵌套深度的可视化提示 */
    int nesting_depth;
    char *nesting_path;                 /* 例: "root/triangle/segment_AB" */
} FuncBlockNesting;

/**
 * @brief 端口映射 —— 嵌套层之间端口的对应关系
 *
 * 当 FuncBlock 被嵌套在其他 FuncBlock 内部时，
 * 内部端口需要映射到外层端口的连接。
 *
 * Semagrams.jl 等价: 盒子嵌套时，内层端口通过盒子"外壳"与外层线缆连接
 */
typedef struct {
    int outer_node_id;                  /* 外层图中的节点 ID */
    int outer_port_idx;                 /* 外层节点的端口索引 */
    int inner_node_id;                  /* 内层图中的节点 ID */
    int inner_port_idx;                 /* 内层节点的端口索引 */
    PortDirection direction;            /* IN → IN 还是 OUT → OUT */
} PortMapping;
```

---

## 5. Lv-00 映射方案：Web GUI 构造面板设计

### 5.1 整体架构

```
┌─────────────────────────────────────────────────────┐
│                   Lv-00 Web GUI                      │
│                                                      │
│  ┌─ 工具箱 (左侧) ─┐  ┌─ 画布 (中央) ────────────┐  │
│  │ [点] [线段]     │  │                            │  │
│  │ [圆] [三角形]   │  │   A ●                      │  │
│  │ [中点] [垂线]   │  │     \                      │  │
│  │ [角平分线]      │  │      \                     │  │
│  │ [平行线] [交点] │  │   B ●──● C                 │  │
│  │ ───────────     │  │       │                    │  │
│  │ 用户 FuncBlock: │  │    M ● (中点, Trust=绿色)  │  │
│  │ [重心] [外心]   │  │                            │  │
│  └─────────────────┘  └────────────────────────────┘  │
│                                                      │
│  ┌─ 约束属性 (底部) ─────────────────────────────┐  │
│  │ 选中: 点 M (midpoint of B and C)               │  │
│  │ 类型: Point | 范畴: MetricSpace                │  │
│  │ 坐标: ( (x_b+x_c)/2, (y_b+y_c)/2 )            │  │
│  │ TrustColor: GREEN  |  关联约束: 2 条           │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
            │ WebSocket (实时通信)
            ▼
┌─────────────────────────────────────────────────────┐
│              Lv-00 C 后端 (WASM)                     │
│  constraint_graph.c  type_system.c  solver.c        │
└─────────────────────────────────────────────────────┘
```

### 5.2 交互流：拖拽构造三角形

```
步骤1: 用户从工具箱拖出 [点] 到画布 → 自动命名为 A
  Web: create_node("A", "GEOM_POINT")
  → 后端: constraint_graph_add_node(graph, NODE_POINT, "A")
  → 返回: NodeHandle { id: 1, type: "Point", ports: [out:Point] }
  → Web: 在画布上渲染圆点 ●  + 标签 "A"

步骤2: 用户拖出 [点] → B，拖出 [点] → C
  同理创建节点 2 (B) 和节点 3 (C)

步骤3: 用户从工具箱拖出 [三角形] 到画布
  Web: create_node("triangle_ABC", "GEOM_TRIANGLE")
  → 后端: constraint_graph_add_node(graph, NODE_TRIANGLE, "ABC")
  → 返回: NodeHandle { id: 4, type: "Triangle",
            in_ports: [in_A:Point, in_B:Point, in_C:Point],
            out_ports: [out:Triangle] }

步骤4: 用户从节点 A 的 out:Point 拖线到 triangle 的 in_A:Point
  Web: connect(1, 0, 4, 0)  // 端口索引
  → 后端: web_port_validate_connection(graph, 1, 0, 4, 0)
  → 检查: 源类型(Point) vs 目标类型(Point) → OK
  → 创建约束边 → 返回 PORT_CONNECT_OK
  → Web: 渲染绿色连接线

步骤5: 同理连接 B.out → triangle.in_B, C.out → triangle.in_C

步骤6: 用户双击 triangle 盒子
  Web: request_expand(4)  // 请求展开节点 4
  → 后端: 返回 triangle 内部约束图 (三个 segment 边)
  → Web: 在画布上展示嵌套视图
```

### 5.3 前端-后端 WebSocket 协议

```
// 客户端 → 服务端
// 创建节点
{ "type": "create_node",
  "payload": { "kind": "GEOM_POINT", "name": "A" } }

// 连接端口
{ "type": "connect",
  "payload": { "src_node": 1, "src_port": 0, "tgt_node": 4, "tgt_port": 0 } }

// 展开嵌套
{ "type": "expand", "payload": { "node_id": 4 } }

// 服务端 → 客户端
// 连接结果
{ "type": "connect_result",
  "status": "ok",  // 或 "type_mismatch", "cycle_detected", ...
  "edge_id": 7,    // 如果成功，返回新约束边 ID
  "message": "连接成功: Point → Point, Line → Triangle" }

// 展开结果
{ "type": "expand_result",
  "node_id": 4,
  "internal_nodes": [ { ... }, { ... } ],
  "internal_edges": [ { ... }, { ... } ],
  "nesting_path": "root/triangle_ABC" }
```

---

## 6. FuncBlock 嵌套块系统映射

### 6.1 FuncBlock 与 Semagrams Box 的等价关系

```
Semagrams.jl Box               Lv-00 FuncBlock          func_block.h 字段
─────────────────────────────────────────────────────────────────────
Box 名称                     →  func_name               char *name
输入端口 (typed)              →  input_ports             FuncBlockPort *inputs
输出端口 (typed)              →  output_ports            FuncBlockPort *outputs
内部接线图 (嵌套)              →  internal_graph          ConstraintGraph *sub_graph
端口类型注解                  →  port_type_region        int type_region_id
嵌套深度                      →  nesting_depth           int depth
与父图的端口映射               →  port_mappings           PortMapping *mappings
双击展开                      →  web_request_expand()    前端 API
```

### 6.2 FuncBlock 嵌套的 Web 渲染层次

```
顶层 DSL:
  funcblock euler_circle(tri : Triangle) -> Circle {
      point O = circumcenter(tri.A, tri.B, tri.C);
      point H = orthocenter(tri.A, tri.B, tri.C);
      point N = midpoint(O, H);  // 九点圆心
      circle c = circle(N, distance(N, midpoint(tri.A, tri.B)));
      return c;
  }

Web GUI 嵌套视图:
  [euler_circle]                      ← 盒子（可拖拽到画布）
    双击展开 ↓
  ┌─ [euler_circle] 内部 ────────────┐
  │  ┌─ [circumcenter] ──→ O        │  ← 子盒子1
  │  │  双击可继续展开 ↓             │
  │  └──────────────────────────────  │
  │  ┌─ [orthocenter] ──→ H         │  ← 子盒子2
  │  └──────────────────────────────  │
  │  ┌─ [midpoint] ──→ N            │  ← 子盒子3
  │  │  in: O, H                     │
  │  └──────────────────────────────  │
  │  ┌─ [circle] ──→ c              │  ← 子盒子4
  │  │  in: center=N, radius=...     │
  │  └──────────────────────────────  │
  └──────────────────────────────────┘
```

---

## 7. 实现路线图

### 7.1 第一阶段：Web GUI 基础画布 (P4)

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 画布渲染器 (Canvas + SVG) | `web/canvas.js`（新文件） | 绘制点/线/圆/约束的基本图元 |
| 工具箱组件 | `web/toolbox.js`（新文件） | 可拖拽的几何工具列表 |
| 节点创建交互 | `web/node_editor.js`（新文件） | 拖放创建 + 命名 |
| 端口渲染 | `web/port.js`（新文件） | 输入/输出端口的视觉表示 |
| WebSocket 通信层 | `web/ws_client.js`（新文件） | 与 Lv-00 WASM 后端的实时通信 |

### 7.2 第二阶段：类型检查连接 (P4)

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| 端口连接拖拽 | `web/connection.js`（新文件） | 端口间的拖线交互 |
| `web_port_validate_connection()` | `src/type_system.c` | 后端类型验证（C 侧，被 WASM 调用） |
| `PortConnectStatus` 返回解析 | `web/connection.js` | 五种连接状态的视觉反馈 |
| 约束图实时更新 | `web/ws_client.js` | 连接建立后即时更新画布 |

### 7.3 第三阶段：嵌套图展开 (P4-P5)

| 任务 | 说明 |
|:---|:---|
| `FuncBlockNesting` 数据结构 | `func_block.h` 扩展嵌套信息 |
| `web_request_expand()` | 前端请求展开指定 FuncBlock 的内部图 |
| 嵌套视图渲染 | 内层图在画布上以缩放/偏移方式显示 |
| 端口映射解析 | 内层端口与外层线缆的视觉连接 |
| 面包屑导航 | `root > euler_circle > circumcenter` 导航条 |

---

## 8. 关键映射表

### 8.1 Semagrams.jl → Lv-00 概念映射

| Semagrams.jl | Lv-00 映射 | Lv-00 文件 |
|:---|:---|:---|
| `Box`（态射盒子） | `FuncBlock`（函数块） | `func_block.h` |
| `InputPort`（输入端口） | `FuncBlockPort`（方向=IN） | `func_block.h` |
| `OutputPort`（输出端口） | `FuncBlockPort`（方向=OUT） | `func_block.h` |
| `Port Type`（端口类型） | `TypeRegion`（类型区域） | `type_system.h` |
| `Wire`（线缆） | `Constraint`（约束边） | `constraint_graph.h` |
| `WiringDiagram`（接线图） | `ConstraintGraph`（约束图） | `constraint_graph.h` |
| `NestedDiagram`（嵌套图示） | `FuncBlockNesting`（嵌套函数块） | `func_block.h`（新增） |
| 类型检查（拖拽时） | `web_port_validate_connection()` | `type_system.c`（新增） |
| 工具箱 (Palette) | `GeomToolboxItem` 数组 | `web/toolbox.js`（新增） |
| 画布 (Canvas) | Web SVG Canvas | `web/canvas.js`（新增） |
| 端口映射（嵌套层之间） | `PortMapping` 结构体 | `func_block.h`（新增） |

### 8.2 几何工具箱预置项目

| 工具 | 中文标签 | 输入端口 | 输出端口 | 关联 FuncBlock |
|:---|:---|:---|:---|:---|
| Point (free) | 自由点 | (无) | out: Point | — |
| Segment | 线段 | in_A: Point, in_B: Point | out: Segment | segment |
| Circle (center+radius) | 圆 | in_center: Point, in_radius: Number | out: Circle | circle |
| Triangle | 三角形 | in_A: Point, in_B: Point, in_C: Point | out: Triangle | triangle |
| Midpoint | 中点 | in_segment: Segment | out_point: Point | midpoint |
| Perpendicular | 垂线 | in_line: Line, in_point: Point | out: Line | perpendicular |
| Parallel | 平行线 | in_line: Line, in_point: Point | out: Line | parallel |
| AngleBisector | 角平分线 | in_vertex: Point, in_A: Point, in_B: Point | out: Line | angle_bisector |
| Intersection | 交点 | in_obj1: GeomEntity, in_obj2: GeomEntity | out: Point | intersection |
| Circumcenter | 外心 | in_A: Point, in_B: Point, in_C: Point | out: Point | circumcenter |
| Orthocenter | 垂心 | in_A: Point, in_B: Point, in_C: Point | out: Point | orthocenter |

### 8.3 WebSocket 协议命令集

| 命令 | 方向 | 说明 |
|:---|:---|:---|
| `create_node` | Web → C | 创建新的几何节点 |
| `connect` | Web → C | 连接两个端口 |
| `disconnect` | Web → C | 断开已有连接 |
| `expand` | Web → C | 请求展开 FuncBlock 嵌套 |
| `collapse` | Web → C | 收起嵌套视图 |
| `solve` | Web → C | 触发约束求解 |
| `drag_move` | Web → C | 用户拖拽自由点 |
| `connect_result` | C → Web | 连接验证结果 |
| `expand_result` | C → Web | 返回嵌套子图 |
| `solve_result` | C → Web | 求解完成，返回坐标 |
| `type_error` | C → Web | 类型冲突通知 |
| `sync_state` | C → Web | 全量状态同步 |

---

> **文档结束**
> 本文档详述了 Semagrams.jl 接线图编辑器的拖拽式编辑、类型检查连接和多层嵌套图架构如何映射到 Lv-00 的 Web GUI 构造面板与 `func_block.h` 系统。核心结论：(1) 借鉴"盒子+线缆"范式，将几何构造工具表示为带类型端口的可拖拽盒子，通过 WebSocket 与 Lv-00 后端实时验证端口连接；(2) 借鉴多层嵌套图结构，为 FuncBlock 实现任意深度的嵌套展开/收起，每个嵌套层对应独立的子约束图；(3) 五级连接状态（OK/类型不匹配/循环/重复/范畴失败）提供与 Semagrams.jl 同级别的交互式类型安全反馈。
