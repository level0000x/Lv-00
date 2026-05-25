# mai 极简设计理念文档

> **借鉴项目**：mai（github.com/xieyuheng/mai）
> **核心借鉴点**："推理规则即代码"哲学、用推理规则表替代条件分支、Prolog 回溯搜索在约束求解中的应用
> **分类**：P4 低优先级 / 架构哲学
> **日期**：2026-05-24

---

## 1. 概述

mai 是一个极简的定理证明器，其核心理念是"推理规则即代码"——将逻辑推理规则直接表达为可执行代码，消除传统证明器中"证明脚本"和"推理引擎"之间的鸿沟。这一哲学与 Lv-00 的"构造=计算=证明"三位一体理念高度共鸣。本文档探讨如何将 mai 的极简设计哲学应用于 Lv-00 内核的精简和约束求解器的重构。

---

## 2. "推理规则即代码"哲学

### 2.1 mai 的核心思想

在 mai 中，一个推理规则不是元语言中的声明，而是第一等的可执行函数：

```
// mai 风格的推理规则：自然演绎的蕴含引入
rule implication_intro(A, B, proof_A_to_B) {
    // proof_A_to_B 是一个从 A 推出 B 的函数
    // 此规则产生 A -> B 的证明
    return (ctx) => proof_A_to_B(ctx.with(A));
}
```

每条推理规则是一个纯函数，接受前提证明并返回结论证明。整个证明过程是推理规则函数的组合调用。

### 2.2 Lv-00 中的应用

在 Lv-00 中，这种哲学体现在三个层面：

**层面一：重写规则引擎**。`rewrite.h/c` 中的每条重写规则已经接近"推理规则即代码"的理念——每条规则包含匹配模式、替换模式和前置条件，且作为一等对象可被组合和调度（`rewrite.h` 第 6.1-6.3 节）。

**层面二：函数块作为推理规则**。每个 `FUNCTION_BLOCK` 节点本质上是一个"推理规则"——给定输入几何构造，产生输出几何构造。打包函数块等同于定义新的推理规则。

**层面三（目标）**：将 Lv-00 内核中的条件分支逻辑提炼为显式的规则表，使代码路径可审计、可验证、可组合。

---

## 3. 用推理规则表替代条件分支

### 3.1 当前情况

Lv-00 内核中散布着条件分支逻辑。例如，`solver.c` 中求解约束时：

```c
// 当前风格（示意）
if (constraint.degree() <= 1) {
    solve_linear(...);
} else if (constraint.degree() == 2) {
    solve_quadratic(...);
} else if (constraint.type == CONSTRAINT_INCIDENCE) {
    solve_incidence(...);
} else {
    mark_out_of_range(...);
}
```

这种方式有若干缺陷：
- 新增求解策略需要修改多个 if-else 链
- 求解策略之间的优先级不可配置
- 测试覆盖率难以保证

### 3.2 规则表重构

借鉴 mai，将条件分支替换为求解策略规则表：

```c
/**
 * @brief 求解策略规则条目
 *
 * 每条规则声明它适用的条件（度数范围、约束类型）、
 * 求解函数和执行成功/失败的后续动作。
 */
typedef struct SolverRule {
    int min_degree;                    // 适用多项式的最低次数
    int max_degree;                    // 适用多项式的最高次数
    ConstraintType applicable_type;    // 适用约束类型（CONSTRAINT_NONE = 全部）
    int (*solve)(ConstraintGraph*, int constraint_id, SymbolicCoord** out, int* out_count);
    const char *rule_name;             // 规则名称（用于日志和调试）
    int priority;                      // 优先级（数值越小越高）
} SolverRule;

/**
 * @brief 求解器规则表
 *
 * 所有求解策略注册于此表中。求解器按优先级依次尝试，
 * 第一个适用的策略被执行。
 */
static SolverRule solver_rules[] = {
    { 0, 0, CONSTRAINT_INCIDENCE,  solve_incidence_coords,  "incidence-zero",  10 },
    { 1, 1, CONSTRAINT_NONE,       solve_linear_system,     "linear-generic",  20 },
    { 2, 2, CONSTRAINT_NONE,       solve_quadratic_system,  "quadratic-generic",30 },
    { 0, 2, CONSTRAINT_INTERSECT,  solve_parametric_geom,   "intersect-param", 25 },
    { 3, 999, CONSTRAINT_NONE,     delegate_to_cas_backend, "delegate-cas",   100 },
};
```

求解器不再包含条件分支，仅遍历规则表：

```c
int solver_apply_rules(ConstraintGraph *graph, int constraint_id,
                        SymbolicCoord **out, int *out_count) {
    for (int i = 0; i < RULE_COUNT(solver_rules); i++) {
        SolverRule *rule = &solver_rules[i];
        if (constraint_degree_in_range(constraint_id, rule->min_degree, rule->max_degree)
            && (rule->applicable_type == CONSTRAINT_NONE
                || get_constraint_type(constraint_id) == rule->applicable_type)) {
            int result = rule->solve(graph, constraint_id, out, out_count);
            if (result == 0) return 0;   // 成功
            // 失败 → 尝试下一个规则
        }
    }
    return -1;  // 所有规则均失败
}
```

### 3.3 收益

- **可扩展性**：新增求解策略只需在规则表中添加一行
- **可测试性**：每条规则可独立单元测试
- **可审计性**：规则表是显式文档——一眼可见所有求解路径
- **可组合性**：规则表支持运行时热加载和优先级调整

---

## 4. Prolog 回溯搜索在约束求解中的应用

### 4.1 mai 的 Prolog 式反向链推理

mai 的推理核心是一个类 Prolog 的反向链引擎。给定目标命题，引擎尝试将推理规则反向应用，将目标分解为子目标，直到所有子目标都是已知公理。搜索过程中使用回溯处理分支选择。

### 4.2 Lv-00 约束求解中的回溯场景

Lv-00 的约束求解中已经有隐式的回溯需求（但目前未系统实现）：

1. **多解选择**：圆与直线相交有两个交点，需要回溯尝试两种选择
2. **辅助构造搜索**：证明过程中需要尝试不同的辅助线，失败的辅助线需要回溯
3. **重写路径探索**：类型等价检查时，多条重写路径的选择（`proof.h` 第 451-453 行的交互式路径探索器）

### 4.3 统一回溯框架设计

借鉴 mai 的 Prolog 风格，设计统一回溯约束求解框架：

```c
/**
 * @brief 回溯求解状态
 *
 * 借鉴 Prolog 的 choice-point 机制。
 * 每个 choice_point 记录当前求解状态和所有未尝试的选择。
 */
typedef struct BacktrackState {
    ConstraintGraph *snapshot;        // 当前约束图快照
    int *untried_choices;             // 未尝试的分支选择索引
    int untried_count;
    int current_choice;               // 当前正在尝试的选择
    struct BacktrackState *parent;    // 父状态（用于回溯）
} BacktrackState;

/**
 * @brief 回溯求解器主循环
 *
 * 借鉴 Prolog 的深度优先搜索 + 回溯：
 * 1. 尝试当前选择分支
 * 2. 如果成功 → 继续下一个子目标
 * 3. 如果失败 → 回溯到最近的 choice_point，尝试下一个分支
 * 4. 如果所有分支都失败 → 回溯到更早的 choice_point
 *
 * @param graph      约束图
 * @param goal       目标约束（要满足的条件）
 * @param max_steps  最大搜索步数（防止无限搜索）
 * @return true 找到解，false 所有路径均失败
 */
bool backtrack_solve(ConstraintGraph *graph, Constraint *goal, int max_steps);
```

### 4.4 与现有求解器的集成

回溯求解器不是替代现有的 Groebner 基求解器，而是在其之上的元层控制：

```
回溯层（BacktrackSolve）    ← 新增，处理分支选择和辅助构造搜索
    │
    ├─ 调用 Groebner 基求解器 ← 已有，处理确定的代数约束系统
    ├─ 调用 CAS 后端          ← 新增（cas_backend_design.md）
    └─ 调用 SMT 后端          ← 已有（smt_backend.h）
```

当 Groebner 基求解返回**多解信号**（而非唯一解）时，回溯层自动创建 choice-point，依次尝试每个解分支。这与 Prolog 中的"; "操作符行为一致。

---

## 5. 内核精简路线图

基于 mai 哲学的内核精简，建议以下分阶段路线：

1. **阶段一（P4-1）**：在 `solver.c` 中引入 `SolverRule` 规则表，替代主要的 if-else 链
2. **阶段二（P4-2）**：实现 `backtrack_solve()` 框架，用于辅助构造搜索
3. **阶段三（P4-3）**：将重写引擎（`rewrite.c`）的规则匹配循环重构为规则表驱动
4. **阶段四（远期）**：探索将 Lv-00 内核全部控制流表达为规则表——实现"内核=规则解释器+规则表的极简架构

---

## 6. 总结

mai 的"推理规则即代码"哲学为 Lv-00 内核的精简提供了清晰的指导方向：将隐式的条件分支显式化为可审计的规则表，将启发式搜索统一为 Prolog 风格的回溯框架。这一方向不仅减少了代码中的隐式逻辑路径，提高了可测试性，还为未来的内核冻结（自举阶段，`design_v2.9.md` 第 17.7 节）奠定了基础——一个规则表驱动的内核更易于在冻结后进行安全的形式化验证。
