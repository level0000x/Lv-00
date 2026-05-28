# Isabelle/HOL - 通用形式化证明助手参考文档

> **项目名称**：Isabelle/HOL + AFP (Archive of Formal Proofs)
> **项目链接**：https://isabelle.in.tum.de/ | https://www.isaforge.org/
> **项目类型**：交互式定理证明器 / 形式化数学库
> **语言/技术栈**：Standard ML (Poly/ML)、Isar 证明语言
> **最后更新**：2024年持续活跃（Isabelle2024 已发布）
> **文档版本**：v1.0
> **适用层级**：第4层（多策略自动推理层）、第2层（基础几何公理层）

---

## 1. 项目概述

### 1.1 项目背景与定位

Isabelle 是由剑桥大学 Lawrence Paulson 开发的通用交互式定理证明器，采用 LCF 风格的内核架构，提供高度可靠的形式化证明环境。Isabelle 的核心创新在于**元逻辑（Meta-Logic）设计**——使用极简的 meta-logic（多态一阶逻辑）作为基础，上层支持多种对象逻辑（Object Logics），包括 HOL（Higher-Order Logic）、ZF 集合论等。

Isabelle/HOL 是最成熟的对象逻辑实现，配合 Archive of Formal Proofs（AFP）形成了**最大的形式化数学库**，收录 700+ 形式化理论。Lv-00 的公理包系统可借鉴 Isabelle 的元逻辑-对象逻辑分层设计。

### 1.2 核心架构设计

Isabelle 的架构分为三层：

```
┌─────────────────────────────────────────────┐
│ Isabelle/Isar - 证明语言与接口层              │
│  declarative proof、proof methods、 tactics  │
├─────────────────────────────────────────────┤
│ Pure - 元逻辑层                              │
│  多态一阶逻辑、类型框架、绑定表达式          │
├─────────────────────────────────────────────┤
│ HOL - 对象逻辑层                            │
│  高阶逻辑、集合论、函数分析                   │
└─────────────────────────────────────────────┘
```

**Isar 证明语言**是 Isabelle 的标志性特性，支持声明式证明风格：

```isabelle
theorem Pythagoras:
  fixes a b :: real
  assumes "a ≥ 0" "b ≥ 0"
  shows "a² + b² = c² ⟷ √(a² + b²) = c"
proof -
  have "a² + b² = (a + b)² - 2·a·b" by (simp add: power2_eq_square)
  then show ?thesis by (simp add: real.sqrt_add)
qed
```

### 1.3 AFP 档案库

Archive of Formal Proofs (AFP) 是 Isabelle 的形式化数学档案：
- 700+ 形式化理论
- 涵盖：代数学、拓扑学、数论、概率论、程序验证
- 同行评审制度保证质量
- 持续更新，与 Isabelle 同步发布

---

## 2. 核心借鉴点

### 2.1 元逻辑分层设计

Isabelle 的元逻辑设计是最核心的借鉴点：

```isabelle
(* 元逻辑层：Pure *)
(* 使用 λ-calculus 表达逻辑结构 *)
consts Trueprop :: "prop"  (* 命题类型 *)
consts imp :: "prop ⇒ prop ⇒ prop"  (* 蕴含 *)
consts All :: "('a ⇒ prop) ⇒ prop"  (* 全称量词 *)

(* 对象逻辑层：HOL *)
typedecl real
consts
  plus :: "real ⇒ real ⇒ real"  (infixl "+" 65)
  times :: "real ⇒ real ⇒ real"  (infixl "·" 70)
  zero :: real  ("0")
  one :: real  ("1")
```

**Lv-00 借鉴价值**：Lv-00 可借鉴此设计，实现**几何元逻辑**（定义几何对象的基本类型和运算），上层**公理包**（Hilbert 公理、Tarski 公理、Euclid 公理等）作为对象逻辑实现。

### 2.2 Isar 声明式证明语言

Isar 的声明式证明风格将证明意图与证明执行分离：

```isabelle
(* 声明式：描述证明结构 *)
theorem "parallelogram_diag":
  assumes "parallelogram A B C D"
  shows "diagonal AC ⟷ diagonal BD"
proof -
  from `parallelogram A B C D`
  have "AB ∥ CD" and "AD ∥ BC" by (unfold parallelogram_def)
  show ?thesis using parallelogram_diagonal_theorem
    by (simp add: `AB ∥ CD` `AD ∥ BC`)
qed
```

**Lv-00 借鉴价值**：Lv-00 的证明 DSL 可借鉴 Isar 语法，提供**声明式几何证明语法**，用户描述证明思路，系统自动执行证明搜索。

### 2.3 证明方法（Proof Methods）

Isabelle 的证明方法是可组合的策略系统：

```isabelle
(* 常用证明方法 *)
by auto           (* 自动证明 *)
by simp           (* 化简 *)
by blast          (* 一阶逻辑自动证明 *)
by (rule xxx)     (* 应用规则 *)
using xxx         (* 添加额外事实 *)
unfolding xxx     (* 展开定义 *)
```

**Lv-00 借鉴价值**：Lv-00 可设计几何证明方法：
- `by geometry` - 几何自动证明
- `by algebra` - 代数化简
- `using midpoint` - 添加中点事实
- `unfolding definition` - 展开几何定义

### 2.4 核心借鉴点对照表

| Isabelle/HOL 特性 | Lv-00 现有能力 | Lv-00 借鉴方案 |
|:---|:---|:---|
| 元逻辑-对象逻辑分层 | `axiom.h` 单一公理层 | 新增 `geo_metalogic.h` 分层架构 |
| Isar 声明式语法 | `proof_parser.y` 过程式证明 | 新增 Isar 风格 DSL，支持声明式证明 |
| Proof Methods | `tactics.h` 简单策略 | 扩展为可组合证明方法系统 |
| AFP 档案库 | `axiom_packages/` 少量公理包 | 扩展公理包生态，建设几何形式化库 |
| LCF 内核架构 | `kernel.h` 基础内核 | 增强内核可靠性验证 |

---

## 3. Lv-00 映射方案

### 3.1 几何元逻辑层设计

基于 Isabelle 的元逻辑设计，Lv-00 可实现几何元逻辑：

```c
// geo_metalogic.h - 几何元逻辑层

#ifndef LV00_GEO_METALOGIC_H
#define LV00_GEO_METALOGIC_H

#include <lv00.h>

// ============ 类型系统 ============

// 几何类型变量
typedef struct Lv00TypeVar Lv00TypeVar;
struct Lv00TypeVar {
    char* name;           // 类型变量名，如 "'a"
    Lv00TypeVar* next;    // 链接下一个类型变量
};

// 几何类型
typedef enum {
    LV00_GTYPE_POINT,     // 点类型
    LV00_GTYPE_LINE,      // 直线类型
    LV00_GTYPE_CIRCLE,   // 圆类型
    LV00_GTYPE_PLANE,     // 平面类型
    LV00_GTYPE_SEGMENT,   // 线段类型
    LV00_GTYPE_ANGLE,    // 角度类型
    LV00_GTYPE_REAL,     // 实数类型（用于度量）
    LV00_GTYPE_BOOL,     // 布尔类型
    LV00_GTYPE_VAR       // 类型变量
} Lv00GeoTypeTag;

typedef struct {
    Lv00GeoTypeTag tag;
    union {
        struct { int unused; } point;     // 空结构占位
        struct { int unused; } line;
        struct { int unused; } circle;
        struct { Lv00GeoTypeTag elem; } set;  // 集合类型
        struct { Lv00TypeVar* var; } var;     // 类型变量
    } data;
} Lv00GeoType;

// 类型上下文（类型环境）
typedef struct {
    char** names;          // 变量名列表
    Lv00GeoType* types;   // 对应的类型
    size_t count;
    struct Lv00TypeEnv* parent;  // 父作用域
} Lv00TypeEnv;

// ============ 几何项（Terms）============

typedef enum {
    LV00_TERM_VAR,        // 几何变量
    LV00_TERM_CONST,       // 几何常量
    LV00_TERM_APP,        // 函数应用
    LV00_TERM_ABS,        // λ-抽象
    LV00_TERM_GEOM        // 几何对象构造
} Lv00TermTag;

typedef struct Lv00Term Lv00Term;
struct Lv00Term {
    Lv00TermTag tag;
    Lv00GeoType type;     // 项的类型
    union {
        struct { char* name; } var;
        struct { char* name; Lv00Term** args; size_t n; } const_;
        struct { Lv00Term* fn; Lv00Term* arg; } app;
        struct { char* vname; Lv00GeoType vtype; Lv00Term* body; } abs;
        struct { Lv00GeoConstructor ctor; Lv00Term** args; size_t n; } geom;
    } data;
};

// 几何构造函数
typedef enum {
    LV00_CONSTRUCT_MK_POINT,      // mk_point(x, y)
    LV00_CONSTRUCT_MK_LINE,       // mk_line(p1, p2)
    LV00_CONSTRUCT_MK_CIRCLE,     // mk_circle(center, radius)
    LV00_CONSTRUCT_MK_SEGMENT,    // mk_segment(p1, p2)
    LV00_CONSTRUCT_MIDPOINT,      // midpoint(p1, p2)
    LV00_CONSTRUCT_INTERSECT      // intersect(l1, l2)
} Lv00GeoConstructor;

// ============ 几何命题（Propositions）============

typedef enum {
    LV00_PROP_EQ,           // 几何相等 A ≡ B
    LV00_PROP_CONG,         // 全等 A ≅ B
    LV00_PROP_PARALLEL,     // 平行 A ∥ B
    LV00_PROP_PERP,         // 垂直 A ⟂ B
    LV00_PROP_ON,           // 在...上 P on L
    LV00_PROP_BETWEEN,      // 在...之间 Between(A,B,C)
    LV00_PROP_CONVEX,       // 凸 convex(S)
    LV00_PROP_EXISTS,       // 存在量词 ∃x. P(x)
    LV00_PROP_FORALL,       // 全称量词 ∀x. P(x)
    LV00_PROP_IMP,          // 蕴含 P ⇒ Q
    LV00_PROP_AND,          // 合取 P ∧ Q
    LV00_PROP_OR            // 析取 P ∨ Q
} Lv00PropTag;

typedef struct Lv00Prop Lv00Prop;
struct Lv00Prop {
    Lv00PropTag tag;
    Lv00TypeEnv* type_env;  // 类型环境
    
    union {
        struct { Lv00Term* lhs; Lv00Term* rhs; } eq;
        struct { Lv00Term* l1; Lv00Term* l2; } parallel;
        struct { Lv00Term* l1; Lv00Term* l2; } perp;
        struct { Lv00Term* p; Lv00Term* obj; } on;
        struct { Lv00Term* a; Lv00Term* b; Lv00Term* c; } between;
        struct { char* vname; Lv00GeoType vtype; Lv00Prop* body; } exists;
        struct { char* vname; Lv00GeoType vtype; Lv00Prop* body; } forall;
        struct { Lv00Prop* antecedent; Lv00Prop* consequent; } imp;
        struct { Lv00Prop* left; Lv00Prop* right; } and;
        struct { Lv00Prop* left; Lv00Prop* right; } or;
    } data;
};

// ============ 证明上下文 ============

typedef struct Lv00ProofContext Lv00ProofContext;
struct Lv00ProofContext {
    // 类型环境
    Lv00TypeEnv* type_env;
    
    // 假设（局部的几何事实）
    Lv00Prop** assumptions;
    size_t assumption_count;
    
    // 目标命题
    Lv00Prop* goal;
    
    // 公理包（对象逻辑选择）
    Lv00AxiomPackage* axiom_pkg;
    
    // 证明状态
    void* proof_state;  // 后端特定状态
};

// ============ API 声明 ============

// 类型操作
Lv00GeoType lv00_gtype_point(void);
Lv00GeoType lv00_gtype_line(void);
Lv00GeoType lv00_gtype_circle(void);
Lv00GeoType lv00_gtype_var(const char* name);
Lv00GeoType lv00_gtype_set(Lv00GeoType elem);
int lv00_gtype_equal(Lv00GeoType a, Lv00GeoType b);
int lv00_gtype_unify(Lv00GeoType a, Lv00GeoType b, Lv00Subst** subst);

// 类型环境操作
Lv00TypeEnv* lv00_type_env_create(Lv00TypeEnv* parent);
void lv00_type_env_add(Lv00TypeEnv* env, const char* name, Lv00GeoType type);
Lv00GeoType lv00_type_env_lookup(Lv00TypeEnv* env, const char* name);
void lv00_type_env_destroy(Lv00TypeEnv* env);

// 命题构造
Lv00Prop* lv00_prop_parallel(Lv00Term* l1, Lv00Term* l2);
Lv00Prop* lv00_prop_perp(Lv00Term* l1, Lv00Term* l2);
Lv00Prop* lv00_prop_on(Lv00Term* p, Lv00Term* obj);
Lv00Prop* lv00_prop_congruent(Lv00Term* a, Lv00Term* b);
Lv00Prop* lv00_prop_between(Lv00Term* a, Lv00Term* b, Lv00Term* c);
Lv00Prop* lv00_prop_exists(const char* vname, Lv00GeoType vtype, Lv00Prop* body);
Lv00Prop* lv00_prop_forall(const char* vname, Lv00GeoType vtype, Lv00Prop* body);
Lv00Prop* lv00_prop_imp(Lv00Prop* ant, Lv00Prop* cons);
Lv00Prop* lv00_prop_and(Lv00Prop* left, Lv00Prop* right);
Lv00Prop* lv00_prop_or(Lv00Prop* left, Lv00Prop* right);

// 证明上下文操作
Lv00ProofContext* lv00_pcontext_create(Lv00AxiomPackage* pkg);
void lv00_pcontext_assume(Lv00ProofContext* ctx, Lv00Prop* prop);
Lv00Prop* lv00_pcontext_goal(Lv00ProofContext* ctx);
void lv00_pcontext_destroy(Lv00ProofContext* ctx);

#endif // LV00_GEO_METALOGIC_H
```

### 3.2 Isar 风格证明 DSL 设计

```c
// proof_isar.h - Isar 风格证明 DSL

#ifndef LV00_PROOF_ISAR_H
#define LV00_PROOF_ISAR_H

#include <lv00.h>
#include <lv00/geo_metalogic.h>

// ============ Isar 证明结构 ============

typedef enum {
    LV00_ISAR_HAVE,    // have P - 引入中间命题
    LV00_ISAR_FROM,    // from facts - 从事实出发
    LV00_ISAR_USING,   // using lemmas - 使用引理
    LV00_ISAR_BY,      // by method - 用方法证明
    LV00_ISAR_SHOW,    // show P - 证明目标
    LV00_ISAR_QED,     // qed - 证明完成
    LV00_ISAR_FIX,     // fix x - 引入变量
    LV00_ISAR_ASSUME,  // assume P - 引入假设
    LV00_ISAR_NOTE     // note facts - 命名事实
} Lv00IsarCommand;

typedef struct Lv00IsarProof Lv00IsarProof;
struct Lv00IsarProof {
    // 证明命令列表
    struct {
        Lv00IsarCommand cmd;
        union {
            struct { Lv00Prop* prop; } have;
            struct { Lv00Prop** facts; size_t n; } from;
            struct { char** lemma_names; size_t n; } using_;
            struct { char* method_name; char** params; size_t n; } by;
            struct { Lv00Prop* prop; } show;
            struct { char* vname; Lv00GeoType vtype; } fix;
            struct { Lv00Prop* prop; } assume;
            struct { char* name; Lv00Prop** facts; size_t n; } note;
        } params;
        Lv00IsarProof* subproof;  // 子证明
    }* commands;
    size_t command_count;
};

// ============ 证明方法 ============

typedef enum {
    LV00_METHOD_AUTO,     // auto - 自动证明
    LV00_METHOD_SIMP,      // simp - 化简
    LV00_METHOD_GEOMETRY,  // geometry - 几何自动证明
    LV00_METHOD_ALGEBRA,   // algebra - 代数化简
    LV00_METHOD_BLAST,     // blast - 一阶逻辑证明
    LV00_METHOD_RULE,      // rule xxx - 应用规则
    LV00_METHOD_MESON,     // meson - MEsoteric SOunded eNough (一阶逻辑)
    LV00_METHOD_ARITH      // arith - 算术证明
} Lv00ProofMethod;

typedef struct Lv00MethodArgs {
    char** modifiers;  // simp: add:, del:, only:
    char** lemmas;     // using lemmas
    int timeout_ms;
} Lv00MethodArgs;

// ============ API 声明 ============

// Isar 证明构建
Lv00IsarProof* lv00_isar_proof_create(void);
void lv00_isar_fix(Lv00IsarProof* proof, const char* vname, Lv00GeoType vtype);
void lv00_isar_assume(Lv00IsarProof* proof, Lv00Prop* prop);
void lv00_isar_have(Lv00IsarProof* proof, Lv00Prop* prop);
void lv00_isar_show(Lv00IsarProof* proof, Lv00Prop* prop);
void lv00_isar_from(Lv00IsarProof* proof, Lv00Prop** facts, size_t n);
void lv00_isar_using(Lv00IsarProof* proof, char** lemmas, size_t n);
void lv00_isar_by(Lv00IsarProof* proof, Lv00ProofMethod method, const Lv00MethodArgs* args);
void lv00_isar_note(Lv00IsarProof* proof, const char* name, Lv00Prop** facts, size_t n);
void lv00_isar_qed(Lv00IsarProof* proof);

// Isar 证明执行
typedef enum {
    LV00_ISAR_SUCCESS,
    LV00_ISAR_UNPROVABLE,
    LV00_ISAR_TYPE_ERROR,
    LV00_ISAR_SYNTAX_ERROR
} Lv00IsarResult;

Lv00IsarResult lv00_isar_execute(Lv00IsarProof* proof, 
                                   Lv00ProofContext* ctx,
                                   Lv00Proof** out_proof);
char* lv00_isar_error_msg(Lv00IsarResult result);

// 清理
void lv00_isar_proof_destroy(Lv00IsarProof* proof);

#endif // LV00_PROOF_ISAR_H
```

### 3.3 使用示例

```c
// 示例：使用 Isar 风格证明平行四边形对角线定理

#include <lv00.h>
#include <lv00/geo_metalogic.h>
#include <lv00/proof_isar.h>

Lv00IsarResult prove_parallelogram_diagonal(Lv00ProofContext* ctx) {
    // 构建 Isar 风格证明
    Lv00IsarProof* proof = lv00_isar_proof_create();
    
    // fix A B C D :: point
    lv00_isar_fix(proof, "A", lv00_gtype_point());
    lv00_isar_fix(proof, "B", lv00_gtype_point());
    lv00_isar_fix(proof, "C", lv00_gtype_point());
    lv00_isar_fix(proof, "D", lv00_gtype_point());
    
    // assume parallelogram A B C D
    Lv00Prop* para_prop = lv00_prop_parallelogram("A", "B", "C", "D");
    lv00_isar_assume(proof, para_prop);
    
    // have "AB ∥ CD"
    Lv00Prop* ab_para_cd = lv00_prop_parallel(
        lv00_term_const("mk_segment", "A", "B"),
        lv00_term_const("mk_segment", "C", "D")
    );
    lv00_isar_have(proof, ab_para_cd);
    lv00_isar_from(proof, &para_prop, 1);
    lv00_isar_by(proof, LV00_METHOD_GEOMETRY, NULL);
    
    // have "AD ∥ BC"
    Lv00Prop* ad_para_bc = lv00_prop_parallel(
        lv00_term_const("mk_segment", "A", "D"),
        lv00_term_const("mk_segment", "B", "C")
    );
    lv00_isar_have(proof, ad_para_bc);
    lv00_isar_from(proof, &para_prop, 1);
    lv00_isar_by(proof, LV00_METHOD_GEOMETRY, NULL);
    
    // show diagonal AC ⟷ diagonal BD
    Lv00Prop* goal = lv00_prop_imp(
        lv00_prop_diagonal("A", "C"),
        lv00_prop_diagonal("B", "D")
    );
    lv00_isar_show(proof, goal);
    lv00_isar_from(proof, &ab_para_cd, 1);
    lv00_isar_using(proof, (char*[]){"parallelogram_diagonal_theorem"}, 1);
    lv00_isar_by(proof, LV00_METHOD_GEOMETRY, NULL);
    
    // qed
    lv00_isar_qed(proof);
    
    // 执行证明
    Lv00Proof* out_proof;
    Lv00IsarResult result = lv00_isar_execute(proof, ctx, &out_proof);
    
    lv00_isar_proof_destroy(proof);
    return result;
}
```

---

## 4. 实现路线图

### 4.1 分阶段实现计划

| 阶段 | 名称 | 时间 | 核心任务 | 交付物 | 优先级 |
|:---:|:---|:---:|:---|:---|:---:|
| 1 | 几何元逻辑层 | 第1-3周 | 类型系统、项系统、命题系统 | `geo_metalogic.h` (约500行) | P0 |
| 2 | Isar DSL 解析器 | 第4-6周 | 语法解析器、证明构建器 | `proof_isar.h` + parser (约400行) | P0 |
| 3 | 证明方法扩展 | 第7-9周 | 方法系统、可组合策略 | `proof_method.h` (约300行) | P1 |
| 4 | 公理包框架 | 第10-12周 | 公理包加载、切换机制 | `axiom_pkg.h` + Hilbert/Tarski 包 | P1 |
| 5 | AFP 风格库建设 | 持续 | 几何形式化理论库 | geometry AFP 子库 | P2 |

### 4.2 依赖关系

```
阶段1 (geo_metalogic.h)
    │
    ├── 依赖：无
    │
    ▼
阶段2 (proof_isar.h)
    │
    ├── 依赖：阶段1
    │   └── 需要：parser generator (yacc/bison 或 lemon)
    │
    ▼
阶段3 (proof_method.h)
    │
    ├── 依赖：阶段1 + 阶段2
    │   └── 需要：现有 tactics.h 集成
    │
    ▼
阶段4 (axiom_pkg.h)
    │
    ├── 依赖：阶段1
    │   └── 需要：现有 axiom.h 扩展
    │
    ▼
阶段5 (geometry AFP)
    │
    ├── 依赖：阶段1-4
    │   └── 需要：公理化几何理论
    │
    ▼
完成
```

---

## 5. 附录

### 5.1 参考资源

- Isabelle 官方文档：https://isabelle.in.tum.de/documentation.html
- Isar 参考手册：https://isabelle.in.tum.de/doc/isar-ref.pdf
- AFP 档案库：https://www.isa-afp.org/
- Programming and Proving in Isabelle/HOL: https://isabelle.in.tum.de/doc/prog-prove.pdf

### 5.2 术语表

| 术语 | 英文 | 定义 |
|:---|:---|:---|
| 元逻辑 | Meta-Logic | 用于表达其他逻辑的基础逻辑系统 |
| 对象逻辑 | Object Logic | 在元逻辑之上定义的具体逻辑系统 |
| Isar | Isabelle's Archimedean Reasoning | Isabelle 的声明式证明语言 |
| Proof Method | Proof Method | 证明策略/方法 |
| AFP | Archive of Formal Proofs | Isabelle 形式化证明档案库 |
| LCF 风格 | LCF-style | 基于小型可信内核的定理证明器设计 |

### 5.3 许可证兼容性

Isabelle 使用 BSD-3-Clause 许可证，Lv-00 可自由参考其设计理念。

---

*文档生成日期：2026-05-28*
*参考版本：Isabelle2024*
