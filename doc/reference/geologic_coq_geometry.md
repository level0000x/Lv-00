# GeoLogic Coq 几何逻辑核心借鉴设计

> **借鉴项目**：GeoLogic（由 Amirhossein Akbari 和 Milad Niqui 在 CWI 荷兰国家数学与计算机科学研究中心开发）
> **核心借鉴点**：Coq 中交互式平面几何逻辑的形式化、构造性几何推理、几何公理的依赖类型编码、几何变换（旋转/平移/对称）的形式化处理、几何直觉与形式化证明的结合
> **分类**：P1 高优先级 / 几何公理形式化与构造性证明引擎
> **日期**：2026-05-25

---

## 1. 项目概述

### 1.1 项目简介

GeoLogic 是一个在 Coq 证明助手中实现的交互式平面几何逻辑系统，由 Amirhossein Akbari 和 Milad Niqui 在荷兰国家数学与计算机科学研究中心（Centrum Wiskunde & Informatica, CWI）开发。该项目的核心目标是将几何直觉与形式化证明有机结合，为平面几何定理提供完全机器可验证的交互式证明环境。

GeoLogic 的技术路线与传统的几何定理证明器（如 GeoGebra 的数值验证、GeoThms 的自动化证明）有本质区别：它不追求完全自动化，而是将人类几何直觉（辅助线、对称性观察、面积法推理）编码为 Coq 中的可复用证明策略，使得用户可以在 Coq 的交互式环境中逐步构建几何证明，每一步都经过 Coq 内核的类型检查。

该项目在形式化几何领域具有独特地位。与 Coq 中已有的几何形式化工作（如 mathlib4 的 EuclideanGeometry 模块、Julien Narboux 的 GeoCoq）相比，GeoLogic 的特色在于：(1) 以"几何逻辑"（Geometry Logic）而非"几何代数"为核心抽象，将几何命题视为逻辑公式而非代数方程；(2) 利用 Coq 的依赖类型系统直接编码几何不变量（如"点 A 在线段 BC 上"被编码为依赖类型 `OnSegment(A)(B)(C)`），使得类型检查器本身成为几何一致性验证器；(3) 提供几何变换（旋转、平移、轴对称）的一等公民形式化，变换的组合构成变换群，其代数性质（如结合律、单位元、逆元）通过 Coq 的类型类机制表达。

### 1.2 技术栈

| 维度 | 详情 |
|:---|:---|
| 实现语言 | Coq（ Gallina 规范语言 + Ltac2 策略语言） |
| 依赖项 | Coq 标准库、MathComp 组件库 |
| 证明方法 | 构造性证明（Constructive Proof）、BHK 解释 |
| 几何基础 | Tarski 公理体系（一阶逻辑，仅点变量）与 Hilbert 公理体系 |
| 核心抽象 | 依赖类型编码几何不变量、类型类编码变换群 |
| 导出格式 | Coq 原生证明项（.v 文件） |
| 开发机构 | CWI（荷兰国家数学与计算机科学研究中心） |

### 1.3 社区活跃度与许可证

GeoLogic 作为学术研究项目，其主要产出形式为学术论文和 Coq 形式化代码库，在形式化数学社区中具有一定影响力。

| 维度 | 评估 |
|:---|:---|
| 论文发表 | 在 FSCD、CPP 等形式化方法与编程语言顶级会议上发表 |
| 代码仓库 | 学术维护，更新频率与论文发表周期同步 |
| 许可证 | 学术许可证（具体条款以仓库为准） |
| 与 Lv-00 关系 | 高度互补——GeoLogic 提供形式化理论框架，Lv-00 提供工程化实现 |

### 1.4 与 Lv-00 的定位对比

两者在"几何形式化"这一共同目标下采取了不同技术路线：GeoLogic 走"几何直觉 → Coq 依赖类型 → 形式化证明项 → Coq 内核验证"的路线，特点是高信任度、高表达力、交互式、学术导向；Lv-00 走"几何直觉 → 约束图 → 符号坐标 → C 语言求解器 → 信任颜色系统"的路线，特点是高性能、可视化、自动化、工程导向。两者的互补性在于：GeoLogic 的形式化框架可以作为 Lv-00 证明导出的"目标格式"（第 6 层数据交换已支持 Coq 导出），而 Lv-00 的自动化求解能力可以弥补 GeoLogic 纯交互式证明的效率不足。

---

## 2. 核心借鉴点

### 2.1 GeoLogic 核心特性一览

GeoLogic 的技术贡献可以归纳为以下五个核心特性：

| 编号 | 特性 | 描述 | 对 Lv-00 的价值 |
|:---|:---|:---|:---|
| G1 | 几何逻辑（Geometry Logic） | 将几何命题视为逻辑公式，支持构造性推理 | 为 Lv-00 第 4 层证明引擎提供逻辑基础 |
| G2 | 依赖类型编码几何不变量 | 利用 Coq 依赖类型直接表达几何约束 | 增强 Lv-00 类型系统的几何语义 |
| G3 | 几何变换的形式化 | 旋转、平移、轴对称作为一等公民，构成变换群 | 完善 Lv-00 geometry_transform.h 的群论基础 |
| G4 | 构造性几何推理 | 基于 BHK 解释的构造性证明，拒绝排中律 | 为 Lv-00 构造性命题提供理论依据 |
| G5 | 多公理体系等价性 | Tarski/Hilbert/Birkhoff 公理体系的等价性证明 | 支持 Lv-00 euclidean_geometry.h 的双公理验证 |

### 2.2 GeoLogic 特性与 Lv-00 证明引擎对照表

以下对照表详细展示 GeoLogic 的每个核心特性在 Lv-00 七层架构中的对应位置和借鉴方式：

| GeoLogic 特性 | GeoLogic 实现方式 | Lv-00 对应模块 | Lv-00 当前状态 | 借鉴方向 |
|:---|:---|:---|:---|:---|
| 几何逻辑（G1） | Coq 归纳类型定义几何命题，Ltac2 策略实现推理规则 | `proof.h` 命题系统（8 种命题类型） | 已实现 8 种命题类型，缺少几何专用推理规则 | 增加几何专用推理规则集 |
| 依赖类型编码（G2） | `OnSegment(A)(B)(C) : Prop`，类型检查即几何验证 | `type_system.h` + `euclidean_geometry.h` | 类型系统支持宇宙层级，几何公理已枚举 | 将几何不变量编码为类型谓词 |
| 几何变换形式化（G3） | 变换群类型类：`IsTransformGroup(G)` | `geometry_transform.h` | 已实现 7 种变换类型和符号计算 | 增加变换群公理验证 |
| 构造性推理（G4） | BHK 解释：证明 = 构造/算法 | `proof.h` ProofColor 系统 | 信任颜色区分构造性/非构造性 | 细化构造性证明的 BHK 语义 |
| 多公理体系等价性（G5） | Tarski/Hilbert/Birkhoff 之间的形式化转换 | `euclidean_geometry.h` | 已定义 4 种公理体系枚举 | 实现公理体系间的自动转换 |
| 面积法推理 | 面积函数作为基本谓词，消去辅助点 | `proof.h` 多策略引擎 | 已声明面积法策略（STRATEGY_DIRECT） | 实现面积法的符号计算 |
| 角度追踪 | 角度等式作为约束，传播角度关系 | `constraint_graph.h` 角度约束 | 约束图支持角度约束类型 | 增加角度传播算法 |
| 辅助线策略 | 用户交互式引入辅助线，Coq 验证合法性 | `interactive_geo.h` | 交互几何支持构造操作 | 增加辅助线的合法性检查 |

### 2.3 几何逻辑（G1）的深入分析

GeoLogic 的"几何逻辑"是其最核心的创新。传统几何定理证明器通常将几何问题转化为代数问题（如吴方法将几何条件转化为多项式方程组），而 GeoLogic 保持了"几何"作为一等逻辑对象的身份。在 GeoLogic 中，一个几何命题（如"三角形内角和等于 180 度"）不是被转化为代数方程，而是被表示为一个逻辑公式，其中的 `Collinear`、`AngleSum` 是几何概念而非代数变量。这种表示方式的优势在于：(1) 保持几何语义；(2) 支持构造性推理，每一步证明都对应一个几何构造操作；(3) 可组合性，几何命题可通过逻辑连接词组合为更复杂的定理。

对 Lv-00 而言，这一思想直接映射到 `proof.h` 中的 `Proposition` 结构体。当前 Lv-00 已支持 8 种命题类型（原子、合取、析取、蕴含、否定、全称、存在、矛盾），但缺少"几何专用"的命题构造器。借鉴 GeoLogic，可以在 `PropositionType` 枚举中增加几何专用类型（详见第 3.2 节）。

### 2.4 依赖类型编码几何不变量（G2）的深入分析

GeoLogic 利用 Coq 的依赖类型系统将几何不变量编码为类型。例如，"点 A 在线段 BC 上"被编码为 `OnSegment(A)(B)(C) : Prop`，"三角形"被编码为包含三个顶点和非退化条件的 `Record Triangle`。这种编码方式的关键优势是：**类型检查器成为几何一致性验证器**。如果用户试图构造一个退化的三角形（三点共线），Coq 的类型检查器会在编译时报错。

Lv-00 的 `type_system.h` 已经支持宇宙层级和依赖类型概念，`euclidean_geometry.h` 也定义了几何公理的枚举。借鉴 GeoLogic 的方法，可以将几何不变量从"运行时约束检查"提升为"编译时类型检查"（详见第 3.3 节）。

### 2.5 几何变换的形式化（G3）的深入分析

GeoLogic 将几何变换（旋转、平移、轴对称）形式化为 Coq 中的代数结构，并验证其构成群：通过类型类 `Isometry` 表达等距性（保持距离），通过 `TransformGroup` 类型类表达群公理（结合律、单位元、逆元、封闭性）。Lv-00 的 `geometry_transform.h` 已经实现了 7 种变换类型（恒等、平移、旋转、轴对称、缩放、粘合、反演）和变换序列复合。借鉴 GeoLogic，可以增加变换群公理的验证功能（详见第 3.4 节）。

---

## 3. Lv-00 映射方案

### 3.1 总体映射架构

GeoLogic 的核心思想在 Lv-00 七层架构中的映射关系如下：

```
GeoLogic                          Lv-00 七层架构
+------------------+              +------------------+
| 几何逻辑（G1）   |  ─────────> | 第 4 层：证明引擎 |
| 命题类型定义     |              | proof.h           |
+------------------+              +------------------+
| 依赖类型编码（G2）|  ─────────> | 第 3 层：算法引擎 |
| 几何不变量       |              | type_system.h     |
+------------------+              +------------------+
| 几何变换形式化（G3）| ────────> | 第 2 层：建模数据 |
| 变换群           |              | geometry_transform.h |
+------------------+              +------------------+
| 构造性推理（G4） |  ─────────> | 第 4 层：证明引擎 |
| BHK 解释        |              | proof.h ProofColor |
+------------------+              +------------------+
| 多公理体系（G5） |  ─────────> | 第 2 层：建模数据 |
| 等价性证明       |              | euclidean_geometry.h |
+------------------+              +------------------+
```

### 3.2 几何命题类型的扩展（G1 映射）

在 `proof.h` 的 `PropositionType` 枚举中增加几何专用命题类型：

```c
/**
 * @brief 扩展命题类型 —— 借鉴 GeoLogic 的几何逻辑
 *
 * 在原有 8 种逻辑命题类型基础上，增加几何专用命题类型。
 * GeoLogic 将几何命题视为逻辑公式，保持几何语义而非转化为代数方程。
 */
typedef enum {
    /* ---- 原有逻辑命题类型 ---- */
    PROPOSITION_ATOMIC,      /* 原子命题 */
    PROPOSITION_CONJUNCTION, /* 合取 */
    PROPOSITION_DISJUNCTION, /* 析取 */
    PROPOSITION_IMPLICATION, /* 蕴含 */
    PROPOSITION_NEGATION,    /* 否定 */
    PROPOSITION_UNIVERSAL,   /* 全称 */
    PROPOSITION_EXISTENTIAL, /* 存在 */
    PROPOSITION_BOTTOM,      /* 矛盾 */

    /* ---- GeoLogic 借鉴：几何专用命题类型 ---- */
    PROPOSITION_COLLINEAR,       /* 三点共线 */
    PROPOSITION_CONCURRENT,      /* 三线共点 */
    PROPOSITION_PARALLEL,        /* 两线平行 */
    PROPOSITION_PERPENDICULAR,   /* 两线垂直 */
    PROPOSITION_ON_SEGMENT,      /* 点在线段上 */
    PROPOSITION_ON_CIRCLE,       /* 点在圆上 */
    PROPOSITION_EQUIDISTANT,     /* 等距（到定点等距） */
    PROPOSITION_ANGLE_EQUAL,     /* 两角相等 */
    PROPOSITION_ANGLE_SUM,       /* 角度和（如三角形内角和） */
    PROPOSITION_AREA_EQUAL,      /* 面积相等（面积法核心） */
    PROPOSITION_RATIO_EQUAL,     /* 比例相等（相似三角形） */
    PROPOSITION_CONGRUENT_TRI,   /* 三角形全等 */
    PROPOSITION_SIMILAR_TRI,     /* 三角形相似 */
    PROPOSITION_MIDPOINT,        /* 中点性质 */
    PROPOSITION_BISECTOR,        /* 角平分线性质 */
    PROPOSITION_CIRCLE_TANGENT   /* 圆的切线性质 */
} PropositionType;
```

### 3.3 几何不变量的类型编码（G2 映射）

借鉴 GeoLogic 的依赖类型编码方法，在 Lv-00 的类型系统中增加几何不变量谓词：

```c
/**
 * @file geo_invariant_type.h
 * @brief 几何不变量的类型编码 —— 借鉴 GeoLogic 的依赖类型方法
 *
 * GeoLogic 将几何不变量编码为 Coq 依赖类型，使得类型检查器本身
 * 成为几何一致性验证器。Lv-00 通过类型谓词系统实现类似功能：
 * 几何不变量被编码为类型谓词，在命题创建时自动验证。
 */

#ifndef LV00_GEO_INVARIANT_TYPE_H
#define LV00_GEO_INVARIANT_TYPE_H

#include "geometry_types.h"
#include "type_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 几何不变量谓词类型
 *
 * 借鉴 GeoLogic 的依赖类型编码：
 *   - GeoLogic: OnSegment(A)(B)(C) : Prop  (Coq 依赖类型)
 *   - Lv-00:   GEO_INVARIANT_ON_SEGMENT  (类型谓词枚举)
 */
typedef enum {
    GEO_INV_NON_DEGENERATE,    /* 非退化：三点不共线 */
    GEO_INV_ON_SEGMENT,        /* 点在线段上 */
    GEO_INV_ON_RAY,            /* 点在射线上 */
    GEO_INV_ON_LINE,           /* 点在直线上 */
    GEO_INV_ON_CIRCLE,         /* 点在圆上 */
    GEO_INV_EQUIDISTANT,       /* 等距 */
    GEO_INV_BETWEEN,           /* 介于性 */
    GEO_INV_SAME_SIDE,         /* 同侧 */
    GEO_INV_CONGRUENT_SEG,     /* 线段全等 */
    GEO_INV_CONGRUENT_ANGLE,   /* 角度全等 */
    GEO_INV_PARALLEL,          /* 平行 */
    GEO_INV_PERPENDICULAR,     /* 垂直 */
    GEO_INV_COCIRCULAR,        /* 四点共圆 */
    GEO_INV_HARMONIC,          /* 调和共轭 */
    GEO_INV_TANGENT            /* 相切 */
} GeoInvariantKind;

/**
 * @brief 几何不变量实例
 *
 * 表示一个具体的几何不变量约束，绑定到具体的几何实体上。
 * 对应 GeoLogic 中依赖类型的实例化。
 */
typedef struct GeoInvariant {
    GeoInvariantKind kind;     /* 不变量类型 */
    int entity_ids[8];         /* 涉及的几何实体ID */
    int entity_count;          /* 实体数量 */
    TrustColor trust;          /* 信任颜色 */
    int proof_step_id;         /* 建立此不变量的证明步骤ID */
    char *justification;       /* 人类可读的理由说明 */
} GeoInvariant;

/**
 * @brief 创建几何不变量
 *
 * 借鉴 GeoLogic 的依赖类型检查：创建不变量时自动验证其合法性。
 * 例如，创建 ON_SEGMENT(A, B, C) 时，自动检查 B 是否确实在 A 和 C 之间。
 *
 * @param kind       不变量类型
 * @param entity_ids 涉及的实体ID数组
 * @param entity_count 实体数量
 * @param trust      信任颜色
 * @return 新创建的几何不变量，或 NULL（验证失败时）
 */
GeoInvariant *geo_invariant_create(
    GeoInvariantKind kind,
    const int *entity_ids,
    int entity_count,
    TrustColor trust
);

/**
 * @brief 验证几何不变量的一致性
 *
 * 检查一组不变量之间是否存在矛盾。
 * 对应 GeoLogic 中 Coq 类型检查器的矛盾检测功能。
 *
 * @param invariants  不变量数组
 * @param count       不变量数量
 * @param error_msg   错误信息输出缓冲区
 * @param error_size  错误信息缓冲区大小
 * @return true 表示一致，false 表示存在矛盾
 */
bool geo_invariant_check_consistency(
    const GeoInvariant **invariants,
    int count,
    char *error_msg,
    size_t error_size
);

/**
 * @brief 将几何不变量附加到类型区域
 *
 * 将几何不变量编码为类型谓词，附加到 Lv-00 的 TypeRegion 上。
 * 这样，类型系统在检查类型一致性时，同时检查几何不变量的一致性。
 *
 * @param region     目标类型区域
 * @param invariant  要附加的几何不变量
 * @return 0 成功，非零错误码
 */
int geo_invariant_attach_to_type(
    TypeRegion *region,
    const GeoInvariant *invariant
);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_INVARIANT_TYPE_H */
```

### 3.4 变换群公理验证（G3 映射）

在 `geometry_transform.h` 基础上增加变换群公理验证：

```c
/**
 * @brief 变换群验证结果
 */
typedef enum {
    TRANSFORM_GROUP_VALID,           /* 变换群公理全部满足 */
    TRANSFORM_GROUP_NOT_CLOSED,      /* 不满足封闭性 */
    TRANSFORM_GROUP_NO_IDENTITY,     /* 缺少单位元 */
    TRANSFORM_GROUP_NO_INVERSE,      /* 缺少逆元 */
    TRANSFORM_GROUP_NOT_ASSOCIATIVE, /* 不满足结合律 */
    TRANSFORM_GROUP_NOT_ISOMETRY     /* 不满足等距性 */
} TransformGroupStatus;

/**
 * @brief 验证变换集合是否构成变换群
 *
 * 借鉴 GeoLogic 的 TransformGroup 类型类：
 *   - 封闭性：任意两个变换的复合仍在集合中
 *   - 单位元：存在恒等变换
 *   - 逆元：每个变换都有逆变换
 *   - 结合律：变换复合满足结合律
 *   - 等距性：每个变换保持距离
 *
 * @param transforms  变换数组
 * @param count       变换数量
 * @param tolerance   数值容差（用于等距性验证）
 * @return 验证结果
 */
TransformGroupStatus transform_verify_group(
    const Lv00Transform **transforms,
    int count,
    double tolerance
);

/**
 * @brief 验证变换的等距性
 *
 * 检查变换是否保持所有点对之间的距离。
 * 对应 GeoLogic 中的 Isometry 类型类。
 *
 * @param transform  要验证的变换
 * @param test_points 测试点对数组（每两个连续点构成一对）
 * @param point_count 测试点数量（必须为偶数）
 * @param tolerance   数值容差
 * @return true 表示等距
 */
bool transform_verify_isometry(
    const Lv00Transform *transform,
    const SymbolicCoord *test_points,
    int point_count,
    double tolerance
);
```

### 3.5 构造性证明的 BHK 语义（G4 映射）

借鉴 GeoLogic 的构造性推理方法，增强 Lv-00 的 ProofColor 系统：

```c
/**
 * @brief 构造性证明的 BHK 解释验证
 *
 * BHK 解释（Brouwer-Heyting-Kolmogorov）是构造性逻辑的语义基础：
 *   - 命题 A 的证明是一个构造（construction）
 *   - A ∧ B 的证明是 A 的证明和 B 的证明的对
 *   - A ∨ B 的证明是 A 的证明或 B 的证明，附带选择信息
 *   - A → B 的证明是一个将 A 的证明转化为 B 的证明的函数
 *   - ⊥ 的证明不存在（不存在性）
 *   - ∃x. P(x) 的证明是一个具体的 x 和 P(x) 的证明
 *
 * GeoLogic 在 Coq 中利用 BHK 解释确保几何证明的构造性。
 * Lv-00 通过 ProofColor 系统和证明步骤追踪实现类似功能。
 */

/**
 * @brief 验证证明步骤的构造性
 *
 * 检查证明导航器中的每个步骤是否满足 BHK 构造性标准：
 *   - PROOF_STEP_ADD_NODE：构造性（创建了几何实体）
 *   - PROOF_STEP_ADD_CONSTRAINT：构造性（添加了几何约束）
 *   - PROOF_STEP_REWRITE：构造性（等式变换）
 *   - PROOF_STEP_EX_FALSO：非构造性（爆炸原理）
 *   - PROOF_STEP_ORACLE：非构造性（外部 Oracle）
 *
 * @param navigator  证明导航器
 * @param step_index 要验证的步骤索引
 * @return true 表示该步骤是构造性的
 */
bool proof_verify_constructive_step(
    const ProofNavigator *navigator,
    int step_index
);

/**
 * @brief 计算证明的构造性比例
 *
 * 统计证明中构造性步骤与非构造性步骤的比例。
 * 对应 GeoLogic 中对证明"构造性程度"的量化评估。
 *
 * @param navigator  证明导航器
 * @return 构造性步骤占总步骤的比例 [0.0, 1.0]
 */
double proof_compute_constructiveness_ratio(
    const ProofNavigator *navigator
);

/**
 * @brief 将非构造性步骤转换为构造性替代方案
 *
 * 尝试将爆炸原理（Ex Falso）或 Oracle 依赖替换为构造性证明。
 * 这是 GeoLogic 中"构造性化"（Constructivization）思想的工程实现。
 *
 * @param navigator  证明导航器
 * @param step_index 非构造性步骤的索引
 * @param max_depth  搜索最大深度
 * @return 替代方案的数量，0 表示无法替换
 */
int proof_constructivize_step(
    ProofNavigator *navigator,
    int step_index,
    int max_depth
);
```

### 3.6 公理体系等价性验证（G5 映射）

在 `euclidean_geometry.h` 基础上实现公理体系间的自动转换：

```c
/**
 * @brief 公理体系等价性验证器
 *
 * 借鉴 GeoLogic 的多公理体系等价性证明：
 *   - Tarski 公理体系（11 条公理 + 连续性公理模式）
 *   - Hilbert 公理体系（5 组 20 条公理）
 *   - Birkhoff 公理体系（4 条公理，依赖实数完备性）
 *
 * 验证两个公理体系之间的等价性需要证明：
 *   1. 体系 A 的每条公理都可以从体系 B 推导
 *   2. 体系 B 的每条公理都可以从体系 A 推导
 */

/**
 * @brief 公理转换方向
 */
typedef enum {
    AXIOM_CONV_TARSKI_TO_HILBERT,   /* Tarski → Hilbert */
    AXIOM_CONV_HILBERT_TO_TARSKI,   /* Hilbert → Tarski */
    AXIOM_CONV_TARSKI_TO_BIRKHOFF,  /* Tarski → Birkhoff */
    AXIOM_CONV_BIRKHOFF_TO_TARSKI,  /* Birkhoff → Tarski */
    AXIOM_CONV_HILBERT_TO_BIRKHOFF, /* Hilbert → Birkhoff */
    AXIOM_CONV_BIRKHOFF_TO_HILBERT  /* Birkhoff → Hilbert */
} AxiomConversionDirection;

/**
 * @brief 公理转换结果
 */
typedef struct {
    bool success;              /* 转换是否成功 */
    int steps_used;            /* 使用的证明步骤数 */
    int lemmas_used;           /* 引用的引理数量 */
    char **lemma_names;        /* 引用的引理名称数组 */
    int lemma_count;           /* 引理数量 */
    char *proof_trace;         /* 证明轨迹（人类可读） */
    TrustColor final_color;    /* 最终信任颜色 */
} AxiomConversionResult;

/**
 * @brief 尝试将一条公理从一个体系转换到另一个体系
 *
 * @param source_system  源公理体系
 * @param target_system  目标公理体系
 * @param axiom_name     要转换的公理名称
 * @param engine         Lv-00 引擎（用于访问已加载的公理包）
 * @return 转换结果
 */
AxiomConversionResult axiom_convert(
    EuclideanAxiomSystem source_system,
    EuclideanAxiomSystem target_system,
    const char *axiom_name,
    LV00Engine *engine
);
```

---

## 4. 实现路线图

### 4.1 短期目标（1-2 个月）

| 编号 | 任务 | 涉及模块 | 优先级 | 预期产出 |
|:---|:---|:---|:---|:---|
| S1 | 扩展 PropositionType 枚举，增加 16 种几何专用命题类型 | `proof.h` | P0 | 几何命题类型系统 |
| S2 | 实现 GeoInvariant 类型，支持 14 种几何不变量谓词 | 新建 `geo_invariant_type.h` | P0 | 几何不变量类型编码 |
| S3 | 实现几何不变量的一致性检查 | `geo_invariant_type.h` | P1 | 矛盾检测功能 |
| S4 | 在 ProofNavigator 中增加构造性比例统计 | `proof.h` | P1 | BHK 构造性度量 |
| S5 | 编写几何命题类型的单元测试 | `tests/` | P1 | 测试覆盖率 > 80% |

### 4.2 中期目标（3-6 个月）

| 编号 | 任务 | 涉及模块 | 优先级 | 预期产出 |
|:---|:---|:---|:---|:---|
| M1 | 实现变换群公理验证（封闭性、单位元、逆元、结合律） | `geometry_transform.h` | P1 | 变换群验证器 |
| M2 | 实现等距性验证（距离保持性检查） | `geometry_transform.h` | P1 | 等距性检查器 |
| M3 | 实现面积法推理策略（符号面积计算 + 消去辅助点） | `proof_engine_enhanced.h` | P1 | 面积法策略 |
| M4 | 实现角度追踪传播算法 | `constraint_graph.h` | P2 | 角度约束传播 |
| M5 | 实现辅助线合法性检查 | `interactive_geo.h` | P2 | 辅助线验证器 |
| M6 | 实现 Tarski → Hilbert 公理转换（核心 5 条公理） | `euclidean_geometry.h` | P2 | 公理转换器原型 |

### 4.3 长期目标（6-12 个月）

| 编号 | 任务 | 涉及模块 | 优先级 | 预期产出 |
|:---|:---|:---|:---|:---|
| L1 | 完成全部 6 个方向的公理体系等价性验证 | `euclidean_geometry.h` | P2 | 完整公理等价性验证器 |
| L2 | 实现 Coq 证明导出（将 Lv-00 证明步骤翻译为 Coq 证明项） | 第 6 层数据交换 | P2 | Coq 导出后端 |
| L3 | 实现非构造性步骤的自动构造性化 | `proof_engine_enhanced.h` | P3 | 构造性化引擎 |
| L4 | 集成 GeoLogic 风格的交互式几何证明工作流 | 第 5 层 + 第 7 层 | P3 | 交互式几何证明 IDE |
| L5 | 建立几何定理形式化证明库（50+ 经典定理） | `axiom_packages/` | P3 | 几何定理证明库 |

### 4.4 里程碑与验收标准

| 里程碑 | 时间节点 | 验收标准 |
|:---|:---|:---|
| M1: 几何命题类型系统 | 第 1 个月末 | 16 种几何命题类型全部实现，通过单元测试 |
| M2: 几何不变量编码 | 第 2 个月末 | 14 种不变量谓词实现，一致性检查通过基准测试 |
| M3: 变换群验证 | 第 4 个月末 | 旋转群、平移群、对称群的群公理验证通过 |
| M4: 面积法策略 | 第 5 个月末 | 至少 10 个经典定理可通过面积法自动证明 |
| M5: 公理等价性 | 第 8 个月末 | Tarski-Hilbert 核心公理的双向转换通过 |
| M6: Coq 导出 | 第 10 个月末 | Lv-00 证明可导出为 Coq .v 文件并通过 coqc 编译 |

---

## 5. 附录

### 5.1 GeoLogic 关键 API 列表（伪 Coq 代码）

以下列出 GeoLogic 中对 Lv-00 借鉴最有价值的核心 API，以伪 Coq 代码表示：

```
(* ---- 几何基础类型 ---- *)
Point : Type                                    (* 几何点 *)
Line : Type                                     (* 几何直线 *)
Circle : Type                                   (* 几何圆 *)

(* ---- 几何谓词（Prop） ---- *)
Collinear : Point -> Point -> Point -> Prop     (* 三点共线 *)
Between : Point -> Point -> Point -> Prop       (* 介于性 *)
Congruent : Point -> Point -> Point -> Point -> Prop  (* 线段全等 AB=CD *)
Perpendicular : Line -> Line -> Prop            (* 两线垂直 *)
Parallel : Line -> Line -> Prop                 (* 两线平行 *)
OnCircle : Point -> Circle -> Prop              (* 点在圆上 *)
Tangent : Line -> Circle -> Prop                (* 直线与圆相切 *)

(* ---- 几何构造（Point 类型，构造性） ---- *)
Midpoint : Point -> Point -> Point              (* 中点构造 *)
Intersection : Line -> Line -> Point            (* 直线交点 *)
Foot : Point -> Line -> Point                   (* 垂足构造 *)
Reflect : Point -> Line -> Point                (* 关于直线的对称点 *)
Rotate : Point -> Point -> Angle -> Point       (* 旋转构造 *)

(* ---- 几何变换 ---- *)
Isometry : (Point -> Point) -> Prop             (* 等距变换谓词 *)
Rotation : Point -> Angle -> Point -> Point     (* 旋转变换函数 *)
Translation : Vec -> Point -> Point             (* 平移变换函数 *)
Reflection : Line -> Point -> Point             (* 轴对称变换函数 *)
Compose : (Point -> Point) -> (Point -> Point) -> Point -> Point  (* 变换复合 *)

(* ---- 变换群 ---- *)
IsTransformGroup : Type -> Prop                 (* 变换群类型类 *)
EuclideanGroup : Type                           (* 欧几里得变换群 *)

(* ---- 证明策略 ---- *)
geo_area_method : Tactic                        (* 面积法策略 *)
geo_angle_chase : Tactic                        (* 角度追踪策略 *)
geo_aux_line : Tactic                           (* 辅助线策略 *)
geo_transform_apply : Tactic                    (* 变换应用策略 *)
```

### 5.2 Lv-00 对应 API 映射

| GeoLogic API | Lv-00 对应 API | 所在头文件 | 状态 |
|:---|:---|:---|:---|
| `Collinear(A)(B)(C)` | `PROPOSITION_COLLINEAR` | `proof.h` | 规划中 |
| `Between(A)(B)(C)` | `GEO_INV_BETWEEN` | 规划 `geo_invariant_type.h` | 规划中 |
| `Congruent(A)(B)(C)(D)` | `PROPOSITION_CONGRUENT_TRI` | `proof.h` | 规划中 |
| `Perpendicular(l1)(l2)` | `PROPOSITION_PERPENDICULAR` | `proof.h` | 规划中 |
| `Parallel(l1)(l2)` | `PROPOSITION_PARALLEL` | `proof.h` | 规划中 |
| `Midpoint(A)(B)` | `GEO_STEP_MIDPOINT` | `geo_spec.h` | 已实现 |
| `Intersection(l1)(l2)` | `GEO_STEP_INTERSECTION` | `geo_spec.h` | 已实现 |
| `Foot(P)(l)` | `GEO_STEP_PERPENDICULAR` | `geo_spec.h` | 已实现 |
| `Reflect(P)(l)` | `TRANSFORM_REFLECTION` | `geometry_transform.h` | 已实现 |
| `Rotate(O)(A)(theta)` | `TRANSFORM_ROTATION` | `geometry_transform.h` | 已实现 |
| `Translation(v)` | `TRANSFORM_TRANSLATION` | `geometry_transform.h` | 已实现 |
| `Isometry(f)` | `transform_verify_isometry()` | `geometry_transform.h` | 规划中 |
| `IsTransformGroup(G)` | `transform_verify_group()` | `geometry_transform.h` | 规划中 |
| `geo_area_method` | `STRATEGY_DIRECT`（面积法变体） | `proof_engine_enhanced.h` | 部分实现 |
| `geo_angle_chase` | 角度传播算法 | `constraint_graph.h` | 规划中 |

### 5.3 参考文献

1. **Akbari, A. and Niqui, M.** "GeoLogic: Formalizing Geometry in Coq." CWI, Amsterdam, Netherlands. 核心论文，描述了在 Coq 中形式化平面几何逻辑的方法论。

2. **Tarski, A.** "What is Elementary Geometry?" In: Henkin, L., Suppes, P., Tarski, A. (eds.) The Axiomatic Method. North-Holland, 1959. Tarski 公理体系的原始文献，GeoLogic 和 Lv-00 共同的理论基础。

3. **Hilbert, D.** "Grundlagen der Geometrie" (Foundations of Geometry). Teubner, 1899. Hilbert 公理体系的原始文献，Lv-00 `euclidean_geometry.h` 的默认公理体系。

4. **Birkhoff, G.D.** "A Set of Postulates for the Foundation of Real Geometry." Annals of Mathematics, 33(2):329-334, 1932. Birkhoff 公理体系，基于实数量度和角度。

5. **Narboux, J.** "Mechanical Theorem Proving in Tarski's Geometry." In: Automated Deduction in Geometry (ADG 2006). LNCS 4869, Springer. GeoCoq 项目的基础，与 GeoLogic 互补的 Coq 几何形式化工作。

6. **Chou, S.-C., Gao, X.-S., and Zhang, J.-Z.** "Machine Proofs in Geometry: Automated Production of Readable Proofs for Geometric Theorems." World Scientific, 1994. 面积法和全角法的基础文献，GeoLogic 面积法推理的理论来源。

7. **Brouwer, L.E.J.** "Intuitionism." In: Collected Works, Vol. 1. North-Holland, 1975. BHK 解释的哲学基础，GeoLogic 构造性推理的理论依据。

8. **Coquand, T. and Huet, G.** "The Calculus of Constructions." Information and Computation, 76(2-3):95-120, 1988. Coq 底层的归纳构造演算（CIC），GeoLogic 的元理论框架。

9. **Mahboubi, A. and Tassi, E.** "Mathematical Components." Zenodo, 2021. MathComp 组件库文档，GeoLogic 依赖的形式化数学库。

10. **Lv-00 项目组.** "Lv-00 系统描述文档." `docs/reports/Lv-00_系统描述文档.md`, 2026. Lv-00 系统的完整架构描述，本文档的映射基础。

### 5.4 术语对照表

| 英文术语 | 中文翻译 | GeoLogic 语境含义 |
|:---|:---|:---|
| Geometry Logic | 几何逻辑 | 将几何命题视为逻辑公式的一阶理论 |
| Constructive Proof | 构造性证明 | 基于 BHK 解释，证明即构造/算法 |
| Dependent Type | 依赖类型 | 类型参数依赖于值的类型系统特性 |
| Type Class | 类型类 | Coq 中实现代数结构的机制 |
| Isometry | 等距变换 | 保持点间距离不变的几何变换 |
| Transform Group | 变换群 | 等距变换在复合运算下构成的群 |
| Betweenness | 介于性 | 描述点在线段上的位置关系 |
| Congruence | 全等 | 线段或角度的相等关系 |
| Area Method | 面积法 | 利用有向面积消去辅助点的证明方法 |
| Angle Chasing | 角度追踪 | 通过角度等式传播推导角度关系的证明方法 |
| Auxiliary Line | 辅助线 | 证明中额外引入的构造线 |
| BHK Interpretation | BHK 解释 | Brouwer-Heyting-Kolmogorov 构造性语义 |
| TCB | 信任计算基 | 系统中必须被信任的最小代码集合 |
| Axiom System | 公理体系 | 一组自洽的公理集合，定义几何理论的基础 |
| Equivalence of Axiom Systems | 公理体系等价性 | 两个公理体系可以互相推导 |
