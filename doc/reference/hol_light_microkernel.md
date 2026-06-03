# HOL Light 微内核架构与最小化推导系统借鉴设计

> **借鉴项目**：HOL Light（github.com/jrh13/hol-light）
> **核心借鉴点**：约 500 行微内核规则推导架构、OCaml sum type 到 Lv-00 C DSL AST 类型映射、最小化信任计算内核
> **分类**：P1 高优先级 / 证明内核最小化与类型映射
> **日期**：2026-05-24

---

## 1. 概述

HOL Light 是由 John Harrison 开发的经典高阶逻辑证明助手，以其极简设计哲学而闻名。其核心证明内核仅约 500 行 OCaml 代码，包含 10 条原始推理规则和 3 条定义原则，所有复杂证明都通过组合这些原始规则导出。HOL Light 的三个核心理念对 Lv-00 的证明内核设计有根本性启发：

1. **500 行微内核**：信任计算（Trusted Computing Base, TCB）被压缩到最小的可能范围——仅 10 条推理规则 + 3 条定义原则。任何在这个内核之外引入的代码都是不可信的，其输出必须通过内核验证。这种"核小而外围大"的架构与 Lv-00 的"约束图验证核心 + 外围证明导航器"模式形成精确对应。

2. **OCaml sum type 作为项表示**：HOL Light 使用 OCaml 的代数数据类型定义其核心项语言，约 6 个构造子即可表达全部高阶逻辑项。这启发 Lv-00 将几何元语言的抽象语法树（AST）也用类似的精心设计的构造子集合表达。

3. **推导是组合的**：所有 HOL 定理都是对核心推理规则的组合应用。Lv-00 的证明步骤链（`ProofNavigator.steps`）也是基础操作的逐步组合。

---

## 2. 500 行微内核映射到 Lv-00 proof.c verify 核心

### 2.1 HOL Light 的 10 条推理规则

HOL Light 的 10 条原始推理规则（每个约 3-5 行 OCaml）如下：

```
1. REFL        : |- t = t                           （自反性）
2. TRANS       : Γ |- s = t, Δ |- t = u  ⇒  Γ∪Δ |- s = u   （传递性）
3. MK_COMB     : Γ |- f = g, Δ |- x = y  ⇒  Γ∪Δ |- f x = g y （同等组合）
4. ABS         : Γ |- s = t  ⇒  Γ |- (λx. s) = (λx. t)       （λ抽象）
5. BETA        : |- (λx. t) x = t                            （β归约）
6. ASSUME      : {t} |- t                                    （假设引入）
7. EQ_MP       : Γ |- p ⇔ q, Δ |- p  ⇒  Γ∪Δ |- q            （相等消除）
8. DEDUCT_ANTISYM : Γ |- p, Δ |- q  ⇒  (Γ-{q})∪(Δ-{p}) |- p = q  （演绎反对称）
9. INST_TYPE   : Γ[α1,...,αn] |- p[α1,...,αn]  ⇒  Γ[τ1,...,τn] |- p[τ1,...,τn]  （类型实例化）
10. INST       : Γ[x1,...,xn] |- p[x1,...,xn]  ⇒  Γ[t1,...,tn] |- p[t1,...,tn]  （项实例化）
```

3 条定义原则：
```
1. new_type_definition    : 定义新的类型常量
2. new_constant_definition : 定义新的项常量
3. new_axiom              : 引入新公理（需证明一致性）
```

### 2.2 Lv-00 proof.c verify 核心的对应规则

Lv-00 的验证核心（`proof.c` 的 verify 函数）需要类似的最小规则集，但映射到几何元语言领域：

| HOL Light 规则 | Lv-00 proof.c verify 映射 | 说明 |
|:---|:---|:---|
| `REFL` | `VERIFY_IDENTITY`：检查两个约束图是否图同构 | 几何图的自反等价 |
| `TRANS` | `VERIFY_TRANSITIVE`：图A≈图B 且 图B≈图C ⇒ 图A≈图C | 约束图传递闭包 |
| `MK_COMB` | `VERIFY_COMPOSE`：FUN_BLOCK组合后的约束满足性 | 函数块组合验证 |
| `ABS` | `VERIFY_ABSTRACT`：函数块的打包/封装 | 将子图封装为可复用块 |
| `BETA` | `VERIFY_APPLY`：函数块应用到具体参数后的约束传播 | β等价的几何形式 |
| `ASSUME` | `VERIFY_PREMISE`：将给定约束加入上下文 | 几何前提登记 |
| `EQ_MP` | `VERIFY_MODUS_PONENS`：前提满足 ⇒ 结论满足 | 约束推理 |
| `INST_TYPE` | `VERIFY_TYPE_INST`：类型变量替换后验证类型等价 | 类型的约束检查 |
| `INST` | `VERIFY_INSTANTIATE`：坐标变量替换后验证约束保持 | 几何实例化验证 |

### 2.3 proof_minimal_verify() 的核心理念

HOL Light 式的极简验证核心意味着 `proof_minimal_verify()` 只执行最基础的合法性检查，不包含任何搜索、启发式、策略选择。其哲学：

```
"验证是简单的，搜索是复杂的。将复杂性放在搜索（不可信代码）中，
 将信任放在验证（微内核）中。"
 — John Harrison, HOL Light 设计哲学
```

在 Lv-00 中对应：

```
不可信的外围代码（ProofNavigator, ProofMultiStrategy）
    ↓ 输出证明步骤
proof_minimal_verify()（信任内核，约 500 行 C）
    ↓ 逐个验证每个基本规则应用
验证通过 / 验证失败 + 精确的错误位置
```

---

## 3. OCaml sum type 到 Lv-00 C DSL AST 的类型映射

### 3.1 HOL Light 的项语言（OCaml sum type）

```ocaml
type hol_type = Tyvar of string | Tyapp of string * hol_type list

type term =
  | Var of string * hol_type        (* 变量 *)
  | Const of string * hol_type      (* 常量 *)
  | Comb of term * term             (* 函数应用 f x *)
  | Abs of term * term              (* λ抽象 λx. t *)

type thm = Sequent of term list * term  (* 定理 = 假设列表 ⊢ 结论 *)
```

仅 3 个类型定义 + 约 6 个构造子，覆盖整个 HOL 系统。

### 3.2 Lv-00 几何 DSL AST 的类型映射

参照 HOL Light 的极简类型设计，Lv-00 的几何元语言 AST 可以用以下核心类型定义来表达：

```c
/**
 * @brief 几何元语言核心 AST（借鉴 HOL Light 极简设计）
 *
 * 仅 5 种节点类型 + 3 种约束类型，覆盖全部几何构造 DSL。
 * 与 HOL Light 的 3 类型 + 6 构造子保持相同的设计极简主义。
 */

/* === 几何 DSL AST 节点类型 === */
typedef enum {
    GEO_AST_POINT,        /**< 点：对应 HOL Light 的 Var（自由变量） */
    GEO_AST_SEGMENT,      /**< 线段：两个点 + 长度约束 */
    GEO_AST_CIRCLE,       /**< 圆：圆心 + 半径 */
    GEO_AST_FUNC_BLOCK,   /**< 函数块：对应 HOL Light 的 Abs（λ抽象） */
    GEO_AST_APPLY         /**< 函数应用：对应 HOL Light 的 Comb（f x） */
} GeoASTNodeType;

/**
 * @brief 几何 DSL AST 节点
 *
 * 借鉴 HOL Light 的 term 定义：用 5 个构造子覆盖全部几何元语言。
 * 与 HOL Light 的映射：
 *   GEO_AST_POINT       → Var（自由坐标点）
 *   GEO_AST_FUNC_BLOCK  → Abs（将子图封装为 λ 可调用块）
 *   GEO_AST_APPLY       → Comb（函数块应用到具体几何节点）
 */
typedef struct GeoASTNode {
    GeoASTNodeType type;         /**< 节点类型 */
    
    /* 对于 POINT */
    char *point_name;            /**< 点名称（如"A"、"O"） */
    SymbolicCoord *coord;        /**< 符号坐标 */
    
    /* 对于 SEGMENT */
    struct GeoASTNode *seg_p1;   /**< 线段端点1 */
    struct GeoASTNode *seg_p2;   /**< 线段端点2 */
    
    /* 对于 CIRCLE */
    struct GeoASTNode *center;   /**< 圆心 */
    struct GeoASTNode *radius_seg; /**< 半径线段 */
    
    /* 对于 FUNC_BLOCK */
    int input_count;             /**< 输入端口数 */
    int output_count;            /**< 输出端口数 */
    struct GeoASTNode **body;    /**< 函数体：内部约束图的 AST 节点数组 */
    int body_count;              /**< 函数体节点数 */
    
    /* 对于 APPLY */
    struct GeoASTNode *func;     /**< 被应用的函数块 */
    struct GeoASTNode **args;    /**< 实参列表 */
    int arg_count;               /**< 实参数量 */
    
    /* 约束关系（附图拉伸，不改变几何性质） */
    ConstraintType constraint;   /**< 约束类型（INCIDENCE/LENGTH_EQ/ANGLE_EQ等） */
} GeoASTNode;

/**
 * @brief 几何定理（对应 HOL Light 的 thm）
 *
 * HOL Light: thm = Sequent of term list * term  （Γ ⊢ p）
 * Lv-00:     GeoTheorem = 前置约束列表 ⊢ 结论约束
 */
typedef struct {
    ConstraintSpec **premises;   /**< 前提约束（类型化约束规格） */
    int premise_count;           /**< 前提数量 */
    ConstraintSpec *conclusion;  /**< 结论约束 */
    ProofColor color;            /**< 证明颜色 */
} GeoTheorem;
```

### 3.3 类型映射对照表

| HOL Light OCaml sum type | Lv-00 C geo DSL AST | 语义对应 |
|:---|:---|:---|
| `Var of string * hol_type` | `GEO_AST_POINT` + `point_name` + `coord` | 命名的自由坐标点 |
| `Const of string * hol_type` | （嵌入在 `ConstraintType` 中） | 几何常数（如 π） |
| `Comb of term * term` | `GEO_AST_APPLY` + `func` + `args` | 函数块应用到节点 |
| `Abs of term * term` | `GEO_AST_FUNC_BLOCK` + `body` | 将约束子图封装为 λ |
| `Tyvar of string` | `TYPE_KIND_VARIABLE` | 类型变量（多态点/线段） |
| `Tyapp of string * hol_type list` | `TYPE_KIND_POINT/LINE_SEGMENT/REGION/...` | 具体几何类型 |
| `Sequent of term list * term` | `GeoTheorem` = `premises` ⊢ `conclusion` | 几何定理 |

---

## 4. proof_minimal_verify() API 设计

### 4.1 函数声明（追加到 proof.h）

```c
/**
 * @brief 最小化证明验证核心 —— 借鉴 HOL Light 500 行微内核架构
 *
 * 这是 Lv-00 的信任计算基（TCB），仅包含最基本的约束验证规则。
 * 所有外围证明搜索/导航/策略的输出都必须经过此核心验证。
 *
 * 设计原则（引用 HOL Light 哲学）：
 *  - "验证是简单的"：每条规则仅检查约束满足性，不进行任何搜索
 *  - "信任核要小"：此函数及其依赖的合一检查/图规范化是唯一的信任代码
 *  - "外围不可信"：ProofNavigator/ProofMultiStrategy 的代码不被信任，
 *    它们的输出必须能通过 verify 的检查
 *
 * 支持的验证规则（对应 HOL Light 的 10 条推理规则 + 3 条定义原则）：
 *
 *  1. VERIFY_IDENTITY      — 图同构自反性检查
 *  2. VERIFY_TRANSITIVE    — 约束传递闭包
 *  3. VERIFY_COMPOSE       — 函数块组合后约束满足
 *  4. VERIFY_ABSTRACT      — 子图封装为函数块的合法性
 *  5. VERIFY_APPLY         — 函数块应用（β等价几何形式）
 *  6. VERIFY_PREMISE       — 前提注册
 *  7. VERIFY_MODUS_PONENS  — 前提满足 ⇒ 结论
 *  8. VERIFY_TYPE_INST     — 类型变量实例化后验证
 *  9. VERIFY_INSTANTIATE   — 坐标变量替换后验证
 * 10. VERIFY_CONSISTENCY   — 约束一致性（无矛盾的 ⊥ 推导）
 *
 * 三条定义原则：
 *  D1. VERIFY_NEW_TYPE     — 新几何类型常量定义的合法性
 *  D2. VERIFY_NEW_CONSTANT — 新几何常量的合法性
 *  D3. VERIFY_NEW_AXIOM    — 新约束引入的一致性检查
 *
 * @param nav              证明导航器（提供完整的证明步骤链）
 * @param out_error_step   输出：第一个验证失败的步骤索引（-1 = 全部通过）
 * @param out_error_rule   输出：验证失败的规则名称
 * @param out_error_detail 输出：失败原因的详细描述（调用者需用 lv00_free 释放）
 * @return true 全部步骤验证通过，false 某个步骤验证失败
 *
 * @note 此函数仅执行验证，不修改导航器状态
 * @note 验证顺序：从步骤 0 到步骤 n-1，逐条应用对应验证规则
 * @note 时间复杂性：O(n × V)，其中 n 是步骤数，V 是单步验证开销
 *
 * @see hol_light_microkernel.md —— HOL Light 架构参考
 * @see minimal_verifier/README.md —— 独立的极简验证器实现
 */
VerifyResult proof_minimal_verify(
    const ProofNavigator *nav,
    int *out_error_step,
    char **out_error_rule,
    char **out_error_detail
);
```

### 4.2 验证规则与结果类型

```c
/**
 * @brief 验证规则类型（对应 HOL Light 的 10 条推理规则）
 */
typedef enum {
    VERIFY_IDENTITY,       /**< 图同构自反性 */
    VERIFY_TRANSITIVE,     /**< 约束传递闭包 */
    VERIFY_COMPOSE,        /**< 函数块组合约束检查 */
    VERIFY_ABSTRACT,       /**< 子图封装合法性 */
    VERIFY_APPLY,          /**< 函数块实例化（β等价） */
    VERIFY_PREMISE,        /**< 前提注册 */
    VERIFY_MODUS_PONENS,   /**< 约束推理（前提→结论） */
    VERIFY_TYPE_INST,      /**< 类型变量实例化 */
    VERIFY_INSTANTIATE,    /**< 坐标变量实例化 */
    VERIFY_CONSISTENCY,    /**< ⊥ 矛盾检查 */
    /* 三条定义原则（对应 HOL Light 的 3 条 definition principles） */
    VERIFY_NEW_TYPE,       /**< 新类型常量定义 */
    VERIFY_NEW_CONSTANT,   /**< 新几何常量定义 */
    VERIFY_NEW_AXIOM,      /**< 新公理一致性检查 */
    VERIFY_RULE_COUNT      /**< 规则总数 */
} VerifyRuleType;

/**
 * @brief 验证结果
 */
typedef enum {
    VERIFY_PASS,           /**< 全部步骤通过验证 */
    VERIFY_FAIL_RULE,      /**< 某条规则验证失败 */
    VERIFY_FAIL_CONSISTENCY, /**< 约束一致性检查失败（矛盾推导） */
    VERIFY_FAIL_TYPE,      /**< 类型检查失败 */
    VERIFY_ERROR           /**< 内部错误 */
} VerifyResult;
```

### 4.3 Verify 核心的信任边界设计

```
     ┌──────────────────────────────────────┐
     │       不可信区域（Untrusted）         │
     │                                      │
     │  ProofNavigator  - 证明步骤导航     │
     │  ProofMultiStrategy - 多策略搜索    │
     │  proof_guided_fill  - 洞填充建议    │
     │  proof_sledgehammer_dispatch - 调度 │
     │                                      │
     │  这些模块的输出（证明步骤）可能含有  │
     │  逻辑错误或不当的约束推导             │
     └──────────────┬───────────────────────┘
                    │ 证明步骤序列
                    ▼
     ┌──────────────────────────────────────┐
     │       信任计算基（TCB）               │
     │                                      │
     │  proof_minimal_verify()              │
     │    ├─ VERIFY_IDENTITY (图同构)       │
     │    ├─ VERIFY_COMPOSE  (函数组合)     │
     │    ├─ VERIFY_APPLY    (函数应用)     │
     │    ├─ ... (共 13 条规则)             │
     │    └─ proof_unify     (合一检查)     │
     │       normalization   (图规范化)     │
     │                                      │
     │  这 ~500 行 C 代码是 Lv-00 的         │
     │  "真理定义者"                         │
     └──────────────────────────────────────┘
```

---

## 5. 实现路线图

### 5.1 第一阶段：最小验证核心实现（P1-1）

- [ ] 实现 10 条验证规则的独立函数
- [ ] 实现 3 条定义原则的独立函数
- [ ] 组装为 `proof_minimal_verify()` 主函数
- [ ] 验证与 HOL Light 规则集的完备性对应
- [ ] 编写每条规则的单元测试（覆盖合法和非法输入）

### 5.2 第二阶段：GeoAST 类型系统（P1-2）

- [ ] 实现 `GeoASTNode` 的 full type definition
- [ ] 实现 `GeoTheorem` 的构造/销毁
- [ ] 实现 OCaml sum type → C struct 的完整映射
- [ ] 实现 AST 的 pretty-print 和比较函数
- [ ] 编写 AST 单元测试

### 5.3 第三阶段：信任边界集成（P1-3）

- [ ] 在 `proof_navigator_add_step()` 中调用 `proof_minimal_verify()` 作为最后检查
- [ ] 在 `proof_multi_strategy_execute()` 中调用验证核心验证策略输出
- [ ] 在 ProofPanel 中添加"验证全部步骤"按钮
- [ ] 实现验证失败时的精确定位和可视化（高亮失败的约束节点）

### 5.4 第四阶段：独立极简验证器（P1-4）

- [ ] 提取验证核心为独立的 `minimal_verifier` 程序
- [ ] 输入格式：JSON 序列化的证明步骤
- [ ] 输出格式：通过/失败 + 详细诊断
- [ ] 作为 CI/CD 流水线的证明检查步骤
- [ ] 编写独立验证器的使用文档

---

## 6. 设计决策与权衡

### 6.1 C vs OCaml 类型安全性

HOL Light 使用 OCaml 的 sum type 获得编译期完备性检查。Lv-00 使用 C 语言，无法获得同等的类型安全。缓解措施：
- 每个 `GeoASTNode.type` 的 switch-case 必须覆盖所有 case + default 错误
- 使用 `assert()` 宏执行运行时类型不变量检查
- 在 CI 中使用 AddressSanitizer/UndefinedBehaviorSanitizer

### 6.2 TCB 规模的权衡

HOL Light 的 TCB 约 500 行 OCaml（约等于 300 行的高阶逻辑核心规则）。Lv-00 的 TCB 更大：
- 合一检查（`proof_unify`）已经约 200 行
- 图规范化（`normalization.h`）已经约 300 行
- 10 条几何验证规则估计约 400 行

总共约 900 行的 TCB 比 HOL Light 大，但这是因为几何约束图比纯语法项更复杂。这是合理的设计取舍。

### 6.3 提取 verify 为独立可执行文件

参考 HOL Light 的"将验证与搜索分离"哲学，Lv-00 的独立验证器是关键的架构选择：
- 用户可以在浏览器端执行验证（WASM）
- 验证不需要加载完整的引擎和公理包
- 外部工具可以调用验证器作为证明检查器

---

## 7. 补充：HOL Light 的推导规则与 Lv-00 具体映射细节

### 7.1 REFL 规则在约束图中的实现

HOL Light 的 `REFL` 规则断言 `|- t = t`（任何项等于自身）。在 Lv-00 约束图中，这对应的是**图同构自反性**——同一个约束图规范化后必然与自身同构。

```c
// VERIFY_IDENTITY 的实现要点：
bool verify_identity(const ConstraintGraph *g1, const ConstraintGraph *g2) {
    // 1. 分别对 g1 和 g2 执行图规范化
    // 2. 规范化后比较节点集和约束集
    // 3. 若两图完全相同 → 通过（自反性自然成立）
    // 4. 若两图不同但等价（重命名） → 仍需检查可合一性
}
```

### 7.2 TRANS 规则的传递闭包

HOL Light 的 `TRANS` 规则：若 Γ |- s = t 且 Δ |- t = u，则 Γ∪Δ |- s = u。在 Lv-00 约束图中，这对应约束的传递闭包计算：

```c
// VERIFY_TRANSITIVE 的实现要点：
bool verify_transitive(
    const ConstraintGraph *g_ab, const ConstraintGraph *g_bc,
    const ConstraintGraph *g_ac) {
    // 1. 验证 g_ab 与 g_bc 共享中间状态 b 的一致性
    // 2. 构造 g_ab ⊕ g_bc（约束图的传递组合）
    // 3. 规范化组合图
    // 4. 检查规范化结果与 g_ac 是否合一
}
```

### 7.3 定义原则的几何化

HOL Light 的 3 条定义原则在 Lv-00 中对应：

- **new_type_definition**（新类型）：`type_create_point/line_segment/region` 等函数——创建新的几何类型常量，验证其宇宙层级有效性。
- **new_constant_definition**（新常量）：在约束图中注册新的几何常量节点（如定义点 O = (0, 0)），验证其与现有约束的一致性和不存在循环定义。
- **new_axiom**（新公理）：`axiom_package_instantiate` + 一致性检查——引入新约束时验证不会导致矛盾（⊥ 推导）。

---

## 8. 总结

HOL Light 的 500 行微内核设计哲学为 Lv-00 的证明验证核心提供了极简主义的架构参照。将验证逻辑从搜索逻辑中彻底分离——验证核心只执行最基本的规则检查，搜索/策略/导航在外围自由演化——是构建可信证明系统的经典模式。OCaml sum type 到 C DSL AST 的类型映射则为 Lv-00 提供了精心设计的几何元语言抽象语法树，用 5 种节点类型和 3 种约束类型覆盖全部几何构造。每条 HOL Light 规则的几何化实现（REFL→图同构，TRANS→传递闭包，MK_COMB→函数组合）确保了 Lv-00 验证核心的正确性在概念上与 HOL Light 的信任计算基保持同构。独立验证器作为信任边界的最外围防线，确保了 Lv-00 证明的正确性不依赖于任何搜索启发式或策略选择。
