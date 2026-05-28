# MathComp/SSReflect - Coq 数学组件库参考文档

> **项目名称**：Mathematical Components (MathComp) + SSReflect
> **项目链接**：https://github.com/math-comp/math-comp
> **项目链接**：https://math-comp.github.io/
> **项目类型**：Coq 的数学形式化库 / 证明语言扩展
> **语言/技术栈**：Coq、SSReflect 证明语言、MathComp 库
> **最后更新**：2024年持续活跃
> **文档版本**：v1.0
> **适用层级**：第4层（多策略自动推理层）、第2层（基础几何公理层）

---

## 1. 项目概述

### 1.1 项目背景与定位

Mathematical Components（MathComp）是 Coq 证明助手的数学形式化库家族，由 Inria 和 École Polytechnique 联合开发。该项目的核心成果包括：

- **SSReflect**：Gérard Huet 和 Georges Gonthier 开发的证明语言扩展
- **MathComp 库**：代数学、线性代数学、数论等数学领域的形式化
- **实际应用**：四色定理（Four Color Theorem）、奇阶定理（Odd Order Theorem）的形式化证明

MathComp 的核心理念是**"小内核 + 大库"**——通过精心设计的抽象层和库支持大规模形式化数学。

### 1.2 SSReflect 证明语言

SSReflect 是 Coq 的证明语言扩展，以其紧凑的符号和强大的自动化著称：

```coq
(* SSReflect 风格证明 *)
From mathcomp Require Import ssreflect ssrfun ssrbool eqtype.

Theorem comm_plus (n m : nat) : n + m = m + n.
Proof.
by elim: n m => [| n IHn] m; rewrite //= ?addn0 addSn IHn.
Qed.

(* 使用 HOAS 和视图 *)
Lemma subset_union (T : finType) (A B : {set T}) :
  A \subset A ∪ B.
Proof. by rewrite subsetIl. Qed.
```

### 1.3 MathComp 库结构

```
Mathematical Components
├── ssreflect/     - SSReflect 基础库
├── ssrfun/       - 函数式编程扩展
├── ssrbool/      - 布尔表达式与决策过程
├── eqtype/       - 可比较类型
├── choice/       - 选择类型
├── fintype/      - 有限类型
├── bigop/        - 大运算符
├── algebra/      - 代数结构
│   ├── ssralg/   - 半环、代数
│   ├── ssrnum/   - 数值类型
│   └── matrix/    - 矩阵
├── field/        - 域论
├── fingroup/     - 有限群
├── galois/       - Galois 理论
└── character/    - 特征标理论
```

---

## 2. 核心借鉴点

### 2.1 SSReflect 的证明模式

SSReflect 的核心创新是**视图机制（View Mechanism）**和**视图配对（View Matching）**：

```coq
(* 视图声明 *)
Lemma negbNE b : b = ~~ b -> b = false.
Proof. by case: b => //= [] []. Qed.

(* 使用视图翻转逻辑 *)
Goal forall b : bool, b || ~~ b = true.
Proof.
move=> b.
rewrite orbN  //= ?  (* 使用否定视图 *)
//=.
Qed.
```

**Lv-00 借鉴价值**：几何证明可借鉴视图机制，定义几何事实的视图变换。

### 2.2 继承与混合格式

MathComp 使用**打包继承（Packaged Inheritance）**组织数学结构：

```coq
(* 混合格式：Structure = Pack + Class *)
Module Ring.
  (* Class: 参数化接口 *)
  Class ring_of (R : Type) := {
    zero : R;
    one : R;
    add : R -> R -> R;
    mul : R -> R -> R;
    addrA : associative add;
    add0r : left_id zero add;
    ...
  }.
  
  (* Pack: 实际的打包类型 *)
  Structure ring := Pack {
    sort : Type;
    mixin_of : ring_of sort
  }.
End Ring.
```

**Lv-00 借鉴价值**：几何公理包可借鉴此模式，实现公理包的**混合格式**——Class 定义公理接口，Pack 定义具体实现。

### 2.3 核心借鉴点对照表

| MathComp/SSReflect 特性 | Lv-00 现有能力 | Lv-00 借鉴方案 |
|:---|:---|:---|
| SSReflect 符号系统 | `proof_parser.y` 基础语法 | 借鉴 SSReflect 紧凑符号 |
| 视图机制 | 无视图系统 | 新增几何视图变换 |
| 结构继承 | `axiom.h` 单一结构 | 实现公理包继承系统 |
| 大运算符 | 无 | 实现几何大运算符 |
| 类型类（Class） | 无 | 实现几何类型类系统 |

---

## 3. Lv-00 映射方案

### 3.1 几何视图机制设计

```c
// geo_views.h - 几何视图机制

#ifndef LV00_GEO_VIEWS_H
#define LV00_GEO_VIEWS_H

#include <lv00.h>

// ============ 几何视图类型 ============

typedef enum {
    LV00_VIEW_PARALLEL,       // 平行视图
    LV00_VIEW_PERPENDICULAR,  // 垂直视图
    LV00_VIEW_CONGRUENT,      // 全等视图
    LV00_VIEW_SIMILAR,        // 相似视图
    LV00_VIEW_COLLINEAR,      // 共线视图
    LV00_VIEW_CYCLIC,        // 圆周视图
    LV00_VIEW_AREA,           // 面积视图
    LV00_VIEW_ANGLE          // 角度视图
} Lv00GeoViewTag;

// 视图项（View Item）
typedef struct Lv00ViewItem Lv00ViewItem;
struct Lv00ViewItem {
    Lv00GeoViewTag tag;
    union {
        struct { Lv00Term* l1; Lv00Term* l2; } parallel;
        struct { Lv00Term* l1; Lv00Term* l2; } perp;
        struct { Lv00Term* seg1; Lv00Term* seg2; } congruent;
        struct { Lv00Term* tri1; Lv00Term* tri2; } similar;
        struct { Lv00Term* p; Lv00Term* q; Lv00Term* r; } collinear;
        struct { Lv00Term* a; Lv00Term* b; Lv00Term* c; Lv00Term* d; } cyclic;
        struct { Lv00Term* poly; float area; } area;
        struct { Lv00Term* l; Lv00Term* m; float angle; } angle;
    } data;
};

// 视图匹配器
typedef struct Lv00ViewMatcher Lv00ViewMatcher;
struct Lv00ViewMatcher {
    // 模式
    Lv00ViewItem* pattern;
    
    // 匹配时的转换
    void* (*transform)(Lv00ViewItem* matched, void* ctx);
    
    // 视图翻转（inverse）
    Lv00ViewMatcher* inverse;
};

// 视图库
typedef struct Lv00ViewLibrary Lv00ViewLibrary;
struct Lv00ViewLibrary {
    Lv00ViewMatcher** matchers;
    size_t matcher_count;
};

// ============ 视图声明 API ============

// 声明视图
void lv00_view_declare(Lv00ViewLibrary* lib, 
                       Lv00ViewMatcher* matcher);

// 翻转视图（逻辑翻转）
Lv00ViewItem* lv00_view_negate(Lv00ViewItem* item);

// 视图等价转换
Lv00ViewItem* lv00_view_to_parallel(Lv00ViewItem* item);
Lv00ViewItem* lv00_view_to_angle(Lv00ViewItem* item);

// ============ 视图应用 API ============

// 在证明中应用视图
typedef struct Lv00ViewStack Lv00ViewStack;
struct Lv00ViewStack {
    Lv00ViewItem** items;
    size_t count;
};

Lv00ViewStack* lv00_view_stack_create(void);
void lv00_view_stack_push(Lv00ViewStack* stack, Lv00ViewItem* item);
Lv00ViewItem* lv00_view_stack_pop(Lv00ViewStack* stack);

// 视图驱动的证明
typedef Lv00Proof* (*Lv00ViewProver)(Lv00ViewItem* item, void* ctx);

void lv00_prove_by_view(Lv00ViewStack* stack, 
                        Lv00ViewProver prover,
                        void* ctx);

// ============ 预定义几何视图 ============

// 预定义视图库
Lv00ViewLibrary* lv00_builtin_views_create(void);

// 预定义视图
extern const Lv00ViewMatcher LV00_VIEW_PARALLEL_TO_EQ;
extern const Lv00ViewMatcher LV00_VIEW_PERP_TO_ANGLE;
extern const Lv00ViewMatcher LV00_VIEW_CONG_TO_SIMILAR;
extern const Lv00ViewMatcher LV00_VIEW_AREA_TO_RATIO;

// ============ SSReflect 风格符号 ============

// SSReflect 紧凑符号宏
#define LV00_SSR_MOVE(v)      lv00_view_stack_push(ctx, v)
#define LV00_SSR_REWRITE(eqs) lv00_rewrite(eqs, ctx)
#define LV00_SSR_CASE(t)      lv00_case_analysis(t, ctx)
#define LV00_SSR_ELIM(x)      lv00_induction(x, ctx)
#define LV00_SSR_BY(m)        lv00_prove_by_method(m, ctx)
#define LV00_SSR_EXACT        lv00_exact_proof

// 示例
void ssr_style_proof_example(Lv00ProofContext* ctx) {
    // move=> H
    Lv00Prop* H = lv00_pcontext_goal(ctx);
    LV00_SSR_MOVE(H);
    
    // rewrite H1 H2
    LV00_SSR_REWRITE((char*[]){"H1", "H2"}, 2);
    
    // by rewrite H3
    LV00_SSR_BY("rewrite");
    LV00_SSR_REWRITE((char*[]){"H3"}, 1);
}

#endif // LV00_GEO_VIEWS_H
```

### 3.2 几何类型类系统

```c
// geo_typeclass.h - 几何类型类系统

#ifndef LV00_GEO_TYPECLASS_H
#define LV00_GEO_TYPECLASS_H

#include <lv00.h>

// ============ 类型类基础结构 ============

// 类型类实例
typedef struct Lv00TypeClassInst Lv00TypeClassInst;
struct Lv00TypeClassInst {
    const char* class_name;    // 类名
    const char* type_name;     // 实例化的类型名
    void* impl;               // 实现数据
    Lv00TypeClassInst* next;  // 优先级链
};

// 类型类定义
typedef struct Lv00TypeClass Lv00TypeClass;
struct Lv00TypeClass {
    const char* name;         // 类名
    
    // 类参数
    struct {
        const char* name;      // 参数名
        Lv00GeoType type;     // 参数类型
    }* params;
    size_t param_count;
    
    // 类方法（虚函数表）
    struct {
        const char* name;     // 方法名
        Lv00GeoType sig;      // 方法类型签名
        void* default_impl;   // 默认实现
    }* methods;
    size_t method_count;
    
    // 继承的父类
    Lv00TypeClass** parents;
    size_t parent_count;
    
    // 实例注册表
    Lv00TypeClassInst* instances;
};

// ============ 预定义几何类型类 ============

// 等距空间类型类（度量空间的几何版本）
extern const Lv00TypeClass LV00_TC_METRIC_SPACE;
#define LV00_METRIC_SPACE_STRUCT(T) \
    typedef struct { \
        float (*distance)(T a, T b);  /* 度量函数 */ \
        int (*equality)(T a, T b);    /* 相等判断 */ \
    } T##_metric_space

// 平面类型类（具有平面几何性质）
extern const Lv00TypeClass LV00_TC_PLANAR;
#define LV00_PLANAR_STRUCT(T) \
    typedef struct { \
        Lv00Term* (*mk_point)(float x, float y); \
        Lv00Term* (*mk_line)(Lv00Term* p1, Lv00Term* p2); \
        int (*is_parallel)(Lv00Term* l1, Lv00Term* l2); \
        int (*is_perp)(Lv00Term* l1, Lv00Term* l2); \
    } T##_planar

// 欧几里得空间类型类
extern const Lv00TypeClass LV00_TC_EUCLIDEAN;
#define LV00_EUCLIDEAN_STRUCT(T) \
    typedef struct { \
        LV00_METRIC_SPACE_STRUCT(T);  /* 继承度量空间 */ \
        LV00_PLANAR_STRUCT(T);        /* 继承平面 */ \
        float (*dot_product)(T v1, T v2);  /* 点积 */ \
        float (*cross_product)(T v1, T v2); /* 叉积 */ \
    } T##_euclidean

// 仿射空间类型类
extern const Lv00TypeClass LV00_TC_AFFINE;
#define LV00_AFFINE_STRUCT(T, V) \
    typedef struct { \
        T (*origin)(void); \
        V (*vector)(T p1, T p2);  /* 方向向量 */ \
        T (*translate)(T p, V v); /* 平移 */ \
    } T##_affine

// ============ 实例声明语法 ============

// 类型类实例声明宏
#define LV00_INSTANCE(class_name, type_name, ...) \
    static const class_name##_struct type_name##_instance = { \
        __VA_ARGS__ \
    }; \
    static void __attribute__((constructor)) \
    register_##class_name##_##type_name##_instance(void) { \
        lv00_tc_register_instance(&class_name, #type_name, &type_name##_instance); \
    }

// 示例：Point 的欧几里得空间实例
typedef Lv00Point* Point;

LV00_INSTANCE(LV00_TC_EUCLIDEAN, Point,
    .distance = point_distance,
    .equality = point_equal,
    .mk_point = point_create,
    .mk_line = point_to_line,
    .is_parallel = point_line_parallel,
    .is_perp = point_line_perp,
    .dot_product = point_dot,
    .cross_product = point_cross
);

// ============ 类型类调度 ============

// 方法查找
void* lv00_tc_lookup_method(Lv00TypeClass* cls, 
                            const char* method_name,
                            Lv00GeoType* type);

// 实例解析
void lv00_tc_resolve_instances(Lv00TypeClass* cls,
                               Lv00GeoType* arg_types,
                               Lv00TypeClassInst** out_inst);

// 约束求解
int lv00_tc_unify_constraints(Lv00TypeClass** constraints,
                              size_t n,
                              Lv00Subst** out_subst);

#endif // LV00_GEO_TYPECLASS_H
```

### 3.3 使用示例

```c
// 示例：使用视图机制证明平行线传递性

#include <lv00.h>
#include <v00/geo_views.h>
#include <lv00/geo_typeclass.h>

// SSReflect 风格的平行线传递性证明
Lv00Proof* prove_parallel_transitive(Lv00ProofContext* ctx,
                                     Lv00Term* l1, Lv00Term* l2, Lv00Term* l3) {
    // 创建视图栈
    Lv00ViewStack* stack = lv00_view_stack_create();
    
    // Goal: l1 ∥ l3 given l1 ∥ l2 and l2 ∥ l3
    // Proof:
    
    // move=> [H12 H23]  // 引入假设
    Lv00ViewItem* H12 = lv00_view_item_parallel(l1, l2);
    Lv00ViewItem* H23 = lv00_view_item_parallel(l2, l3);
    lv00_view_stack_push(stack, H12);
    lv00_view_stack_push(stack, H23);
    
    // rewrite {H12, H23} // 重写
    lv00_rewrite_context(ctx, (Lv00ViewItem*[]){H12, H23}, 2);
    
    // have {l1 ∥ l3} :  // 中间引理
    Lv00ViewItem* l1_para_l3 = lv00_view_item_parallel(l1, l3);
    lv00_view_stack_push(stack, l1_para_l3);
    
    // by move: l2 => {Heq}  // 引入 l2 并证明
    Lv00Term* l2_tmp = lv00_term_var("l2");
    
    // case/E  // 分类讨论
    Lv00ViewCaseResult* case_result = lv00_view_case_analysis(l2_tmp, ctx);
    
    // //= : 检查视图
    if (lv00_view_match(l1_para_l3, case_result->item)) {
        // rewrite H12 H23.
        // done.  // 证明完成
        return lv00_proof_done(ctx);
    }
    
    // by []  // 默认完成
    return lv00_proof_done(ctx);
}

// 示例：使用类型类调度计算几何量

float compute_triangle_area_typeclass(Lv00Point* a, Lv00Point* b, Lv00Point* c) {
    // 查找欧几里得空间实例
    Lv00TCInstance* inst;
    lv00_tc_resolve_instances(&LV00_TC_EUCLIDEAN, 
                             (Lv00GeoType*[]){lv00_typeof(a)}, 
                             &inst);
    
    // 获取叉积方法
    float (*cross)(Lv00Point*, Lv00Point*) = 
        (float (*)(Lv00Point*, Lv00Point*))
        lv00_tc_lookup_method(inst->cls, "cross_product", NULL);
    
    // 计算面积
    Lv00Vector* v1 = inst->methods.vector(b, a);
    Lv00Vector* v2 = inst->methods.vector(c, a);
    return 0.5f * cross(v1, v2);
}
```

---

## 4. 实现路线图

### 4.1 分阶段实现计划

| 阶段 | 名称 | 时间 | 核心任务 | 交付物 | 优先级 |
|:---:|:---|:---:|:---|:---|:---:|
| 1 | 视图机制 | 第1-3周 | 视图类型、匹配器、栈 | `geo_views.h` (约400行) | P0 |
| 2 | 类型类系统 | 第4-6周 | Class 定义、实例注册、调度 | `geo_typeclass.h` (约350行) | P1 |
| 3 | SSReflect 符号 | 第7-8周 | 紧凑符号、重写引擎 | proof_ssr.h (约250行) | P1 |
| 4 | 几何库集成 | 第9-11周 | 预定义视图、常用几何类型类 | geometry_views.mathlib | P2 |
| 5 | 继承系统 | 第12-14周 | 公理包继承、多重继承 | axiom_inherit.h | P2 |

### 4.2 依赖关系

```
阶段1 (geo_views.h)
    │
    ├── 依赖：无
    │
    ▼
阶段2 (geo_typeclass.h)
    │
    ├── 依赖：无
    │
    ▼
阶段3 (proof_ssr.h)
    │
    ├── 依赖：阶段1
    │
    ▼
阶段4 (geometry_views.mathlib)
    │
    ├── 依赖：阶段1 + 阶段2 + 阶段3
    │
    ▼
阶段5 (axiom_inherit.h)
    │
    ├── 依赖：阶段2
    │
    ▼
完成
```

---

## 5. 附录

### 5.1 参考资源

- MathComp 官网：https://math-comp.github.io/
- MathComp GitHub：https://github.com/math-comp/math-comp
- SSReflect 文档：https://coq.inria.fr/refman/proof-engine/ssr.html
- Mathematical Components Book：https://math-comp.github.io/mcb/

### 5.2 术语表

| 术语 | 英文 | 定义 |
|:---|:---|:---|
| SSReflect | Small Scale Reflection | Coq 的证明语言扩展 |
| 视图 | View | 逻辑事实的抽象表示 |
| 视图配对 | View Matching | 将目标与视图匹配的过程 |
| 类型类 | Type Class | 参数化多态的一种形式 |
| 混合格式 | Mixin Pattern | 通过继承混入实现 |
| Structure | Structure | MathComp 中的打包类型 |

### 5.3 许可证兼容性

MathComp 使用 CeCILL-B 许可证（与 GPL 兼容），Lv-00 可自由参考设计理念。

---

*文档生成日期：2026-05-28*
*参考版本：MathComp 2.2+
