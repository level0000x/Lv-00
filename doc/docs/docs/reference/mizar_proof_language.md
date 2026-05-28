# Mizar - 声明式数学证明语言参考文档

> **项目名称**：Mizar
> **项目链接**：https://github.com/JUrban/Mizar
> **项目链接**：https://mmlquery.uwb.edu.pl/
> **项目类型**：形式化数学证明语言与证明库
> **语言/技术栈**：Pascal/Object Pascal、Mizar 语言
> **最后更新**：2024年持续活跃
> **文档版本**：v1.0
> **适用层级**：第4层（多策略自动推理层）

---

## 1. 项目概述

### 1.1 项目背景与定位

Mizar 是由波兰数学家 Andrzej Trybulec 于 1973 年开始开发的形式化数学系统，是**历史最悠久的交互式定理证明器之一**。Mizar 的核心理念是**声明式证明风格**——用户描述"数学应该是什么样"，系统验证证明的正确性。

Mizar 最有名的成就是 **Mizar Mathematical Library (MML)**，包含超过 52,000 条形式化定理，涵盖数学的各个分支。

### 1.2 声明式证明风格

Mizar 的证明语言以其**声明式**特性著称：

```mizar
theorem Th1: for n being Nat holds
  (n + 1)^2 = n^2 + 2*n + 1
proof
  let n be Nat;
  thus (n + 1)^2 = (n + 1)*(n + 1) by SQUARE_1:def 1
    .= n*n + 2*n + 1 by NEWTON:4;
end;
```

关键特性：
- `for ... holds ...` 声明全称命题
- `let` 引入变量
- `thus` 和 `hence` 表达推理结论
- `by` 引用已证明的定理

### 1.3 MML 档案库

Mizar Mathematical Library (MML) 的规模：
- 期刊形式发布（Formalized Mathematics, 1986-）
- 52,000+ 形式化定理
- 12,000+ 定义
- 涵盖：代数学、拓扑学、几何学、数论、概率论等

---

## 2. 核心借鉴点

### 2.1 声明式证明模式

Mizar 的声明式证明是其核心价值：

```mizar
scheme LambdaFunc{A() -> set}:
  ex f being Function st
    for x being set holds f.x = A()
provided
  A() is Function-yielding

proof
  reconsider f = (A()) as Function by FUNCT_1:def 6;
  take f;
  thus thesis by FUNCT_1:18;
end;
```

**Lv-00 借鉴价值**：Lv-00 的证明 DSL 可借鉴 Mizar 的**模式匹配证明方案**（scheme），定义几何证明的通用模式。

### 2.2 自然演绎风格

Mizar 使用**自然演绎**风格：

```mizar
theorem ExistsExample:
  ex x being Real st x^2 = 4
proof
  take 2;
  thus thesis;
end;

theorem ImplicationExample:
  (for x holds P(x)) implies Q
proof
  assume for x holds P(x);
  thus Q;
end;
```

**Lv-00 借鉴价值**：几何证明中的存在性证明和蕴含证明可以借鉴此模式。

### 2.3 核心借鉴点对照表

| Mizar 特性 | Lv-00 现有能力 | Lv-00 借鉴方案 |
|:---|:---|:---|
| 声明式证明 | `proof_parser.y` 过程式 | 新增声明式证明语法 |
| Scheme（模式证明） | 无 | 新增几何 Scheme 机制 |
| 自然演绎 | `natural_deduction.h` | 增强自然演绎系统 |
| MML 档案库 | `axiom_packages/` | 扩展几何定理库 |
| 证明引用（by） | `proof.h` 基础 | 增强定理引用系统 |

---

## 3. Lv-00 映射方案

### 3.1 Mizar 风格证明 DSL

```c
// proof_mizar.h - Mizar 风格证明系统

#ifndef LV00_PROOF_MIZAR_H
#define LV00_PROOF_MIZAR_H

#include <lv00.h>

// ============ Mizar 风格语法结构 ============

typedef enum {
    LV00_MIZ_THEOREM,     // theorem
    LV00_MIZ_LEMMA,        // lemma
    LV00_MIZ_PROP,         // proposition
    LV00_MIZ_COROLLARY,    // corollary
    LV00_MIZ_SCHEME        // scheme（模式证明）
} Lv00MizarKind;

// Scheme 参数
typedef struct {
    const char* var_name;
    Lv00GeoType var_type;
} Lv00SchemeParam;

// Scheme 约束
typedef struct {
    const char* param_name;
    const char* pred;  // 谓词表达式
} Lv00SchemeConstraint;

// Mizar 证明项
typedef struct Lv00MizarProof Lv00MizarProof;
struct Lv00MizarProof {
    // 证明结构
    struct {
        Lv00Prop** assumptions;
        size_t assumption_count;
        
        Lv00Prop* conclusion;
        
        // 推理步骤
        struct {
            Lv00Prop* prop;           // 结论
            const char* reason;       // 推理原因（by 引用）
            Lv00Prop** by_refs;      // 引用的定理
            size_t ref_count;
        }* steps;
        size_t step_count;
        
        // thus/hence 区别
        int is_hence;  // hence 隐含算术
    } proof_body;
    
    // Scheme 特有字段
    struct {
        Lv00SchemeParam* params;
        size_t param_count;
        Lv00SchemeConstraint* constraints;
        size_t constraint_count;
    } scheme_spec;
};

// ============ 证明声明 ============

typedef struct Lv00MizarDecl Lv00MizarDecl;
struct Lv00MizarDecl {
    Lv00MizarKind kind;
    const char* name;
    
    // 定理体
    Lv00Prop* statement;
    Lv00MizarProof* proof;
    
    // Scheme 参数（如果是 scheme）
    Lv00SchemeParam* scheme_params;
    size_t scheme_param_count;
};

// ============ Mizar 证明执行器 ============

typedef struct Lv00MizarExecutor Lv00MizarExecutor;
struct Lv00MizarExecutor {
    // 定理库
    Lv00MizarDecl** theorems;
    size_t theorem_count;
    
    // 当前证明上下文
    Lv00ProofContext* ctx;
    
    // 引用解析器
    void* ref_resolver;
};

// ============ API 声明 ============

// 证明声明构造
Lv00MizarDecl* lv00_mizar_theorem_create(const char* name, 
                                          Lv00Prop* statement,
                                          Lv00MizarProof* proof);
Lv00MizarDecl* lv00_mizar_scheme_create(const char* name,
                                         Lv00SchemeParam* params,
                                         size_t param_count,
                                         Lv00SchemeConstraint* constraints,
                                         size_t constraint_count,
                                         Lv00Prop* conclusion,
                                         Lv00MizarProof* proof);

// 证明构造
Lv00MizarProof* lv00_mizar_proof_create(void);
void lv00_mizar_assume(Lv00MizarProof* proof, Lv00Prop* p);
void lv00_mizar_thus(Lv00MizarProof* proof, Lv00Prop* p, const char* by);
void lv00_mizar_hence(Lv00MizarProof* proof, Lv00Prop* p, const char* by);
void lv00_mizar_take(Lv00MizarProof* proof, Lv00Term* witness);
void lv00_mizar_let(Lv00MizarProof* proof, const char* var, Lv00GeoType type);
void lv00_mizar_now(Lv00MizarProof* proof);
void lv00_mizar_qed(Lv00MizarProof* proof);

// Scheme 特有的构造
void lv00_mizar_scheme_param(Lv00MizarDecl* decl, const char* name, Lv00GeoType type);
void lv00_mizar_provided(Lv00MizarProof* proof, Lv00Prop* condition);

// 证明执行
Lv00MizarExecutor* lv00_mizar_executor_create(void);
void lv00_mizar_register(Lv00MizarExecutor* exec, Lv00MizarDecl* decl);
int lv00_mizar_verify(Lv00MizarExecutor* exec, Lv00MizarDecl* decl);

// 解析 Mizar 源码
Lv00MizarDecl* lv00_mizar_parse(const char* source);

// 清理
void lv00_mizar_decl_destroy(Lv00MizarDecl* decl);
void lv00_mizar_proof_destroy(Lv00MizarProof* proof);
void lv00_mizar_executor_destroy(Lv00MizarExecutor* exec);

#endif // LV00_PROOF_MIZAR_H
```

### 3.2 使用示例

```c
// 示例：Mizar 风格的几何定理

#include <lv00.h>
#include <lv00/proof_mizar.h>

// Mizar 风格定理：三角形内角和为 180 度
Lv00MizarDecl* mizar_triangle_angle_sum(void) {
    // Scheme 声明
    Lv00SchemeParam params[] = {
        { "A", lv00_gtype_point() },
        { "B", lv00_gtype_point() },
        { "C", lv00_gtype_point() }
    };
    
    // Scheme 约束
    Lv00SchemeConstraint constraints[] = {
        { "A", "A ≠ B ∧ A ≠ C" },
        { "B", "B ≠ A ∧ B ≠ C" },
        { "C", "C ≠ A ∧ C ≠ B" }
    };
    
    // 结论
    Lv00Prop* conclusion = lv00_prop_equation(
        lv00_term_func("angle_sum", "A", "B", "C"),
        lv00_term_real(180.0f)
    );
    
    // 证明
    Lv00MizarProof* proof = lv00_mizar_proof_create();
    
    // let A, B, C be Point;
    lv00_mizar_let(proof, "A", lv00_gtype_point());
    lv00_mizar_let(proof, "B", lv00_gtype_point());
    lv00_malar_let(proof, "C", lv00_gtype_point());
    
    // assume A ≠ B ∧ A ≠ C ∧ B ≠ C;
    Lv00Prop* nondegen = lv00_prop_and(
        lv00_prop_neq(lv00_term_var("A"), lv00_term_var("B")),
        lv00_prop_and(
            lv00_prop_neq(lv00_term_var("A"), lv00_term_var("C")),
            lv00_prop_neq(lv00_term_var("B"), lv00_term_var("C"))
        )
    );
    lv00_mizar_assume(proof, nondegen);
    
    // consider D such that B-D-C by ...
    Lv00Term* D = lv00_term_var("D");
    lv00_mizar_take(proof, D);
    // by some_theorem  // 引用定理
    
    // thus angle(A,B,C) = angle(A,B,D) + angle(D,B,C)
    Lv00Prop* angle_split = lv00_prop_equation(
        lv00_term_func("angle", "A", "B", "C"),
        lv00_term_add(
            lv00_term_func("angle", "A", "B", "D"),
            lv00_term_func("angle", "D", "B", "C")
        )
    );
    lv00_mizar_thus(proof, angle_split, "angle_def");
    
    // hence thesis by elementary_geometry_1;
    Lv00Prop* thesis = conclusion;
    lv00_mizar_hence(proof, thesis, "elementary_geometry_1");
    
    // qed
    lv00_mizar_qed(proof);
    
    // 创建 Scheme
    return lv00_mizar_scheme_create(
        "TriangleAngleSum",
        params, 3,
        constraints, 3,
        conclusion,
        proof
    );
}

// 示例：Scheme 用于几何证明模式

Lv00MizarDecl* mizar_sas_scheme(void) {
    // Scheme 参数
    Lv00SchemeParam params[] = {
        { "A", lv00_gtype_point() },
        { "B", lv00_gtype_point() },
        { "C", lv00_gtype_point() },
        { "D", lv00_gtype_point() },
        { "E", lv00_gtype_point() },
        { "F", lv00_gtype_point() }
    };
    
    // 约束条件
    Lv00SchemeConstraint constraints[] = {
        { "A", "Triangle(A,B,C)" },
        { "D", "Triangle(D,E,F)" },
        { "条件", "AB = DE ∧ BC = EF ∧ angle(A,B,C) = angle(D,E,F)" }
    };
    
    // 结论
    Lv00Prop* conclusion = lv00_prop_congruent(
        lv00_term_triangle("A", "B", "C"),
        lv00_term_triangle("D", "E", "F")
    );
    
    // 证明（略）
    Lv00MizarProof* proof = lv00_mizar_proof_create();
    // ... 完整证明 ...
    lv00_mizar_qed(proof);
    
    return lv00_mizar_scheme_create(
        "SAS_CongCriterion",
        params, 6,
        constraints, 3,
        conclusion,
        proof
    );
}
```

---

## 4. 实现路线图

### 4.1 分阶段实现计划

| 阶段 | 名称 | 时间 | 核心任务 | 交付物 | 优先级 |
|:---:|:---|:---:|:---|:---|:---:|
| 1 | Mizar 解析器 | 第1-3周 | 语法解析、AST | `mizar_parser.y` (约400行) | P0 |
| 2 | Mizar 执行器 | 第4-6周 | 定理注册、引用解析 | `proof_mizar.h` (约350行) | P0 |
| 3 | Scheme 机制 | 第7-9周 | 模式证明、参数化定理 | scheme_engine.c | P1 |
| 4 | 几何定理库 | 第10-12周 | 几何 Mizar 库 | geometry_mml/ | P1 |
| 5 | 工具链集成 | 第13-14周 | IDE、证明助手 | mizar_mode/ | P2 |

### 4.2 依赖关系

```
阶段1 (mizar_parser.y)
    │
    ├── 依赖：无
    │
    ▼
阶段2 (proof_mizar.h)
    │
    ├── 依赖：阶段1 + proof_parser.y
    │
    ▼
阶段3 (scheme_engine.c)
    │
    ├── 依赖：阶段2
    │
    ▼
阶段4 (geometry_mml/)
    │
    ├── 依赖：阶段2 + 阶段3
    │
    ▼
阶段5 (mizar_mode/)
    │
    ├── 依赖：阶段1-4
    │
    ▼
完成
```

---

## 5. 附录

### 5.1 参考资源

- Mizar 官网：https://mizar.uwb.edu.pl/
- Mizar GitHub：https://github.com/JUrban/Mizar
- MML Query：https://mmlquery.uwb.edu.pl/
- Mizar 教程：https://mizar.uwb.edu.pl/wiki/static/doc/tutorial.pdf
- Formalized Mathematics 期刊：https://fm.mizarg.eu/

### 5.2 术语表

| 术语 | 英文 | 定义 |
|:---|:---|:---|
| Mizar | Mizar | 形式化数学证明语言和系统 |
| MML | Mizar Mathematical Library | Mizar 形式化数学库 |
| Scheme | Scheme | 参数化证明模式 |
| thus/hence | thus/hence | 证明中的推理表达 |
| by | by | 引用已证明的定理 |
| thesis | thesis | 当前待证明的目标 |

### 5.3 许可证兼容性

Mizar 使用 GPL 许可证，Lv-00 可自由参考设计理念。

---

*文档生成日期：2026-05-28*
*参考版本：Mizar 9.0+*
