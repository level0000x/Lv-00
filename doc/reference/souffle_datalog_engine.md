# Lv-00 参考设计：Souffle Datalog 引擎

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [Souffle](https://github.com/souffle-lang/souffle) —— 高性能 Datalog 引擎，将声明式逻辑规则编译为并行 C++ 代码
> **目标**: 借鉴 Souffle 的 Datalog 声明式约束传播、递归聚合、ADT 记录类型、并行编译和证明溯源机制，将 Lv-00 的 ConstraintGraph 编码为 Datalog 事实，通过 Souffle 加速递归约束求解与依赖链追踪

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 Souffle 是什么

Souffle（法语"呼吸"之意）是由悉尼大学和莫纳什大学联合开发的高性能 Datalog 引擎。它被广泛应用于指针分析（Doop）、Java 安全分析、区块链智能合约验证等静态分析场景。Souffle 的核心设计哲学是：**将 Datalog 声明式规则编译为高度并行化的 C++ 代码**，运行时获得接近手写 C++ 的性能，同时保持 Datalog 的声明式表达力：

```prolog
// Souffle 示例：三角形重心判断
.type Point = [x: number, y: number]

.decl is_point(p: Point)
.decl has_segment(a: Point, b: Point)
.decl midpoint(m: Point, a: Point, b: Point)

// Datalog 规则：两点确定一条线段
has_segment(A, B) :- is_point(A), is_point(B), A != B.

// 聚合：计算三角形重心坐标
.decl centroid(gx: number, gy: number, a: Point, b: Point, c: Point)
centroid(sum M.x { M.x } / 3, sum M.y { M.y } / 3, A, B, C) :-
    midpoint(M, A, B).
```

Souffle 的关键机制：

1. **声明式 Datalog 规则**：用 Horn 子句表达约束和推导，执行顺序由引擎自动优化
2. **递归推导 + 半朴素求值**：支持递归规则，semi-naive 求值每轮只对新事实计算
3. **聚合操作**：内置 sum / min / max / count，在规则体内嵌聚合
4. **ADT 记录类型**：`.type Point = [x: number, y: number]` 定义结构化数据类型
5. **C++ 编译**：Datalog 程序编译为并行 C++（OpenMP），获得本机代码性能
6. **证明溯源**：每条推导事实可追踪"为什么成立"的完整推导链
7. **组件系统**：支持模块化 Datalog 程序，大程序拆分为多个 .dl 组件

### 1.2 为什么借鉴 Souffle

Lv-00 的 `constraint_graph.h` 将几何约束建模为图上的节点和边，由 `solver.h` 提取约束方程后批量求解——这等价于在命令式 C 中**手动实现 Datalog 解释器**。借鉴 Souffle 意味着：

1. 将几何约束关系直接声明为 Datalog 规则——更高的抽象层次
2. 利用递归引擎处理"构造链"推导（A→B, B→C, 故 A→C）
3. 利用聚合操作处理多解合并和最优选择（sum/min/max 映射到几何聚合）
4. 利用 C++ 编译获得并行求解性能——多约束分支可并行传播
5. 利用 Provenance 系统记录"证明步骤"——映射到 Lv-00 证明树

---

## 2. 核心借鉴要点

### 2.1 借鉴点对照总表

| 编号 | Souffle 特性 | Lv-00 对应 | 映射说明 |
|------|-------------|-----------|---------|
| **A** | Datalog 声明式约束传播 | ConstraintGraph 约束传播 | 几何约束推理天然是 Datalog 规则，如"两点确定一线段" |
| **B** | 递归推导 + 聚合 | 递归约束求解 | Semi-naive 求值映射到几何构造链的迭代展开 |
| **C** | ADT / 记录类型 | SymbolicCoord | `.type Point = [x: number, y: number]` 对应符号坐标体系 |
| **D** | 并行 C++ 编译 | C 内核性能需求 | Datalog 编译为并行 C++，对标 Lv-00 C 内核 |
| **E** | 证明溯源（Provenance） | 证明树依赖链 | 追踪每条推导来源，重建证明步骤 |

### 2.2 借鉴点 A：Datalog 声明式约束传播

Souffle 用 Horn 子句声明约束关系，引擎自动处理传播。Lv-00 的几何约束传播天然符合 Datalog 语义：

| 几何知识 | Datalog 规则 | Lv-00 语义 |
|---------|-------------|-----------|
| 两点确定线段 | `has_segment(A,B) :- is_point(A), is_point(B), A != B.` | INCIDENCE 约束生成线段 AB |
| M 是 AB 中点 | `midpoint(M,A,B) :- has_segment(A,B), collinear(A,M,B), dist(A,M)=dist(M,B).` | MIDPOINT 传播：M 在 AB 上等距 |
| 三点共线 | `collinear(A,B,C) :- on_line(A,L), on_line(B,L), on_line(C,L).` | INCIDENCE 传递闭包 |
| 全等三角形 | `seg_equal(AB,DE) :- triangle_congruent(ABC,DEF), corresponding(AB,DE).` | 全等 → 对应边等价链 |
| 勾股定理 | `sq(dist(B,C)) = sq(dist(A,B)) + sq(dist(A,C)) :- right_angle(BAC).` | 直角三角形边长关系 |

### 2.3 借鉴点 B：递归推导 + 聚合

Souffle 的递归和聚合直接对应 Lv-00 的递归约束求解：

| Souffle 特性 | Lv-00 几何场景 | 示例 |
|-------------|--------------|------|
| 递归推导（semi-naive） | 构造链迭代展开 | `dep(X,Z) :- dep(X,Y), dep(Y,Z).` → 传递闭包 |
| sum 聚合 | 多点加权平均 | 重心 = sum(x)/3, sum(y)/3 |
| min/max 聚合 | 极值点坐标 | 包围盒 min_x = min{x_i} |
| count 聚合 | 约束引用计数 | 统计某点上多少条约束 |
| 递归 + 聚合组合 | 多边形细分收敛 | 迭代细分至面积 < epsilon |

```prolog
// 递归示例：构造依赖传递闭包
.decl depends_transitive(a: symbol, b: symbol)
depends_transitive(A, B) :- depends_direct(A, B).
depends_transitive(A, C) :- depends_transitive(A, B), depends_direct(B, C).

// 聚合示例：三角形重心
.decl centroid_x(tri_id: number, cx: number)
centroid_x(T, sum Vx { P.x } / 3) :- triangle_vertex(T, P), P.x = Vx.
```

### 2.4 借鉴点 C：ADT / 记录类型

Souffle 的 ADT 声明直接对应 Lv-00 的 `SymbolicCoord` 体系：

| Lv-00 坐标类型 | Souffle ADT 映射 |
|---------------|-----------------|
| `SYMBOLIC_RATIONAL` | `Rational { numer: number, denom: number }` |
| `SYMBOLIC_ALGEBRAIC` | `Algebraic { poly: [number], root_idx: number }` |
| `SYMBOLIC_QUADRATIC` | `Quadratic { a: number, b: number, c: number }` |
| `SYMBOLIC_TRANSCENDENTAL` | `Transcendental { kind: symbol, args: [number] }` |

```prolog
.type SymbolicCoord = Rational { numer: number, denom: number }
                    | Algebraic { poly: [number], root_idx: number }
                    | Quadratic { a: number, b: number, c: number }
                    | Transcendental { kind: symbol, args: [number] }

.type GeomNode = [id: number, ntype: symbol, cx: SymbolicCoord, cy: SymbolicCoord]
```

### 2.5 借鉴点 D：并行 C++ 编译

Souffle 编译管线将 Datalog 翻译为高性能 C++ 可执行文件：

```
Datalog 源文件 (.dl)
  → 语法分析 + 类型检查 → SCC 依赖分析
  → RAM（Relational Algebra Machine）中间表示
  → C++ 代码生成（OpenMP 并行标注）→ GCC/Clang 编译
```

对于 Lv-00 的映射：

| Souffle 编译阶段 | Lv-00 对应 |
|-----------------|-----------|
| 语法分析 + 类型检查 | `.lvz` 解析器 + `ConstraintType` 检查 |
| SCC 依赖分析 | `ConstraintGraph` 拓扑排序（循环依赖检测） |
| RAM 中间表示 | `ConstraintPropagationRule` 数组 |
| C++ 代码生成 | `src/datalog_backend_gen.c` |
| 并行执行 | OpenMP 并行传播独立约束分支 |

### 2.6 借鉴点 E：证明溯源（Provenance）

Souffle 的 provenance 系统追踪每条推导事实的来源，支持三种查询模式：

| Provenance 模式 | Lv-00 映射 |
|----------------|-----------|
| **Why provenance** | 证明树反向构建：`proof_tree_build(step)` |
| **Why-not provenance** | 调试未满足约束时展示缺失前提 |
| **How provenance** | 完整推导 DAG 的依赖链可视化 |

```prolog
// Souffle provenance 查询示例
.explain centroid(3.0, 4.0, A, B, C)
// 输出：centroid 成立因为 midpoint_expand 规则推导了 D, E, F 三个中点

// 对应的 Lv-00 证明步骤：
// Step 1: D = midpoint(B, C)    [规则: midpoint_expand]
// Step 2: E = midpoint(C, A)    [规则: midpoint_expand]
// Step 3: F = midpoint(A, B)    [规则: midpoint_expand]
// Step 4: G = centroid(A,B,C)   [规则: centroid_from_midpoints, 基于 Step 1-3]
```

---

## 3. Lv-00 映射方案

### 3.1 整体架构：`include/lv00/datalog_backend.h`

新增 Datalog 后端主接口，三层抽象：

```c
/**
 * @file datalog_backend.h
 * @brief Souffle 风格 Datalog 后端 —— 将 ConstraintGraph 编码为
 *        Datalog 事实并通过 Souffle 子进程或 C API 求解
 *
 * 三层抽象：
 *   DatalogRule → DatalogProgram → DatalogBackend
 *
 * 工作流程：
 *   ConstraintGraph → datalog_program_encode(graph)
 *     → datalog_backend_solve(backend, program)
 *     → ProvenanceRecord → proof 树重建
 */

#ifndef LV00_DATALOG_BACKEND_H
#define LV00_DATALOG_BACKEND_H

#include "lv00/constraint_graph.h"
#include "lv00/proof.h"

/* ================================================================
 * 第 1 层：DatalogRule —— 单条 Horn 子句
 * ================================================================ */

typedef struct {
    char *name;                         /**< 变量名 */
    GeomType type_constraint;
    int bound_node_id;                  /**< 已绑定节点 ID，-1 表示自由 */
} DatalogVariable;

typedef struct {
    char *predicate;                    /**< 谓词名（如 "has_segment"） */
    DatalogVariable **args;
    int arg_count;
    bool is_negated;
} DatalogAtom;

typedef struct {
    char *rule_name;
    DatalogAtom *consequent;
    DatalogAtom **antecedents; int antecedent_count;
    char **inequality_constraints; int ineq_count;
    int priority;
} DatalogRule;

/* ================================================================
 * 第 2 层：DatalogProgram —— 规则集合
 * ================================================================ */

typedef struct {
    char *program_name;
    DatalogRule **rules; int rule_count; int capacity;
    char **type_decls;              int type_decl_count;
    char **relation_decls;          int relation_decl_count;
    char **facts;                   int fact_count;
} DatalogProgram;

/**
 * @brief 将 ConstraintGraph 编码为 DatalogProgram
 *
 * 节点映射：
 *   GEOM_POINT    → is_point(node_id). coord_x(id, val). coord_y(id, val).
 *   GEOM_SEGMENT  → is_segment(node_id). endpoint(id, A). endpoint(id, B).
 * 约束映射：
 *   INCIDENCE(P,L)    → on_object(P, L).
 *   BETWEENNESS(A,B,C) → between(A, B, C).
 */
DatalogProgram *datalog_program_encode(
    const ConstraintGraph *graph,
    DatalogRule **rules, int rule_count);

char *datalog_program_to_dl(const DatalogProgram *program);

void datalog_program_free(DatalogProgram *program);

/* ================================================================
 * 第 3 层：DatalogBackend —— Souffle 引擎交互
 * ================================================================ */

typedef enum {
    DATALOG_SOLVED, DATALOG_PARTIAL, DATALOG_UNSAT,
    DATALOG_TIMEOUT, DATALOG_ERROR
} DatalogSolveStatus;

/**
 * @brief Provenance 记录 —— 对应 Souffle 的 provenance 输出
 */
typedef struct {
    int fact_id;
    char *rule_name;
    int *antecedent_fact_ids;
    int antecedent_count;
    int derivation_round;
} ProvenanceRecord;

typedef struct {
    char *souffle_binary; char *work_dir;
    int timeout_ms; bool enable_provenance;
    DatalogSolveStatus last_status; char *last_error_msg;
    ProvenanceRecord *provenance_records; int provenance_count;
} DatalogBackend;

DatalogBackend *datalog_backend_create(
    const char *souffle_binary, const char *work_dir);

DatalogSolveStatus datalog_backend_solve(
    DatalogBackend *backend, const DatalogProgram *program);

ProofStep *datalog_backend_reconstruct_proof(
    const DatalogBackend *backend, const ConstraintGraph *graph);

void datalog_backend_free(DatalogBackend *backend);

#endif /* LV00_DATALOG_BACKEND_H */
```

### 3.2 ConstraintGraph 到 Datalog 事实的编码

节点和约束到 Datalog 事实的映射实现：

```c
// 节点编码
static void encode_nodes_to_facts(const ConstraintGraph *graph,
                                   DatalogProgram *program)
{
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = &graph->nodes[i];
        switch (node->type) {
        case GEOM_POINT:
            datalog_program_add_fact(program, "is_point", "node_%d", node->id);
            if (node->symbolic_coords[0])
                datalog_program_add_fact(program, "coord_x", "node_%d, %s",
                    node->id, symbolic_coord_to_string(node->symbolic_coords[0]));
            break;
        case GEOM_LINE_SEGMENT:
            datalog_program_add_fact(program, "is_segment", "node_%d", node->id);
            for (int j = 0; j < node->participant_count; j++)
                datalog_program_add_fact(program, "endpoint", "node_%d, node_%d",
                    node->id, node->participants[j]);
            break;
        }
    }
}

// 约束编码
static void encode_constraints_to_facts(const ConstraintGraph *graph,
                                         DatalogProgram *program)
{
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = &graph->constraints[i];
        switch (c->type) {
        case INCIDENCE:
            datalog_program_add_fact(program, "on_object",
                "node_%d, node_%d", c->participants[0], c->participants[1]);
            break;
        case BETWEENNESS:
            datalog_program_add_fact(program, "between",
                "node_%d, node_%d, node_%d",
                c->participants[0], c->participants[1], c->participants[2]);
            break;
        }
    }
}
```

### 3.3 Souffle 输出到证明树的重建

```c
/**
 * @brief 将 Souffle provenance 输出转换为 Lv-00 ProofStep
 *
 * Souffle provenance 格式：
 *   fact_id  rule_name  (antecedent_fact_ids...)
 * 例如：42  midpoint_expand  (12, 15, 20)
 */
ProofStep *provenance_to_proof_steps(
    const ProvenanceRecord *records, int record_count,
    int target_fact_id, const ConstraintGraph *graph)
{
    ProvenanceRecord *target = find_record(records, record_count, target_fact_id);
    if (!target) return NULL;

    ProofStep *step = proof_step_create();
    step->rule_name = strdup(target->rule_name);
    step->description = rule_to_human_readable(target->rule_name, graph);

    for (int i = 0; i < target->antecedent_count; i++) {
        ProofStep *child = provenance_to_proof_steps(
            records, record_count, target->antecedent_fact_ids[i], graph);
        if (child) proof_step_add_child(step, child);
    }
    return step;
}
```

### 3.4 与现有 solver.h 的集成

```c
/**
 * @brief 扩展求解器：优先 Datalog 后端，失败时回退代数/SMT
 *
 * 策略：
 *   1. ConstraintGraph → DatalogProgram 编码
 *   2. Souffle 求解
 *   3. DATALOG_SOLVED    → 写回结果 + 重建证明树
 *   4. DATALOG_PARTIAL   → 已解约束写回，剩余用代数求解
 *   5. 失败/Souffle 不可用 → 完全回退 solve_algebraic_system()
 */
SolverResult solver_solve_with_datalog(
    ConstraintGraph *graph, DatalogBackend *datalog, SolverConfig *config)
{
    DatalogRule **geom_rules = load_geometry_rules(config->lvz_path);
    DatalogProgram *program = datalog_program_encode(
        graph, geom_rules, geom_rule_count);

    DatalogSolveStatus status = datalog_backend_solve(datalog, program);

    if (status == DATALOG_SOLVED) {
        apply_datalog_results_to_graph(graph, datalog);
        ProofStep *proof = datalog_backend_reconstruct_proof(datalog, graph);
        datalog_program_free(program);
        return SOLVER_OK_WITH_PROOF(proof);
    }
    if (status == DATALOG_PARTIAL) {
        apply_datalog_results_to_graph(graph, datalog);
        SolverResult fallback = solve_algebraic_system(graph);
        datalog_program_free(program);
        return fallback;
    }
    // 完全回退
    datalog_program_free(program);
    return solve_algebraic_system(graph);
}
```

### 3.5 映射总结表

| 现有 API / 结构 | 在 Datalog 后端中的角色 |
|---------------|----------------------|
| `ConstraintGraph.nodes[]` | 编码为 Datalog 事实（is_point / is_segment / coord_x 等） |
| `ConstraintGraph.constraints[]` | 编码为 Datalog 事实（on_object / between / intersects 等） |
| `Constraint.type` | 映射到 Datalog 谓词名 |
| `.lvz` 公理包 | Datalog 规则的声明源（`@datalog_rule` 指令） |
| `solver_solve()` | 扩展为 `solver_solve_with_datalog()`——Datalog 优先 + 回退 |
| `SymbolicCoord` | 映射为 Souffle ADT（Rational / Algebraic / Quadratic 等变体） |
| `ProofStep` / 证明树 | 从 ProvenanceRecord 递归重建 |
| `solver_handle_multiple_solutions()` | 聚合 + 提取最优解 |

---

## 4. 实现路线图

### 4.1 分阶段实施表

| 阶段 | 内容 | 主要文件 | 工期 | 依赖 |
|------|------|---------|------|------|
| **Phase 1** | Datalog 规则 DSL | `include/lv00/datalog_backend.h`（新增）<br>`src/lvz_parser.c`（扩展） | 3-4 周 | `constraint_graph.h`（已有） |
| **Phase 2** | Souffle 集成 | `src/datalog_backend_souffle.c`（新增）<br>`cmake/FindSouffle.cmake`（新增） | 2-3 周 | Phase 1 |
| **Phase 3** | Provenance 回溯 | `src/datalog_provenance.c`（新增）<br>`src/proof_tree.c`（扩展） | 2-3 周 | Phase 2 |

### 4.2 Phase 1 详情：Datalog 规则 DSL

**目标**：建立 Datalog 规则的内部表示，支持 .lvz 加载和 ConstraintGraph 编码。

| 子任务 | 说明 |
|--------|------|
| 定义 `DatalogRule` / `DatalogAtom` / `DatalogProgram` 结构体 | 第 1、2 层数据结构 |
| 实现 `datalog_program_encode()` | 遍历图，将节点/约束编码为 Datalog 事实 |
| 实现 `datalog_program_to_dl()` | 序列化为标准 Souffle .dl 语法 |
| 扩展 .lvz 解析器支持 `@datalog_rule` | 公理包中声明 Datalog 推理规则 |
| 单元测试 | 编码 → 序列化 → 反序列化往返测试 |
| 预定义几何规则库 | 8-12 条核心几何推理规则（见附录 B） |

**.lvz 公理包 Datalog 规则声明示例**：

```
// file: geometry_inference.lvz
@datalog_rule [two_points_segment]
    has_segment(A, B) :- is_point(A), is_point(B), A != B.

@datalog_rule [midpoint_deduction]
    midpoint(M, A, B) :-
        has_segment(A, B), collinear(A, M, B),
        ratio(A, M, M, B, 1, 1).

@datalog_rule [collinear_transitive]
    collinear(A, C, B) :- collinear(A, B, C).

@datalog_rule [centroid_coords] : aggregate(sum)
    centroid_x(T, sum Vx { Vx } / 3) :-
        triangle_vertex(T, V), coord_x(V, Vx).
```

### 4.3 Phase 2 详情：Souffle 集成

**目标**：DatalogBackend 与 Souffle 引擎交互，子进程调用模式。

| 子任务 | 说明 |
|--------|------|
| 实现 `datalog_backend_create()` / `free()` | 初始化/销毁上下文 |
| 实现 `datalog_backend_solve()` | 序列化 → 写 .dl → spawn souffle → 解析输出 |
| 实现 Souffle 输出解析器 | 解析 `--show all` 推导事实 |
| CMake 集成 | `FindSouffle.cmake` 自动查找 |
| Souffle 不可用时优雅降级 | 回退到现有 solver.h 路径 |
| 集成测试 | 三角形重心、中点定理等测试用例 |

**Souffle 子进程调用流程**：

```
Lv-00 (C)                                     Souffle (子进程)
───────────────────────────────────────────────────────────
1. datalog_backend_solve(backend, program)
2. 写入临时 .dl 文件
3. spawn "souffle --show all --provenance tmp.dl"
                                              → 4. 类型检查
                                              → 5. 编译为 RAM 计划
                                              → 6. semi-naive 求值至不动点
7. 读取 stdout → 解析推导事实
8. 读取 provenance 输出文件
9. 返回 DATALOG_SOLVED + ProvenanceRecord[]
```

### 4.4 Phase 3 详情：Provenance 回溯

**目标**：从 Souffle provenance 输出重建 Lv-00 证明步骤树。

| 子任务 | 说明 |
|--------|------|
| 定义 `ProvenanceRecord` 结构体 | 对应 Souffle provenance 格式 |
| 实现 `provenance_to_proof_steps()` | provenance DAG → ProofStep 树 |
| 规则名 → 人类可读描述映射 | `"midpoint_expand"` → "中点 M 是线段 AB 的中点" |
| 实现 `datalog_backend_reconstruct_proof()` | 整合返回完整证明树 |
| Web GUI 渲染 Datalog 推导证明 | ProofPanel 新增"Datalog 推导路径"视图 |
| 证明回归测试 | 验证证明步骤语义正确性 |

---

## 5. 附录

### 附录 A：参考链接

| 资源 | 链接 | 说明 |
|------|------|------|
| Souffle 仓库 | [github.com/souffle-lang/souffle](https://github.com/souffle-lang/souffle) | 主仓库，C++ 实现 |
| Souffle 用户文档 | [souffle-lang.github.io](https://souffle-lang.github.io/) | 语言参考、教程、API |
| Souffle 论文 (CC 2016) | "Souffle: On Synthesis of Program Analyzers" | 设计论文 |
| Provenance 论文 (TaPP 2017) | "Provenance for Datalog" | provenance 系统论文 |
| Doop 指针分析 | [github.com/plast-lab/doop](https://github.com/plast-lab/doop) | Souffle 旗舰应用 |
| Lv-00 constraint_graph.h | `include/lv00/constraint_graph.h` | Lv-00 约束图 |
| Lv-00 solver.h | `include/lv00/solver.h` | Lv-00 求解器 |
| Lv-00 proof.h | `include/lv00/proof.h` | Lv-00 证明系统 |

### 附录 B：预定义几何推理规则库

以下 8 条核心规则覆盖 Lv-00 的几何约束推理，从 .lvz 公理包加载：

| 编号 | 规则名 | Datalog 规则 |
|------|--------|-------------|
| R1 | two_points_segment | `has_segment(A,B) :- is_point(A), is_point(B), A != B.` |
| R2 | collinear_symmetric | `collinear(A,C,B) :- collinear(A,B,C).` |
| R3 | collinear_transitive | `collinear(A,C,D) :- collinear(A,B,C), collinear(B,C,D).` |
| R4 | midpoint_deduction | `midpoint(M,A,B) :- has_segment(A,B), collinear(A,M,B), dist_equal(A,M,M,B).` |
| R5 | centroid_from_midpoints | `centroid(G,A,B,C) :- midpoint(D,B,C), midpoint(E,C,A), midpoint(F,A,B), concurrent(AD,BE,CF,G).` |
| R6 | right_angle_pythagoras | `sq(dist(B,C)) = sq(dist(A,B)) + sq(dist(A,C)) :- right_angle(BAC).` |
| R7 | angle_bisector_ratio | `ratio(BA,BC,DA,DC) :- angle_bisector(BD,ABC), on_segment(D,AC).` |
| R8 | intersect_unique | `point_unique(P) :- intersection(P,AB,CD), AB != CD, !parallel(AB,CD).` |

### 附录 C：Souffle 与 DLV / LogicBlox 对比

| 维度 | Souffle | DLV | LogicBlox |
|------|---------|-----|-----------|
| 编译目标 | 并行 C++ | C++/Java（解释为主） | 专有引擎 |
| ADT 支持 | 原生 `.type` 记录类型 | 无 | 有限 |
| Provenance | why / why-not / how | 有限 | 商业支持 |
| 聚合 | sum/min/max/count | 有限 | 丰富 |
| 开源 | UPL 许可证，活跃社区 | 学术使用 | 商业闭源 |
| Lv-00 适用性 | **首选**：编译为 C++，可链接 C 项目 | 备选 | 不适用 |

### 附录 D：Souffle Provenance 输出示例

```
// 运行: souffle --provenance centroid.dl
// 输入 centroid.dl:
.type Point = [x: number, y: number]
.decl is_point(p: Point)
.decl centroid(g: Point, a: Point, b: Point, c: Point)

is_point(P(0,0)). is_point(P(6,0)). is_point(P(0,8)).

centroid(g, a, b, c) :-
    is_point(a), is_point(b), is_point(c),
    g = [(a.x+b.x+c.x)/3, (a.y+b.y+c.y)/3].

// Provenance 输出:
// Fact centroid(P(2,2.66667), P(0,0), P(6,0), P(0,8)):
//   └── Rule: centroid@centroid.dl:10
//       ├── Fact is_point(P(0,0)) [input]
//       ├── Fact is_point(P(6,0)) [input]
//       └── Fact is_point(P(0,8)) [input]
```

---

> **文档结束**
> 本文档详述了 Souffle Datalog 引擎如何映射到 Lv-00 的 constraint_graph.h 和 solver.h——通过 `DatalogRule` / `DatalogProgram` / `DatalogBackend` 三层抽象，将几何约束图编码为 Datalog 事实，利用 Souffle 的半朴素求值引擎加速递归约束求解，并借助 Provenance 系统从推导路径重建证明树。核心结论：Datalog 是几何约束推理的"母语"——每个几何谓词天然是一个 Datalog 关系，每条构造规则天然是一条 Horn 子句；Souffle 的编译-求值-溯源管线为 Lv-00 提供了从约束声明到证明输出的完整声明式求解路径。
