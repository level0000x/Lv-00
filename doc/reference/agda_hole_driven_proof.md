# Agda 逐洞填充证明范式与 Cubical 路径类型借鉴设计

> **借鉴项目**：Agda（github.com/agda/agda）
> **核心借鉴点**：逐洞填充（Hole-Driven）证明编辑范式、Cubical 路径类型系统、交互式类型上下文显示
> **分类**：P1 高优先级 / 证明编辑 UX 增强
> **日期**：2026-05-24

---

## 1. 概述

Agda 是一个基于依赖类型论的函数式编程语言和证明助手，由 Ulf Norell 在 Chalmers 理工大学开发。其两个核心特性对 Lv-00 的证明编辑体验有直接借鉴价值：

1. **逐洞填充（Hole-Driven Development）**：用户在代码中插入"洞"（`?`），Agda 显示洞的当前类型上下文（目标类型 + 作用域中的绑定），用户逐步细化直到洞被填满。这一范式天然适配 Lv-00 的几何元语言"命题模式 = 待填充的约束骨架"的直觉。

2. **Cubical 路径类型**：Agda 2.6+ 内置 Cubical Agda 模式，支持区间变量 `I`、路径类型 `PathP`、组合操作 `hcomp` 等。这些原语与 Lv-00 将"线段作为区间"、"函数块作为路径"的几何直觉形成精准对应。

本文档详述如何将这两个特性映射到 Lv-00 的 Web GUI 证明面板（ProofPanel.tsx）和类型系统（type_system.h）。

---

## 2. 逐洞填充映射到 ProofPanel.tsx

### 2.1 Agda 的 Hole 交互模型

在 Agda 中，洞的交互流程如下：

```
-- 用户输入
f : (n : Nat) → Vec A n → Vec A (n + 0)
f n xs = ?

-- Agda 响应（C-c C-,）
Goal: Vec A (n + 0)
————————————————————————————————
n : Nat
xs : Vec A n
```

核心信息结构：**目标类型（Goal）** + **上下文（Context）** + **约束条件**。

### 2.2 Lv-00 的对应概念

在 Lv-00 的几何构造证明中，"洞"不是代码中的占位符，而是**约束图中尚未确定的区域**——即命题模式的输入/输出端口之间尚未被几何构造填充的空白部分。映射关系如下：

| Agda Hole 概念 | Lv-00 ProofPanel 映射 | 说明 |
|:---|:---|:---|
| `?` 洞标记 | `UNRESOLVED_REGION` 可视化区域（虚线框） | 约束图中尚未确定的子图 |
| Goal（目标类型） | 命题模式的 `output_port` + `postcondition_constraint_ids` | 输出端口及后置条件构成待证明的目标 |
| Context（上下文） | `precondition_region_ids` + `input_port_ids` | 前置条件区域和输入端口构成可用上下文 |
| `C-c C-,` 显示目标 | 点击虚线框区域弹出上下文面板 | 交互式查看当前洞的类型信息 |
| `C-c C-SPC` 填充 | 拖拽函数块/辅助线到虚线框中 | 几何构造直接填洞 |
| `C-c C-c` 分情况 | 根据命题类型（∧/∨/→）自动展开子洞 | 复合命题的模式分解 |

### 2.3 ProofPanel.tsx 的洞交互设计

ProofPanel.tsx 需要新增以下 UI 组件和状态管理来支持逐洞填充范式：

```typescript
// === ProofPanel.tsx 新增类型定义 ===

/** 洞的状态 */
enum HoleStatus {
  UNEXPLORED,    // 未探索：洞尚未被关注
  ACTIVE,        // 当前激活：用户正在编辑此洞
  PARTIALLY_FILLED, // 部分填充：已添加约束但未完成
  FILLED,        // 已填满：约束骨架完全满足
  BLOCKED        // 阻塞：缺少所需公理包或引理
}

/** 洞的上下文信息 */
interface HoleContext {
  holeId: number;
  status: HoleStatus;
  // 目标信息
  goalProposition: Proposition;     // 待证明的命题模式
  // 上下文信息
  availableNodes: number[];         // 作用域中可引用的节点ID
  availableConstraints: string[];   // 可用的已知约束（自然语言描述）
  availableLemmas: number[];        // 可引用的已证明引理ID
  // 填充建议（由 proof_guided_fill 生成）
  suggestedFills: FillSuggestion[];
}

/** 填充建议 */
interface FillSuggestion {
  description: string;           // 自然语言描述（如"作∠A的角平分线"）
  functionBlockId: number;       // 推荐的函数块ID（可直接拖拽）
  estimatedDifficulty: number;   // 估计难度 (0-1)
  color: ProofColor;            // 建议的信任颜色
}
```

**核心交互流程：**

```
用户操作                    ProofPanel 状态变化
──────────────────────────────────────────────────
1. 点击命题面板中的目标     → 创建 ACTIVE 洞，显示 HoleContext
2. 查看上下文面板           → 渲染前置条件 + 可用引理列表
3. 接受 proof_guided_fill    → 获取填充建议，高亮推荐区域
   的填充建议
4. 拖拽函数块到虚线框       → 填充约束，重新运行合一检查
5. 洞被填满                 → status → FILLED，颜色变为绿色
```

### 2.4 AGDA风格的上下文面板布局

```
┌─────────────────────────────────────────────────┐
│ 🔍 Hole #14: 证明线段AB = 线段CD                 │
├─────────────────────────────────────────────────┤
│ 目标 (Goal):                                     │
│   LINE_SEGMENT(AB) ≡ LINE_SEGMENT(CD)            │
│   后置条件: CONSTRAINT(AB, LENGTH_EQ, CD)         │
├─────────────────────────────────────────────────┤
│ 上下文 (Context) — 可用资源:                      │
│   • 已知点: A, B, C, D, O (交点)                  │
│   • 已知约束: AB ⟂ CD, ∠AOB = 90°                 │
│   • 已知引理: SAS_congruence, ASA_congruence      │
│   • 可用函数块: 平行线_辅助线, 三角形_全等_判定    │
├─────────────────────────────────────────────────┤
│ 📋 填充建议 (由 proof_guided_fill 生成):          │
│   [推荐] 构造 △ABO ≅ △CDO (SAS准则)     难度:0.3  │
│   [备选] 通过坐标系计算距离               难度:0.6  │
│   [备选] 作辅助线构造相似三角形           难度:0.5  │
├─────────────────────────────────────────────────┤
│ [应用建议] [手动构造] [请求外部求解器]             │
└─────────────────────────────────────────────────┘
```

---

## 3. Cubical 路径类型扩展 type_system.h

### 3.1 Cubical Agda 的核心类型原语

Cubical Agda 在 Martin-Lof 类型论基础上新增：

```
I          : 区间类型（de Morgan 代数）
PathP A a b : 依赖路径类型（= Π(i:I) → A i，满足 i0 → a, i1 → b）
hcomp      : 齐次组合（从部分填充到全路径）
transp     : 传输（沿路径的类型转换）
```

其中 `I` 上的 de Morgan 代数操作包括：`i0`（最小值）、`i1`（最大值）、`~ i`（反转）、`i ∧ j`（交）、`i ∨ j`（并）。

### 3.2 Lv-00 type_system.h 的扩展策略

Lv-00 不需要完整实现 Cubical 类型论的内核。替代方案是：在 `type_system.h` 中增加 `TYPE_KIND_PATH` 和 `TYPE_KIND_INTERVAL` 两种类型种类，并建立它们与几何元语言中已有概念的桥梁。

**新增类型种类：**

```c
// === type_system.h 扩展（TYPE_KIND 枚举补充） ===
typedef enum {
    // ... 已有的种类 ...
    TYPE_KIND_INTERVAL,     /**< 区间类型 I：端点自由的线段 */
    TYPE_KIND_PATH,         /**< 路径类型 Path A a b：从a到b的路径 */
    TYPE_KIND_CUBICAL_PROP  /**< Cubical 命题：内射/满射/同伦等价等高阶性质 */
} TypeKindExtension;
```

### 3.3 Interval 类型的几何化编码

```
Interval  ≡  LINE_SEGMENT(
    left   : POINT (自由点, 符号坐标 = I0),
    right  : POINT (自由点, 符号坐标 = I1)
)
```

**de Morgan 操作在 Lv-00 约束图中的实现：**

| Cubical 操作 | Lv-00 几何操作 | 实现方式 |
|:---|:---|:---|
| `i0` | `LINE_SEGMENT.left` 端点 | 线段的左端点坐标 |
| `i1` | `LINE_SEGMENT.right` 端点 | 线段的右端点坐标 |
| `~ i` | `FLIP(line_segment)` | 交换端点顺序 |
| `i ∧ j` | `MIN(line_segment_i, line_segment_j)` | 两个线段的正则交集 |
| `i ∨ j` | `MAX(line_segment_i, line_segment_j)` | 两个线段的正则并集 |

### 3.4 路径类型的函数块编码

路径类型 `Path A a b` 在 Lv-00 中编码为：

```
PathType := FUNCTION_BLOCK {
    input:  [interval: INTERVAL]
    output: [result: A]
    boundary_conditions: {
        CONSTRAINT(result@(interval.left),  EQUIV, a),
        CONSTRAINT(result@(interval.right), EQUIV, b)
    }
    determinant: VERIFIED  // 路径由两端点唯一确定
}
```

**hcomp 操作**（齐次组合）的 Lv-00 编码：

```
hcomp := FUNCTION_BLOCK {
    input:  [
        base: A,                         // 基础面
        partial_paths: [PARTIAL_PATH]*   // 已确立的部分边界路径
    ]
    output: [full_path: Path A base target]
    internal: {
        // 将部分边界路径拼接为完整路径
        FOR_EACH partial_path: 
            CONSTRAINT(full_path.interval, COINCIDES, partial_path.interval)
    }
}
```

### 3.5 类型检查层面的路径相等判定

```
type_check_path_equality(p: Path A a b, q: Path A a b):
    1. 规范化两条路径的约束图
    2. 若 p 的约束图与 q 的约束图合一 → 路径相等
    3. 若 p 的约束图可经 hcomp 简化为 q 的约束图 → 路径同伦等价
    4. 若 p 和 q 的端点分别合一但路径本体不同 → 路径不相等（但端点相等）
```

---

## 4. proof_guided_fill() API 设计

### 4.1 函数声明（追加到 proof.h）

```c
/**
 * @brief 引导式洞填充 —— 借鉴 Agda 逐洞填充范式
 *
 * 给定证明导航器中的一个未完成洞（unresolved region），分析当前上下文
 * 并生成一组填充建议。每条建议包含可应用的函数块、预计难度和信任颜色。
 *
 * 工作流程：
 *  1. 解析洞的目标类型和可用上下文
 *  2. 在已加载的函数块/引理库中搜索匹配项
 *  3. 对每个匹配项评估适用性（合一检查 + 依赖颜色传播）
 *  4. 生成排序后的填充建议列表
 *
 * @param nav              证明导航器
 * @param hole_id          洞ID（对应 unresolved_region 的节点ID）
 * @param out_suggestions  输出：填充建议数组（调用者需用 lv00_free 释放）
 * @param out_count        输出：填充建议数量
 * @return 找到的建议数量，失败返回 -1
 *
 * @note 借鉴 Agda 的 C-c C-SPC 交互：显示洞的类型上下文 + 候选填充项
 * @note 建议按以下优先级排序：
 *        1. 完全匹配（合一检查通过，信任颜色为绿）
 *        2. 部分匹配（需额外辅助构造）
 *        3. 外部求解器建议（标记橙色 oracle 依赖）
 *
 * @see arend_hott_design_notes.md —— HoTT 路径直觉与此处几何填洞的深层对应
 */
int proof_guided_fill(
    ProofNavigator *nav,
    int hole_id,
    FillSuggestion **out_suggestions,
    int *out_count
);
```

### 4.2 FillSuggestion 结构体设计

```c
/**
 * @brief 洞填充建议结构体
 *
 * 借鉴 Agda 的 hole interaction，每条建议包含：
 * - 建议的描述性文字（如"应用 SAS 全等判定"）
 * - 建议应用的函数块/引理
 * - 估计的难度（用于排序）
 * - 信任颜色（预评估的依赖纯度）
 */
typedef struct FillSuggestion {
    char *description;               /**< 自然语言填充描述 */
    int suggested_func_block_id;     /**< 建议的函数块ID（-1 = 需创建新辅助构造） */
    int suggested_lemma_id;          /**< 建议的引理ID（-1 = 非引理建议） */
    double estimated_difficulty;     /**< 估计难度 (0.0 ~ 1.0) */
    ProofColor estimated_color;      /**< 预评估的信任颜色 */
    int *remaining_sub_holes;        /**< 填充后仍剩余的子洞ID数组 */
    int remaining_count;             /**< 剩余子洞数量 */
    bool requires_aux_construction;  /**< 是否需要辅助构造 */
    char *aux_construction_hint;     /**< 辅助构造提示（如"作点A关于BC的对称点"）*/
} FillSuggestion;
```

---

## 5. 实现路线图

### 5.1 第一阶段：ProofPanel 洞交互原型（P1-1）

- [ ] 在 ProofPanel.tsx 中添加 `HoleContextPanel` 组件
- [ ] 实现洞状态管理（UNEXPLORED → ACTIVE → FILLED）
- [ ] 实现上下文面板渲染（前置条件展示 + 可用引理列表）
- [ ] 实现拖拽函数块到虚线框的填洞交互
- [ ] 实现复合命题的自动子洞展开

### 5.2 第二阶段：guided_fill 逻辑实现（P1-2）

- [ ] 实现 `proof_guided_fill()` 的核心逻辑（在 `src/proof.c` 中）
- [ ] 实现函数块库匹配搜索（基于类型签名的合一检查）
- [ ] 实现填充建议排序（难度估计 + 信任颜色预测）
- [ ] 实现辅助构造自动生成提示
- [ ] 实现建议预览（预览填充后的约束图状态）

### 5.3 第三阶段：Cubical 基础类型（P1-3）

- [ ] 在 `type_system.h` 中新增 `TYPE_KIND_INTERVAL` 和 `TYPE_KIND_PATH`
- [ ] 实现 Interval 类型的 geometric encoding
- [ ] 实现路径相等判定（基于约束图合一 + hcomp 规范化）
- [ ] 实现 de Morgan 代数操作的几何模拟
- [ ] 编写 Cubical 路径类型的单元测试

### 5.4 第四阶段：Web GUI 集成（P1-4）

- [ ] 将填充建议连接到 ProofPanel 的推荐面板
- [ ] 实现"一键填充"功能（自动应用最优建议）
- [ ] 实现填充历史导航（借鉴 Agda 的 C-c C-x 和 give 机制）
- [ ] 实现路径可视化（在约束图中渲染区间线段和路径映射）

---

## 6. 设计决策与权衡

### 6.1 不完整实现 Cubical 类型论

Lv-00 仅在"几何直觉"层面借用 Cubical Agda 的类型原语，而不实现完整的 de Morgan 立方类型核。原因：
- Lv-00 的约束图已经是比单纯类型更丰富的抽象
- 完整 Cubical 类型论的实现复杂度极高（约 5000+ 行核心代码）
- 几何元语言的"线段"天然是区间，"函数块"天然是路径——不需要重新发明

### 6.2 洞模型 vs 策略模型

逐洞填充（bottom-up）与多策略证明（top-down）并不冲突，而是互补：
- **逐洞填充**：适合手动引导的精细证明，用户在每个洞做出具体构造选择
- **多策略**：适合自动探索，系统尝试不同方法找到最优路径
- **Lv-00 设计**：洞是策略的内部状态——当某个策略的某一步无法自动推进时，该策略在该点创建一个洞等待用户输入

### 6.3 与 Arend/HoTT 路径类型的区别

| 方面 | Arend/HoTT 路径 | Cubical Agda 路径 | Lv-00 路径 |
|:---|:---|:---|:---|
| 理论基础 | 同伦类型论 | Cubical 类型论 | 几何元语言 + 约束图 |
| Interval | 抽象区间类型 | de Morgan 区间代数 | LINE_SEGMENT 几何节点 |
| 路径组合 | `p * q` 函数复合 | `p ∙ q` + hcomp | Compose 组合子 |
| 路径反转 | `sym p` | `λ i → p (~ i)` | 线段端点翻转 |
| 高维路径 | 迭代路径类型 | 嵌套 Interval | 高维约束图 |

---

## 7. 补充：Cubical Agda 的 transp 与 hcomp 细节

### 7.1 transp（传输）的几何化

`transp` 是 Cubical Agda 中最复杂的原语，它沿路径将元素从一个类型传输到另一个类型：

```
transp (λ i → A i) φ a : A i1  [φ ⊢ transp ... a = a]
```

在 Lv-00 中，类型传输对应**约束图的连续形变**——在保持拓扑性质的前提下，将节点从一种类型区域移动到另一种：

```
GEO_TRANSP := FUNCTION_BLOCK {
    input:  [
        type_path: PATH(TypeRegion, A0, A1),  // 类型空间中的路径
        element: POINT(in A0),                 // 在A0中的点
        cofibration: CONSTRAINT                 // 余纤维条件φ
    ]
    output: [transported: POINT(in A1)]
    invariant:  // 在φ成立时，transported = element（保持原位）
}
```

### 7.2 de Morgan 连接词（∧ / ∨）的正则几何实现

Cubical 类型论中的区间连接词 `i ∧ j` 和 `i ∨ j` 在 de Morgan 代数中有精确定义。在 Lv-00 的几何编码中，它们对应于区间线段的布尔正则操作：

```
INTERVAL_MEET(i, j)  →  取两个区间线段的交集（交叠部分）
INTERVAL_JOIN(i, j)  →  取两个区间线段的并集（覆盖部分）
```

这些操作在约束图层面由已有的正则布尔操作（`normalization.h` 中的区域交并）提供支持，不需要新增专门的类型规则。

### 7.3 Path 类型的连续函数语义

在 HoTT 中，路径 `p : a = b` 的核心语义是"存在从 a 到 b 的连续映射"。Lv-00 中的约束图天然支持连续性：
- 当节点沿约束图变换时，约束关系保持
- 规范化遍保证约束图的"平滑形变"（不引入新的奇点）
- 这一性质使得 Lv-00 的约束图在不需要显式声明连续性的情况下，自动满足 HoTT 对路径的连续性要求

---

## 8. 总结

Agda 的逐洞填充范式为 Lv-00 的 ProofPanel.tsx 提供了成熟的交互模型。将"洞"映射为约束图中的未确定区域，将"填充"映射为拖拽函数块，既保留了 Agda 的流畅证明编辑体验，又利用了 Lv-00 几何可视化的天然优势。Cubical 路径类型与 Lv-00 的线段/函数块抽象之间的对应关系进一步强化了"几何即是证明"的元语言直觉——使相等性证明可以被直观地"看见"为约束图上的路径。transp 和 hcomp 的几何化编码表明，Lv-00 不需要完整实现 Cubical 类型论的内核即可获得其几何直觉的表达能力。
