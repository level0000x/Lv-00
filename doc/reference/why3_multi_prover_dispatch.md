# Why3 多求解器分派与中间验证语言借鉴设计

> **借鉴项目**：Why3（gitlab.inria.fr/why3/why3）
> **核心借鉴点**：WhyML 中间验证语言、Driver 多后端分派机制、证明任务分解与会话管理
> **分类**：P2 高优先级 / 多引擎调度与中间表示
> **日期**：2026-05-24

---

## 1. 概述

Why3 是由 INRIA（法国国家信息与自动化研究所）开发的一个形式化验证平台，其核心定位是"多求解器后端之上的通用验证框架"。Why3 的设计哲学是**关注点分离**：用户使用统一的中间语言 WhyML 编写程序和规约，Why3 则通过 Driver 机制将验证任务翻译为不同后端求解器的输入格式（SMT-LIB、Coq、Isabelle 等），并自动分派求解。这种架构对 Lv-00 的多引擎验证系统具有极高的借鉴价值。

Why3 的三个核心技术对 Lv-00 的借鉴意义：

1. **WhyML 中间验证语言**：WhyML 是 Why3 的一阶逻辑语言，用于描述程序和验证条件。它作为用户层面的 DSL（如 Why3ML 编程语言）和底层求解器之间的"通用中间表示"，使得所有的程序分析和变换都在 WhyML 层面完成，与具体求解器无关。对于 Lv-00 而言，这意味着需要设计一个 **GVIL（几何验证中间语言，Geometric Verification Intermediate Language）**，将几何构造和命题统一编码为与后端求解器无关的中间表示。

2. **Driver 多后端分派机制**：Why3 通过 Driver 文件定义如何将 WhyML 理论翻译为特定求解器的输入。Driver 不仅处理语法翻译，还控制"将哪些命题发送给哪个求解器"——这种基于**命题特征的后端自动选择**是 Lv-00 多引擎调度的核心灵感来源。对于几何验证，不同求解器各有所长：SMT 擅长等式推理、代数求解器擅长多项式约束、几何专用求解器擅长面积法证明。

3. **证明任务分解与会话管理**：Why3 的会话系统将一次完整的验证分解为多个独立的任务（goal），每个任务被分派到多个求解器并行执行，结果被持久化存储。这种"任务分解 + 并行分派 + 结果缓存"模型完美适用于 Lv-00 的复合几何命题验证场景。

Why3 的总体架构可以概括为：

```
Why3ML 编程语言（用户层）
    ↓ 编译/翻译
WhyML 中间语言（IR 层）
    ↓ Driver 翻译
┌───────┬───────┬───────┬───────┐
│ SMT   │ Coq   │ PVS   │ Isabelle│
│(CVC4/ │       │       │         │
│ Z3/   │       │       │         │
│ Alt-  │       │       │         │
│ Ergo) │       │       │         │
└───────┴───────┴───────┴───────┘
```

Lv-00 的对应架构目标是：

```
几何 DSL（用户层）
    ↓ 编译/语义分析
GVIL 几何验证中间语言（IR 层）
    ↓ Engine Scheduler 翻译
┌───────┬───────┬───────┬───────┐
│ SMT   │ 代数  │ 几何  │ 数值  │
│求解器 │求解器 │求解器 │求解器 │
│(Z3/  │(Groeb-│(面积  │(数值  │
│ CVC5) │ner基) │法/全  │逼近)  │
│       │       │等法)  │       │
└───────┴───────┴───────┴───────┘
```

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 Driver 机制：基于命题特征的自动后端选择

Why3 的 Driver 机制允许用户定义**transform**（变换）和**prover**（证明器）的选择规则。其核心思想是：不同的命题具有不同的结构特征，应将它们路由到最合适的求解器。例如：

- **量词命题**（`forall` / `exists`）可能需要交互式定理证明器（如 Coq）
- **线性算术命题**（`x + y = z`）适合 SMT 求解器（如 Z3、CVC4）
- **代数等式**（多项式等式）适合 Alt-Ergo 或专用的代数求解器
- **归纳命题**需要交互式证明辅助

在 Lv-00 的几何验证场景中，同样存在命题特征分类的需求：

| 几何命题特征 | 最适合的求解器 | 理由 |
|:---|:---|:---|
| 线段长度等式（如 `|AB| = |CD|`） | SMT 求解器（Z3/CVC5） | 可编码为实数等式，SMT 的 NRA 理论天然支持 |
| 角度等式（如 `∠ABC = ∠DEF`） | 代数求解器（Groebner 基） | 角度关系编码为多项式等式后可用 Groebner 基消元 |
| 共线/共点条件（如 `collinear(A,B,C)`） | 几何专用求解器（面积法） | 面积法的行列式表达天然处理共线/共点 |
| 三角形全等/相似 | 几何专用求解器（全等法） | 全等判定定理（SSS/SAS/ASA）是几何专有的 |
| 存在性命题（如 `∃P, equilateral(P,Q,R)`） | 数值求解器 + 代数求解器 | 需构造性满足，数值法可快速给出候选解 |
| 不等式（如 `|AB| + |BC| > |AC|`） | SMT 求解器 | SMT 的 LRA/NRA 理论天然支持不等式 |

### 2.2 WhyML 中间语言启发 GVIL 设计

WhyML 提供了一套丰富的类型和表达式系统作为中间表示：

```
(* WhyML 中的整数类型 *)
type int

(* WhyML 中的代数数据类型 *)
type option 'a = None | Some 'a

(* WhyML 中的逻辑公式 *)
predicate p (x: int) = x > 0
```

在 Lv-00 中，GVIL 需要支持以下几何原语作为一等公民：

**GVIL 类型系统**：

| GVIL 类型 | 几何语义 | 对应 WhyML 类型构造 |
|:---|:---|:---|
| `GeoPoint` | 二维平面上的点 | 乘积类型 `(real, real)` |
| `GeoLine` | 直线（两点定义或方程定义） | 记录类型 `{p1: point; p2: point}` |
| `GeoSegment` | 线段（端点 + 长度约束） | 精化类型 `line{length = n}` |
| `GeoCircle` | 圆（圆心 + 半径约束） | 记录类型 `{center: point; radius: real}` |
| `GeoTriangle` | 三角形（三点不共线） | 精化类型 `tuple(point,point,point){noncollinear}` |
| `GeoPolygon` | 多边形（有序顶点列表） | 列表类型 `list point` |
| `GeoAngle` | 角（顶点 + 两条射线） | 记录类型 `{vertex: point; ray1: line; ray2: line}` |

**GVIL 谓词系统**（命题编码）：

| GVIL 谓词 | 几何语义 | 编码策略 |
|:---|:---|:---|
| `collinear(A, B, C)` | A、B、C 三点共线 | 面积为零：`det(B-A, C-A) = 0` |
| `perpendicular(L1, L2)` | L1 垂直于 L2 | 点积为零：`(L1.dir) · (L2.dir) = 0` |
| `parallel(L1, L2)` | L1 平行于 L2 | 叉积为零：`L1.dir × L2.dir = 0` |
| `concyclic(P1, P2, P3, P4)` | 四点共圆 | 行列式条件（带符号面积） |
| `congruent(T1, T2)` | 三角形 T1 全等于 T2 | 三边 SSS 等式 |
| `similar(T1, T2)` | 三角形 T1 相似于 T2 | 三边比例等式 |
| `between(P, A, B)` | P 在线段 AB 上（含端点） | `collinear(P,A,B) ∧ (AP·AB≥0) ∧ (BP·BA≥0)` |
| `equidistant(P, A, B)` | P 到 A 和 B 距离相等 | `\|P-A\| = \|P-B\|` |

### 2.3 对照表：Why3 概念 → Lv-00 engine_scheduler.h 映射

| Why3 概念 | Lv-00 engine_scheduler.h 映射 | 说明 |
|:---|:---|:---|
| `Driver` | `EngineProfile` 结构体 | 定义如何将 GVIL 命题翻译为特定求解器输入 |
| `Transform`（变换） | `EnginePass` 枚举 | 命题预处理变换（归一化、化简、拆分） |
| `Prover`（证明器） | `SolverBackend` 枚举 | 后端求解器类型标识 |
| `Goal`（证明目标） | `ProofGoal` 结构体 | 单个待验证的几何命题 |
| `Task`（任务） | `SolverTask` 结构体 | 封装一次求解请求（目标 + 后端 + 超时） |
| `Session`（会话） | `ProofSession` 结构体 | 管理所有任务的生命周期和结果缓存 |
| `Task split`（任务拆分） | `proof_goal_decompose()` | 将复合命题拆分为独立子任务 |
| `Why3 IDE` | `proof_debugger` 模块 | 交互式证明调试界面 |

### 2.4 代码示例：Lv-00 中基于命题特征的后端自动选择路由

```c
/**
 * @file engine_scheduler.h
 * @brief 多引擎证明任务调度器——借鉴 Why3 Driver 多后端分派机制
 *
 * 核心功能：
 *  1. 分析几何命题的特征（命题类型、运算符、几何构造复杂度）
 *  2. 根据特征矩阵自动选择最合适的后端求解器
 *  3. 多后端并行的结果合并与投票策略
 */

#ifndef LV00_ENGINE_SCHEDULER_H
#define LV00_ENGINE_SCHEDULER_H

#include "proof.h"
#include "gvil.h"

/* ── 求解器后端类型枚举 ─────────────────────────────── */

/**
 * @brief 后端求解器类型
 *
 * 借鉴 Why3 的多后端架构，每种求解器擅长不同类型的几何命题。
 */
typedef enum {
    SOLVER_SMT_Z3,           /**< Z3 SMT 求解器（擅长等式/不等式） */
    SOLVER_SMT_CVC5,         /**< CVC5 SMT 求解器（Z3 的备选） */
    SOLVER_ALGEBRA_GROEBNER, /**< Groebner 基代数求解器（擅长多项式消元） */
    SOLVER_GEOM_AREA,        /**< 面积法几何求解器（擅长共线/共点/平行） */
    SOLVER_GEOM_FULLANGLE,   /**< 全等法几何求解器（擅长角度/全等命题） */
    SOLVER_NUMERIC_NEWTON,   /**< 牛顿法数值求解器（擅长存在性构造） */
    SOLVER_COUNT
} SolverBackend;

/* ── 命题特征结构体 ────────────────────────────────── */

/**
 * @brief 几何命题的特征描述
 *
 * 用于 Driver 规则匹配：分析命题的结构特征，决定路由到哪个求解器。
 */
typedef struct {
    bool has_equality;           /**< 含等式（如 |AB| = |CD|） */
    bool has_inequality;         /**< 含不等式（如 |AB| > |CD|） */
    bool has_angle;              /**< 含角度（如 ∠ABC） */
    bool has_collinearity;       /**< 含共线性（如 collinear(A,B,C)） */
    bool has_concyclicity;       /**< 含共圆性（如 concyclic(A,B,C,D)） */
    bool has_congruence;         /**< 含全等判断（如 △ABC ≅ △DEF） */
    bool has_existential;        /**< 含存在量词（如 ∃P, ...） */
    bool has_universal;          /**< 含全称量词（如 ∀P, ...） */
    int  algebraic_degree;       /**< 代数次数（多项式最高次数） */
    int  point_count;            /**< 涉及的几何点数 */
    int  constraint_count;       /**< 涉及的总约束数 */
    bool is_synthetic;           /**< 是否为纯综合几何命题 */
} PropositionFeature;

/* ── 引擎配置 Profile（对应 Why3 Driver） ──────────── */

/**
 * @brief 单个求解器的引擎配置
 *
 * 对应 Why3 的 Driver 定义：描述一个后端如何接收和求解命题。
 * 每个后端可以有自己的超时、编码参数和信任级别。
 */
typedef struct {
    SolverBackend backend;           /**< 后端标识 */
    const char *command_template;    /**< 命令行模板（如 "z3 -T:%d %s"） */
    int  timeout_ms;                 /**< 超时（毫秒） */
    int  trust_level;                /**< 信任级别（0-10，越高越可信） */
    bool supports_inequalities;      /**< 是否支持不等式 */
    bool supports_angles;            /**< 是否支持角度推理 */
    bool supports_existential;       /**< 是否支持存在量词构造 */
    bool supports_universal;         /**< 是否支持全称量词 */
} EngineProfile;

/* ── Driver 决策函数 ────────────────────────────────── */

/**
 * @brief 分析命题特征
 *
 * 遍历 GVIL 命题表达式树，提取命题的结构化特征。
 * 相当于 Why3 中的 transform + feature_extraction 的组合。
 *
 * @param goal  待分析的证明目标
 * @return 命题特征描述
 */
PropositionFeature proposition_extract_features(const ProofGoal *goal);

/**
 * @brief 根据命题特征匹配最合适的求解器
 *
 * 这是 Lv-00 的 Driver 内核——基于特征向量和各个后端的
 * 能力矩阵，通过加权评分选出最优求解器。
 *
 * 借鉴 Why3 的 Driver 选择逻辑：
 *  - 命题含角度 → 优先选择支持角度推理的后端（全等法 / Groebner）
 *  - 命题含存在量词 → 优先选择支持构造性求解的后端（数值法）
 *  - 命题代数次数 >= 3 → 避免纯 SMT，优先使用 Groebner 基
 *
 * @param feature        命题特征
 * @param profiles       可用的引擎配置数组
 * @param profile_count  引擎配置数量
 * @param out_rankings   输出：各求解器的得分排名（调用者需预分配 SolverBackend[SOLVER_COUNT]）
 * @return 最优求解器索引（-1 表示无可用求解器）
 */
int engine_driver_select(
    const PropositionFeature *feature,
    const EngineProfile *profiles,
    int profile_count,
    SolverBackend *out_rankings
);

/**
 * @brief 多后端并行分派与结果投票
 *
 * 借鉴 Why3 的策略：将一个 ProofGoal 同时发送到多个后端，
 * 收集结果后进行投票。投票策略：
 *  - 如果任一高信任级别后端返回 PROVED → 直接通过
 *  - 如果两个后端返回 PROVED → 高置信度通过
 *  - 如果所有后端返回 UNKNOWN → 标记为需要交互式证明
 *  - 如果任一后端返回 COUNTEREXAMPLE → 记录反例并标记失败
 *
 * @param goal      待验证的证明目标
 * @param profiles  引擎配置数组
 * @param count     引擎数量
 * @return 合并后的验证结果
 */
ProofResult engine_multi_dispatch(
    const ProofGoal *goal,
    const EngineProfile *profiles,
    int count
);

/* ── 证明任务分解（Task Split） ─────────────────────── */

/**
 * @brief 将复合几何命题分解为独立子任务
 *
 * 借鉴 Why3 的 split_goal 变换：将形如 "A ∧ B ∧ C"
 * 的合取命题分解为独立的子命题 A、B、C。
 *
 * 在 Lv-00 中的典型场景：
 *  - 命题："△ABC 是等腰三角形 AND AB 垂直于 CD"
 *    拆分为：
 *      Goal 1: is_isosceles(△ABC)
 *      Goal 2: perpendicular(AB, CD)
 *    两个子目标可以并行分派，互不依赖。
 *
 * 依赖感知分解：如果 B 的证明依赖 A 的结论，则 B
 * 在 A 完成前不能分派（标记为 HOLD）。
 *
 * @param goal        复合证明目标
 * @param out_subgoals 输出：分解后的子目标数组
 * @param out_count   输出：子目标数量
 * @return 分解结果
 */
DecomposeResult proof_goal_decompose(
    const ProofGoal *goal,
    ProofGoal ***out_subgoals,
    int *out_count
);

/* ── 会话管理（Session） ────────────────────────────── */

/**
 * @brief 证明会话管理器
 *
 * 借鉴 Why3 Session：管理整个证明会话中所有任务的生命周期，
 * 支持任务的状态持久化（避免重复求解已验证的命题）和增量验证
 * （当约束图变更时仅重验证受影响的命题）。
 */
typedef struct {
    int session_id;                   /**< 会话唯一标识 */
    ProofGoal **goals;                /**< 所有证明目标 */
    int goal_count;                   /**< 目标总数 */
    int completed_count;              /**< 已完成数 */
    int failed_count;                 /**< 失败数 */

    /* ── 结果缓存（避免重复求解） ── */
    /* key = goal_hash, value = cached_result */
    struct hash_table *result_cache;

    /* ── 增量验证脏标记 ── */
    struct hash_table *dirty_goals;   /**< 因约束图变更而失效的目标 */

    /* ── 统计信息 ── */
    int64_t total_solver_time_ms;     /**< 总求解器耗时 */
    int  total_solver_calls;          /**< 总求解器调用次数 */
    int  cache_hits;                  /**< 缓存命中次数 */
} ProofSession;

/**
 * @brief 创建证明会话
 */
ProofSession *proof_session_create(void);

/**
 * @brief 向会话中添加证明目标
 */
void proof_session_add_goal(ProofSession *session, ProofGoal *goal);

/**
 * @brief 执行会话中的所有待处理目标
 *
 * 工作流程（借鉴 Why3 Session.run_all）：
 *  1. 遍历所有 goals，标记依赖关系图
 *  2. 将无依赖的 goal 放入就绪队列
 *  3. 对就绪队列中的 goal 并行分派到多个后端
 *  4. 收集结果并更新依赖图
 *  5. 将新就绪的 goal 加入队列（重复 3-5 直到队列空）
 *  6. 生成验证报告
 */
void proof_session_run(ProofSession *session,
                       const EngineProfile *profiles,
                       int profile_count);

/**
 * @brief 使受约束图变更影响的目标失效（增量验证）
 */
void proof_session_invalidate(ProofSession *session,
                              const bool *affected_goal_indices,
                              int count);

/**
 * @brief 销毁会话并释放资源
 */
void proof_session_destroy(ProofSession *session);

#endif /* LV00_ENGINE_SCHEDULER_H */
```

### 2.5 Driver 评分矩阵的设计原理

Why3 的 Driver 选择本质上是一个启发式决策过程。Lv-00 将其形式化为一个**加权特征匹配矩阵**：

```
命题特征提取：
  goal → feature_vector = (f1, f2, ..., fn)
  其中 fi ∈ {0, 1} 表示该特征是否存在

后端能力矩阵：
  profile[j].capabilities = (c_j1, c_j2, ..., c_jn)
  其中 c_ji ∈ [0.0, 1.0] 表示后端 j 对该特征的处理能力

评分：
  score[j] = Σ_i (feature_vector[i] × c_ji × weight[i])
  其中 weight[i] 是该特征的重要性权重

选择：
  best_backend = argmax_j (score[j])
```

具体实现中，角度特征的权重高于等式特征（因为几何中角度的处理远比等式复杂），存在量词的权重高于全称量词（因为存在性构造更困难）。

---

## 3. 实现方案

### 3.1 第一阶段：GVIL 中间验证语言设计（P2-1）

- [ ] 定义 GVIL 的类型系统（`GeoPoint`, `GeoLine`, `GeoSegment`, `GeoCircle`, `GeoTriangle`）
- [ ] 定义 GVIL 的谓词系统（`collinear`, `perpendicular`, `parallel`, `concyclic`, `congruent`, `similar`）
- [ ] 定义 GVIL 的 AST 节点结构（表达式、命题、声明、目标）
- [ ] 实现 GVIL 的序列化/反序列化（用于跨进程通信）
- [ ] 实现 GVIL 的类型检查器
- [ ] 编写 GVIL 的语义测试用例

### 3.2 第二阶段：命题特征提取与 Driver 选择（P2-2）

- [ ] 实现 `proposition_extract_features()` 特征提取器
- [ ] 定义各后端的能力矩阵（`EngineProfile`）
- [ ] 实现 `engine_driver_select()` 评分和选择逻辑
- [ ] 实现 Driver 评分的可配置权重（允许用户覆盖默认启发式）
- [ ] 编写特征提取的单元测试

### 3.3 第三阶段：多后端并行分派（P2-3）

- [ ] 实现 `engine_multi_dispatch()` 并行分派
- [ ] 实现各后端的 GVIL → 求解器输入的翻译器：
  - [ ] GVIL → SMT-LIB（Z3/CVC5）
  - [ ] GVIL → 多项式系统（Groebner 基）
  - [ ] GVIL → 面积法输入
  - [ ] GVIL → 全等法输入
- [ ] 实现后端进程管理（启动、超时杀死、输出捕获）
- [ ] 实现求解结果的统一解析器
- [ ] 实现多后端结果投票策略

### 3.4 第四阶段：任务分解与会话管理（P2-4）

- [ ] 实现 `proof_goal_decompose()` 合取命题分解
- [ ] 实现子目标依赖图分析
- [ ] 实现 `ProofSession` 的创建和生命周期管理
- [ ] 实现结果缓存（哈希表，键为命题规范化形式）
- [ ] 实现增量验证的脏标记机制
- [ ] 实现会话的持久化存储和恢复
- [ ] 实现验证进度报告生成

### 3.5 第五阶段：集成与优化（P2-5）

- [ ] 将 GVIL 编译管道接入几何 DSL 前端
- [ ] 实现 Driver 选择的运行时统计分析（追踪各后端的成功率）
- [ ] 基于运行时统计的 Driver 权重自适应调整
- [ ] 实现求解器结果的跨目标复用（如"已证明 A → 使用 A 证明 B"）
- [ ] 进行大规模几何题库的基准测试
- [ ] 编写完整文档和集成测试

---

## 4. 参考资源

- **Why3 官方文档**：[https://why3.lri.fr/doc/](https://why3.lri.fr/doc/)
- **Why3 源码仓库**：[https://gitlab.inria.fr/why3/why3](https://gitlab.inria.fr/why3/why3)
- **WhyML 语言参考**：Bobot, Filliatre, Marche, Paskevich. "Why3: Shepherd Your Herd of Provers" (2011)
- **Driver 机制详情**：Why3 Manual, Chapter 10: "Drivers" —— 详细描述 Driver 文件的语法和变换规则
- **证明会话系统**：Why3 Manual, Chapter 9: "The Why3 IDE and Proof Sessions"
- **任务分解策略**：Filliatre, Paskevich. "Abstraction and Genericity in Why3" (2020)
- **SMT 求解器选择策略**：Conchon et al. "A Three-Tier Strategy for Reasoning about Floating-Point Numbers in SMT" (2017, 借鉴求解器分派思想)
- **代数求解器集成**：Gregoire, Thery. "A Purely Functional Library for Modular Arithmetic and Its Application to Certifying Large Prime Numbers" (2006)
- **本系列相关文档**：
  - `fstar_refinement_smt.md` —— F* 精化类型 + SMT 混合验证借鉴
  - `dafny_layered_verification.md` —— Dafny 三层验证架构借鉴
  - `yices2_ef_solving.md` —— Yices 2 EF 混合求解架构借鉴
  - `rosette_symbolic_vm.md` —— Rosette 符号虚拟机借鉴
