# Enzyme 自动微分框架参考文档

## 1. 项目概述

### 1.1 项目简介

Enzyme 是一个由 MIT 和劳伦斯利弗莫尔国家实验室（LLNL）联合开发的自动微分（Automatic Differentiation, AD）框架，由 William Moses 博士主导开发。该项目的核心创新在于直接在 LLVM 中间表示（IR）层面实现自动微分，而非传统的源代码或抽象语法树（AST）层面。

传统的自动微分工具通常需要在编译过程的早期阶段介入，处理源代码或 AST。这种方式虽然直观，但会错过编译器后续优化阶段产生的优化机会。Enzyme 的独特之处在于它在 LLVM IR 层面工作，此时源代码已经被转换为一种与语言无关的中间表示，并且已经经过了编译器的前端优化。这意味着 Enzyme 可以对经过优化的代码进行微分，生成更高效的梯度计算代码。

Enzyme 支持多种编程语言，包括 C、C++、Julia、Rust 和 Fortran，这得益于其基于 LLVM IR 的设计。任何能够编译到 LLVM IR 的语言都可以利用 Enzyme 进行自动微分。

### 1.2 技术栈

| 组件 | 技术 |
|------|------|
| 核心实现 | C++（LLVM 插件） |
| 目标平台 | LLVM IR（支持 LLVM 7+） |
| 支持语言 | C、C++、Julia、Rust、Fortran 等 |
| 微分模式 | 前向模式（Forward Mode）、反向模式（Reverse Mode） |
| 优化集成 | LLVM Pass 系统 |
| 构建系统 | CMake |

### 1.3 社区活跃度

Enzyme 项目拥有活跃的开发和用户社区：

- **GitHub 仓库**：https://github.com/EnzymeAD/Enzyme
- **官方文档**：https://enzyme.mit.edu/
- **学术论文**：项目基于多篇顶级会议论文（PLDI、NeurIPS 等）
- **工业应用**：已被多个科学计算和机器学习项目采用
- **持续集成**：活跃的 CI/CD 流程，支持多平台测试

### 1.4 许可证

Enzyme 采用 Apache 2.0 许可证发布，这是一种宽松的开源许可证，允许：

- 商业使用
- 修改和分发
- 专利授权
- 私人使用

使用者需要保留版权声明和许可证文本。

---

## 2. 核心借鉴点

### 2.1 LLVM 级自动微分的优势

Enzyme 的核心设计理念是在 LLVM IR 层面进行自动微分，这带来了以下优势：

1. **语言无关性**：任何编译到 LLVM IR 的语言都可以使用 Enzyme
2. **优化后微分**：对已经过优化的代码进行微分，避免重复计算
3. **精确控制**：可以精确控制内存访问模式和并行化策略
4. **与编译器优化协同**：生成的梯度代码可以进一步被 LLVM 优化

### 2.2 技术架构

Enzyme 的处理流程分为以下几个阶段：

```
源代码 → LLVM IR（前端优化后）
              ↓
    ┌─────────────────┐
    │   预处理阶段     │  ── 准备 IR，标记需要微分的函数
    └────────┬────────┘
             ↓
    ┌─────────────────┐
    │   类型分析阶段   │  ── 分析数据类型和内存布局
    └────────┬────────┘
             ↓
    ┌─────────────────┐
    │   活性分析阶段   │  ── 确定哪些变量影响输出
    └────────┬────────┘
             ↓
    ┌─────────────────┐
    │ 导数函数生成阶段 │  ── 生成前向或反向模式导数代码
    └────────┬────────┘
             ↓
    ┌─────────────────┐
    │ AD 专用优化阶段  │  ── 应用自动微分特定的优化
    └────────┬────────┘
             ↓
    生成的梯度函数（LLVM IR）
```

### 2.3 关键特性对照表

| Enzyme 特性 | Lv-00 公式引擎微分需求 | 借鉴价值 |
|-------------|------------------------|----------|
| LLVM IR 级微分 | 表达式树级微分 | 学习 IR 级分析和转换技术，应用于表达式树优化 |
| 活性分析（Activity Analysis） | 变量依赖分析 | 识别公式中哪些子表达式需要计算梯度 |
| 类型分析（Type Analysis） | 表达式类型推断 | 确保微分结果类型正确，支持多态表达式 |
| 缓存策略优化 | 中间结果缓存 | 优化梯度计算中的重复子表达式求值 |
| 前向/反向模式选择 | 基于计算图的微分模式选择 | 根据公式复杂度自动选择最优微分策略 |
| 内存访问优化 | 表达式求值顺序优化 | 减少梯度计算过程中的内存分配 |
| 与 LLVM Pass 集成 | 与 Lv-00 优化管线集成 | 设计可扩展的优化 Pass 架构 |

### 2.4 核心算法借鉴

#### 2.4.1 活性分析算法

Enzyme 的活性分析用于确定函数的哪些输入会影响哪些输出。对于 Lv-00 的公式引擎，这对应于确定公式中的哪些变量会影响目标表达式的值。

核心思想：
- 从输出变量开始逆向分析
- 标记所有可能影响输出的变量为"活性"
- 剪枝非活性变量的梯度计算

#### 2.4.2 缓存策略

反向模式自动微分需要保存前向传播中的中间结果。Enzyme 采用多种缓存策略：
- 值缓存：直接保存计算结果
- 指针缓存：保存内存地址
- 重计算：在内存受限时选择重新计算

Lv-00 公式引擎可以借鉴这些策略，在表达式求值过程中优化中间结果的存储和复用。

---

## 3. Lv-00 映射方案

### 3.1 架构映射

将 Enzyme 的设计理念映射到 Lv-00 的 7 层架构中：

| Enzyme 组件 | Lv-00 层级 | 映射说明 |
|-------------|-----------|----------|
| LLVM IR | 第 3 层（公式引擎） | 公式表达式树作为"IR"表示 |
| 类型分析 | 第 1 层（基础类）+ 第 3 层 | 扩展符号坐标和多项式的类型系统 |
| 活性分析 | 第 3 层（公式引擎） | 表达式依赖图分析 |
| 导数生成 | 第 3 层（公式引擎） | 表达式树转换和微分规则应用 |
| 优化 Pass | 第 3 层（算法引擎） | 表达式简化、常量折叠等优化 |

### 3.2 C 代码示例

以下示例展示如何在 Lv-00 第 3 层公式引擎中集成自动微分功能。

#### 3.2.1 表达式节点定义

```c
/* lv00_expr.h - Lv-00 公式引擎表达式定义 */
#ifndef LV00_EXPR_H
#define LV00_EXPR_H

#include "lv00_types.h"

typedef enum {
    EXPR_CONST,      /* 常量 */
    EXPR_VAR,        /* 变量 */
    EXPR_ADD,        /* 加法 */
    EXPR_SUB,        /* 减法 */
    EXPR_MUL,        /* 乘法 */
    EXPR_DIV,        /* 除法 */
    EXPR_POW,        /* 幂运算 */
    EXPR_SIN,        /* 正弦 */
    EXPR_COS,        /* 余弦 */
    EXPR_EXP,        /* 指数 */
    EXPR_LOG,        /* 对数 */
    EXPR_CALL        /* 函数调用 */
} ExprType;

typedef struct ExprNode {
    ExprType type;
    union {
        double const_val;           /* EXPR_CONST */
        char var_name[32];          /* EXPR_VAR */
        struct {
            struct ExprNode *left;
            struct ExprNode *right;
        } binary;                   /* 二元运算 */
        struct {
            struct ExprNode *arg;
        } unary;                    /* 一元运算 */
        struct {
            char func_name[32];
            struct ExprNode **args;
            int arg_count;
        } call;                     /* 函数调用 */
    } data;
    
    /* 微分相关元数据 */
    int is_active;                  /* 活性标记 */
    struct ExprNode *grad_cache;    /* 梯度缓存 */
} ExprNode;

/* 表达式操作 */
ExprNode* expr_create_const(double val);
ExprNode* expr_create_var(const char *name);
ExprNode* expr_create_binary(ExprType type, ExprNode *left, ExprNode *right);
ExprNode* expr_create_unary(ExprType type, ExprNode *arg);
void expr_free(ExprNode *expr);
ExprNode* expr_clone(const ExprNode *expr);

#endif /* LV00_EXPR_H */
```

#### 3.2.2 自动微分核心实现

```c
/* lv00_autodiff.c - Lv-00 自动微分实现 */
#include "lv00_autodiff.h"
#include "lv00_expr.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 活性分析：标记影响输出的变量 */
static void activity_analysis(ExprNode *expr, const char **active_vars, int var_count) {
    if (expr == NULL) return;
    
    expr->is_active = 0;
    
    switch (expr->type) {
        case EXPR_VAR:
            /* 检查变量是否在活性列表中 */
            for (int i = 0; i < var_count; i++) {
                if (strcmp(expr->data.var_name, active_vars[i]) == 0) {
                    expr->is_active = 1;
                    break;
                }
            }
            break;
            
        case EXPR_CONST:
            /* 常量为非活性 */
            expr->is_active = 0;
            break;
            
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_POW:
            /* 二元运算：任一操作数活性则结果活性 */
            activity_analysis(expr->data.binary.left, active_vars, var_count);
            activity_analysis(expr->data.binary.right, active_vars, var_count);
            expr->is_active = expr->data.binary.left->is_active || 
                             expr->data.binary.right->is_active;
            break;
            
        case EXPR_SIN:
        case EXPR_COS:
        case EXPR_EXP:
        case EXPR_LOG:
            /* 一元运算：参数活性则结果活性 */
            activity_analysis(expr->data.unary.arg, active_vars, var_count);
            expr->is_active = expr->data.unary.arg->is_active;
            break;
            
        default:
            break;
    }
}

/* 前向模式自动微分 */
ExprNode* forward_diff(ExprNode *expr, const char *var) {
    if (expr == NULL) return NULL;
    
    /* 非活性表达式导数为 0 */
    if (!expr->is_active) {
        return expr_create_const(0.0);
    }
    
    ExprNode *result = NULL;
    
    switch (expr->type) {
        case EXPR_CONST:
            /* d/dx(c) = 0 */
            result = expr_create_const(0.0);
            break;
            
        case EXPR_VAR:
            /* d/dx(x) = 1, d/dx(y) = 0 (y != x) */
            if (strcmp(expr->data.var_name, var) == 0) {
                result = expr_create_const(1.0);
            } else {
                result = expr_create_const(0.0);
            }
            break;
            
        case EXPR_ADD:
            /* d/dx(a + b) = da/dx + db/dx */
            result = expr_create_binary(EXPR_ADD,
                forward_diff(expr->data.binary.left, var),
                forward_diff(expr->data.binary.right, var));
            break;
            
        case EXPR_SUB:
            /* d/dx(a - b) = da/dx - db/dx */
            result = expr_create_binary(EXPR_SUB,
                forward_diff(expr->data.binary.left, var),
                forward_diff(expr->data.binary.right, var));
            break;
            
        case EXPR_MUL:
            /* d/dx(a * b) = da/dx * b + a * db/dx (乘积法则) */
            {
                ExprNode *da = forward_diff(expr->data.binary.left, var);
                ExprNode *db = forward_diff(expr->data.binary.right, var);
                ExprNode *term1 = expr_create_binary(EXPR_MUL, da, expr_clone(expr->data.binary.right));
                ExprNode *term2 = expr_create_binary(EXPR_MUL, expr_clone(expr->data.binary.left), db);
                result = expr_create_binary(EXPR_ADD, term1, term2);
            }
            break;
            
        case EXPR_DIV:
            /* d/dx(a / b) = (da/dx * b - a * db/dx) / b^2 (商法则) */
            {
                ExprNode *da = forward_diff(expr->data.binary.left, var);
                ExprNode *db = forward_diff(expr->data.binary.right, var);
                ExprNode *term1 = expr_create_binary(EXPR_MUL, da, expr_clone(expr->data.binary.right));
                ExprNode *term2 = expr_create_binary(EXPR_MUL, expr_clone(expr->data.binary.left), db);
                ExprNode *numerator = expr_create_binary(EXPR_SUB, term1, term2);
                ExprNode *denominator = expr_create_binary(EXPR_POW, 
                    expr_clone(expr->data.binary.right), 
                    expr_create_const(2.0));
                result = expr_create_binary(EXPR_DIV, numerator, denominator);
            }
            break;
            
        case EXPR_SIN:
            /* d/dx(sin(u)) = cos(u) * du/dx */
            {
                ExprNode *du = forward_diff(expr->data.unary.arg, var);
                ExprNode *cos_u = expr_create_unary(EXPR_COS, expr_clone(expr->data.unary.arg));
                result = expr_create_binary(EXPR_MUL, cos_u, du);
            }
            break;
            
        case EXPR_COS:
            /* d/dx(cos(u)) = -sin(u) * du/dx */
            {
                ExprNode *du = forward_diff(expr->data.unary.arg, var);
                ExprNode *sin_u = expr_create_unary(EXPR_SIN, expr_clone(expr->data.unary.arg));
                ExprNode *neg_sin_u = expr_create_binary(EXPR_MUL, 
                    expr_create_const(-1.0), sin_u);
                result = expr_create_binary(EXPR_MUL, neg_sin_u, du);
            }
            break;
            
        case EXPR_EXP:
            /* d/dx(exp(u)) = exp(u) * du/dx */
            {
                ExprNode *du = forward_diff(expr->data.unary.arg, var);
                ExprNode *exp_u = expr_create_unary(EXPR_EXP, expr_clone(expr->data.unary.arg));
                result = expr_create_binary(EXPR_MUL, exp_u, du);
            }
            break;
            
        case EXPR_LOG:
            /* d/dx(log(u)) = du/dx / u */
            {
                ExprNode *du = forward_diff(expr->data.unary.arg, var);
                result = expr_create_binary(EXPR_DIV, du, expr_clone(expr->data.unary.arg));
            }
            break;
            
        default:
            result = expr_create_const(0.0);
            break;
    }
    
    return result;
}

/* 反向模式自动微分（基于表达式树遍历） */
typedef struct GradMap {
    char var_name[32];
    ExprNode *grad_expr;
    struct GradMap *next;
} GradMap;

static void reverse_diff_accumulate(ExprNode *expr, ExprNode *out_grad, GradMap **grad_map);

void reverse_diff(ExprNode *expr, const char **vars, int var_count, GradMap **out_grad_map) {
    /* 初始化梯度映射 */
    *out_grad_map = NULL;
    
    /* 执行活性分析 */
    activity_analysis(expr, vars, var_count);
    
    /* 从输出梯度 1 开始反向传播 */
    ExprNode *seed_grad = expr_create_const(1.0);
    reverse_diff_accumulate(expr, seed_grad, out_grad_map);
}

static void reverse_diff_accumulate(ExprNode *expr, ExprNode *out_grad, GradMap **grad_map) {
    if (expr == NULL || !expr->is_active) return;
    
    switch (expr->type) {
        case EXPR_VAR:
            /* 累加变量梯度 */
            {
                GradMap *entry = *grad_map;
                while (entry != NULL) {
                    if (strcmp(entry->var_name, expr->data.var_name) == 0) {
                        entry->grad_expr = expr_create_binary(EXPR_ADD, 
                            entry->grad_expr, out_grad);
                        return;
                    }
                    entry = entry->next;
                }
                /* 新变量 */
                GradMap *new_entry = malloc(sizeof(GradMap));
                strcpy(new_entry->var_name, expr->data.var_name);
                new_entry->grad_expr = out_grad;
                new_entry->next = *grad_map;
                *grad_map = new_entry;
            }
            break;
            
        case EXPR_ADD:
            /* dL/da = dL/dout, dL/db = dL/dout */
            reverse_diff_accumulate(expr->data.binary.left, expr_clone(out_grad), grad_map);
            reverse_diff_accumulate(expr->data.binary.right, out_grad, grad_map);
            break;
            
        case EXPR_MUL:
            /* dL/da = dL/dout * b, dL/db = dL/dout * a */
            {
                ExprNode *grad_left = expr_create_binary(EXPR_MUL, 
                    expr_clone(out_grad), expr_clone(expr->data.binary.right));
                ExprNode *grad_right = expr_create_binary(EXPR_MUL, 
                    out_grad, expr_clone(expr->data.binary.left));
                reverse_diff_accumulate(expr->data.binary.left, grad_left, grad_map);
                reverse_diff_accumulate(expr->data.binary.right, grad_right, grad_map);
            }
            break;
            
        /* 其他运算规则... */
        
        default:
            expr_free(out_grad);
            break;
    }
}
```

#### 3.2.3 与 Lv-00 第 3 层集成

```c
/* lv00_formula_engine.c - 公式引擎集成 */
#include "lv00_formula_engine.h"
#include "lv00_autodiff.h"

/* 公式引擎上下文 */
typedef struct FormulaContext {
    ExprNode *expression;
    char **variables;
    int var_count;
    int use_forward_mode;  /* 1=前向模式, 0=反向模式 */
} FormulaContext;

/* 计算梯度 */
int formula_compute_gradient(FormulaContext *ctx, 
                              const double *input_vals,
                              double *output_grads) {
    if (ctx == NULL || input_vals == NULL || output_grads == NULL) {
        return -1;
    }
    
    /* 根据表达式复杂度选择微分模式 */
    int input_count = ctx->var_count;
    int output_count = 1;  /* 标量输出 */
    
    if (ctx->use_forward_mode) {
        /* 前向模式：输入少时高效 */
        for (int i = 0; i < input_count; i++) {
            ExprNode *grad_expr = forward_diff(ctx->expression, ctx->variables[i]);
            
            /* 简化梯度表达式 */
            grad_expr = expr_simplify(grad_expr);
            
            /* 求值 */
            output_grads[i] = expr_evaluate(grad_expr, ctx->variables, input_vals, input_count);
            
            expr_free(grad_expr);
        }
    } else {
        /* 反向模式：输出少时高效 */
        GradMap *grad_map = NULL;
        reverse_diff(ctx->expression, (const char **)ctx->variables, input_count, &grad_map);
        
        /* 提取梯度值 */
        GradMap *entry = grad_map;
        while (entry != NULL) {
            for (int i = 0; i < input_count; i++) {
                if (strcmp(entry->var_name, ctx->variables[i]) == 0) {
                    ExprNode *simplified = expr_simplify(entry->grad_expr);
                    output_grads[i] = expr_evaluate(simplified, 
                        ctx->variables, input_vals, input_count);
                    expr_free(simplified);
                    break;
                }
            }
            entry = entry->next;
        }
        
        /* 清理梯度映射 */
        free_grad_map(grad_map);
    }
    
    return 0;
}

/* 自动选择微分模式 */
void formula_select_mode(FormulaContext *ctx) {
    /* 经验法则：输入维度 < 输出维度时用前向模式，否则用反向模式 */
    if (ctx->var_count <= 5) {  /* 阈值可配置 */
        ctx->use_forward_mode = 1;
    } else {
        ctx->use_forward_mode = 0;
    }
}
```

### 3.3 优化策略映射

| Enzyme 优化 | Lv-00 实现 |
|-------------|-----------|
| 常量传播 | 表达式常量折叠 |
| 死代码消除 | 非活性子表达式剪枝 |
| 公共子表达式消除 | 表达式缓存和复用 |
| 内存访问优化 | 求值顺序优化，减少临时变量 |
| 向量化 | 批量梯度计算 |

---

## 4. 实现路线图

### 4.1 短期目标（1-2 个月）

| 任务 | 描述 | 优先级 |
|------|------|--------|
| 基础表达式系统 | 实现表达式节点定义和基本操作 | 高 |
| 前向模式 AD | 实现基础前向模式自动微分 | 高 |
| 基本微分规则 | 实现 +、-、*、/、sin、cos、exp、log 的导数 | 高 |
| 表达式求值 | 实现表达式树的数值求值 | 高 |
| 简单测试用例 | 验证多项式、三角函数的梯度计算 | 中 |

### 4.2 中期目标（3-6 个月）

| 任务 | 描述 | 优先级 |
|------|------|--------|
| 反向模式 AD | 实现反向模式自动微分 | 高 |
| 活性分析 | 实现表达式依赖分析和活性标记 | 高 |
| 表达式简化 | 实现常量折叠、代数简化 | 高 |
| 模式选择启发式 | 根据输入/输出维度自动选择微分模式 | 中 |
| 与约束求解器集成 | 在约束求解中使用梯度信息 | 中 |
| 性能基准测试 | 建立梯度计算性能基准 | 中 |

### 4.3 长期目标（6-12 个月）

| 任务 | 描述 | 优先级 |
|------|------|--------|
| 高级优化 | 实现公共子表达式消除、求值顺序优化 | 中 |
| 高阶导数 | 支持二阶及更高阶导数计算 | 中 |
| 稀疏梯度 | 优化稀疏结构的梯度计算 | 低 |
| JIT 编译 | 探索表达式到机器码的即时编译 | 低 |
| 并行梯度计算 | 利用多线程加速批量梯度计算 | 低 |
| 与证明引擎集成 | 在几何证明中使用导数进行推理 | 低 |

---

## 5. 附录

### 5.1 Enzyme 关键 API 列表

Enzyme 提供以下核心 API 供用户调用：

| API 名称 | 功能描述 | 使用场景 |
|----------|----------|----------|
| `__enzyme_autodiff` | 通用自动微分接口，自动选择模式 | 大多数情况下的默认选择 |
| `__enzyme_fwddiff` | 前向模式自动微分 | 输入维度 <= 输出维度时 |
| `__enzyme_reverse` | 反向模式自动微分 | 输出维度 < 输入维度时 |
| `__enzyme_register_gradient` | 注册自定义函数的梯度 | 用户定义函数的微分 |
| `__enzyme_virtual` | 虚函数支持 | C++ 虚函数微分 |
| `__enzyme_allocation` | 内存分配追踪 | 涉及动态内存的函数 |

#### 使用示例

```c
#include <enzyme/enzyme.h>

/* 需要微分的函数 */
double f(double x, double y) {
    return x * x + y * y;
}

/* 使用 Enzyme 计算梯度 */
void compute_gradient(double x, double y, double *dx, double *dy) {
    /* 反向模式：计算 df/dx 和 df/dy */
    *dx = __enzyme_autodiff((void*)f, x, y, 1.0, 0.0);
    *dy = __enzyme_autodiff((void*)f, x, y, 0.0, 1.0);
}
```

### 5.2 Lv-00 自动微分 API 设计

基于 Enzyme 的设计理念，为 Lv-00 设计的 API：

```c
/* lv00_autodiff.h - Lv-00 自动微分公共接口 */
#ifndef LV00_AUTODIFF_H
#define LV00_AUTODIFF_H

#include "lv00_expr.h"

/* 梯度计算模式 */
typedef enum {
    AD_MODE_FORWARD,    /* 前向模式 */
    AD_MODE_REVERSE,    /* 反向模式 */
    AD_MODE_AUTO        /* 自动选择 */
} ADMode;

/* 梯度计算选项 */
typedef struct ADOptions {
    ADMode mode;
    int simplify;           /* 是否简化结果表达式 */
    int cache_intermediates; /* 是否缓存中间结果 */
} ADOptions;

/* 计算单变量导数 */
ExprNode* ad_diff(ExprNode *expr, const char *var, const ADOptions *opts);

/* 计算多变量梯度 */
int ad_gradient(ExprNode *expr, 
                const char **vars, 
                int var_count, 
                ExprNode **grad_exprs,
                const ADOptions *opts);

/* 数值梯度计算 */
int ad_eval_gradient(ExprNode *expr,
                     const char **vars,
                     int var_count,
                     const double *input_vals,
                     double *output_grads,
                     const ADOptions *opts);

/* 表达式简化 */
ExprNode* expr_simplify(ExprNode *expr);

/* 表达式求值 */
double expr_evaluate(ExprNode *expr, 
                     const char **vars, 
                     const double *vals, 
                     int var_count);

#endif /* LV00_AUTODIFF_H */
```

### 5.3 参考文献

1. Moses, W., & Churavy, V. (2020). Instead of Rewriting Foreign Code for Machine Learning, Automatically Synthesize Fast Gradients. *Advances in Neural Information Processing Systems (NeurIPS)*, 33.

2. Moses, W., et al. (2021). Reverse-Mode Automatic Differentiation and Optimization of GPU Kernels via Enzyme. *Proceedings of the International Conference on Supercomputing (ICS)*.

3. Moses, W., et al. (2022). Scalable Automatic Differentiation of Multiple Parallel Paradigms through Compiler Augmentation. *ACM Transactions on Parallel Computing*.

4. Griewank, A., & Walther, A. (2008). *Evaluating Derivatives: Principles and Techniques of Algorithmic Differentiation* (2nd ed.). SIAM.

5. Baydin, A. G., et al. (2018). Automatic Differentiation in Machine Learning: A Survey. *Journal of Machine Learning Research*, 18(153), 1-43.

6. Enzyme 官方文档：https://enzyme.mit.edu/

7. Enzyme GitHub 仓库：https://github.com/EnzymeAD/Enzyme

8. LLVM 项目文档：https://llvm.org/docs/

---

## 文档信息

- 创建日期：2026-05-25
- 版本：1.0
- 维护者：Lv-00 开发团队
- 关联模块：第 3 层公式引擎
