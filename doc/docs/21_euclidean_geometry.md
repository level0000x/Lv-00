# 21 欧几里得几何公理体系 (Euclidean Geometry Axiom System)

## 1 模块概述

欧几里得几何公理体系模块（`euclidean_geometry.h`）实现 Lv-00 的可插拔几何公理包，提供 Hilbert 五大公理组的完整形式化表示，并支持 Birkhoff（1932）与 Tarski（1959）双公理体系的翻译映射与等价性验证。该模块借鉴 mathlib4 `EuclideanGeometry` 的形式化设计，以类型安全的谓词系统实现免坐标风格（SyntheticGeometry）的几何推理，与 Lv-00 的 `ConstraintGraph` 紧密集成。

### 1.1 设计定位

本模块是 Lv-00 **可插拔公理包**的一部分，而非 Lv-00 底层框架的唯一理论根基。Lv-00 的核心框架（约束图、符号坐标、求解器等）独立于任何特定公理体系运行；本模块作为可选插件，为需要经典欧几里得几何推理的场景提供标准化公理支持。遵循项目既定要求：不将外来公理系统套用为 Lv-00 的底层理论根基。

### 1.2 架构层次

```
EuclideanContext          -- 全局上下文（活跃公理体系、注册实体、一致性状态）
  |
  +-- Hilbert 五大公理组  -- 关联/顺序/全等/平行/连续
  +-- 几何关系谓词        -- 共线性/介于性/全等性/平行/垂直
  +-- EquivalenceProofChain -- Birkhoff/Tarski 等价性证明链
  +-- ConstraintGraph     -- 约束图集成（借引用）
```

---

## 2 公理体系枚举

### 2.1 EuclideanAxiomSystem 枚举

```c
typedef enum {
    EUCLID_BIRKHOFF, /**< Birkhoff 公理体系：实数量度 + 角度 */
    EUCLID_TARSKI,   /**< Tarski 公理体系：一阶逻辑，仅点变量 */
    EUCLID_HILBERT,  /**< Hilbert 公理体系：五大公理组（默认） */
    EUCLID_CUSTOM    /**< 用户自定义公理体系 */
} EuclideanAxiomSystem;
```

| 体系 | 公理数量 | 特征 |
|------|---------|------|
| Birkhoff | 4 条 | 基于实数度量和角度，公理数量少但依赖实数完备性 |
| Tarski | 11 条 + 连续性公理模式 | 一阶逻辑，仅含点变量，Betweenness/Congruence 两个基础谓词 |
| Hilbert | 20 条 | 五大公理组的经典表述，最接近传统几何教材 |
| Custom | 用户定义 | 可扩展的自定义公理体系 |

默认启用 `EUCLID_HILBERT`。

---

## 3 Hilbert 五大公理组

### 3.1 关联公理（Incidence Axioms, I.1-I.8）

定义点与线的从属关系，构成几何空间的基本骨架。

```c
typedef enum {
    INCIDENCE_TWO_POINTS_ONE_LINE,         /**< I.1: 任意两点确定唯一一条直线 */
    INCIDENCE_LINE_CONTAINS_TWO_POINTS,    /**< I.2: 每条直线至少含两点 */
    INCIDENCE_THREE_NONCOLLINEAR_POINTS,   /**< I.3: 存在至少三个不共线的点 */
    INCIDENCE_THREE_POINTS_ONE_PLANE,      /**< I.4: 任意不共线三点确定唯一平面 */
    INCIDENCE_PLANE_CONTAINS_LINE,         /**< I.5: 若直线两点在平面内，则整条线在平面内 */
    INCIDENCE_TWO_PLANES_INTERSECT_LINE,   /**< I.6: 两平面交于一直线 */
    INCIDENCE_PLANE_CONTAINS_THREE_POINTS, /**< I.7: 每个平面至少含三个不共线点 */
    INCIDENCE_FOUR_NONCOPLANAR_POINTS      /**< I.8: 存在至少四个不共面的点 */
} IncidenceAxiom;
```

实现中通过 `euclidean_verify_axiom_inconsistency()` 验证 I.1（任意两点确定唯一直线）：遍历已注册点对，检查是否存在两个点共享多条不同直线的情况。

### 3.2 顺序公理（Order/Betweenness Axioms, II.1-II.4）

定义点在线上的顺序关系，是 Tarski 公理体系的基石。

```c
typedef enum {
    ORDER_BETWEENNESS_SYMMETRY,     /**< II.1: B 在 A,C 之间 <=> B 在 C,A 之间 */
    ORDER_TWO_POINTS_ONE_BETWEEN,   /**< II.2: 给定 A,C，存在 B 在 A,C 之间 */
    ORDER_THREE_POINTS_ONE_BETWEEN, /**< II.3: 任意三个共线点，恰有一点在其余两点之间 */
    ORDER_PASCH_AXIOM               /**< II.4: Pasch 公理 */
} OrderAxiom;
```

实现中通过 `euclidean_verify_axiom_inconsistency()` 验证 II.3：检查是否存在两个不同的点被声称在同一对端点之间，且三点共线的矛盾情况。

### 3.3 全等公理（Congruence Axioms, III.1-III.5）

定义线段和角的相等关系，支持 SAS/SSS/ASA 等推理链。

```c
typedef enum {
    CONGRUENCE_SEGMENT_TRANSFER, /**< III.1: 线段可转移 */
    CONGRUENCE_TRANSITIVITY,     /**< III.2: 全等传递性 */
    CONGRUENCE_SEGMENT_ADDITION, /**< III.3: 线段加法保全等 */
    CONGRUENCE_ANGLE_TRANSFER,   /**< III.4: 角度可转移 */
    CONGRUENCE_SAS               /**< III.5: SAS 全等 */
} CongruenceAxiom;
```

实现中验证 III.2（全等传递性）：检查 CONTAINMENT 类型约束中是否存在共享参与者但不一致的传递关系。

### 3.4 平行公理（Parallel Axiom, IV）

```c
typedef enum {
    PARALLEL_PLAYFAIR,     /**< Playfair 公理：过直线外一点有且仅有一条平行线 */
    PARALLEL_EUCLID_FIFTH, /**< Euclid 第五公设 */
    PARALLEL_PROCLUS       /**< Proclus 等价形式 */
} ParallelAxiom;
```

### 3.5 连续公理（Continuity Axioms, V.1-V.2）

```c
typedef enum {
    CONTINUITY_ARCHIMEDES,       /**< V.1: Archimedes 公理 */
    CONTINUITY_LINE_COMPLETENESS /**< V.2: 直线完备性 */
} ContinuityAxiom;
```

### 3.6 公理启用位掩码

五大公理组通过 `uint32_t enabled_axioms_mask` 统一管理，各组的位偏移如下：

| 位范围 | 公理组 | 条目数 |
|--------|--------|--------|
| bits 0-7 | IncidenceAxiom | 8 条 |
| bits 8-11 | OrderAxiom | 4 条 |
| bits 12-16 | CongruenceAxiom | 5 条 |
| bits 17-19 | ParallelAxiom | 3 条 |
| bits 20-21 | ContinuityAxiom | 2 条 |

默认值 `0x003FFFFF` 启用全部 22 条公理。

---

## 4 几何关系谓词

借鉴 mathlib4 `SyntheticGeometry` 的类型安全谓词设计，每个谓词封装一种几何关系，支持符号和数值两种验证模式。

### 4.1 CollinearityPredicate -- 共线性谓词

```c
typedef struct CollinearityPredicate {
    int *point_ids;            /**< 点 ID 数组 */
    int point_count;           /**< 点数量（>= 3） */
    bool is_collinear;         /**< 验证结果 */
    double collinearity_error; /**< 共线误差（数值模式） */
    bool verified_symbolic;    /**< 是否已通过符号验证 */
} CollinearityPredicate;
```

符号模式使用行列式法：`det = (bx - ax)*(cy - ay) - (by - ay)*(cx - ax) = 0`。

### 4.2 BetweennessPredicate -- 介于性谓词

```c
typedef struct BetweennessPredicate {
    int point_a_id;   /**< 点 A 的 ID */
    int point_b_id;   /**< 点 B 的 ID（可能介于 A,C 之间） */
    int point_c_id;   /**< 点 C 的 ID */
    bool is_between;  /**< 验证结果 */
    bool verified;    /**< 是否已验证 */
    double ratio;     /**< BA:BC 的比值（数值模式） */
    int axiom_source; /**< 推理依据的公理枚举值 */
} BetweennessPredicate;
```

判断条件：`|AB| + |BC| == |AC|`，若 `0 < ratio < 1` 则 B 在 A 和 C 之间。

### 4.3 CongruencePredicate -- 全等性谓词

```c
typedef struct CongruencePredicate {
    int obj_type; /**< 0=线段全等, 1=角度全等 */
    union {
        struct { int seg_a1_id, seg_a2_id, seg_b1_id, seg_b2_id; } seg;
        struct { int ang_vertex_a, ang_side_a1, ang_side_a2,
                 ang_vertex_b, ang_side_b1, ang_side_b2; } ang;
    } args;
    bool is_congruent;    /**< 验证结果 */
    bool verified;        /**< 是否已验证 */
    double tolerance;     /**< 容差（数值模式） */
    int proof_step_count; /**< 证明步骤数量 */
    int *proof_step_ids;  /**< 证明步骤 ID 数组 */
} CongruencePredicate;
```

### 4.4 ParallelPredicate / PerpendicularPredicate

```c
typedef struct ParallelPredicate {
    int line_a_id;           /**< 直线 A 的 ID */
    int line_b_id;           /**< 直线 B 的 ID */
    bool is_parallel;        /**< 验证结果 */
    int parallel_axiom_used; /**< 使用的平行公理版本 */
} ParallelPredicate;

typedef struct PerpendicularPredicate {
    int line_a_id;         /**< 直线 A 的 ID */
    int line_b_id;         /**< 直线 B 的 ID */
    bool is_perpendicular; /**< 验证结果 */
    double angle_degrees;  /**< 夹角（度） */
} PerpendicularPredicate;
```

---

## 5 Birkhoff/Tarski 翻译映射

### 5.1 设计目标

Birkhoff 公理体系基于实数量度（Ruler Postulate、Protractor Postulate、SAS、平行公理），而 Tarski 公理体系基于一阶逻辑（仅点变量，Betweenness 和 Congruence 两个基础谓词）。本模块提供双向翻译映射，将度量公理转化为免坐标风格的谓词系统。

### 5.2 Birkhoff -> Tarski 翻译映射

```c
static const int birkhoff_to_tarski[] = {
    0,  /* Birkhoff 0 (Ruler)      -> Tarski 0 (标识公理) */
    1,  /* Birkhoff 0 (Ruler)      -> Tarski 1 (对称公理) */
    2,  /* Birkhoff 0 (Ruler)      -> Tarski 2 (传递公理) */
    3,  /* Birkhoff 1 (Protractor) -> Tarski 3 (全等标识) */
    4,  /* Birkhoff 1 (Protractor) -> Tarski 4 (线段构造) */
    5,  /* Birkhoff 2 (SAS)        -> Tarski 5 (五段公理) */
    -1, /* 占位 */
    6,  /* Birkhoff 2 (SAS)        -> Tarski 6 (恒等公理) */
    7,  /* Birkhoff 2 (SAS)        -> Tarski 7 (Pasch 公理) */
    8,  /* Birkhoff 2 (SAS)        -> Tarski 8 (下维公理) */
    9,  /* Birkhoff 2 (SAS)        -> Tarski 9 (上维公理) */
    10, /* Birkhoff 3 (Parallel)   -> Tarski 10 (欧几里得公理) */
};
```

### 5.3 Tarski -> Birkhoff 逆向映射

```c
static const int tarski_to_birkhoff[] = {
    0,  /* Tarski 0  -> Birkhoff 0 (Ruler) */
    0,  /* Tarski 1  -> Birkhoff 0 */
    0,  /* Tarski 2  -> Birkhoff 0 */
    1,  /* Tarski 3  -> Birkhoff 1 (Protractor) */
    1,  /* Tarski 4  -> Birkhoff 1 */
    2,  /* Tarski 5  -> Birkhoff 2 (SAS) */
    2,  /* Tarski 6  -> Birkhoff 2 */
    2,  /* Tarski 7  -> Birkhoff 2 */
    2,  /* Tarski 8  -> Birkhoff 2 */
    2,  /* Tarski 9  -> Birkhoff 2 */
    3,  /* Tarski 10 -> Birkhoff 3 (Parallel) */
};
```

---

## 6 EquivalenceProofChain 结构

等价性证明链是连接 Birkhoff 和 Tarski 两个公理体系的核心数据结构，支持双向翻译映射和验证状态追踪。

### 6.1 验证状态枚举

```c
typedef enum {
    EQUIV_STATUS_PENDING,   /**< 待验证 */
    EQUIV_STATUS_VERIFIED,  /**< 已验证等价 */
    EQUIV_STATUS_FAILED,    /**< 验证失败（不等价） */
    EQUIV_STATUS_INCOMPLETE /**< 不完全（缺少必要的引理） */
} EquivVerificationStatus;
```

### 6.2 EquivalenceProofChain 结构

```c
typedef struct EquivalenceProofChain {
    EuclideanAxiomSystem source_system; /**< 源公理体系 */
    EuclideanAxiomSystem target_system; /**< 目标公理体系 */
    EquivVerificationStatus status;     /**< 当前验证状态 */

    int *axiom_translation_map; /**< 公理翻译映射表 */
    int translation_count;      /**< 翻译映射条目数 */

    int *lemma_ids;  /**< 所需引理的 ID 数组 */
    int lemma_count; /**< 引理数量 */

    bool birhoff_implies_tarski;  /**< Birkhoff => Tarski 方向已验证 */
    bool tarski_implies_birkhoff; /**< Tarski => Birkhoff 方向已验证 */

    ConstraintGraph *verification_graph; /**< 验证过程中构建的约束图 */
} EquivalenceProofChain;
```

### 6.3 验证流程

1. 创建等价性证明链（`euclidean_create_equivalence_chain`），初始化双向翻译映射
2. 构建内部验证约束图（`verification_graph`）
3. 调用 `euclidean_verify_equivalence` 验证两个方向的正确性
4. 验证通过后 `status` 设为 `EQUIV_STATUS_VERIFIED`

---

## 7 EuclideanContext 上下文

### 7.1 结构定义

```c
typedef struct EuclideanContext {
    EuclideanAxiomSystem active_axiom_system; /**< 当前活跃的公理体系 */

    int *registered_points;  /**< 已注册点 ID 数组 */
    int point_count;
    int point_capacity;
    int *registered_lines;   /**< 已注册线 ID 数组 */
    int line_count;
    int line_capacity;
    int *registered_circles; /**< 已注册圆 ID 数组 */
    int circle_count;
    int circle_capacity;

    ConstraintGraph *constraint_graph; /**< 关联的约束图（借引用） */

    uint32_t enabled_axioms_mask; /**< 公理启用位掩码 */

    bool is_consistent;              /**< 当前上下文是否一致 */
    int inconsistency_source;        /**< 导致不一致的公理/谓词 ID */
    char inconsistency_message[256]; /**< 不一致的详细描述 */

    EquivalenceProofChain *equivalence_chain; /**< 等价性证明链 */
} EuclideanContext;
```

### 7.2 生命周期

| 操作 | 说明 |
|------|------|
| 创建 | `euclidean_init(graph)` -- 默认 Hilbert 体系，启用全部公理 |
| 销毁 | `euclidean_destroy(ctx)` -- 释放实体列表和等价性链（不释放外部 ConstraintGraph） |
| 切换体系 | `euclidean_set_axiom_system(ctx, system)` -- 切换时执行一致性检查 |
| 绑定约束图 | `euclidean_bind_graph(ctx, graph)` -- 后续声明和断言作用到此约束图 |

---

## 8 API 列表

### 8.1 初始化与配置

| 函数 | 说明 |
|------|------|
| `euclidean_init(graph)` | 创建欧几里得几何上下文 |
| `euclidean_destroy(ctx)` | 销毁上下文 |
| `euclidean_set_axiom_system(ctx, system)` | 设置活跃公理体系 |
| `euclidean_get_axiom_system(ctx)` | 获取当前公理体系 |
| `euclidean_bind_graph(ctx, graph)` | 绑定约束图 |
| `euclidean_toggle_axiom(ctx, group, axiom_id, enabled)` | 启用/禁用特定公理 |

### 8.2 几何实体声明

| 函数 | 说明 |
|------|------|
| `euclidean_declare_point(ctx, x, y, name)` | 声明一个点 |
| `euclidean_declare_line(ctx, p1_id, p2_id)` | 声明一条直线（两点确定） |
| `euclidean_declare_circle(ctx, center_id, radius)` | 声明一个圆 |

### 8.3 几何谓词断言

| 函数 | 说明 |
|------|------|
| `euclidean_assert_collinear(ctx, point_ids, count)` | 断言一组点共线 |
| `euclidean_assert_between(ctx, a_id, b_id, c_id)` | 断言点 B 在 A 和 C 之间 |
| `euclidean_assert_congruent(ctx, a1, a2, b1, b2)` | 断言两条线段全等 |

### 8.4 定理验证与等价性

| 函数 | 说明 |
|------|------|
| `euclidean_verify_theorem(ctx, proposition, proof_out)` | 验证定理是否成立 |
| `euclidean_check_consistency(ctx)` | 检查公理体系一致性 |
| `euclidean_export_birkhoff(ctx)` | 导出为 Birkhoff 体系约束图 |
| `euclidean_export_tarski(ctx)` | 导出为 Tarski 体系约束图 |
| `euclidean_create_equivalence_chain(ctx)` | 创建等价性证明链 |
| `euclidean_destroy_equivalence_chain(chain)` | 销毁等价性证明链 |
| `euclidean_verify_equivalence(ctx, chain)` | 验证等价性证明链 |

---

## 9 与 Lv-00 自建公理体系的关系

本模块作为 Lv-00 的可插拔公理包存在，与 Lv-00 自建公理体系的关系如下：

1. **独立性**：Lv-00 的核心框架（`ConstraintGraph`、`SymbolicCoord`、求解器等）不依赖本模块。核心框架提供通用的约束表示和求解能力，可服务于任意公理体系。

2. **集成方式**：本模块通过 `ConstraintGraph` 借引用集成，所有几何声明和谓词断言同步到约束图中，利用核心框架的求解能力完成验证。

3. **可替换性**：用户可通过 `euclidean_set_axiom_system()` 在 Birkhoff、Tarski、Hilbert 和自定义体系之间切换，或完全禁用本模块。

4. **等价性保障**：`EquivalenceProofChain` 提供不同公理体系间的翻译映射验证，确保在切换体系时推理结果的一致性。

5. **不套用外来公理系统**：本模块不将 Hilbert/Birkhoff/Tarski 中的任何一套作为 Lv-00 的底层理论根基。这些公理体系仅作为本模块提供的可选推理工具，Lv-00 的理论根基由其自建公理体系独立定义。

---

## 10 实现文件

- **头文件**：`core/include/lv00/euclidean_geometry.h`
- **源文件**：`core/src/layer3_geometry/euclidean_geometry.c`

## 11 依赖

| 依赖模块 | 用途 |
|----------|------|
| `constraint_graph.h` | 约束图核心数据结构 |
| `symbolic_coord.h` | 符号坐标系统 |
| `lv00_utils.h` | 统一内存分配器 |
| `lv00_internal.h` | 内部常量与工具宏 |
| `error_codes.h` | 统一错误码系统 |
| `debug.h` | 调试断言 |

## 12 内部常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `EUCLID_INITIAL_CAPACITY` | 8 | 点/线/圆注册数组初始容量 |
| `EUCLID_EQUIV_TRANSLATION_CAPACITY` | 32 | 等价性证明链翻译映射容量 |
| `EUCLID_COLLINEARITY_EPSILON` | 1e-10 | 共线性验证浮点容差 |
| `EUCLID_CONGRUENCE_TOLERANCE` | 1e-8 | 线段全等验证百分比容差 |
| `EUCLID_DEFAULT_AXIOM_MASK` | 0x003FFFFF | 默认启用全部公理的位掩码 |
