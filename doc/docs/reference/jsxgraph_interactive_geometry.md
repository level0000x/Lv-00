# JSXGraph 交互式几何引擎核心借鉴设计

> **借鉴项目**：JSXGraph（jsxgraph.org, github.com/jsxgraph/jsxgraph）
> **核心借鉴点**：Board-Element 两层模型、依赖图驱动的自动约束更新、声明式几何构造 API、渲染器抽象（SVG/Canvas）
> **分类**：P3 中优先级 / Web 交互几何渲染
> **日期**：2026-05-24

---

## 1. 概述

JSXGraph 是由德国拜罗伊特大学（University of Bayreuth）数学系自 2008 年起维护的开源跨浏览器 JavaScript 交互几何库，支持几何构造、函数绘制、图表和数据可视化。截至 2026 年，JSXGraph 已累积超过 1000 颗 GitHub Star，被广泛用于数学教育平台（如 Moodle 插件、GeoGebra 替代方案）和交互式数学文档系统。其架构设计经历了超过 15 年的工程打磨，对 Lv-00 的 Web GUI 几何面板设计有直接的借鉴价值。

JSXGraph 的核心架构包含四个紧密协作的子系统：（1）**Board-Element 两层模型**——将几何画布（Board）与几何元素（Element）严格分离，Board 管理坐标系、事件系统和元素注册表，Element 携带各自的几何定义和渲染逻辑；（2）**依赖图（Dependency Graph）**——每个 Element 在构造时注册其依赖的其他 Element，当父元素发生坐标更新时，依赖图自动触发受影响元素的重新计算；（3）**声明式几何构造 API**——通过 `board.create('point', [x, y])` 风格的统一构造接口，将几何对象创建与底层渲染解耦；（4）**渲染器抽象层**——通过 `JXG.AbstractRenderer` 接口抽象 SVG、Canvas 和 VML（已废弃）三种渲染后端，使同一套几何逻辑可在不同渲染环境中复用。

对 Lv-00 而言，JSXGraph 的借鉴价值不在于其数值计算的浮点引擎（JSXGraph 所有坐标均为 `number` 类型，依赖 IEEE 754 双精度浮点），而在于其**用户界面层的交互模型、声明式构造 DSL 和依赖图自动更新机制**。Lv-00 的核心优势在于**符号精确坐标**——几何点的坐标以符号代数形式存储（`Rational`/`AlgebraicNumber`），因此 Lv-00 可以在 Web GUI 层面借鉴 JSXGraph 的交互范式，但底层计算引擎使用完全不同的精确符号体系。这种"借鉴交互层、替换计算层"的策略使得两项目形成互补关系。

本文将从 Board-Element 架构映射、依赖图更新机制、声明式 API 转化和渲染器抽象四个方面，系统阐述 JSXGraph 的设计如何启发 Lv-00 的 Web GUI 几何面板实现，并重点讨论数值坐标与符号坐标的差异及互补策略。

---

## 2. Board-Element 两层模型 → Lv-00 Web GUI 几何面板

### 2.1 JSXGraph 的 Board-Element 架构

JSXGraph 的 Board-Element 模型是整个库的骨架。每一块画布（Board）是一个独立的几何构造空间，拥有独立的坐标系、视口变换和事件分发器。Element 则是 Board 的子对象，通过 `board.create()` 方法创建并自动注册到 Board 的元素列表中。

```
Board（画布容器）
├─ 坐标系（CoordSystem）
│  ├─ 原点位置（originX, originY）
│  ├─ 缩放因子（zoomX, zoomY）
│  └─ 坐标变换矩阵（从用户坐标到屏幕像素）
├─ 事件系统（EventEmitter）
│  ├─ 鼠标/触摸事件 → 拖拽/移动/悬停
│  ├─ 更新事件 → 重绘触发
│  └─ 自定义事件 → 扩展钩子
├─ 元素注册表（ElementRegistry）
│  ├─ Element[byId] 快速查找
│  ├─ Element[byName] 命名查找
│  └─ Element[byType] 按类型分组
└─ 渲染器（AbstractRenderer）
   ├─ SVG 渲染器
   ├─ Canvas 渲染器
   └─ （VML 渲染器 — 已废弃）
```

关键设计原则：
- **单一 Board 单一视口**：每个 `<div>` 容器可挂载恰好一个 Board，Board 之间完全独立
- **Element 生命周期**：创建 → 注册 → 更新（循环）→ 移除，每个阶段有明确的事件钩子
- **懒渲染**：Element 仅在必要时才调用渲染器的 `update()`，而不是每次坐标变化都重绘

### 2.2 Lv-00 Web GUI 组件映射

| JSXGraph 概念 | Lv-00 Web GUI 组件 | 说明 |
|:---|:---|:---|
| `JXG.Board` | `<GeometryPanel>` React 组件 | 几何画布容器，管理坐标系和视口 |
| `JXG.Point` | `<GeometryPoint>` 组件 + `PointElement` 数据模型 | 点元素，携带符号坐标 |
| `JXG.Line` | `<GeometryLine>` 组件 + `LineElement` 数据模型 | 线元素，由两点或多点定义 |
| `JXG.Circle` | `<GeometryCircle>` 组件 + `CircleElement` 数据模型 | 圆元素，由圆心和半径定义 |
| `JXG.Curve` / `JXG.Polygon` | `<GeometryPolygon>` / `<GeometryCurve>` 组件 | 多边形和曲线 |
| `board.create('point', [x, y])` | `panel.createElement('point', { coords })` | 声明式几何构造 |
| `board.removeObject(el)` | `panel.removeElement(id)` | 元素移除 |
| `board.on('update', handler)` | `constraintGraph.subscribe(id, handler)` | 约束更新订阅 |
| `board.renderer` | `GeometryRenderer` 接口（抽象） | 渲染器抽象 |
| `board.suspendUpdate()` / `board.unsuspendUpdate()` | `panel.batchUpdate(() => {...})` | 批量更新控制 |
| `board.getCoordsTopLeftCorner()` | `panel.viewport.getBounds()` | 视口边界查询 |
| `JXG.Coords` | `SymbolicCoords` 类 | 坐标表示（JSXGraph 数值 vs Lv-00 符号） |

### 2.3 Lv-00 GeometryPanel 组件的 React 实现

```tsx
/**
 * @file GeometryPanel.tsx
 * @brief Lv-00 Web GUI 几何面板 —— 借鉴 JSXGraph Board-Element 两层模型
 *
 * 与 JSXGraph 的关键差异：
 *  - 坐标系统：JSXGraph 使用 number (IEEE 754)，Lv-00 使用 SymbolicCoords
 *  - 渲染精度：JSXGraph 取决于浮点精度，Lv-00 通过符号计算保证精确性
 *  - 更新策略：JSXGraph 立即求值，Lv-00 延迟求值 + constraint_graph 驱动的惰性更新
 */

import React, { useRef, useEffect, useState, useCallback } from 'react';
import { ConstraintGraph } from '../core/constraint_graph';
import { SymbolicCoords, SymbolicNumber } from '../core/symbolic';

// ─── 坐标变换（用户坐标 ↔ 屏幕像素）──────────────────────────────
interface ViewportTransform {
    /** 用户坐标原点在屏幕上的像素位置 */
    originPx: { x: number; y: number };
    /** 缩放因子：1 用户单位 = scale 像素 */
    scale: number;

    /** 将用户坐标转换为屏幕像素坐标 */
    userToScreen(coord: SymbolicCoords): { x: number; y: number };

    /** 将屏幕像素坐标转换为用户坐标（近似） */
    screenToUser(px: number, py: number): SymbolicCoords;
}

// ─── 元素类型枚举（对应 JSXGraph 的元素类型）───────────────────────
type ElementKind =
    | 'point'
    | 'line'
    | 'line_segment'
    | 'circle'
    | 'polygon'
    | 'angle'
    | 'text'
    | 'intersection';

// ─── GeometryElement 基类（对应 JSXGraph 的 JXG.GeometryElement）───
interface GeometryElementData {
    id: string;
    kind: ElementKind;
    /** 元素名称（用户可见的标签） */
    label: string;
    /** 依赖的其他元素 ID 列表（对应 JSXGraph 的 dependency graph） */
    dependsOn: string[];
    /** 该元素是否隐藏（辅助构造/Ghost 元素） */
    hidden: boolean;
    /** 附加的约束 ID 列表（对应 TypeRegion.constraint_ids） */
    constraintIds: number[];
    /** 元素当前的符号坐标（精确） */
    coords: SymbolicCoords | null;
    /** 元素当前在屏幕上的像素位置（缓存，避免重复变换） */
    screenCoords: { x: number; y: number } | null;
}

// ─── GeometryPanel 组件（对应 JSXGraph 的 JXG.Board）──────────────
interface GeometryPanelProps {
    /** 画布宽度（像素） */
    width: number;
    /** 画布高度（像素） */
    height: number;
    /** 外部约束图（由 Lv-00 核心引擎提供） */
    constraintGraph: ConstraintGraph;
    /** 初始元素列表 */
    initialElements?: GeometryElementData[];
    /** 元素创建/更新/删除的回调 */
    onElementCreated?: (el: GeometryElementData) => void;
    onElementUpdated?: (el: GeometryElementData) => void;
    onElementRemoved?: (id: string) => void;
}

const GeometryPanel: React.FC<GeometryPanelProps> = ({
    width,
    height,
    constraintGraph,
    initialElements = [],
    onElementCreated,
    onElementUpdated,
    onElementRemoved,
}) => {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const svgRef = useRef<SVGSVGElement>(null);

    // ── 元素注册表（对应 JSXGraph 的 ElementRegistry）─────────────
    const [elements, setElements] = useState<Map<string, GeometryElementData>>(
        () => new Map(initialElements.map(el => [el.id, el]))
    );

    // ── 视口变换 ─────────────────────────────────────────────────
    const [viewport, setViewport] = useState<ViewportTransform>({
        originPx: { x: width / 2, y: height / 2 },
        scale: 50, // 1 用户单位 = 50 像素
        userToScreen(coord: SymbolicCoords) {
            // 符号坐标 → 浮点近似（仅用于屏幕渲染）
            const fx = coord.x.approximate();
            const fy = coord.y.approximate();
            return {
                x: this.originPx.x + fx * this.scale,
                y: this.originPx.y - fy * this.scale, // Y 轴翻转
            };
        },
        screenToUser(px: number, py: number): SymbolicCoords {
            const ux = (px - this.originPx.x) / this.scale;
            const uy = (this.originPx.y - py) / this.scale;
            return new SymbolicCoords(
                SymbolicNumber.fromFloat(ux),
                SymbolicNumber.fromFloat(uy)
            );
        },
    });

    // ── 声明式几何构造 API（对应 JSXGraph 的 board.create()）──────
    const createElement = useCallback(
        (
            kind: ElementKind,
            params: Record<string, unknown>
        ): string | null => {
            // 生成唯一 ID
            const id = `el_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`;

            // 从参数中提取依赖关系
            const dependsOn: string[] = [];
            if (params.parents && Array.isArray(params.parents)) {
                dependsOn.push(...(params.parents as string[]));
            }

            const el: GeometryElementData = {
                id,
                kind,
                label: (params.label as string) || `${kind}_${id.slice(-4)}`,
                dependsOn,
                hidden: (params.hidden as boolean) || false,
                constraintIds: (params.constraintIds as number[]) || [],
                coords: null,
                screenCoords: null,
            };

            setElements(prev => {
                const next = new Map(prev);
                next.set(id, el);
                return next;
            });

            // 向 constraint_graph 注册该元素及其依赖
            constraintGraph.registerNode(id, kind, dependsOn);

            onElementCreated?.(el);
            return id;
        },
        [constraintGraph, onElementCreated]
    );

    // ── 批量更新控制（对应 JSXGraph 的 suspendUpdate/unsuspendUpdate）─
    const [batchPending, setBatchPending] = useState(false);

    const batchUpdate = useCallback((fn: () => void) => {
        setBatchPending(true);
        fn();
        setBatchPending(false);
        // 批量更新完成后，一次性触发 constraint_graph 的重算
        if (!batchPending) {
            constraintGraph.triggerUpdate();
        }
    }, [constraintGraph, batchPending]);

    // ── 约束图更新订阅（对应 JSXGraph 的 board.on('update')）─────
    useEffect(() => {
        const unsubscribe = constraintGraph.subscribe('update', (changedIds: string[]) => {
            setElements(prev => {
                const next = new Map(prev);
                for (const id of changedIds) {
                    const el = next.get(id);
                    if (el) {
                        // 从 constraint_graph 获取最新的符号坐标
                        const newCoords = constraintGraph.getCoords(id);
                        if (newCoords) {
                            const screen = viewport.userToScreen(newCoords);
                            next.set(id, { ...el, coords: newCoords, screenCoords: screen });
                        }
                    }
                }
                return next;
            });
        });

        return unsubscribe;
    }, [constraintGraph, viewport]);

    // ── 渲染循环（Canvas 或 SVG）─────────────────────────────────
    useEffect(() => {
        // SVG 渲染模式（对应 JSXGraph 的 SVG 渲染器）
        const svg = svgRef.current;
        if (!svg) return;

        // 清空上一次的渲染（简单策略：重新生成全部 SVG 子元素）
        while (svg.firstChild) {
            svg.removeChild(svg.firstChild);
        }

        elements.forEach(el => {
            if (el.hidden || !el.screenCoords) return;

            const { x, y } = el.screenCoords;

            switch (el.kind) {
                case 'point': {
                    const circle = document.createElementNS(
                        'http://www.w3.org/2000/svg', 'circle'
                    );
                    circle.setAttribute('cx', String(x));
                    circle.setAttribute('cy', String(y));
                    circle.setAttribute('r', '4');
                    circle.setAttribute('fill', '#2563eb');
                    circle.setAttribute('stroke', '#1e40af');
                    circle.dataset.elementId = el.id;
                    svg.appendChild(circle);
                    break;
                }
                case 'line': {
                    // 线需要两个端点，此处仅示意单元素渲染
                    break;
                }
                // ... 其他元素类型的 SVG 渲染
            }
        });
    }, [elements]);

    return (
        <div className="geometry-panel" style={{ position: 'relative', width, height }}>
            {/* SVG 层：矢量渲染（对应 JSXGraph 的 SVG 渲染器） */}
            <svg
                ref={svgRef}
                width={width}
                height={height}
                style={{ position: 'absolute', top: 0, left: 0 }}
            />
            {/* Canvas 层：备选渲染后端（对应 JSXGraph 的 Canvas 渲染器） */}
            <canvas
                ref={canvasRef}
                width={width}
                height={height}
                style={{ display: 'none' }} // 默认使用 SVG，Canvas 作为备选
            />
        </div>
    );
};

export { GeometryPanel };
export type { GeometryElementData, ViewportTransform, ElementKind };
```

### 2.4 数值坐标 vs 符号坐标：差异与互补

这是 Lv-00 与 JSXGraph 最根本的哲学差异：

| 方面 | JSXGraph（数值坐标） | Lv-00（符号坐标） |
|:---|:---|:---|
| 坐标类型 | `number` (IEEE 754 double) | `Rational` / `AlgebraicNumber` / `SymbolicExpression` |
| 精度模型 | 浮点误差累积（~1e-15 级别） | 精确代数运算（0 误差） |
| 几何判定 | 容差判定（`Math.abs(a - b) < eps`） | 严格等式判定（Groebner 基/面积法） |
| 构造范式 | "拖拽→数值调整→重绘" | "声明约束→符号求解→精确定位" |
| 共线性判定 | 数值面积 < epsilon → "近似共线" | 行列式 = 0 → 精确共线 |
| 交点计算 | 浮点 Newton 迭代 | 代数消元（结式法） |
| 优势 | 实时交互、平滑动画、低延迟 | 几何正确性保证、无退化情况 |

**互补策略**：Lv-00 的 Web GUI 在渲染层使用浮点近似（将 `SymbolicCoords` 转为 `(x_px, y_px)` 像素坐标），这是唯一需要降级为浮点的环节。所有几何判定、约束求解和证明验证均在符号层完成。这等价于 JSXGraph 的渲染器抽象——将自己的 `JXG.Coords`（数值）转为屏幕坐标——只是 Lv-00 在更上游就保持了符号精度。

---

## 3. 依赖图驱动的自动约束更新

### 3.1 JSXGraph 的依赖图机制

JSXGraph 中每个 Element 在创建时声明其"父元素"（parents），这些 parents 构成一个隐式依赖图。当某个 Element 的坐标发生变化（例如被用户拖拽），JSXGraph 沿依赖图的拓扑顺序重新计算所有受影响的子孙 Element：

```
用户拖拽点 A
  → A.coords 更新
  → 遍历 A 的孩子 B（B.parents 包含 A）
  → B.coords 重新计算（例如：B = midpoint(A, C)）
  → 遍历 B 的孩子（继续传播）
  → 直到无更多后代
  → 触发重绘
```

依赖图的拓扑排序在每次 Element 创建时增量维护，确保更新传播不会出现"先更新子元素再更新父元素"的逆序问题。

### 3.2 映射到 Lv-00 constraint_graph.h

Lv-00 已有的 `constraint_graph.h` 天然提供了比 JSXGraph 更强大的依赖图能力：

| JSXGraph 依赖图能力 | Lv-00 constraint_graph.h 能力 | 增强 |
|:---|:---|:---|
| 祖先→后代单向传播 | 前向+后向约束传播（双向） | Lv-00 支持反向约束（目标→源） |
| 仅依靠拓扑排序 | 拓扑排序 + 约束环检测 | 检测循环约束并报告 |
| 无约束类型区分 | 等式约束 + 不等式约束 + 几何谓词 | 更细粒度的约束分类 |
| 数值重算 | 符号重算 + 方程求解 | 处理未定方程组 |
| 无外部求解器集成 | constraint_graph → solver.h 桥接 | 复杂约束可委托求解器 |
| 无证明信息 | 每个约束边携带证明步骤引用 | 约束图即是证明图的骨架 |

### 3.3 Lv-00 的惰性符号更新策略

JSXGraph 的更新策略是"急切的"（一旦父元素变化立即触发子元素重算），而 Lv-00 因其符号引擎的计算成本较高，采用**惰性符号更新**：

```
用户交互（例如拖拽点 A）
  → 产生新的符号约束（A 的位置约束）
  → 推入 constraint_graph 的脏队列
  → 不立即求解  ← 关键差异
  → 等待 batchUpdate 批次结束或 render 被调用
  → 对脏子图进行符号求解：
      ├─ 检查是否有充分约束（唯一解）
      ├─ 欠约束 → 保留符号参数（自由变量）
      ├─ 过约束 → 检测冲突并报告
      └─ 生成 nogood（借鉴 Chuffed LCG）
  → 将符号解浮点化（仅渲染需要）
  → 触发 UI 重绘
```

```c
/**
 * @brief 惰性符号更新 —— Lv-00 constraint_graph 的更新策略
 *
 * 与 JSXGraph 的急切更新不同，Lv-00 积累一批交互后统一求解。
 * 这样可以利用整个脏子图的全局信息进行优化。
 */
typedef enum {
    UPDATE_STRATEGY_EAGER,   /**< 急切更新（类似 JSXGraph） */
    UPDATE_STRATEGY_LAZY,    /**< 惰性更新（Lv-00 默认） */
    UPDATE_STRATEGY_BATCHED  /**< 批量更新（事务包装） */
} UpdateStrategy;

/**
 * @brief 标记节点为脏并加入更新队列
 *
 * 注意：此函数不会立即触发求解。求解发生在 constraint_graph_solve()
 * 被调用时（通常在一次交互批次的末尾）。
 */
void constraint_graph_mark_dirty(
    ConstraintGraph *cg,
    int node_id,
    const char *reason  /**< 脏标记原因（用于调试和可解释性） */
);

/**
 * @brief 对脏子图执行符号求解
 *
 * 遍历脏节点的拓扑闭包，对每个受影响的子图：
 *  1. 收集约束方程
 *  2. 判断约束充分性（确定/欠约束/过约束）
 *  3. 调用 solver.h 求解
 *  4. 回写求解结果到节点符号坐标
 *  5. 生成 nogood（如果过约束）
 */
ConstraintSolveResult constraint_graph_solve(
    ConstraintGraph *cg,
    ConstraintSolver *solver,
    UpdateStrategy strategy
);
```

---

## 4. 声明式几何构造 DSL → Lv-00 前端几何构造语言

### 4.1 JSXGraph 的声明式 API 风格

```javascript
// JSXGraph 风格：声明式构造
const board = JXG.JSXGraph.initBoard('jxgbox', {
    boundingbox: [-5, 5, 5, -5],
    axis: true
});

// 每个 board.create() 都是一条声明式语句
const A = board.create('point', [1, 2], { name: 'A', color: 'blue' });
const B = board.create('point', [3, 4], { name: 'B', color: 'blue' });
const AB = board.create('segment', [A, B], { name: 'AB' });

// 由已有元素派生的构造（声明式依赖）
const M = board.create('midpoint', [A, B], { name: 'M' });
const circle = board.create('circle', [M, A], { name: 'circle_MA' });

// 交点构造
const C = board.create('point', [0, 1], { name: 'C' });
const BC = board.create('segment', [B, C]);
const I = board.create('intersection', [AB, BC, 0], { name: 'I' }); // 第 0 个交点
```

关键特征：
- 每个构造调用都明确声明父元素（通过数组参数传递引用）
- 构造类型在第一个字符串参数中声明
- 属性和样式在第三个对象参数中声明
- 构造的求值顺序由父元素的依赖关系自动决定，无需用户手动排序

### 4.2 Lv-00 的声明式几何构造 DSL

Lv-00 的前端几何构造 DSL 在 JSXGraph 风格基础上增加了**类型安全**和**约束声明**：

```typescript
/**
 * @file geometry_dsl.ts
 * @brief Lv-00 声明式几何构造 DSL —— 借鉴 JSXGraph board.create() 风格
 *
 * 与 JSXGraph 的关键扩展：
 *  - 每个构造可附带显式约束（如 "collinear", "perpendicular"）
 *  - 坐标可以是符号表达式（如 'sqrt(2)', 'a/2'）
 *  - 支持命名/匿名两种构造模式
 */

import { SymbolicNumber } from '../core/symbolic';

// ─── 几何构造类型（比 JSXGraph 更丰富的类型系统）─────────────────
type GeoConstructor =
    // ── 零维 ──
    | { kind: 'point'; coords: [SymbolicNumber, SymbolicNumber] }
    | { kind: 'free_point'; coords: [SymbolicNumber, SymbolicNumber] }

    // ── 一维 ──
    | { kind: 'line'; through: [string, string] }                       // 过两点的直线
    | { kind: 'segment'; endpoints: [string, string] }                  // 线段
    | { kind: 'ray'; origin: string; through: string }                  // 射线

    // ── 由约束定义的构造（JSXGraph 不直接支持）─────────────────
    | { kind: 'point_on_line'; line: string; constraints: string[] }   // 线上某点
    | { kind: 'point_on_circle'; circle: string; constraints: string[] } // 圆上某点
    | { kind: 'intersection'; of: [string, string]; index: number }    // 交点

    // ── 二维 ──
    | { kind: 'circle'; center: string; radius: SymbolicNumber }
    | { kind: 'circle_through'; center: string; point_on: string }
    | { kind: 'polygon'; vertices: string[] }

    // ── 高级构造（超越 JSXGraph）─────────────────────────────
    | { kind: 'angle_bisector'; vertex: string; arms: [string, string] }
    | { kind: 'perpendicular_bisector'; segment: string }
    | { kind: 'parallel'; to: string; through: string }
    | { kind: 'perpendicular'; to: string; through: string };

// ─── 构造 DSL 的主入口（对应 JSXGraph 的 board.create()）───────────
interface GeometryPanelDSL {
    /**
     * 声明式几何构造
     *
     * 用法示例（与 JSXGraph 风格类似但增强类型安全）：
     *
     *   const A = panel.create({ kind: 'point', coords: [0, 0] });
     *   const B = panel.create({ kind: 'point', coords: [1, 0] });
     *   const AB = panel.create({ kind: 'segment', endpoints: [A, B] });
     *   const C = panel.create({ kind: 'point_on_circle', circle: circle_AB,
     *                             constraints: ['not_collinear_with', A, B] });
     */
    create(ctor: GeoConstructor, options?: {
        label?: string;
        hidden?: boolean;
        constraints?: string[];  // Lv-00 特有：附加几何约束
        style?: Record<string, string>;
    }): string;  // 返回元素 ID

    /** 移除元素 */
    remove(id: string): void;

    /** 批量构造（事务包装，对应 JSXGraph 的批量更新） */
    batch(fn: () => void): void;

    /** 获取元素的符号坐标 */
    getCoords(id: string): SymbolicCoords | null;

    /** 监听约束更新（对应 JSXGraph 的 board.on('update')） */
    onUpdate(handler: (changedIds: string[]) => void): () => void;
}
```

### 4.3 DSL 使用示例：Lv-00 前端构造欧几里得几何

```typescript
// 构造一个欧几里得几何命题的交互式图形
const board = createGeometryPanel({
    width: 800,
    height: 600,
    constraintGraph: cg,
});

// 使用声明式 DSL 构造图形（风格类似 JSXGraph，但坐标是符号的）
board.batch(() => {
    // 基础点
    const A = board.create({ kind: 'point', coords: [0, 0] }, { label: 'A' });
    const B = board.create({ kind: 'point', coords: [1, 0] }, { label: 'B' });
    const C = board.create({ kind: 'point', coords: [0.5, 1] }, { label: 'C' });

    // 三角形边
    const AB = board.create({ kind: 'segment', endpoints: [A, B] });
    const BC = board.create({ kind: 'segment', endpoints: [B, C] });
    const CA = board.create({ kind: 'segment', endpoints: [C, A] });

    // 高级构造：中垂线 → 外心
    const perp_AB = board.create({ kind: 'perpendicular_bisector', segment: AB });
    const perp_BC = board.create({ kind: 'perpendicular_bisector', segment: BC });
    const circumcenter = board.create({
        kind: 'intersection',
        of: [perp_AB, perp_BC],
        index: 0
    }, { label: 'O' });

    // 附加约束（JSXGraph 不支持的原生功能）
    // 声明 C 必须在以 O 为圆心的外接圆上
    const circumcircle = board.create({
        kind: 'circle_through',
        center: circumcenter,
        point_on: A
    });

    // Lv-00 特有：将约束声明为明确的几何命题
    // "C 在三角形 ABC 的外接圆上"——这是一个需要验证的命题
    board.createConstraint(circumcircle, C, 'point_on_circle');
});
```

---

## 5. 渲染器抽象（SVG/Canvas）及 Lv-00 适配

### 5.1 JSXGraph 的渲染器抽象

JSXGraph 通过 `JXG.AbstractRenderer` 接口抽象渲染后端：

```javascript
// JSXGraph 的渲染器抽象（简化表示）
class AbstractRenderer {
    drawPoint(x, y, options) { /* 子类实现 */ }
    drawLine(p1, p2, options) { /* 子类实现 */ }
    drawCircle(center, radius, options) { /* 子类实现 */ }
    drawPolygon(points, options) { /* 子类实现 */ }
    drawText(text, position, options) { /* 子类实现 */ }
    // ... 更多绘制原语
}

class SVGRenderer extends AbstractRenderer { /* SVG 实现 */ }
class CanvasRenderer extends AbstractRenderer { /* Canvas 实现 */ }
```

这种抽象使得几何逻辑完全独立于渲染技术。对 Lv-00 而言，渲染器抽象不仅需要支持 SVG/Canvas 的切换，还需要处理**符号坐标→像素坐标的降级渲染**，以及**辅助构造（Ghost 元素）的视觉差异化**。

### 5.2 Lv-00 的渲染器抽象设计

```typescript
/**
 * @brief Lv-00 的几何渲染器抽象 —— 借鉴 JSXGraph AbstractRenderer
 *
 * 与 JSXGraph 的关键差异：
 *  - 接收的是 SymbolicCoords（符号坐标）而非 number
 *  - 每种样式有三个变体：normal / ghost（半透明虚线） / selected（高亮）
 *  - 支持分层渲染（元素层、辅助线层、标注层、交互层）
 */
interface GeometryRenderer {
    /** 渲染类型标识 */
    readonly kind: 'svg' | 'canvas' | 'webgl';

    /** 清除整个画布 */
    clear(): void;

    // ── 绘制原语（接收符号坐标，内部转为像素坐标）─────────────────

    /** 绘制点（三种样式变体） */
    drawPoint(
        coords: SymbolicCoords,
        style: 'normal' | 'ghost' | 'selected',
        options?: PointOptions
    ): void;

    /** 绘制线段 */
    drawSegment(
        from: SymbolicCoords,
        to: SymbolicCoords,
        style: 'normal' | 'ghost' | 'selected',
        options?: LineOptions
    ): void;

    /** 绘制直线（无限延伸的视觉表示） */
    drawLine(
        p1: SymbolicCoords,
        p2: SymbolicCoords,
        style: 'normal' | 'ghost' | 'selected',
        options?: LineOptions
    ): void;

    /** 绘制圆 */
    drawCircle(
        center: SymbolicCoords,
        radius: SymbolicNumber,  // 符号半径
        style: 'normal' | 'ghost' | 'selected',
        options?: CircleOptions
    ): void;

    /** 绘制多边形 */
    drawPolygon(
        vertices: SymbolicCoords[],
        style: 'normal' | 'ghost' | 'selected',
        options?: PolygonOptions
    ): void;

    /** 绘制文本/标签 */
    drawLabel(
        text: string,
        position: SymbolicCoords,
        options?: TextOptions
    ): void;

    // ── 分层控制 ──────────────────────────────────────────────

    /** 开始渲染指定层 */
    beginLayer(layer: 'element' | 'auxiliary' | 'annotation' | 'interaction'): void;

    /** 结束当前层 */
    endLayer(): void;

    // ── 视口 ──────────────────────────────────────────────────

    /** 设置视口变换 */
    setViewport(transform: ViewportTransform): void;

    /** 获取当前视口 */
    getViewport(): ViewportTransform;
}
```

---

## 6. 实现路线图

### 6.1 第一阶段：Web GUI 基础框架（P3-1）

- [ ] 搭建 `GeometryPanel` React 组件骨架
  - 集成 SVG 渲染器（`SVGRenderer` 实现）
  - 实现符号坐标 → 屏幕像素坐标的视口变换
  - 实现基本的平移/缩放交互（鼠标拖拽/滚轮）
- [ ] 实现 `GeometryElementData` 数据模型
  - 元素 ID 生成、注册表、生命周期管理
  - 元素类型枚举（point, line, circle, polygon 等）
- [ ] 实现声明式构造 DSL 的最小可用版本
  - `create()` 方法支持 point/segment/circle 三种构造
  - `remove()` 方法支持元素删除
- [ ] 编写基础组件的单元测试和交互测试

### 6.2 第二阶段：约束图集成（P3-2）

- [ ] 将 `GeometryPanel` 与 `constraint_graph.h` 桥接
  - 元素创建时向 constraint_graph 注册节点
  - 元素参数变化时标记对应节点为脏
- [ ] 实现惰性符号更新策略
  - 脏队列管理
  - 批量更新控制（`batchUpdate` API）
  - 约束图更新完成后触发 UI 重绘
- [ ] 实现依赖可视化调试模式
  - 高亮显示所有脏路径
  - 显示每个节点的依赖链
- [ ] 实现约束冲突的可视化反馈
  - 过约束节点用红色标记
  - 显示冲突约束的 "nogood" 信息

### 6.3 第三阶段：高级交互功能（P3-3）

- [ ] 实现元素拖拽
  - 拖拽自由点：产生新的位置约束
  - 拖拽约束点：沿约束轨迹移动（如果欠约束）
  - 拖拽固定点：拒绝移动并给出反馈
- [ ] 实现 Canvas 渲染器作为备选后端
  - 实现 `CanvasRenderer` 类
  - 性能对比测试（SVG vs Canvas 在大规模构造场景下）
- [ ] 实现辅助构造（Ghost 元素）的视觉差异化
  - Ghost 元素使用半透明虚线样式
  - Ghost 切换：一键显示/隐藏所有辅助构造
- [ ] 实现选择/高亮系统
  - 单选和多选元素
  - 选中元素的高亮渲染
  - 选中后显示属性面板（坐标、约束列表）

### 6.4 第四阶段：声明式 DSL 完善（P3-4）

- [ ] 扩展 `GeoConstructor` 联合类型
  - 支持所有几何构造类型（角平分线、中垂线、平行线、垂线等）
  - 支持约束定义构造（线上点、圆上点、交点等）
- [ ] 实现 DSL 的类型安全校验
  - 编译时检查构造参数的类型是否匹配
  - 例如：`circle` 的 `center` 必须是 point 类型
- [ ] 实现 DSL 的序列化/反序列化
  - 将构造序列保存为 JSON（可分享、可版本控制）
  - 回放构造序列以重建图形
- [ ] 编写 DSL 使用文档和交互式示例

### 6.5 第五阶段：符号-数值混合渲染优化（P3-5）

- [ ] 实现符号坐标的浮点缓存层
  - `SymbolicCoords` → `Float64Pair` 的惰性求值缓存
  - 仅在符号值变化时重新计算浮点近似
- [ ] 实现分层渲染的性能优化
  - 静态层（不变化的元素）预渲染到离屏 Canvas
  - 动态层（被拖拽/更新的元素）每帧重新渲染
- [ ] 实现 GPU 加速渲染（WebGL 后端）
  - 大量点/线/圆的批量 GPU 渲染
  - 视口变换矩阵在 GPU 上执行

---

## 7. 设计决策与权衡

### 7.1 为什么使用 JSXGraph 作为 Web GUI 借鉴对象而非 GeoGebra

| 方面 | JSXGraph | GeoGebra |
|:---|:---|:---|
| 架构模式 | 轻量库，Board-Element 简单模型 | 单体应用，复杂的 MVC 架构 |
| 代码行数 | ~30k 行 JavaScript | ~500k 行 Java/JavaScript 混合 |
| API 风格 | 声明式 `board.create()` | 命令式 `GeoGebraApp.evalCommand()` |
| 渲染后端 | 显式抽象（SVG/Canvas） | 内部耦合 |
| 许可 | MIT/LGPL | GPL（限制商业使用） |
| 对 Lv-00 迁移 | 直接映射到 React 组件 | 架构差异过大 |

JSXGraph 的轻量设计和显式架构抽象更适合作为 Lv-00 Web GUI 的参考模型。

### 7.2 符号坐标的渲染代价

将 `SymbolicCoords` 转为屏幕像素坐标需要：
1. 符号求值（可能涉及代数表达式化简）
2. 浮点近似（精确值 → `f64`）
3. 坐标变换（用户坐标 → 屏幕像素）

步骤 1 和 2 的代价在交互式拖拽场景下可能成为瓶颈。缓解策略：
- 对"自由点"（无符号依赖）直接缓存其浮点坐标
- 对"约束点"使用增量求值（仅当符号值变化时重新近似）
- 使用 Web Worker 将符号求值移到后台线程，避免阻塞 UI

### 7.3 JSXGraph 不提供的 Lv-00 独有功能

以下功能是 Lv-00 基于符号引擎的独有优势，JSXGraph 无法实现：
- **精确共线性/共圆性判定**：无浮点容差误差
- **证明模式**：用户可以声明几何命题并让 Lv-00 自动验证
- **符号坐标的代数约简**：`sqrt(2)*sqrt(2)` 自动约简为 `2`
- **约束冲突的精确诊断**：明确指出哪组约束互斥
- **构造步骤的 Ghost 消除**：编译期分离核心构造与辅助构造

---

## 8. 参考资源

- **JSXGraph 官方网站**：https://jsxgraph.org
- **JSXGraph GitHub 仓库**：https://github.com/jsxgraph/jsxgraph
- **JSXGraph API 参考文档**：https://jsxgraph.org/docs/index.html
- **JSXGraph Wiki（含教程和示例）**：https://jsxgraph.org/wiki/index.php/Main_Page
- **Board 类源码**：https://github.com/jsxgraph/jsxgraph/blob/master/src/base/board.js
- **GeometryElement 基类源码**：https://github.com/jsxgraph/jsxgraph/blob/master/src/base/element.js
- **依赖图与更新循环文档**：https://jsxgraph.org/docs/symbols/JXG.Board.html#update
- **渲染器抽象相关**：
  - `JXG.SVGRenderer`：https://github.com/jsxgraph/jsxgraph/blob/master/src/renderer/svg.js
  - `JXG.CanvasRenderer`：https://github.com/jsxgraph/jsxgraph/blob/master/src/renderer/canvas.js
