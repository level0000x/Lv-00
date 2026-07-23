> **状态**: 已归档 (2026-07-23)
> **原因**: 此文档描述的是 v3.3 时代的五层架构整改方案，当前项目已演进为十层架构 (v1.1.0+)。
> **替代**: 参考 `TEN_LAYER_OPTIMIZED_PLAN.md` (十层架构优化方案)

---

> **[历史文档]** 此文档为历史设计文档，当前架构已升级为十层。以下内容描述的是 v3.3 时代的五层架构整改计划，仅供参考。

# Lv-00 五层架构与学术化整改实施计划

> **执行前置说明**：本计划用于将 Lv-00 从现有 v3.3 五层工程架构，规整为用户指定的"元语言解析层 → 几何公理层 → 约束拓扑层 → 多策略推理层 → 输出证明编译层"学术化架构。实施应遵循 TDD、增量迁移、保持公开 API 兼容、每阶段可编译可测试的原则。

**目标**：完成 Lv-00 五层架构标准化拆分、元语言规范补全、约束归一化补证、多策略推理边界收束、代数内核分层和工程代码治理。

**架构原则**：保留极薄 `core/shared` 公共层承载错误码、基础类型、内存和诊断；业务五层保持单向依赖。禁止解析层调用推理层，禁止约束层生成输出证明，禁止证明输出层反向修改推理内核状态。

**技术栈**：C11、CMake OBJECT libraries、GMP、现有 Lv-00 C API、现有 `test/c` 测试体系、可选 `ENABLE_LAYER_VALIDATION` 编译期边界验证。

---

## 一、执行评估量规

| 维度 | 合格标准 | 验收方式 |
|---|---|---|
| 架构解耦 | 五层目录、CMake target、头文件职责与理论层级一致 | 开启 `ENABLE_LAYER_VALIDATION` 编译；检查跨层 include |
| 单向依赖 | 下层不包含上层头文件，不调用上层函数 | 静态 grep + 编译目标依赖检查 |
| 元语言完整性 | BNF 覆盖声明、约束、命题、判断、表达式、命令 | 文档审查 + parser 测试 |
| 类型安全 | 几何实体为一等公民，语句经类型检查后进入 IR | 类型系统单元测试 |
| 约束安全 | 能区分相容、矛盾、欠约束、过约束 | 约束图测试与退化用例测试 |
| 推理可靠性 | 八类推理策略有形式规则、作用域、边界和证明链 | 推理规则测试 + proof trace 审查 |
| 反证边界 | 反证假设局部化，禁止全局无界逻辑爆炸 | 反证/矛盾回归测试 |
| 代数严谨性 | 区分有理数域、二次代数数域、实数域 | 数域转换与 Groebner 后端测试 |
| 工程可维护性 | 大文件拆分、工具模块剥离、注释含数学原理 | 文件大小、模块职责、代码审查 |

---

## 二、目标五层目录方案

当前项目已经有 `core/src/parser`、`core/src/axiom`、`core/src/core`、`core/src/preset` 等目录，但大量模块仍集中在 `core/src/core`。目标结构建议如下：

```text
core/
  include/lv/
    shared/                  # 公共基础类型、错误、诊断、内存接口
    language/                # 词法、BNF、AST、Typed AST、IR
    axiom/                   # 几何本体、公理、基础度量关系
    constraint/              # 约束图、拓扑归一化、相容性检测
    reasoning/               # 多策略推理、调度器、代数后端、SMT/ATP
    proof/                   # 证明链、证明编译、导出与可视化接口
  src/
    shared/
    language/
    axiom/
    constraint/
    reasoning/
    proof/
```

### 2.1 层级映射

| 新层级 | 对应职责 | 现有主要文件迁移候选 |
|---|---|---|
| 1. 词法语法解析层 | BNF、lexer、parser、AST、表达式优先级、符号规约 | `formula_parser.c`、`lexer_shared.c`、`formula_converter.c`、`dsl_compiler.c`、`math_input.c`、`type_system.c` 的语法相关部分 |
| 2. 基础几何公理层 | 几何本体、点线圆、度量关系、固有公理库 | `euclidean_geometry.c`、`geometry_types.h`、`axiom_pkg.c`、`axiom_grade.c`、`preset_basic_geometry.c` |
| 3. 约束拓扑规约层 | 约束图、节点等价、冗余剔除、拓扑归一化 | `constraint_graph.c`、`normalization.c`、`geo_topology.c`、`symbolic_coord.c`、`geo_event_detect.c` |
| 4. 多策略自动推理层 | 正向/反向/反证/代数/Groebner/SMT/ATP/调度 | `engine.c`、`engine_scheduler.c`、`solver.c`、`groebner_engine.c`、`smt_backend_impl.c`、`proof_multi_strategy.c`、`logic_check.c`、`rewrite.c`、`unify.c` |
| 5. 输出证明编译层 | 命题格式化、证明链、溯源、TikZ、跨语言导出 | `proof_export_enhanced.c`、`proof_trace.c`、`proof_widget.c`、`tikz_export.c`、`stream.c`、`interop.c` |
| shared | 内存、错误、日志、配置、工具，不承载数学推理 | `memory_pool.c`、`error_codes.c`、`debug.c`、`context.c`、`lv_utils.c`、`runtime_monitor.c` |

---

## 三、分阶段执行路线

### 阶段 0：基线锁定与风险隔离

**目标**：在改动前固定当前行为，避免大规模迁移后无法定位退化。

- [ ] 运行现有 CMake 配置与测试，记录当前失败项。
- [ ] 开启或验证 `ENABLE_LAYER_VALIDATION`，记录当前跨层违规。
- [ ] 建立新增测试命名规则：`test_language_*`、`test_axiom_*`、`test_constraint_*`、`test_reasoning_*`、`test_proof_*`。
- [ ] 禁止一次性移动大文件；每次只移动一个职责子集。

### 阶段 1：CMake 与目录骨架规整

**目标**：先建立新五层目录与 OBJECT target，不立刻迁移所有实现。

建议新增 target：

```cmake
lv_shared
lv_layer1_language
lv_layer2_axiom
lv_layer3_constraint
lv_layer4_reasoning
lv_layer5_proof
```

迁移原则：

- `shared` 不依赖任何业务层。
- `language` 只能依赖 `shared`。
- `axiom` 只能依赖 `shared` 和语言层的稳定 IR 类型，不能调用 parser。
- `constraint` 依赖 `shared`、`axiom`，不能依赖 reasoning/proof。
- `reasoning` 依赖 `constraint`、`axiom`、`shared`。
- `proof` 依赖 `reasoning` 的只读 proof object，不回写 reasoning 内核。

### 阶段 2：元语言学术规范化

**目标**：补全完整 lv 元语言定义，形成可测试的 AST/Typed IR。

交付：

- [ ] `doc/docs/lv_LANGUAGE_SPEC.md`：BNF、语言属性、符号表、操作语义、指称语义。
- [ ] `core/include/lv/language/lv_ast.h`：AST 节点定义。
- [ ] `core/include/lv/language/lv_typed_ir.h`：Typed IR 定义。
- [ ] `test/c/test_language_bnf.c`：声明、约束、逻辑判断、表达式解析测试。

BNF 覆盖范围：

```bnf
Program        ::= Statement*
Statement      ::= Declaration | ConstraintStmt | AssertStmt | ProveStmt | LetStmt | ComputeStmt
Declaration    ::= EntityType Identifier ("," Identifier)* ";"
EntityType     ::= "Point" | "Line" | "Circle" | "Segment" | "Angle" | "Triangle" | "Scalar"
ConstraintStmt ::= "Constraint" ConstraintExpr ";"
AssertStmt     ::= "Assert" LogicExpr ";"
ProveStmt      ::= "Prove" LogicExpr ";"
LetStmt        ::= "Let" Identifier ":" Type "=" Expr ";"
ComputeStmt    ::= "Compute" Expr ";"
LogicExpr      ::= LogicExpr "->" LogicExpr | LogicExpr "<->" LogicExpr | LogicTerm
LogicTerm      ::= "forall" Binder "." LogicExpr | "exists" Binder "." LogicExpr | "not" LogicTerm | Predicate
Predicate      ::= Relation "(" ArgList? ")" | Expr CompareOp Expr
ConstraintExpr ::= Relation "(" ArgList? ")" | Expr ConstraintOp Expr
Expr           ::= Expr AddOp Term | Term
Term           ::= Term MulOp Factor | Factor
Factor         ::= Identifier | Number | MeasureExpr | "(" Expr ")"
```

### 阶段 3：基础几何公理层独立

**目标**：将几何实体、本体、度量关系、公理包从求解器和输出层中剥离。

交付：

- [ ] `core/include/lv/axiom/geometry_ontology.h`
- [ ] `core/include/lv/axiom/metric_relations.h`
- [ ] `core/include/lv/axiom/euclidean_axioms.h`
- [ ] `test/c/test_axiom_geometry_ontology.c`

数学边界：

- 点、线、圆、角、线段、三角形作为一等实体。
- 基础度量关系只定义语义，不执行推理搜索。
- 公理层只声明可用公理与前置条件，不生成证明文本。

### 阶段 4：约束拓扑规约层整改

**目标**：建立约束图归一化、安全相容检测和退化情况处理。

交付：

- [ ] 约束状态枚举：`CONSISTENT`、`INCONSISTENT`、`UNDER_CONSTRAINED`、`OVER_CONSTRAINED`。
- [ ] 拓扑约束合并算法的收敛性、终止性、冗余剔除正确性说明。
- [ ] 退化测试：共点、重合点、平行线、重合线、零长度线段、相切圆。

核心不变量：

```text
Normalize(Normalize(G)) = Normalize(G)
Equivalent(G1, G2) => Normalize(G1) = Normalize(G2)
RemoveRedundant(C, G) 不改变 G 的可满足模型集合
```

### 阶段 5：多策略自动推理层形式化

**目标**：八类推理策略有统一接口、优先级和边界。

统一接口建议：

```c
typedef enum {
    lv_REASON_FORWARD_DEDUCTION,
    lv_REASON_GEOMETRIC_PROPERTY,
    lv_REASON_ALGEBRAIC_SIMPLIFY,
    lv_REASON_GROEBNER_ELIMINATION,
    lv_REASON_BOOLEAN_DECOMPOSITION,
    lv_REASON_REDUCTIO_AD_ABSURDUM,
    lv_REASON_CONTRADICTION_CLOSURE,
    lv_REASON_SMT_MODAL_CHECK
} lvReasoningStrategy;
```

每种策略必须记录：

```text
适用条件
输入约束状态
推理规则
逻辑范式
假设作用域
使用边界
容错范围
证明链节点
可信度等级
```

调度优先级建议：

1. 类型与语法合法性
2. 公理直接匹配
3. 约束归一化推出
4. 几何性质推演
5. 布尔逻辑拆解
6. 代数坐标化简
7. Groebner 消元
8. SMT/ATP 校验
9. 局部反证闭包

### 阶段 6：反证法与矛盾推演漏洞修复

**目标**：将原"全域逻辑爆炸"收束为"局部矛盾闭包溯源"。

必须实现的安全规则：

- 反证假设必须进入假设栈。
- 矛盾只能在当前假设域内传播。
- `P ∧ ¬P ⊢ Q` 不得污染全局公理库。
- 每个矛盾结论必须带来源路径。
- 证明结束后回收临时假设。
- 未闭合假设域下的结论标记为条件性结论。

建议将用户可见术语从"全域逻辑爆炸"改为"局部矛盾闭包溯源"。

### 阶段 7：代数计算内核优化

**目标**：三层数域和多项式系统统一封装。

交付：

- [ ] 有理数域 `Q`：基于 GMP/rational。
- [ ] 二次代数数域：根式、最小多项式、隔离区间。
- [ ] 实数域 `R`：仅作为语义域和 SMT/区间验证后端，不用浮点作为证明事实。
- [ ] 多项式系统：几何条件统一转方程组。
- [ ] Groebner 后端加预算、超时、项数上限和基化简次数限制。

### 阶段 8：输出证明编译层

**目标**：证明对象和输出格式分离。

交付：

- [ ] Proof Object：机器可复核的证明链。
- [ ] Proof Trace：逻辑溯源存档。
- [ ] Proof Compiler：自然语言、LaTeX、JSON、TikZ、跨语言导出。
- [ ] 输出层只读，不改变推理层状态。

---

## 四、首批建议执行任务

### Task 1：更新架构文档与 CMake 层命名

**文件：**

- 修改：`doc/docs/ARCHITECTURE_v3.3.md`
- 修改：`CMakeLists.txt`
- 新增：`doc/docs/lv_LANGUAGE_SPEC.md`

**步骤：**

- [ ] 将旧五层"Parser/Resource/Geometry/Reasoning/Output"改为新五层"Language/Axiom/Constraint/Reasoning/Proof Output"。
- [ ] 保留 `shared` 作为非业务公共层，明确其不是第 2 层业务架构。
- [ ] CMake 中先新增注释与分组变量，不一次性移动所有源文件。
- [ ] 运行 CMake 配置验证。

### Task 2：先补语言规格，不改 parser 主逻辑

**文件：**

- 新增：`doc/docs/lv_LANGUAGE_SPEC.md`
- 测试：`test/c/test_minimal_parse.c` 或新增 `test/c/test_language_bnf.c`

**步骤：**

- [ ] 写 BNF。
- [ ] 写符号表。
- [ ] 写操作语义和指称语义。
- [ ] 为已有 parser 添加最小解析覆盖测试。

### Task 3：约束状态枚举与相容检测接口

**文件：**

- 修改：`core/include/lv/constraint_graph.h`
- 修改：`core/src/core/constraint_graph.c`
- 测试：`test/c/test_constraint_compatibility.c`

**步骤：**

- [ ] 先写失败测试：空图欠约束、重复等价约束相容、明显矛盾约束矛盾。
- [ ] 新增 `lvConstraintStatus` 枚举。
- [ ] 实现最小 `constraint_graph_check_compatibility`。
- [ ] 通过测试后再扩展退化情况。

### Task 4：反证作用域测试与接口收束

**文件：**

- 修改：`core/include/lv/proof.h`
- 修改：`core/src/core/proof.c`
- 测试：`test/c/test_proof_contradiction_scope.c`

**步骤：**

- [ ] 先写失败测试：局部矛盾不得推出全局任意命题。
- [ ] 增加假设域 ID 或 proof scope 字段。
- [ ] 将 ex falso 证明步骤绑定到 scope。
- [ ] 输出 proof trace 中的矛盾来源。

---

## 五、实施注意事项

1. 不建议现在一次性移动 `solver.c`、`proof.c`、`rewrite.c` 等大文件；应先拆接口，再迁移实现。
2. 所有行为变化必须先写测试，再改实现。
3. 文档、目录、CMake 可以先行；推理语义和内核逻辑必须分阶段落地。
4. 对外 API 保持兼容，新增 API 优先使用 `lv_` 前缀，旧 API 用适配层保留。
5. "逻辑爆炸"不再作为无界机制公开，改成局部矛盾闭包。
6. Groebner 与 SMT 是后端验证器，不应成为所有推理的默认第一路径。

---

## 六、当前仓库初查结论

- 已存在 `doc/docs/ARCHITECTURE_v3.3.md`，但其第 2 层是资源管理层，与本次用户提出的"基础几何公理层"不一致。
- 已存在 CMake 五层 OBJECT 库，但层级命名与目标学术五层不完全一致。
- 大量实现集中在 `core/src/core`，需要分批迁移到 `language/axiom/constraint/reasoning/proof/shared`。
- 已存在 `doc/docs/REFACTORING_PLAN_v3.4.1.md`，主要关注大文件拆分；本计划在其之上增加理论架构、语义规范和推理安全边界。
- `proof.h` 中仍公开描述"爆炸原理：从矛盾推导任意命题"，需要改为带作用域的局部机制。

---

## 七、下一步执行顺序

建议立即执行：

1. 先新增语言规范文档 `lv_LANGUAGE_SPEC.md`。
2. 更新架构文档，使五层定义与本次目标一致。
3. 新增约束相容状态接口测试。
4. 新增反证作用域测试。
5. 再按测试结果改 `constraint_graph` 和 `proof` 实现。
