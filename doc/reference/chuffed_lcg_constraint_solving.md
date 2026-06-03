# Chuffed 惰性子句生成约束求解核心借鉴设计

> **借鉴项目**：Chuffed（github.com/chuffed/chuffed）
> **核心借鉴点**：惰性子句生成（LCG）、FD+SAT 深度融合、可解释传播器（Instrumented Propagators）、冲突驱动回跳
> **分类**：P2 高优先级 / 约束求解引擎
> **日期**：2026-05-24

---

## 1. 概述

Chuffed 是由澳大利亚莫纳什大学（Monash University）优化研究组开发的开源惰性子句生成（Lazy Clause Generation, LCG）约束求解器，使用 C++ 编写，在 MiniZinc Challenge 中多次获得 FD（Finite Domain）赛道冠军。Chuffed 的核心创新在于将**有限域传播（FD Propagation）**与**布尔可满足性求解（SAT Solving）**深度融合——FD 传播器的每次推论都被记录为布尔蕴含子句，一旦冲突发生，SAT 引擎对积累的子句进行冲突分析并学习新子句（nogood），驱动非时序回跳（non-chronological backtracking）。

这种 LCG 架构对 Lv-00 的约束求解引擎有根本性的借鉴价值。Lv-00 的约束图（`constraint_graph.h`）中的几何约束传播天然是一个 FD 传播问题——每个几何关系（如"三点共线"、"两线段等长"、"点在圆上"）都可以建模为 FD 约束，其传播器在符号变量域上做推理。Chuffed 的 LCG 架构提供了一种将**几何约束的符号推理**与**冲突驱动的 nogood 学习**统一在同一框架下的工程方案。

Chuffed 的四个核心组件对 Lv-00 的直接映射如下：（1）**惰性子句生成（LCG）**——将几何约束传播器的每次符号推论记录为蕴含子句，对应 Lv-00 的 `ConstraintReason` 追踪机制；（2）**FD+SAT 深度融合**——几何变量的域约简（如"x 必须为有理数且 > 0"）编码为布尔变量骨架上的文字传播，对应 Lv-00 的符号约束 + 布尔骨架双引擎；（3）**可解释传播器（Instrumented Propagators）**——每个几何约束传播步骤都携带可追溯的"原因"信息，对应 Lv-00 证明系统中每个 `ProofStep` 的 `justification` 字段；（4）**冲突分析 → nogood 学习 → 非时序回跳**——从几何约束冲突中提取不可满足核心，生成 nogood 防止同一组子句在未来再被尝试。

本文将围绕上述四个映射方向，系统阐述 Chuffed LCG 架构如何启发 Lv-00 的约束求解引擎设计，并给出几何约束可解释传播和冲突学习的 C 代码示例。

---

## 2. LCG 架构：FD 传播 + SAT 学习统一框架

### 2.1 Chuffed LCG 的工作流程

Chuffed 的 LCG 架构将 FD 求解器的搜索过程与 SAT 求解器的冲突分析无缝结合：

```
搜索决策（分支）
  ↓
[FD 传播层]
  ├─ 对当前变量域执行约束传播
  ├─ 每步域约简记录为蕴含子句：
  │    reason:  (x >= 3) ← (y <= 5) ∧ (z = 1)
  │    编码为:  ¬(y <= 5) ∨ ¬(z = 1) ∨ (x >= 3)
  └─ 若域变空 → 传播失败
       ↓
    [SAT 冲突分析层]
       ├─ 收集冲突涉及的蕴含子句
       ├─ 对蕴含子句图进行归结（resolution）
       ├─ 生成第一唯一蕴含点（1-UIP）nogood
       └─ 将 nogood 加入子句库
       ↓
    [非时序回跳]
       ├─ 根据 nogood 确定回跳层级
       └─ 撤销回跳层级以上的所有决策和传播
```

关键洞察：**FD 传播器本身不变，只是被"instrumented"（装上仪表）以记录每次推理的原因**。这使得 FD 求解器的现有传播逻辑无需重写，只需在传播时附带记录"我是因为哪些域文字而做了这次域约简"。

### 2.2 LCG 映射到 Lv-00 constraint_graph.h + solver.h

| Chuffed LCG 概念 | Lv-00 映射（constraint_graph.h / solver.h） | 几何语义 |
|:---|:---|:---|
| 变量域（Domain） | `SymbolicDomain` — 符号变量的可能取值集合 | 几何点的可能坐标集合（如 x ∈ Q, x > 0） |
| FD 传播器（Propagator） | `ConstraintPropagator` — 几何约束的传播函数 | 共线性约束：已知 A,B 坐标 → 限制 C 在直线 AB 上 |
| 域约简（Domain Reduction） | `constraint_graph_restrict_domain()` — 限制符号域 | C 的 y 坐标被限制为与 A,B 共线的值 |
| 蕴含子句（Implication Clause） | `ConstraintReason` — 传播原因记录 | "C 的 y 坐标被约简是因为 A,B 已确定且 collinear(A,B,C) 激活" |
| 冲突（Conflict） | `constraint_graph_detect_conflict()` — 域变空检测 | C 同时被要求共线于 AB 和 DE，但两线不重合 |
| Nogood 学习 | `constraint_graph_learn_nogood()` — 不可满足核心提取 | 学习 nogood: ¬(C 自由) ∨ ¬(collinear(A,B,C)) ∨ ¬(collinear(D,E,C)) 当 AB 不平行于 DE |
| 非时序回跳 | `constraint_graph_backjump()` — 撤销到安全层级 | 回跳到 AB 或 DE 被定义的层级，而非简单的上一层 |
| VSIDS 决策启发式 | `constraint_graph_decision_heuristic()` | 选择下一个要实例化的几何变量 |

### 2.3 几何约束的 FD 传播器示例：三点共线

以"三点共线"（collinear(A, B, C)）约束为例，将其建模为 FD 传播器：

```
collinear(A, B, C) 的约束语义：
  determinant |Ax Ay 1|
              |Bx By 1| = 0
              |Cx Cy 1|

即：(Bx - Ax)*(Cy - Ay) - (By - Ay)*(Cx - Ax) = 0
```

传播规则（当两个点的坐标已知时，限制第三个点）：

```
规则 1: 当 A, B 已知 → C 的坐标被限制为满足直线方程
  域约简: Cy ∈ { y | (Bx - Ax)*(y - Ay) = (By - Ay)*(Cx - Ax) }
  原因记录: (C 约束为 collinear(A,B,C)) ← (A 坐标已确定) ∧ (B 坐标已确定)

规则 2: 当 A, C 已知 → B 的坐标被限制（同理）
规则 3: 当 B, C 已知 → A 的坐标被限制（同理）
规则 4: 当 C 的坐标被两个不重合直线同时约简 → 域变空 → 冲突
  冲突解释: collinear(A,B,C) ∧ collinear(D,E,C) 但 AB 与 DE 不重合
           → C 必须在两直线交点上，但两直线平行/不重合
           → 冲突！除非 C 恰好是两直线交点
```

---

## 3. FD+SAT 深度融合：符号约束 + 布尔骨架

### 3.1 Chuffed 的双层表示

Chuffed 为每个 FD 变量维护一个 SAT 层面的布尔编码。以整型变量 `x ∈ {0..5}` 为例：

```
FD 层:  x ∈ {0, 1, 2, 3, 4, 5}
SAT 层: 布尔文字:
  ⟦x = 0⟧, ⟦x = 1⟧, ⟦x = 2⟧, ⟦x = 3⟧, ⟦x = 4⟧, ⟦x = 5⟧
  ⟦x ≤ 0⟧, ⟦x ≤ 1⟧, ⟦x ≤ 2⟧, ⟦x ≤ 3⟧, ⟦x ≤ 4⟧
  ⟦x ≥ 0⟧, ⟦x ≥ 1⟧, ⟦x ≥ 2⟧, ⟦x ≥ 3⟧, ⟦x ≥ 4⟧, ⟦x ≥ 5⟧

约束: 每个文字在 SAT 层有真值
传播: FD → 域约简 → SAT 文字赋值
冲突: SAT → 归结 → nogood → FD 回跳
```

双层表示的关键收益：**SAT 层负责高效的冲突分析和子句学习，FD 层负责高效的域推理和传播**。两者通过文字映射桥接。

### 3.2 映射到 Lv-00：符号约束 + 布尔骨架

在 Lv-00 中，这种双层表示有自然的对应：

```
符号约束层（对应 FD 层）:
  - 几何变量的符号域: SymbolicDomain（如 Point.x ∈ Rational, Point.x > 0）
  - 约束传播: constraint_graph_propagate() 在符号域上执行代数推理
  - 域约简: 从 SymbolicDomain 中排除不可能的值

布尔骨架层（对应 SAT 层）:
  - 几何谓词的真值: 如 ⟦collinear(A,B,C)⟧, ⟦on_circle(O,r,P)⟧, ⟦parallel(L1,L2)⟧
  - 定性约束: 如 ⟦三角形 ABC 是锐角三角形⟧ → ⟦∠A < 90°⟧ ∧ ⟦∠B < 90°⟧ ∧ ⟦∠C < 90°⟧
  - 情况分支: ⟦点 P 在线段 AB 上⟧ vs ⟦点 P 在线段 AB 的延长线上⟧
```

```c
/**
 * @brief Lv-00 的双层约束表示 —— 借鉴 Chuffed FD+SAT 架构
 *
 * 符号约束层 (SymbolicConstraintLayer):
 *   - 处理可代数化的几何约束（距离、角度、共线性等）
 *   - 使用 Groebner 基 / 面积法进行符号域传播
 *
 * 布尔骨架层 (BooleanSkeletonLayer):
 *   - 处理定性几何谓词的真值
 *   - 使用 CDCL SAT 求解器（如 MiniSat 接口）处理逻辑组合
 *
 * 两层通过桥接表 (BridgeTable) 连接：
 *   符号域约简 → 布尔文字赋值 → SAT 传播
 *   SAT 冲突 → nogood → 符号域回退
 */

typedef struct {
    /** 符号约束层 */
    ConstraintGraph *symbolic_layer;

    /** 布尔骨架层（可选的 SAT 求解器句柄） */
    void *sat_solver;  // Minisat::Solver* 或等价接口

    /** 桥接表：符号文字 → SAT 文字 */
    struct {
        int symbolic_literal_id;  // 符号约束图中的文字标识
        int sat_var;              // SAT 求解器中的变量编号
        char *description;        // 文字的可读描述
    } *bridge_table;
    int bridge_table_size;
    int bridge_table_capacity;
} DualLayerConstraintSystem;

/**
 * @brief 将符号域约简传播到布尔骨架层
 *
 * 当符号约束层对变量 v 做了域约简，
 * 对应更新 SAT 层中 ⟦v ∈ reduced_domain⟧ 的文字赋值。
 */
void dual_layer_sync_downward(
    DualLayerConstraintSystem *dlcs,
    int variable_id,
    SymbolicDomain new_domain
);

/**
 * @brief 将 SAT 冲突的 nogood 翻译回符号约束层
 *
 * SAT 层发现冲突后，提取的 nogood 中可能包含几何谓词的布尔文字。
 * 需要将这些布尔文字翻译回符号约束以供回跳使用。
 */
NogoodClause *dual_layer_translate_nogood(
    DualLayerConstraintSystem *dlcs,
    const int *sat_nogood,
    int sat_nogood_len
);
```

---

## 4. 可解释传播器（Instrumented Propagators）

### 4.1 Chuffed 的可解释传播设计

传统 FD 传播器执行域约简时，不记录"为什么做了这个约简"。Chuffed 的可解释传播器（Instrumented Propagator）在每个域约简操作中附带一个 `reason` 参数，指明此约简的因果链：

```cpp
// Chuffed 风格：可解释的域约简
bool collinear_propagator::propagate() {
    // 检查前提条件是否满足
    if (a.isFixed() && b.isFixed()) {
        // 传播：A 和 B 已知 → C 必须共线
        // 计算 A, B 确定的直线方程
        Rational slope = computeSlope(a.val(), b.val());

        // 域约简：C 必须在该直线上
        // reason 记录：C 的约简是因为 A 和 B 已被固定
        Clause* reason = explain_coords_known(a, b);
        //                                       ↑ 这个 reason 就是蕴含子句

        if (!c.restrictToLine(a.val(), b.val(), reason)) {
            // C 的域变为空 → 传播失败，返回冲突
            return false;
        }
    }
    // 其他传播规则...
    return true;
}

// 冲突时，调用 explain() 生成解释
Clause* collinear_propagator::explain(Literal conflict_lit) {
    // 返回导致 conflict_lit 被强制赋值为假的蕴含子句
    // 例如：¬(A 坐标已确定) ∨ ¬(B 坐标已确定) ∨ (C 共线于 AB)
}
```

### 4.2 Lv-00 可解释几何传播器实现

```c
/**
 * @file constraint_propagator.h
 * @brief Lv-00 可解释几何约束传播器 —— 借鉴 Chuffed Instrumented Propagators
 *
 * 每条几何约束都有对应的传播器实现，传播器在每次域约简时记录 ConstraintReason。
 * 这是将几何证明可追溯性嵌入约束求解引擎的关键机制。
 */

#include "constraint_graph.h"
#include "symbolic_domain.h"

// ─── 传播原因的类型定义 ─────────────────────────────────────────

/**
 * @brief 约束传播原因 —— 对应 Chuffed 的蕴含子句
 *
 * 每条 reason 记录一次域约简的因果链：
 *   "变量 v 的域被约简到 D' 是因为前提 literals 列表全部成立"
 *
 * 在冲突分析时，reason 被展开为第一个蕴含子句参与归结。
 */
typedef struct {
    /** 被约简的变量 ID */
    int target_var_id;

    /** 约简前的域（用于回退） */
    SymbolicDomain old_domain;

    /** 约简后的域 */
    SymbolicDomain new_domain;

    /** 前提文字列表：导致此约简的已知条件 */
    int *premise_literals;
    int premise_count;

    /** 触发此传播的约束 ID */
    int constraint_id;

    /** 此 reason 在决策树中的层级 */
    int decision_level;

    /** 自然语言描述（可读解释，用于调试和用户界面） */
    char *explanation_text;
} ConstraintReason;

// ─── 共线性传播器 ────────────────────────────────────────────────

/**
 * @brief 共线性约束的可解释传播器
 *
 * 约束：collinear(A, B, C)
 * 语义：det([Ax,Ay,1; Bx,By,1; Cx,Cy,1]) = 0
 *
 * 传播规则：
 *  1. A, B 固定 → 限制 C 在直线 AB 上
 *  2. A, C 固定 → 限制 B 在直线 AC 上
 *  3. B, C 固定 → 限制 A 在直线 BC 上
 *  4. C 同时被两条不重合直线约束 → 冲突 → 生成 nogood
 */
typedef struct {
    int point_a_id;
    int point_b_id;
    int point_c_id;
} CollinearPropagator;

/**
 * @brief 执行共线性约束传播
 *
 * @return PROPAGATE_OK — 传播成功（可能做了域约简）
 *         PROPAGATE_CONFLICT — 传播失败（冲突），*out_conflict_reason 包含冲突信息
 */
PropagateStatus collinear_propagate(
    const CollinearPropagator *prop,
    ConstraintGraph *cg,
    int decision_level,
    ConstraintReason **out_conflict_reason
) {
    SymbolicCoords a, b, c;
    bool a_fixed = constraint_graph_get_fixed_coords(cg, prop->point_a_id, &a);
    bool b_fixed = constraint_graph_get_fixed_coords(cg, prop->point_b_id, &b);
    bool c_fixed = constraint_graph_get_fixed_coords(cg, prop->point_c_id, &c);

    // 规则 1: A 和 B 固定 → 限制 C
    if (a_fixed && b_fixed && !c_fixed) {
        // 计算直线 AB 的方程
        SymbolicNumber slope_num = symbolic_sub(b.y, a.y);
        SymbolicNumber slope_den = symbolic_sub(b.x, a.x);

        // 构造域约简：C 必须满足 (Cx - Ax) * (By - Ay) = (Cy - Ay) * (Bx - Ax)
        SymbolicDomain new_cx, new_cy;
        compute_line_restriction(a, b, &new_cx, &new_cy);

        // 创建解释原因（reason）
        ConstraintReason *reason = constraint_reason_create(
            /* target: */ prop->point_c_id,
            /* premises: */ (int[]){
                constraint_graph_get_fixed_literal(cg, prop->point_a_id),
                constraint_graph_get_fixed_literal(cg, prop->point_b_id)
            },
            /* premise_count: */ 2,
            /* constraint_id: */ constraint_graph_get_constraint_id(cg, "collinear"),
            /* decision_level: */ decision_level,
            /* explanation: */ "点 C 的坐标被限制在直线 AB 上，因为 A 和 B 的坐标已确定"
        );

        // 执行域约简
        if (!constraint_graph_restrict_domain(cg, prop->point_c_id, &new_cx, &new_cy, reason)) {
            // 域约简失败 → 域变空 → 冲突
            // C 已经被其他约束限制在与 AB 不重合的直线上
            *out_conflict_reason = constraint_reason_create_conflict(
                reason,
                "冲突：点 C 同时被要求共线于 AB 和另一条不重合直线——两线交点唯一，"
                "但当前 C 的域与交点坐标不一致"
            );
            return PROPAGATE_CONFLICT;
        }
    }

    // 规则 2: A 和 C 固定 → 限制 B（对称）
    // 规则 3: B 和 C 固定 → 限制 A（对称）

    // 规则 4: 三点都固定 → 验证共线性
    if (a_fixed && b_fixed && c_fixed) {
        SymbolicNumber det = compute_collinear_determinant(a, b, c);
        if (!symbolic_is_zero(det)) {
            *out_conflict_reason = constraint_reason_create_conflict_simple(
                decision_level,
                "冲突：A、B、C 三点不共线，行列式 = %s（非零）",
                symbolic_number_to_string(det)
            );
            return PROPAGATE_CONFLICT;
        }
    }

    return PROPAGATE_OK;
}
```

---

## 5. 冲突分析 → Nogood 学习 → 非时序回跳

### 5.1 Chuffed 的完整冲突处理流程

当传播器检测到域变空（冲突）时，Chuffed 启动以下流程：

```
冲突检测：域(D) = ∅
  ↓
[冲突分析 — 归结]
  ├─ 从冲突文字开始（域变空的文字）
  ├─ 沿蕴含子句图反向归结：
  │   conflict_reason 的前提 → 前提的 reason → 前提的前提的 reason → ...
  ├─ 直到到达 1-UIP（第一唯一蕴含点）或决策文字
  └─ 生成 nogood 子句：learned_clause
       ↓
    [Nogood 学习]
       ├─ 将 nogood 加入子句数据库
       ├─ 如果 nogood 很短（≤ 阈值），标记为"永久学习"
       ├─ 否则标记为"临时"（可被删除策略回收）
       └─ 更新 VSIDS（变量活跃度）
       ↓
    [非时序回跳]
       ├─ 计算 nogood 中"第二高"的决策层级
       ├─ 回到该层级（可能跳过多个决策层级）
       ├─ 在该层级，nogood 变为单元子句 → 强制传播
       └─ 继续搜索
```

**1-UIP（First Unique Implication Point）** 是 CDCL SAT 求解器的核心概念：在冲突蕴含图中，从冲突节点向根方向遍历，第一个切割所有冲突路径的节点即为 1-UIP。使用 1-UIP 生成的 nogood 是"最短且最有信息量"的冲突解释。

### 5.2 Lv-00 的几何约束冲突分析与 Nogood 学习

```c
/**
 * @brief Lv-00 冲突分析 —— 借鉴 Chuffed CDCL 的 1-UIP nogood 学习
 *
 * 当几何约束传播检测到冲突（域变空），调用此函数进行冲突分析。
 *
 * 工作流程（借鉴 Chuffed 的 conflict_analysis()）：
 *  1. 从冲突 reason 开始，构建冲突蕴含图
 *  2. 沿蕴含图反向归结，找到 1-UIP 切割
 *  3. 生成 nogood 子句（不可满足核心）
 *  4. 将 nogood 加入 constraint_graph 的 nogood_store
 *  5. 计算回跳层级并执行非时序回跳
 */

/**
 * @brief 冲突蕴含图中的节点
 */
typedef struct {
    int literal_id;              /**< 蕴含的文字 */
    int decision_level;          /**< 决策层级 */
    ConstraintReason *reason;    /**< 导致此文字被蕴含的原因（可为 NULL = 决策文字） */
} ImplicationNode;

/**
 * @brief Nogood 子句 —— 由冲突分析学到的不可满足核心
 */
typedef struct {
    /** nogood 中的文字列表（析取式，表示"不允许这些文字同时为真"） */
    int *literals;
    int literal_count;

    /** 此子句的活跃度（VSIDS 权重，用于子句删除策略） */
    double activity;

    /** 此子句是否被永久保留（短 nogood 通常永久保留） */
    bool is_permanent;

    /** 此 nogood 的人类可读描述 */
    char *description;
} NogoodClause;

/**
 * @brief 执行冲突分析并学习 nogood
 *
 * @param cg              约束图
 * @param conflict_reason 冲突原因（传播器返回的）
 * @param out_backjump_level 输出：回跳到的决策层级
 * @return 新学到的 nogood 子句
 *
 * 几何约束冲突分析的特殊性：
 *  - 原因追踪链中包含代数条件（如"直线 AB 与 DE 不重合"）
 *  - 归结过程中可能需要代数判定（如判断两条直线是否平行）
 *  - Nogood 可能是参数化的（如"对于任意点 X，如果 collinear(A,B,X) 且 collinear(C,D,X)，
 *    则要求 A,B 与 C,D 确定的直线重合"）
 */
NogoodClause *constraint_graph_analyze_conflict(
    ConstraintGraph *cg,
    ConstraintReason *conflict_reason,
    int *out_backjump_level
) {
    // ── 步骤 1: 构建冲突蕴含图 ───────────────────────────────────
    // 从冲突文字开始，反向遍历 reason 链表，收集所有相关蕴含节点

    ImplicationNode *implication_graph = NULL;
    int graph_size = 0;

    build_implication_graph(conflict_reason, &implication_graph, &graph_size);

    // ── 步骤 2: 找到 1-UIP ────────────────────────────────────────
    // 对蕴含图进行遍历，找到第一唯一蕴含点
    // 算法：从冲突节点开始，按决策层级逆序遍历

    int uip_literal_id = find_first_uip(implication_graph, graph_size);

    // ── 步骤 3: 生成 nogood（不可满足核心）─────────────────────────
    // nogood = 从 conflict 到 1-UIP 路径上所有决策文字的否定
    // 在几何上下文中，nogood 可能包含：
    //   ¬(A 坐标为 (x1,y1)) ∨ ¬(B 坐标为 (x2,y2)) ∨ ¬(collinear(A,B,C) 激活)

    NogoodClause *nogood = malloc(sizeof(NogoodClause));
    nogood->literal_count = 0;
    nogood->literals = malloc(sizeof(int) * graph_size);

    // 收集 1-UIP 切割经过的所有文字
    collect_uip_cut_literals(
        implication_graph, graph_size,
        uip_literal_id,
        nogood->literals, &nogood->literal_count
    );

    // 生成人类可读描述
    nogood->description = generate_nogood_description(
        cg, nogood->literals, nogood->literal_count
    );
    nogood->activity = 1.0;
    nogood->is_permanent = (nogood->literal_count <= 5);

    // ── 步骤 4: 加入 nogood 存储 ─────────────────────────────────
    nogood_store_add(cg->nogood_store, nogood);

    // ── 步骤 5: 计算回跳层级 ──────────────────────────────────────
    // 回跳层级 = nogood 中"第二高"的决策层级
    // （第一高是冲突层级，第二高是回跳目标）

    int max_level = -1, second_max_level = -1;
    for (int i = 0; i < nogood->literal_count; i++) {
        int level = constraint_graph_get_decision_level(cg, nogood->literals[i]);
        if (level > max_level) {
            second_max_level = max_level;
            max_level = level;
        } else if (level > second_max_level && level < max_level) {
            second_max_level = level;
        }
    }
    *out_backjump_level = (second_max_level >= 0) ? second_max_level : 0;

    // ── 步骤 6: 清理 ──────────────────────────────────────────────
    free_implication_graph(implication_graph, graph_size);

    return nogood;
}

/**
 * @brief 执行非时序回跳
 *
 * 与简单的时序回溯（每次回退一层）不同，
 * 非时序回跳根据 nogood 的决策层级信息，直接跳过多层。
 *
 * 在回跳目标层级，nogood 变为单元子句 → 强制传播剩余文字。
 */
void constraint_graph_backjump(
    ConstraintGraph *cg,
    int target_level,
    NogoodClause *guiding_nogood
) {
    // 撤销从当前层级到 target_level+1 的所有决策和传播
    for (int level = cg->current_decision_level; level > target_level; level--) {
        constraint_graph_undo_level(cg, level);
    }

    cg->current_decision_level = target_level;

    // 在目标层级，将 nogood 作为单元子句强制传播
    // nogood 中除了一个文字外，其他文字在此层级都已为假
    // → 剩余文字必须为真，以此引导搜索
    constraint_graph_propagate_nogood_unit(cg, guiding_nogood, target_level);
}
```

---

## 6. 几何约束的 Nogood 示例

### 6.1 示例 1：三点共线冲突

考虑以下几何构造场景：

```
已知：
  A = (0, 0)   — 决策层级 1
  B = (2, 0)   — 决策层级 2
  D = (0, 3)   — 决策层级 3
  E = (2, 3)   — 决策层级 4

约束声明：
  collinear(A, B, C)  — C 在直线 AB (y = 0) 上，决策层级 5
  collinear(D, E, C)  — C 在直线 DE (y = 3) 上，决策层级 5
```

传播器 `collinear(A,B,C)` 将 C 的 y 坐标域约简为 {0}（reason: A 和 B 已知），传播器 `collinear(D,E,C)` 试图将 C 的 y 坐标域约简为 {3}，导致域交集为空。

**冲突分析**：

```
冲突蕴含图（简化）：
  ⟦C.y = 0⟧    ← collinear(A,B,C) 传播 ← ⟦A 固定⟧(level 1) ∧ ⟦B 固定⟧(level 2)
  ⟦C.y = 3⟧    ← collinear(D,E,C) 传播 ← ⟦D 固定⟧(level 3) ∧ ⟦E 固定⟧(level 4)
  ⟦C.y = 0⟧ ∧ ⟦C.y = 3⟧ → ⊥ (冲突)

1-UIP 切割包含的文字：
  ¬⟦collinear(A,B,C) 激活⟧ ∨ ¬⟦collinear(D,E,C) 激活⟧ ∨ ⟦AB ∥ DE⟧
                                      ↑ 直线 AB (y=0) 平行于 DE (y=3) 且不重合

学到的 nogood：
  "如果同时声明 C 与 A,B 共线 且 C 与 D,E 共线，
   则要求直线 AB 与 DE 重合（即 A,B,D,E 四点共线）"
```

### 6.2 示例 2：过约束的三角形

```
已知：
  A = (0, 0)   — 决策层级 1
  B = (1, 0)   — 决策层级 2
  C = (?, ?)   — 未定

约束声明（过约束）：
  AB = 1                    → |B - A| = 1
  BC = 1                    → |C - B| = 1    （决策层级 3）
  CA = 1                    → |A - C| = 1    （决策层级 4）
  ∠C = 90°                  → AC ⊥ BC        （决策层级 5）
```

等边三角形 ABC 要求每个内角 = 60°，但约束声明了 ∠C = 90°。传播后域变空。

**学到的 nogood**：
```
¬⟦AB = 1⟧ ∨ ¬⟦BC = 1⟧ ∨ ¬⟦CA = 1⟧ ∨ ¬⟦∠C = 90°⟧
```

这是一个通用的三角形知识：不存在等边直角三角形。

---

## 7. 实现路线图

### 7.1 第一阶段：可解释传播器基础设施（P2-1）

- [ ] 定义 `ConstraintReason` 数据结构
  - reason 的创建、销毁、序列化
  - reason 的前提文字列表管理
  - reason 的决策层级标注
- [ ] 实现 `constraint_graph_restrict_domain()` 带 reason 参数的版本
  - 每次域约简自动记录 reason
  - reason 链的完整性维护
- [ ] 为三个核心几何约束实现可解释传播器
  - `collinear_propagate()` — 共线性传播
  - `distance_propagate()` — 等距传播（线段等长）
  - `angle_propagate()` — 等角传播
- [ ] 编写传播器的单元测试（含预期冲突场景）

### 7.2 第二阶段：冲突分析与 Nogood 学习（P2-2）

- [ ] 实现蕴含图构建器 `build_implication_graph()`
  - 从冲突 reason 出发反向遍历前提链
  - 检测循环引用（防御性编程）
- [ ] 实现 1-UIP 查找算法 `find_first_uip()`
  - 决策层级逆序扫描
  - 切割验证
- [ ] 实现 nogood 生成器 `collect_uip_cut_literals()`
  - 从 1-UIP 切割提取文字
  - 生成 nogood 的人类可读描述
- [ ] 实现 `NogoodStore`（nogood 数据库）
  - 添加/查询/删除 nogood
  - 活跃度管理和淘汰策略

### 7.3 第三阶段：非时序回跳与搜索重启（P2-3）

- [ ] 实现 `constraint_graph_backjump()`
  - 撤销对应决策层级
  - 单元传播
- [ ] 实现 VSIDS 风格的决策启发式
  - 根据冲突参与频率更新变量活跃度
  - 选择最活跃的未定变量进行分支
- [ ] 实现搜索重启策略
  - Luby 序列重启
  - 基于 nogood 数量的自适应重启
- [ ] 编写端到端冲突分析集成测试

### 7.4 第四阶段：双层约束系统桥接（P2-4）

- [ ] 实现 `DualLayerConstraintSystem`
  - 符号约束层 → 布尔骨架层的文字映射
  - 桥接表管理（创建、查询、删除）
- [ ] 实现 `dual_layer_sync_downward()` 和 `dual_layer_translate_nogood()`
- [ ] 集成轻量级 SAT 求解器后端（MiniSat 或 CryptoMiniSat 命令行调用）
- [ ] 实现双层回跳的一致性保证
  - 符号层回跳 → 同步更新 SAT 层的赋值

### 7.5 第五阶段：几何约束传播器的完整覆盖（P2-5）

- [ ] 扩展可解释传播器到所有 Lv-00 几何约束类型
  - 平行约束：`parallel(L1, L2)` 传播器
  - 垂直约束：`perpendicular(L1, L2)` 传播器
  - 共圆约束：`concyclic(A, B, C, D)` 传播器
  - 相似约束：`similar(T1, T2)` 传播器
  - 相切约束：`tangent(C, L)` 传播器
- [ ] 实现传播调度器 `PropagatorScheduler`
  - 优先级队列（最"紧"的约束优先传播）
  - 传播次数统计和效率跟踪
- [ ] 性能基准测试和传播器优化

---

## 8. 设计决策与权衡

### 8.1 几何约束的特殊性：超越整数域的传播

Chuffed 的原生 FD 传播器面向的是整数域（`IntVar`），而 Lv-00 的几何约束面向的是**有理数域和代数数域**。这引入了几个关键差异：

| 方面 | Chuffed FD | Lv-00 几何约束 |
|:---|:---|:---|
| 变量域 | 整数有限集合 {0..n} | 有理数无限集合，代数数域 |
| 域约简方式 | 排除具体整数值 | 方程约束下的域限缩 |
| 传播完备性 | 通常可达域一致性 | 多项式方程组的完全求解不可判定 |
| 冲突判定 | 域变空 | 方程组无解（或解不满足额外约束） |

缓解策略：
- 对于线性几何约束（如共线性、平行），使用高斯消元获取精确闭式解
- 对于二次约束（如等距、等角），使用 Groebner 基进行消元
- 对于无法精确判定的约束，回退到"延迟判定"策略（保留约束但不强制传播）

### 8.2 符号传播的计算成本

与 Chuffed 的整数传播（O(1) ∼ O(n) 的整数运算）不同，Lv-00 的符号传播涉及多项式代数运算：

- **线性约束传播**：O(n^3) 矩阵消元（与点数相关的规模）
- **二次约束传播**：O(d^n) Groebner 基计算（d 为多项式次数，n 为变量数）

缓解策略：
- **惰性传播**：仅在搜索决策后才传播（不每次变量变化都传播全部约束）
- **传播缓存**：缓存已计算的传播结果，仅在输入变化时重新计算
- **增量 Groebner 基**：使用 F5 算法的增量变体，避免每次从头计算
- **nogood 驱动的剪枝**：学到的 nogood 可以避免进入无解搜索分支，相当于用"蛮力学习"弥补"传播不完整"

### 8.3 Chuffed 不提供的 Lv-00 独有挑战

- **符号值的无限精度**：Chuffed 的域是有限集合，可以用位图高效表示。Lv-00 的符号域是无界的，需要不等式链（上界+下界）表示。
- **几何约束的全局语义**：Chuffed 的约束通常是局部的。Lv-00 的几何约束可能有全局效果（如改变一个点的坐标可能导致整个图形的拓扑变化）。
- **证明步骤的完整性追踪**：Lv-00 不仅需要传播，还需要每个传播步骤可被证明步骤引用（对应 `ProofStep` 系统的 `justification` 字段）。

---

## 9. 参考资源

- **Chuffed GitHub 仓库**：https://github.com/chuffed/chuffed
- **Chuffed 官方文档**：https://github.com/chuffed/chuffed/blob/master/doc/basic.md
- **核心论文 — LCG 架构**：
  - Ohrimenko, O., Stuckey, P. J., & Codish, M. (2009). "Propagation via lazy clause generation." *Constraints*, 14(3), 357–391.
  - Feydy, T., & Stuckey, P. J. (2009). "Lazy Clause Generation Reengineered." *CP 2009*, LNCS 5732, 352–366.
- **CDCL 冲突分析参考**：
  - Marques-Silva, J., Lynce, I., & Malik, S. (2021). "Conflict-Driven Clause Learning SAT Solvers." In *Handbook of Satisfiability* (2nd ed.), IOS Press.
- **MiniZinc Challenge 结果**（Chuffed FD 赛道表现）：https://www.minizinc.org/challenge/
- **Chuffed 传播器源码参考**：
  - 传播器基类：https://github.com/chuffed/chuffed/blob/master/chuffed/branching/branching.h
  - 解释机制：https://github.com/chuffed/chuffed/blob/master/chuffed/core/engine.h
