# Lv-00 参考落地设计文档：Cadabra WYSIWYG 公式编辑模式

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: Cadabra (github.com/kpeeters/cadabra2) —— 张量场论 CAS，"所见即所得+LaTeX 源码"双向编辑
> **目标**: 借鉴 Cadabra 的"所见即所得+LaTeX 源码"双向编辑模式，映射到 Lv-00 Web GUI 的 FormulaPanel 公式编辑体验

---

## 目录

1. [项目概述与 Lv-00 借鉴动机](#1-项目概述与-lv-00-借鉴动机)
2. [核心借鉴要点：双向编辑模式](#2-核心借鉴要点双向编辑模式)
3. [Lv-00 FormulaPanel 映射方案](#3-lv-00-formulapanel-映射方案)
4. [公式编辑器组件设计](#4-公式编辑器组件设计)
5. [几何对象的可视化编辑](#5-几何对象的可视化编辑)
6. [与现有模块的集成](#6-与现有模块的集成)
7. [实现路线图](#7-实现路线图)
8. [关键映射表](#8-关键映射表)

---

## 1. 项目概述与 Lv-00 借鉴动机

### 1.1 Cadabra 是什么

Cadabra 是专门为**张量场论和高能物理**设计的计算机代数系统，由 Kasper Peeters 开发。其最突出的特点是**"所见即所得+LaTeX 源码"双向编辑模式**——用户看到的是渲染后的漂亮数学公式（像在 LaTeX PDF 中一样），但背后自动生成和维护对应的 LaTeX 源码。

```
Cadabra 的双向编辑:
┌─────────────────────────────────┐
│  渲染视图（用户看到的）:          │
│                                 │
│     ∂                           │
│    ── ( √(-g) g^{μν} ∂ φ ) = 0│
│    ∂x^{μ}                   ν   │
│                                 │
├─────────────────────────────────┤
│  LaTeX 源码（自动生成/更新）:     │
│  \partial_{\mu}                │
│  \left( \sqrt{-g} g^{\mu\nu}  │
│  \partial_{\nu} \phi \right)   │
│  = 0                           │
└─────────────────────────────────┘
         ↕ 双向同步
用户在任一视图中编辑，另一视图自动更新
```

关键特性：

- **所见即所得（WYSIWYG）渲染**：数学公式实时渲染为 LaTeX 质量的排印效果
- **LaTeX 源码同步**：所有编辑器操作同时生成可复用的 LaTeX 源码
- **单元格式笔记本**：类似 Jupyter Notebook 但专为数学公式优化
- **输入辅助**：命令补全、模板插入、括号匹配
- **Cadabra 脚本**：每个 Cell 可附加 Python 脚本用于执行符号计算

### 1.2 Lv-00 借鉴动机

Lv-00 的几何公式编辑面临双重需求——用户既需要直观地看到几何体的可视化表示（图形），又需要精确地编辑几何体的参数（数值/符号坐标、约束条件）。Cadabra 的"公式即代码"双向模式为这种需求提供了成熟的 UX 参考：

| 借鉴方向 | Cadabra 特性 | Lv-00 现有基础 | 差距 |
|:---|:---|:---|:---|
| **公式渲染** | LaTeX 数学公式实时渲染 | `formula_renderer.js` 可渲染简单公式 | 缺几何体专用渲染 |
| **源码同步** | 渲染视图与 LaTeX 源码双向同步 | `formula_parser.js` / `graph_to_formula.js` 单向转换 | 缺双向同步机制 |
| **命令补全** | LaTeX 命令的上下文补全 | 无 | 缺几何构造关键字补全 |
| **模板系统** | 公式模板快速插入 | 无 | 缺几何构造模板 |
| **单元格交互** | 每个公式是独立可编辑单元格 | `app.js` + `ui.js` 基础框架 | 需完整 FormulaPanel 实现 |
| **LaTeX 内嵌** | 源码编辑器中嵌入 LaTeX 渲染 | `mathlive_integration.js`（已有） | 需与 DSL 编辑器深度集成 |

### 1.3 总体架构对照

```
Cadabra                                Lv-00 FormulaPanel
─────────────────────────────────────────────────────────────
Cadabra Notebook (GUI)           →    Web GUI FormulaPanel
  TeXView (渲染视图)             →    FormulaRenderer (Canvas/SVG + MathLive)
  TeXSource (源码视图)           →    FormulaEditor (CodeMirror + MathLive)
 双向同步引擎                    →    formula_module.js (parser + converter)
 命令补全                        →    lv00_autocomplete.js
 Cell 抽象                       →    FormulaCell (JS 类)
 Python 后端引擎                 →    WASM Lv-00 引擎 (lv00_web_bindings.c)
```

---

## 2. 核心借鉴要点：双向编辑模式

### 2.1 三种视图模式

Cadabra 的编辑器同时提供三种视图：

| 视图 | 用途 | 用户操作 |
|:---|:---|:---|
| **WYSIWYG 视图** | 看到 LaTeX 质量的渲染公式 | 直接点击公式元素编辑、拖拽重组 |
| **LaTeX 源码视图** | 查看/编辑底层 LaTeX 代码 | 键盘输入标准 LaTeX 语法 |
| **Python 脚本视图** | 执行符号计算 | 编写 Cadabra Python API 脚本 |

Lv-00 FormulaPanel 需要三种对应视图：

| Cadabra 视图 | Lv-00 视图 | 说明 |
|:---|:---|:---|
| WYSIWYG 视图 | **几何画布视图** | Canvas/SVG 上显示几何体，可拖拽、缩放、选中 |
| LaTeX 源码视图 | **DSL 源码视图** | 显示/编辑 Lv-00 DSL 代码（如 `point A(0,0); line l = segment(A,B);`） |
| Python 脚本视图 | **DSL 源码视图（复用）** | Lv-00 DSL 本身就是"脚本"层，无需额外视图 |

### 2.2 双向同步的三条通道

借鉴 Cadabra 的双向同步机制，Lv-00 FormulaPanel 在三个表示之间建立双向同步：

```
                  formula_parser.js
DSL 源码 ←──────────────────────────→ AST (公式抽象语法树)
   ↕          formula_to_graph.js        ↕     graph_to_formula.js
几何画布 ←──────────────────────────→ 约束图 (ConstraintGraph)
              render.js (重绘)
```

这三条通道的设计原则：

1. **DSL → AST**：修改 DSL 源码时，增量解析为 AST，保持未修改部分不变
2. **AST → 约束图**：AST 变更触发约束图更新，约束求解器自动重算受影响的几何体坐标
3. **约束图 → 几何画布**：坐标变化触发画布重绘（只重绘受影响的几何体）
4. **几何画布 → DSL**：用户拖拽几何对象时，新的坐标值回写到 DSL 源码（如 `point A(12, 34)` 更新为 `point A(15, 38)`）

### 2.3 增量同步而非全量刷新

Cadabra 的关键性能优化是**增量同步**——只有被修改的单元格/表达式才重新渲染。Lv-00 FormulaPanel 同样需要增量更新：

```c
/**
 * @brief 约束图的增量更新提示 —— 借鉴 Cadabra 的增量渲染
 */
typedef struct {
    bool node_dirty;           /* 节点坐标/类型是否变更 */
    bool constraint_dirty;     /* 约束是否变更 */
    bool rendering_dirty;      /* 是否需要重新渲染 */
    int affected_node_count;   /* 受影响的节点数 */
    int *affected_node_ids;    /* 受影响的节点 ID 列表 */
} IncrementalUpdateHint;

/**
 * @brief 增量更新画布 —— 只重绘标记为 dirty 的节点
 *
 * Cadabra 等价: 只重新渲染被修改的 TeXBox，不动其他单元格
 */
void formula_panel_incremental_render(
    ConstraintGraph *graph,
    const IncrementalUpdateHint *hint,
    CanvasContext *canvas);
```

---

## 3. Lv-00 FormulaPanel 映射方案

### 3.1 FormulaPanel 三面板布局

借鉴 Cadabra 的笔记本界面，Lv-00 FormulaPanel 设计为三面板布局：

```
┌──────────────────────────────────────────────────────────────┐
│ Lv-00 FormulaPanel                                           │
├──────────────┬───────────────────────┬───────────────────────┤
│              │                       │                       │
│  几何画布     │  DSL 源码编辑器        │  属性面板              │
│  (Canvas/SVG)│  (CodeMirror+MathLive)│  (PropertyPanel)      │
│              │                       │                       │
│  - 几何体渲染 │  - 语法高亮            │  - 选中对象属性编辑     │
│  - 拖拽交互   │  - 自动补全            │  - 类型信息显示        │
│  - 约束可视化 │  - LaTeX 公式内嵌      │  - 约束列表            │
│  - 证明步骤   │  - Live Preview       │  - 公理包信息           │
│    动画       │                       │                       │
│              │                       │                       │
├──────────────┴───────────────────────┴───────────────────────┤
│ 状态栏: 当前模式 | 选中对象 | 类型 | 坐标 | 证明颜色           │
└──────────────────────────────────────────────────────────────┘
```

### 3.2 几何体渲染引擎

借鉴 Cadabra 的 LaTeX 渲染器，Lv-00 几何画布需要渲染带标签的几何对象：

```javascript
/**
 * @brief 几何体渲染器 —— 借鉴 Cadabra TeXView 的渲染管线
 *
 * Cadabra 管线: LaTeX源码 → pdflatex → 位图 → 屏幕显示
 * Lv-00 管线: ConstraintGraph → GeometryRenderer → Canvas 2D/SVG
 *
 * 渲染层级（从底到顶）:
 *   z=0: 网格/坐标轴
 *   z=1: 区域填充（三角形/圆/多边形内部）
 *   z=2: 线/圆弧/样条
 *   z=3: 点（带标签）
 *   z=4: 约束标注（共线标记、垂直标记等）
 *   z=5: 拖拽手柄/选中高亮
 */
class GeometryRenderer {
    /**
     * 渲染几何节点
     * @param {GeomNode} node - 几何节点
     * @param {Object} style  - 渲染样式（颜色、线宽等）
     * @param {boolean} incremental - 是否增量渲染
     */
    renderNode(node, style, incremental = false);

    /**
     * 渲染 LaTeX 标签 —— 借鉴 Cadabra 的公式渲染
     * 几何体标签（如 A, B, l1）使用 MathLive 在 Canvas 上渲染
     */
    renderLaTeXLabel(text, position, style);

    /** 渲染约束标注——共线/垂直/平行等符号标记 */
    renderConstraintAnnotation(constraint, style);
}
```

### 3.3 DSL 源码编辑器的 LaTeX 内嵌

借鉴 Cadabra 在源码视图中嵌入 LaTeX 渲染的能力，Lv-00 DSL 编辑器同样内嵌公式渲染：

```
Cadabra 源码编辑器:
  ex := \int_{-\infty}^{\infty}{ e^{-x^2} } dx;
  //     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ 渲染为漂亮的积分公式

Lv-00 DSL 编辑器:
  point A(0, 0);
  //  "A" 以 LaTeX 风格渲染（斜体，与其他数学符号一致）
  //  "(0, 0)" 渲染为 LaTeX 坐标对
  //  整行同时是合法的 DSL 代码和美观的数学表达

  line l = segment(A, B);
  //  "segment(A, B)" 渲染为 LaTeX $\overline{AB}$
```

这种内嵌渲染通过 MathLive 组件实现：

```javascript
/**
 * @brief DSL 编辑器中的 LaTeX 公式内嵌 —— 借鉴 Cadabra 的 TeXSource
 *
 * 使用 MathLive 组件将 DSL 中的数学表达式片段替换为渲染的 LaTeX。
 * DSL 关键字保持原始文本样式，数学表达式转换为 LaTeX 渲染。
 *
 * 示例: "point A(0, 0);" →
 *   <span class="keyword">point</span>
 *   <mathlive-mathfield>A</mathlive-mathfield>
 *   <mathlive-mathfield>(0, 0)</mathlive-mathfield>
 *   <span class="punctuation">;</span>
 */
function embedLatexInDSL(dslCode) {
    const tokens = tokenizeDSL(dslCode);
    return tokens.map(token => {
        if (token.type === 'IDENTIFIER' || token.type === 'NUMBER') {
            return `<mathlive-mathfield>${token.text}</mathlive-mathfield>`;
        }
        return `<span class="${token.type}">${token.text}</span>`;
    }).join('');
}
```

---

## 4. 公式编辑器组件设计

### 4.1 自动补全系统

借鉴 Cadabra 的上下文感知命令补全，为 Lv-00 DSL 提供智能补全：

```javascript
/**
 * @brief Lv-00 DSL 自动补全 —— 借鉴 Cadabra 的命令补全
 *
 * Cadabra 根据当前作用域（Python 环境中的可用函数）
 * 提供上下文感知的补全建议。
 *
 * Lv-00 根据以下上下文提供补全：
 *   1. 当前作用域中已知的几何对象名（A, B, l1, circle_k, ...）
 *   2. 已加载公理包中的操作符（midpoint, centroid, ...）
 *   3. 当前光标位置的语法期望（类型后建议变量名，等号后建议表达式）
 */
const AUTOCOMPLETE_PROVIDERS = {
    KEYWORD: {
        trigger: /^[a-z_]+$/,
        completions: ['point', 'line', 'circle', 'segment', 'midpoint',
                      'intersection', 'perpendicular', 'parallel',
                      'funcblock', 'proposition', 'prove', 'export']
    },
    GEOM_OBJECT: {
        trigger: /^[A-Z][a-zA-Z0-9_]*$/,
        source: 'constraintGraph.getNodeNames()'
    },
    OPERATOR: {
        trigger: /^[a-z_]+\($/,
        source: 'funcBlockRegistry.getOperationNames()'
    },
    STRATEGY: {
        trigger: /strategy\s*=\s*$/,
        completions: ['direct_construction', 'area_method', 'grobner_basis',
                      'vector_method', 'full_angle', 'deductive_db', 'oracle']
    }
};
```

### 4.2 模板系统

借鉴 Cadabra 的公式模板快速插入，为 Lv-00 提供几何构造模板：

```
Cadabra 模板:                           Lv-00 构造模板:
┌────────────────────┐                 ┌───────────────────────┐
│ \int{#1}^{#2}{#3}  │                 │ point #1(#2, #3);     │
│ \frac{#1}{#2}      │                 │ line #1 = segment(#2, │
│ \sqrt{#1}          │                 │   #3);                │
│ ...                │                 │ circle #1 =           │
└────────────────────┘                 │   circle_center_radius│
                                       │   (#2, #3);           │
                                       │ prove #1 using        │
                                       │   strategy=#2;        │
                                       ...                     │
                                       └───────────────────────┘
```

### 4.3 公式单元（FormulaCell）

借鉴 Cadabra 的 Cell 抽象，每个几何构造语句对应一个 FormulaCell：

```javascript
/**
 * @brief 公式单元 —— 借鉴 Cadabra 的 Cell 抽象
 *
 * Cadabra 的每个 Cell 是一个独立的可编辑/可执行单元。
 * Lv-00 的每个几何构造语句同样是一个 FormulaCell。
 *
 * Cadabra Cell 生命周期: create → edit → execute → display result
 * Lv-00 Cell 生命周期: create → edit → parse → build graph → render
 */
class FormulaCell {
    constructor(type, content) {
        this.id = generateCellId();
        this.type = type;      // 'geometry' | 'proof' | 'export' | 'comment'
        this.content = content; // DSL 源码
        this.result = null;    // 执行结果（AST 节点 ID 或证明状态）
        this.state = 'idle';   // 'idle' | 'editing' | 'executing' | 'done' | 'error'
        this.renderedDOM = null;
        this.isDirty = true;
    }

    /** 解析 Cell 内容为 AST */
    parse() {
        this.ast = formulaParser.parse(this.content);
        this.state = 'parsed';
    }

    /** 将 AST 转换为约束图更新 */
    applyToGraph(constraintGraph) {
        formulaToGraph.apply(this.ast, constraintGraph);
        this.state = 'done';
        this.isDirty = false;
    }

    /** 从约束图反序列化回 DSL 源码 */
    syncFromGraph(constraintGraph, nodeIds) {
        this.content = graphToFormula.convert(constraintGraph, nodeIds);
        this.isDirty = true;
    }
}
```

---

## 5. 几何对象的可视化编辑

### 5.1 画布上的直接操纵

Cadabra 的 WYSIWYG 视图允许用户点击公式中的元素直接编辑。Lv-00 几何画布同样支持直接操纵：

| 操作 | 画布手势 | 对应 DSL 变更 | Cadabra 等价 |
|:---|:---|:---|:---|
| 拖拽点 | 鼠标拖拽点 | `point A(x, y)` 坐标更新 | 点击 TeX 原子修改 |
| 连接两点 | 按住 Shift 点击 A 再点 B | `segment(A, B)` 自动生成 | 无直接等价 |
| 画圆 | 按住 C 点击圆心，拖拽到半径点 | `circle k = circle_center_radius(O, P)` | 无直接等价 |
| 选中多个对象 | 矩形框选 | 选中的节点 ID 集合 → 属性面板 | 选中 TeX 子表达式 |
| 删除对象 | 选中后按 Delete | 从 DSL 源码中删除对应语句 | 删除 TeX 原子 |

### 5.2 拖拽与约束保持

借鉴 Cadabra "修改参数后自动重算"的设计，Lv-00 画布拖拽应保持约束一致性：

```javascript
/**
 * @brief 拖拽几何对象时的约束保持 —— 借鉴 Cadabra 的增量重算
 *
 * 当用户拖拽一个点时，所有依赖该点的几何体（线段、圆、交点）
 * 都需要根据约束系统重新计算位置。
 *
 * Cadabra 等价: 修改一个符号的值后，所有引用了该符号的表达式
 * 在当前 Cell 中自动重新求值。
 */
class DragConstraintMaintainer {
    /**
     * 开始拖拽一个几何节点
     * @param {number} nodeId - 被拖拽的节点 ID
     */
    beginDrag(nodeId) {
        this.draggedNode = nodeId;
        this.affectedNodes = constraintGraph.findDependents(nodeId);
        this.snapshot = constraintGraph.createSnapshot();
    }

    /**
     * 拖拽过程中增量更新
     * @param {number} newX, newY - 新的目标位置
     */
    onDrag(newX, newY) {
        // 1. 更新被拖拽节点的坐标
        constraintGraph.setNodeSymbolicCoord(this.draggedNode, newX, newY);

        // 2. 重算所有受影响的节点
        for (const affectedId of this.affectedNodes) {
            solver.solveIncremental(affectedId, constraintGraph);
        }

        // 3. 增量重绘
        const hint = { affected_node_ids: [this.draggedNode, ...this.affectedNodes] };
        renderer.incrementalRender(constraintGraph, hint);

        // 4. 更新 DSL 源码（如果有源码视图打开）
        dslEditor.syncFromGraph(this.draggedNode);
    }

    /** 结束拖拽——提交或回滚 */
    endDrag(commit = true) {
        if (!commit) {
            constraintGraph.restoreSnapshot(this.snapshot);
            renderer.fullRender(constraintGraph);
        }
        this.draggedNode = null;
    }
}
```

### 5.3 画布与 DSL 编辑器的焦点同步

Cadabra 在用户切换 WYSIWYG 视图和 LaTeX 源码视图时，始终保持光标/选中位置同步。Lv-00 同样需要在几何画布和 DSL 编辑器之间保持焦点同步：

```
用户点击画布上的点A → DSL编辑器自动滚动到 point A(...); 行并高亮
用户在DSL编辑器中选中 point B(...); → 画布自动居中到点B并高亮
```

---

## 6. 与现有模块的集成

### 6.1 与 MathLive 的集成

Lv-00 已经集成了 MathLive（`web/js/mathlive_integration.js`，1108 行），提供了几何宏注册、实时预览等功能。Cadabra 风格的双向编辑可以在 MathLive 基础上叠加：

```javascript
/**
 * @brief FormulaPanel 的 MathLive 增强层 —— 借鉴 Cadabra 的双向编辑
 *
 * 在 MathLive 提供的基础 LaTeX 编辑能力之上，
 * 增加 Cadabra 风格的"源码 ↔ 渲染"双向同步功能。
 */
class MathLiveCadabraAdapter {
    constructor(mathFieldElement) {
        this.mathField = new MathLive.MathfieldElement();
        this.mathField.addEventListener('input', (evt) => {
            this.onMathFieldChanged(evt);
        });
    }

    /** MathLive 内容变化 → 更新 DSL 源码和约束图 */
    onMathFieldChanged(evt) {
        const latex = this.mathField.getValue('latex');
        const dslCode = this.latexToDSL(latex);
        this.dslEditor.setValue(dslCode);
        this.syncToGraph(dslCode);
    }

    /** 约束图变化 → 更新 MathLive 渲染 */
    onGraphChanged(nodeIds) {
        const dslCode = graphToFormula.convert(constraintGraph, nodeIds);
        const latex = this.dslToLatex(dslCode);
        this.mathField.setValue(latex, { suppressChangeEvent: true });
    }

    /** DSL ↔ LaTeX 互转 */
    latexToDSL(latex) { /* LaTeX 几何表达式 → DSL 语句 */ }
    dslToLatex(dsl) { /* DSL 语句 → LaTeX 几何表达式 */ }
}
```

### 6.2 与现有前端模块的集成关系

| 现有模块 | FormulaPanel 中的角色 |
|:---|:---|
| `formula_parser.js` | DSL 源码 → AST 的解析器 |
| `formula_to_graph.js` | AST → ConstraintGraph 的编译器 |
| `graph_to_formula.js` | ConstraintGraph → DSL 源码的反编译 |
| `formula_renderer.js` | Canvas/SVG 上的几何体渲染 |
| `formula_module.js` | 公式解析/渲染/转换的总调度 |
| `mathlive_integration.js` | LaTeX 公式的 WYSIWYG 编辑基础 |
| `mathjson_protocol.js` | 结构化的数学通信协议 |
| `spell_compiler.js` | Spell 四阶段编译（用于 Cell 执行） |
| `ui.js` / `interaction.js` | 画布交互事件处理 |
| `lv00_web_bindings.c` | WASM 后端计算引擎 |

---

## 7. 实现路线图

### 7.1 第一阶段：FormulaCell 基础框架（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| `FormulaCell` JS 类 | `web/js/formula_cell.js`（新文件） | 借鉴 Cadabra Cell 的编辑/执行/显示生命周期 |
| 三面板布局 UI | `web/js/formula_panel.js`（新文件） | 几何画布 + DSL 编辑器 + 属性面板 |
| 自动补全提供者 | `web/js/lv00_autocomplete.js`（新文件） | 上下文感知的 DSL 关键字/对象/操作符补全 |
| 模板库 | `web/js/lv00_templates.js`（新文件） | 几何构造模板（点/线/圆/证明/导出） |
| 几何画布渲染器 | `web/js/geometry_renderer.js`（新文件） | Canvas 2D 上的几何体分层渲染 |

**预估规模**：约 400 行 JS 代码

### 7.2 第二阶段：双向同步引擎（P3）

| 任务 | 文件 | 说明 |
|:---|:---|:---|
| DSL ↔ AST 增量同步 | `web/js/incremental_sync.js`（新文件） | Cadabra 风格的增量解析/反编译 |
| AST ↔ 约束图同步 | `web/js/graph_sync.js`（新文件） | 约束图变更的脏标记和增量传播 |
| 约束图 → 画布增量渲染 | `web/js/geometry_renderer.js`（扩展） | `IncrementalUpdateHint` 驱动只重绘 dirty 节点 |
| 画布拖拽 → DSL 回写 | `web/js/interaction.js`（扩展） | `DragConstraintMaintainer` 拖拽保持约束 |
| MathLive 双向适配层 | `web/js/mathlive_cadabra_adapter.js`（新文件） | MathLive ↔ DSL ↔ 约束图 三角同步 |

**预估规模**：约 350 行 JS 代码

### 7.3 第三阶段：高级编辑体验（P4，远期）

| 任务 | 说明 |
|:---|:---|
| Cadabra 风格 LaTeX 内嵌 | 在 DSL 源码编辑器中，数学表达式以 LaTeX 渲染显示 |
| 证明步骤动画 | 借鉴 Manim 的动画编排 + Cadabra 的单元执行，将证明步骤动画化 |
| 协作编辑 | 多用户同时编辑同一个几何构造（类似 Cadabra 的共享笔记本） |
| 导出到 Cadabra | `.lvz` 中的几何公式可导出为 Cadabra 可执行的 Python 脚本 |

---

## 8. 关键映射表

### 8.1 Cadabra → Lv-00 概念映射

| Cadabra 概念 | Lv-00 映射 | Lv-00 文件 |
|:---|:---|:---|
| `Cell` | `FormulaCell` | `web/js/formula_cell.js`（新） |
| `TeXView`（渲染视图） | `GeometryRenderer` (Canvas 2D) | `web/js/geometry_renderer.js`（新） |
| `TeXSource`（源码视图） | `DSLEditor` (CodeMirror) | `web/js/formula_panel.js`（新） |
| `Notebook` | `FormulaPanel`（三面板布局） | `web/js/formula_panel.js`（新） |
| 双向同步 | 增量同步引擎 (DSL ↔ AST ↔ Graph ↔ Canvas) | `web/js/incremental_sync.js`（新） |
| 命令补全 | `AUTOCOMPLETE_PROVIDERS` | `web/js/lv00_autocomplete.js`（新） |
| 公式模板 | `GEOM_TEMPLATES` | `web/js/lv00_templates.js`（新） |
| Python 后端 | WASM Lv-00 引擎 | `web/lv00_web_bindings.c` |
| 增量重算 | `DragConstraintMaintainer` | `web/js/interaction.js`（扩展） |
| LaTeX 渲染引擎 | MathLive 组件 | `web/js/mathlive_integration.js`（已有） |

### 8.2 编辑体验对照

| 操作 | Cadabra | Lv-00 FormulaPanel |
|:---|:---|:---|
| 输入公式 | 键盘输入 LaTeX 命令 | 键盘输入 DSL 关键字 |
| 修改公式 | 在 WYSIWYG 视图中点击编辑 | 在画布上拖拽几何对象 |
| 查看源码 | 切换到 TeXSource 视图 | 右侧面板显示/编辑 DSL 源码 |
| 执行计算 | `Ctrl+Enter` 执行当前 Cell | `Ctrl+Enter` 解析当前构造，更新约束图 |
| 引用结果 | Cell 输出可在后续 Cell 中引用 | `:N` 标记引用历史输出 |
| 模板插入 | 模板菜单选择插入 | 模板菜单选择插入几何构造 |

---

## 附录 A：Cadabra 风格的双向同步完整数据流

```
用户操作                  数据流                      视图更新
────────────────────────────────────────────────────────────
在DSL编辑器中               公式解析器
输入 point A(10,20);   →   formula_parser.js    →   AST
                                                    │
                                                    ▼
                                              约束图编译器
                                              formula_to_graph.js
                                                    │
                                                    ▼
                                              ConstraintGraph
                                              (A: GEOM_POINT
                                               coords=(10,20))
                                                    │
                                                    ▼
                                              几何渲染器
                                              geometry_renderer.js
                                                    │
                                                    ▼
用户看到:               ←   画布重绘             ←   画布上显示点A
  点A出现在(10,20)

---
用户拖拽点A             →   坐标更新              →   ConstraintGraph
从(10,20)到(15,25)          (A.coords: 15,25)      节点坐标变更
                                                    │
                              ┌─────────────────────┘
                              │
                    ┌─────────┴──────────┐
                    ▼                    ▼
              几何渲染器             DSL反编译器
              incremental_render    graph_to_formula.js
                    │                    │
                    ▼                    ▼
用户看到:        画布上点A           DSL源码更新为
  点A移动到(15,25)               point A(15, 25);
```

---

> **文档结束**
> 本文档详述了 Cadabra "所见即所得+LaTeX 源码"双向编辑模式如何映射到 Lv-00 Web GUI 的 FormulaPanel 公式编辑体验。核心设计：通过 FormulaCell 抽象、三面板布局（几何画布/DSL 编辑器/属性面板）、三条双向同步通道（DSL ↔ AST ↔ ConstraintGraph ↔ Canvas），实现"在画布上拖拽几何体 → DSL 源码自动更新，反之亦然"的流畅编辑体验。
