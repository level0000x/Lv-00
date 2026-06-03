# Aesop: Lean 4 白盒自动化证明策略参考文档

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴点](#2-核心借鉴点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 项目简介

**Aesop**（Automated Extensible Search for Obvious Proofs）是由 Jannis Limperg 开发、Lean 社区维护的 Lean 4 自动化证明策略系统。作为 Lean 4 生态系统的核心组件，Aesop 提供白盒化的自动化证明方法，允许用户通过声明式规则集来指导和控制证明搜索过程。Aesop 的设计哲学强调可解释性和可扩展性，每一步证明搜索都是透明的，用户可以精确理解证明是如何构建的。

### 1.2 技术栈

| 组件 | 技术 | 说明 |
|------|------|------|
| 实现语言 | Lean 4 | 依赖类型函数式编程语言 |
| 元编程框架 | Lean 4 Meta API | 提供语法分析和代码生成能力 |
| 规则引擎 | 自定义规则库 | 支持正向/反向/规范化规则 |
| 搜索算法 | 最佳优先搜索 | 可配置为深度优先或广度优先 |
| 集成方式 | Lean 策略（Tactic） | 与 Lean 证明环境无缝集成 |

### 1.3 社区活跃度

- **GitHub 仓库**: https://github.com/leanprover-community/aesop
- **维护状态**: 活跃维护，与 Lean 4 版本同步更新
- **社区贡献**: 接受社区 PR，有完善的贡献指南
- **依赖项目**: 被 Mathlib4 等多个重要 Lean 项目依赖

### 1.4 许可证

Aesop 采用 **Apache License 2.0** 开源许可证，允许自由使用、修改和商业用途。

---

## 2. 核心借鉴点

### 2.1 Aesop 核心架构

```
规则库（Rule Set）
    ↓
证明状态（Proof State）
    ↓
搜索策略（Search Strategy）
    ↓
规则应用（Rule Application）
    ↓
证明构造（Proof Construction）
```

**核心组件说明：**
- **规则库**：存储用户定义和内置的证明规则，支持规则优先级和权重配置
- **证明状态**：表示当前证明搜索的进展，包含待证明的目标和已建立的假设
- **搜索策略**：最佳优先搜索（默认）、深度优先搜索、广度优先搜索
- **规则应用**：将选定的规则应用于当前证明状态，生成新的子目标
- **证明构造**：将成功的搜索路径转换为可验证的证明项

### 2.2 关键创新点

| 创新特性 | 说明 | 借鉴价值 |
|----------|------|----------|
| 可解释的证明搜索 | 每一步操作都可追踪和审查 | 提升 Lv-00 证明过程透明度 |
| 规则优先级系统 | 基于权重的规则排序 | 优化 Lv-00 证明策略选择 |
| 与元编程深度集成 | 利用 Lean 4 的宏系统 | 参考实现 Lv-00 的 DSL 设计 |
| 增量式证明构建 | 支持部分证明和回溯 | 增强 Lv-00 的交互式证明能力 |
| 规则组合机制 | 支持规则的逻辑组合 | 丰富 Lv-00 的证明策略库 |

### 2.3 Aesop 与 Lv-00 第 4 层对照表

| Aesop 特性 | Lv-00 第 4 层对应组件 | 映射关系 | 实现复杂度 |
|------------|----------------------|----------|------------|
| 规则库（Rule Set） | 证明策略管理器 | 直接映射 | 中 |
| 正向规则（Forward Rule） | 正向推理引擎 | 直接映射 | 中 |
| 反向规则（Backward Rule） | 反向推理引擎 | 直接映射 | 中 |
| 规范化规则（Normalization Rule） | 表达式简化器 | 直接映射 | 低 |
| 最佳优先搜索 | 多策略引擎的优先级调度 | 适配映射 | 高 |
| 证明状态管理 | 命题管理器 | 扩展映射 | 中 |
| 规则优先级 | 策略权重系统 | 直接映射 | 低 |
| 搜索树维护 | 证明树数据结构 | 直接映射 | 中 |
| 元编程集成 | 几何 DSL 解析器 | 参考设计 | 高 |
| 证明项生成 | 证明序列化模块 | 适配映射 | 中 |

### 2.4 具体借鉴建议

**规则系统借鉴：**
- 规则分类体系：正向规则、反向规则、规范化规则
- 规则属性标注：成功率估计、应用条件、副作用声明
- 规则组合机制：顺序组合、选择组合、重复组合

**搜索策略借鉴：**
- 启发式函数设计：目标复杂度评估、规则匹配度评分、历史成功率加权
- 搜索空间剪枝：循环检测机制、超时控制策略、深度限制机制
- 增量搜索支持：部分证明保存、搜索状态序列化

---

## 3. Lv-00 映射方案

### 3.1 架构映射

基于 Aesop 的设计理念，Lv-00 第 4 层证明引擎的增强架构：

```
Lv-00 第 4 层：证明引擎（Proof Engine）
├── 4.1 命题管理器（Proposition Manager）
├── 4.2 多策略引擎（Multi-Strategy Engine）[借鉴 Aesop]
│   ├── 4.2.1 规则库管理器
│   ├── 4.2.2 搜索策略调度器
│   ├── 4.2.3 证明状态追踪器
│   └── 4.2.4 启发式评估器
├── 4.3 证明方法集（Proof Methods）
│   ├── 4.3.1 正向推理（借鉴 Aesop Forward Rule）
│   ├── 4.3.2 反向推理（借鉴 Aesop Backward Rule）
│   ├── 4.3.3 矛盾法
│   ├── 4.3.4 归纳法
│   ├── 4.3.5 坐标法
│   ├── 4.3.6 向量法
│   ├── 4.3.7 面积法
│   └── 4.3.8 全等/相似三角形法
└── 4.4 证明构造器（Proof Constructor）
```

### 3.2 核心数据结构定义

```c
/* aesop_proof_engine.h - Lv-00 证明引擎核心数据结构 */

#ifndef AESOP_PROOF_ENGINE_H
#define AESOP_PROOF_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RULE_FORWARD,
    RULE_BACKWARD,
    RULE_NORMALIZATION,
    RULE_ELIMINATION,
    RULE_INTRODUCTION
} RuleType;

typedef enum {
    SEARCH_BEST_FIRST,
    SEARCH_DEPTH_FIRST,
    SEARCH_BREADTH_FIRST
} SearchStrategy;

typedef enum {
    RULE_SUCCESS,
    RULE_FAILURE,
    RULE_BLOCKED,
    RULE_TIMEOUT
} RuleResult;

typedef struct {
    uint32_t rule_id;
    char name[64];
    RuleType type;
    float priority;
    float success_rate;
    uint32_t max_applications;
    bool is_safe;
    bool is_terminal;
} RuleMetadata;

typedef struct ProofRule {
    RuleMetadata meta;
    RuleResult (*apply)(struct ProofRule* rule,
                        struct ProofState* state,
                        struct ProofGoal* goal);
    bool (*check_conditions)(struct ProofRule* rule,
                             struct ProofState* state);
    struct ProofRule* next;
} ProofRule;

typedef struct {
    ProofRule* rules;
    uint32_t rule_count;
    void* index_table;
} RuleSet;

typedef struct ProofGoal {
    uint32_t goal_id;
    void* proposition;
    float complexity;
    uint32_t depth;
    struct ProofGoal* parent;
    struct ProofGoal* next;
} ProofGoal;

typedef struct ProofState {
    uint32_t state_id;
    ProofGoal* goals;
    uint32_t goal_count;
    void* assumptions;
    void* substitutions;
    float heuristic_score;
    struct ProofState* parent;
} ProofState;

typedef struct SearchNode {
    ProofState* state;
    ProofRule* applied_rule;
    struct SearchNode* parent;
    struct SearchNode* children;
    struct SearchNode* sibling;
    uint32_t visit_count;
    float accumulated_cost;
} SearchNode;

typedef float (*HeuristicFunc)(ProofState* state);

typedef struct {
    SearchStrategy strategy;
    HeuristicFunc heuristic;
    uint32_t max_depth;
    uint32_t max_iterations;
    float timeout_seconds;
    bool enable_caching;
    bool enable_parallel;
} SearchConfig;

typedef struct {
    RuleSet* rule_set;
    SearchConfig* config;
    SearchNode* root;
    SearchNode* current;
    void* priority_queue;
    void* state_cache;
    uint32_t iteration_count;
} SearchContext;

typedef struct ProofStep {
    uint32_t step_id;
    ProofRule* rule;
    void* premises;
    void* conclusion;
    char description[256];
    struct ProofStep* next;
} ProofStep;

typedef struct {
    uint32_t proof_id;
    ProofStep* steps;
    uint32_t step_count;
    bool is_complete;
    void* verification_data;
} ProofTree;

typedef struct {
    bool success;
    ProofTree* proof;
    char error_message[512];
    uint32_t search_nodes;
    float search_time;
} ProofResult;

#endif
```

### 3.3 核心算法实现

```c
/* aesop_search.c - 借鉴 Aesop 最佳优先搜索 */

#include "aesop_proof_engine.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

float default_heuristic(ProofState* state) {
    if (state == NULL || state->goals == NULL) return 0.0f;
    float total_score = 0.0f;
    ProofGoal* goal = state->goals;
    while (goal != NULL) {
        float complexity_penalty = goal->complexity * 10.0f;
        float depth_penalty = goal->depth * 2.0f;
        float goal_count_penalty = state->goal_count * 5.0f;
        total_score += 100.0f - complexity_penalty
                       - depth_penalty - goal_count_penalty;
        goal = goal->next;
    }
    return total_score / state->goal_count;
}

ProofRule* select_best_rule(RuleSet* rule_set,
                            ProofState* state,
                            ProofGoal* goal) {
    if (rule_set == NULL || rule_set->rules == NULL) return NULL;
    ProofRule* best_rule = NULL;
    float best_score = -1.0f;
    ProofRule* rule = rule_set->rules;
    while (rule != NULL) {
        if (rule->check_conditions != NULL) {
            if (!rule->check_conditions(rule, state)) {
                rule = rule->next;
                continue;
            }
        }
        float score = rule->meta.priority;
        score *= (0.5f + 0.5f * rule->meta.success_rate);
        if (rule->meta.type == RULE_NORMALIZATION) score += 0.2f;
        if (score > best_score) {
            best_score = score;
            best_rule = rule;
        }
        rule = rule->next;
    }
    return best_rule;
}

RuleResult apply_rule_to_state(ProofRule* rule,
                               ProofState* state,
                               SearchNode* node) {
    if (rule == NULL || state == NULL) return RULE_FAILURE;
    ProofGoal* goal = state->goals;
    while (goal != NULL) {
        RuleResult result = rule->apply(rule, state, goal);
        if (result == RULE_SUCCESS) {
            rule->meta.success_rate =
                0.9f * rule->meta.success_rate + 0.1f;
            return RULE_SUCCESS;
        }
        goal = goal->next;
    }
    rule->meta.success_rate = 0.9f * rule->meta.success_rate;
    return RULE_FAILURE;
}

ProofResult best_first_search(SearchContext* ctx,
                              ProofState* initial_state) {
    ProofResult result = {0};
    result.success = false;
    if (ctx == NULL || initial_state == NULL) {
        strcpy(result.error_message,
               "Invalid search context or initial state");
        return result;
    }
    SearchNode* root = (SearchNode*)calloc(1, sizeof(SearchNode));
    root->state = initial_state;
    root->applied_rule = NULL;
    root->parent = NULL;
    root->accumulated_cost = 0.0f;
    ctx->root = root;
    ctx->current = root;
    uint32_t iteration = 0;
    while (iteration < ctx->config->max_iterations) {
        iteration++;
        SearchNode* current = ctx->current;
        if (current == NULL) {
            strcpy(result.error_message, "Search space exhausted");
            break;
        }
        if (current->state->goals == NULL ||
            current->state->goal_count == 0) {
            result.success = true;
            result.proof = construct_proof_tree(current);
            result.search_nodes = iteration;
            return result;
        }
        current->visit_count++;
        ProofGoal* goal = current->state->goals;
        while (goal != NULL) {
            ProofRule* rule = select_best_rule(ctx->rule_set,
                                               current->state,
                                               goal);
            if (rule != NULL) {
                ProofState* new_state = copy_proof_state(current->state);
                RuleResult rule_result = apply_rule_to_state(rule,
                                                             new_state,
                                                             current);
                if (rule_result == RULE_SUCCESS) {
                    SearchNode* child = (SearchNode*)calloc(1,
                                                        sizeof(SearchNode));
                    child->state = new_state;
                    child->applied_rule = rule;
                    child->parent = current;
                    child->sibling = current->children;
                    child->accumulated_cost = current->accumulated_cost + 1.0f;
                    child->state->heuristic_score =
                        ctx->config->heuristic(child->state);
                    current->children = child;
                } else {
                    free_proof_state(new_state);
                }
            }
            goal = goal->next;
        }
        ctx->current = select_next_node(ctx);
    }
    if (iteration >= ctx->config->max_iterations) {
        strcpy(result.error_message, "Maximum iterations reached");
    }
    result.search_nodes = iteration;
    return result;
}

ProofTree* construct_proof_tree(SearchNode* success_node) {
    if (success_node == NULL) return NULL;
    ProofTree* proof = (ProofTree*)calloc(1, sizeof(ProofTree));
    proof->is_complete = true;
    SearchNode* node = success_node;
    uint32_t step_count = 0;
    while (node != NULL && node->applied_rule != NULL) {
        ProofStep* step = (ProofStep*)calloc(1, sizeof(ProofStep));
        step->step_id = step_count++;
        step->rule = node->applied_rule;
        snprintf(step->description, sizeof(step->description),
                 "Apply rule '%s' (type: %d)",
                 node->applied_rule->meta.name,
                 node->applied_rule->meta.type);
        step->next = proof->steps;
        proof->steps = step;
        node = node->parent;
    }
    proof->step_count = step_count;
    return proof;
}

void free_proof_state(ProofState* state) {
    if (state == NULL) return;
    ProofGoal* goal = state->goals;
    while (goal != NULL) {
        ProofGoal* next = goal->next;
        free(goal);
        goal = next;
    }
    free(state);
}
```

### 3.4 几何证明专用规则示例

```c
/* geometry_rules.c - Lv-00 几何证明专用规则 */

#include "aesop_proof_engine.h"

RuleResult rule_transitivity(ProofRule* rule,
                             ProofState* state,
                             ProofGoal* goal) {
    /* A = B, B = C => A = C */
    return RULE_SUCCESS;
}

RuleResult rule_congruent_triangles(ProofRule* rule,
                                    ProofState* state,
                                    ProofGoal* goal) {
    /* 全等三角形 => 对应边/角相等 */
    return RULE_SUCCESS;
}

RuleResult rule_similar_triangles(ProofRule* rule,
                                  ProofState* state,
                                  ProofGoal* goal) {
    /* 相似三角形 => 对应边成比例、对应角相等 */
    return RULE_SUCCESS;
}

RuleResult rule_prove_segment_equal(ProofRule* rule,
                                    ProofState* state,
                                    ProofGoal* goal) {
    /* 证明线段相等 => 证明三角形全等 */
    return RULE_SUCCESS;
}

RuleResult rule_simplify_expression(ProofRule* rule,
                                    ProofState* state,
                                    ProofGoal* goal) {
    /* 简化代数表达式 */
    return RULE_SUCCESS;
}

RuleSet* init_geometry_rule_set(void) {
    RuleSet* rule_set = (RuleSet*)calloc(1, sizeof(RuleSet));
    ProofRule* transitivity = (ProofRule*)calloc(1, sizeof(ProofRule));
    strcpy(transitivity->meta.name, "transitivity");
    transitivity->meta.type = RULE_FORWARD;
    transitivity->meta.priority = 0.9f;
    transitivity->meta.success_rate = 0.95f;
    transitivity->meta.is_safe = true;
    transitivity->apply = rule_transitivity;
    transitivity->next = rule_set->rules;
    rule_set->rules = transitivity;
    rule_set->rule_count++;
    ProofRule* congruent = (ProofRule*)calloc(1, sizeof(ProofRule));
    strcpy(congruent->meta.name, "congruent_triangles");
    congruent->meta.type = RULE_FORWARD;
    congruent->meta.priority = 0.8f;
    congruent->meta.success_rate = 0.85f;
    congruent->meta.is_safe = true;
    congruent->apply = rule_congruent_triangles;
    congruent->next = rule_set->rules;
    rule_set->rules = congruent;
    rule_set->rule_count++;
    return rule_set;
}
```

### 3.5 使用示例

```c
/* main.c - Lv-00 证明引擎使用示例 */

#include "aesop_proof_engine.h"
#include <stdio.h>

int main(void) {
    RuleSet* rule_set = init_geometry_rule_set();
    SearchConfig config = {
        .strategy = SEARCH_BEST_FIRST,
        .heuristic = default_heuristic,
        .max_depth = 50,
        .max_iterations = 10000,
        .timeout_seconds = 30.0f,
        .enable_caching = true,
        .enable_parallel = false
    };
    SearchContext ctx = {
        .rule_set = rule_set,
        .config = &config,
        .root = NULL,
        .current = NULL,
        .iteration_count = 0
    };
    ProofState* initial_state = create_initial_state();
    ProofResult result = best_first_search(&ctx, initial_state);
    if (result.success) {
        printf("证明成功！\n");
        printf("搜索节点数: %u\n", result.search_nodes);
        printf("证明步骤数: %u\n", result.proof->step_count);
        ProofStep* step = result.proof->steps;
        while (step != NULL) {
            printf("步骤 %u: %s\n", step->step_id, step->description);
            step = step->next;
        }
    } else {
        printf("证明失败: %s\n", result.error_message);
    }
    return result.success ? 0 : 1;
}
```

---

## 4. 实现路线图

### 4.1 分阶段实施计划

| 阶段 | 时间范围 | 目标 | 关键任务 | 交付物 |
|------|----------|------|----------|--------|
| **短期** | 1-2 个月 | 基础框架搭建 | 1. 设计证明引擎核心数据结构<br>2. 实现规则系统基础框架<br>3. 实现简单的正向/反向推理 | 1. aesop_proof_engine.h<br>2. 基础规则库<br>3. 单元测试套件 |
| **中期** | 3-4 个月 | 搜索策略实现 | 1. 实现最佳优先搜索算法<br>2. 实现启发式评估函数<br>3. 集成到 Lv-00 第 4 层<br>4. 添加几何专用规则 | 1. 完整搜索算法实现<br>2. 几何规则库<br>3. 与现有证明引擎集成 |
| **长期** | 5-6 个月 | 高级特性与优化 | 1. 实现证明解释和可视化<br>2. 添加并行搜索支持<br>3. 优化性能和内存使用<br>4. 完善文档和示例 | 1. 证明可视化工具<br>2. 性能优化版本<br>3. 完整用户文档 |

### 4.2 详细任务分解

**短期任务（第 1-2 个月）：**
- 第 1-2 周：需求分析与设计，分析 Aesop 源码，设计增强架构
- 第 3-4 周：实现核心数据结构（ProofRule、ProofState、ProofGoal）
- 第 5-6 周：实现规则类型枚举和元数据管理，添加 5-10 个基础几何规则
- 第 7-8 周：实现正向推理引擎和反向推理引擎，集成测试

**中期任务（第 3-4 个月）：**
- 第 9-10 周：实现搜索树数据结构和节点扩展机制
- 第 11-12 周：实现优先队列和最佳优先搜索主循环
- 第 13-14 周：设计并实现启发式评估函数，调优搜索性能
- 第 15-16 周：将新证明引擎集成到 Lv-00 第 4 层，端到端测试

**长期任务（第 5-6 个月）：**
- 第 17-18 周：设计证明树可视化格式，实现证明步骤文本描述生成
- 第 19-20 周：设计并行搜索架构，实现多线程搜索支持
- 第 21-22 周：分析性能瓶颈，优化内存分配策略
- 第 23-24 周：编写用户手册，创建示例集合，编写 API 参考文档

### 4.3 风险评估与缓解策略

| 风险 | 可能性 | 影响 | 缓解策略 |
|------|--------|------|----------|
| 搜索空间爆炸 | 高 | 高 | 实现有效的剪枝策略和深度限制 |
| 启发式函数不准确 | 中 | 中 | 基于实际几何问题调优启发式函数 |
| 与现有系统集成困难 | 中 | 高 | 采用渐进式集成策略，保持向后兼容 |
| 性能不达标 | 中 | 中 | 预留并行化扩展点，优化关键路径 |
| 内存使用过高 | 低 | 中 | 实现状态压缩和缓存淘汰策略 |

---

## 5. 附录

### 5.1 关键 API 列表

#### 规则系统 API

```c
ProofRule* rule_create(const char* name, RuleType type);
void rule_destroy(ProofRule* rule);
int rule_set_metadata(ProofRule* rule, const RuleMetadata* meta);
RuleSet* rule_set_create(void);
void rule_set_destroy(RuleSet* rule_set);
int rule_set_add(RuleSet* rule_set, ProofRule* rule);
int rule_set_remove(RuleSet* rule_set, uint32_t rule_id);
ProofRule* rule_set_find(RuleSet* rule_set, const char* name);
```

#### 证明状态 API

```c
ProofState* proof_state_create(void);
void proof_state_destroy(ProofState* state);
ProofState* proof_state_copy(const ProofState* state);
int proof_state_add_goal(ProofState* state, void* proposition);
int proof_state_add_assumption(ProofState* state, void* assumption);
bool proof_state_is_complete(const ProofState* state);
```

#### 搜索 API

```c
SearchContext* search_context_create(RuleSet* rule_set,
                                     const SearchConfig* config);
void search_context_destroy(SearchContext* ctx);
ProofResult best_first_search(SearchContext* ctx, ProofState* initial);
ProofResult depth_first_search(SearchContext* ctx, ProofState* initial);
ProofResult breadth_first_search(SearchContext* ctx, ProofState* initial);
```

#### 证明构造 API

```c
ProofTree* proof_tree_create(void);
void proof_tree_destroy(ProofTree* tree);
int proof_tree_add_step(ProofTree* tree, const ProofStep* step);
bool proof_tree_verify(const ProofTree* tree);
int proof_tree_to_json(const ProofTree* tree, char* buffer, size_t size);
int proof_tree_to_text(const ProofTree* tree, char* buffer, size_t size);
```

### 5.2 参考文献

1. **Aesop 官方仓库** - https://github.com/leanprover-community/aesop
2. **Lean 4 元编程手册** - Tactic Framework、Meta API 章节
3. **Limperg, J. (2023). Aesop: White-Box Proof Search in Lean 4** - 架构设计论文
4. **Lv-00 架构文档** - `c:\Users\xingg\Documents\trae_projects\Lv-00\docs\ARCHITECTURE_v3.3.md`
5. **Lv-00 证明引擎文档** - `c:\Users\xingg\Documents\trae_projects\Lv-00\docs\09_proof.md`
6. **Automated Theorem Proving: Theory and Practice** - Bundy, A. 等
7. **Handbook of Automated Reasoning** - Robinson, A. & Voronkov, A.

---

## 文档信息

- **文档版本**: 1.0
- **创建日期**: 2026-05-25
- **最后更新**: 2026-05-25
- **作者**: Lv-00 开发团队

---

*本文档为 Lv-00 项目参考文档系列的一部分，用于指导 Aesop 自动化证明策略在 Lv-00 中的借鉴和实现。*
