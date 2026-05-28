# F* - 证明导向编程语言参考文档

> **项目名称**：F* (FStar)
> **项目链接**：https://github.com/FStarLang/FStar
> **项目链接**：https://fstar-lang.org/
> **项目类型**：证明导向函数式编程语言
> **语言/技术栈**：ML 风格语法、依赖类型、Z3 SMT 求解器
> **最后更新**：2024年持续活跃
> **文档版本**：v1.0
> **适用层级**：第4层（多策略自动推理层）

---

## 1. 项目概述

### 1.1 项目背景与定位

F*（发音 "F star"）是由 Microsoft Research 和 INRIA 联合开发的函数式编程语言，支持**依赖类型**和**程序验证**。F* 的核心理念是将程序与规格统一在同一种语言中——类型可以包含程序级别的断言，实现"类型即规格"。

F* 已用于验证多个关键系统组件：
- **EverCrypt**：经过验证的密码学算法库
- **HACL***：经过验证的加密库
- **Vale**：经过验证的汇编代码生成器
- **Project Everest**：构建经过验证的 HTTPS 协议栈

### 1.2 核心语言特性

```fstar
// 依赖类型：类型包含程序级别的断言
val factorial: n:nat{n >= 0} -> nat{factorial_result: factorial_result >= 1}

// 纯粹规格
let rec factorial (n:nat) : Tot nat = 
  if n = 0 then 1 else n * factorial (n - 1)

// 带有前置条件和后置条件的函数
val max: a:int -> b:int -> Pure int
  (requires true)
  (ensures (fun r -> r >= a && r >= b && (r = a || r = b)))

let max a b = if a >= b then a else b

// SMT 求解器辅助证明
val plus_comm: x:nat -> y:nat -> Lemma (x + y = y + x)
let rec plus_comm x y = match x with
  | 0 -> ()
  | _ -> plus_comm (x - 1) y; assert (x + y = y + x)
```

### 1.3 验证架构

F* 的验证管道：

```
┌─────────────┐
│ F* 源代码    │
└──────┬──────┘
       │ 解析 + 类型检查
       ▼
┌─────────────┐
│ KC (KreMLin)│
└──────┬──────┘
       │ 提取到 OCaml/C/KLow*
       ▼
┌─────────────┐
│ Z3 求解器   │
└──────┬──────┘
       │ SMT 查询
       ▼
┌─────────────┐
│ 验证结果    │
└─────────────┘
```

---

## 2. 核心借鉴点

### 2.1 依赖类型系统

F* 的依赖类型允许类型包含运行时值：

```fstar
// 依赖函数类型：参数影响返回类型
val vector_get: #a:Type -> #n:nat -> v:vec a n -> i:nat{i < n} -> a

// 依存类型确保数组访问安全
val sum: #n:nat -> vec int n -> Tot int
let rec sum #n v = match v with
  | [] -> 0
  | x :: xs -> x + sum xs
```

**Lv-00 借鉴价值**：几何证明中可使用依赖类型表达几何约束——例如，线段长度非负、三点不共线等。

### 2.2 Ghost 代码与证明

F* 支持在类型层面嵌入证明（ghost 代码）：

```fstar
// ghost 参数不参与运行时计算
val sort: #n:nat -> vec int n -> Tot (vec int n)
  (ensures (fun r -> 
    sorted r &&  // 输出已排序
    perm (r, #n) // 输入输出的排列关系
  ))

// 证明引理
let lemma_perm_transitive () : Lemma (forall #n. perm #n)
  = admit () // 简化：接受为真
```

**Lv-00 借鉴价值**：几何证明可分离为两部分：
- **计算部分**：几何对象构造
- **证明部分**：几何约束验证

### 2.3 核心借鉴点对照表

| F* 特性 | Lv-00 现有能力 | Lv-00 借鉴方案 |
|:---|:---|:---|
| 依赖类型 | `type.h` 基础类型 | 新增依赖类型系统，支持几何约束 |
| Ghost 代码 | 分离的 proof.h | 统一类型系统中的证明嵌入 |
| Pure/Effect 系统 | 无 Effect 区分 | 新增 Effect 标记（Pure/Geo/IO） |
| SMT 集成 | Z3 已有 | 增强 Z3 几何约束查询 |
| 验证器后端 | `kernel.h` | 参考 F* 内核设计增强可靠性 |

---

## 3. Lv-00 映射方案

### 3.1 几何依赖类型系统设计

```c
// geo_dependent_types.h - 几何依赖类型系统

#ifndef LV00_GEO_DEPENDENT_TYPES_H
#define LV00_GEO_DEPENDENT_TYPES_H

#include <lv00.h>

// ============ 几何约束类型 ============

// 几何约束表达式（用于依赖类型）
typedef struct Lv00GeoConstraint Lv00GeoConstraint;
struct Lv00GeoConstraint {
    Lv00ConstraintTag tag;
    union {
        // 数值约束
        struct { float value; } non_negative;     // >= 0
        struct { float value; float lower; } range;
        
        // 几何约束
        struct { Lv00Term* p; Lv00Term* l; } on_line;
        struct { Lv00Term* l1; Lv00Term* l2; } parallel;
        struct { Lv00Term* l1; Lv00Term* l2; } perpendicular;
        struct { Lv00Term* a; Lv00Term* b; Lv00Term* c; } collinear;
        struct { Lv00Term* a; Lv00Term* b; Lv00Term* c; } between;
        
        // 复合约束
        struct { Lv00GeoConstraint* left; Lv00GeoConstraint* right; } and;
        struct { Lv00GeoConstraint* pred; } not;
    } data;
};

// 约束标签
typedef enum {
    LV00_CONSTRAINT_TRUE,
    LV00_CONSTRAINT_FALSE,
    LV00_CONSTRAINT_NON_NEGATIVE,
    LV00_CONSTRAINT_RANGE,
    LV00_CONSTRAINT_ON_LINE,
    LV00_CONSTRAINT_PARALLEL,
    LV00_CONSTRAINT_PERPENDICULAR,
    LV00_CONSTRAINT_COLLINEAR,
    LV00_CONSTRAINT_BETWEEN,
    LV00_CONSTRAINT_CONGRUENT,
    LV00_CONSTRAINT_EQUAL,
    LV00_CONSTRAINT_AND,
    LV00_CONSTRAINT_OR,
    LV00_CONSTRAINT_NOT,
    LV00_CONSTRAINT_IMP
} Lv00ConstraintTag;

// ============ 依赖类型表示 ============

typedef struct Lv00DepType Lv00DepType;
struct Lv00DepType {
    Lv00GeoType base_type;    // 基类型
    
    // 依赖约束（可为 NULL 表示无约束）
    Lv00GeoConstraint* constraint;
    
    // 参数绑定（用于依赖函数类型）
    struct {
        char* param_name;
        Lv00GeoType param_type;
        struct {
            Lv00GeoConstraint* constraint;
        }* constraint_in_scope;  // 参数范围内的约束
    }* params;
    size_t param_count;
};

// ============ 依赖函数类型 ============

typedef struct Lv00DepFnType Lv00DepFnType;
struct Lv00DepFnType {
    char* param_name;          // 参数名
    Lv00DepType param_type;    // 参数类型（可含约束）
    Lv00DepType result_type;   // 返回类型（可依赖参数）
    
    // Effect 标记
    Lv00EffectTag effect;
};

// Effect 标签
typedef enum {
    LV00_EFFECT_PURE,     // 纯计算，无副作用
    LV00_EFFECT_GEOM,      // 几何构造
    LV00_EFFECT_GHOST,     // Ghost 计算（不生成代码）
    LV00_EFFECT_VERIFY     // 验证/证明计算
} Lv00EffectTag;

// ============ API 声明 ============

// 约束构造
Lv00GeoConstraint* lv00_constraint_true(void);
Lv00GeoConstraint* lv00_constraint_non_negative(float value);
Lv00GeoConstraint* lv00_constraint_range(float value, float lower, float upper);
Lv00GeoConstraint* lv00_constraint_on_line(Lv00Term* p, Lv00Term* l);
Lv00GeoConstraint* lv00_constraint_parallel(Lv00Term* l1, Lv00Term* l2);
Lv00GeoConstraint* lv00_constraint_perpendicular(Lv00Term* l1, Lv00Term* l2);
Lv00GeoConstraint* lv00_constraint_collinear(Lv00Term* a, Lv00Term* b, Lv00Term* c);
Lv00GeoConstraint* lv00_constraint_between(Lv00Term* a, Lv00Term* b, Lv00Term* c);
Lv00GeoConstraint* lv00_constraint_and(Lv00GeoConstraint* left, Lv00GeoConstraint* right);
Lv00GeoConstraint* lv00_constraint_not(Lv00GeoConstraint* pred);

// 依赖类型构造
Lv00DepType lv00_dep_type_base(Lv00GeoType base);
Lv00DepType lv00_dep_type_with_constraint(Lv00GeoType base, Lv00GeoConstraint* c);
Lv00DepType lv00_dep_type_with_param(const char* name, Lv00DepType param_type, 
                                      Lv00DepType result_type);

// 约束求解与验证
int lv00_constraint_satisfiable(Lv00GeoConstraint* c, Lv00ProofContext* ctx);
int lv00_constraint_entails(Lv00GeoConstraint* premise, Lv00GeoConstraint* conclusion);
void lv00_constraint_to_smt(Lv00GeoConstraint* c, Lv00SMTExpr* out_smt);

// Ghost 证明构造
typedef struct Lv00GhostExpr Lv00GhostExpr;
Lv00GhostExpr* lv00_ghost_lemma(const char* name, Lv00GeoConstraint* conclusion);
void lv00_ghost_admit(Lv00GhostExpr** out);

// 清理
void lv00_constraint_destroy(Lv00GeoConstraint* c);
void lv00_dep_type_destroy(Lv00DepType* t);

#endif // LV00_GEO_DEPENDENT_TYPES_H
```

### 3.2 几何 Effect 系统

```c
// geo_effects.h - 几何 Effect 系统

#ifndef LV00_GEO_EFFECTS_H
#define LV00_GEO_EFFECTS_H

#include <lv00.h>
#include <lv00/geo_dependent_types.h>

// ============ Effect 层次结构 ============

typedef enum {
    LV00_EFFECT_TOTAL,    // 全Total函数：保证终止
    LV00_EFFECT_PURE,     // 纯函数：无副作用
    LV00_EFFECT_GEOM,     // 几何构造副作用
    LV00_EFFECT_STATE,    // 状态副作用
    LV00_EFFECT_NONDET,   // 非确定性
    LV00_EFFECT_VERIFY    // 验证/证明
} Lv00EffectKind;

// Effect 描述
typedef struct {
    Lv00EffectKind kind;
    Lv00DepType result_type;       // 返回类型
    Lv00GeoConstraint* pre;       // 前置条件
    Lv00GeoConstraint* post;      // 后置条件
    Lv00GeoConstraint* raises;    // 可能的异常
} Lv00Effect;

// 预定义的 Effect 描述
extern const Lv00Effect LV00_EFFECT_PURE_DESC;
extern const Lv00Effect LV00_EFFECT_GEOM_DESC;
extern const Lv00Effect LV00_EFFECT_VERIFY_DESC;

// ============ Pure 函数（纯 Total 函数）============

#define LV00_PURE(t, post) \
    __attribute__((annotate("effect:Pure"), \
                    annotate("post:" #post))) \
    t

// 示例：纯几何查询
LV00_PURE(bool, result == true || result == false)
lv00_is_parallel(Lv00Line* l1, Lv00Line* l2);

// ============ 几何构造 Effect ============

#define LV00_GEOM(t, pre, post) \
    __attribute__((annotate("effect:Geom"), \
                    annotate("pre:" #pre), \
                    annotate("post:" #post))) \
    t

// 示例：几何构造
LV00_GEOM(Lv00Point*, 
          true,  // 前置条件
          result != NULL && lv00_point_valid(result))  // 后置条件
lv00_construct_midpoint(Lv00Point* a, Lv00Point* b);

// ============ 验证 Effect（证明导向）============

#define LV00_VERIFY(t, pre, post) \
    __attribute__((annotate("effect:Verify"), \
                    annotate("pre:" #pre), \
                    annotate("post:" #post))) \
    t

// 示例：验证定理
LV00_VERIFY(Lv00Proof*,
            lv00_constraint_forall("x", lv00_gtype_point(),
                lv00_constraint_forall("y", lv00_gtype_point(),
                    lv00_constraint_exists("z", lv00_gtype_point(),
                        lv00_constraint_collinear("x", "y", "z")))),
            result != NULL)  // 证明成功返回非 NULL
lv00_prove_three_points_exist(void);

// ============ API 声明 ============

// Effect 检查
int lv00_check_effect(Lv00EffectKind effect, Lv00DepType* result_type,
                     Lv00GeoConstraint* pre, Lv00GeoConstraint* post);

// Effect 提升
Lv00EffectKind lv00_effect_join(Lv00EffectKind e1, Lv00EffectKind e2);
Lv00EffectKind lv00_effect_sub_effect(Lv00EffectKind sub, Lv00EffectKind sup);

// SMT 验证
int lv00_verify_postcondition(Lv00DepType* fn_type, void* fn_result,
                              Lv00GeoConstraint* post);

#endif // LV00_GEO_EFFECTS_H
```

### 3.3 使用示例

```c
// 示例：使用依赖类型验证几何构造

#include <lv00.h>
#include <lv00/geo_dependent_types.h>
#include <lv00/geo_effects.h>

// 使用依赖类型约束参数
Lv00Point* lv00_construct_triangle_centroid(
    Lv00Point* a,   // 第一个顶点
    Lv00Point* b,   // 第二个顶点
    Lv00Point* c    // 第三个顶点
    // 隐式约束：a、b、c 不共线
) 
    // 前置条件：三个点存在且不共线
    __attribute__((annotate("requires: a != NULL && b != NULL && c != NULL && !collinear(a,b,c)")))
    // 后置条件：返回值是三角形的重心
    __attribute__((annotate("ensures: is_centroid(result, a, b, c)")))
{
    // 计算边中点
    Lv00Point* m_ab = lv00_construct_midpoint(a, b);
    Lv00Point* m_ac = lv00_construct_midpoint(a, c);
    
    // 构造中线交点
    Lv00Line* median1 = lv00_construct_line(m_ab, c);
    Lv00Line* median2 = lv00_construct_line(m_ac, b);
    Lv00Point* centroid = lv00_intersect_lines(median1, median2);
    
    // 验证后置条件
    #ifdef LV00_VERIFY
    lv00_verify_postcondition(
        &(Lv00DepType){ .base_type = lv00_gtype_point() },
        centroid,
        lv00_constraint_centroid(centroid, a, b, c)
    );
    #endif
    
    return centroid;
}

// 证明引理
void lv00_lemma_centroid_exists(
    Lv00Point* a,
    Lv00Point* b,
    Lv00Point* c
) 
    // Ghost 函数：编译时验证，不产生运行时代码
    __attribute__((annotate("effect:Ghost")))
{
    // 构造性证明：给出具体的重心构造
    Lv00Point* centroid = lv00_construct_triangle_centroid(a, b, c);
    
    // 验证重心性质
    // 1. 重心在三条中线上
    lv00_assert(lv00_point_on_line(centroid, lv00_construct_line(a, lv00_construct_midpoint(b, c))));
    lv00_assert(lv00_point_on_line(centroid, lv00_construct_line(b, lv00_construct_midpoint(a, c))));
    lv00_assert(lv00_point_on_line(centroid, lv00_construct_line(c, lv00_construct_midpoint(a, b))));
    
    // 2. 重心将每条中线分为 2:1 的比例
    float ratio = lv00_measure_distance(centroid, a) / 
                  lv00_measure_distance(centroid, lv00_construct_midpoint(b, c));
    lv00_assert_float_eq(ratio, 2.0f / 3.0f);
}
```

---

## 4. 实现路线图

### 4.1 分阶段实现计划

| 阶段 | 名称 | 时间 | 核心任务 | 交付物 | 优先级 |
|:---:|:---|:---:|:---|:---|:---:|
| 1 | 几何约束系统 | 第1-3周 | 约束表达式、约束求解 | `geo_dependent_types.h` (约400行) | P0 |
| 2 | Effect 系统 | 第4-5周 | Effect 标记、检查 | `geo_effects.h` (约200行) | P1 |
| 3 | SMT 集成 | 第6-8周 | 约束→SMT 转换、Z3 查询 | `smt_geometry.h` (约300行) | P0 |
| 4 | 验证框架 | 第9-11周 | 后置条件检查、引理系统 | `verify.h` (约300行) | P1 |
| 5 | Ghost 编译 | 第12-14周 | Ghost 代码消除、验证优化 | 编译器插件 | P2 |

### 4.2 依赖关系

```
阶段1 (geo_dependent_types.h)
    │
    ├── 依赖：无
    │
    ▼
阶段2 (geo_effects.h)
    │
    ├── 依赖：阶段1
    │
    ▼
阶段3 (smt_geometry.h)
    │
    ├── 依赖：阶段1
    │   └── 需要：Z3 库
    │
    ▼
阶段4 (verify.h)
    │
    ├── 依赖：阶段1 + 阶段3
    │
    ▼
阶段5 (编译器插件)
    │
    ├── 依赖：阶段2 + 阶段4
    │
    ▼
完成
```

---

## 5. 附录

### 5.1 参考资源

- F* 官方文档：https://fstar-lang.org/tutorial/
- F* GitHub：https://github.com/FStarLang/FStar
- Verified Cryptography：https://project-everest.github.io/
- Programming with Dependent Types in F*：https://arxiv.org/abs/1311.5402

### 5.2 术语表

| 术语 | 英文 | 定义 |
|:---|:---|:---|
| 依赖类型 | Dependent Type | 类型依赖于值的类型系统 |
| Ghost 代码 | Ghost Code | 仅用于验证、不参与运行时计算 |
| Effect | Effect | 程序副作用的抽象 |
| Pure 函数 | Pure Function | 无副作用的函数 |
| Tot | Total | 保证终止的函数 |
| SMT | Satisfiability Modulo Theories | 可满足性模理论 |

### 5.3 许可证兼容性

F* 使用 Apache-2.0 许可证，Lv-00 可自由参考和借鉴。

---

*文档生成日期：2026-05-28*
*参考版本：F* 1.12*
