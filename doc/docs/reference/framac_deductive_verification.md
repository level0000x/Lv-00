# Frama-C 演绎验证参考文档

> 面向 Lv-00 项目的 Frama-C 形式化 C 程序验证借鉴分析

---

## 1. 项目概述

### 1.1 简介

Frama-C（[frama-c.com](https://frama-c.com)）是一个工业级 C 程序形式化验证平台，由法国 CEA LIST 和 Inria 联合开发维护。它提供了一套完整的静态分析、运行时验证和演绎证明工具链，能够对关键安全系统（如航空、核电、铁路信号）中的 C 代码进行严格的数学正确性证明。

Frama-C 的核心规约语言是 **ACSL（ANSI/ISO C Specification Language）**，这是一种基于一阶逻辑和分离逻辑的行为接口规约语言（Behavioral Interface Specification Language），可以在不修改原始 C 代码逻辑的前提下，将形式规约以特殊注释的形式嵌入源代码。

Frama-C 与 Lv-00 的相关性在于：Lv-00 的 C 内核（constraint_graph、solver、normalization 等关键模块）是系统的可信计算基（TCB），对这些模块进行形式化验证是保障整个证明平台正确性的关键路径。Frama-C 的 ACSL + WP + E-ACSL 工具链为这一目标提供了成熟、经过工业验证的技术方案。

### 1.2 技术栈

| 组件 | 语言 / 技术 | 说明 |
|---|---|---|
| 核心框架 | OCaml | Frama-C 主体由 OCaml 编写，提供插件体系架构 |
| C 前端 | CIL (C Intermediate Language) | 基于 CIL 的 C 解析与 AST 操作，支持 C99 子集 |
| 规约语言 | ACSL | 一阶逻辑 + 分离逻辑，支持行为规约与数据不变量 |
| 证明引擎 | Why3 | 中间验证平台，将验证目标翻译到多种 SMT/ATP 后端 |
| SMT 求解器 | Z3, Alt-Ergo, CVC4, CVC5 | 通过 Why3 调度的多证明器后端 |
| 交互式证明 | Coq | 对自动证明器无法解决的复杂目标进行交互式证明 |
| 运行时检查 | E-ACSL | 将 ACSL 规约编译为 C 运行时代码进行动态验证 |
| 构建集成 | CMake / Makefile | 支持将 Frama-C 集成到现有 C 项目的构建流程 |

### 1.3 许可证

Frama-C 以 **LGPL 2.1** 许可证发布。这意味着：
- 可以将 Frama-C 作为独立的验证工具集成到 Lv-00 的 CI/CD 流程中，无需修改 Lv-00 自身的许可证。
- 如果仅调用 Frama-C 的命令行工具或 API，Lv-00 的代码不受 LGPL 传染性条款影响。
- Frama-C 的 ACSL 规约编写风格、验证方法论等设计思想可以自由借鉴，不受许可证约束。

---

## 2. 核心借鉴点

Frama-C 的设计理念与 Lv-00 存在深层的哲学共鸣：两者都追求将构造与证明统一在同一表达框架中。下表系统梳理了五大核心借鉴点及其与 Lv-00 的对应关系。

### 2.1 核心借鉴对照表

| # | Frama-C 能力 | Frama-C 实现方式 | Lv-00 对应模块 | 借鉴价值 |
|---|---|---|---|---|
| a | ACSL 代码+规约一体化 | 规约以 `/*@ ... */` 注释嵌入 C 源码，规约与实现物理共存 | `func_block` / 构造即证明 | 哲学一致性：规约即文档，规约即证明目标 |
| b | WP 最弱前置条件计算 | 通过 WP 插件从后置条件反推前置条件，生成验证条件（VC） | `proof.h` / 证明引擎 | 自动化验证流程：从目标反推前置条件 |
| c | E-ACSL 运行时断言检查 | 将 ACSL 规约编译为 C 运行时断言，违规时触发 abort/log | `debug.h` / 调试模式 | 开发调试：将规约转化为可执行的运行时检查 |
| d | 模块化函数契约验证 | 每个函数独立验证，通过 `requires/ensures` 契约组合为系统证明 | `func_block` 组合子 | 组合式证明：函数级验证 + 契约组合 |
| e | Why3 多证明器调度 | Why3 将 VC 翻译为多后端输入，并行调用 Z3/Alt-Ergo/CVC4/Coq | `smt_backend.h` / 多策略引擎 | 多后端冗余：不同证明器互补覆盖 |

### 2.2 详细分析

#### a. ACSL 代码+规约一体化

**Frama-C 模式：**

```c
/*@ requires x >= 0;
    assigns \nothing;
    ensures \result >= 0;
    ensures \result * \result <= x;
    ensures ((\result + 1) * (\result + 1)) > x;
 */
int integer_sqrt(int x) {
    // ... 实现代码 ...
}
```

**借鉴要点：** ACSL 的最大设计洞见是"规约应该紧邻实现"——规约不是在外部文档或单独文件中描述，而是作为特殊注释嵌入在函数声明之前。这种设计使得：

1. **可维护性**：修改实现时必须同时审视规约，防止规约与实现脱节。
2. **自文档化**：函数签名 + ACSL 规约构成了函数行为的完整描述。
3. **工具友好**：Frama-C 可以直接解析源码中的 ACSL 注释，无需额外的映射配置。

**Lv-00 映射：** Lv-00 的"构造即证明"哲学与此完全一致：用户在构造数学对象（函数块、约束图、证明树）的同时即完成了证明的构造。Lv-00 可以参考 ACSL 的注释风格，在 C 内核头文件中添加规约注释，使得头文件同时成为函数的行为规约文档。

#### b. WP（Weakest Precondition）插件

**Frama-C 模式：**

WP 插件的工作流程如下：

```
C代码 + ACSL规约
    │
    ▼
WP 插件：计算最弱前置条件
    │
    ▼
生成验证条件（Verification Conditions, VC）
    │
    ▼
Why3 将 VC 翻译为 SMT-LIB 格式
    │
    ▼
SMT 求解器验证（Z3 / Alt-Ergo / CVC4）
    │
    ▼
验证结果：✅ 有效 / ❌ 反例 / ? 超时
```

核心思想是 Dijkstra 的最弱前置条件演算：给定一段程序 C 和一个后置条件 Q，WP 计算最弱的前提条件 P，使得 `{P} C {Q}` 成立。如果当前前置条件蕴含 P，则程序正确。

**借鉴要点：**

1. **逆向推理**：从预期结果反推所需条件，与正向执行流互补。
2. **验证条件生成**：WP 将程序正确性问题分解为多个一阶逻辑公式（验证条件）。
3. **SMT 求解**：验证条件交给 SMT 求解器自动判定。

**Lv-00 映射：** Lv-00 的 `proof.h` 证明引擎可以借鉴 WP 的 VC 生成模式：当用户构造一个证明时，Lv-00 可以将证明目标分解为多个验证条件，每个条件对应一个可以交给 SMT 求解器判断的逻辑公式。这与 Lv-00 现有的 `smt_backend.h` 多求解器调度能力完全兼容。

#### c. 运行时断言检查（E-ACSL）

**Frama-C 模式：**

```c
/*@ assert x != 0; */
int y = 100 / x;    // E-ACSL 会在运行时检查 x != 0

/*@ loop invariant 0 <= i <= n; */
for (int i = 0; i < n; i++) {
    // E-ACSL 在每次循环迭代前检查不变量
}
```

E-ACSL 插件将 ACSL 中的 `assert`、`loop invariant`、函数契约等规约编译为 C 运行时代码：

- `requires` 在函数入口处检查
- `ensures` 在函数出口处检查
- `assert` 在语句位置检查
- `loop invariant` 在循环每次迭代前检查

**借鉴要点：** 静态验证（WP）和动态检查（E-ACSL）形成互补：静态验证在编译时证明"对所有输入都正确"，动态检查在运行时验证"当前执行路径正确"。两者的组合提供了分层验证策略。

**Lv-00 映射：** Lv-00 的 `debug.h` 已经提供了调试模式下的不变量断言机制。E-ACSL 的模式为 Lv-00 提供了一个参考：可以将 Lv-00 内部的状态不变量（如图的连通性、解的一致性等）表达为 ACSL 风格的运行时断言，在 Debug 构建中执行，在 Release 构建中由编译器优化掉。

#### d. 模块化函数契约验证

**Frama-C 模式：**

```c
/*@ requires valid_matrix(m, rows, cols);
    requires rows > 0 && cols > 0;
    assigns \nothing;
    ensures \result >= 0;
 */
double matrix_norm(double *m, int rows, int cols);

// 调用方验证时，Frama-C 仅使用 matrix_norm 的契约，
// 不需要展开其实现。实现可以被单独验证。
```

Frama-C 的模块化验证策略：每个函数通过对自身的 `requires/ensures` 契约独立证明。调用方验证时，Frama-C 仅使用被调用函数的契约（而非其实现体），从而将验证复杂度保持在线性级别，避免验证组合爆炸。

**借鉴要点：**

1. **契约即接口**：函数的 `requires/ensures` 契约定义了函数的抽象行为——调用方不需要知道实现细节。
2. **独立验证**：每个函数可以被独立验证，不依赖于调用上下文。
3. **组合式证明**：系统级证明由函数级证明通过契约组合而成。

**Lv-00 映射：** Lv-00 的 `func_block` 组合子正是此思想的体现：每个 `func_block` 有明确的前置条件和后置条件，func_block 的验证通过组合子串联。Frama-C 的模块化验证策略为 Lv-00 的 `func_block` 提供了严格的 C 语言级别的验证参考。

#### e. Why3 多证明器调度

**Frama-C 模式：**

```
                    ┌─────────────────┐
                    │   Why3 调度层    │
                    │  (中间表示: WhyML) │
                    └───────┬─────────┘
            ┌───────────────┼───────────────┐
            ▼               ▼               ▼
    ┌───────────┐   ┌───────────┐   ┌───────────┐
    │   Z3      │   │ Alt-Ergo  │   │   CVC4    │
    │ (Microsoft)│   │ (Inria)   │   │ (Stanford)│
    └───────────┘   └───────────┘   └───────────┘
            │               │               │
            └───────────────┼───────────────┘
                            ▼
                    ┌───────────┐
                    │   Coq     │  ← 交互式备选
                    │ (Inria)   │     当 SMT 超时
                    └───────────┘
```

Why3 作为中间验证平台的关键能力：
- 将验证目标从 Frama-C 的 Why3 中间表示翻译到不同求解器的输入格式。
- 并行调用多个求解器——只要有一个返回 "Valid"，即宣告验证通过。
- 对自动求解器超时的目标，可输出到 Coq 进行交互式证明。

**借鉴要点：**

1. **中间表示层**：Why3 的 WhyML 语言将验证目标与具体求解器解耦。
2. **多求解器并行**：不同求解器在不同类型的目标上各有所长。
3. **降级策略**：自动证明失败时降级到交互式证明。

**Lv-00 映射：** Lv-00 的 `smt_backend.h` 已经实现了多策略证明引擎，支持 Z3 等多种 SMT 求解器。Why3 的设计为 Lv-00 的 SMT 后端抽象层提供了扩展方向——引入中间表示层以支持更多证明器后端。

---

## 3. Lv-00 映射方案

本节提供具体的 Lv-00 C 内核代码的 ACSL 规约示例，展示如何将 Frama-C 的验证方法论应用于 Lv-00 的核心模块。

### 3.1 constraint_graph 模块：节点与边不变量

Lv-00 的 `constraint_graph.h` 定义了约束图的核心数据结构。下面对其核心操作编写 ACSL 规约。

```c
/* constraint_graph.h — ACSL 规约示例 */

/*@
  // 图节点的结构不变量
  predicate valid_node{L}(lv00_node *n) =
    n != \null &&
    n->id > 0 &&
    n->type >= NODE_CONSTRAINT && n->type <= NODE_VARIABLE &&
    \valid(n);

  // 约束边的结构不变量
  predicate valid_edge{L}(lv00_edge *e) =
    e != \null &&
    \valid_read(e->src) && \valid_read(e->dst) &&
    valid_node(e->src) && valid_node(e->dst);

  // 图的不变量：所有节点 ID 唯一
  predicate nodes_unique_ids{L}(lv00_graph *g) =
    \forall integer i, j;
      0 <= i < g->node_count &&
      0 <= j < g->node_count &&
      i != j ==>
      g->nodes[i]->id != g->nodes[j]->id;

  // 约束图可达性不变量
  predicate constraint_reachable{L}(lv00_graph *g, lv00_node *src, lv00_node *dst) =
    \exists integer k; 0 <= k < g->edge_count &&
    g->edges[k]->src == src &&
    g->edges[k]->dst == dst;
 */

/*@
  requires \valid(g) && g->node_count >= 0;
  requires \valid(n) && valid_node(n);
  requires nodes_unique_ids(g);
  requires !node_in_graph(g, n);   // 插入前节点不在图中

  assigns g->nodes[g->node_count], g->node_count;
  ensures g->node_count == \old(g->node_count) + 1;
  ensures node_in_graph(g, n);     // 插入后节点在图中
  ensures nodes_unique_ids(g);     // ID 唯一性保持不变
 */
int graph_add_node(lv00_graph *g, lv00_node *n);

/*@
  requires \valid(g) && g->edge_count < MAX_EDGES;
  requires \valid(e) && valid_edge(e);
  requires node_in_graph(g, e->src) && node_in_graph(g, e->dst);
  requires !edge_in_graph(g, e);   // 插入前边不在图中

  assigns g->edges[g->edge_count], g->edge_count;
  ensures g->edge_count == \old(g->edge_count) + 1;
  ensures edge_in_graph(g, e);     // 插入后边在图中
  ensures constraint_reachable(g, e->src, e->dst);
 */
int graph_add_edge(lv00_graph *g, lv00_edge *e);
```

**验证目标：** 通过 Frama-C WP 验证 `graph_add_node` 和 `graph_add_edge` 正确维护了图的结构不变量——节点 ID 唯一性、可达性关系的正确建立，以及插入操作前后的状态转换。

### 3.2 solver 模块：方程求解正确性

Lv-00 的 `solver.h` 定义了符号方程求解的核心接口。下面编写 ACSL 规约来形式化求解器的"解"的正确性。

```c
/* solver.h — ACSL 规约示例 */

/*@
  // 定义"代入后等式成立"
  predicate solution_holds{L}(lv00_equation *eq, lv00_substitution *sub) =
    \let result = apply_substitution(eq->lhs, sub);
    \let expected = apply_substitution(eq->rhs, sub);
    expression_equal(result, expected);

  // 定义"解集合中的每一组解都满足方程"
  predicate all_solutions_valid{L}(lv00_equation *eq, lv00_solution_set *sol) =
    \forall integer i; 0 <= i < sol->count ==>
    solution_holds(eq, &(sol->solutions[i]));
 */

/*@
  requires \valid(eq);
  requires eq->type >= EQ_LINEAR && eq->type <= EQ_POLYNOMIAL;
  requires \valid(sol);
  requires sol->solutions == \null && sol->count == 0;

  assigns sol->solutions[0..sol->count-1], sol->count;
  ensures sol->count >= 0;
  ensures sol->count > 0 ==> all_solutions_valid(eq, sol);
  ensures sol->count == 0 ==> no_solution_exists(eq);
 */
int solve_equation(lv00_equation *eq, lv00_solution_set *sol);

/*@
  requires \valid(sys);
  requires sys->eq_count > 0;
  requires all_equations_satisfied(sys, \old(sys->solutions));

  assigns sys->solutions[0..sys->sol_count-1], sys->sol_count;
  ensures sys->sol_count >= 0;
  ensures sys->sol_count > 0 ==>
    \forall integer i; 0 <= i < sys->eq_count ==>
    solution_holds(&(sys->equations[i]), &(sys->solutions[0]));
 */
int solve_system(lv00_equation_system *sys);
```

**验证目标：** 证明 `solve_equation` 返回的每一个解代入原方程后等式成立（即求解器返回的解是正确的），以及在无解情况下正确报告。`solve_system` 则证明了方程组的联立求解正确性。

### 3.3 normalization 模块：代数规范化不变性

```c
/* normalization.h — ACSL 规约示例 */

/*@
  // 规范化保持数学等价性
  predicate mathematically_equivalent{L}(lv00_expr *a, lv00_expr *b) =
    \forall integer x_val;
    eval_expression(a, x_val) == eval_expression(b, x_val);
 */

/*@
  requires \valid(expr);
  requires expr->well_formed;

  assigns *expr;   // 可能修改表达式
  ensures \result == 0;                    // 规范化成功
  ensures mathematically_equivalent(\old(expr), expr);  // 等价性保持
  ensures is_normal_form(expr);            // 输出为标准型
 */
int normalize_expression(lv00_expr *expr);
```

**验证目标：** 证明规范化变换不会改变表达式的数学含义（等价性保持不变量），同时规范化后的输出确实处于标准形式。

### 3.4 综合：TCB 质量提升路径

Lv-00 的 C 内核模块构成了系统的可信计算基（Trusted Computing Base, TCB）——如果这些模块存在 bug，整个证明平台的可靠性都将受损。通过 Frama-C 对核心模块进行 ACSL 规约 + WP 演绎验证，可以实现以下 TCB 质量提升：

| TCB 模块 | ACSL 规约量 | 验证覆盖度 | TCB 质量提升 |
|---|---|---|---|
| constraint_graph | 节点/边不变量 + 图操作的正确性 | 全函数级契约 | 防止图结构损坏 |
| solver | 解的正确性 + 方程组联立求解 | 核心求解路径 | 确保解的可信度 |
| normalization | 等价性保持 + 标准型保证 | 规范化全路径 | 防止代数变换错误 |
| rewrite | 重写规则的应用正确性 | 规则匹配 + 替换 | 保证重写语义正确 |
| unify | 合一结果的最一般性 | 合一算法核心 | 防止合一结果过强/过弱 |

---

## 4. 实现路线图

### 4.1 总体时间线

Lv-00 的 Frama-C 形式化验证集成分为三个阶段，每个阶段产出独立的可验证成果，阶段之间呈递进关系。

### 4.2 分阶段实施计划

| 阶段 | 名称 | 工作内容 | 预计产出 | 前置依赖 | 预计工时 |
|---|---|---|---|---|---|
| Phase 1 | 核心不变量标注 | 为 `constraint_graph.h`、`solver.h`、`normalization.h` 编写完整的 ACSL 规约 | ACSL 规约注释 + 不变量文档 | 模块 API 稳定 | 3-4 周 |
| Phase 2 | WP 验证集成 | CMake 集成 Frama-C WP、CI 自动验证流程、验证报告生成 | CI 配置 + 验证报告模板 | Phase 1 完成 | 2-3 周 |
| Phase 3 | 运行时检查 | E-ACSL 编译 + Debug 构建集成 + fuzz 测试联动 | Debug 构建 + fuzz 集成 | Phase 1 完成、Phase 2 可选 | 2 周 |

### 4.3 Phase 1: 核心不变量标注

**目标：** 为 Lv-00 C 内核的核心头文件添加 ACSL 规约，使每个关键函数具备可验证的行为契约。

**具体任务：**

1. **constraint_graph.h ACSL 规约**
   - 定义图节点结构不变量 `valid_node`
   - 定义边结构不变量 `valid_edge`
   - 为 `graph_add_node`、`graph_add_edge`、`graph_remove_node`、`graph_find_path` 添加函数契约
   - 定义图级别不变量：连通性、无悬挂边、ID 唯一性

2. **solver.h ACSL 规约**
   - 定义"解正确性"谓词 `solution_holds`
   - 定义"全部解正确"谓词 `all_solutions_valid`
   - 为 `solve_equation`、`solve_system` 添加函数契约
   - 定义求解终止性保证

3. **normalization.h ACSL 规约**
   - 定义"数学等价性"谓词 `mathematically_equivalent`
   - 定义"标准型"谓词 `is_normal_form`
   - 为 `normalize_expression` 添加函数契约

4. **辅助模块 ACSL 规约（可选扩展）**
   - `rewrite.h`：重写规则应用正确性
   - `unify.h`：合一结果的最一般性（most general unifier）
   - `func_block.h`：函数组合子的契约传递性

**交付物：** 带有完整 ACSL 规约的头文件 + 不变量文档（markdown 格式）。

### 4.4 Phase 2: WP 验证集成

**目标：** 将 Frama-C WP 集成到 Lv-00 的 CMake 构建系统和 CI 流程中，实现每次提交时的自动形式验证。

**具体任务：**

1. **CMake 构建集成**
   ```cmake
   # Lv-00 CMakeLists.txt 中添加 Frama-C 验证目标
   find_program(FRAMAC_EXECUTABLE frama-c)
   if(FRAMAC_EXECUTABLE)
     add_custom_target(verify
       COMMAND ${FRAMAC_EXECUTABLE} -wp
               -wp-prover z3,alt-ergo,cvc4
               -wp-timeout 30
               src/core/constraint_graph.c
               src/core/solver.c
               src/core/normalization.c
       COMMENT "Running Frama-C WP deductive verification"
     )
   endif()
   ```

2. **CI 自动化**
   - GitHub Actions workflow 中添加 `verify` 步骤
   - 验证失败时自动生成 HTML 验证报告
   - 历史验证结果存档（回归测试用）

3. **验证报告**
   - 每次验证生成结构化 JSON 报告（通过 `frama-c -wp-report`）
   - 报告包含：已验证函数数、验证条件数、通过/失败/超时统计
   - 将报告渲染为 markdown summary 贴到 PR comment

4. **证明器配置**
   - 默认并行调用 Z3 + Alt-Ergo + CVC4
   - 配置合理的超时策略（建议 30s per VC）
   - 对自动证明超时的目标标记为待交互式证明（留待后续 Coq 集成）

### 4.5 Phase 3: 运行时检查

**目标：** 将 ACSL 规约通过 E-ACSL 编译为运行时断言，在 Debug 构建和 fuzz 测试中实时检查不变量。

**具体任务：**

1. **E-ACSL 编译集成**
   ```cmake
   # Debug 构建中启用 E-ACSL
   if(CMAKE_BUILD_TYPE STREQUAL "Debug")
     add_custom_command(
       OUTPUT ${CMAKE_BINARY_DIR}/e_acsl/
       COMMAND ${FRAMAC_EXECUTABLE} -e-acsl
               -e-acsl-output-dir ${CMAKE_BINARY_DIR}/e_acsl/
               src/core/constraint_graph.c
               src/core/solver.c
     )
   endif()
   ```

2. **Debug 构建配置**
   - Debug 模式下链接 E-ACSL 运行时库（`libeacsl-rt`）
   - 配置断言违规的处理策略：开发环境 abort + 栈回溯、CI 环境 log + 继续
   - 添加 `LV00_ACSL_CHECK` 宏封装 E-ACSL 断言

3. **Fuzz 测试联动**
   - 将 E-ACSL 插桩后的二进制作为 fuzz target
   - 利用 libFuzzer / AFL++ 生成海量随机输入
   - E-ACSL 运行时断言在 fuzz 中发现不变量违反时触发
   - 自动收集触发失败的输入作为回归测试用例

### 4.6 长期愿景

| 阶段 | 内容 | 说明 |
|---|---|---|
| Phase 4（远期） | Coq 交互式证明集成 | 对 WP 自动证明超时的复杂验证目标，导出到 Coq 进行交互式证明 |
| Phase 5（远期） | TCB 全覆盖 | 将 ACSL 规约扩展到 Lv-00 所有 C 内核模块，实现 TCB 全覆盖验证 |
| Phase 6（远期） | 规约驱动开发 | 新模块开发时先编写 ACSL 规约，再编写实现——"规约先行"的开发模式 |

---

## 5. 附录

### 5.1 Frama-C 快速参考

| 命令 | 说明 |
|---|---|
| `frama-c -wp file.c` | 对 file.c 执行 WP 演绎验证 |
| `frama-c -wp-prover z3,alt-ergo` | 指定证明器列表 |
| `frama-c -wp-timeout 30` | 设置每个验证条件的超时（秒） |
| `frama-c -wp-report report.json` | 生成 JSON 验证报告 |
| `frama-c -e-acsl file.c` | 对 file.c 执行 E-ACSL 运行时检查转换 |
| `frama-c -eva file.c` | 基于抽象解释的值分析（EVA） |
| `frama-c-gui` | Frama-C 图形界面 |

### 5.2 ACSL 语法速查

| ACSL 构造 | 示例 | 说明 |
|---|---|---|
| `requires` | `requires x > 0;` | 函数前置条件 |
| `ensures` | `ensures \result >= 0;` | 函数后置条件 |
| `assigns` | `assigns \nothing;` | 函数副作用声明 |
| `assert` | `assert inv != 0;` | 插入断言 |
| `loop invariant` | `loop invariant 0 <= i <= n;` | 循环不变量 |
| `\forall` | `\forall integer i; 0 <= i < n ==> a[i] >= 0` | 全称量化 |
| `\exists` | `\exists integer i; 0 <= i < n && a[i] == 0` | 存在量化 |
| `\old` | `\old(x)` | 函数入口时的变量值 |
| `\result` | `\result` | 函数返回值 |
| `predicate` | `predicate sorted{L}(int *a, int n) = ...` | 自定义谓词 |

### 5.3 相关资源

| 资源 | 链接 |
|---|---|
| Frama-C 官方网站 | [https://frama-c.com](https://frama-c.com) |
| ACSL 语言参考手册 | [https://frama-c.com/html/acsl.html](https://frama-c.com/html/acsl.html) |
| WP 插件手册 | [https://frama-c.com/html/wp.html](https://frama-c.com/html/wp.html) |
| E-ACSL 插件手册 | [https://frama-c.com/html/e-acsl.html](https://frama-c.com/html/e-acsl.html) |
| Why3 平台 | [https://why3.lri.fr](https://why3.lri.fr) |
| Frama-C GitHub 仓库 | [https://github.com/Frama-C/Frama-C-snapshot](https://github.com/Frama-C/Frama-C-snapshot) |
| Lv-00 原有参考：why3_multi_prover_dispatch.md | `docs/reference/why3_multi_prover_dispatch.md` |

### 5.4 Lv-00 相关模块索引

| Lv-00 模块（头文件） | 路径 | 计划 ACSL 覆盖 |
|---|---|---|
| constraint_graph | `include/lv00/constraint_graph.h` | 节点/边不变量 + 图操作 |
| solver | `include/lv00/solver.h` | 解正确性 + 方程组求解 |
| normalization | `include/lv00/normalization.h` | 等价性保持 + 标准型 |
| rewrite | `include/lv00/rewrite.h` | 重写规则应用正确性（Phase 1 扩展） |
| unify | `include/lv00/unify.h` | 最一般合一（Phase 1 扩展） |
| func_block | `include/lv00/func_block.h` | 组合子契约传递（Phase 1 扩展） |
| proof | `include/lv00/proof.h` | 证明目标分解（Phase 2） |
| smt_backend | `include/lv00/smt_backend.h` | SMT 求解器调度（Phase 2） |
| debug | `include/lv00/debug.h` | E-ACSL 运行时检查集成（Phase 3） |

---

> **文档版本：** v1.0
> **创建日期：** 2026-05-24
> **适用范围：** Lv-00 形式化验证集成工作
> **关联文档：** [why3_multi_prover_dispatch.md](why3_multi_prover_dispatch.md)
