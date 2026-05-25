# Boolector SMT 求解器参考文档

## 1. 项目概述

### 1.1 项目简介

Boolector 是由奥地利林茨约翰内斯开普勒大学（Johannes Kepler University Linz）开发的高性能 SMT（Satisfiability Modulo Theories）求解器，专为位向量（Bit-Vectors）和数组（Arrays）理论设计。该项目自 2007 年启动以来，在 SMT 求解领域取得了显著成就，多次在 SMT-COMP 国际竞赛中获得冠军，成为硬件验证、软件分析和密码学研究的重要工具。

Boolector 的设计目标是在处理位向量运算和数组操作时实现卓越的求解性能。它采用了一系列创新技术，包括 Lambert 变换优化位向量乘法、动态变量消除、以及增量求解支持，使其在处理复杂约束问题时表现出色。

### 1.2 技术栈

| 技术组件 | 描述 |
|---------|------|
| **核心语言** | C 语言（符合 C99 标准） |
| **构建系统** | CMake |
| **SAT 后端** | MiniSat、CaDiCaL、PicoSAT、Lingeling |
| **输入格式** | SMT-LIB v2、BTOR、BTOR2 |
| **API 接口** | C API、Python 绑定 |
| **依赖库** | GMP（可选，用于高精度运算） |

Boolector 的技术架构采用分层设计：

1. **词法/语法分析层**：解析 SMT-LIB v2、BTOR 和 BTOR2 格式的输入文件
2. **重写/简化层**：应用多种等价变换和简化规则，降低问题复杂度
3. **位向量编码层**：将高层次的位向量运算编码为布尔公式
4. **SAT 求解层**：调用底层 SAT 求解器完成最终求解

### 1.3 社区活跃度

Boolector 拥有活跃的开源社区和持续的维护更新：

- **GitHub 仓库**：https://github.com/Boolector/boolector
- **星标数量**：超过 500 星（截至 2024 年）
- **主要维护者**：Armin Biere、Aina Niemetz、Mathias Preiner 等
- **学术引用**：在形式化验证领域被广泛引用，相关论文超过 100 篇
- **竞赛成绩**：多次获得 SMT-COMP 位向量类别冠军（2008、2010、2012、2014、2015、2016、2018）

### 1.4 许可证

Boolector 采用 **MIT 许可证** 发布，允许：

- 商业使用
- 修改和分发
- 私有使用
- 再许可

仅需保留原始版权声明和许可证文本。这一宽松的许可协议使得 Boolector 可以被广泛集成到各类商业和学术项目中。

---

## 2. 核心借鉴点

### 2.1 关键技术特性

Boolector 的核心技术特性对 Lv-00 的约束求解引擎设计具有重要参考价值：

#### 2.1.1 位向量运算优化

Boolector 在位向量运算方面采用了多项级优化技术：

1. **Lambert 变换**：针对位向量乘法运算的特殊优化，通过代数变换减少约束复杂度
2. **常量传播**：在约束构建阶段即进行常量折叠和简化
3. **位级优化**：利用位向量的结构特性进行细粒度优化
4. **算术归一化**：将复杂的算术表达式归一化为标准形式

#### 2.1.2 数组理论处理

Boolector 采用基于读取链（read-over-write）的数组编码方案：

- 支持多维数组和嵌套数组
- 实现数组的延迟扩展（lazy expansion）
- 优化数组读取操作的约束生成

#### 2.1.3 增量求解支持

Boolector 支持增量式约束求解，允许：

- 动态添加和删除约束
- 保留学习子句以加速后续求解
- 支持假设推理（assumption-based reasoning）

#### 2.1.4 SAT 后端集成

Boolector 设计了统一的 SAT 求解器接口，支持多种后端：

| SAT 求解器 | 特点 |
|-----------|------|
| MiniSat | 经典实现，稳定可靠 |
| CaDiCaL | 现代高性能求解器 |
| PicoSAT | 轻量级实现 |
| Lingeling | Armin Biere 开发，高度优化 |

### 2.2 Boolector 与 Lv-00 对照表

| 特性维度 | Boolector | Lv-00 当前状态 | 借鉴优先级 |
|---------|-----------|---------------|-----------|
| **理论基础** | 位向量 + 数组理论 | 符号坐标 + 多项式算术 | 高 |
| **约束编码** | 位级布尔编码 | 电路级约束系统 | 高 |
| **求解策略** | 重写简化 + SAT 求解 | 图重写 + 合一引擎 | 中 |
| **增量求解** | 完整支持 | 部分支持 | 高 |
| **变量消除** | 动态变量消除 | 静态分析 | 中 |
| **算术优化** | Lambert 变换 | 基础代数简化 | 高 |
| **数组处理** | 读取链编码 | 几何节点关系 | 中 |
| **多后端支持** | 4+ SAT 求解器 | 单一求解器 | 低 |
| **量化公式** | 完整支持 | 有限支持 | 中 |
| **API 设计** | C API + Python | C 内部 API | 中 |

### 2.3 关键技术借鉴分析

#### 2.3.1 约束重写系统

Boolector 的重写系统采用基于模式匹配的等价变换，这对 Lv-00 的图重写引擎具有参考价值：

```
输入约束 → 词法分析 → 语法分析 → AST 构建
    ↓
重写引擎（多轮迭代）
    ↓
简化后约束 → 编码器 → SAT 公式 → SAT 求解器
```

Lv-00 可以借鉴这一流程，将几何约束的图表示通过重写规则逐步简化。

#### 2.3.2 位向量编码策略

Boolector 将位向量运算编码为布尔电路的技术可以直接应用于 Lv-00 的位电路系统（第 1 层）：

- 算术运算（加、减、乘）的电路编码
- 比较运算的优化编码
- 移位运算的电路实现

#### 2.3.3 增量求解架构

Boolector 的增量求解机制涉及以下关键设计：

1. **上下文管理**：维护约束的添加/删除历史
2. **学习子句管理**：在增量求解中保留有用的学习子句
3. **假设传播**：支持基于假设的快速验证

Lv-00 的证明引擎（第 4 层）可以借鉴这一架构，支持动态约束管理。

---

## 3. Lv-00 映射方案

### 3.1 架构映射

将 Boolector 的技术映射到 Lv-00 的 7 层架构：

| Boolector 组件 | Lv-00 目标层 | 映射说明 |
|---------------|-------------|---------|
| 词法/语法分析 | 第 2 层（建模数据） | 几何约束的解析与建模 |
| 重写/简化引擎 | 第 3 层（算法引擎） | 约束图重写优化 |
| 位向量编码器 | 第 1 层（位电路系统） | 多项式到位电路的编码 |
| SAT 求解接口 | 第 3 层（约束求解） | 可插拔的求解器后端 |
| 增量求解管理 | 第 4 层（证明引擎） | 命题动态管理 |

### 3.2 代码实现示例

以下代码示例展示如何在 Lv-00 中借鉴 Boolector 的位向量求解优化技术。

#### 3.2.1 位向量表达式节点定义

```c
/* file: lv00/core/layer1/bitvector_expr.h */
#ifndef LV00_BITVECTOR_EXPR_H
#define LV00_BITVECTOR_EXPR_H

#include <stdint.h>
#include <stdbool.h>

/* 位向量表达式类型（借鉴 Boolector 的节点分类） */
typedef enum {
    BV_CONST,           /* 常量 */
    BV_VAR,             /* 变量 */
    BV_NOT,             /* 按位非 */
    BV_AND,             /* 按位与 */
    BV_OR,              /* 按位或 */
    BV_XOR,             /* 按位异或 */
    BV_ADD,             /* 加法 */
    BV_SUB,             /* 减法 */
    BV_MUL,             /* 乘法（应用 Lambert 变换） */
    BV_UDIV,            /* 无符号除法 */
    BV_UREM,            /* 无符号取模 */
    BV_SHL,             /* 左移 */
    BV_LSHR,            /* 逻辑右移 */
    BV_ASHR,            /* 算术右移 */
    BV_ULT,             /* 无符号小于 */
    BV_SLT,             /* 有符号小于 */
    BV_EQ,              /* 相等 */
    BV_CONCAT,          /* 连接 */
    BV_EXTRACT,         /* 提取 */
    BV_SEXT,            /* 符号扩展 */
    BV_ZEXT,            /* 零扩展 */
    BV_ITE              /* 条件选择 */
} bv_expr_kind_t;

/* 位向量表达式节点 */
typedef struct bv_expr_node {
    bv_expr_kind_t kind;
    uint32_t width;                    /* 位宽 */
    uint32_t ref_count;                /* 引用计数 */
    
    union {
        /* 常量值 */
        struct {
            uint64_t high;             /* 高位（超过 64 位时使用） */
            uint64_t low;              /* 低位 */
        } const_val;
        
        /* 变量 */
        struct {
            uint32_t var_id;
            char* name;
        } var;
        
        /* 一元运算 */
        struct {
            struct bv_expr_node* operand;
        } unary;
        
        /* 二元运算 */
        struct {
            struct bv_expr_node* left;
            struct bv_expr_node* right;
        } binary;
        
        /* 提取操作 */
        struct {
            struct bv_expr_node* operand;
            uint32_t high_bit;
            uint32_t low_bit;
        } extract;
        
        /* 条件选择 */
        struct {
            struct bv_expr_node* cond;
            struct bv_expr_node* then_branch;
            struct bv_expr_node* else_branch;
        } ite;
    } data;
    
    /* 重写缓存（借鉴 Boolector 的节点哈希） */
    uint64_t hash;
    struct bv_expr_node* next_hash;    /* 哈希冲突链 */
    
    /* 简化标记 */
    bool is_simplified;
    struct bv_expr_node* simplified;   /* 简化后的节点 */
} bv_expr_node_t;

/* 表达式管理器 */
typedef struct bv_expr_mgr {
    /* 节点哈希表（用于结构共享） */
    bv_expr_node_t** hash_table;
    uint32_t hash_size;
    uint32_t node_count;
    
    /* 重写规则表 */
    void* rewrite_rules;
    
    /* 统计信息 */
    uint64_t rewrite_count;
    uint64_t simplification_count;
} bv_expr_mgr_t;

/* API 函数声明 */
bv_expr_mgr_t* bv_expr_mgr_create(void);
void bv_expr_mgr_destroy(bv_expr_mgr_t* mgr);

bv_expr_node_t* bv_const_uint64(bv_expr_mgr_t* mgr, uint64_t val, uint32_t width);
bv_expr_node_t* bv_var(bv_expr_mgr_t* mgr, const char* name, uint32_t width);
bv_expr_node_t* bv_add(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b);
bv_expr_node_t* bv_mul(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b);

/* 重写和简化 */
bv_expr_node_t* bv_rewrite(bv_expr_mgr_t* mgr, bv_expr_node_t* node);
bv_expr_node_t* bv_simplify(bv_expr_mgr_t* mgr, bv_expr_node_t* node);

#endif /* LV00_BITVECTOR_EXPR_H */
```

#### 3.2.2 Lambert 变换优化实现

```c
/* file: lv00/core/layer1/bv_lambert.c */
/* Lambert 变换：优化位向量乘法约束 */

#include "bitvector_expr.h"
#include <stdlib.h>
#include <string.h>

/* 
 * Lambert 变换核心思想：
 * 对于乘法 a * b = c，引入辅助变量分解为部分积
 * 参考 Boolector 的 mulch 实现
 */

/* 检查是否可以应用 Lambert 变换 */
static bool can_apply_lambert_transform(bv_expr_node_t* a, 
                                         bv_expr_node_t* b,
                                         uint32_t width) {
    /* 变换适用于中等位宽（8-64 位）的乘法 */
    if (width < 8 || width > 64) {
        return false;
    }
    
    /* 检查操作数是否包含变量 */
    if (a->kind == BV_CONST || b->kind == BV_CONST) {
        /* 至少一个是常量时，使用常量乘法优化而非 Lambert */
        return false;
    }
    
    return true;
}

/* 应用 Lambert 变换分解乘法 */
bv_expr_node_t* bv_mul_lambert_transform(bv_expr_mgr_t* mgr,
                                          bv_expr_node_t* a,
                                          bv_expr_node_t* b,
                                          uint32_t width) {
    /* 
     * 将乘法分解为部分积的和：
     * a * b = sum_{i=0}^{n-1} (a_i * b) << i
     * 其中 a_i 是 a 的第 i 位
     */
    
    bv_expr_node_t* result = bv_const_uint64(mgr, 0, width);
    
    for (uint32_t i = 0; i < width; i++) {
        /* 提取 a 的第 i 位 */
        bv_expr_node_t* a_bit = bv_extract(mgr, a, i, i);
        
        /* 零扩展 a_bit 到完整位宽 */
        bv_expr_node_t* a_bit_ext = bv_zext(mgr, a_bit, width);
        
        /* 计算 a_bit * b（当 a_bit = 1 时为 b，否则为 0） */
        bv_expr_node_t* partial = bv_and(mgr, a_bit_ext, b);
        
        /* 左移 i 位 */
        bv_expr_node_t* shifted = bv_shl_const(mgr, partial, i);
        
        /* 累加到结果 */
        result = bv_add(mgr, result, shifted);
    }
    
    return result;
}

/* 优化的乘法节点创建 */
bv_expr_node_t* bv_mul_optimized(bv_expr_mgr_t* mgr,
                                  bv_expr_node_t* a,
                                  bv_expr_node_t* b) {
    uint32_t width = a->width;
    
    /* 常量折叠 */
    if (a->kind == BV_CONST && b->kind == BV_CONST) {
        uint64_t val_a = a->data.const_val.low;
        uint64_t val_b = b->data.const_val.low;
        uint64_t result = (val_a * val_b) & ((1ULL << width) - 1);
        return bv_const_uint64(mgr, result, width);
    }
    
    /* 零/一优化 */
    if (a->kind == BV_CONST) {
        uint64_t val_a = a->data.const_val.low;
        if (val_a == 0) return bv_const_uint64(mgr, 0, width);
        if (val_a == 1) return b;
        /* 2 的幂次优化为左移 */
        if ((val_a & (val_a - 1)) == 0) {
            uint32_t shift = 0;
            while (val_a > 1) {
                val_a >>= 1;
                shift++;
            }
            return bv_shl_const(mgr, b, shift);
        }
    }
    
    if (b->kind == BV_CONST) {
        /* 对称处理 */
        return bv_mul_optimized(mgr, b, a);
    }
    
    /* 应用 Lambert 变换（用于约束求解阶段） */
    if (can_apply_lambert_transform(a, b, width)) {
        /* 标记为需要 Lambert 变换（延迟执行） */
        bv_expr_node_t* node = create_binary_node(mgr, BV_MUL, a, b);
        node->flags |= BV_FLAG_LAMBERT_TRANSFORM;
        return node;
    }
    
    return create_binary_node(mgr, BV_MUL, a, b);
}
```

#### 3.2.3 约束求解引擎集成

```c
/* file: lv00/core/layer3/constraint_solver.c */
/* 借鉴 Boolector 的增量求解架构 */

#include "../layer1/bitvector_expr.h"
#include "../layer2/constraint_graph.h"

/* 求解器上下文（借鉴 Boolector 的 Btor 结构） */
typedef struct lv00_solver_ctx {
    /* 表达式管理器 */
    bv_expr_mgr_t* expr_mgr;
    
    /* 约束图 */
    constraint_graph_t* cg;
    
    /* SAT 求解器接口 */
    sat_solver_interface_t* sat_solver;
    
    /* 增量求解状态 */
    struct {
        uint32_t current_level;          /* 当前决策层级 */
        uint32_t* assertion_levels;      /* 各约束的添加层级 */
        uint32_t assertion_count;
        uint32_t assertion_capacity;
    } incremental;
    
    /* 学习子句管理 */
    struct {
        clause_t** clauses;
        uint32_t count;
        uint32_t capacity;
        bool* is_active;
    } learned_clauses;
    
    /* 统计信息 */
    struct {
        uint64_t sat_calls;
        uint64_t sat_time_ms;
        uint64_t rewrite_applications;
    } stats;
} lv00_solver_ctx_t;

/* 创建求解器上下文 */
lv00_solver_ctx_t* lv00_solver_create(void) {
    lv00_solver_ctx_t* ctx = calloc(1, sizeof(lv00_solver_ctx_t));
    
    ctx->expr_mgr = bv_expr_mgr_create();
    ctx->cg = constraint_graph_create();
    ctx->sat_solver = sat_solver_create_default();
    
    /* 初始化增量求解状态 */
    ctx->incremental.assertion_capacity = 1024;
    ctx->incremental.assertion_levels = malloc(
        sizeof(uint32_t) * ctx->incremental.assertion_capacity);
    
    return ctx;
}

/* 添加约束（支持增量求解） */
int lv00_solver_assert(lv00_solver_ctx_t* ctx, 
                        bv_expr_node_t* constraint) {
    /* 重写和简化 */
    bv_expr_node_t* simplified = bv_rewrite(ctx->expr_mgr, constraint);
    simplified = bv_simplify(ctx->expr_mgr, simplified);
    
    /* 添加到约束图 */
    constraint_graph_add(ctx->cg, simplified);
    
    /* 记录添加层级 */
    uint32_t idx = ctx->incremental.assertion_count++;
    if (idx >= ctx->incremental.assertion_capacity) {
        ctx->incremental.assertion_capacity *= 2;
        ctx->incremental.assertion_levels = realloc(
            ctx->incremental.assertion_levels,
            sizeof(uint32_t) * ctx->incremental.assertion_capacity);
    }
    ctx->incremental.assertion_levels[idx] = ctx->incremental.current_level;
    
    return LV00_OK;
}

/* 推入新层级（用于假设推理） */
void lv00_solver_push(lv00_solver_ctx_t* ctx) {
    ctx->incremental.current_level++;
    sat_solver_push(ctx->sat_solver);
}

/* 弹出到上一层级 */
void lv00_solver_pop(lv00_solver_ctx_t* ctx, uint32_t levels) {
    uint32_t target_level = ctx->incremental.current_level - levels;
    
    /* 移除该层级后添加的约束 */
    while (ctx->incremental.assertion_count > 0 &&
           ctx->incremental.assertion_levels[ctx->incremental.assertion_count - 1] > target_level) {
        ctx->incremental.assertion_count--;
        constraint_graph_remove_last(ctx->cg);
    }
    
    ctx->incremental.current_level = target_level;
    sat_solver_pop(ctx->sat_solver, levels);
}

/* 求解主函数 */
sat_result_t lv00_solver_check_sat(lv00_solver_ctx_t* ctx) {
    clock_t start = clock();
    ctx->stats.sat_calls++;
    
    /* 1. 约束图重写优化 */
    constraint_graph_rewrite(ctx->cg, ctx->expr_mgr);
    
    /* 2. 编码为 CNF */
    cnf_formula_t* cnf = encode_to_cnf(ctx->cg, ctx->expr_mgr);
    
    /* 3. 调用 SAT 求解器 */
    sat_result_t result = sat_solver_solve(ctx->sat_solver, cnf);
    
    /* 4. 处理求解结果 */
    if (result == SAT_SATISFIABLE) {
        /* 提取模型 */
        extract_model(ctx, cnf);
    } else if (result == SAT_UNSATISFIABLE) {
        /* 提取不可满足核心 */
        extract_unsat_core(ctx, cnf);
    }
    
    ctx->stats.sat_time_ms += (clock() - start) * 1000 / CLOCKS_PER_SEC;
    
    cnf_formula_destroy(cnf);
    return result;
}
```

#### 3.2.4 重写规则引擎

```c
/* file: lv00/core/layer3/rewrite_engine.c */
/* 借鉴 Boolector 的重写系统 */

#include "../layer1/bitvector_expr.h"

/* 重写规则类型 */
typedef enum {
    REWRITE_CONST_FOLD,      /* 常量折叠 */
    REWRITE_IDENTITY,        /* 恒等式简化 */
    REWRITE_ANNIHILATOR,     /* 零元简化 */
    REWRITE_ABSORPTION,      /* 吸收律 */
    REWRITE_DISTRIBUTIVE,    /* 分配律 */
    REWRITE_LAMBERT_MUL      /* Lambert 乘法变换 */
} rewrite_rule_type_t;

/* 重写规则结构 */
typedef struct rewrite_rule {
    rewrite_rule_type_t type;
    bv_expr_kind_t target_kind;
    bv_expr_node_t* (*apply)(bv_expr_mgr_t* mgr, 
                              bv_expr_node_t* node,
                              void* user_data);
    int priority;
} rewrite_rule_t;

/* 常量折叠规则 */
static bv_expr_node_t* rewrite_const_fold(bv_expr_mgr_t* mgr,
                                           bv_expr_node_t* node,
                                           void* user_data) {
    (void)user_data;
    
    switch (node->kind) {
        case BV_ADD:
            if (node->data.binary.left->kind == BV_CONST &&
                node->data.binary.right->kind == BV_CONST) {
                uint64_t a = node->data.binary.left->data.const_val.low;
                uint64_t b = node->data.binary.right->data.const_val.low;
                uint64_t mask = (1ULL << node->width) - 1;
                return bv_const_uint64(mgr, (a + b) & mask, node->width);
            }
            break;
            
        case BV_MUL:
            if (node->data.binary.left->kind == BV_CONST &&
                node->data.binary.right->kind == BV_CONST) {
                uint64_t a = node->data.binary.left->data.const_val.low;
                uint64_t b = node->data.binary.right->data.const_val.low;
                uint64_t mask = (1ULL << node->width) - 1;
                return bv_const_uint64(mgr, (a * b) & mask, node->width);
            }
            break;
            
        /* 更多常量折叠规则... */
        
        default:
            break;
    }
    
    return node;
}

/* 恒等式简化规则 */
static bv_expr_node_t* rewrite_identity(bv_expr_mgr_t* mgr,
                                         bv_expr_node_t* node,
                                         void* user_data) {
    (void)mgr;
    (void)user_data;
    
    switch (node->kind) {
        case BV_ADD:
            /* x + 0 = x */
            if (node->data.binary.right->kind == BV_CONST &&
                node->data.binary.right->data.const_val.low == 0) {
                return node->data.binary.left;
            }
            if (node->data.binary.left->kind == BV_CONST &&
                node->data.binary.left->data.const_val.low == 0) {
                return node->data.binary.right;
            }
            break;
            
        case BV_MUL:
            /* x * 1 = x */
            if (node->data.binary.right->kind == BV_CONST &&
                node->data.binary.right->data.const_val.low == 1) {
                return node->data.binary.left;
            }
            if (node->data.binary.left->kind == BV_CONST &&
                node->data.binary.left->data.const_val.low == 1) {
                return node->data.binary.right;
            }
            break;
            
        /* 更多恒等式规则... */
        
        default:
            break;
    }
    
    return node;
}

/* 重写规则表 */
static rewrite_rule_t rewrite_rules[] = {
    { REWRITE_CONST_FOLD,   BV_ADD, rewrite_const_fold,   100 },
    { REWRITE_CONST_FOLD,   BV_MUL, rewrite_const_fold,   100 },
    { REWRITE_CONST_FOLD,   BV_AND, rewrite_const_fold,   100 },
    { REWRITE_CONST_FOLD,   BV_OR,  rewrite_const_fold,   100 },
    { REWRITE_IDENTITY,     BV_ADD, rewrite_identity,      90 },
    { REWRITE_IDENTITY,     BV_MUL, rewrite_identity,      90 },
    { REWRITE_ANNIHILATOR,  BV_MUL, NULL,                  90 },
    { REWRITE_ANNIHILATOR,  BV_AND, NULL,                  90 },
    /* 更多规则... */
};

/* 应用所有适用的重写规则 */
bv_expr_node_t* rewrite_engine_apply(bv_expr_mgr_t* mgr,
                                      bv_expr_node_t* node) {
    bool changed = true;
    bv_expr_node_t* current = node;
    int iterations = 0;
    const int max_iterations = 100;
    
    while (changed && iterations < max_iterations) {
        changed = false;
        iterations++;
        
        /* 遍历所有规则 */
        for (size_t i = 0; i < sizeof(rewrite_rules)/sizeof(rewrite_rules[0]); i++) {
            rewrite_rule_t* rule = &rewrite_rules[i];
            
            if (rule->target_kind != current->kind) {
                continue;
            }
            
            if (rule->apply == NULL) {
                continue;
            }
            
            bv_expr_node_t* result = rule->apply(mgr, current, NULL);
            if (result != current) {
                current = result;
                changed = true;
                mgr->rewrite_count++;
                break;  /* 重新开始遍历 */
            }
        }
    }
    
    return current;
}
```

### 3.3 几何约束的位向量编码

Lv-00 的核心应用场景是几何证明，以下展示如何将几何约束编码为位向量运算：

```c
/* file: lv00/core/layer2/geometry_encoding.c */

/* 将符号坐标编码为位向量 */
typedef struct {
    bv_expr_node_t* x;  /* x 坐标 */
    bv_expr_node_t* y;  /* y 坐标 */
    uint32_t precision; /* 定点数精度 */
} bv_point_t;

/* 创建几何点 */
bv_point_t* bv_point_create(bv_expr_mgr_t* mgr, 
                             const char* name,
                             uint32_t precision) {
    bv_point_t* p = malloc(sizeof(bv_point_t));
    p->precision = precision;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "%s_x", name);
    p->x = bv_var(mgr, buf, 32 + precision);
    
    snprintf(buf, sizeof(buf), "%s_y", name);
    p->y = bv_var(mgr, buf, 32 + precision);
    
    return p;
}

/* 编码两点距离约束：dist(A, B) = d */
bv_expr_node_t* encode_distance_constraint(bv_expr_mgr_t* mgr,
                                            bv_point_t* a,
                                            bv_point_t* b,
                                            bv_expr_node_t* d) {
    /* 
     * (x2-x1)^2 + (y2-y1)^2 = d^2
     * 使用定点数运算
     */
    
    bv_expr_node_t* dx = bv_sub(mgr, b->x, a->x);
    bv_expr_node_t* dy = bv_sub(mgr, b->y, a->y);
    
    /* 平方运算（使用 Lambert 变换优化） */
    bv_expr_node_t* dx2 = bv_mul(mgr, dx, dx);
    bv_expr_node_t* dy2 = bv_mul(mgr, dy, dy);
    
    bv_expr_node_t* sum = bv_add(mgr, dx2, dy2);
    bv_expr_node_t* d2 = bv_mul(mgr, d, d);
    
    /* 返回等式约束 */
    return bv_eq(mgr, sum, d2);
}

/* 编码三点共线约束 */
bv_expr_node_t* encode_collinear_constraint(bv_expr_mgr_t* mgr,
                                             bv_point_t* a,
                                             bv_point_t* b,
                                             bv_point_t* c) {
    /*
     * 三点共线当且仅当叉积为零：
     * (b.x - a.x) * (c.y - a.y) = (b.y - a.y) * (c.x - a.x)
     */
    
    bv_expr_node_t* bx_ax = bv_sub(mgr, b->x, a->x);
    bv_expr_node_t* cy_ay = bv_sub(mgr, c->y, a->y);
    bv_expr_node_t* by_ay = bv_sub(mgr, b->y, a->y);
    bv_expr_node_t* cx_ax = bv_sub(mgr, c->x, a->x);
    
    bv_expr_node_t* left = bv_mul(mgr, bx_ax, cy_ay);
    bv_expr_node_t* right = bv_mul(mgr, by_ay, cx_ax);
    
    return bv_eq(mgr, left, right);
}
```

---

## 4. 实现路线图

### 4.1 短期目标（1-3 个月）

| 任务 | 描述 | 优先级 | 依赖 |
|-----|------|-------|------|
| 位向量表达式系统 | 实现基础 BV 节点类型和操作 | 高 | 无 |
| 常量折叠 | 实现基础常量传播优化 | 高 | BV 表达式系统 |
| 恒等式简化 | 实现加减乘除的恒等式规则 | 高 | BV 表达式系统 |
| 节点哈希共享 | 实现结构共享减少内存占用 | 中 | BV 表达式系统 |
| 基础 CNF 编码 | 实现位向量到 CNF 的基础编码 | 高 | BV 表达式系统 |

### 4.2 中期目标（3-6 个月）

| 任务 | 描述 | 优先级 | 依赖 |
|-----|------|-------|------|
| Lambert 变换 | 实现位向量乘法的 Lambert 优化 | 高 | 基础 BV 系统 |
| 重写规则引擎 | 实现可扩展的规则应用框架 | 高 | 基础简化 |
| 增量求解支持 | 实现 push/pop 和假设推理 | 中 | CNF 编码 |
| SAT 后端接口 | 抽象 SAT 求解器接口支持多后端 | 中 | CNF 编码 |
| 数组理论支持 | 实现基础数组约束编码 | 中 | BV 系统 |
| 几何约束编码 | 将几何约束映射到 BV 运算 | 高 | BV 系统 |

### 4.3 长期目标（6-12 个月）

| 任务 | 描述 | 优先级 | 依赖 |
|-----|------|-------|------|
| 动态变量消除 | 实现求解过程中的变量消除 | 中 | 增量求解 |
| 学习子句管理 | 优化学习子句的存储和垃圾回收 | 低 | 增量求解 |
| 量化公式支持 | 实现 forall/exists 量词处理 | 低 | 完整 BV 系统 |
| 多 SAT 后端 | 集成 CaDiCaL、Lingeling 等 | 低 | SAT 接口 |
| 性能调优 | 基于基准测试的性能优化 | 中 | 完整系统 |
| SMT-LIB 兼容 | 支持 SMT-LIB v2 输入格式 | 低 | 完整系统 |

### 4.4 里程碑规划

```
Month 1-2:  [基础 BV 系统] ======> [常量折叠] ======> [恒等式简化]
                ↓
Month 3-4:  [CNF 编码] ==========> [Lambert 变换] ==> [重写引擎]
                ↓
Month 5-6:  [增量求解] ==========> [几何编码] ======> [SAT 接口]
                ↓
Month 7-9:  [变量消除] ==========> [数组理论] ======> [性能调优]
                ↓
Month 10-12: [量化公式] =========> [多后端] ========> [SMT-LIB]
```

---

## 5. 附录

### 5.1 关键 API 列表

#### 5.1.1 位向量表达式 API

| 函数 | 签名 | 描述 |
|-----|------|------|
| bv_expr_mgr_create | `bv_expr_mgr_t* bv_expr_mgr_create(void)` | 创建表达式管理器 |
| bv_expr_mgr_destroy | `void bv_expr_mgr_destroy(bv_expr_mgr_t* mgr)` | 销毁表达式管理器 |
| bv_const_uint64 | `bv_expr_node_t* bv_const_uint64(bv_expr_mgr_t* mgr, uint64_t val, uint32_t width)` | 创建常量节点 |
| bv_var | `bv_expr_node_t* bv_var(bv_expr_mgr_t* mgr, const char* name, uint32_t width)` | 创建变量节点 |
| bv_not | `bv_expr_node_t* bv_not(bv_expr_mgr_t* mgr, bv_expr_node_t* a)` | 按位非 |
| bv_and | `bv_expr_node_t* bv_and(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 按位与 |
| bv_or | `bv_expr_node_t* bv_or(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 按位或 |
| bv_xor | `bv_expr_node_t* bv_xor(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 按位异或 |
| bv_add | `bv_expr_node_t* bv_add(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 加法 |
| bv_sub | `bv_expr_node_t* bv_sub(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 减法 |
| bv_mul | `bv_expr_node_t* bv_mul(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 乘法 |
| bv_udiv | `bv_expr_node_t* bv_udiv(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 无符号除法 |
| bv_urem | `bv_expr_node_t* bv_urem(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 无符号取模 |
| bv_shl | `bv_expr_node_t* bv_shl(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 左移 |
| bv_lshr | `bv_expr_node_t* bv_lshr(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 逻辑右移 |
| bv_ashr | `bv_expr_node_t* bv_ashr(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 算术右移 |
| bv_ult | `bv_expr_node_t* bv_ult(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 无符号小于 |
| bv_slt | `bv_expr_node_t* bv_slt(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 有符号小于 |
| bv_eq | `bv_expr_node_t* bv_eq(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 相等比较 |
| bv_concat | `bv_expr_node_t* bv_concat(bv_expr_mgr_t* mgr, bv_expr_node_t* a, bv_expr_node_t* b)` | 位向量连接 |
| bv_extract | `bv_expr_node_t* bv_extract(bv_expr_mgr_t* mgr, bv_expr_node_t* a, uint32_t high, uint32_t low)` | 位提取 |
| bv_zext | `bv_expr_node_t* bv_zext(bv_expr_mgr_t* mgr, bv_expr_node_t* a, uint32_t width)` | 零扩展 |
| bv_sext | `bv_expr_node_t* bv_sext(bv_expr_mgr_t* mgr, bv_expr_node_t* a, uint32_t width)` | 符号扩展 |
| bv_ite | `bv_expr_node_t* bv_ite(bv_expr_mgr_t* mgr, bv_expr_node_t* cond, bv_expr_node_t* t, bv_expr_node_t* e)` | 条件选择 |
| bv_rewrite | `bv_expr_node_t* bv_rewrite(bv_expr_mgr_t* mgr, bv_expr_node_t* node)` | 应用重写规则 |
| bv_simplify | `bv_expr_node_t* bv_simplify(bv_expr_mgr_t* mgr, bv_expr_node_t* node)` | 简化表达式 |

#### 5.1.2 求解器 API

| 函数 | 签名 | 描述 |
|-----|------|------|
| lv00_solver_create | `lv00_solver_ctx_t* lv00_solver_create(void)` | 创建求解器上下文 |
| lv00_solver_destroy | `void lv00_solver_destroy(lv00_solver_ctx_t* ctx)` | 销毁求解器上下文 |
| lv00_solver_assert | `int lv00_solver_assert(lv00_solver_ctx_t* ctx, bv_expr_node_t* constraint)` | 添加约束 |
| lv00_solver_push | `void lv00_solver_push(lv00_solver_ctx_t* ctx)` | 推入新层级 |
| lv00_solver_pop | `void lv00_solver_pop(lv00_solver_ctx_t* ctx, uint32_t levels)` | 弹出层级 |
| lv00_solver_check_sat | `sat_result_t lv00_solver_check_sat(lv00_solver_ctx_t* ctx)` | 检查可满足性 |
| lv00_solver_get_model | `model_t* lv00_solver_get_model(lv00_solver_ctx_t* ctx)` | 获取模型 |
| lv00_solver_get_unsat_core | `bv_expr_node_t** lv00_solver_get_unsat_core(lv00_solver_ctx_t* ctx, uint32_t* count)` | 获取不可满足核心 |

### 5.2 参考文献

1. **Boolector 原始论文**
   - Brummayer, R., & Biere, A. (2009). Boolector: An Efficient SMT Solver for Bit-Vectors and Arrays. In *Tools and Algorithms for the Construction and Analysis of Systems* (pp. 174-177). Springer.

2. **Lambert 变换**
   - Brummayer, R. (2009). *Efficient SMT Solving for Bit-Vectors and the Extensional Theory of Arrays*. PhD Thesis, Johannes Kepler University Linz.

3. **SMT-LIB 标准**
   - Barrett, C., Fontaine, P., & Tinelli, C. (2017). *The SMT-LIB Standard: Version 2.6*. Technical Report, University of Iowa.

4. **位向量求解综述**
   - Ganesh, V., & Dill, D. L. (2007). A Decision Procedure for Bit-Vectors and Arrays. In *Computer Aided Verification* (pp. 519-531). Springer.

5. **SAT 求解技术**
   - Biere, A., Heule, M., & van Maaren, H. (2009). *Handbook of Satisfiability*. IOS Press.
   - Biere, A. (2020). *SAT Solving*. Lecture Notes, Johannes Kepler University Linz.

6. **数组理论**
   - Stump, A., Barrett, C. W., Dill, D. L., & Levitt, J. R. (2001). A Decision Procedure for an Extensional Theory of Arrays. In *Logic in Computer Science* (pp. 29-37). IEEE.

7. **增量 SAT 求解**
   - Een, N., & Sorensson, N. (2003). An Extensible SAT-solver. In *Theory and Applications of Satisfiability Testing* (pp. 502-518). Springer.

8. **约束求解在几何中的应用**
   - Wu, W. T. (1978). On the Decision Problem and the Mechanization of Theorem-Proving in Elementary Geometry. *Scientia Sinica*, 21(2), 159-172.
   - Chou, S. C., Gao, X. S., & Zhang, J. Z. (1994). *Machine Proofs in Geometry*. World Scientific.

---

## 文档信息

- **文档版本**: 1.0
- **创建日期**: 2026-05-25
- **最后更新**: 2026-05-25
- **维护者**: Lv-00 开发团队
- **关联项目**: Lv-00 形式化几何证明系统
