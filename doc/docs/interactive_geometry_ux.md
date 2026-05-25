# Cinderella/Dr.Geo 交互 UX 设计记录

> **借鉴项目**：Cinderella（cinderella.de）和 Dr.Geo（drgeo.eu）
> **核心借鉴点**：随机化定理验证、几何构造=代码生成的 UI 设计、连续运动下的构造一致性
> **分类**：P4 低优先级 / 交互 UX 增强
> **日期**：2026-05-24

---

## 1. 概述

Cinderella 和 Dr.Geo 是两个开创性的交互式几何系统。Cinderella 的随机化定理验证改变了几何教学中"眼见为实"的验证方式，而 Dr.Geo 的"几何构造=代码生成"理念启发了程序化几何思考。本文档探讨如何将这些交互范式引入 Lv-00 的 Web GUI（`web-gui/`）和传统 Web 前端（`web/`）中。

---

## 2. Cinderella 随机化定理验证的实现方案

### 2.1 Cinderella 的核心机制

Cinderella 的"定理检查器"并非形式化证明工具，而是一种概率验证机制：

1. 用户构造一个几何图形并提出一个猜想（如"三条中线总交于一点"）
2. Cinderella 随机移动构造图的自由点
3. 在每个随机位置上，检查猜想是否成立
4. 如果连续 N 次（默认 100 次）测试均通过，则报告"该猜想很可能成立"
5. 如果某次测试失败，则立即报告反例

### 2.2 Lv-00 中的实现方案

在 Lv-00 中，随机化验证被定位为**形式化证明的前置探索工具**——它不能替代证明，但可以在以下场景中发挥关键作用：

#### 2.2.1 架构位置

随机化验证作为 `modules/proof.js` 的一个子模块实现，与现有的 Coq 导出和矛盾证明功能并列：

```javascript
// 在 ProofPanel 中新增的随机化功能
const RANDOMIZATION_DEFAULTS = {
    TRIALS: 100,              // 默认试验次数
    PERTURBATION_RANGE: 1.0,  // 自由点随机扰动范围
    CONFIDENCE_THRESHOLD: 1.0 // 置信度阈值（1.0 = 必须全通过）
};
```

#### 2.2.2 实现流程

```
1. 识别自由点（约束图中自由度 > 0 的 POINT 节点）
2. for (trial = 0; trial < N; trial++):
   a. 对每个自由点随机赋予新坐标（在原坐标附近的高斯扰动）
   b. 运行增量求解器（solver），计算约束点的新位置
   c. 检查猜想（目标约束）是否在所有求解结果中成立
   d. 如果失败 → 立即报告反例并终止
3. 全部通过 → 报告"该猜想在 N 次随机测试中均成立"
```

#### 2.2.3 与增量求解器的集成

Lv-00 的增量求解器（`solver.h` 第 209 行，"脏变量"标记机制）使得随机测试非常高效。每次随机扰动只需：

1. 修改自由点的符号坐标（标记为"脏"）
2. 调用增量求解，仅重新计算受影响的下游变量
3. 验证目标约束

```javascript
/**
 * @brief 随机化定理验证（借鉴 Cinderella 的 RandomCheck）
 *
 * @param {Object} graph - 当前约束图
 * @param {Array<number>} freePointIds - 自由点 ID 列表
 * @param {Object} conjecture - 猜想描述 {type, participants, expected}
 * @param {number} trials - 试验次数
 * @returns {Object} {passed: boolean, counterexample: Object|null, stats: Object}
 */
function randomizationCheck(graph, freePointIds, conjecture, trials = 100) {
    // 实现细节见 proofs.js 中的 randomizationCheck 函数
}
```

---

## 3. Dr.Geo "几何构造=代码生成"的 UI 设计

### 3.1 Dr.Geo 的核心理念

Dr.Geo 的独特之处在于：
- 用户在画布上绘制的每一步几何构造都**同时生成对应的 Smalltalk 代码**
- 代码面板和几何画布**实时双向同步**
- 可以直接在代码中修改构造参数，画布即时更新
- 这使用户从"图形操作"自然过渡到"程序化几何思维"

### 3.2 Lv-00 Web GUI 中的 UI 设计

#### 3.2.1 双视图布局

借鉴 Dr.Geo 的双面板设计，在 Lv-00 的 Web GUI 中增加可切换的"代码同步视图"：

```
┌──────────────────────────────────────────────────────┐
│  工具栏 / Toolbar                                      │
├──────────────────────┬───────────────────────────────┤
│                      │  DSL 代码面板                   │
│   几何画布            │  ┌───────────────────────────┐│
│   (Canvas)           │  │ axiom_package euclidean   ││
│                      │  │                          ││
│   · A (自由点)        │  │ let A = point(0, 0)     ││
│   · B (自由点)        │  │ let B = point(3, 0)     ││
│   · AB (线段)         │  │ let AB = segment(A, B)  ││
│   · C (圆与线交点)     │  │ let circle1 =           ││
│                      │  │   circle(A, B)           ││
│                      │  │ let C = intersect(       ││
│                      │  │   circle1, AB)           ││
│                      │  └───────────────────────────┘│
│                      │  ◉ 实时同步  ◉ 双向编辑         │
├──────────────────────┴───────────────────────────────┤
│  状态栏 / StatusBar                                    │
└──────────────────────────────────────────────────────┘
```

#### 3.2.2 DSL 代码生成规则

Lv-00 的 DSL（借鉴 GCLC 的设计，已在 `dsl_design_gclc_reference.md` 中定义）与几何画布操作的映射：

| 画布操作 | DSL 代码生成 | 说明 |
|:---|:---|:---|
| 点击空白处创建自由点 | `let P = point(x, y)` | 坐标自动从点击位置确定 |
| 依次点击两点创建线段 | `let s = segment(P, Q)` | |
| 点击圆心+一点创建圆 | `let c = circle(center, on)` | |
| 线段与圆的交点 | `let X = intersect(line, circle)` | 多解时弹出选择器 |
| 施加约束（右键菜单） | `constrain(incidence, P, L)` | |
| 打包为函数块 | `let f = pack([internal], [in], [out])` | |

#### 3.2.3 代码→画布的反向同步

借鉴 Dr.Geo 的双向编辑能力：
- 用户在代码面板中修改构造参数 → 画布实时更新（通过 `setPoints` / `setSegments` 等 Store 操作）
- 用户在代码面板中添加新的 `let` 语句 → 画布上出现新构造
- 如果代码修改产生冲突，画布上以红色高亮冲突元素

---

## 4. 连续运动下的构造一致性保持策略

### 4.1 问题定义

在交互式几何中，用户拖拽一个自由点时，所有依赖该点的约束构造需要实时重算。核心挑战是：

- **分支切换**：圆与线段相交可能有 2 个交点。自由点移动过程中，交点可能从一个分支"跳"到另一个
- **奇点处理**：当两个交点合并为一个（线段恰好与圆相切）时，构造的拓扑类型发生变化
- **性能**：拖拽过程中需要每秒 30-60 次求解

### 4.2 Cinderella 的一致性策略

Cinderella 使用复数算术（而非实数）来保证连续性：
- 所有几何对象定义在复数域上
- 代数方程的解不需要在实数范围内成立
- 连续运动时，复数解连续演化，分支不跳变
- 仅当用户需要"可见"的几何对象时，才取实数部分

### 4.3 Lv-00 适配方案

Lv-00 的符号坐标系统（`symbolic_coord.h`）目前工作在实数域上。为保证连续运动下的构造一致性，采用以下分层策略：

#### 4.3.1 连续追踪模式

在拖拽自由点时，不每次重新求解整个约束系统，而是采用**路径追踪**方法：

```
1. 记录拖拽前的精确解 S_old
2. 对自由点施加小位移 Δ
3. 以 S_old 为初始猜测，使用 Newton 迭代求解新位置 S_new
4. 如果 Newton 收敛 → 采用 S_new
5. 如果 Newton 发散 → 回退到全局 Groebner 基求解
```

这种方法（也称为"同伦延续"）保证了小位移下的解连续性。

#### 4.3.2 分支切换检测

当 Newton 迭代发散或求解器返回不同的解分支时：

1. 新解与旧解的差异超过阈值（如点移动距离 > 画面像素的 10%）
2. 触发**分支切换警告**——在画布上以闪烁动画标记该点
3. 用户可选择：(a) 接受新分支；(b) 回退到切换前的状态；(c) 手动选择分支

#### 4.3.3 多解选择器缓存

对于圆-线交点等多解构造，在拖拽期间缓存用户上次选择的分支：

```javascript
/**
 * @brief 多解分支缓存
 *
 * 在连续拖拽期间，记住用户在上一个位置选择的分支，
 * 在新位置自动选择"最近"的分支。
 */
class BranchCache {
    constructor() {
        this.cache = new Map();  // key: constraintId, value: selectedBranchIndex
    }

    /**
     * @param {number} constraintId
     * @param {Array<{x: number, y: number}>} solutions
     * @param {{x: number, y: number}} previousSelection
     * @returns {number} 最近分支的索引
     */
    selectNearestBranch(constraintId, solutions, previousSelection) {
        // 选择欧氏距离最近的分支
        let minDist = Infinity;
        let nearestIdx = 0;
        for (let i = 0; i < solutions.length; i++) {
            const dx = solutions[i].x - previousSelection.x;
            const dy = solutions[i].y - previousSelection.y;
            const dist = dx * dx + dy * dy;
            if (dist < minDist) {
                minDist = dist;
                nearestIdx = i;
            }
        }
        this.cache.set(constraintId, nearestIdx);
        return nearestIdx;
    }
}
```

### 4.4 奇点处理

当自由点移动到使两条线段平行（原本相交的交点消失）时：

1. 求解器检测到某个约束方程退化（Jacobian 矩阵秩下降）
2. 暂停拖拽动画，弹出提示"当前配置产生退化情况"
3. 允许用户：(a) 接受当前退化解；(b) 回退；(c) 以极限方式推断交点（将交点标记为"无穷远点"）

---

## 5. 实现优先级

1. **P4-1（本阶段）**：在 `web/js/modules/proof.js` 中实现 `randomizationCheck()` 函数和分支缓存 `BranchCache` 类
2. **P4-2（后续）**：在 `web-gui/src/components/panels/ProofPanel.tsx` 中增加随机验证 UI 控件
3. **P4-3（后续）**：实现 DSL 代码同步面板（CodeSyncPanel 组件）
4. **P4-4（远期）**：路径追踪拖拽模式与连续运动一致性保障

---

## 6. 总结

Cinderella 的随机化定理验证为 Lv-00 提供了一种轻量级的猜想探索工具——在形式化证明前快速评估猜想的可信度。Dr.Geo 的"构造=代码"双向同步理念可以有效降低 Lv-00 用户的学习曲线，帮助用户从画布交互自然过渡到 DSL 编程。连续运动下的构造一致性策略通过路径追踪 + 分支缓存 + 奇点检测三层机制，在保证交互流畅性的同时维护几何语义的正确性。
