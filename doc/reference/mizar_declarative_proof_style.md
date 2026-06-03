# Mizar 声明式证明风格 核心借鉴设计

> **借鉴项目**：Mizar（github.com/MizarProject/system）
> **核心借鉴点**：声明式证明风格、自然语言证明构造（consider/let/assume/then）、"语义集合+语法类型"双层抽象、MML 大规模数学库组织
> **分类**：P2 高优先级 / 证明可读性与输出格式
> **日期**：2026-05-24

---

## 1. 概述

Mizar 是世界上最早的数学证明形式化系统之一，由 Andrzej Trybulec 于 1973 年创立，目前在波兰 Bialystok 大学、日本信州大学和加拿大 Alberta 大学共同维护。Mizar 的核心哲学是"可读性优先"——Mizar 的证明语言被设计得尽可能接近数学家在实际论文中撰写证明的方式，这与 Lv-00"证明输出可供人阅读和审核"的设计目标高度一致。

Mizar 最独特的创新在于**声明式证明风格（Declarative Proof Style）**。与 Coq/Lean/Isabelle(Ltac-style) 的过程式证明不同，Mizar 的证明由一系列"声明语句"组成：`consider x such that P(x);`（考虑满足 P 的 x）、`let A be Point;`（设 A 为点）、`assume P;`（假设 P）、`then Q;`（则有 Q）、`thus thesis;`（因此得证）。这种声明式风格天然对应几何证明中"已知-设-求证"的三步法，使 Lv-00 能够将内部符号计算步骤"翻译"为人类可读的几何证明文本。

Mizar 的另一个关键设计是**"语义集合+语法类型"双层抽象**。在 Mizar 中，所有数学对象底层属于某个语义集合（如 `Element of REAL`），上层通过语法类型（`mode`、`struct`）来区分不同范畴的对象。这与 Lv-00 中"所有几何对象底层是坐标集合 + 约束条件，上层通过 TypeRegion 的语法类型（Point/Line/Circle）区分"的架构精确对应。这种分离使得 Lv-00 能够在保持底层统一坐标表示的同时，为上层提供语义丰富的类型语法。

Mizar Mathematical Library（MML）是目前世界上最大的形式化数学库，包含超过 1400 篇文章、60000 多个定理和 12000 多个定义。MML 的组织方式——按 article 分篇、引入环境声明（environ）、支持跨文章引用——为 Lv-00 的公理包生态系统提供了可借鉴的库组织范本。

---

## 2. 声明式 vs 过程式证明风格

### 2.1 两种风格的根本差异

在证明助手中，存在两种根本不同的证明构建风格：

| 维度 | 声明式（Declarative） | 过程式（Procedural/Tactic-based） |
|:---|:---|:---|
| **代表系统** | Mizar、Isar（Isabelle）、C-zar（Coq） | Coq（Ltac）、Lean（tactic mode）、HOL Light |
| **证明结构** | `proof ... end` 块，包含声明语句 | `Proof. tactic1. tactic2. ... Qed.` |
| **可读性** | 高——接近自然语言数学证明 | 低——需理解策略执行效果 |
| **构建方式** | 逐声明构建，类似写下推理步骤 | 逐策略执行，类似编程 |
| **中间状态** | 每个声明都是显式推理步骤 | 中间目标不可见（除非 explicitly intro） |
| **适合场景** | 输出、展示、教学、审查 | 交互式开发、自动化探索 |

Mizar 的声明式证明块示例：

```
theorem Th1:
  for A, B, C being Point st A <> B holds
    ex D being Point st D is_midpoint_of A,B
proof
  let A, B, C be Point;
  assume A <> B;
  consider D being Point such that
    D is_midpoint_of A,B by AXIOM_MIDPOINT;
  thus thesis;
end;
```

对应的过程式（Coq/Ltac）风格：

```coq
Theorem th1 : forall (A B C : Point),
  A <> B -> exists D : Point, midpoint D A B.
Proof.
  intros A B C Hneq.
  apply axiom_midpoint; assumption.
Qed.
```

### 2.2 声明式风格的证明构造原语

Mizar 的核心声明式原语及其数学语义：

| Mizar 原语 | 数学语义 | 推理规则 |
|:---|:---|:---|
| `let x be T` | 引入变量 x，类型为 T | ∀-intro |
| `assume P` | 假设命题 P 成立 | →-intro |
| `given x such that P(x)` | 已引入满足 P(x) 的 x | ∃-elim |
| `consider x such that P(x)` | 从已知条件构造满足 P(x) 的 x | ∃-intro |
| `then Q` | 从前一语句推出 Q | modus ponens |
| `hence Q` | 从前一语句推出 Q 并将其加入当前目标 | modus ponens + discharge |
| `thus thesis` | 证明当前目标 | close goal |
| `per cases` | 分情况讨论 | case analysis |
| `suppose P` | 情况 P 下的子证明 | case branch |
| `set x = expr` | 定义缩写 | definition expansion |
| `reconsider x as T` | 将 x 视为类型 T（类型转换） | type coercion |

### 2.3 声明式风格对 Lv-00 的借鉴价值

Lv-00 的证明引擎在内部使用符号计算（坐标消解、等式推导）完成几何推理，但输出的证明需要是**人类可读的**。声明式风格提供了一条清晰的翻译路径：

```
内部符号计算步骤                    →    声明式输出
─────────────────────────────────────────────────────
坐标消解: x_B = (x_A + x_C) / 2    →    consider D being Point such that
                                          D is_midpoint_of A,C
等式推导: d(A,B)^2 = d(A,C)^2       →    then |AB| = |AC| by distance_calc
约束满足: angle BAC = 90°           →    hence angle BAC is right
目标合一: triangle XYZ 满足模式       →    thus thesis
```

---

## 3. "语义集合 + 语法类型"双层抽象

### 3.1 Mizar 的双层抽象模型

Mizar 的类型系统基于**双层抽象**：

**底层——语义集合（Semantic Set）**：
- 所有数学对象都存在于某个底层集合中，如 `Element of REAL`、`Element of the carrier of G`
- 语义集合之间没有强类型隔离——同一个对象可以被视为不同集合的元素

**上层——语法类型（Syntactic Type / Mode）**：
- `mode` 定义语法类型，如 `mode Point of TOP-REAL n is Element of REAL n`
- `struct` 定义复合结构，如代数结构（Group、Ring、Field）
- 语法类型可通过 `reconsider` 进行重新归类

### 3.2 映射到 Lv-00 的几何对象表示

Lv-00 的几何对象表示天然就是双层抽象的实例：

| Mizar 概念 | Lv-00 映射 | 说明 |
|:---|:---|:---|
| 语义集合（Element of ...） | 坐标集合（`Coordinate*` 数组） | 所有几何对象底层都是坐标 |
| mode（语法类型） | `TypeRegion` + `TYPE_KIND_*` | Point/Line/Circle 等类型标签 |
| struct（复合结构） | `ConstraintGraph*` + `node_ids` | 三角形/四边形等复合图形 |
| `reconsider as`（重归类） | 类型转换 + 约束检查 | 将"三点集合"重归类为"三角形" |
| `cluster`（属性集群） | `TypeRegion.constraint_ids` | 为类型附加约束属性 |

### 3.3 Lv-00 双层抽象的核心 C 数据结构

```c
/**
 * @brief Lv-00 的"语义集合+语法类型"双层抽象
 *
 * 借鉴 Mizar 的 Element of SET + mode 双层模型：
 *
 * 底层（语义集合）：CoordinateUnion —— 所有几何对象都是坐标集合
 *   - 点：1 个坐标
 *   - 线段：2 个坐标（端点）
 *   - 圆：1 个坐标（圆心）+ 1 个标量（半径）
 *   - 三角形：3 个坐标（顶点）
 *   - 任意多边形：n 个坐标
 *
 * 上层（语法类型）：TypeRegion —— 通过 TYPE_KIND_* 区分类型语义
 *   - TYPE_KIND_POINT：坐标为 1 个点
 *   - TYPE_KIND_LINE_SEGMENT：坐标为 2 个点
 *   - TYPE_KIND_CIRCLE：坐标表示圆心 + 半径
 *   - TYPE_KIND_TRIANGLE：坐标为 3 个点 + 三角形特有约束
 */

/**
 * @brief 坐标联合体——语义集合层
 *
 * 借鉴 Mizar 的 "Element of the carrier of ..."。
 * 所有几何对象在底层都是坐标集合，具体语义由上层 TypeRegion 赋予。
 */
typedef union {
    double point[2];           /**< 二维点坐标 (x, y) */
    double point3d[3];         /**< 三维点坐标 (x, y, z) */
    double segment[2][2];      /**< 线段两端点坐标 */
    double circle_params[3];   /**< 圆圆参数 (cx, cy, r) */
    double triangle[3][2];     /**< 三角形三顶点坐标 */
    double polygon_n[][2];     /**< 多边形顶点坐标 */
} CoordinateUnion;

/**
 * @brief 类型区域——语法类型层
 *
 * 借鉴 Mizar 的 mode/struct。
 * 通过 kind 字段区分 Point/Line/Circle 等语法类型，
 * 通过 constraint_ids 附加属性约束（如"直角三角形"）。
 */
typedef struct {
    TypeKind kind;              /**< 语法类型标签（TYPE_KIND_POINT等） */
    int *constraint_ids;        /**< 附加约束（借鉴 Mizar cluster） */
    int constraint_count;       /**< 约束数量 */
    UniverseLevel universe;     /**< 宇宙层级 */
    char *type_name;            /**< 人类可读类型名 */
} TypeRegion;
```

### 3.4 双层抽象的类型转换（reconsider 映射）

Mizar 中的 `reconsider x as T` 将已引入的对象重新归类为更具体的类型。在 Lv-00 中对应为：给定三点坐标集合，将其重新归类为"三角形"——前提是三点不共线。

```c
/**
 * @brief 类型重归类——借鉴 Mizar 的 reconsider
 *
 * 尝试将几何对象从当前类型转换为目标类型。
 * 转换成功的前提：对象满足目标类型的约束条件。
 *
 * @example
 *   // Mizar: reconsider T as Triangle by A1, A2;
 *   // Lv-00:
 *   RegionConversionResult rc = type_reconsider(
 *       ts, coord_node_id, TYPE_KIND_TRIANGLE);
 *   // 内部检查：三点是否不共线、是否构成有效三角形
 */
typedef enum {
    RECONSIDER_SUCCESS,          /**< 转换成功 */
    RECONSIDER_CONSTRAINT_FAIL,  /**< 不满足目标类型约束 */
    RECONSIDER_TYPE_MISMATCH,    /**< 类型不兼容 */
    RECONSIDER_NEED_PROOF        /**< 需要用户提供证明 */
} RegionConversionResult;

RegionConversionResult type_reconsider(
    TypeSystem *ts,
    int object_id,
    TypeKind target_kind
);
```

---

## 4. MML 库组织方式对公理包生态的参考

### 4.1 MML 的 article 模型

MML 的组织方式以 article 为基本单位，每个 article 是一个独立的文件，包含：

```
environ          ← 环境声明（引用哪些已有 article）
  vocabularies   ← 词汇表
  notations      ← 符号表
  constructors   ← 构造器
  registrations  ← 注册（cluster等）
  requirements   ← 要求
  definitions    ← 所需定义
  theorems       ← 所需定理
  schemes        ← 所需模式

begin            ← 正文开始
  definition     ← 新定义
  registration   ← 新注册
  theorem        ← 新定理
  ...
end
```

### 4.2 映射到 Lv-00 的公理包系统

| Mizar MML 概念 | Lv-00 公理包映射 | 说明 |
|:---|:---|:---|
| `article` | `axiom_package *.lvp` | 独立的公理/定理文件 |
| `environ` 块 | 包清单（`depends: [...]`） | 声明依赖关系 |
| `definition` | `AXIOM_DEFINE` + 类型构造 | 定义新类型/新谓词 |
| `theorem` | `THEOREM` + 命题模式 | 已证明的定理 |
| `registration` | `TYPE_REGISTER` + cluster | 注册类型属性 |
| `scheme` | `PROOF_SCHEME` + 通用模式 | 可参数化的证明模式 |
| 跨 article 引用 | `lv00_package_resolve()` | 解析跨包引用 |

### 4.3 Lv-00 公理包文件格式设计

```
# -*- Lv-00 Axiom Package -*-
# 借鉴 Mizar MML 的 article 模型

[package]
name = "euclidean_geometry_core"
version = "1.0.0"
description = "欧几里得平面几何核心公理包"
authors = ["Lv-00 Team"]
date = "2026-05-24"

[depends]
euclidean_geometry_points = ">=0.9.0"
euclidean_geometry_lines = ">=0.9.0"
euclidean_geometry_circles = ">=0.9.0"

[exports]
types = ["Point", "Line", "Circle", "Triangle", "Angle"]
axioms = ["AXIOM_PARALLEL_POSTULATE", "AXIOM_BETWEENNESS"]
theorems = ["THM_PYTHAGOREAN", "THM_MIDLINE", "THM_ANGLE_BISECTOR"]
predicates = ["collinear", "concyclic", "similar", "congruent"]

[content]
include "definitions.lv0"
include "axioms.lv0"
include "theorems.lv0"
include "lemmas.lv0"
```

---

## 5. 声明式证明输出 API

### 5.1 对照表：Mizar 声明式构造 → Lv-00 proof.h API

| Mizar 声明式构造 | Lv-00 proof.h 对应 API | 输出效果 |
|:---|:---|:---|
| `let A be Point` | `proof_declare_variable(nav, "A", TYPE_KIND_POINT)` | 输出："设 A 为点" |
| `assume A <> B` | `proof_assume_condition(nav, "A_neq_B")` | 输出："假定 A != B" |
| `consider D such that P(D)` | `proof_construct_from(nav, "D", pattern_id)` | 输出："取点 D，满足 D 为 AB 之中点" |
| `then Q by Th1` | `proof_infer(nav, "Q", "Th1")` | 输出："由定理1，得 Q" |
| `hence thesis` | `proof_conclude(nav)` | 输出："因此得证" |
| `per cases` | `proof_case_split(nav, cases)` | 输出："分以下情况讨论..." |
| `suppose case1` | `proof_enter_case(nav, 1)` | 输出："情况1：..." |
| `set X = expr` | `proof_define_abbrev(nav, "X", expr)` | 输出："记 X = ..." |

### 5.2 声明式证明输出 API 设计

```c
/**
 * @brief 声明式证明输出 API —— 借鉴 Mizar 声明式证明风格
 *
 * 这些 API 将 Lv-00 内部的符号计算步骤"翻译"为
 * 人类可读的声明式几何证明文本。
 *
 * 设计原则：
 *  - 每个 API 调用对应于 Mizar 的一个声明式证明原语
 *  - API 自动从内部约束图提取推理依据
 *  - 输出格式支持 Markdown/LaTeX/纯文本三种模式
 *
 * @file proof.h
 */

/**
 * @brief 声明式证明输出格式
 */
typedef enum {
    PROOF_FORMAT_PLAINTEXT,   /**< 纯文本格式 */
    PROOF_FORMAT_MARKDOWN,    /**< Markdown 格式 */
    PROOF_FORMAT_LATEX,       /**< LaTeX 数学格式 */
    PROOF_FORMAT_HTML         /**< HTML 格式（用于 Web GUI） */
} ProofOutputFormat;

/**
 * @brief 声明一个几何变量
 *
 * 对应 Mizar 的 let A be Point
 * 输出："设 A 为点"
 */
ProofStatement *proof_declare_variable(
    ProofNavigator *nav,
    const char *variable_name,
    TypeKind type_kind
);

/**
 * @brief 引入假设条件
 *
 * 对应 Mizar 的 assume P
 * 输出："假定 A != B"
 */
ProofStatement *proof_assume_condition(
    ProofNavigator *nav,
    const char *condition_id
);

/**
 * @brief 构造满足条件的存在对象
 *
 * 对应 Mizar 的 consider x such that P(x)
 * 输出："取点 D，满足 D 为 AB 的中点"
 */
ProofStatement *proof_construct_from(
    ProofNavigator *nav,
    const char *target_name,
    int pattern_id,
    const char *justification
);

/**
 * @brief 从前一步推出新结论
 *
 * 对应 Mizar 的 then Q by ...
 * 输出："于是，由定理1，AD 平行于 BC"
 */
ProofStatement *proof_infer(
    ProofNavigator *nav,
    const char *conclusion,
    const char *justification
);

/**
 * @brief 得出最终结论
 *
 * 对应 Mizar 的 thus thesis / hence thesis
 * 输出："因此，三角形 ABC 为等腰三角形。证毕。"
 */
ProofStatement *proof_conclude(
    ProofNavigator *nav
);

/**
 * @brief 将整个证明翻译为声明式证明文本
 *
 * 遍历 ProofNavigator 中的步骤序列，
 * 对每个步骤调用相应的声明式输出，生成完整证明。
 *
 * @param nav         证明导航器
 * @param format      输出格式
 * @param out_proof   输出：声明式证明文本（调用者释放）
 * @return 步骤数量（>0 成功，-1 失败）
 */
int proof_to_declarative(
    ProofNavigator *nav,
    ProofOutputFormat format,
    char **out_proof
);
```

### 5.3 代码示例：输出类自然语言的几何证明

以下示例展示 Lv-00 如何将三角形中线定理的内部符号计算过程，输出为声明式几何证明：

```c
/**
 * @brief 示例：三角形中线定理的声明式证明输出
 *
 * 定理：在三角形 ABC 中，设 D 为 BC 中点，则 AD 是中线。
 *
 * 内部符号计算步骤：
 *  1. 获取 B 和 C 的坐标
 *  2. 计算中点坐标：D = ((x_B + x_C)/2, (y_B + y_C)/2)
 *  3. 验证 D 满足中点约束：|BD| = |DC| 和 B-D-C 共线
 *  4. 验证 AD 满足中线约束：连接顶点 A 与对边中点 D
 *  5. 模式匹配：AD 是三角形 ABC 的中线
 *
 * 输出：声明式几何证明
 */
void example_midline_declarative_proof(void)
{
    ProofNavigator *nav = proof_navigator_create();
    ProofOutputFormat format = PROOF_FORMAT_PLAINTEXT;

    // --- 声明式证明步骤 ---

    // Mizar: let A, B, C be Point
    proof_declare_variable(nav, "A", TYPE_KIND_POINT);
    proof_declare_variable(nav, "B", TYPE_KIND_POINT);
    proof_declare_variable(nav, "C", TYPE_KIND_POINT);

    // Mizar: assume A, B, C are non-collinear
    proof_assume_condition(nav, "non_collinear_ABC");

    // Mizar: consider D being Point such that D is_midpoint_of B,C
    //         by AXIOM_MIDPOINT_EXISTS
    proof_construct_from(nav, "D", PATTERN_MIDPOINT,
        "中点存在公理");

    // Mizar: then D lies on segment BC by definition of midpoint
    proof_infer(nav, "D 在 BC 上",
        "中点定义");

    // Mizar: hence AD is median of triangle ABC
    proof_conclude(nav);

    // --- 生成声明式证明文本 ---
    char *proof_text = NULL;
    int steps = proof_to_declarative(nav, format, &proof_text);

    printf("%s\n", proof_text);
    // 输出：
    // ─────────────────────────────────────
    // 定理：在三角形 ABC 中，AD 是中线。
    //
    // 证明：
    //   设 A、B、C 为不共线的三点。
    //   由中点存在公理，取点 D，满足 D 为 BC 的中点。
    //   由中点定义，D 在 BC 上。
    //   因此，AD 是三角形 ABC 的中线。证毕。
    // ─────────────────────────────────────

    free(proof_text);
    proof_navigator_destroy(nav);
}
```

### 5.4 证明输出格式化器

```c
/**
 * @brief 声明式证明的格式化输出器
 *
 * 将内部的 ProofStatement 序列转换为特定格式的文本。
 * 支持三种格式的格式化器可插拔注册。
 */
typedef struct {
    /** 格式化单条声明语句 */
    char *(*format_declare)(const ProofStatement *stmt);
    /** 格式化假设语句 */
    char *(*format_assume)(const ProofStatement *stmt);
    /** 格式化构造语句 */
    char *(*format_construct)(const ProofStatement *stmt);
    /** 格式化推理语句 */
    char *(*format_infer)(const ProofStatement *stmt);
    /** 格式化结论语句 */
    char *(*format_conclude)(const ProofStatement *stmt);
    /** 格式化段落分隔 */
    char *(*format_paragraph_break)(void);
    /** 格式化证明块头部 */
    char *(*format_proof_header)(const char *theorem_name);
    /** 格式化证明块尾部 */
    char *(*format_proof_footer)(void);
} ProofFormatter;

/** 注册内置格式化器 */
ProofFormatter *proof_formatter_plaintext(void);
ProofFormatter *proof_formatter_markdown(void);
ProofFormatter *proof_formatter_latex(void);
void proof_formatter_register(ProofOutputFormat format, ProofFormatter *fmt);
```

---

## 6. 符号计算到声明式证明的翻译机制

### 6.1 翻译流水线

Lv-00 内部使用符号计算（坐标消解、等式推导、约束求解）完成几何推理。将这些内部步骤"翻译"为人类可读的声明式证明，需要以下翻译流水线：

```
内部符号计算步骤
  │
  ├─ 步骤1：坐标消解
  │   内部：substitute(coord_B, (coord_A + coord_C) / 2)
  │   翻译: "取点 D，使得 D 为 AC 的中点"
  │
  ├─ 步骤2：距离等式推导
  │   内部：deduce(|AB|^2 = |AC|^2, from coord_substitution)
  │   翻译: "于是，|AB| = |AC|"
  │
  ├─ 步骤3：约束模式匹配
  │   内部：match_pattern(triangle_ABC, PATTERN_ISOSCELES)
  │   翻译: "因此，三角形 ABC 为等腰三角形"
  │
  └─ 输出：声明式证明文本
```

### 6.2 翻译表的设计

```c
/**
 * @brief 符号步骤 → 声明式语句的翻译表
 *
 * 每个内部符号操作类型对应一个翻译模板。
 * 模板中的 %s 参数从约束图中自动提取。
 */
typedef struct {
    SymbolicOpType op_type;          /**< 内部符号操作类型 */
    const char *template_cn;         /**< 中文声明式模板 */
    const char *template_en;         /**< 英文声明式模板 */
    const char *justification_hint;  /**< 默认推理依据 */
} DeclarativeTranslationEntry;

static const DeclarativeTranslationEntry translation_table[] = {
    {
        .op_type = SYM_OP_MIDPOINT_CONSTRUCT,
        .template_cn = "取点 %s，满足 %s 为 %s 的中点",
        .template_en = "consider %s such that %s is the midpoint of %s",
        .justification_hint = "中点存在公理"
    },
    {
        .op_type = SYM_OP_DISTANCE_EQUALITY,
        .template_cn = "于是，|%s%s| = |%s%s|",
        .template_en = "then |%s%s| = |%s%s|",
        .justification_hint = "距离计算"
    },
    {
        .op_type = SYM_OP_ANGLE_EQUALITY,
        .template_cn = "于是，∠%s%s%s = ∠%s%s%s",
        .template_en = "then ∠%s%s%s = ∠%s%s%s",
        .justification_hint = "角度计算"
    },
    {
        .op_type = SYM_OP_COLLINEAR_CHECK,
        .template_cn = "故 %s、%s、%s 三点共线",
        .template_en = "hence %s, %s, %s are collinear",
        .justification_hint = "共线判定"
    },
    {
        .op_type = SYM_OP_PATTERN_MATCH,
        .template_cn = "因此，%s 为 %s。证毕。",
        .template_en = "thus %s is %s. QED.",
        .justification_hint = "模式匹配"
    },
    // ... 更多翻译条目
};
```

### 6.3 推理依据的自动提取

```c
/**
 * @brief 从约束图自动提取推理依据
 *
 * 对于每个符号步骤，在约束图中查找其依赖的
 * 公理/定理/定义节点，生成推理依据文本。
 *
 * @example
 *   步骤 "|AB| = |CD|" 的依赖：
 *     - 公理 AXIOM_DISTANCE_FORMULA
 *     - 前一步的坐标消解结果
 *   → 推理依据："由距离公式及上一步"
 */
char *proof_extract_justification(
    ProofNavigator *nav,
    int step_index
);
```

---

## 7. 实现路线图

### 7.1 第一阶段：声明式证明基础设施（P2-1）

- [ ] 定义 `ProofStatement` 数据结构及其序列化格式
- [ ] 实现 `ProofFormatter` 接口和三个内置格式化器
  - [ ] `proof_formatter_plaintext()` —— 纯文本格式器
  - [ ] `proof_formatter_markdown()` —— Markdown 格式器
  - [ ] `proof_formatter_latex()` —— LaTeX 格式器
- [ ] 实现基本声明式 API：`proof_declare_variable`、`proof_assume_condition`
- [ ] 实现推导 API：`proof_construct_from`、`proof_infer`、`proof_conclude`
- [ ] 编写声明式 API 的单元测试
  - [ ] 测试基本证明块的生成
  - [ ] 测试三种输出格式的一致性

### 7.2 第二阶段：符号→声明式翻译（P2-2）

- [ ] 定义 `SymbolicOpType` 枚举，覆盖所有内部符号操作类型
- [ ] 建立 `DeclarativeTranslationEntry` 翻译表（中英双模板）
- [ ] 实现 `proof_extract_justification()` 推理依据自动提取
- [ ] 实现 `proof_to_declarative()` 证明步骤→声明式文本的完整翻译
- [ ] 处理复杂证明结构：分情况讨论、反证法、归纳
- [ ] 编写翻译管线的集成测试
  - [ ] 三角形中线定理的声明式输出
  - [ ] 勾股定理的声明式输出
  - [ ] 三角形内角和定理的声明式输出

### 7.3 第三阶段：MML 风格公理包系统（P2-3）

- [ ] 设计公理包文件格式（`.lvp` 格式）
- [ ] 实现 `lv00_package_create()` / `lv00_package_load()` 包管理 API
- [ ] 实现 `lv00_package_resolve()` 跨包依赖解析
- [ ] 实现 environ 风格的环境声明块
- [ ] 实现公理包版本管理和兼容性检查
- [ ] 创建标准公理包 `euclidean_geometry_core.lvp`
- [ ] 编写包系统的集成测试

### 7.4 第四阶段：双层抽象与类型重归类（P2-4）

- [ ] 完善 `CoordinateUnion` 语义集合层
- [ ] 实现 `type_reconsider()` 类型重归类
- [ ] 实现重归类时的约束自动检查
- [ ] 实现 `RECONSIDER_NEED_PROOF` 的交互式证明回退
- [ ] 编写双层抽象的使用文档和示例

---

## 8. 设计决策与权衡

### 8.1 声明式 vs 过程式输出的选择

Lv-00 选择声明式输出作为主要的证明输出格式，基于以下考量：

- **目标用户**：几何学习者、数学教师、几何爱好者——他们期望看到类似教科书的证明，而非策略序列
- **审查需求**：Lv-00 生成的证明需要被人的直觉审查（"看起来对"），声明式天然更适合人工审查
- **GRPO 友好**：强化学习训练需要可读的证明作为奖励信号输入，声明式文本比过程式策略序列更适合作为文本奖励的输入
- **混合策略**：内部开发使用过程式（更快），对外输出使用声明式（更可读）——两者通过翻译层解耦

### 8.2 双层抽象与单层类型系统的权衡

双层抽象（语义集合 + 语法类型）相比 Coq/Lean 的单层依赖类型系统，牺牲了部分类型安全性，但获得了：

- **灵活性**：同一个坐标集合可以被重新归类为不同类型（三点集合 → 三角形/三点共线）
- **可演化性**：添加新的语法类型不需要修改底层坐标表示
- **Mizar 兼容性**：可直接参考 MML 中 60000+ 定理的推理模式

成本在于：需要额外的类型转换检查和运行时约束验证。

### 8.3 MML 兼容性的长期价值

借鉴 MML 的 article 库组织方式，使 Lv-00 的公理包生态可以：

1. **逐步积累**：每个 article 独立可发表、独立可审查
2. **依赖清晰**：environ 块显式声明所有外部引用
3. **可检索性**：按主题组织（几何/代数/数论），类似学术期刊
4. **社区贡献**：第三方可以创建自己的公理包 article

---

## 9. 补充：Mizar 证明检查器的内部架构

### 9.1 Mizar 验证器的处理阶段

Mizar 证明检查器（Verifier）按以下顺序处理 article：

```
article 文件
  │
  ├─ 1. 词法分析（Tokenizer）
  │   识别关键字（let/assume/then等）、标识符、符号
  │
  ├─ 2. 语法分析（Parser）
  │   构建证明块 AST
  │
  ├─ 3. 环境导入（Environment Import）
  │   解析 environ 块，加载依赖 article
  │
  ├─ 4. 类型检查（Type Checker）
  │   验证所有类型声明的一致性
  │
  ├─ 5. 证明检查（Checker / Refiner）
  │   逐声明检查推理步骤的有效性
  │   ├─ 每个 then/hence 语句触发一次推理验证
  │   ├─ 推理验证通过重写和归结完成
  │   └─ 不通过则标记错误位置
  │
  ├─ 6. 注册处理（Registrations）
  │   处理 cluster/identify/reduce 注册
  │
  └─ 7. 导出（Export）
      生成 .miz → .xml 的中间表示
```

### 9.2 Mizar 推理检查的"Reconsider"逻辑

Mizar 内部的一条核心推理规则是"类型重聚"（Reconsider），允许将类型不够精确的对象通过附加上下文信息重新归类为精确类型。例如：

```
已知 x 是 Element of REAL，且 x > 0
→ reconsider x as positive Real
→ 后续可以使用 positive Real 的所有定理
```

这个机制在 Lv-00 中对应几何对象的"语境化重归类"：

```
已知三点 A、B、C 的坐标，且 collinear(A,B,C) = false
→ type_reconsider({A,B,C}, TYPE_KIND_TRIANGLE)
→ 后续可以使用三角形的所有定理（内角和、中线等）
```

---

## 10. 参考资源

- Mizar 项目主页：https://github.com/MizarProject/system
- Mizar Mathematical Library（MML）：http://mizar.org/library/
- Mizar 语言参考手册：http://mizar.org/language/
- Mizar 证明风格指南（Mizar Proof Style Guide）：http://mizar.org/proof_style/
- 《Mizar in a Nutshell》—— Adam Grabowski 著，Mizar 入门教程
- 《The Role of Mizar in the Formalization of Mathematics》—— 关于 Mizar 在数学形式化中的角色
- Isabelle/Isar 参考手册（Isar 是 Mizar 风格在 Isabelle 中的实现）：https://isabelle.in.tum.de/doc/isar-ref.pdf
- C-zar（Coq 的声明式证明模式）文档：https://coq.inria.fr/doc/
- Lv-00 相关文档：
  - `proof.h` —— 证明导航器与证明步骤数据结构
  - `type_system.h` —— 类型系统与 TypeRegion 定义
  - `axiom_package.h` —— 公理包系统 API
