# Lv-00 五层架构规范 v3.4-academic

> **项目**：Lv-00 几何元语言系统  
> **架构版本**：v3.4-academic  
> **最后更新**：2026-05-25  
> **适用范围**：所有 C 源文件、头文件、测试、文档与构建目标  
> **整改目标**：将原 v3.3 工程五层规整为“词法语法解析层 → 基础几何公理层 → 约束拓扑规约层 → 多策略自动推理层 → 输出证明编译层”的学术化五层单向依赖架构。

---

## 0. 执行评估量规

| 维度 | 合格标准 | 验收方式 |
|---|---|---|
| 五层一致性 | 目录、CMake target、文档和数学架构一一对应 | 架构审查 |
| 单向依赖 | 下层不 include 上层头文件，不调用上层符号 | `ENABLE_LAYER_VALIDATION` + 静态检查 |
| 共享层克制 | shared 只含基础类型、错误、内存、诊断，不含数学推理 | 代码审查 |
| 语义输入稳定 | 解析层只输出 AST/Typed IR | parser 与 IR 测试 |
| 推理边界清晰 | 约束层不证明，推理层不格式化输出 | 模块依赖检查 |
| 证明可追溯 | 输出层只读 Proof Object，证明链可溯源 | proof trace 测试 |

---

## 1. 架构概览

Lv-00 采用严格的**五层单向依赖学术架构**。五层是业务数学架构；另设一个极薄 `shared` 公共基础层，用于承载所有层都可能需要的基础设施。

```text
┌──────────────────────────────────────────────────────────────┐
│ 第5层：输出证明编译层 Proof Output Compilation Layer          │
│ 命题格式化、证明链生成、逻辑溯源存档、可视化转换、跨语言导出   │
├──────────────────────────────────────────────────────────────┤
│ 第4层：多策略自动推理层 Multi-strategy Reasoning Layer        │
│ 正向演绎、反向溯源、反证、代数消元、Groebner、SMT/ATP 调度     │
├──────────────────────────────────────────────────────────────┤
│ 第3层：约束拓扑规约层 Constraint Topology Normalization Layer │
│ 约束图构建、等价节点化简、冗余剔除、拓扑归一化、相容检测       │
├──────────────────────────────────────────────────────────────┤
│ 第2层：基础几何公理层 Foundational Geometry Axiom Layer       │
│ 原始几何本体、基础度量关系、固有公理库、退化条件声明           │
├──────────────────────────────────────────────────────────────┤
│ 第1层：词法语法解析层 Lexical & Syntax Parsing Layer          │
│ BNF 文法、词法规则、符号规约、表达式优先级、AST/Typed IR       │
└──────────────────────────────────────────────────────────────┘

shared：错误码、基础类型、source span、内存、日志、诊断、配置。
```

### 1.1 核心理念

- **单向依赖**：上层可以读取下层产物，下层绝不反向依赖上层。
- **语义分离**：解析不推理，公理不求解，约束不证明，推理不输出，输出不回写。
- **契约驱动**：层间通过 AST、Typed IR、Constraint System、Proof Object 等稳定对象通信。
- **最小共享**：`shared` 不是业务层，不承载几何规则、约束算法或证明逻辑。

---

## 2. 层级定义

### 2.1 第1层：词法语法解析层

**职责**：将 Lv00 源文本转为 Token、AST、Typed AST 和 Geometric IR。

| 功能模块 | 描述 |
|---|---|
| `lexer` | 词法分析、Token 生成、源码位置保留 |
| `parser` | 按 BNF 构建 AST |
| `symbol_table` | 名称绑定、作用域、重复声明检查 |
| `type_checker` | 几何实体、度量值、命题、证明对象类型检查 |
| `operator_precedence` | 表达式优先级和结合性规约 |
| `typed_ir` | 输出稳定 Typed IR 供下游层消费 |

**允许依赖**：`shared`。  
**禁止依赖**：基础几何公理层、约束拓扑层、推理层、输出层。

**关键数据结构**：

```text
Lv00Token
Lv00SourceSpan
Lv00AstNode
Lv00TypedAst
Lv00TypedIR
Lv00ParseDiagnostic
```

**现有迁移候选**：

```text
core/src/parser/formula_parser.c
core/src/parser/lexer_shared.c
core/src/parser/formula_converter.c
core/src/core/dsl_compiler.c
core/src/core/math_input.c
```

---

### 2.2 第2层：基础几何公理层

**职责**：定义几何实体、本体、基础度量关系、固有公理库和退化条件。

| 功能模块 | 描述 |
|---|---|
| `geometry_ontology` | Point、Line、Circle、Segment、Angle、Triangle 等几何实体 |
| `metric_relations` | 长度、距离、角度、面积、半径等基础度量 |
| `primitive_axioms` | 欧氏几何基础公理、隶属关系、公理前置条件 |
| `degeneracy_conditions` | 重合点、共线、零长度、半径为零等退化声明 |
| `axiom_package` | 公理包加载、校验、版本管理 |

**允许依赖**：`shared`，以及第1层输出的稳定 Typed IR 类型定义。  
**禁止依赖**：parser 实现、约束归一化、求解器、证明输出。

**关键数据结构**：

```text
Lv00GeometryEntity
Lv00MetricRelation
Lv00AxiomRule
Lv00AxiomPackage
Lv00DegeneracyCondition
```

**现有迁移候选**：

```text
core/src/core/euclidean_geometry.c
core/include/lv00/geometry_types.h
core/src/axiom/axiom_pkg.c
core/src/axiom/axiom_grade.c
core/src/preset/preset_basic_geometry.c
```

---

### 2.3 第3层：约束拓扑规约层

**职责**：将几何事实与公理关系组织为约束系统，执行拓扑归一化、等价类合并、冗余剔除和相容性检测。

| 功能模块 | 描述 |
|---|---|
| `constraint_graph` | 约束图节点、边、索引与基础操作 |
| `equivalence_merge` | 等价节点合并、并查集/规范代表元 |
| `redundancy_elimination` | 冗余约束检测与剔除 |
| `topology_normalization` | 图结构规范化、幂等归一化 |
| `compatibility_check` | 相容、矛盾、欠约束、过约束四态检测 |
| `degenerate_geometry` | 共点、平行线退化、重合线、点圆等特殊情形判定 |

**允许依赖**：`shared`、基础几何公理层。  
**禁止依赖**：多策略推理层、输出证明编译层。

**关键数据结构**：

```text
Lv00ConstraintGraph
Lv00Constraint
Lv00ConstraintStatus
Lv00NormalizationResult
Lv00TopologyClass
```

**四态约束状态**：

```text
LV00_CONSTRAINT_CONSISTENT
LV00_CONSTRAINT_INCONSISTENT
LV00_CONSTRAINT_UNDER_CONSTRAINED
LV00_CONSTRAINT_OVER_CONSTRAINED
```

**现有迁移候选**：

```text
core/src/core/constraint_graph.c
core/src/core/normalization.c
core/src/core/geo_topology.c
core/src/core/symbolic_coord.c
core/src/core/geo_event_detect.c
```

---

### 2.4 第4层：多策略自动推理层

**职责**：基于约束系统、公理库和证明目标执行多策略自动推理，并生成机器可复核 Proof Object。

| 推理策略 | 职责 |
|---|---|
| 正向演绎推理 | 从已知事实和公理推出新事实 |
| 几何性质推演 | 使用几何定理和基础关系推导 |
| 代数坐标化简 | 将几何条件转为代数表达式并化简 |
| Groebner 多项式消元 | 对多项式方程组执行消元验证 |
| 布尔逻辑拆解 | 分解合取、析取、蕴含、否定等逻辑结构 |
| 反证归谬法 | 在局部假设域内推出矛盾 |
| 局部矛盾闭包溯源 | 追踪矛盾来源，禁止全局无界爆炸 |
| SMT 模态校验 | 使用 SMT/ATP/SAT/BDD 后端辅助验证 |

**允许依赖**：`shared`、基础几何公理层、约束拓扑规约层。  
**禁止依赖**：输出证明编译层。

**关键数据结构**：

```text
Lv00ReasoningContext
Lv00ReasoningStrategy
Lv00ReasoningScheduler
Lv00ProofObject
Lv00ProofStep
Lv00AssumptionScope
Lv00ContradictionTrace
```

**调度原则**：

1. 类型/语义合法性优先；
2. 公理直接匹配优先于高成本代数后端；
3. 约束归一化优先于搜索；
4. Groebner 与 SMT 作为后端验证器，不作为所有问题默认第一路径；
5. 反证法必须绑定假设作用域。

**现有迁移候选**：

```text
core/src/core/engine.c
core/src/core/engine_scheduler.c
core/src/core/solver.c
core/src/core/solver_core.c
core/src/core/groebner_engine.c
core/src/core/smt_backend_impl.c
core/src/core/atp_backend.c
core/src/core/sat_encoding.c
core/src/core/bdd_encoding.c
core/src/core/rewrite.c
core/src/core/unify.c
core/src/core/logic_check.c
core/src/core/proof_multi_strategy.c
```

---

### 2.5 第5层：输出证明编译层

**职责**：将 Proof Object 编译为人类可读或机器可处理的输出，不参与推理，不修改内核状态。

| 功能模块 | 描述 |
|---|---|
| `proof_formatting` | 命题、步骤、规则的自然语言格式化 |
| `proof_trace_archive` | 逻辑溯源存档、来源引用、假设域记录 |
| `visualization` | 证明树、约束图、几何图可视化 |
| `cross_language_export` | Lean、Coq、LaTeX、JSON、TikZ 等导出 |
| `stream_output` | 流式事件和增量证明输出 |
| `error_reporting` | 面向用户的诊断输出 |

**允许依赖**：`shared`、基础几何公理层、约束拓扑规约层、多策略自动推理层的只读结果。  
**禁止依赖**：parser 实现；禁止回写推理上下文。

**关键数据结构**：

```text
Lv00ProofCompiler
Lv00ProofTrace
Lv00ExportFormat
Lv00VisualizationModel
Lv00StreamEvent
```

**现有迁移候选**：

```text
core/src/core/proof_export_enhanced.c
core/src/core/proof_trace.c
core/src/core/proof_widget.c
core/src/core/tikz_export.c
core/src/core/stream.c
core/src/core/stream_context_util.c
core/src/interop/interop.c
```

---

## 3. Shared 公共基础层

`shared` 是跨层公共基础设施，不属于业务五层之一。

**允许包含**：

```text
错误码、状态码、source span、诊断、内存池、日志、配置、基础容器、字符串工具、平台宏。
```

**禁止包含**：

```text
几何公理、约束归一化、推理策略、Groebner 调度、SMT 校验、证明格式化。
```

**现有迁移候选**：

```text
core/src/core/lv00.c
core/src/core/context.c
core/src/core/error_codes.c
core/src/core/debug.c
core/src/core/memory_pool.c
core/src/core/runtime_monitor.c
core/src/utils/lv00_utils.c
core/include/lv00/lv00_internal.h
```

---

## 4. 层级依赖图

```text
                   ┌───────────────────────────────┐
                   │  L5 输出证明编译层             │
                   │  Proof Output Compilation      │
                   └───────────────┬───────────────┘
                                   │ 只读 Proof Object
                                   ▼
                   ┌───────────────────────────────┐
                   │  L4 多策略自动推理层           │
                   │  Multi-strategy Reasoning      │
                   └───────────────┬───────────────┘
                                   │ Constraint System
                                   ▼
                   ┌───────────────────────────────┐
                   │  L3 约束拓扑规约层             │
                   │  Constraint Topology           │
                   └───────────────┬───────────────┘
                                   │ Geometry Axioms
                                   ▼
                   ┌───────────────────────────────┐
                   │  L2 基础几何公理层             │
                   │  Geometry Axiom Foundation     │
                   └───────────────┬───────────────┘
                                   │ Typed IR
                                   ▼
                   ┌───────────────────────────────┐
                   │  L1 词法语法解析层             │
                   │  Lexical & Syntax Parsing      │
                   └───────────────────────────────┘

        shared：所有层可依赖，但 shared 不依赖任何业务层。
```

---

## 5. 严格规则

| 编号 | 规则 | 说明 |
|---|---|---|
| R1 | 下层禁止 include 上层头文件 | 防止反向耦合 |
| R2 | 下层禁止调用上层函数 | 防止运行时反向依赖 |
| R3 | 输出层禁止修改推理上下文 | 输出只读 |
| R4 | 解析层禁止执行证明或求解 | 只生成 AST/Typed IR |
| R5 | 公理层禁止执行搜索 | 只定义本体、公理、前置条件 |
| R6 | 约束层禁止格式化证明 | 只处理约束系统 |
| R7 | 推理层禁止生成最终展示格式 | 只生成 Proof Object |
| R8 | shared 禁止承载数学业务逻辑 | 保持基础设施纯净 |

---

## 6. 数据流规范

```text
Source Text
  ↓
Token Stream
  ↓
AST
  ↓
Typed AST / Geometric IR
  ↓
Geometry Ontology + Axiom References
  ↓
Constraint Graph / Normalized Constraint System
  ↓
Reasoning Context / Proof Object
  ↓
Proof Trace / Export / Visualization
```

每一步必须保留：

```text
source_span
origin_id
diagnostic_context
semantic_kind
```

用于错误定位、证明溯源和跨语言导出。

---

## 7. API 契约

### 7.1 解析层输出契约

```text
输入：UTF-8 Lv00 源文本
输出：AST、Typed IR、诊断列表
不得输出：约束归一化结果、证明结果、可视化对象
```

### 7.2 公理层输出契约

```text
输入：Typed IR 中的几何实体和关系引用
输出：几何本体对象、公理规则、退化条件
不得输出：搜索结论、证明文本
```

### 7.3 约束层输出契约

```text
输入：几何实体、公理关系、约束事实
输出：Constraint Graph、Normalization Result、Constraint Status
不得输出：最终证明文本
```

### 7.4 推理层输出契约

```text
输入：Normalized Constraint System、Proof Goal、Reasoning Options
输出：Proof Object、Reasoning Trace、Contradiction Trace
不得输出：LaTeX/TikZ/自然语言最终文本
```

### 7.5 输出层输出契约

```text
输入：Proof Object、Proof Trace、Visualization Model
输出：Text、JSON、LaTeX、TikZ、Lean、Coq 等格式
不得修改：Reasoning Context、Constraint Graph、Axiom Package
```

---

## 8. 文件到层级的迁移映射

| 当前文件/目录 | 目标层级 | 备注 |
|---|---|---|
| `core/src/parser/*` | L1 language | 词法、解析、公式转换 |
| `core/src/core/dsl_compiler.c` | L1 language | 后续移入 `src/language` |
| `core/src/core/math_input.c` | L1 language | 数学输入解析 |
| `core/src/axiom/*` | L2 axiom | 公理包与评级 |
| `core/src/core/euclidean_geometry.c` | L2 axiom | 基础几何本体/度量需拆分 |
| `core/src/core/constraint_graph.c` | L3 constraint | 约束图核心 |
| `core/src/core/normalization.c` | L3 constraint | 拓扑/表达式归一化需拆分归属 |
| `core/src/core/symbolic_coord.c` | L3 constraint / L4 algebra | 坐标对象与代数推理需拆分 |
| `core/src/core/engine*.c` | L4 reasoning | 推理调度 |
| `core/src/core/solver*.c` | L4 reasoning | 求解与代数后端 |
| `core/src/core/groebner_engine.c` | L4 reasoning | Groebner 后端 |
| `core/src/core/smt_backend_impl.c` | L4 reasoning | SMT 后端 |
| `core/src/core/proof.c` | L4 reasoning / L5 proof | Proof Object 与输出逻辑需拆分 |
| `core/src/core/proof_trace.c` | L5 proof | 溯源输出，若参与推理需拆分 |
| `core/src/core/proof_export_enhanced.c` | L5 proof | 导出 |
| `core/src/core/proof_widget.c` | L5 proof | 可视化 |
| `core/src/core/tikz_export.c` | L5 proof | TikZ 导出 |
| `core/src/core/stream.c` | L5 proof / shared | 事件结构与输出逻辑需拆分 |
| `core/src/utils/lv00_utils.c` | shared | 通用工具 |
| `core/src/core/error_codes.c` | shared | 错误码 |
| `core/src/core/memory_pool.c` | shared | 内存池 |

---

## 9. 迁移指南

### 9.1 第一阶段：文档与目标结构

- 建立 `FIVE_LAYER_ACADEMIC_REFACTOR_PLAN.md`。
- 建立 `LV00_LANGUAGE_SPEC.md`。
- 更新本架构文档。
- 不一次性移动大文件。

### 9.2 第二阶段：CMake 分组与目录骨架

建议新增目录：

```text
core/src/shared
core/src/language
core/src/axiom
core/src/constraint
core/src/reasoning
core/src/proof
core/include/lv00/shared
core/include/lv00/language
core/include/lv00/axiom
core/include/lv00/constraint
core/include/lv00/reasoning
core/include/lv00/proof
```

建议 CMake target：

```text
lv00_shared
lv00_layer1_language
lv00_layer2_axiom
lv00_layer3_constraint
lv00_layer4_reasoning
lv00_layer5_proof
```

### 9.3 第三阶段：测试先行

优先新增：

```text
test/c/test_language_bnf.c
test/c/test_constraint_compatibility.c
test/c/test_proof_contradiction_scope.c
```

### 9.4 第四阶段：接口收束

- 增加 `Lv00ConstraintStatus`。
- 增加 `Lv00AssumptionScope`。
- 将无界 ex falso 改为局部矛盾闭包。
- 将输出导出从推理内核中剥离。

---

## 10. 禁止事项

1. 禁止 parser 直接调用 solver、proof、SMT、Groebner。
2. 禁止 axiom 层执行证明搜索。
3. 禁止 constraint 层生成自然语言证明。
4. 禁止 reasoning 层直接生成 TikZ/LaTeX 最终输出。
5. 禁止 proof/output 层修改约束图或推理上下文。
6. 禁止 shared 层加入几何定理、推理规则或导出格式逻辑。
7. 禁止将局部矛盾扩散为全局任意命题。
8. 禁止将浮点近似结果直接标记为严格证明事实。

---

## 11. 构建系统集成原则

现有 CMake 已具有五层 OBJECT 库雏形，但旧第 2 层为资源管理层，与本次学术五层不一致。整改时应分两步：

1. **兼容期**：保留旧 target，新增新层注释、映射和测试，避免立即破坏构建。
2. **迁移期**：逐步将源文件迁入新目录和新 target。
3. **收敛期**：删除旧命名或将旧 target 作为兼容别名。

构建检查建议：

```bash
cmake -S . -B build_verify -DENABLE_LAYER_VALIDATION=ON
cmake --build build_verify
ctest --test-dir build_verify --output-on-failure
```

---

## 12. 与元语言规范的关系

本架构文档定义系统分层；`LV00_LANGUAGE_SPEC.md` 定义第 1 层的输入契约。

第 1 层输出的 Typed IR 是第 2 层及后续所有层的入口。后续任何语法、符号、类型或操作语义变更，必须同步更新：

```text
doc/docs/LV00_LANGUAGE_SPEC.md
parser 测试
Typed IR 定义
相关层的输入契约
```

---

## 13. 当前状态结论

- 原 v3.3 文档的“资源管理层”不再作为业务第 2 层，而改为 `shared` 公共基础层。
- 新业务第 2 层为“基础几何公理层”。
- 当前源码仍大量集中在 `core/src/core`，后续需按职责分批迁移。
- `proof.h` 中关于“爆炸原理”的描述需要整改为带假设作用域的局部矛盾闭包。
- 约束层需要新增四态相容检测机制。

---

## 14. 下一步落地任务

1. 新增约束相容检测测试与接口。
2. 新增反证作用域测试与接口。
3. 在 CMake 中逐步引入新层 target 或别名。
4. 拆分 `proof.c` 中证明对象、反证机制、导出逻辑。
5. 拆分 `solver.c` 中代数方程、Groebner、自由度分析和调度逻辑。
