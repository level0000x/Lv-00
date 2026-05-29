# Desmos 声明式脚本与计算层 UX 借鉴设计

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [Desmos API / Computational Layer](https://www.desmos.com/api) —— 全球最大数学交互平台，声明式脚本范式 + 双向数据绑定 + 数十亿次用户交互验证的 UX 模式
> **目标**: 借鉴 Desmos 的声明式脚本语法（`number/graph/sketch`）、双向数据绑定和 Computation Layer 活动创作系统，设计 Lv-00 的 DSL 声明式语法、Web GUI 交互模型和实时反馈机制

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 Desmos 与 Computation Layer 是什么

Desmos 是全球最大的数学交互平台，其图形计算器每月服务数千万用户，累计交互次数超过数十亿次。Desmos 的 API 和 Computation Layer（CL）是其活动创作系统的核心，提供了三个对 Lv-00 的 UX 和 DSL 设计至关重要的组件：

1. **声明式脚本范式**：Desmos 使用声明式脚本管理数据绑定、渲染和交互。单条语句即可完成"声明变量 → 绑定参数 → 渲染图形 → 响应用户交互"的全链路操作。例如 `number("a"): 5` 声明一个名为 `a` 的滑块变量，`graph: circle((0,0), a)` 声明以原点为圆心、半径为 `a` 的圆——变量 `a` 改变时圆自动重绘。这种"声明即生效"的范式是 Lv-00 DSL 语法设计的直接灵感来源。

2. **双向数据绑定**：Desmos 的核心交互模型是双向绑定——用户拖动滑块改变 `a` 的值 → 所有依赖 `a` 的图形（圆、线、点等）实时重计算并重渲染。这不是简单的"变量更新 → 视图刷新"，而是经过了完整的代数求解管道：变量更新 → 约束传播 → 图形重算 → SVG 重渲染。整个管道对用户完全透明，延迟通常 < 16ms（60fps 保证）。

3. **Computation Layer (CL)**：CL 是 Desmos 用于构建交互式数学活动的脚本层。它提供了丰富的 UI 组件（输入框、按钮、草图、选择题）和事件系统（`onChange`、`onSubmit`、`onSketch`），使教师和内容创作者可以构建自定义的数学探索活动。CL 的 `correct: this.submitted and this.numericValue = 42` 语法启发了 Lv-00 的 `solver_feedback_solve()` 实时验证反馈机制。

### 1.2 为什么借鉴 Desmos

Lv-00 的 Web GUI 需要一个直观的交互模型和 DSL 语法。Desmos 经过了数十亿次用户交互的验证，其声明式脚本范式和双向绑定模型已被证明是最适合数学可视化场景的 UX 范式。借鉴 Desmos 意味着：

1. **DSL 语法采用声明式风格**：`point A = (0, 0)`、`segment AB: A -- B`、`prove: |AM| == |MB|`——每条语句声明一个"是什么"，而非"怎么做"
2. **Web GUI 交互模型采用双向绑定**：用户在画布中拖动点 → 约束图更新 → 求解器传播 → 所有受影响图形重绘
3. **实时验证反馈**：用户完成构造后，验证结果以绿色/红色即时反馈，无需手动触发

---

## 2. 核心借鉴要点

### 2.1 声明式脚本范式

Desmos 的每条语句都是声明式的——描述"什么东西应该存在"而不是"如何创建它"：

```
// Desmos 声明式脚本示例
number("a"): 5              // 声明：存在一个名为 a 的数值变量，初始值为 5
number("b"): 3              // 声明：存在一个名为 b 的数值变量，初始值为 3
graph: circle((0,0), a)     // 声明：画布上有一个以原点为圆心、a 为半径的圆
graph: line((0,0), (a,b))   // 声明：画布上有一条从原点到 (a,b) 的直线
sketch: polygon(A, B, C)    // 声明：用户可手绘多边形 ABC 的草图区域
```

关键特征：
- **无顺序依赖**：语句的书写顺序不影响结果——所有声明被并行解析，依赖关系由变量名解析（而非执行顺序）
- **隐式依赖追踪**：`circle((0,0), a)` 自动订阅 `a` 的变更——用户不需要手动写 `onChange` 回调
- **类型推断**：`number("a")` 自动创建滑块（slider），`graph` 自动识别为图形渲染

### 2.2 双向数据绑定管道

Desmos 的双向绑定不是一个简单的"变量 → 视图"映射，而是一个完整的约束求解管道：

```
用户拖动滑块 a: 5 → 8
       ↓
[变量系统] a 的值更新为 8
       ↓
[依赖图遍历] 找出所有依赖 a 的图形:
  - circle((0,0), a)    → 半径变为 8
  - line((0,0), (a, b)) → 终点变为 (8, b值)
  - 任何引用 a 的表达式 → 重计算
       ↓
[表达式求值引擎] 并行重算所有受影响表达式
       ↓
[图形渲染引擎] 重新生成 SVG/Canvas 图元
       ↓
[浏览器合成] 提交到 GPU 进行帧渲染
       ↓
总延迟 < 16ms (保证 60fps)
```

### 2.3 Computation Layer (CL) 的活动创作

CL 使教师能够创建交互式活动，具有丰富的反馈机制：

```
// Desmos CL 示例：判断学生答案是否正确
correct: this.submitted and this.numericValue = 42

// 条件可见性：只有当 checkbox 被选中时才显示提示
hidden: not checkbox.isSelected(1)

// 实时反馈：当学生提交答案时触发
onSubmit: 
    when correct "正确！答案为 42。" 
    otherwise "再试一次。提示：答案是生命、宇宙和一切的终极答案。"
```

### 2.4 核心借鉴点映射表

| Desmos 概念 | Lv-00 对应概念 | 映射说明 |
|:---|:---|:---|
| `number("a"): 5` | `GeomNode` 的符号坐标声明 | 声明式参数定义 |
| `graph: circle(...)` | `FUNCTION_BLOCK` 构造块 | 声明式图形构造 |
| `sketch: polygon(...)` | Web GUI 草图交互区域 | 用户手绘/拖拽区域 |
| 双向数据绑定 | 约束图依赖追踪 + 自动传播 | 变量变化 → 约束传播 → 图形更新 |
| 隐式依赖追踪 | `constraint_graph` 邻接表 | 通过图邻接确定依赖关系 |
| 滑块（Slider） | Web GUI 参数调节器 | 用户可拖拽的参数输入 |
| 表达式求值引擎 | `solver.h` 符号/代数求值 | 约束方程求解 |
| `correct:` 反馈 | `solver_feedback_solve()` 验证反馈 | 实时验证结果展示 |
| `hidden:` 条件可见性 | `graph_node_set_visible()` | 辅助线/中间构造的条件显示 |
| `onSubmit` 事件 | `proof_step_execute` 协议消息 | 用户操作事件 |
| `onChange` 事件 | 变量域变化时触发传播队列 | 约束变更事件 |
| 活动创作（Activity Builder） | Lv-00 几何探索模板 | 预置的交互式几何探索场景 |
| 教师仪表板 | 证明进度追踪面板 | 对学生证明进度的可视化追踪 |

---

## 3. Lv-00 映射方案

### 3.1 声明式 DSL 语法设计

借鉴 Desmos 的声明式脚本风格，设计 Lv-00 的几何构造 DSL：

```
# === Lv-00 声明式几何构造 DSL ===
# 借鉴 Desmos 的 number/graph/sketch 三层声明式模型

# 第一层：参数声明（对应 Desmos number / slider）
param side_length = 10                    # 声明参数：边长 = 10（显示为滑块）
param angle_A = 60                        # 声明参数：角度 = 60°（显示为角度选择器）
param radius_value = 5                    # 声明参数：半径 = 5

# 第二层：几何构造声明（对应 Desmos graph）
point A = (0, 0)                          # 声明点 A
point B = (side_length, 0)                # 声明点 B（依赖参数 side_length）
point C = polar(side_length, angle_A)     # 声明点 C（极坐标构造）

segment AB = A -- B                       # 声明线段 AB
segment BC = B -- C                       # 声明线段 BC
segment CA = C -- A                       # 声明线段 CA

circle omega = center A radius radius_value       # 声明圆 omega
midpoint M of AB                          # 声明中点 M
perpendicular L through M to AB           # 声明垂线 L

# 第三层：命题声明（借鉴 ensures 概念）
prove: |OA| == |OB| == |OC|               # 声明要证明的命题
prove: L is_perpendicular_to AB           # 声明要证明的命题

# 第四层：交互声明（对应 Desmos sketch / CL）
sketch_area: bounds(-10, -10, 20, 20)     # 声明可拖拽区域
drag_point: C is_draggable                # 声明点 C 可拖拽
on_drag_point C: update_all_dependents    # 拖拽事件 → 更新所有依赖
```

**DSL 语法设计的声明式原则**（借鉴 Desmos）：

| 原则 | 说明 | Desmos 对应 |
|:---|:---|:---|
| 声明优先于命令 | `point A = (0, 0)` 而非 `create_point(A, 0, 0)` | `number("a"): 5` |
| 隐式依赖追踪 | 引用即订阅——无需手动声明依赖关系 | 表达式引用自动追踪 |
| 类型自动推断 | `segment AB = A -- B` 自动推断 AB 为线段类型 | `graph:` 自动推断渲染类型 |
| 命名即引用 | 所有声明都有名称，通过名称引用 | 变量名全局可见 |
| 参数化是一等公民 | `param` 声明自动创建可调节的 UI 控件 | Slider 自动生成 |

```c
/**
 * @brief Lv-00 声明式 DSL 解析器 —— 借鉴 Desmos 声明式脚本范式
 *
 * 解析 Lv-00 的几何构造 DSL 文本，生成 AST。
 * DSL 采用声明式风格：每条语句声明一个"是什么"，而非"怎么做"。
 *
 * 四层声明模型：
 *   Layer 1: param  —— 参数声明（绑定到滑块/选择器 UI 控件）
 *   Layer 2: point/segment/circle/midpoint/... —— 几何构造声明
 *   Layer 3: prove  —— 命题声明（ensures 子句）
 *   Layer 4: sketch_area/drag_point/on_drag —— 交互声明
 */
typedef enum {
    DSL_LAYER_PARAM,            /**< Layer 1: 参数声明 */
    DSL_LAYER_CONSTRUCTION,     /**< Layer 2: 几何构造声明 */
    DSL_LAYER_PROPOSITION,      /**< Layer 3: 命题声明 */
    DSL_LAYER_INTERACTION       /**< Layer 4: 交互声明 */
} DSLLayer;

typedef struct {
    DSLLayer layer;                    /**< 所属层级 */
    char *declaration_type;            /**< 声明类型（"point", "segment", "prove" 等） */
    char *identifier;                  /**< 标识符名称 */
    char **dependencies;               /**< 依赖的标识符列表（隐式追踪） */
    int dependency_count;              /**< 依赖数量 */
    char *value_expression;            /**< 值表达式（坐标、参数等） */
    int source_line;                   /**< 源码行号 */
} DSLStatement;

/**
 * @brief 解析 Lv-00 声明式 DSL
 *
 * @param dsl_text       DSL 源文本
 * @param out_statements 输出：解析后的语句数组
 * @param out_count      输出：语句数量
 * @return 成功返回 0，语法错误返回负值
 */
int lv00_dsl_parse(const char *dsl_text, DSLStatement **out_statements, int *out_count);

/**
 * @brief 从 DSL 语句生成 ConstraintGraph
 *
 * 借鉴 Desmos 的"声明 → 图形"的即时实例化模型。
 * 每条 DSL 语句直接实例化为相应的约束图节点和约束。
 *
 * @param statements    DSL 语句数组
 * @param count         语句数量
 * @param out_graph     输出：生成的约束图
 * @return 成功返回 0，失败返回负值
 */
int lv00_dsl_to_graph(const DSLStatement *statements, int count,
                      ConstraintGraph **out_graph);
```

### 3.2 双向数据绑定在 Lv-00 中的实现

将 Desmos 的双向绑定模型映射到 Lv-00 的约束图架构：

```c
/**
 * @brief 双向数据绑定引擎 —— 借鉴 Desmos 的依赖追踪 + 自动重算
 *
 * Desmos 的双向绑定管道:
 *   变量变化 → 依赖图遍历 → 表达式重算 → 图形重渲染
 *
 * Lv-00 的等效管道:
 *   GeomNode 坐标变化 → constraint_graph 邻接遍历 → 约束传播 → Web GUI 重渲染
 *
 * 关键设计：
 *   - 依赖追踪：通过 constraint_graph 的邻接表隐式追踪
 *     一个节点变化 → graph_find_constraints_involving(node_id) 找到所有受影响约束
 *   - 传播触发：受影响约束对应的 propagator 入队
 *   - 批量重算：propagator_run_loop() 批量执行传播
 *   - 脏标记：仅在传播完成后提交变更到渲染层
 */
typedef struct {
    int changed_node_id;                /**< 发生变化的节点 ID */
    SymbolicCoord old_value;            /**< 旧值（用于撤销） */
    SymbolicCoord new_value;            /**< 新值 */

    /* 双向绑定追踪 */
    int *affected_constraint_ids;       /**< 受影响的约束 ID 数组 */
    int affected_count;                 /**< 受影响约束数量 */
    bool propagation_complete;          /**< 传播是否完成 */

    /* 渲染标记 */
    int *dirty_node_ids;                /**< 需要重绘的节点 ID 数组 */
    int dirty_count;                    /**< 需要重绘的节点数量 */
    bool render_needed;                 /**< 是否需要提交到渲染层 */
} BindingUpdateContext;

/**
 * @brief 处理节点坐标变化并触发双向绑定传播
 *
 * 借鉴 Desmos 的"拖动滑块 → 全部重算"的单向流动 + 双向反馈。
 *
 * 当用户在 Web GUI 中拖动一个点时：
 *  1. Web GUI 发送坐标变更到 Engine
 *  2. Engine 调用此函数，触发依赖追踪和约束传播
 *  3. 传播完成后，Engine 返回受影响的节点和渲染指令
 *  4. Web GUI 根据渲染指令更新画布
 *
 * @param graph         约束图
 * @param node_id       变更的节点 ID
 * @param new_coords    新坐标值
 * @param out_ctx       输出：绑定更新上下文（包含渲染指令）
 * @return 成功返回 0，冲突返回负值
 */
int binding_update_propagate(
    ConstraintGraph *graph,
    int node_id,
    const SymbolicCoord *new_coords,
    BindingUpdateContext **out_ctx
);
```

**双向绑定的完整数据流**：

```
Web GUI (用户拖动点 C)
   │
   │ WebSocket: {"action": "binding_update", "node_id": 3, "coords": {...}}
   ▼
Lv-00 Engine
   │
   ├─ 1. 更新 GeomNode[3] 的 SymbolicCoord
   │
   ├─ 2. 依赖追踪: graph_find_constraints_involving(graph, 3)
   │     → [INCIDENCE(C, BC), INCIDENCE(C, CA)]
   │
   ├─ 3. 约束传播: propagator_run_loop(graph, queue)
   │     - INCIDENCE(C, BC) 传播 → BC 的方程更新
   │     - 如果 B 的坐标有界 → B 的域可能缩小
   │     - 继续传播直到不动点
   │
   ├─ 4. 脏标记收集: 收集所有域发生变化的节点
   │     dirty_nodes = [BC的渲染属性, B的坐标(如果域缩小), ...]
   │
   └─ 5. 返回渲染指令
   │
   ▼
Web GUI 收到更新
   │
   ├─ 更新所有 dirty_nodes 在画布上的位置/形状
   ├─ 绿色标记（已确定节点）vs 蓝色标记（未确定节点）
   └─ 更新约束状态指示器
```

### 3.3 实时验证反馈机制

借鉴 Desmos CL 的 `correct:` 即时反馈语法，设计 Lv-00 的实时验证反馈：

```c
/**
 * @brief 实时求解验证反馈 —— 借鉴 Desmos CL 的 correct: 反馈机制
 *
 * Desmos CL 中，学生提交答案后立即得到正确/错误反馈:
 *   correct: this.submitted and this.numericValue = 42
 *
 * Lv-00 的等效机制：用户完成构造步骤后，立即对相关的 ensures
 * 命题执行验证，并以绿色/红色即时反馈。
 *
 * 验证反馈的三个层级:
 *   1. 即时反馈（< 100ms）: 简单的类型检查和边界条件
 *   2. 快速反馈（< 1s）:   约束传播 + 合一检查
 *   3. 完整反馈（< 10s）:  完整求解器验证
 */
typedef enum {
    FEEDBACK_LEVEL_INSTANT,       /**< 即时反馈（类型检查） */
    FEEDBACK_LEVEL_FAST,          /**< 快速反馈（约束传播 + 合一） */
    FEEDBACK_LEVEL_FULL           /**< 完整反馈（求解器验证） */
} FeedbackLevel;

typedef struct {
    bool is_correct;                   /**< 命题是否成立 */
    FeedbackLevel level_used;          /**< 使用的反馈层级 */
    char *feedback_message;            /**< 反馈消息（人类可读） */
    char *suggestion;                  /**< 建议（失败时，如"尝试添加辅助线"） */
    int *highlight_nodes;              /**< 需要高亮的节点 */
    int highlight_count;               /**< 高亮节点数量 */
    int64_t response_time_ms;          /**< 响应时间 */
} SolverFeedback;

/**
 * @brief 对构造块的 ensures 命题执行分层实时反馈
 *
 * 借鉴 Desmos CL 的即时反馈模型，采用三级反馈策略：
 *   - 即时反馈（< 100ms）：类型检查 + requires 满足性
 *   - 快速反馈（< 1s）：约束传播 + 合一检查
 *   - 完整反馈（< 10s）：完整代数求解
 *
 * 每一层的结果立即返回给 Web GUI，不等待更高层完成。
 * 这种渐进式反馈给用户"即时响应"的感受。
 *
 * @param graph         约束图
 * @param block_id      构造块 ID
 * @param solver        求解器引擎
 * @param out_feedback  输出：求解反馈
 * @return 成功返回 0，失败返回负值
 *
 * @see desmos_cl_declarative_ux.md —— Desmos CL 反馈参考
 */
int solver_feedback_solve(
    ConstraintGraph *graph,
    int block_id,
    ConstraintSolver *solver,
    SolverFeedback *out_feedback
);
```

**Web GUI 中的反馈展示**：

```
构造状态面板:
┌─────────────────────────────────────────┐
│ 构造块: △ABC                            │
│ 状态: ⬤ 已验证 (3/3 ensures 通过)       │
├─────────────────────────────────────────┤
│ requires:                               │
│   ✓ A, B, C 不共线                      │
│   ✓ AB + BC > AC (三角不等式)           │
├─────────────────────────────────────────┤
│ ensures:                                │
│   ✓ ∠A + ∠B + ∠C = 180°                │
│   ✓ |AB| > 0（非退化）                  │
│   ✓ △ABC 的面积为 24                    │
├─────────────────────────────────────────┤
│ 建议: 无（所有命题已验证）              │
└─────────────────────────────────────────┘
```

### 3.4 Web GUI 交互模型 —— 借鉴 Desmos 的 UX 模式

Desmos 的数十亿次用户交互验证了以下 UX 模式，直接映射到 Lv-00：

| Desmos UX 模式 | Lv-00 Web GUI 实现 | 说明 |
|:---|:---|:---|
| 滑块（Slider） | 参数调节器（range input） | 用户拖动滑块调节参数（边长、角度等） |
| 拖拽点（Draggable Points） | 画布中拖动节点 | 用户拖动几何点，触发双向绑定更新 |
| 即时渲染（Instant Rendering） | RAF（requestAnimationFrame）重绘 | 60fps 保证，延迟 < 16ms |
| 颜色编码（Color Coding） | 绿色=已验证, 蓝色=未确定, 红色=冲突 | 状态一目了然 |
| 表达式列表（Expressions List） | 约束列表侧边栏 | 文本形式查看所有约束 |
| 缩放/平移（Zoom/Pan） | 画布缩放和平移 | 鼠标滚轮缩放，拖拽平移 |
| 撤销/重做（Undo/Redo） | 证明步骤的撤销/重做 | Ctrl+Z / Ctrl+Y |
| 隐藏/显示（Hide/Show） | 辅助线/构造的可见性切换 | 减少画布杂乱的视觉噪音 |
| 动画（Animation） | 证明步骤的逐帧动画播放 | 展示证明的逐步推导 |

### 3.5 交互式几何探索模板（借鉴 Desmos Activity Builder）

```c
/**
 * @brief 几何探索活动模板 —— 借鉴 Desmos Activity Builder
 *
 * Desmos 允许教师通过 Activity Builder 创建交互式数学探索活动。
 * Lv-00 提供预置的几何探索模板，用户可在模板基础上修改。
 *
 * 模板类型：
 *  - 三角形性质探索：拖动顶点，观察面积/周长变化
 *  - 外心/内心/重心探索：构造三角形的特殊点
 *  - 圆的性质探索：改变半径/圆心，观察切线变化
 *  - 相似/全等探索：比较两个三角形的对应关系
 *
 * 每个模板包含：
 *  - 预设的几何构造（由 DSL 描述）
 *  - 可拖拽的交互点
 *  - 预设的验证命题
 *  - 引导性的探索步骤
 */
typedef struct {
    char *template_id;                 /**< 模板 ID */
    char *template_name;               /**< 模板名称（如"三角形外心探索"） */
    char *description;                 /**< 模板描述 */
    char *dsl_source;                  /**< 预设 DSL 源码 */
    int *draggable_node_ids;           /**< 可拖拽的节点 ID */
    int draggable_count;               /**< 可拖拽节点数量 */
    int *ensures_proposition_ids;      /**< 预设的验证命题 */
    int ensures_count;                 /**< 命题数量 */
    char **exploration_steps;          /**< 探索步骤引导文本 */
    int step_count;                    /**< 步骤数量 */
} ExplorationTemplate;

/**
 * @brief 从模板创建几何探索会话
 *
 * @param template_id    模板 ID
 * @param out_graph      输出：从模板实例化的约束图
 * @param out_proof_nav  输出：从模板初始化的证明导航器
 * @return 成功返回 0，失败返回负值
 */
int exploration_template_load(
    const char *template_id,
    ConstraintGraph **out_graph,
    ProofNavigator **out_proof_nav
);
```

---

## 4. 实现路线图

### 4.1 第一阶段：声明式 DSL 语法（P1-P2）

- [ ] 完成 Lv-00 声明式 DSL 语法规范的正式文档
- [ ] 实现 `lv00_dsl_parse()` 解析器（四层声明模型）
- [ ] 实现 `lv00_dsl_to_graph()` DSL → ConstraintGraph 转换
- [ ] 实现 `graph_to_dsl()` 反向转换（约束图 → DSL 文本，用于双向同步）
- [ ] 在 Web GUI 中实现 DSL 编辑器（Monaco Editor + 语法高亮）
- [ ] 实现 DSL 文本 ↔ 画布的双向同步
- [ ] 编写 DSL 语法的单元测试和示例文件

### 4.2 第二阶段：双向数据绑定（P2）

- [ ] 实现 `BindingUpdateContext` 数据结构
- [ ] 实现 `binding_update_propagate()` 双向绑定核心逻辑
- [ ] 实现依赖追踪的增量更新（仅传播受影响约束）
- [ ] 实现脏标记收集和批量渲染指令生成
- [ ] Web GUI 中实现拖拽交互 → WebSocket 消息 → 渲染更新
- [ ] 性能优化：确保双向绑定管道延迟 < 50ms
- [ ] 实现 RAF（requestAnimationFrame）渲染循环

### 4.3 第三阶段：实时验证反馈（P2-P3）

- [ ] 实现 `solver_feedback_solve()` 分层反馈
- [ ] 实现即时反馈层（类型检查 < 100ms）
- [ ] 实现快速反馈层（约束传播 + 合一 < 1s）
- [ ] 实现完整反馈层（代数求解 < 10s）
- [ ] Web GUI 中实现反馈面板（构造状态面板）
- [ ] 实现颜色编码反馈（绿色/蓝色/红色）
- [ ] 实现失败时的交互式建议（"尝试添加辅助线"）

### 4.4 第四阶段：探索模板与 UX 打磨（P3-P4）

- [ ] 创建 5-10 个预置几何探索模板
- [ ] 实现 `exploration_template_load()` 模板加载
- [ ] Web GUI 中实现模板浏览器
- [ ] 实现证明步骤的动画播放（逐帧展示推导）
- [ ] 实现画布缩放/平移/适配
- [ ] 实现辅助线/构造的可见性切换
- [ ] 收集用户交互数据，优化常用 UX 路径

---

## 5. 设计决策与权衡

### 5.1 声明式 DSL vs 命令式 API

Desmos 的成功证明了声明式范式在数学可视化场景中的优势。Lv-00 是否也应该完全采用声明式？

| 方面 | 声明式 DSL | 命令式 API（当前 proof_step） |
|:---|:---|:---|
| 可读性 | 高（描述"是什么"） | 中（描述"怎么做"） |
| 可组合性 | 高（语句独立，自动依赖追踪） | 中（步骤顺序依赖） |
| 学习曲线 | 需学习 DSL 语法 | 需学习 API 调用 |
| 版本控制 | 极好（纯文本 diff） | 差（JSON/二进制格式） |
| 灵活性 | 受限于 DSL 表达能力 | 完全灵活 |

**决策**：声明式 DSL 作为主要用户接口，命令式 API 保留为底层执行模型。用户通过 DSL 声明构造意图，Engine 将 DSL 编译为命令式 proof_step 序列执行。这类似于 TypeScript（声明式）编译为 JavaScript（命令式）的关系。

### 5.2 反馈的分层策略

Desmos 的反馈是即时的（< 16ms），但 Lv-00 的几何验证可能需要更长时间。

**决策**：采用三级分层反馈：
- 即时层（< 100ms）：类型检查 + requires 验证。结果在用户完成输入的瞬间可见
- 快速层（< 1s）：约束传播 + 合一检查。在即时层返回后异步执行，给出"确定性"反馈
- 完整层（< 10s）：完整代数求解。仅在前两层无法确定时启动

UI 上展示"已验证"（绿色 ✓）、"部分验证"（黄色 ~）、"正在验证"（橙色旋转）、"未验证"（蓝色 ?）四种状态，对应三层反馈的渐进结果。

### 5.3 与现有画布渲染的集成

Lv-00 的 Web GUI 需要同时支持 DSL 文本编辑和交互式画布拖拽。这两种模式如何共存？

**决策**：共享数据源 + 双向同步。
- 约束图（`ConstraintGraph`）是唯一的数据源
- DSL 文本和画布都是约束图的"视图"（View）
- 在 DSL 编辑器中修改文本 → 重解析 → 更新约束图 → 画布自动刷新
- 在画布中拖拽点 → 更新节点坐标 → 约束传播 → DSL 文本自动同步（通过 `graph_to_dsl()` 反向转换）

---

## 6. 总结

Desmos 的声明式脚本范式和双向数据绑定模型，经过数十亿次用户交互的验证，为 Lv-00 的 DSL 语法设计和 Web GUI 交互模型提供了经过实战检验的 UX 蓝图。声明式语法（`point A = (0, 0)`、`segment AB = A -- B`、`prove: |AM| == |MB|`）使几何构造的意图表达清晰直观，隐式依赖追踪消除了手动的事件订阅管理。双向数据绑定将用户拖拽一个点的操作自动传播为全部相关约束的重新求解和全部相关图形的重新渲染，延迟控制在 50ms 以内。Computation Layer 的分层反馈机制使验证结果以即时（< 100ms）、快速（< 1s）、完整（< 10s）三种粒度渐进呈现，给予用户"即时响应"的感受。几何探索模板继承了 Desmos Activity Builder 的交互式数学探索理念，使 Lv-00 不仅是证明工具，更是几何学习的交互式平台。

| Desmos 核心概念 | Lv-00 映射组件 | 实现文件 |
|:---|:---|:---|
| 声明式脚本（number/graph/sketch） | Lv-00 四层 DSL 语法 | `lv00_dsl.c`（新文件） |
| DSL → 图形实例化 | `lv00_dsl_to_graph()` | `lv00_dsl.c` |
| 图形 → DSL 反向转换 | `graph_to_dsl()` | `lv00_dsl.c` |
| 双向数据绑定 | `binding_update_propagate()` + `BindingUpdateContext` | `binding.c`（新文件） |
| 依赖追踪 | `graph_find_constraints_involving()` 邻接遍历 | `constraint_graph.h` |
| Slider / 参数调节器 | Web GUI 参数面板 | Web GUI 项目 |
| CL `correct:` 即时反馈 | `solver_feedback_solve()` 分层反馈 | `solver_feedback.c`（新文件） |
| Activity Builder 模板 | `ExplorationTemplate` + 模板浏览器 | `exploration.c`（新文件） |
| 颜色编码（绿/蓝/红） | Web GUI 节点状态可视化 | Web GUI 项目 |
| 滑块交互 | Web GUI 拖拽交互 + WebSocket 协议 | Web GUI 项目 |

---

> **文档结束**
> 本文档详述了 Desmos 的声明式脚本范式、双向数据绑定模型和 Computation Layer 反馈机制如何映射到 Lv-00 的 DSL 语法、Web GUI 交互模型和实时验证反馈系统。核心结论：声明式 DSL 使几何构造的意图表达变得直观——用户声明"什么应该存在"而非"如何创建"；双向数据绑定使交互体验丝滑——拖拽一个点自动传播为全图更新；分层反馈使验证结果即时可见——绿色/红色颜色编码一目了然。Desmos 的数十亿次用户交互验证了这些 UX 模式在数学可视化场景中的有效性，Lv-00 借此站在巨人的肩膀上构建自己的几何交互体验。
