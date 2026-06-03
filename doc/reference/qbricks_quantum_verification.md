# QBricks 量子程序形式化验证借鉴设计

> **借鉴项目**：QBricks
> **来源**：HASLab / INESC TEC / Universidade do Minho（葡萄牙），Mario Pereira 等人开发
> **核心借鉴点**：领域特定操作的形式化建模、WhyML 函数式规约编码、多证明器分派验证、量子态/门/测量的分层抽象
> **分类**：P2 高优先级 / 领域建模与验证架构
> **日期**：2026-05-25

---

## 1. 项目概述

### 1.1 项目简介

QBricks 是由葡萄牙 HASLab / INESC TEC / Universidade do Minho 联合开发的量子程序形式化验证开源环境。该项目基于 Why3 框架构建，使用 WhyML（Why3 的编程语言）作为规约与建模语言，为量子电路和量子算法提供严格的形式化规范与验证支持。QBricks 的核心定位是"量子计算的形式化验证基础设施"，其设计哲学是将量子计算操作（量子态、量子门、量子测量）建模为 WhyML 函数，利用 Why3 的多证明器分派机制进行自动验证。

QBricks 的三个核心技术贡献对 Lv-00 具有直接借鉴价值：

1. **领域特定操作的函数式建模**：QBricks 将量子计算中的基本操作（Hadamard 门、CNOT 门、Pauli 门、量子测量等）编码为 WhyML 函数，每个函数携带精确的前置条件（precondition）和后置条件（postcondition）。这种"领域操作即函数"的建模方式使得量子算法的规约与实现统一在同一个语言框架下。对 Lv-00 而言，几何构造操作（作中点、作垂线、作角平分线等）同样可以采用"几何操作即函数"的建模策略。

2. **分层抽象的量子态建模**：QBricks 对量子态采用分层抽象策略——底层是复数向量表示的量子态振幅，中层是量子门操作的酉变换语义，上层是量子电路的组合语义。每一层都有独立的规约和验证条件，层与层之间通过精化（refinement）关系连接。这种分层建模方法直接适用于 Lv-00 的几何抽象层级：符号坐标层（L1）到几何节点层（L2）到约束求解层（L3）到证明引擎层（L4）。

3. **基于 Why3 的多证明器自动分派**：QBricks 充分利用 Why3 的 Driver 机制，将量子验证任务自动分派到 Alt-Ergo、Z3、CVC5 等多个 SMT 求解器。不同类型的量子性质（酉性保持、概率守恒、纠缠特性）被路由到最擅长处理该类性质的求解器。这与 Lv-00 的多引擎调度架构高度一致。

### 1.2 技术栈

| 维度 | 技术选型 | 说明 |
|:---|:---|:---|
| 建模语言 | WhyML（Why3 编程语言） | 一阶逻辑 + 程序构造的混合语言，支持规约注解 |
| 验证框架 | Why3 | 多证明器分派平台，支持 SMT-LIB 2.0 输出 |
| SMT 求解器 | Alt-Ergo、Z3、CVC5 | 多后端并行验证，结果投票合并 |
| 交互式证明 | Coq（可选后端） | 复杂量子性质的交互式证明支持 |
| 许可证 | LGPL v2.1+ | 弱 copyleft，允许闭源项目链接使用 |
| 编程范式 | 函数式 + 规约驱动 | WhyML 的纯函数语义天然适合量子操作的纯变换建模 |

### 1.3 社区活跃度

QBricks 是学术界的形式化验证项目，主要活跃于量子计算形式化方法的研究社区。项目特点：

- **研究导向**：由 HASLab（高级计算系统实验室）和 INESC TEC（通信系统与信息技术研究所）主导，研究成果发表在量子计算形式化方法相关会议和期刊上。
- **Why3 生态集成**：作为 Why3 框架的领域特定扩展，QBricks 复用了 Why3 的证明器基础设施（IDE、会话管理、证明任务分派），无需独立构建底层验证平台。
- **学术影响力**：量子计算形式化验证是新兴交叉领域，QBricks 为该领域提供了基于成熟验证框架（Why3）的实践路径，其方法论对其他领域（如几何证明）的形式化验证具有通用借鉴价值。

### 1.4 许可证

QBricks 采用 LGPL v2.1+ 许可证。对于 Lv-00 而言，LGPL 的关键约束是：

- **允许链接**：Lv-00 可以链接 QBricks 的库代码而不受 copyleft 传染。
- **修改需开源**：如果 Lv-00 直接修改并分发 QBricks 的源代码，修改部分需以 LGPL 发布。
- **独立开发不受影响**：Lv-00 借鉴 QBricks 的设计思想（而非直接使用其代码）完全不受许可证约束。
- **Why3 本身**：Why3 采用 LGPL，Lv-00 如果直接集成 Why3 作为验证后端，需注意链接方式。

---

## 2. 核心借鉴点

### 2.1 领域特定操作的函数式建模

QBricks 的核心建模策略是将量子计算操作编码为 WhyML 函数。每个量子门操作被建模为一个纯函数，其规约精确描述了该门对量子态的变换语义：

```
(* QBricks 中 Hadamard 门的 WhyML 建模（示意） *)
let hadamard (q: qubit_state) : qubit_state
  requires is_normalized(q)
  ensures  is_normalized(result)
  ensures  unitary_preserved(q, result)
  ensures  probability_conserved(q, result)
= { ... }
```

这种建模方式的关键优势在于：

- **规约与实现统一**：量子门的数学性质（酉性、概率守恒）直接编码在函数的 requires/ensures 中，验证器自动检查实现是否满足规约。
- **组合性**：量子电路由量子门组合而成，电路的规约由门的规约自动推导。
- **可复用性**：已验证的量子门函数可以作为更高层量子算法的构建块。

对 Lv-00 的借鉴意义：几何构造操作（如"作中点"、"作垂线"、"作角平分线"）可以同样建模为携带规约的函数，使得几何定理的验证建立在已验证的构造操作之上。

### 2.2 分层抽象与精化关系

QBricks 对量子态采用三层抽象：

| 层 | 抽象级别 | 内容 | 验证方法 |
|:---|:---|:---|:---|
| 底层 | 量子态振幅 | 复数向量、归一化条件 | SMT 求解器（复数算术） |
| 中层 | 量子门操作 | 酉变换、门矩阵定义 | SMT 求解器（矩阵代数） |
| 上层 | 量子电路 | 门序列组合、电路等价性 | SMT + 交互式证明 |

层与层之间通过**精化关系**（refinement）连接：上层的抽象操作被精化为下层的具体实现，精化关系本身也是一个需要验证的命题。

### 2.3 对照表：QBricks 特性 vs Lv-00 函数块系统/证明引擎

| QBricks 特性 | Lv-00 函数块系统（L2）映射 | Lv-00 证明引擎（L4）映射 | 说明 |
|:---|:---|:---|:---|
| WhyML 量子门函数 | `FuncBlock`（函数块） | `ProofGoal`（证明目标） | 量子门函数对应几何构造函数块 |
| `requires` 前置条件 | `fb->precondition_ids` | 前提命题列表 | 输入约束对应前置几何条件 |
| `ensures` 后置条件 | `fb->postcondition_ids` | 目标命题列表 | 输出保证对应后置命题 |
| 量子态类型 | `GeoNode`（几何节点） | -- | 量子态对应点/线/圆等几何对象 |
| 量子电路组合 | `FuncBlock` 组合子（`fb_compose`） | 复合命题分解 | 电路组合对应函数块组合 |
| 酉性验证 | `fb_verify_unitary` | `proof_verify_block` | 酉性检查对应性质验证 |
| Why3 Driver 分派 | `EngineProfile`（引擎配置） | `engine_driver_select` | 多求解器路由机制完全对应 |
| 证明义务（VC） | 约束图一致性检查 | `proof_generate_vc` | Why3 的 VC 对应验证条件生成 |
| 会话管理 | -- | `ProofSession` | Why3 会话对应证明会话管理 |
| 精化关系 | 函数块实例化（`fb_instantiate`） | 引理应用（`proof_lemma_apply`） | 抽象到具体的精化对应实例化 |
| 量子测量建模 | `FuncBlock`（带概率分支） | 多策略引擎分支 | 测量的概率性对应多策略分支 |

### 2.4 多证明器分派的领域适配

QBricks 利用 Why3 的 Driver 机制，针对不同量子性质选择不同的求解器：

| 量子性质类型 | 首选求解器 | 理由 |
|:---|:---|:---|
| 酉性保持（`U * U^dagger = I`） | Z3 | 矩阵等式推理，Z3 的非线性算术支持强 |
| 概率守恒（归一化） | Alt-Ergo | 线性算术 + 简单非线性，Alt-Ergo 效率高 |
| 纠缠特性（不可分性） | CVC5 | 量词推理能力较强 |
| 电路等价性 | Z3 + CVC5 | 需要组合多个理论（矩阵 + 位向量） |

这种"性质类型到求解器"的映射策略与 Lv-00 的"几何命题特征到求解器"映射完全一致。QBricks 的贡献在于展示了这种策略在量子计算这一特定领域中的成功实践。

---

## 3. Lv-00 映射方案

### 3.1 几何操作的函数式建模

借鉴 QBricks 的"量子门即 WhyML 函数"策略，Lv-00 将几何构造操作建模为携带规约的 C 函数块。每个函数块封装一个几何构造操作，其规约描述该操作的几何语义：

```c
/**
 * @file geom_operations.h
 * @brief 几何构造操作的形式化建模 -- 借鉴 QBricks 的量子门函数式建模
 *
 * QBricks 将量子门建模为 WhyML 函数（requires + ensures），
 * Lv-00 将几何构造建模为 C 函数块（preconditions + postconditions）。
 */
#ifndef LV00_GEOM_OPERATIONS_H
#define LV00_GEOM_OPERATIONS_H

#include "func_block.h"

/* 中点构造函数块 -- 对应 QBricks Hadamard 门 */
typedef struct {
    GeoNodeId point_a;    /**< 输入点 A */
    GeoNodeId point_b;    /**< 输入点 B */
    GeoNodeId result_m;   /**< 输出中点 M */
} GeomMidpointArgs;

static const FuncBlockSpec geom_midpoint_spec = {
    .name = "geom_midpoint",
    .preconditions = {
        "not_equal(A, B)",              /* A != B */
    },
    .postconditions = {
        "collinear(A, M, B)",           /* M 在 AB 上 */
        "distance_eq(A, M, M, B)",      /* |AM| = |MB| */
        "between(M, A, B)",             /* M 在 A、B 之间 */
    },
    .precondition_count = 1,
    .postcondition_count = 3,
};

/* 垂线构造函数块 -- 对应 QBricks CNOT 门 */
typedef struct {
    GeoNodeId point_p;     /**< 输入点 P（不在 L 上） */
    GeoNodeId line_l;      /**< 输入线 L */
    GeoNodeId result_line; /**< 输出垂线 L_perp */
} GeomPerpendicularArgs;

static const FuncBlockSpec geom_perpendicular_spec = {
    .name = "geom_perpendicular",
    .preconditions = {
        "not_on_line(P, L)",
    },
    .postconditions = {
        "on_line(P, L_perp)",
        "perpendicular(L_perp, L)",
        "unique_intersection(L_perp, L, F)",
        "distance_eq(P, F, P, L)",
    },
    .precondition_count = 1,
    .postcondition_count = 4,
};

/* 注册所有内置几何构造函数块 */
void geom_operations_register(FuncBlockRegistry *registry);

#endif /* LV00_GEOM_OPERATIONS_H */
```

### 3.2 函数块组合与验证条件生成

借鉴 QBricks 中量子电路由量子门组合而成、电路规约由门规约自动推导的策略，Lv-00 的复合几何构造由基本函数块组合而成：

```c
/**
 * @brief 中位线定理的函数块组合验证
 *
 * 定理：三角形两边中点的连线平行于第三边，且等于第三边的一半。
 *
 * 函数块组合序列：
 *   Step 1: D = midpoint(A, B)       -- 规约: collinear(A,D,B), |AD|=|DB|
 *   Step 2: E = midpoint(B, C)       -- 规约: collinear(B,E,C), |BE|=|EC|
 *   Step 3: L_DE = line(D, E)        -- 规约: D,E 在 L_DE 上
 *   Goal:   parallel(L_DE, line(A,C))
 */
typedef struct {
    GeoNodeId point_a;
    GeoNodeId point_b;
    GeoNodeId point_c;
    GeoNodeId midpoint_d;      /* Step 1 产出 */
    GeoNodeId midpoint_e;      /* Step 2 产出 */
    GeoNodeId line_de;         /* Step 3 产出 */
} MidlineTheoremComposition;

FuncBlockChain *midline_theorem_build(
    FuncBlockRegistry *registry,
    GeoNodeId a, GeoNodeId b, GeoNodeId c
);

/**
 * @brief 从函数块组合自动生成验证条件
 *
 * 借鉴 QBricks/Why3 的 VCG 机制：
 *   - 遍历函数块链，收集每个块的 postcondition
 *   - 将最终目标命题作为待验证的结论
 *   - 生成 VC: (已知事实的合取) => 目标命题
 */
VCGenResult geom_composition_generate_vc(
    const FuncBlockChain *chain,
    const GvilPredicate *goal,
    VerificationCondition ***out_vcs,
    int *out_count
);
```

### 3.3 分层抽象的几何建模

借鉴 QBricks 的量子态三层抽象，Lv-00 的几何建模同样采用分层策略：

```c
/**
 * @brief 几何建模的分层抽象 -- 借鉴 QBricks 的量子态分层建模
 *
 * QBricks:  复数向量(底层) -> 量子门操作(中层) -> 量子电路(上层)
 * Lv-00:    符号坐标(底层) -> 几何构造操作(中层) -> 几何定理(上层)
 */

/* 底层：符号坐标 -- 对应 QBricks 的复数向量层 */
typedef struct {
    SymExpr x;    /**< x 坐标（符号表达式） */
    SymExpr y;    /**< y 坐标（符号表达式） */
} SymbolicPoint;

AlgebraResult symbolic_verify_relation(
    const SymbolicPoint *p1,
    const SymbolicPoint *p2,
    GeometricRelation relation
);

/* 中层：几何构造操作 -- 对应 QBricks 的量子门操作层 */
typedef struct {
    const char *op_name;
    int input_count;
    int output_count;
    SymExprTransform transform;
    GvilPredicate **invariants;
    int invariant_count;
} GeomOperation;

ProofResult geom_op_verify_property(
    const GeomOperation *op,
    const AlgebraicProperty *property
);

/* 上层：几何定理 -- 对应 QBricks 的量子电路上层 */
typedef struct {
    const char *theorem_name;
    GeomOperation **constructions;
    int construction_count;
    GvilPredicate *goal;
    GvilPredicate **hypotheses;
    int hypothesis_count;
} GeometricTheorem;

ProofResult geometric_theorem_verify(
    const GeometricTheorem *theorem,
    const EngineProfile *profiles,
    int count
);
```

### 3.4 验证条件的多引擎分派

借鉴 QBricks 利用 Why3 Driver 进行多证明器分派的策略，Lv-00 将几何验证条件分派到不同的求解器后端：

```c
/**
 * @brief 几何验证条件的领域分类
 *
 * QBricks:  酉性保持 -> Z3, 概率守恒 -> Alt-Ergo, 纠缠特性 -> CVC5
 * Lv-00:    线性等式 -> Z3, 多项式消元 -> Groebner 基, 共线/共点 -> 面积法
 */
typedef enum {
    GEO_VC_LINEAR_EQ,         /**< 线性等式（如 |AB| = |CD|） */
    GEO_VC_POLYNOMIAL,        /**< 多项式等式（如角度关系） */
    GEO_VC_COLLINEARITY,      /**< 共线性命题 */
    GEO_VC_CONCIRCULARITY,    /**< 共圆性命题 */
    GEO_VC_CONGRUENCE,        /**< 全等/相似命题 */
    GEO_VC_EXISTENTIAL,       /**< 存在性命题 */
} GeoVCType;

GeoVCType geo_vc_classify(const VerificationCondition *vc);
SolverBackend geo_vc_select_solver(GeoVCType vc_type);

ProofResult geo_vc_multi_dispatch(
    const VerificationCondition *vc,
    const EngineProfile *profiles,
    int count
);
```

---

## 4. 实现路线图

### 4.1 短期（1-2 个月）：基础建模层

| 序号 | 任务 | 对应 Lv-00 层 | 借鉴 QBricks 特性 | 产出 |
|:---|:---|:---|:---|:---|
| S-1 | 定义 `FuncBlockSpec` 结构体（规约容器） | L2 函数块系统 | WhyML 函数的 requires/ensures | `func_block.h` |
| S-2 | 实现基本几何构造的规约编码（中点、垂线、平行线、角平分线） | L2 函数块系统 | 量子门函数的规约编码 | `geom_operations.h` |
| S-3 | 实现规约的 GVIL 编码（将自然语言规约转为 GVIL 谓词） | L2 + L3 | WhyML 谓词到 SMT-LIB 的翻译 | `spec_to_gvil.c` |
| S-4 | 实现函数块注册表（注册/查询/枚举） | L2 函数块系统 | Why3 理论库管理 | `func_block_registry.c` |
| S-5 | 编写规约编码的单元测试（5 个基本构造） | L2 | QBricks 量子门规约测试 | `test_geom_spec.c` |

### 4.2 中期（3-5 个月）：组合验证层

| 序号 | 任务 | 对应 Lv-00 层 | 借鉴 QBricks 特性 | 产出 |
|:---|:---|:---|:---|:---|
| M-1 | 实现函数块顺序组合子（`fb_compose_sequence`） | L2 函数块系统 | 量子电路的门序列组合 | `func_block_compose.c` |
| M-2 | 实现函数块并行组合子（`fb_compose_parallel`） | L2 函数块系统 | 多量子比特并行门操作 | `func_block_compose.c` |
| M-3 | 实现组合构造的 VC 自动生成 | L4 证明引擎 | Why3 VCG 从量子电路生成 VC | `geom_composition.c` |
| M-4 | 实现几何 VC 的领域分类（`geo_vc_classify`） | L3 + L4 | QBricks 量子性质分类 | `geom_dispatcher.c` |
| M-5 | 集成到多引擎调度器（`engine_scheduler.h`） | L3 + L4 | Why3 Driver 多证明器分派 | `geom_dispatcher.c` |
| M-6 | 验证经典定理：中位线定理、垂心定理、欧拉线 | L4 | QBricks 量子算法正确性验证 | `test_theorems.c` |

### 4.3 长期（6-12 个月）：完整验证体系

| 序号 | 任务 | 对应 Lv-00 层 | 借鉴 QBricks 特性 | 产出 |
|:---|:---|:---|:---|:---|
| L-1 | 实现分层抽象的完整验证（符号坐标到构造操作到定理） | L1-L4 | QBricks 量子态三层抽象验证 | `geom_abstraction.c` |
| L-2 | 实现精化关系的自动验证（高层定理精化为低层代数证明） | L3 + L4 | QBricks 电路到门矩阵的精化验证 | `refinement_checker.c` |
| L-3 | 构建几何定理库（50+ 经典定理的函数块组合编码） | L2 + L4 | QBricks 量子算法库 | `theorem_library/` |
| L-4 | 实现增量验证（约束图变更后仅重验证受影响的定理） | L3 + L4 | Why3 会话的增量验证 | `incremental_verify.c` |
| L-5 | 实现证明导出（将函数块组合验证结果导出为 Coq 脚本） | L6 数据交换 | QBricks 导出到 Coq 后端 | `export_coq.c` |
| L-6 | 大规模基准测试（100+ 几何题库的端到端验证） | L7 应用框架 | QBricks 量子电路基准测试 | `benchmark/` |

---

## 5. 附录

### 5.1 关键 API 列表

| API 名称 | 所在文件 | 功能 | 借鉴来源 |
|:---|:---|:---|:---|
| `geom_operations_register()` | `geom_operations.h` | 注册所有内置几何构造函数块 | QBricks 量子门库初始化 |
| `geom_midpoint_spec` | `geom_operations.h` | 中点构造的规约定义 | QBricks Hadamard 门规约 |
| `geom_perpendicular_spec` | `geom_operations.h` | 垂线构造的规约定义 | QBricks CNOT 门规约 |
| `midline_theorem_build()` | `geom_composition.h` | 构建中位线定理的函数块组合 | QBricks 量子电路组合 |
| `orthocenter_theorem_build()` | `geom_composition.h` | 构建垂心定理的函数块组合 | QBricks 量子电路组合 |
| `geom_composition_generate_vc()` | `geom_composition.h` | 从函数块组合生成验证条件 | Why3 VCG |
| `func_block_compose()` | `geom_composition.h` | 函数块组合子（顺序/并行/条件） | QBricks 电路组合子 |
| `symbolic_verify_relation()` | `geom_abstraction.h` | 验证符号点的几何关系 | QBricks 底层振幅验证 |
| `geom_op_verify_property()` | `geom_abstraction.h` | 验证几何操作的代数性质 | QBricks 门操作性质验证 |
| `geometric_theorem_verify()` | `geom_abstraction.h` | 验证几何定理（端到端） | QBricks 量子电路验证 |
| `geo_vc_classify()` | `geom_dispatcher.h` | 分析几何 VC 的领域类型 | QBricks 量子性质分类 |
| `geo_vc_select_solver()` | `geom_dispatcher.h` | 根据领域类型选择求解器 | QBricks/Why3 Driver |
| `geo_vc_multi_dispatch()` | `geom_dispatcher.h` | 几何 VC 的多引擎并行分派 | Why3 多证明器分派 |

### 5.2 参考文献

1. **QBricks 项目仓库**：https://github.com/qbricks/qbricks -- QBricks 源代码和文档
2. **HASLab 研究主页**：https://haslab.uminho.pt/ -- 高级计算系统实验室，QBricks 的主要开发机构
3. **Why3 官方文档**：https://why3.lri.fr/doc/ -- Why3 形式化验证平台的官方文档
4. **WhyML 语言参考**：Bobot, Filliatre, Marche, Paskevich. "Why3: Shepherd Your Herd of Provers" (2011) -- Why3/WhyML 的设计论文
5. **量子计算形式化验证综述**：Gay, Nagarajan, Papanikolaou. "Quantum Programming Languages: Survey and Bibliography" (ACM Computing Surveys, 2023) -- 量子程序形式化方法综述
6. **量子电路等价性验证**：Amy, Maslov, Mosca, Roetteler. "A Meet-in-the-Middle Algorithm for Fast Synthesis of Depth-Optimal Quantum Circuits" (IEEE TCAD, 2013) -- 量子电路验证的理论基础
7. **Why3 Driver 机制**：Why3 Manual, Chapter 10: "Drivers" -- Driver 文件的语法和变换规则
8. **形式化验证中的领域特定建模**：Chlipala. "Certified Programming with Dependent Types" (MIT Press, 2013) -- 领域特定形式化建模方法论
9. **几何定理机械化证明**：Chou, Gao, Zhang. "Machine Proofs in Geometry" (World Scientific, 1994) -- 几何定理的机械化证明方法
10. **Cameleer: A Deductive Verification Tool for OCaml**：同一团队（Mario Pereira 等）的 OCaml 演绎验证工具，与 QBricks 共享 Why3 集成经验
11. **本系列相关文档**：
    - `why3_multi_prover_dispatch.md` -- Why3 多求解器分派机制借鉴设计
    - `dafny_layered_verification.md` -- Dafny 三层验证架构借鉴设计
    - `dafny_ensures_verification.md` -- Dafny ensures 规约一体化验证
    - `fstar_refinement_smt.md` -- F* 精化类型 + SMT 混合验证
    - `coq_ltac_proof_engine.md` -- Coq Ltac 证明引擎借鉴设计
    - `isabelle_sledgehammer_integration.md` -- Isabelle Sledgehammer 集成借鉴设计

> **文档维护说明**: 本文档应随 QBricks 借鉴功能的实现进度同步更新。
