# 22 证明导出、追踪树与交互可视化 (Proof Export, Trace Tree & Interactive Widgets)

## 1 模块概述

本组模块构成 Lv-00 的证明输出子系统，负责将推理引擎产生的证明数据转化为多种目标格式、结构化追踪记录和交互式可视化组件。三个模块各司其职：

- **proof_export_enhanced.h**：增强证明导出，支持 HTML/LaTeX/Coq/Lean 4/JSON/DOT 六种格式序列化
- **proof_trace.h**：证明追踪树，以树形结构记录证明步骤、管理前提、导出教科书风格文本和 Mizar 风格形式化证明
- **proof_widget.h**：证明交互可视化组件，借鉴 ProofWidgets4 提供 8 种 Widget、智能策略推荐和搜索树/依赖图 JSON 序列化

### 1.1 与核心 proof.h 的关系

`proof.h` 定义 Lv-00 证明系统的核心数据结构（`Proposition`、`ProofStep`、`Proof`、`ProofNavigator`），负责证明的构造和推理。本组三个模块不参与证明的构造过程，而是消费 `proof.h` 产生的证明数据：

```
proof.h (核心证明系统)
  |-- Proposition / ProofStep / Proof / ProofNavigator
  |
  +-- proof_export_enhanced.h  -- 序列化为外部格式
  +-- proof_trace.h            -- 构建树形追踪记录
  +-- proof_widget.h           -- 提供交互式可视化数据契约
```

### 1.2 与 Layer 10 输出层的关系

本组模块均属于 Lv-00 第十层（输出层），位于 `core/src/layer10_output/`（`proof_export_enhanced.c`、`proof_widget.c`）和 `core/src/layer9_reasoning/`（`proof_trace.c`）。输出层负责将推理层产生的内部数据转化为人类可读或机器可消费的外部表示。

---

## 2 proof_export_enhanced.h -- 增强证明导出

### 2.1 设计概述

提供统一的证明导出接口，支持六种目标格式的序列化。借鉴 Mathport（Lean 迁移）、Why3（多证明器调度）和 MMT（数学知识管理）的多格式互操作设计。

### 2.2 导出格式枚举

```c
typedef enum Lv00ExportFormat {
    EXPORT_HTML   = 0, /**< HTML web page */
    EXPORT_LATEX  = 1, /**< LaTeX document */
    EXPORT_COQ    = 2, /**< Coq proof script */
    EXPORT_LEAN4  = 3, /**< Lean 4 proof script */
    EXPORT_JSON   = 4, /**< JSON structured data */
    EXPORT_DOT    = 5  /**< DOT (Graphviz) graph */
} Lv00ExportFormat;
```

| 格式 | 用途 | 特征 |
|------|------|------|
| HTML | 人类可读的 Web 页面 | 带样式的证明步骤展示，支持缩进 |
| LaTeX | 出版级排版 | 使用 `amsmath`/`amsthm` 宏包 |
| Coq | 机器可检验证明脚本 | `Theorem ... Proof. ... Qed.` 结构 |
| Lean 4 | 机器可检验证明脚本 | `import Mathlib; theorem ... := by` 结构 |
| JSON | 程序化消费 | 结构化数据，含完整步骤信息 |
| DOT | 图可视化 | Graphviz 有向图，步骤间有边连接 |

### 2.3 导出配置

```c
typedef struct Lv00ExportConfig {
    Lv00ExportFormat format;              /**< 目标导出格式 */
    bool             include_proof_trace; /**< 包含详细证明追踪 */
    bool             include_geometry;    /**< 包含几何构造数据 */
    bool             pretty_print;        /**< 启用美化输出/缩进 */
} Lv00ExportConfig;
```

### 2.4 导出结果

```c
typedef struct Lv00ExportResult {
    char   *output;       /**< 导出内容（null-terminated） */
    size_t  output_size;  /**< 输出长度（字节） */
    bool    success;      /**< 是否成功 */
    char   *error_msg;    /**< 错误信息（失败时） */
} Lv00ExportResult;
```

调用者须通过 `proof_export_result_destroy()` 释放结果。

### 2.5 证明步骤结构

```c
typedef struct Lv00ProofStep {
    int         step_id;    /**< 步骤唯一标识符 */
    const char *rule;       /**< 应用的规则名称 */
    const char *premise;    /**< 前提描述 */
    const char *conclusion; /**< 结论描述 */
    int         depth;      /**< 嵌套深度 */
} Lv00ProofStep;

typedef struct Lv00Proof {
    Lv00ProofStep *steps;   /**< 步骤数组 */
    size_t         n_steps; /**< 步骤数量 */
    const char    *theorem; /**< 定理陈述 */
} Lv00Proof;
```

### 2.6 StringBuffer 内部结构

实现中使用动态字符串缓冲区构建输出：

```c
typedef struct {
    char  *data;      /**< 缓冲区数据 */
    size_t size;      /**< 当前大小 */
    size_t capacity;  /**< 容量 */
} StringBuffer;
```

扩容策略：当空间不足时，容量翻倍或按 `size + len + 256` 取较大值。

### 2.7 各格式序列化策略

| 格式 | 序列化函数 | 关键特征 |
|------|-----------|----------|
| HTML | `export_html()` | DOCTYPE + CSS 样式 + div.proof-step 结构 |
| LaTeX | `export_latex()` | `documentclass{article}` + `begin{theorem/proof}` |
| Coq | `export_coq()` | `Theorem ... : Prop. Proof. ... Qed.` |
| Lean 4 | `export_lean4()` | `import Mathlib; theorem ... := by` |
| JSON | `export_json()` | 含 JSON 转义（`json_escape()`） |
| DOT | `export_dot()` | `digraph Proof { rankdir=TB; }` |

### 2.8 核心 API

| 函数 | 说明 |
|------|------|
| `proof_export_enhanced(proof, config)` | 将证明导出为指定格式 |
| `proof_export_from_navigator(theorem_name, format)` | 从导航器上下文导出（简化接口） |
| `proof_export_result_destroy(result)` | 销毁导出结果并释放内存 |

---

## 3 proof_trace.h -- 证明追踪树

### 3.1 设计概述

提供以树形结构记录证明过程的系统。每个节点记录推理步骤（使用的公理、前提、结论、子步骤），支持层次化嵌套子证明。可生成数学教科书风格的详细证明文本和 Mizar 风格的形式化证明。

### 3.2 前提描述结构

```c
typedef struct {
    int   premise_id;    /**< 前提唯一标识符 */
    char *description;   /**< 前提文字描述（可为 NULL） */
    bool  is_axiom;      /**< 是否为公理 */
} Lv00ProofPremise;
```

### 3.3 Lv00ProofTreeNode -- 证明树节点

```c
struct Lv00ProofTreeNode {
    int    id;                /**< 节点唯一标识符（树内自增） */
    int    step_index;        /**< 对应 ProofNavigator 中的步骤索引（-1=无关联） */

    char  *axiom_used;        /**< 使用的公理/规则名称 */
    Lv00ProofPremise *premises; /**< 前提数组 */
    int    premise_count;
    int    premise_capacity;

    char  *conclusion;        /**< 推导出的结论描述 */

    Lv00ProofTreeNode *parent;   /**< 父节点（根节点为 NULL） */
    Lv00ProofTreeNode **children; /**< 子节点数组 */
    int    child_count;
    int    child_capacity;

    int    depth;             /**< 树中深度 */
    bool   is_contradiction_branch; /**< 是否为反证法分支 */
    bool   is_lemma;          /**< 是否为引理（可折叠子证明） */
};
```

### 3.4 Lv00ProofTree -- 证明追踪树

```c
struct Lv00ProofTree {
    Lv00ProofTreeNode *root;           /**< 树根节点 */
    Lv00ProofTreeNode **all_nodes;     /**< 所有节点的线性数组（便于遍历） */
    int    node_count;
    int    node_capacity;

    int    total_steps;      /**< 总步骤数 */
    int    max_depth;        /**< 最大深度 */
    int    axiom_count;      /**< 使用的不同公理数量 */
    int    lemma_count;      /**< 引理数量 */
    int    contradiction_count; /**< 反证法分支数 */

    char  *theorem_name;     /**< 定理名称 */
    char  *proof_strategy;   /**< 证明策略描述 */
};
```

### 3.5 文本导出格式

#### 教科书风格

生成缩进格式的逐步证明文本，包含：

```
========================================
  定理证明: <theorem_name>
========================================

证明策略: <strategy>

步骤 1: 使用: <axiom> [前提: P1, P2] => <conclusion>
  步骤 2: 使用: <axiom> [前提: P3] => <conclusion>
  [反证法分支] 步骤 3: 使用: <axiom> => <conclusion> [矛盾!]
  [引理] 步骤 4: 使用: <axiom> => <conclusion>

========================================
证明共 N 步，最大深度 D
```

#### Mizar 风格

生成接近自然语言的形式化证明，使用 Mizar 关键字：

```mizar
:: Mizar风格形式化证明
:: 由Lv-00几何元语言内核生成

theorem
  <conclusion>
proof
  hence <conclusion> by P1, P2  :: 使用 <axiom>
  thus <conclusion> by P3
end;
```

### 3.6 内部常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `PROOF_TRACE_CHILD_CAP` | 4 | 初始子节点容量 |
| `PROOF_TRACE_PREMISE_CAP` | 4 | 初始前提容量 |
| `PROOF_TRACE_NODE_CAP` | 64 | 初始节点数组容量 |
| `PROOF_TRACE_INDENT` | 2 | 导出文本每级缩进宽度（空格） |
| `PROOF_TRACE_LINE_BUF` | 512 | 导出文本行缓冲区大小 |
| `PROOF_TREE_DESTROY_MAX_DEPTH` | 10000 | 递归销毁最大深度保护 |

### 3.7 核心 API

| 函数 | 说明 |
|------|------|
| `lv00_proof_tree_create(theorem_name, strategy)` | 创建证明追踪树 |
| `lv00_proof_tree_destroy(tree)` | 销毁证明追踪树 |
| `lv00_proof_tree_add_step(tree, parent, axiom, conclusion, index)` | 添加推理步骤 |
| `lv00_proof_tree_add_premise(node, id, desc, is_axiom)` | 添加前提 |
| `lv00_proof_tree_export_text(tree, filepath)` | 导出为教科书风格文本 |
| `lv00_proof_tree_export_mizar(tree, filepath)` | 导出为 Mizar 风格文本 |
| `lv00_proof_tree_mark_contradiction(node)` | 标记为反证法分支 |
| `lv00_proof_tree_mark_lemma(node)` | 标记为引理 |
| `lv00_proof_tree_get_stats(tree, steps, depth, axioms)` | 获取统计摘要 |

---

## 4 proof_widget.h -- 证明交互可视化组件

### 4.1 设计概述

借鉴 ProofWidgets4（Lean 社区）的 React 组件嵌入证明环境设计，为 Lv-00 的 Web GUI 前端提供 C API 数据契约。前端通过 JSON 序列化与本层通信，实现证明状态的双向同步。

### 4.2 Widget 类型枚举（8 种）

```c
typedef enum {
    WIDGET_GOAL_DISPLAY = 0,     /**< 目标显示：渲染当前证明目标的 HTML 树 */
    WIDGET_HYPOTHESIS_PANEL = 1, /**< 前提面板：显示可用前提，支持点击选中 */
    WIDGET_APPLY_BUTTON = 2,     /**< 策略按钮：用户点击回传策略到证明引擎 */
    WIDGET_STEP_NAVIGATOR = 3,   /**< 步骤导航：前进/后退/跳转 */
    WIDGET_SEARCH_TREE = 4,      /**< 搜索树：展示自动证明搜索的分支与节点 */
    WIDGET_TIMELINE = 5,         /**< 证明时间线：按时间顺序展示已完成步骤 */
    WIDGET_DEPENDENCY_GRAPH = 6, /**< 依赖图：展示定理/引理/前提的依赖关系 */
    WIDGET_TACTIC_HISTORY = 7    /**< 策略历史：展示已应用的策略序列 */
} ProofWidgetType;
```

### 4.3 布局类型枚举

```c
typedef enum {
    LAYOUT_GRID = 0,       /**< 网格布局（行列式） */
    LAYOUT_HORIZONTAL = 1, /**< 水平排列 */
    LAYOUT_VERTICAL = 2,   /**< 垂直排列（默认） */
    LAYOUT_TABBED = 3      /**< 标签页布局 */
} Lv00LayoutType;
```

### 4.4 高亮状态枚举

```c
typedef enum {
    HIGHLIGHT_NORMAL = 0,    /**< 默认状态 */
    HIGHLIGHT_ACTIVE = 1,    /**< 当前步骤 */
    HIGHLIGHT_COMPLETED = 2, /**< 已完成 */
    HIGHLIGHT_FAILED = 3,    /**< 失败 */
    HIGHLIGHT_SEARCHING = 4  /**< 搜索中（带动画） */
} Lv00HighlightState;
```

### 4.5 核心数据结构

#### ProofWidgetState -- Widget 状态

```c
typedef struct ProofWidgetState {
    int widget_id;               /**< Widget 唯一标识符 */
    ProofWidgetType widget_type; /**< Widget 类型 */
    bool is_active;              /**< 是否活动 */
    bool is_enabled;             /**< 是否启用 */
    char *display_label;         /**< 显示标签 */
    int bound_step_id;           /**< 绑定的证明步骤 ID */
    char *interaction_data;      /**< 用户交互数据 JSON */
} ProofWidgetState;
```

#### Lv00HypothesisEntry -- 前提条目

```c
struct Lv00HypothesisEntry {
    int hyp_id;       /**< 前提标识符 */
    char *name;       /**< 前提名称 */
    char *type_text;  /**< 前提类型文本 */
    char *value_text; /**< 前提值文本 */
    int source_step;  /**< 来源步骤 ID */
    bool is_selected; /**< 是否被用户选中 */
};
```

#### Lv00GoalDisplay -- 证明目标显示

```c
struct Lv00GoalDisplay {
    Lv00HypothesisEntry *hypotheses; /**< 前提条目数组 */
    int hyp_count;
    char *goal_text;                 /**< 目标文本 */
    char **context_terms;            /**< 上下文可用项名称数组 */
    int context_count;
    int depth;                       /**< 目标嵌套深度 */
    bool is_solved;                  /**< 目标是否已解决 */
};
```

#### Lv00ProofStepHighlight -- 步骤高亮

```c
struct Lv00ProofStepHighlight {
    int step_id;              /**< 证明步骤 ID */
    Lv00HighlightState color; /**< 高亮颜色/状态 */
    bool is_animated;         /**< 是否需要动画效果 */
    float progress;           /**< 动画进度（0.0 ~ 1.0） */
    char *tooltip_text;       /**< 工具提示文本 */
};
```

#### Lv00WidgetLayout -- 构件布局

```c
struct Lv00WidgetLayout {
    ProofWidgetState *widgets;  /**< Widget 状态数组 */
    int widget_count;
    int widget_capacity;
    Lv00LayoutType layout_type; /**< 布局类型 */
    int columns;                /**< 列数（仅 GRID 有效） */
    int rows;                   /**< 行数（仅 GRID 有效） */
    int *order_indices;         /**< Widget 顺序索引数组 */
    char *persistence_key;      /**< 持久化 key */
};
```

### 4.6 智能策略推荐

基于当前目标和可用前提的启发式算法，推荐最有可能成功的证明策略：

| 策略 | 基础置信度 | 触发条件 |
|------|-----------|----------|
| `intro` | 0.9 | 有隐含/全称目标 |
| `apply` | 0.8 | 前提中有匹配的命题 |
| `cases` | 0.7 | 有析取目标 |
| `rewrite` | 0.7 | 有等式 |
| `induction` | 0.5 | 有归纳类型目标 |
| `reflexivity` | 0.4 | 等式两端相同 |
| `symmetry` | 0.3 | 对称关系 |
| `transitivity` | 0.3 | 传递关系 |
| `congruence` | 0.3 | 同余关系 |
| `subst` | 0.3 | 替换关系 |

### 4.7 搜索树与依赖图 JSON 序列化

#### 搜索树 JSON

```json
{
  "type": "search_tree",
  "nodes": [
    {"id": 0, "status": "completed"},
    {"id": 1, "status": "pending"}
  ]
}
```

#### 依赖图 JSON

```json
{
  "type": "dependency_graph",
  "nodes": [],
  "edges": [],
  "step_count": 0,
  "is_complete": false
}
```

#### 布局导出 JSON

```json
{
  "layout_type": "VERTICAL",
  "persistence_key": "proof_panel_main",
  "widgets": [
    {"id": 0, "type": "GOAL_DISPLAY", "label": "Current Goal",
     "active": true, "enabled": true, "bound_step": -1}
  ]
}
```

### 4.8 策略回传机制

前端用户点击策略按钮后，通过 `proof_widget_apply_tactic()` 将策略名称和参数回传到证明引擎。支持的策略包括 `intro`、`apply`、`cases`、`rewrite`、`induction`、`reflexivity` 等。引擎执行后通过查询接口（`proof_widget_get_goal`、`proof_widget_get_step_highlights`）获取更新后的渲染数据。

### 4.9 核心 API

#### 生命周期

| 函数 | 说明 |
|------|------|
| `proof_widget_init(layout_capacity)` | 初始化 Widget 系统 |
| `proof_widget_destroy(layout)` | 销毁 Widget 系统 |

#### Widget 注册与更新

| 函数 | 说明 |
|------|------|
| `proof_widget_register(layout, type, label, bound_step)` | 注册新 Widget |
| `proof_widget_update(layout, id, active, enabled, label, step, json)` | 更新 Widget 状态 |

#### 证明状态查询

| 函数 | 说明 |
|------|------|
| `proof_widget_get_goal(navigator, out_goal)` | 获取当前证明目标数据 |
| `proof_widget_get_hypotheses(navigator, out, max)` | 获取当前前提数据 |
| `goal_display_free(goal)` | 释放目标显示数据 |

#### 智能推荐与可视化

| 函数 | 说明 |
|------|------|
| `proof_widget_suggest_tactic(nav, suggestions, confidences, max)` | 智能推荐证明策略 |
| `proof_widget_get_step_highlights(nav, highlights, max)` | 获取步骤高亮状态 |
| `proof_widget_get_search_tree(navigator)` | 获取搜索树 JSON |
| `proof_widget_get_dependency_graph(navigator)` | 获取依赖图 JSON |

#### 布局导出与策略回传

| 函数 | 说明 |
|------|------|
| `proof_widget_export_layout(layout)` | 导出布局 JSON |
| `proof_widget_apply_tactic(nav, name, args, success, feedback)` | 回传策略应用 |

#### 布局管理

| 函数 | 说明 |
|------|------|
| `proof_widget_set_layout_type(layout, type, cols, rows)` | 设置布局类型 |
| `proof_widget_set_persistence_key(layout, key)` | 设置持久化 key |
| `proof_widget_set_order(layout, indices, count)` | 设置 Widget 显示顺序 |

---

## 5 实现文件

| 模块 | 头文件 | 源文件 |
|------|--------|--------|
| proof_export_enhanced | `core/include/lv00/proof_export_enhanced.h` | `core/src/layer5_output/proof_export_enhanced.c` |
| proof_trace | `core/include/lv00/proof_trace.h` | `core/src/layer4_reasoning/proof_trace.c` |
| proof_widget | `core/include/lv00/proof_widget.h` | `core/src/layer5_output/proof_widget.c` |

## 6 依赖

| 依赖模块 | 使用方 | 用途 |
|----------|--------|------|
| `proof.h` | proof_widget | ProofNavigator、ProofStep 等核心类型 |
| `constraint_graph.h` | proof_widget | 约束图核心数据结构 |
| `lv00.h` | proof_export_enhanced | LV00_PUBLIC_API 宏 |
| `lv00_utils.h` | proof_trace, proof_widget | 统一内存分配器 |
| `lv00_internal.h` | proof_widget | 内部常量与工具宏 |
| `error_codes.h` | proof_widget | 统一错误码系统 |

## 7 内部常量

| 常量 | 值 | 所属模块 |
|------|-----|----------|
| `PROOF_WIDGET_INITIAL_CAPACITY` | 8 | proof_widget |
| `PROOF_WIDGET_CTX_INITIAL_CAPACITY` | 16 | proof_widget |
| `PROOF_WIDGET_MAX_SUGGESTIONS` | 10 | proof_widget |
| `PROOF_WIDGET_JSON_BUFFER` | 16384 | proof_widget |
| `PROOF_WIDGET_ESCAPE_BUFFER` | 128 | proof_widget |
| `MIZAR_BUF_INIT_SIZE` | 8192 | proof_trace |
| `MIZAR_INDENT` | `"  "` | proof_trace |
