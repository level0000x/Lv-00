# 约束相容检测与反证作用域测试落点规划

> **目的**：为十层架构整改中的两个关键安全点建立测试先行方案：约束相容检测、反证法作用域收束。该文档用于指导后续 C 接口和实现调整，避免无测试重构。

---

## 0. 执行评估量规

| 维度 | 合格标准 | 验收方式 |
|---|---|---|
| 测试先行 | 每个新增行为先有失败测试 | 新增测试文件可单独运行 |
| 四态完整 | 相容、矛盾、欠约束、过约束均有测试 | `test_constraint_compatibility` 覆盖 |
| 反证安全 | 局部矛盾不污染全局证明上下文 | `test_proof_contradiction_scope` 覆盖 |
| 接口稳定 | 新增接口不破坏现有测试 | 全量 CTest |
| 十层边界 | 约束测试不依赖 proof 输出，proof 测试只读约束状态 | include 与链接检查 |

---

## 1. 测试文件落点

新增测试建议放在现有 `test/c` 目录：

```text
test/c/test_constraint_compatibility.c
test/c/test_proof_contradiction_scope.c
```

CMake 注册点：

```cmake
add_lv00_test_and_register(test_constraint_compatibility
    test/c/test_constraint_compatibility.c constraint_compatibility_test)

add_lv00_test_and_register(test_proof_contradiction_scope
    test/c/test_proof_contradiction_scope.c proof_contradiction_scope_test)
```

> 注意：当前仓库实际测试目录为 `test/c`，不是 `tests`。后续新增文件必须放入 `test/c`。

---

## 2. 约束相容检测接口规划

### 2.1 新增枚举

建议在 `core/include/lv00/constraint_graph.h` 中新增：

```c
typedef enum Lv00ConstraintStatus {
    LV00_CONSTRAINT_STATUS_CONSISTENT = 0,
    LV00_CONSTRAINT_STATUS_INCONSISTENT = 1,
    LV00_CONSTRAINT_STATUS_UNDER_CONSTRAINED = 2,
    LV00_CONSTRAINT_STATUS_OVER_CONSTRAINED = 3,
    LV00_CONSTRAINT_STATUS_INVALID = 4
} Lv00ConstraintStatus;
```

### 2.2 新增结果结构

```c
typedef struct Lv00ConstraintCompatibilityResult {
    Lv00ConstraintStatus status;
    int conflicting_constraint_id;
    int redundant_constraint_count;
    int free_degree_count;
    const char *diagnostic;
} Lv00ConstraintCompatibilityResult;
```

### 2.3 新增函数

```c
bool graph_check_compatibility(
    const ConstraintGraph *graph,
    Lv00ConstraintCompatibilityResult *out_result
);
```

设计要求：

- `graph == NULL` 返回 `false`，`status = LV00_CONSTRAINT_STATUS_INVALID`。
- 空图或只有孤立点：`UNDER_CONSTRAINED`。
- 无矛盾且约束足够：`CONSISTENT`。
- 存在直接冲突：`INCONSISTENT`。
- 重复或可由其他约束推出的约束过多：`OVER_CONSTRAINED`。

---

## 3. 约束相容检测测试用例

### 3.1 空图欠约束

行为：空约束图没有足够事实，不能被误判为严格相容证明上下文。

```c
static void test_empty_graph_is_under_constrained(void) {
    ConstraintGraph *g = graph_create();
    assert(g != NULL);

    Lv00ConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(g, &result);

    assert(ok);
    assert(result.status == LV00_CONSTRAINT_STATUS_UNDER_CONSTRAINED);

    graph_free(g);
}
```

### 3.2 单线段基础相容

行为：两个不同点构成线段时，约束系统基础相容。

```c
static void test_single_segment_is_consistent(void) {
    ConstraintGraph *g = graph_create();
    assert(g != NULL);

    int a = add_point(g, 0, 1, 0, 1);
    int b = add_point(g, 1, 1, 0, 1);
    assert(a >= 0);
    assert(b >= 0);
    assert(graph_add_line_segment(g, a, b) >= 0);

    Lv00ConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(g, &result);

    assert(ok);
    assert(result.status == LV00_CONSTRAINT_STATUS_CONSISTENT);

    graph_free(g);
}
```

### 3.3 重合点构造直线矛盾或退化

行为：`line(A,A)` 不得静默进入普通相容状态。

```c
static void test_degenerate_line_from_same_point_is_not_consistent(void) {
    ConstraintGraph *g = graph_create();
    assert(g != NULL);

    int a = add_point(g, 0, 1, 0, 1);
    assert(a >= 0);
    int line = graph_add_line_segment(g, a, a);
    (void)line;

    Lv00ConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(g, &result);

    assert(ok);
    assert(result.status == LV00_CONSTRAINT_STATUS_INCONSISTENT ||
           result.status == LV00_CONSTRAINT_STATUS_UNDER_CONSTRAINED);

    graph_free(g);
}
```

### 3.4 重复约束过约束

行为：同一约束重复加入，应被标记为过约束或产生冗余计数。

```c
static void test_duplicate_segment_constraint_is_over_constrained_or_redundant(void) {
    ConstraintGraph *g = graph_create();
    assert(g != NULL);

    int a = add_point(g, 0, 1, 0, 1);
    int b = add_point(g, 1, 1, 0, 1);
    assert(graph_add_line_segment(g, a, b) >= 0);
    assert(graph_add_line_segment(g, a, b) >= 0);

    Lv00ConstraintCompatibilityResult result;
    bool ok = graph_check_compatibility(g, &result);

    assert(ok);
    assert(result.status == LV00_CONSTRAINT_STATUS_OVER_CONSTRAINED ||
           result.redundant_constraint_count > 0);

    graph_free(g);
}
```

---

## 4. 反证作用域接口规划

现有 `proof.h` 中存在“爆炸原理：从矛盾推导任意命题”的公开接口或描述。整改方向不是删除反证法，而是将其限制在**局部假设域**。

### 4.1 新增作用域类型

建议在 proof 相关头文件中新增：

```c
typedef int Lv00ProofScopeId;

#define LV00_PROOF_SCOPE_GLOBAL 0
#define LV00_PROOF_SCOPE_INVALID -1
```

### 4.2 新增作用域 API

```c
Lv00ProofScopeId proof_begin_assumption_scope(ProofNavigator *nav,
                                               const Proposition *assumption);

bool proof_close_assumption_scope(ProofNavigator *nav,
                                  Lv00ProofScopeId scope_id);

bool proof_apply_ex_falso_scoped(ProofNavigator *nav,
                                 const ConstraintGraph *contradiction_proof,
                                 const Proposition *target,
                                 Lv00ProofScopeId scope_id);
```

保留旧 `proof_apply_ex_falso` 时，应作为兼容包装，但必须默认拒绝无作用域全局爆炸，或仅允许在显式全局矛盾证明存在时使用。

---

## 5. 反证作用域测试用例

### 5.1 局部矛盾不得推出全局任意命题

```c
static void test_local_contradiction_does_not_prove_global_target(void) {
    ProofNavigator *nav = proof_navigator_create();
    assert(nav != NULL);

    Proposition *assumption = proposition_create(100, PROPOSITION_TYPE_ATOMIC);
    Proposition *target = proposition_create(200, PROPOSITION_TYPE_ATOMIC);
    assert(assumption != NULL);
    assert(target != NULL);

    Lv00ProofScopeId scope = proof_begin_assumption_scope(nav, assumption);
    assert(scope != LV00_PROOF_SCOPE_INVALID);

    ConstraintGraph *local_bottom = graph_create();
    assert(local_bottom != NULL);

    bool ok = proof_apply_ex_falso_scoped(nav, local_bottom, target, scope);
    assert(ok);

    bool global_ok = proof_has_global_proposition(nav, target);
    assert(!global_ok);

    graph_free(local_bottom);
    proposition_free(target);
    proposition_free(assumption);
    proof_navigator_free(nav);
}
```

### 5.2 作用域关闭后临时假设被回收

```c
static void test_closed_assumption_scope_releases_temporary_assumption(void) {
    ProofNavigator *nav = proof_navigator_create();
    assert(nav != NULL);

    Proposition *assumption = proposition_create(101, PROPOSITION_TYPE_ATOMIC);
    assert(assumption != NULL);

    Lv00ProofScopeId scope = proof_begin_assumption_scope(nav, assumption);
    assert(scope != LV00_PROOF_SCOPE_INVALID);

    bool closed = proof_close_assumption_scope(nav, scope);
    assert(closed);

    bool still_active = proof_scope_is_active(nav, scope);
    assert(!still_active);

    proposition_free(assumption);
    proof_navigator_free(nav);
}
```

### 5.3 无效作用域不得应用爆炸原理

```c
static void test_ex_falso_rejects_invalid_scope(void) {
    ProofNavigator *nav = proof_navigator_create();
    assert(nav != NULL);

    Proposition *target = proposition_create(300, PROPOSITION_TYPE_ATOMIC);
    ConstraintGraph *bottom = graph_create();
    assert(target != NULL);
    assert(bottom != NULL);

    bool ok = proof_apply_ex_falso_scoped(nav, bottom, target, LV00_PROOF_SCOPE_INVALID);
    assert(!ok);

    graph_free(bottom);
    proposition_free(target);
    proof_navigator_free(nav);
}
```

---

## 6. 实现顺序

1. 先新增 `test/c/test_constraint_compatibility.c`，引用预期接口，确认编译失败。
2. 在 `constraint_graph.h` 添加枚举、结果结构和函数声明。
3. 在 `constraint_graph.c` 添加最小实现，使空图、单线段、退化线段、重复线段测试通过。
4. 注册 CMake 测试并运行。
5. 新增 `test/c/test_proof_contradiction_scope.c`，引用预期接口，确认编译失败。
6. 在 `proof.h` 添加作用域类型和函数声明。
7. 在 `proof.c` 添加最小作用域栈实现。
8. 将旧 `proof_apply_ex_falso` 限制为兼容包装，不再无界污染全局证明。
9. 全量运行测试。

---

## 7. 数学正确性说明草案

### 7.1 约束相容性

约束集合 `C` 的相容性定义为：

```text
Consistent(C) ⇔ ∃M. M ⊨ C
Inconsistent(C) ⇔ ¬∃M. M ⊨ C
UnderConstrained(C) ⇔ ∃M1,M2. M1 ⊨ C ∧ M2 ⊨ C ∧ Target(M1) ≠ Target(M2)
OverConstrained(C) ⇔ ∃c∈C. C\{c} ⊨ c 或 c 与 C 中其他约束重复表达同一事实
```

### 7.2 局部矛盾闭包

在假设域 `S` 内，若：

```text
Γ, A_S ⊢ ⊥
```

则只能推出：

```text
Γ ⊢ ¬A_S
```

或在 `S` 内标记任意命题为条件性结论，不得推出：

```text
Γ ⊢ Q
```

除非 `⊥` 是在全局上下文 `Γ` 内无假设地成立。

---

## 8. 风险与兼容策略

| 风险 | 策略 |
|---|---|
| 旧测试依赖无界 ex falso | 保留旧函数，但在文档中标记 deprecated，并新增 scoped API |
| 约束图内部结构不足以判断所有四态 | 首批只实现基础直接判定，复杂情况返回欠约束并附诊断 |
| CMake 一次性改动过大 | 只新增两个测试注册，不移动旧 target |
| 退化几何判断分散 | 先在 constraint 层建立接口，再逐步迁移具体判断 |

---

## 9. 验收命令

```bash
cmake -S . -B build_verify_tests -DBUILD_TESTS=ON -DENABLE_LAYER_VALIDATION=ON
cmake --build build_verify_tests
ctest --test-dir build_verify_tests --output-on-failure -R "constraint_compatibility|proof_contradiction_scope"
```

通过条件：

- 两个新增测试可编译并通过；
- 旧 `proof_test` 不因接口新增而破坏；
- `constraint_graph` 不依赖 proof/output 层；
- `proof` 不通过输出层回写约束图。
