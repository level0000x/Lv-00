# Alloy 关系逻辑模型查找器核心借鉴设计

> **借鉴项目**：Alloy（github.com/AlloyTools/org.alloytools.alloy）
> **核心借鉴点**：关系即一切（Relation-as-Everything）的统一建模范式、有限范围假设（Small Scope Hypothesis）、Kodkod 关系逻辑→SAT 编码、交互式反例可视化、模块化规约体系、增量分析工作流
> **分类**：P1 高优先级 / 关系建模与约束求解
> **日期**：2026-05-24

---

## 1. 概述

Alloy 是由 MIT 软件设计小组在 Daniel Jackson 教授领导下开发的轻量级形式化方法工具。它是一种基于**关系逻辑**（Relational Logic）的声明式建模语言和分析器，核心理念是"一切皆为关系"（Everything is a Relation）——将系统建模为原子集合以及原子间的关系，并使用一阶关系逻辑描述系统约束。Alloy Analyzer 通过其 SAT 编码引擎 Kodkod，在用户指定的有限范围内穷举搜索满足约束的所有实例（模型），或寻找违反断言的**反例**。

Alloy 在软件工程界的影响力极为深远。它被广泛用于以下场景：

- **软件设计验证**：在设计阶段建模数据结构、API 契约和组件交互，在有限范围内穷举检查设计缺陷。Jackson 的"小范围假设"（Small Scope Hypothesis）声称：大多数设计缺陷在很小的范围内（3-6 个对象）就能暴露，因此有限范围搜索在实践中极为有效。
- **安全协议分析**：建模安全协议的消息传递、认证和授权流程，自动发现安全漏洞。
- **课程教学**：在 MIT、CMU 等顶尖院校的形式化方法课程中，Alloy 是核心教学工具，因为其关系逻辑比一阶逻辑/时序逻辑更直观，且可视化反例大大降低了形式化方法的学习曲线。
- **遗留系统逆向**：通过建模遗留系统的数据结构和业务规则，自动发现不一致性和未文档化的约束。

Alloy 工具链的核心组件包括：

1. **Alloy 语言**：声明式建模语言，基于一阶关系逻辑。核心构造包括 `sig`（签名/类型原子集）、`field`（关系/关联）、`fact`（全局约束/公理）、`pred`（可参数化谓词）、`assert`（断言/待验证命题）和 `fun`（可复用函数）。
2. **Alloy Analyzer**：集成分析环境，提供可视化实例浏览、反例追踪、元模型导出等功能。
3. **Kodkod**：关系模型查找引擎，将 Alloy 规约转化为 SAT 问题，是 Alloy 的"心脏"。Kodkod 将关系逻辑公式编译为命题逻辑合取范式（CNF），再交由 SAT 求解器（如 MiniSat、Glucose）求解。
4. **Alloy*（Alloy 扩展）**：对 Alloy 的高阶量化扩展，支持更丰富的规约表达能力。

Lv-00 是一个面向几何证明的交互式构造与验证系统。几何构造中的约束图本质上是一种**关系的图形化表示**——点与点之间的距离关系、角度关系、共线关系、共圆关系等都可以自然地建模为 Alloy 风格的关系。将约束图映射为关系逻辑，再将关系逻辑编译为 SAT 问题，就形成了一条从几何构造到 SAT 求解的完整形式化路径。这一路径与 Alloy→Kodkod→SAT 的路径在架构上高度同构。

以下将从六个核心维度详细阐述 Alloy 对 Lv-00 的借鉴映射。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 "关系即一切"的统一建模范式 → Lv-00 约束图的关系化重构

Alloy 最核心的设计哲学是"一切皆为关系"（Everything is a Relation）。在 Alloy 中：

- 标量 `x: A` 被建模为从单例集合到 A 的偏函数关系
- 集合 `s: set A` 被建模为从 A 到自身的子集关系
- 二元关联 `r: A -> B` 直接就是关系
- 多元关联 `r: A -> B -> C` 是多元关系（笛卡尔积的子集）

这一统一的建模范式避免了传统形式化方法中"集合、函数、元组、列表"等多种数据结构的语义鸿沟，使得模型转换和组合变得自然。

在 Lv-00 中，几何构造的约束图天然适合"关系化重构"——将约束图中的所有实体统一建模为 Alloy 风格的关系：

| Alloy 关系概念 | 数学定义 | Lv-00 约束图映射 | 实体类型 |
|:---|:---|:---|:---|
| `sig Point` | 点原子集合 | `PointSet` —— 构造中所有几何点的集合 | `TYPE_KIND_POINT` |
| `sig Line` | 线原子集合 | `LineSet` —— 所有衍生直线的集合 | `TYPE_KIND_LINE` |
| `sig Circle` | 圆原子集合 | `CircleSet` —— 所有衍生圆的集合 | `TYPE_KIND_CIRCLE` |
| `field collinear: Point -> Point -> Point` | 三点共线的三元关系 | `ConstraintEdge(CONSTRAINT_COLLINEAR, p1, p2, p3)` | 约束类型 |
| `field concyclic: set Point` | 四点共圆的关系（四元→集合表示） | `ConstraintEdge(CONSTRAINT_CONCYCLIC, p1, p2, p3, p4)` | 约束类型 |
| `field on_segment: Point -> Segment` | 点在线上关系 | `ConstraintEdge(CONSTRAINT_ON_SEGMENT, point, seg)` | 约束类型 |
| `field dist_eq: Point -> Point -> Point -> Point` | 距离等值（四元关系） | `ConstraintEdge(CONSTRAINT_DIST_EQ, a, b, c, d)` | 约束类型 |
| `fact no_self_loop` | 全局公理（所有状态下成立） | 构造不变量 | `GeoInvariant` |
| `pred construct_triangle` | 参数化谓词（可复用构造模板） | 可复用构造块 | `FuncBlock` |

#### 关系化约束图模型的形式化定义

在 Alloy 中建模几何构造的一个典型规约如下：

```alloy
-- Alloy 模型：几何构造的关系化描述
module geometry

-- 基本签名（原子类型）
sig Point {}
sig Segment {
    from, to: one Point
}
sig Circle {
    center: one Point,
    radius_point: one Point  -- 圆上一点，定义半径
}

-- 关系：共线性（三点）
pred collinear[p1, p2, p3: Point] {
    -- 三点共线：在 Alloy 中通过几何约束图编码
    -- 对应的 Lv-00 约束：constraint_add_collinear(graph, p1, p2, p3)
}

-- 关系：四点共圆
pred concyclic[p1, p2, p3, p4: Point] {
    -- 四点在同一圆上
}

-- 事实（全局公理）：每个线段连接两个不同的点
fact segment_connects_distinct_points {
    all s: Segment | s.from != s.to
}

-- 断言：三角形的三边长度满足三角不等式
assert triangle_inequality {
    all a, b, c: Point, ab, bc, ca: Segment |
        ab.from = a and ab.to = b and
        bc.from = b and bc.to = c and
        ca.from = c and ca.to = a
        => distance[ab] + distance[bc] >= distance[ca]
}

-- 检查断言（在小范围内寻找反例）
check triangle_inequality for 5 Point, 3 Segment
```

这种关系化描述与 Lv-00 的约束图模型具有结构同构性，为 Lv-00 提供了一条从关系规约到约束求解的直接路径。

| Alloy 构造 | 语义作用 | Lv-00 对应 | C 结构 |
|:---|:---|:---|:---|
| `sig X { f: Y }` | 声明原子集 X 及其关联 Y | 约束图中的节点类型声明 | `NodeId` + `ConstraintGraph` |
| `fact { F }` | 全局约束，所有实例必须满足 | 构造不变量 | `GeoInvariant` |
| `pred P[x: X] { F }` | 可复用约束谓词 | 构造模板 / 可复用约束块 | `FuncBlock` |
| `assert A { F }` | 待验证断言 | 证明目标 / 待证命题 | `ProofGoal` |
| `run P for N` | 在 N 个原子范围内搜索 P 的实例 | 在限定构造步数内搜索构造实例 | `geo_model_check()` |
| `check A for N` | 在 N 个原子范围内寻找 A 的反例 | 在限定构造步数内寻找反例 | `geo_model_check()` |
| `fun F[x: X]: Y { expr }` | 返回关系的可复用表达式 | 可复用的几何计算函数 | `geo_compute_fn` |
| `all x: X | F` | 全称量化（所有 x 满足 F） | 约束图全称遍历 | `graph_forall_match()` |
| `some x: X | F` | 存在量化（存在 x 满足 F） | 约束图存在搜索 | `graph_exists_match()` |
| `no x: X | F` | 无 x 满足 F | 约束图否定搜索 | `graph_none_match()` |

### 2.2 有限范围假设（Small Scope Hypothesis）→ Lv-00 小规模定理快速验证

Alloy 方法论的基石之一是"小范围假设"（Small Scope Hypothesis），由 Daniel Jackson 在其著作《Software Abstractions》中系统阐述：

> **小范围假设**：如果某个设计缺陷（或规约不一致）在足够大的范围内存在，那么它很可能在很小的范围内（通常不超过 6 个原子）就已经存在。因此，在有限的小范围内穷举搜索即可发现绝大多数设计缺陷。

这一假设已被大量实证研究验证。Jackson 分析了数百个实际案例，发现超过 90% 的设计缺陷在范围 3-6 之间就能被检测到。这不仅是一个方法论假设，更是使形式化验证在实际工程中可行的关键条件——因为无界验证在一般情况下是不可判定的。

在 Lv-00 的几何证明领域，小范围假设有自然的对应物——**小规模定理验证**：

| Small Scope Hypothesis | Lv-00 小规模定理验证 | 说明 |
|:---|:---|:---|
| 搜索范围限定在 N 个原子 | 构造步数限定在 K 步 | 限制构造复杂度 |
| N=3 ~ 6 能发现大多数缺陷 | K=10 ~ 30 能验证大多数几何定理 | 几何构造的类似界限 |
| 范围递增策略（scope monotonicity） | 步数递增验证策略 | 从 K=5 开始逐步增加 |
| unsat core 最小化 | 反例最小化 | 将长反例压缩为最短反例 |
| 对称破缺（symmetry breaking） | 对称性归约 | 减少搜索空间 |

```c
/**
 * @brief 小规模验证配置 —— 借鉴 Alloy 的 Small Scope Hypothesis
 *
 * 借鉴 Alloy 的范围（scope）概念，Lv-00 的"范围"是：
 *   - 自由点的最大数量（scope_points）
 *   - 构造步骤的最大数量（scope_steps）
 *   - 约束的最大数量（scope_constraints）
 *
 * 小范围假设在几何证明中的映射：
 *   如果一个几何定理存在反例，那么该反例很可能只涉及
 *   少量点和少量构造步骤。
 */
typedef struct {
    int     scope_points;           /**< 自由点数量上限（默认：6） */
    int     scope_steps;            /**< 构造步骤上限（默认：20） */
    int     scope_constraints;      /**< 约束数量上限（默认：50） */
    bool    scope_incremental;      /**< 是否启用范围递增策略 */
    int     scope_increment;        /**< 每次递增的步长（默认：1） */
    int     scope_max_points;       /**< 范围递增的最终上限（默认：12） */
} SmallScopeConfig;

/**
 * @brief 小规模定理验证 —— Alloy 风格的有限范围穷举搜索
 *
 * 借鉴 Alloy Analyzer 的 check 命令：
 *   check theorem_name for N Point, M Step
 *
 * 在给定的有限构造范围内，穷举搜索所有可能的构造路径，
 * 检查在每个构造结果上定理是否成立。
 *
 * @param theorem      待验证的几何定理
 * @param scope_cfg    小范围配置
 * @param out_counter  输出：发现的反例（若存在）
 * @return true 如果在给定范围内未发现反例
 *
 * @note 小范围验证通过的定理不能保证在大范围内一定成立，
 *       但可以在实践中提供高置信度的验证结果。
 *       这是 Alloy Small Scope Hypothesis 的核心理念。
 */
bool geo_small_scope_verify(
    ProofTheorem        *theorem,
    SmallScopeConfig    *scope_cfg,
    CounterExample      *out_counter
);
```

### 2.3 Kodkod 的关系逻辑 → SAT 编码 → Lv-00 constraint_graph → SAT 转换

Kodkod 是 Alloy 的关系模型查找引擎，其核心功能是将用一阶关系逻辑描述的 Alloy 规约转化为 SAT（布尔可满足性）问题。这一转化过程是 Alloy 能够自动搜索模型和反例的技术基石。

Kodkod 的编译流程：

```
Alloy 规约（sig/field/fact/pred/assert）
    → Alloy 解析器（构建 AST）
    → 类型检查与作用域解析
    → 将规约翻译为一阶关系逻辑公式
    → 在给定范围内展开量词和关系（universe bound）
    → Kodkod：关系逻辑 → 命题逻辑 CNF 编码
    → SAT 求解器（MiniSat / Glucose / Z3）
    → SAT 结果解码为 Alloy 实例或反例
```

在 Lv-00 中，约束图（constraint_graph）同样可以被编码为 SAT 问题。这种编码不是从"关系逻辑"出发，而是从"几何约束关系图"出发，但其编码思路与 Kodkod 高度相似。

| Kodkod 编码步骤 | 技术细节 | Lv-00 对应 | 实现模块 |
|:---|:---|:---|:---|
| 关系变量编码 | 将每个关系元组映射为布尔变量 | 将每个约束边的满足状态映射为布尔变量 | `constraint_to_bool_var()` |
| 关系约束编码 | 将量词/连接词转化为子句 | 将几何约束逻辑转化为 CNF 子句 | `geo_constraint_to_clause()` |
| 基数约束编码 | `#r = n` → 基数电路 → CNF | 度数约束（如恰好 2 个平行约束） | `degree_constraint_encode()` |
| 范围限定 | 展开全称量词 `all x: X` 为合取 | 展开所有可能的节点绑定 | `universe_node_expand()` |
| 对称破缺 | 添加额外的对称破缺子句 | 顶点序号规范化 | `symmetry_break_clauses()` |
| SAT 求解 | 调用 SAT 求解器 | 调用 MiniSat / CaDiCaL | `sat_solve()` |
| 解码 | 将 SAT 赋值映射回关系元组 | 将 SAT 赋值映射回约束图状态 | `sat_model_to_graph()` |

#### 约束图 → SAT 编码的核心框架

```c
/**
 * @brief SAT 变量映射 —— Kodkod 风格的关系→布尔编码
 *
 * 将约束图中的每一对节点关系映射为 SAT 布尔变量。
 * 借鉴 Kodkod 的 "关系→布尔矩阵" 编码方案。
 */
typedef struct {
    ConstraintGraph  *source_graph;     /**< 源约束图 */
    int               total_vars;       /**< SAT 变量总数 */
    int               total_clauses;    /**< SAT 子句总数 */

    /**
     * var_map[i] = SAT 变量编号（1-based）
     * 映射表结构：
     *   - 对于类型约束：每个 (node, type) 对 → 1 个布尔变量
     *   - 对于二元关系：每个 (src, dst) 对 → 1 个布尔变量
     *   - 对于 N 元关系：每个 N 元组 → 1 个布尔变量（基数可能爆炸）
     */
    int              *var_map;
    int               var_map_size;

    /**
     * 反向映射：SAT 变量 → (node_id0, node_id1, ..., constraint_type)
     */
    int             **reverse_map;      /**< reverse_map[var] = [约束类型, n0, n1, ...] */

    /**
     * 子句缓冲区（增量构建 CNF）
     */
    int             **clause_buffer;    /**< 子句数组（每行一个子句，0 终止） */
    int               clause_buffer_cap;
    int               clause_buffer_count;
} SatEncoding;

/**
 * @brief 约束图 → SAT 编码器
 *
 * 借鉴 Kodkod 的编译管道，将整个约束图编码为 SAT 问题。
 *
 * 编码步骤：
 *   1. 遍历约束图，为每个节点-类型和每条约束边分配 SAT 变量
 *   2. 编码"类型互斥"子句：每个节点恰好有一个类型
 *   3. 编码几何约束子句：coordinate_eq → constraint_edge
 *   4. 编码传递性子句：parallel(a,b) /\ parallel(b,c) → parallel(a,c)
 *   5. 编码基数约束子句：度数上限等
 *
 * @param graph        源约束图
 * @param scope_cfg    小范围配置（限定节点和约束的数量）
 * @param out_encoding 输出：完成的 SAT 编码
 * @return true 如果编码成功
 */
bool constraint_graph_to_sat(
    ConstraintGraph      *graph,
    SmallScopeConfig     *scope_cfg,
    SatEncoding          *out_encoding
);

/**
 * @brief 对 SAT 编码求解并解码回约束图
 *
 * 借鉴 Alloy Analyzer 的"求解→解码→可视化"工作流：
 *   1. 将 SAT 编码写入 DIMACS 文件或直接调用求解器 API
 *   2. 执行 SAT 求解
 *   3. 如果 SATISFIABLE，将 SAT 模型解码为约束图实例
 *   4. 如果 UNSATISFIABLE，提取 unsat core（可选）
 *
 * @param encoding    SAT 编码
 * @param out_graph   输出：解码后的约束图实例（SATISFIABLE）或 NULL（UNSATISFIABLE）
 * @param out_core    输出：unsat core 子句列表（UNSATISFIABLE 时）
 * @return SAT_RESULT_SAT / SAT_RESULT_UNSAT / SAT_RESULT_UNKNOWN
 */
SatResult sat_solve_and_decode(
    SatEncoding          *encoding,
    ConstraintGraph     **out_graph,
    int                 **out_core
);
```

#### 几何约束 → CNF 子句的具体编码规则

```c
/**
 * @brief 几何约束到 CNF 子句的编码规则示例
 *
 * 本节展示几种典型几何约束的 SAT 编码。
 * 借鉴 Kodkod 的关系逻辑编码方案，但针对几何领域做了特化。
 */

/**
 * 共线约束 collinear(a, b, c) 的 SAT 编码：
 *
 * 令布尔变量：
 *   V_col(a,b,c) = 1 表示 a,b,c 共线
 *   V_on_line(a, line_L) = 1 表示点 a 在线 L 上
 *
 * 编码规则：
 *   1. V_col(a,b,c) → (∃ L: V_on_line(a,L) ∧ V_on_line(b,L) ∧ V_on_line(c,L))
 *      展开为 CNF 子句（在有限范围内枚举所有可能的 L）
 *
 *   2. (V_on_line(a,L) ∧ V_on_line(b,L) ∧ V_on_line(c,L)) → V_col(a,b,c)
 *
 * 平行约束 parallel(seg_ab, seg_cd) 的 SAT 编码：
 *
 * 令布尔变量：
 *   V_parallel(ab, cd) = 1 表示线段 ab 和 cd 平行
 *   V_slope_eq(ab, cd) = 1 表示斜率相等
 *
 * 编码规则：
 *   1. V_parallel(ab, cd) ↔ V_slope_eq(ab, cd)
 *   2. V_slope_eq(ab, cd) ↔ (V_slope_x(ab) * V_slope_y(cd) = V_slope_x(cd) * V_slope_y(ab))
 *      斜率等式的乘性编码需要额外的辅助变量...
 */

/**
 * @brief 通用几何约束编码器
 *
 * 将一条 ConstraintEdge 转换为 CNF 子句集合并追加到 SatEncoding 中。
 *
 * @param encoding    目标 SAT 编码
 * @param edge        要编码的约束边
 * @return 编码的子句数量，-1 表示编码失败
 */
int geo_constraint_encode_clause(
    SatEncoding       *encoding,
    const ConstraintEdge *edge
);
```

### 2.4 Alloy Analyzer 的交互式反例可视化 → Lv-00 证明反例可视化

Alloy Analyzer 最具吸引力的特性之一是其**交互式反例可视化**。当用户执行 `check` 命令发现反例时，Alloy Analyzer 不仅报告"断言不成立"，还会展示一个可视化的反例实例——用户可以交互式地浏览反例中的每个原子和关系，直观理解为什么断言被违反。

这一交互式可视化能力对 Lv-00 的几何证明系统同样至关重要。几何证明中的反例天然具有可视化特征——一个不满足某些几何属性的点构造，可以在画布上直接渲染出来。

| Alloy Analyzer 功能 | Lv-00 反例可视化 | 实现方式 |
|:---|:---|:---|
| 实例浏览（Instance Browser） | 反例几何图渲染 | SVG/Canvas 渲染几何构造图 |
| 主题定制（Theme） | 构造显示选项 | 颜色/线型/标签的定制 |
| 投影（Projection） | 几何变换投影 | 旋转/缩放/平移反例图 |
| 魔术布局（Magic Layout） | 自动布局 | 约束图自动布局算法 |
| 评估器（Evaluator） | 表达式求值 | 在反例上求值指定表达式 |
| XML 输出 | JSON 反例导出 | 反例数据序列化为 JSON |
| 增量分析 | 迭代检查 | 修改约束后重新检查（仅重检变化部分） |

```c
/**
 * @brief 反例可视化元素 —— 借鉴 Alloy 的 Instance Browser
 *
 * 将反例（一组不满足定理的几何构造状态）分解为可渲染的
 * 几何元素列表。借鉴 Alloy Analyzer 将关系实例可视化为
 * 节点-边图的方式。
 */
typedef enum {
    VIZ_ELEM_POINT,             /**< 几何点（带坐标） */
    VIZ_ELEM_SEGMENT,           /**< 线段 */
    VIZ_ELEM_CIRCLE,            /**< 圆 */
    VIZ_ELEM_ANGLE_ARC,         /**< 角度弧 */
    VIZ_ELEM_HIGHLIGHTED,       /**< 高亮的违反区域 */
    VIZ_ELEM_LABEL              /**< 文字标签 */
} VizElementType;

typedef struct {
    VizElementType   type;
    float            x, y;              /**< 屏幕坐标（已布局） */
    float            x2, y2;            /**< 端点（线段/弧） */
    float            radius;            /**< 半径（圆） */
    int              node_ids[4];       /**< 原始约束图节点 ID */
    int              node_count;
    char            *label;             /**< 显示标签 */
    bool             is_violation;      /**< 是否为违反区域（红色高亮） */
} VizElement;

/**
 * @brief 反例可视化数据
 *
 * 从一个 CounterExample 中提取可渲染的几何元素。
 * 借鉴 Alloy Analyzer 的实例可视化流程。
 */
typedef struct {
    CounterExample  *source;            /**< 源反例 */
    VizElement      *elements;          /**< 可视化元素列表 */
    int              element_count;
    int              violation_step;    /**< 违反发生时的构造步骤 */
    char            *violation_desc;    /**< 违反的几何属性描述 */
} CounterExampleVisualization;

/**
 * @brief 从反例生成可视化数据
 *
 * 将 CounterExample 中的几何状态序列转化为 2D 可视化数据。
 * 使用符号坐标求值器将符号坐标转换为数值坐标。
 *
 * @param counter    反例对象
 * @param theme      可视化主题（颜色、线型等）
 * @return 可视化数据对象，调用者负责释放
 */
CounterExampleVisualization *counterexample_visualize(
    CounterExample      *counter,
    VisualizationTheme  *theme
);
```

### 2.5 模块化规约（module/sig/fact/pred/assert）→ Lv-00 公理包组织

Alloy 的模块化规约体系是其工程化能力的关键支柱。一个 Alloy 模型可以按层次组织为多个模块（`module`），每个模块包含签名（`sig`）、事实（`fact`）、谓词（`pred`）和断言（`assert`），并通过 `open` 语句导入其他模块。

在 Lv-00 中，公理包（Axiom Package）系统正是借鉴了这一模块化设计，将不同的数学/几何公理体系组织为独立、可组合的公理包。

| Alloy 模块化概念 | Lv-00 公理包映射 | 文件/结构 |
|:---|:---|:---|
| `module name` | 公理包标识符 | `axiom_packages/*.lvz` |
| `open module` | 公理依赖声明 | `manifest.json → dependencies` |
| `sig X { ... }` | 类型声明（点/线/圆等几何类型） | `#type Point / Line / Circle` |
| `fact { ... }` | 全局公理（所有构造实例满足的约束） | `#axiom` 块 |
| `pred P[x: X] { ... }` | 可复用构造模板 | `#template` 块 |
| `assert A { ... }` | 待证明的定理/引理 | `#theorem` 块 |
| `fun F[x: X]: Y { ... }` | 可复用的几何计算函数 | `#function` 块 |
| `check A for N` | 小范围验证命令 | `#verify` 块 |

#### 公理包模块化结构

```c
/**
 * @brief 公理包清单 —— 借鉴 Alloy 的模块系统
 *
 * 每个 .lvz 公理包对应一个 Alloy 风格的"模块"。
 * manifest.json 描述公理包的元数据和依赖关系。
 *
 * Alloy 模块结构：
 *   module euclidean_geometry
 *   open ordering  -- 导入依赖
 *   sig Point { x, y: Int }
 *   fact planar_axioms { ... }
 *
 * Lv-00 公理包结构：
 *   package euclidean_geometry
 *   depends: [ordering, metric_space]
 *   #type Point { ... }
 *   #axiom planar_axioms { ... }
 */
typedef struct {
    char            *package_name;      /**< 公理包名称（如 "euclidean_plane"） */
    char            *version;           /**< 版本号（如 "1.2.0"） */
    char            *description;       /**< 人类可读的描述 */
    char            *author;            /**< 作者 */
    char           **dependencies;      /**< 依赖的其他公理包名称列表 */
    int              dependency_count;

    /* Alloy sig 对应：类型声明 */
    TypeDecl        *type_decls;        /**< 自定义几何类型声明 */
    int              type_decl_count;

    /* Alloy fact 对应：全局公理 */
    ConstraintBlock *axioms;            /**< 公理约束块 */
    int              axiom_count;

    /* Alloy pred 对应：可复用构造模板 */
    FuncBlock       *templates;         /**< 构造模板 */
    int              template_count;

    /* Alloy assert 对应：待证定理 */
    ProofTheorem    *theorems;          /**< 定理列表 */
    int              theorem_count;

    /* Alloy fun 对应：几何计算函数 */
    ComputeFunc     *compute_funcs;     /**< 计算函数 */
    int              compute_func_count;
} AxiomPackage;

/**
 * @brief 公理依赖图 —— 借鉴 Alloy 的 open 语句
 *
 * 维护所有已加载公理包之间的依赖关系。
 * 拓扑排序用于确定加载和验证顺序。
 */
typedef struct {
    AxiomPackage   **packages;          /**< 所有已注册的公理包 */
    int              package_count;
    int            **adjacency;         /**< 邻接矩阵：adj[i][j] = 1 表示 i 依赖 j */
    int              matrix_size;
    int             *topo_order;        /**< 拓扑序（依赖在前） */
} AxiomDependencyGraph;

/**
 * @brief 加载公理包并解析其依赖
 *
 * 借鉴 Alloy 的模块加载流程：
 *   1. 读取 manifest.json
 *   2. 递归加载所有依赖
 *   3. 检查循环依赖
 *   4. 合并所有类型和公理声明
 *   5. 验证合并后的一致性
 *
 * @param dep_graph    依赖图（会被更新）
 * @param package_path 公理包文件路径
 * @return 加载后的公理包索引，-1 表示加载失败
 */
int axiom_package_load(
    AxiomDependencyGraph *dep_graph,
    const char           *package_path
);
```

### 2.6 增量分析（检查→修改→再检查工作流）→ Lv-00 迭代构造工作流

Alloy 的一个核心使用模式是**增量分析工作流**：

1. **建模**：用 sig/fact/pred 描述系统
2. **运行**（`run`）：生成满足约束的实例，验证模型是否"有解"
3. **检查**（`check`）：在有限范围内验证断言
4. **审查反例**：如果检查失败，分析反例
5. **修改模型**：修改约束或断言以消除反例或增强规约
6. **重复**：回到步骤 2

这一工作流在几何构造中同样适用。Lv-00 借鉴这一模式设计了**迭代构造工作流**：

```
            ┌─────────────┐
            │  建模阶段    │
            │  声明点/线/圆 │
            │  添加约束    │
            └──────┬──────┘
                   │
            ┌──────▼──────┐
            │  构造验证    │ ◄── "run"：生成可达构造
            │  检查一致性  │
            └──────┬──────┘
                   │
            ┌──────▼──────┐
            │  定理验证    │ ◄── "check"：穷举搜索反例
            │  小范围搜索  │
            └──────┬──────┘
                   │
          ┌────────▼────────┐
          │   反例分析       │
          │   可视化审查     │
          └────────┬────────┘
                   │
          ┌────────▼────────┐
          │   修改构造       │ ◄── 修改约束、添加新点/线
          │   新增/删除约束  │
          └────────┬────────┘
                   │
                   └────── 循环 ──────┘
```

```c
/**
 * @brief 迭代构造工作流状态 —— 借鉴 Alloy 的增量分析周期
 *
 * 维护构造迭代的全生命周期状态。
 * 每个迭代周期包括：建模 → 验证 → 分析 → 修改
 */
typedef enum {
    ITER_PHASE_MODELING,        /**< 建模阶段：编辑约束和构造 */
    ITER_PHASE_VERIFYING,       /**< 验证阶段：约束一致性检查 */
    ITER_PHASE_THEOREM_CHECK,   /**< 定理检查：小范围反例搜索 */
    ITER_PHASE_ANALYZING,       /**< 分析阶段：审查反例/结果 */
    ITER_PHASE_REVISING         /**< 修订阶段：修改约束 */
} IterationPhase;

typedef struct {
    int                  iteration_id;      /**< 当前迭代编号 */
    IterationPhase       phase;             /**< 当前迭代阶段 */
    GeoConstructionSpec *current_spec;      /**< 当前构造规约 */
    VerificationReport  *last_report;       /**< 上一个周期的验证报告 */
    CounterExample      *last_counter;      /**< 上一个周期发现的反例 */
    int                  iteration_count;   /**< 累积迭代次数 */
    double               total_time_secs;   /**< 累积耗时 */

    /* 增量分析状态 */
    bool                 is_dirty;          /**< 自上次检查后是否有修改 */
    int                 *changed_constraints;/**< 本次修改的约束 ID 列表 */
    int                  changed_count;
} IterationWorkflow;

/**
 * @brief 执行一次迭代验证周期
 *
 * 借鉴 Alloy Analyzer 的 run/check 交替使用模式。
 * 执行"检查→分析→（可选修改后）再检查"的一个完整周期。
 *
 * @param wf      工作流状态
 * @param scope   小范围配置
 * @return true 如果验证通过（未发现反例）
 *
 * @note 增量模式下，仅重新检查 changed_constraints 影响的范围，
 *       避免对不变的约束重复编码和求解。
 */
bool iteration_cycle_run(
    IterationWorkflow   *wf,
    SmallScopeConfig    *scope
);

/**
 * @brief 增量式重检 —— 仅检查发生变化的约束
 *
 * 借鉴 Alloy 的增量分析：用户修改模型后，不需要从头重新分析。
 * 仅对受影响的约束和定理进行重新检查。
 *
 * 实现策略：
 *   1. 维护约束依赖图
 *   2. 当约束 C 被修改时，标记所有直接和间接依赖 C 的定理为"脏"
 *   3. 仅重新验证脏的定理
 *
 * @param wf       工作流状态
 * @param changed  变化的约束 ID 列表
 * @param cnt      变化的约束数量
 */
void iteration_incremental_recheck(
    IterationWorkflow   *wf,
    int                 *changed,
    int                  cnt
);
```

---

## 3. Lv-00 映射方案（C 代码级详细设计）

### 3.1 总体架构：Alloy 三层映射

本节给出将 Alloy 方法论映射到 Lv-00 C 代码库的完整架构设计：

```
+------------------------------------------------------------------+
|                    第 3 层：交互与分析层                           |
|  counterexample_viz.h  |  反例可视化引擎                           |
|  iteration_workflow.h  |  迭代构造工作流                           |
|  axiom_pkg_loader.c    |  公理包加载与依赖解析                      |
+------------------------------------------------------------------+
|                    第 2 层：编码与求解层                           |
|  sat_encoding.h        |  约束图→SAT 编码器（Kodkod 映射）          |
|  sat_solver_bridge.c   |  SAT 求解器桥接（MiniSat/Glucose）        |
|  model_decode.c        |  SAT 模型→约束图解码                      |
+------------------------------------------------------------------+
|                    第 1 层：关系建模范式层                          |
|  relation_model.h      |  Alloy 风格的关系化约束图模型              |
|  constraint_relational.c |  几何约束的关系化表示                   |
|  scope_config.h        |  小范围配置与界定                          |
+------------------------------------------------------------------+
```

### 3.2 关系建模范式层：Alloy 风格的关系化约束图

```c
/**
 * @file relation_model.h
 * @brief Alloy 风格的"关系即一切"约束图模型
 *
 * 将几何约束图统一建模为关系代数。
 * 借鉴 Alloy 的核心理念：一切皆为关系。
 */

#ifndef LV00_RELATION_MODEL_H
#define LV00_RELATION_MODEL_H

#include "lv00/constraint_graph.h"
#include "lv00/type_system.h"

/* ================================================================
 * 原子类型（对应 Alloy 的 sig）
 * ================================================================ */

/**
 * @brief 几何原子（对应 Alloy 的 sig 原子）
 *
 * 在 Alloy 中，sig 定义了一组不可区分的原子。
 * 在 Lv-00 中，点/线/圆就是几何原子类型。
 */
typedef enum {
    ATOM_POINT,             /**< sig Point */
    ATOM_SEGMENT,           /**< sig Segment { from, to: Point } */
    ATOM_CIRCLE,            /**< sig Circle { center, radius: Point } */
    ATOM_TRIANGLE,          /**< sig Triangle { a, b, c: Point } */
    ATOM_QUADRILATERAL,     /**< sig Quadrilateral { ... } */
    ATOM_ANGLE,             /**< sig Angle { vertex, arm1, arm2: Point } */
    ATOM_TYPE_COUNT
} AtomKind;

/**
 * @brief 原子实例（对应 Alloy 中的一个原子）
 */
typedef struct {
    int         atom_id;            /**< 全局唯一原子 ID */
    AtomKind    kind;               /**< 原子类型 */
    char       *label;              /**< 用户标签 */
} Atom;

/**
 * @brief 关系（对应 Alloy 的 field）
 *
 * 在 Alloy 中，field 是从一个 sig 到其他 sig 的关系。
 * 例如：field from: Segment -> Point 表示从 Segment 到 Point 的关系。
 *
 * 在 Lv-00 中，关系被表示为约束图中的约束边。
 */
typedef enum {
    REL_UNARY,              /**< 一元关系：set Point（子集） */
    REL_BINARY,             /**< 二元关系：Point -> Point */
    REL_TERNARY,            /**< 三元关系：Point -> Point -> Point（如 collinear） */
    REL_QUATERNARY          /**< 四元关系：如 dist_eq */
} RelationArity;

typedef struct {
    int             relation_id;
    char           *name;                /**< 关系名称 */
    RelationArity   arity;
    AtomKind        domain_types[4];     /**< 每个参数位置的原子类型 */
    int             domain_count;
} Relation;

/* ================================================================
 * Alloy 核心构造的 C 映射
 * ================================================================ */

/**
 * @brief Alloy 的 run 命令 → Lv-00 的构造实例搜索
 *
 * Alloy:  run myPred for 5 Point
 * Lv-00:  geo_run_instance_search(&pred, scope_5_points);
 */
typedef struct {
    ConstraintBlock   *predicate;         /**< 要满足的约束谓词 */
    SmallScopeConfig   scope;             /**< 搜索范围 */
    int                max_instances;     /**< 最多返回多少个实例（Alloy 默认返回所有） */
    ConstraintGraph  **instances;         /**< 输出：找到的构造实例 */
    int                instance_count;
} RunCommand;

bool geo_run_instance_search(RunCommand *cmd);

/**
 * @brief Alloy 的 check 命令 → Lv-00 的反例搜索
 *
 * Alloy:  check myAssert for 5 Point
 * Lv-00:  geo_check_assertion(&assert, scope_5_points, &counter);
 */
typedef struct {
    ProofTheorem      *assertion;         /**< 要验证的断言 */
    SmallScopeConfig   scope;             /**< 搜索范围 */
    CounterExample    *counterexample;    /**< 输出：反例（如果存在） */
    bool               assertion_holds;   /**< 断言是否在范围内成立 */
} CheckCommand;

bool geo_check_assertion(CheckCommand *cmd);

/**
 * @brief Alloy 的 fact → Lv-00 的全局公理
 *
 * Alloy:  fact noSelfLoop { all p: Point | p != p.next }
 * Lv-00:  geo_fact_register(&no_self_loop_invariant);
 *
 * 公理（fact）在 Alloy 中是全局的——所有实例都必须满足。
 * 在 Lv-00 中，全局公理被实现为构造不变量（GeoInvariant），
 * 在模型检查的每一步都被验证。
 */
typedef struct {
    char           *name;                 /**< 公理名称（人类可读） */
    char           *description;
    GeoInvariant   *invariant;            /**< 底层不变式检查函数 */
    bool            is_soft;              /**< 是否为软约束（Alloy 没有，Lv-00 扩展） */
    int             priority;             /**< 检查优先级（0 最高） */
} GeoFact;

/**
 * @brief 注册全局公理
 *
 * 借鉴 Alloy 的 fact 机制。每个 fact 被自动添加到全局不变式列表中。
 * 所有后续的构造验证都会包含该 fact 作为前提。
 *
 * @param fact  要注册的公理（生命周期由调用者管理）
 * @return 公理的全局 ID
 */
int geo_fact_register(GeoFact *fact);

#endif /* LV00_RELATION_MODEL_H */
```

### 3.3 编码与求解层：约束图 → SAT

```c
/**
 * @file sat_encoding.h
 * @brief Kodkod 风格的约束图 → SAT 编码管道
 *
 * 本模块实现了将 Lv-00 约束图编译为 SAT 问题的完整管道。
 * 借鉴 Kodkod 的架构：
 *   Relation Logic → Bounds → CNF Translation → SAT Solver
 *
 * Lv-00 的对应：
 *   Constraint Graph → Small Scope → CNF Encoding → SAT Solver
 */

#ifndef LV00_SAT_ENCODING_H
#define LV00_SAT_ENCODING_H

#include "lv00/constraint_graph.h"
#include "lv00/relation_model.h"

/* ------ SAT 变量分配策略 ------ */

/**
 * @brief SAT 变量的命名方案
 *
 * 每个 SAT 变量对应约束图中的一个"事实"：
 *   - VAR_TYPE(node_id, atom_kind)：节点 node_id 是否是 atom_kind 类型
 *   - VAR_REL_BIN(relation_id, src, dst)：二元关系 (src,dst) 是否在关系中
 *   - VAR_REL_TERN(relation_id, a, b, c)：三元关系 (a,b,c) 是否在关系中
 *   - VAR_CONSTRAINT(constraint_id)：约束边是否被满足（活跃）
 */
typedef enum {
    VAR_CATEGORY_TYPE,          /**< 类型变量 */
    VAR_CATEGORY_REL_BIN,       /**< 二元关系变量 */
    VAR_CATEGORY_REL_TERN,      /**< 三元关系变量 */
    VAR_CATEGORY_REL_QUAT,      /**< 四元关系变量 */
    VAR_CATEGORY_CONSTRAINT,    /**< 约束满足变量 */
    VAR_CATEGORY_AUX            /**< 辅助变量（Tseitin 编码引入） */
} SatVarCategory;

/**
 * @brief 分配一个新的 SAT 变量
 *
 * 借鉴 Kodkod 的变量分配方案。
 * 维护变量到约束图实体的双向映射。
 *
 * @param enc       SAT 编码上下文
 * @param category  变量类别
 * @param args      类别相关参数（如 node_id, relation_id）
 * @param arg_count 参数数量
 * @return 新分配的 SAT 变量编号（1-based），0 表示分配失败
 */
int sat_var_alloc(
    SatEncoding     *enc,
    SatVarCategory   category,
    int             *args,
    int              arg_count
);

/* ------ CNF 子句构建器 ------ */

/**
 * @brief 向 CNF 缓冲区追加一条子句
 *
 * 借鉴 Kodkod 的增量 CNF 构建：
 *   子句格式：[lit1, lit2, ..., litN, 0]
 *   正文字面量 = SAT 变量编号
 *   负文字面量 = -SAT 变量编号
 *   终止符 = 0
 *
 * @param enc     SAT 编码上下文
 * @param lits    文字面量数组（以 -var 表示否定，以 0 终止）
 * @return 添加的子句在缓冲区中的索引
 */
int sat_add_clause(SatEncoding *enc, int *lits);

/**
 * @brief 便捷宏：向 CNF 添加单元子句（强制某变量为真/假）
 */
#define sat_add_unit(enc, lit) \
    do { int __tmp[] = { (lit), 0 }; sat_add_clause((enc), __tmp); } while(0)

/**
 * @brief 便捷宏：向 CNF 添加二元子句（A 或 B）
 */
#define sat_add_binary(enc, a, b) \
    do { int __tmp[] = { (a), (b), 0 }; sat_add_clause((enc), __tmp); } while(0)

/* ------ 约束→子句编码 ------ */

/**
 * @brief 编码：每个节点恰好有一个几何类型
 *
 * 对应 Alloy 中的 sig 语义：每个原子恰好属于一个 sig。
 * 在 Lv-00 中：每个节点恰好有一个 TYPE_KIND。
 *
 * 编码规则（针对每个 node）：
 *   (V_type(node,0) \/ V_type(node,1) \/ ... \/ V_type(node,K-1))  -- 至少一个类型
 *   对每对 (i,j) i≠j: (~V_type(node,i) \/ ~V_type(node,j))          -- 至多一个类型
 */
int sat_encode_type_exclusivity(SatEncoding *enc, ConstraintGraph *graph);

/**
 * @brief 编码：二元几何关系的传递性
 *
 * 例如 parallel(a,b) ∧ parallel(b,c) → parallel(a,c)
 */
int sat_encode_transitivity(SatEncoding *enc, Relation *rel);

/**
 * @brief 编码：距离等值约束
 *
 * dist_eq(a,b,c,d) → distance(a,b) = distance(c,d)
 *
 * 距离等值被编码为 SAT 子句的合取：
 *   对于每对可能的 (a,b) 和 (c,d)：
 *     如果符号坐标代数表明它们不可能等距 → 添加禁止子句
 *     如果符号坐标代数表明它们必然等距 → 添加强制单元子句
 *     否则 → 添加可变子句（SAT 求解器自行决定）
 */
int sat_encode_distance_equality(
    SatEncoding         *enc,
    ConstraintGraph     *graph,
    const ConstraintEdge *edge
);

/* ------ 求解器桥接 ------ */

/**
 * @brief SAT 求解器抽象接口
 *
 * 借鉴 Kodkod 的多求解器支持架构。
 * 通过统一接口支持多种 SAT 求解器后端。
 */
typedef struct {
    /** 求解器初始化，设置变量数量 */
    bool (*init)(void *solver_state, int num_vars);

    /** 添加子句，lits 以 0 终止 */
    bool (*add_clause)(void *solver_state, int *lits);

    /** 求解，返回 true=SAT, false=UNSAT */
    bool (*solve)(void *solver_state);

    /** 获取变量的赋值：返回 true(正)/false(负) */
    bool (*get_value)(void *solver_state, int var);

    /** 释放求解器资源 */
    void (*destroy)(void *solver_state);

    void *solver_state;
} SatSolver;

/**
 * @brief 从 SAT 编码创建求解器并求解
 */
SatResult sat_solve_encoding(SatEncoding *enc, SatSolver *solver, ConstraintGraph **out_model);

#endif /* LV00_SAT_ENCODING_H */
```

---

## 4. 实现路线图

### 阶段 I：关系化模型基础（第 1-2 周）

**目标**：完成 Alloy 风格的"关系即一切"约束图建模基础。

- [ ] 实现 `Atom`、`Relation`、`RelationArity` 等基础数据结构
- [ ] 实现几何实体类型到 Alloy sig 的映射表
- [ ] 实现 `GeoFact` 全局公理注册机制
- [ ] 实现 `RunCommand` / `CheckCommand` 的查询接口
- [ ] 编写单元测试：验证关系模型的类型互斥性和基数约束

**交付物**：`relation_model.h`、`relation_model.c` 及对应测试。

### 阶段 II：SAT 编码管道（第 3-5 周）

**目标**：完成 Kodkod 风格的约束图 → SAT 转换管道。

- [ ] 实现 `SatEncoding` 结构和 `sat_var_alloc()` 变量分配
- [ ] 实现 `constraint_graph_to_sat()` —— 主编码入口
- [ ] 实现各几何约束的编码规则（共线、平行、垂直、等距、等角）
- [ ] 实现传递性编码和类型互斥编码
- [ ] 实现 SAT 求解器后端桥接（MiniSat）
- [ ] 实现 `sat_solve_and_decode()` —— 求解结果解码
- [ ] 编写端到端测试：小规模几何命题的 SAT 编码和求解

**交付物**：`sat_encoding.h`、`sat_encoding.c`、`sat_solver_bridge.c` 及测试套件。

### 阶段 III：可视化与工作流（第 6-7 周）

**目标**：完成 Alloy 风格的交互式反例可视化和迭代工作流。

- [ ] 实现 `CounterExampleVisualization` 反例可视化数据结构
- [ ] 实现几何反例的 SVG 渲染
- [ ] 实现 `IterationWorkflow` 迭代构造状态机
- [ ] 实现增量重检 `iteration_incremental_recheck()`
- [ ] 集成前端：将反例可视化嵌入 Tauri WebView

**交付物**：`counterexample_viz.h`、`counterexample_viz.c`、`iteration_workflow.h`、`iteration_workflow.c`。

### 阶段 IV：公理包与集成（第 8-9 周）

**目标**：完成模块化公理包系统和端到端集成。

- [ ] 实现 `AxiomPackage` 公理包数据结构
- [ ] 实现 `axiom_package_load()` 加载器和依赖解析
- [ ] 实现公理包依赖图的拓扑排序和循环依赖检测
- [ ] 将 `relation_model` + `sat_encoding` + 现有 `constraint_graph` 集成
- [ ] 端到端集成测试：欧几里得公理包的完整加载和验证
- [ ] 性能基准测试：不同范围下 SAT 编码与求解的性能

**交付物**：`axiom_pkg.h` 更新、`axiom_pkg_loader.c`、集成测试套件、性能报告。

---

## 5. 附录：参考文献与相关项目

### 5.1 核心参考文献

1. Jackson, D. (2012). *Software Abstractions: Logic, Language, and Analysis* (Revised Edition). MIT Press. — Alloy 的权威著作，涵盖关系逻辑、小范围假设和 Alloy 语言设计哲学。

2. Jackson, D. (2006). "Dependable Software by Design." *Scientific American*, 294(6), 68-75. — Alloy 理念的通俗阐述。

3. Torlak, E., & Jackson, D. (2007). "Kodkod: A Relational Model Finder." *International Conference on Tools and Algorithms for the Construction and Analysis of Systems (TACAS)*, 632-647. — Kodkod 引擎的原始论文，详述关系逻辑 → SAT 的编码方案。

4. Zave, P. (2015). "Using Lightweight Modeling to Understand Chord." *ACM SIGCOMM Computer Communication Review*, 42(2), 49-57. — Alloy 在分布式系统协议分析中的经典应用案例。

5. Near, J. P., & Jackson, D. (2012). "Rubicon: Bounded Verification of Web Applications." *ACM SIGSOFT International Symposium on Foundations of Software Engineering (FSE)*. — Alloy 在 Web 应用验证中的扩展应用。

### 5.2 相关开源项目

| 项目 | 仓库 | 关联 |
|:---|:---|:---|
| Alloy 主仓库 | `github.com/AlloyTools/org.alloytools.alloy` | 本报告借鉴对象 |
| Kodkod | 随 Alloy 分发（`github.com/emina/kodkod`） | SAT 编码引擎 |
| Alloy*（扩展） | `github.com/aleksandarmilicevic/alloy-star` | 高阶关系逻辑 |
| Sterling | `github.com/sterling-ts/sterling` | Alloy 的下一代 Web UI |
| Aunit | `github.com/AlloyTools/org.alloytools.alloy` | Alloy 单元测试框架 |
| TLC (TLA+) | `github.com/tlaplus/tlaplus` | 互补项目：时序逻辑 vs 关系逻辑 |
| MiniSat | `github.com/niklasso/minisat` | Alloy 默认 SAT 求解器 |
| Glucose | `github.com/audemard/glucose` | Kodkod 可选的现代 SAT 求解器 |

### 5.3 Lv-00 项目内关联文档

- `docs/reference/tlaplus_formal_specification.md` — TLA+ 形式化规约借鉴（互补设计）
- `docs/reference/maude_rewriting_semantics.md` — 重写逻辑借鉴
- `docs/reference/minizinc_model_data_separation.md` — 约束模型与数据分离
- `docs/reference/minikanren_relational_proof.md` — 关系化证明
- `docs/reference/egg_egraph_rewriting.md` — E-graph 重写
- `docs/reference/souffle_datalog_engine.md` — Datalog 关系引擎
- `docs/architecture_v3.2.md` — Lv-00 系统架构文档
- `include/lv00/constraint_graph.h` — 约束图核心头文件
- `include/lv00/solver.h` — 求解器核心头文件
- `include/lv00/axiom_pkg.h` — 公理包核心头文件

---

> **文档版本**：v1.0
> **最后更新**：2026-05-24
> **维护者**：Lv-00 项目组
> **许可证**：MIT
