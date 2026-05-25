# Mermaid.js 文本驱动图表渲染借鉴设计

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [Mermaid.js](https://github.com/mermaid-js/mermaid) —— 基于文本的 JavaScript 图表渲染引擎，文本→实时图表的声明式渲染管道
> **目标**: 借鉴 Mermaid.js 的文本驱动实时渲染管道和多种图表语法（时序图、状态图、类图），为 Lv-00 Web GUI 提供"约束编辑器 Mermaid 模式"、"证明步骤时序可视化"和"类型系统文档化"三重映射

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 Mermaid.js 是什么

Mermaid.js 是一个基于 JavaScript 的文本驱动图表渲染引擎。用户编写简洁的文本语法描述图表结构，Mermaid 实时将其渲染为 SVG 图形。其核心理念是"用文本表达图表"——无需拖拽、无需图形界面，纯文本描述即可生成专业的流程图、时序图、状态图、类图等。Mermaid 的三个核心特性对 Lv-00 的 Web GUI 可视化有直接借鉴价值：

1. **文本→实时图表渲染管道**：用户在编辑器面板中写入 `graph LR; A-->B`，预览面板实时渲染为从左到右的流程图。这种"写文本、看图表"的声明式交互模式非常适合 Lv-00 的约束编辑体验——用户可以用文本描述几何约束，实时看到约束关系图。

2. **多种图表语法的统一框架**：Mermaid 支持流程图（graph）、时序图（sequenceDiagram）、状态图（stateDiagram）、类图（classDiagram）等 10+ 种图表类型。每种图表有专用的语法子集，但共享相同的渲染基础设施。这种"多样化语法 + 统一渲染管道"的设计启发了 Lv-00 在同一个 Web GUI 中提供多种可视化模式。

3. **Markdown 原生集成**：Mermaid 在绝大多数 Markdown 渲染器（GitHub、GitLab、Notion、Obsidian 等）中被原生支持。这意味着 Lv-00 的类型系统和 API 文档可以直接在 Markdown 中嵌入 Mermaid 图表，确保跨平台的可读性。

### 1.2 为什么借鉴 Mermaid.js

Lv-00 的约束系统（`constraint_graph.h`）和证明系统（`proof.h`）的数据是高度结构化的，但这些结构化数据在 Web GUI 中的可视化方式尚待设计。借鉴 Mermaid.js 意味着：

1. **约束编辑器 Mermaid 模式**：用户可以用类似 Mermaid 的文本语法声明几何约束，编辑器实时渲染为约束图。这提供了"声明式约束编辑"的替代入口，降低了拖拽构造的学习成本。

2. **证明步骤时序可视化**：借鉴 Mermaid 的 `sequenceDiagram` 语法，将证明步骤序列以时序图形式展示——每个证明步骤是一个时间段，显示状态变化和约束传播。

3. **类型系统文档化**：借鉴 Mermaid 的 `classDiagram` 语法，将 Lv-00 的几何类型层次（点、线、圆、三角形、多边形等）以类图形式自动生成文档。

---

## 2. 核心借鉴要点

### 2.1 文本→实时图表渲染管道

Mermaid 的核心渲染管道：

```
用户输入的文本描述（DSL 语法）
       ↓
[解析器 (Parser)] 
  ├─ 词法分析: graph → 图表类型关键词
  ├─ 语法分析: A-->B → Edge(from=A, to=B, type=ARROW)
  └─ 构建 AST (抽象语法树)
       ↓
[布局引擎 (Layout Engine)]
  ├─ Dagre: 有向图层次布局
  ├─ ELK: 复杂图自动布局
  └─ 计算节点位置和边路径
       ↓
[渲染器 (Renderer)]
  ├─ 生成 SVG 元素 (rect, path, text)
  ├─ 应用 CSS 样式
  └─ 注入 DOM
       ↓
实时预览面板中的 SVG 图表
```

关键设计：
- **声明式**：用户只需描述关系（A-->B），布局引擎自动计算美观的位置
- **实时性**：输入文本的每次修改都触发完整的解析→布局→渲染管道
- **可扩展**：通过注册新的图表类型和语法，可扩展支持新的可视化模式

### 2.2 三种图表语法的借鉴

#### 2.2.1 流程图语法 → 约束关系图

```
graph TD
    A[已知点 A] -->|INCIDENCE| AB[线段 AB]
    B[已知点 B] -->|INCIDENCE| AB
    AB -->|MIDPOINT| M[中点 M]
    A -->|INCIDENCE| AC[线段 AC]
    C[已知点 C] -->|INCIDENCE| AC
    M -->|PERPENDICULAR| L[垂直平分线 L]
```

#### 2.2.2 时序图语法 → 证明步骤时序

```
sequenceDiagram
    participant User as 用户
    participant Engine as Lv-00 Engine
    participant Solver as 求解器
    
    User->>Engine: proof_step_execute(构造中点M)
    Engine->>Engine: 验证类型约束 ✓
    Engine->>Solver: 检查新约束一致性
    Solver-->>Engine: 约束满足 (3 new, 0 conflict)
    Engine-->>User: 步骤完成 (step_id=7, branches=[8])
```

#### 2.2.3 类图语法 → 类型系统文档

```
classDiagram
    class GeomNode {
        +int id
        +GeomNodeType type
        +SymbolicCoord[] coords
        +int[] constraint_ids
    }
    class Point {
        +float x
        +float y
    }
    class LineSegment {
        +int endpoint_a_id
        +int endpoint_b_id
    }
    class Circle {
        +int center_id
        +float radius_value
    }
    
    GeomNode <|-- Point
    GeomNode <|-- LineSegment
    GeomNode <|-- Circle
```

### 2.3 核心借鉴点映射表

| Mermaid.js 概念 | Lv-00 对应概念 | 映射说明 |
|:---|:---|:---|
| 文本 DSL 语法 | Lv-00 约束声明语法（类 Mermaid 文本） | 文本→约束图的声明式入口 |
| 解析器（Parser） | 约束文本解析器（`constraint_dsl_parse()`） | 文本语法 → AST |
| 布局引擎（Layout Engine） | 约束图自动布局（`graph_layout_auto()`） | 节点位置自动计算 |
| 渲染器（Renderer） | Web GUI Canvas/SVG 渲染 | 约束图可视化 |
| 实时更新 | `on_input_change` → 重新渲染 | 每次编辑即时反馈 |
| `graph`（流程图） | 约束关系图模式 | 节点=几何对象，边=约束 |
| `sequenceDiagram`（时序图） | 证明步骤时序图模式 | 参与者=用户/引擎/求解器 |
| `stateDiagram`（状态图） | 约束求解状态机模式 | 状态=约束状态转换 |
| `classDiagram`（类图） | 类型系统文档模式 | 类=几何类型层次 |
| Markdown 原生集成 | 文档内嵌 Lv-00 图表 | README/API 文档中的可视化 |
| SVG 输出 | Web GUI 矢量渲染 | 无损缩放，适合几何展示 |

---

## 3. Lv-00 映射方案

### 3.1 约束编辑器 Mermaid 模式

为 Lv-00 的 Web GUI 提供一种"声明式约束编辑"模式，用户可以用类似 Mermaid 的文本语法声明约束：

```
# Lv-00 约束声明语法（Lv00约束DSL）
constraint_graph {
    # 声明几何对象（对应 GeomNode）
    point A (0, 0)
    point B (10, 0)
    point C (5, 8)
    
    # 声明约束（对应 Constraint）
    segment AB: A -- B
    segment BC: B -- C
    segment CA: C -- A
    
    # 声明构造（对应 FUNCTION_BLOCK）
    midpoint M of AB
    
    # 声明命题（对应 ensures）
    prove: |AM| == |MB|
    
    # 声明辅助线
    auxiliary: line through M perpendicular to AB
}
```

**DSL 语法设计原则**（借鉴 Mermaid 的简洁性）：
- `point <name> (<x>, <y>)` → 创建 `GeomNode` (type=POINT)
- `<name>: <a> -- <b>` → 创建线段 + 添加 INCIDENCE 约束
- `midpoint <name> of <segment>` → 创建中点构造块
- `prove: <expression>` → 创建 ensures 命题
- `auxiliary: <description>` → 创建 Ghost 辅助构造

```c
/**
 * @brief 约束 DSL 解析结果 —— AST 节点
 *
 * 借鉴 Mermaid parser 的 AST 设计，将约束声明文本解析为结构化的 AST。
 */
typedef enum {
    AST_NODE_DECLARE_POINT,       /**< point A (0, 0) */
    AST_NODE_DECLARE_SEGMENT,     /**< segment AB: A -- B */
    AST_NODE_DECLARE_CIRCLE,      /**< circle C: center P radius 5 */
    AST_NODE_CONSTRUCT,           /**< midpoint M of AB */
    AST_NODE_CONSTRAINT,          /**< INCIDENCE / BETWEENNESS / etc. */
    AST_NODE_PROPOSITION,         /**< prove: expression */
    AST_NODE_AUXILIARY,           /**< auxiliary: description */
    AST_NODE_EDGE,                /**< -- or --> (约束关系) */
} ConstraintASTNodeType;

typedef struct ConstraintASTNode {
    ConstraintASTNodeType type;        /**< 节点类型 */
    char *identifier;                  /**< 标识符名称 */
    char *value;                       /**< 值（坐标、表达式等） */
    struct ConstraintASTNode **children; /**< 子节点 */
    int child_count;                   /**< 子节点数量 */
    int source_line;                   /**< 源码行号（错误定位） */
} ConstraintASTNode;

/**
 * @brief 解析约束 DSL 文本并生成 AST
 *
 * 借鉴 Mermaid 的 parser → AST 管道。
 *
 * @param dsl_text      约束 DSL 文本
 * @param out_ast       输出：AST 根节点（调用者需用 ast_destroy 释放）
 * @return 成功返回 0，语法错误返回负值
 */
int constraint_dsl_parse(const char *dsl_text, ConstraintASTNode **out_ast);
```

**Web GUI 中的 Mermaid 模式布局**：

```
┌──────────────────────────────────────────────────────┐
│  [Mermaid 模式]  [画布模式]  [混合模式]              │
├──────────────────────┬───────────────────────────────┤
│                      │                               │
│   约束DSL编辑器      │   实时约束图预览              │
│   (Monaco Editor)    │   (SVG/Canvas 渲染)           │
│                      │                               │
│   point A (0, 0)     │   ┌───┐    ┌───┐             │
│   point B (10, 0)    │   │ A │────│ B │             │
│   segment AB: A--B   │   └───┘    └───┘             │
│   midpoint M of AB   │     │  AB    │                │
│                      │     │        │                │
│                      │   ┌───┐      │                │
│                      │   │ M │      │                │
│                      │   └───┘      │                │
│                      │                               │
├──────────────────────┴───────────────────────────────┤
│  解析状态: ✓ 无语法错误  |  约束图: 3节点, 2边      │
└──────────────────────────────────────────────────────┘
```

### 3.2 证明步骤时序可视化

借鉴 Mermaid 的 `sequenceDiagram` 语法，将证明步骤以时序图形式可视化：

```c
/**
 * @brief 证明步骤时序图节点 —— 借鉴 Mermaid sequenceDiagram
 *
 * 将证明树中的一个线性路径（根到目标）以时序图形式展示。
 * 每个步骤包含参与者、动作和状态变化。
 */
typedef struct {
    int step_index;                     /**< 时序步骤序号 */
    char *actor;                        /**< 执行者（"User" / "Engine" / "Solver"） */
    char *action;                       /**< 动作描述 */
    char *target;                       /**< 动作目标 */
    bool is_synchronous;                /**< true = --→ (同步), false = --→> (异步) */
    char *result;                       /**< 动作结果 */
    int64_t duration_ms;                /**< 步骤耗时 */
    char *state_change;                 /**< 状态变化描述 */
} ProofSequenceStep;

/**
 * @brief 从证明树构建时序图
 *
 * 遍历证明树，提取每一步的参与者、动作和状态变化，
 * 生成可供 Mermaid sequenceDiagram 渲染的步骤序列。
 *
 * @param nav           证明导航器
 * @param target_node   目标节点（提取根到 target_node 的路径）
 * @param out_steps     输出：时序步骤数组
 * @param out_count     输出：步骤数量
 * @return 成功返回 0，失败返回 -1
 */
int proof_sequence_build(
    ProofNavigator *nav,
    int target_node,
    ProofSequenceStep **out_steps,
    int *out_count
);

/**
 * @brief 将证明时序步骤导出为 Mermaid sequenceDiagram 文本
 *
 * 生成的文本可直接嵌入 Markdown 文档或在 Web GUI 中渲染。
 *
 * @param steps         时序步骤数组
 * @param count         步骤数量
 * @param out_mermaid   输出：Mermaid sequenceDiagram 文本
 * @return 成功返回 0，失败返回 -1
 */
int proof_sequence_to_mermaid(
    const ProofSequenceStep *steps,
    int count,
    char **out_mermaid
);
```

**生成的 Mermaid 时序图示例**：

```
sequenceDiagram
    participant U as 用户
    participant E as Lv-00 Engine
    participant S as 约束求解器
    participant T as 类型系统
    
    U->>E: proof_step_execute(构造中点M)
    activate E
    E->>T: 类型检查(输入: Point×2)
    T-->>E: ✓ TYPE_POINT × 2
    E->>S: 注册新约束(INCIDENCE × 2)
    S-->>E: 约束传播完成(域缩小)
    E->>T: 类型推断(输出类型)
    T-->>E: TYPE_POINT
    E-->>U: step_id=7, branches=[]
    deactivate E
    
    Note over U,S: 第一步完成：中点M构造成功
```

### 3.3 约束求解状态图可视化

借鉴 Mermaid 的 `stateDiagram` 语法，展示约束求解过程中各种约束状态及其转换：

```
stateDiagram-v2
    [*] --> Pending: 约束注册
    Pending --> Propagating: 变量域变化触发
    Propagating --> Consistent: 传播成功
    Propagating --> Conflict: 检测到冲突
    Consistent --> Subsumed: 约束永久满足
    Consistent --> Propagating: 相关变量域再变化
    Conflict --> Backtracking: 回溯到分叉点
    Backtracking --> Propagating: 选择新分支
    Subsumed --> [*]: 约束休眠
    Conflict --> [*]: 无解报告
```

```c
/**
 * @brief 约束状态机可视化 —— 借鉴 Mermaid stateDiagram
 *
 * 将 Lv-00 约束生命周期中的状态转换建模为有限状态机。
 */
typedef enum {
    CONSTRAINT_STATE_PENDING,         /**< 刚注册，等待首次传播 */
    CONSTRAINT_STATE_PROPAGATING,     /**< 正在传播中 */
    CONSTRAINT_STATE_CONSISTENT,      /**< 传播成功，约束满足 */
    CONSTRAINT_STATE_SUBSUMED,        /**< 约束永久满足，已休眠 */
    CONSTRAINT_STATE_CONFLICT,        /**< 检测到冲突 */
    CONSTRAINT_STATE_BACKTRACKING,    /**< 回溯中 */
    CONSTRAINT_STATE_DEAD             /**< 死约束（分支已废弃） */
} ConstraintState;

/**
 * @brief 将约束图的状态机导出为 Mermaid stateDiagram 文本
 *
 * @param graph         约束图
 * @param out_mermaid   输出：Mermaid stateDiagram 文本
 * @return 成功返回 0，失败返回 -1
 */
int constraint_state_to_mermaid(
    const ConstraintGraph *graph,
    char **out_mermaid
);
```

### 3.4 类型系统类图文档化

借鉴 Mermaid 的 `classDiagram` 语法，自动生成 Lv-00 类型系统的文档：

```c
/**
 * @brief 类型系统类图生成器 —— 借鉴 Mermaid classDiagram
 *
 * 遍历 TypeSystem 中的所有类型定义，自动生成 Mermaid classDiagram，
 * 展示类型层次、继承关系和方法签名。
 *
 * 生成的类图可直接嵌入 Markdown 文档，在 GitHub/GitLab 中原生渲染。
 *
 * @param ts            类型系统
 * @param show_methods  是否显示方法/属性
 * @param out_mermaid   输出：Mermaid classDiagram 文本
 * @return 成功返回 0，失败返回 -1
 */
int typesystem_to_mermaid_classdiagram(
    const TypeSystem *ts,
    bool show_methods,
    char **out_mermaid
);
```

**自动生成的类型系统类图示例**：

```
classDiagram
    class GeomNode {
        +int id
        +GeomNodeType type
        +SymbolicCoord[] coords
        +int[] constraint_ids
        +node_is_determined() bool
    }
    class Point {
        +float x
        +float y
        +distance_to(Point) float
    }
    class LineSegment {
        +int endpoint_a_id
        +int endpoint_b_id
        +length() float
        +midpoint() Point
        +slope() float
    }
    class Circle {
        +int center_id
        +float radius_value
        +area() float
        +contains(Point) bool
    }
    class Triangle {
        +int vertex_a_id
        +int vertex_b_id
        +int vertex_c_id
        +area() float
        +is_right_angle() bool
    }
    class Angle {
        +int vertex_id
        +int arm_a_id
        +int arm_b_id
        +measure() float
    }
    
    GeomNode <|-- Point : extends
    GeomNode <|-- LineSegment : extends
    GeomNode <|-- Circle : extends
    GeomNode <|-- Triangle : extends
    GeomNode <|-- Angle : extends
    Triangle o-- Point : contains 3
    Circle o-- Point : center
```

### 3.5 Web GUI 集成架构

```
┌─────────────────────────────────────────────────────┐
│                   Lv-00 Web GUI                      │
├─────────────────────────────────────────────────────┤
│  顶部工具栏                                         │
│  [Mermaid模式] [画布模式] [时序模式] [状态模式]     │
├─────────────────────────────────────────────────────┤
│                                                      │
│  主视图区域（根据模式切换）                          │
│  ┌─────────────────────────────────────────────┐    │
│  │ Mermaid模式:  编辑器 + 图表预览 (并排)      │    │
│  │ 画布模式:     交互式几何画布                 │    │
│  │ 时序模式:     证明步骤时序图 + 详情面板     │    │
│  │ 状态模式:     约束状态机图 + 约束列表        │    │
│  └─────────────────────────────────────────────┘    │
│                                                      │
├─────────────────────────────────────────────────────┤
│  底部状态栏: 约束图状态 | 引擎状态 | 视图模式       │
└─────────────────────────────────────────────────────┘
         │                              │
         ▼                              ▼
   ┌──────────┐              ┌──────────────────┐
   │ Mermaid  │              │ Canvas 渲染器     │
   │ 渲染引擎 │              │ (原生几何画布)    │
   │ (mermaid │              │ (Fabric.js /      │
   │  .js)    │              │  Konva.js)        │
   └──────────┘              └──────────────────┘
         │                              │
         └──────────┬───────────────────┘
                    ▼
          ┌──────────────────┐
          │ Lv-00 Protocol   │
          │ (WebSocket JSON) │
          └──────────────────┘
                    │
                    ▼
          ┌──────────────────┐
          │ Lv-00 Engine     │
          │ (C 核心)          │
          └──────────────────┘
```

---

## 4. 实现路线图

### 4.1 第一阶段：Mermaid 基础设施集成（P2）

- [ ] 在 Web GUI 项目中集成 mermaid.js 库（npm install mermaid）
- [ ] 实现 Mermaid 渲染组件（React/Vue 组件，接收 mermaid 文本，渲染 SVG）
- [ ] 实现视图模式切换工具栏（Mermaid模式 / 画布模式 / 时序模式 / 状态模式）
- [ ] 实现实时更新：编辑器文本变化 → 重新渲染 Mermaid 图表
- [ ] Monaco Editor 集成：Mermaid 语法高亮和自动补全

### 4.2 第二阶段：约束编辑器 Mermaid 模式（P2-P3）

- [ ] 设计并实现 Lv-00 约束 DSL 语法规范
- [ ] 实现 `constraint_dsl_parse()` 解析器
- [ ] 实现 AST → ConstraintGraph 的转换（`ast_to_graph()`）
- [ ] 实现约束图 → Mermaid `graph` 语法的反向转换（`graph_to_mermaid()`）
- [ ] Web GUI 中实现 Mermaid 模式的双面板布局

### 4.3 第三阶段：时序图与状态图（P3）

- [ ] 实现 `proof_sequence_build()` 时序步骤提取
- [ ] 实现 `proof_sequence_to_mermaid()` Mermaid sequenceDiagram 导出
- [ ] 实现 `constraint_state_to_mermaid()` 状态机 Mermaid stateDiagram 导出
- [ ] Web GUI 中实现时序模式视图（可播放的步骤动画）
- [ ] Web GUI 中实现状态模式视图（约束生命周期追踪）

### 4.4 第四阶段：类图与文档集成（P3-P4）

- [ ] 实现 `typesystem_to_mermaid_classdiagram()` 类型系统类图生成
- [ ] 集成到文档生成管道（CI/CD 自动生成类型系统文档）
- [ ] 在 README 和 API 文档中嵌入自动生成的类图
- [ ] 性能优化：大型约束图（>100节点）的增量渲染

---

## 5. 设计决策与权衡

### 5.1 文本 DSL vs 拖拽编辑

Mermaid 是纯文本驱动的——用户必须写文本语法。Lv-00 是否需要文本 DSL？

| 方面 | 文本 DSL（Mermaid 风格） | 拖拽编辑（画布模式） |
|:---|:---|:---|
| 精确性 | 高（精确指定坐标和约束） | 中（依赖鼠标精度） |
| 学习曲线 | 需学习语法 | 直观（所见即所得） |
| 复杂构造 | 适合（可批量声明） | 逐步操作 |
| 版本控制 | 极好（纯文本可 diff） | 差（二进制/JSON 格式） |
| 可复制性 | 极高（复制粘贴即重现） | 低（环境依赖） |

**决策**：两种模式并存。默认画布模式用于入门和视觉探索，Mermaid 文本模式用于精确构造、批量操作和版本控制。两种模式共享同一个约束图状态——在文本模式中修改 DSL，画布实时刷新；在画布中拖拽，DSL 文本同步更新。

### 5.2 自研渲染 vs 直接使用 Mermaid.js

**决策**：混合策略。
- **Mermaid.js 用于文档输出**：生成的 Mermaid 文本可直接嵌入 Markdown 文档，利用 GitHub/GitLab 的原生渲染
- **自研 Canvas 渲染用于画布模式**：几何画布需要精确的坐标渲染、拖拽交互和动画效果，这些超出 Mermaid.js 的能力范围
- **共享布局算法**：约束图的自动布局（节点位置计算）可以共享，Mermaid 的 Dagre 布局引擎可被两者复用

### 5.3 语法兼容性

Lv-00 的约束 DSL 是否应该完全兼容 Mermaid 语法？

**决策**：部分兼容。约束 DSL 使用 Mermaid 风格的语法框架（关键词 + 箭头 + 缩进），但扩展了约束和构造特有的语法（`midpoint`、`prove`、`auxiliary`）。这样既保持了与 Mermaid 生态的兼容性（熟悉 Mermaid 的用户可快速上手），又满足了 Lv-00 的领域特定需求。

---

## 6. 总结

Mermaid.js 的文本驱动渲染管道为 Lv-00 的 Web GUI 提供了三重可视化能力：(1) 约束编辑器 Mermaid 模式——用户可以用声明式文本语法描述几何约束，实时渲染为约束关系图，降低了拖拽构造的学习成本并为精确构造和版本控制提供了纯文本入口；(2) 证明步骤时序可视化——借鉴 `sequenceDiagram` 语法展示证明步骤的时间线和参与者交互，使证明过程透明可追溯；(3) 类型系统文档化——借鉴 `classDiagram` 语法自动生成几何类型层次图，嵌入 Markdown 文档后可在所有主流平台原生渲染。

| Mermaid.js 核心概念 | Lv-00 映射组件 | 实现文件 |
|:---|:---|:---|
| 文本 DSL 解析器 | `constraint_dsl_parse()` + AST | `constraint_dsl.c`（新文件） |
| `graph`（流程图） | 约束关系图 Mermaid 模式 | Web GUI MermaidMode 组件 |
| `sequenceDiagram`（时序图） | `proof_sequence_build/to_mermaid()` | `proof_sequence.c`（新文件） |
| `stateDiagram`（状态图） | `constraint_state_to_mermaid()` | `constraint_state.c`（新文件） |
| `classDiagram`（类图） | `typesystem_to_mermaid_classdiagram()` | `typesystem_doc.c`（新文件） |
| SVG 渲染管道 | Web GUI Mermaid 渲染组件 | Web GUI 项目 |
| 实时更新 | `on_input_change` → 重渲染 | Web GUI 事件系统 |
| Markdown 原生集成 | 文档中嵌入图表 | docs/ 目录 |
| Dagre 布局引擎 | `graph_layout_auto()` | `graph_layout.c`（新文件） |

---

> **文档结束**
> 本文档详述了 Mermaid.js 的文本驱动图表渲染管道如何映射到 Lv-00 Web GUI 的三种可视化模式。核心结论：通过引入约束 DSL 的 Mermaid 模式，Lv-00 获得了声明式约束编辑的纯文本入口；通过时序图和状态图，证明过程和约束生命周期变得透明可追溯；通过类图自动生成，类型系统文档可在所有 Markdown 平台中原生渲染。三种模式共享统一的约束图数据源，确保不同视图之间的数据一致性。
