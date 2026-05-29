# Lv-00 参考设计：Antimony/Kokopelli 节点式数据流图

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: [Antimony](https://github.com/mkeeter/antimony) —— 基于节点图的可视化 CAD 建模环境  
> **目标**: 借鉴 Antimony 的节点式数据流图构造范式，映射到 Lv-00 的 `constraint_graph.h`——约束图即带类型检查的数据流图

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 Antimony 是什么

Antimony 是 Matt Keeter 开发的基于节点图的交互式 3D CAD 建模工具，其后端为 libfive 的 F-Rep 引擎。Antimony 的核心理念：**CAD 建模 = 连接数据流节点**。每个节点是一个"带输入端口的函数块"，节点之间的连线表示数据流（输出端口→输入端口）。

```
// Antimony 示例：通过连线构建一个圆角立方体
// 节点图（文本表示）：
[Box] → (shape) → [Offset (r=0.2)] → (shape) → [Export STL]
  ^                    ^
  |                    |
width=2, height=1     r=0.2
```

Antimony 的关键特性：

1. **节点即函数块**：每个节点封装一个几何操作（如 `Box`、`Sphere`、`Offset`、`Rotate`）
2. **连线即约束**：节点间的连线定义数据流方向，形成有向无环图（DAG）
3. **类型检查**：连线端口有类型标注（如 `shape`、`number`、`point`），编译器在连线时进行类型一致性检查
4. **即时重算**：修改任意节点的参数，下游节点自动重新计算
5. **脚本层 Kokopelli**：Python 脚本驱动节点图的创建和连线，实现参数化建模

### 1.2 为什么借鉴 Antimony

Lv-00 的 `constraint_graph.h` 已实现了"节点（GeomNode）+ 约束边（Constraint）"的图结构，`GEOM_PORT` 和 `GEOM_FUNCTION_BLOCK` 类型已经提供了端口和函数块的抽象，`CONNECTION` 约束类型定义了端口之间的连线。这与 Antimony 的"节点+连线=数据流图"模型在结构上高度一致。然而，Lv-00 当前缺失 Antimony 的两大核心能力：

1. **端口的类型检查**：`CONNECTION` 约束在连接两个端口时，没有验证端口类型的一致性
2. **数据流驱动的即时重算**：函数块输入变化后，没有自动触发下游重新求解

---

## 2. 核心借鉴要点

### 2.1 节点式数据流图的映射

| Antimony 概念 | Lv-00 对应概念 | 映射说明 |
|--------------|---------------|---------|
| 节点 (Node) | `GeomNode`（`GeomType = GEOM_FUNCTION_BLOCK`） | 每个函数块是一个节点 |
| 输入端口 (Input Port) | `Port`（`PortType = PORT_INPUT`） | `GeomNode.data.port` 联合体 |
| 输出端口 (Output Port) | `Port`（`PortType = PORT_OUTPUT`） | `FuncBlock.output_port_ids[]` |
| 连线 (Wire) | `Constraint`（`ConstraintType = CONNECTION`） | 端口 A → 端口 B 的约束边 |
| DAG 拓扑 | 约束图的有向版本 | 当前约束边是无向的，需引入方向 |
| 类型标注 | `Port.is_polymorphic` + `Port.type_region` | 已有类型区域（`TypeRegion*`）标记 |
| 即时重算 | `solver.h` 的增量求解（`dirty_variable_ids`） | 已有脏变量机制，但未自动触发 |
| Kokopelli 脚本层 | Lv-00 DSL 的函数块语法 | DSL 中的 `funcblock` 和 `connect` 语句 |

### 2.2 数据流图的关键不变式

Antimony 的数据流图维护以下不变式（Lv-00 约束图应等价维护）：

| 不变式 | Antimony 保证 | Lv-00 对应 |
|--------|-------------|-----------|
| **无环约束** | 连线图必须是 DAG | 约束图当前不强制无环性，函数块内层应有此保证 |
| **类型一致性** | 输出端口类型 ⊆ 输入端口类型 | `TypeRegion` 的子类型关系检查 |
| **单一赋值** | 每个输入端口只能连一条线 | 每个 `PORT_INPUT` 的 `connected_to` 唯一（或 NULL） |
| **扇出自由** | 一个输出端口可以连到多个输入端口 | `PORT_OUTPUT` 无连接数量限制 |
| **参数化** | 无输入端口的节点 = 根参数（由用户设置） | `GEOM_POINT` 可以是根参数（已知坐标） |

---

## 3. Lv-00 映射方案

### 3.1 约束图即数据流图

借鉴 Antimony 的节点式构图范式，Lv-00 的约束图可以显式地视为一种**带类型检查的数据流图**：

```
                  ┌───────────────────────────────────────────┐
                  │     ConstraintGraph (Antimony 等价)        │
                  │                                           │
  ┌──────────┐    │   ┌──────────┐       ┌──────────┐         │
  │ point A  │    │   │ midpoint │       │ point M  │         │
  │ (已知坐标)│───────→│  (函数块) │───────→│ (交点坐标)│         │
  │          │    │   │          │       │          │         │
  │ PORT_OUT │    │   │ PORT_IN  │       │ PORT_OUT │         │
  │          │    │   │ PORT_OUT │       │          │         │
  └──────────┘    │   └──────────┘       └──────────┘         │
                  │        ^                                   │
  ┌──────────┐    │        │                                   │
  │ point B  │────┼────────┘ (另一输入)                        │
  │ (已知坐标)│    │                                           │
  └──────────┘    │                                           │
                  │   Constraints:                            │
                  │     CONNECTION(A.out → midpoint.in_1)      │
                  │     CONNECTION(B.out → midpoint.in_2)      │
                  │     CONNECTION(midpoint.out → M.in)        │
                  │                                           │
                  └───────────────────────────────────────────┘
```

### 3.2 端口类型检查

Antimony 的核心质量保证机制是端口的类型检查。Lv-00 已通过 `TypeRegion*` 为端口引入了类型标注，但尚未在 `CONNECTION` 创建时强制执行类型检查：

```c
/**
 * @brief Antimony 风格的端口类型检查
 *
 * 在创建 CONNECTION 约束（即连线）之前，验证输出端口类型
 * 是否为输入端口所需类型的子类型。
 *
 * 类型规则（继承自 TypeRegion 的子类型关系）：
 * - Point <: Geometry （点可作为几何类型的输入）
 * - Segment <: Geometry
 * - Region <: Geometry
 * - Number <: Scalar
 * - Scalar <: Expression
 *
 * @return true 如果类型兼容，可以连线
 */
bool port_type_check(const Port *src_port, const Port *dst_port);

/**
 * @brief 创建带类型检查的端口连线（Antimony 风格）
 *
 * 相比 constraint_create(CONNECTION, src, dst)，
 * 此函数额外执行 port_type_check，类型不匹配时拒绝连线。
 *
 * @return 创建的 Constraint ID，类型不匹配时返回 -1
 */
int constraint_graph_connect_typed(ConstraintGraph *graph,
                                   int src_port_node_id,
                                   int dst_port_node_id);
```

### 3.3 数据流驱动的即时重算

Antimony 的核心用户体验是"修改即所见"——修改上游节点的参数后，所有下游节点自动更新。Lv-00 已有增量求解的脏变量机制，需要加入自动触发：

```c
/**
 * @brief Antimony 风格的脏标记传播
 *
 * 当一个函数块的输入端口的连接对象发生变化时，
 * 沿 CONNECTION 约束边向前（下游）传播脏标记。
 *
 * 传播规则：
 * 1. 如果端口 A 的 connected_to 对象的 `symbolic_coords` 被修改
 * 2. 遍历所有以 A 为输入的 CONNECTION 约束
 * 3. 将约束的目标端口所在函数块标记为脏
 * 4. 递归传播到更下游的函数块
 *
 * 这是 Antimony "即时重算"在 Lv-00 中的等价实现。
 */
int constraint_graph_propagate_dirty(ConstraintGraph *graph, int changed_node_id);

/**
 * @brief 自动增量求解（Antimony 风格）
 *
 * 在 dirty 传播后自动调用求解器，按拓扑序重新求解脏函数块。
 * 拓扑序保证每个函数块的输入在求解前已经就绪。
 *
 * @return 更新后的节点数量
 */
int constraint_graph_autosolve(ConstraintGraph *graph);
```

### 3.4 函数块作为一等节点

Antimony 将每个操作封装为一个节点。Lv-00 的 `FuncBlock` 已具备这一特性，借鉴 Antimony 可以进一步形式化：

```c
/**
 * @brief Antimony 风格的"一等函数块节点"
 *
 * 每个 FuncBlock 在约束图中是一个可被连线引用的独立节点。
 * 其输入/输出端口通过 CONNECTION 约束与其他节点相连，
 * 形成类似 Antimony 的有向数据流图。
 */
typedef struct FuncBlockNode {
    GeomNode node;              /* 基础节点（type = GEOM_FUNCTION_BLOCK） */

    /* Antimony 节点元数据 */
    char *display_name;          /* 用户可见名称（如 "midpoint", "rotate"） */
    char *category;              /* 节点分类（如 "构造", "变换", "证明"） */
    char *description;           /* 节点描述 */

    /* 端口信息 */
    Port **input_ports;          /* 输入端口数组 */
    int input_count;
    Port **output_ports;         /* 输出端口数组 */
    int output_count;

    /* 数据流状态（Antimony 风格） */
    bool is_dirty;               /* 是否需要重新求解 */
    int topological_order;       /* 拓扑排序序号（用于增量求解） */
    double last_solve_time_ms;   /* 上次求解耗时 */
} FuncBlockNode;
```

### 3.5 映射到现有 constraint_graph.h

| `constraint_graph.h` 现有结构 | Antimony 借鉴后的角色 |
|-----------------------------|---------------------|
| `GeomType` (GEOM_POINT/SEGMENT/REGION/PORT/FUNCTION_BLOCK) | 节点类型（Antimony 的 Shape/Transform/Boolean 等分类） |
| `Port`（`is_polymorphic` + `type_region`） | 端口的类型标注（Antimony 的核心） |
| `Port.connected_to` | 连线目标（Antimony 的 Wire） |
| `ConstraintType.CONNECTION` | 连线本身（Antimony 的数据流边） |
| `FuncBlock.internal_node_ids[]` | 节点的内部计算图 |
| `FuncBlock.input_port_ids[]` / `output_port_ids[]` | 节点的对外接口 |
| 哈希索引（`node_by_id`） | O(1) 节点查找（Antimony 的节点引用） |
| `constraint_graph_add_constraint()` | 创建连线（Antimony 的 `canvas.connect(a, b)`） |

### 3.6 DSL 中的数据流图表达

借鉴 Antimony 的 Kokopelli Python 脚本层，Lv-00 DSL 已支持函数块的声明和例化。借鉴 Antimony 后，DSL 可以显式表达数据流连线：

```
// Lv-00 DSL 的 Antimony 风格数据流语法
// ─────────────────────────────────────────
// 类似 Kokopelli 的 Python 脚本

// 创建根节点（已知点，无输入端口）
point A(0, 0);
point B(6, 0);
point C(3, 4);

// 创建函数块节点（有输入/输出端口）
funcblock midpoint(p1: Point, p2: Point) -> Point {
    // 内部数据流图
    point result((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);
    return result;
}

// 数据流连线（隐式——通过参数传递）
point M_AB = midpoint(A, B);  // A的输出 → midpoint.p1, B的输出 → midpoint.p2
                               // midpoint.result → M_AB

// 等价于 Antimony 的显式连线语法（可选显式写法）
// connect A.output → midpoint.input[0];
// connect B.output → midpoint.input[1];
// connect midpoint.output → M_AB.input;

// 视觉化：约束图的数据流视图
//   A ──→ [midpoint] ──→ M_AB
//   B ──→
```

---

## 4. 实现路线图

### 4.1 第一阶段：端口类型检查（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 实现 `port_type_check(src, dst)` | `src/constraint_graph.c` | 端口类型兼容性验证 |
| 实现 `constraint_graph_connect_typed()` | `src/constraint_graph.c` | 带类型检查的连线创建 |
| 迁移所有 `constraint_create(CONNECTION, ...)` 调用 | 全局 | 替换为 `connect_typed` |
| 添加类型不匹配时的诊断错误消息 | `include/lv00/error_codes.h` | 新错误码 `ERR_PORT_TYPE_MISMATCH` |

**预估规模**：约 150 行 C 代码

### 4.2 第二阶段：脏标记传播与自动重算（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 实现 `constraint_graph_propagate_dirty()` | `src/constraint_graph.c` | 沿 CONNECTION 边传播脏标记 |
| 实现 `constraint_graph_autosolve()` | `src/constraint_graph.c` | 按拓扑序自动重算脏函数块 |
| 实现 `constraint_graph_topological_sort()` | `src/constraint_graph.c` | 对函数块进行拓扑排序 |
| 在 `GeomNode` 中添加 `is_dirty` 和 `topological_order` 字段 | `include/lv00/constraint_graph.h` | 数据流图状态标记 |

**预估规模**：约 250 行 C 代码

### 4.3 第三阶段：可视化与调试（P3+）

| 任务 | 说明 |
|------|------|
| 数据流图可视化导出（DOT 格式） | 类似 Antimony 的画布视图，导出为 Graphviz 格式 |
| 节点分类系统 | 将函数块按"构造/变换/测量/证明"分类 |
| 子图展开/折叠 | 将函数块内部节点图展开或折叠（类似 Antimony 的 group/ungroup） |
| 连线高亮 | 当端口被选中时，高亮所有相连的连线（CONNECTION 约束） |

---

## 附录 A：Antimony 与 Lv-00 结构对照

| Antimony 结构 | Lv-00 结构 | 说明 |
|--------------|-----------|------|
| `Node` | `GeomNode`（`type=GEOM_FUNCTION_BLOCK`） | 每个函数块 = 一个 Antimony 节点 |
| `Node.inputs[]` | `FuncBlock.input_port_ids[]` | 输入端口列表 |
| `Node.outputs[]` | `FuncBlock.output_port_ids[]` | 输出端口列表 |
| `Datum`（带类型的值） | `SymbolicCoord` + `TypeRegion` | Antimony 的值带有类型，Lv-00 坐标带有代数类型 |
| `Wire` | `Constraint`（`type=CONNECTION`） | 节点间的连线 |
| `Canvas` | `ConstraintGraph` | 全局构图空间 |
| `Canvas.solve()` | `scheduler_solve(graph)` | 触发全局求解 |
| `Kokopelli` Python 脚本 | Lv-00 DSL 文本 | 参数化构图的脚本层 |
| 节点 Inspector | `FuncBlock` 的元数据字段 | 节点的名称/描述/分类 |

---

## 附录 B：数据流图与约束图的统一视角

```
        传统约束图 (无向)               Antimony 数据流图 (有向)
        ─────────────────             ──────────────────────
        
        A ──INCIDENCE── seg           A(已知) ──→ [midpoint] ──→ M(待解)
        B ──INCIDENCE── seg           B(已知) ──→
        
        区别:                           区别:
        · 边是无向的                     · 边是有向的 (output→input)
        · 没有类型检查                   · 有端口类型检查
        · 全图一次性求解                 · 按拓扑序增量求解
        
        统一: Lv-00 的约束图同时支持两种模式
        ──────────────────────────────────────
        · INCIDENCE/BETWEENNESS/INTERSECTION → 无向约束模式（用于命题和证明）
        · CONNECTION → 有向数据流模式（用于函数块构图）
        · 两种模式通过 CONNECTION 约束的"方向性"统一在同一个 ConstraintGraph 中
```

---

> **文档结束**  
> 本文档详述了 Antimony/Kokopelli 的节点式数据流图构造范式如何映射到 Lv-00 的 `constraint_graph.h`。核心结论：Lv-00 的约束图天然具备数据流图的结构（`GEOM_FUNCTION_BLOCK` 节点 + `CONNECTION` 边 + `Port` 端口 + `TypeRegion` 类型标注），借鉴 Antimony 后只需补充三个关键能力即可形成完整的数据流图系统：(1) 端口类型检查，(2) 脏标记沿 CONNECTION 边自动传播，(3) 按拓扑序的增量自动求解。
