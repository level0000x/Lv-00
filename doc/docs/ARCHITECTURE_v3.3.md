# Lv-00 五层架构规范 v3.3

> **项目**: Lv-00 几何元语言系统
> **架构版本**: 3.3
> **最后更新**: 2026-05-24
> **适用范围**: 所有 C 源文件（`src/` 目录）及头文件（`include/lv00/` 目录）

---

## 目录

1. [架构概览](#1-架构概览)
2. [层级定义](#2-层级定义)
3. [层级关系图](#3-层级关系图)
4. [严格规则](#4-严格规则)
5. [数据流规范](#5-数据流规范)
6. [API 契约](#6-api-契约)
7. [文件到层级的映射表](#7-文件到层级的映射表)
8. [迁移指南](#8-迁移指南)
9. [设计原则与编码标准](#9-设计原则与编码标准)
10. [反模式（禁止事项）](#10-反模式禁止事项)
11. [构建系统集成](#11-构建系统集成)
12. [附录：层级验证宏](#12-附录层级验证宏)

---

## 1. 架构概览

Lv-00 采用严格的**五层单向依赖架构**，灵感来源于 OSI 网络模型和编译器前端-后端管道设计。每一层只暴露向上的抽象接口，下层对上层完全透明，上层绝不允许反向依赖下层。

```
┌──────────────────────────────────────────────────────────────┐
│                    第5层: 结果输出层                          │
│  step_formatting, structured_conclusions, error_reporting,   │
│  data_export, stream_events, tikz_export, proof_widget       │
├──────────────────────────────────────────────────────────────┤
│                    第4层: 公理推理层                          │
│  theorem_matching, bidirectional_deduction, branch_reasoning,│
│  logic_verification, solver, rewrite, unify, normalization   │
├──────────────────────────────────────────────────────────────┤
│                    第3层: 几何拓扑层                          │
│  graph_construction, constraints, graph_traversal,           │
│  geometric_relations, symbolic_coord, euclidean_geometry     │
├──────────────────────────────────────────────────────────────┤
│                    第2层: 资源管理层                          │
│  memory_pool, object_lifecycle, cache_management,            │
│  error_codes, debugging, utility_functions                   │
├──────────────────────────────────────────────────────────────┤
│                    第1层: 输入解析层                          │
│  syntax_tokenization, expression_normalization,              │
│  input_filtering, formula_parsing, lexer, DSL_compilation    │
└──────────────────────────────────────────────────────────────┘
```

### 1.1 核心理念

- **单向依赖**: 上层调用下层，下层绝不知道上层的存在
- **逐层抽象**: 每一层在前一层的基础上提供更高级的抽象
- **契约驱动**: 层间通过明确定义的 API 契约通信
- **最小知识原则**: 每一层只需了解其直接下层，不得跨层调用

---

## 2. 层级定义

### 2.1 第1层: 输入解析层（Input/Parser Layer）

**职责**: 将外部输入（文本、DSL 代码、数学表达式）转化为系统内部数据结构。

| 功能模块 | 描述 |
|---------|------|
| `syntax_tokenization` | 词法分析，将原始文本分割为 token 流 |
| `expression_normalization` | 表达式标准化，统一不同表示形式 |
| `input_filtering` | 输入过滤与校验，拒绝非法输入 |
| `formula_parsing` | 数学公式解析，生成内部 AST |
| `formula_conversion` | 公式格式转换（LaTeX <-> 内部表示） |
| `formula_rendering` | 公式渲染为可视化表示 |
| `DSL_compilation` | 领域特定语言编译（.lv / .lvmod 等） |

**依赖关系**: Layer 1 可以调用 Layer 2（资源管理），但不能调用 Layer 3/4/5。

**关键数据结构**:
- `Token` / `TokenStream` — 词法 token
- `ASTNode` — 抽象语法树节点
- `FormulaIR` — 公式中间表示

---

### 2.2 第2层: 资源管理层（Resource/Memory Layer）

**职责**: 管理系统级资源，提供内存分配、对象生命周期管理、缓存、错误码和调试基础设施。

| 功能模块 | 描述 |
|---------|------|
| `memory_pool` | 内存池分配器，减少碎片化 |
| `object_lifecycle` | 统一的对象创建/销毁/深拷贝接口 |
| `cache_management` | 结果缓存，避免重复计算 |
| `error_codes` | 统一错误码系统 |
| `debug_infrastructure` | 调试日志、断言、追踪 |
| `utility_functions` | 通用工具函数（字符串、数学辅助等） |

**依赖关系**: Layer 2 是最底层逻辑层，不依赖任何上层，只能使用 C 标准库和 GMP。

**关键数据结构**:
- `Lv00Arena` — 内存竞技场
- `Lv00Error` — 统一错误结构
- `DebugContext` — 调试上下文

---

### 2.3 第3层: 几何拓扑层（Geometry/Topology Layer）

**职责**: 提供几何空间的数学表示、约束图的构建与操作、拓扑关系的计算。

| 功能模块 | 描述 |
|---------|------|
| `graph_construction` | 约束图的创建、节点/边管理 |
| `constraints` | 几何约束的定义、验证与传播 |
| `graph_traversal` | 图遍历算法（BFS、DFS、拓扑排序） |
| `geometric_relations` | 几何关系计算（距离、角度、包含） |
| `symbolic_coord` | 符号坐标系系统 |
| `euclidean_geometry` | 欧几里得几何原语 |
| `interactive_geometry` | 交互式几何编辑支持 |
| `high_dim` | 高维几何结构（>3 维） |
| `geometry_compression` | 几何数据压缩与解压缩 |
| `geo_event_detection` | 几何事件检测（碰撞、包含、相交） |

**依赖关系**: Layer 3 可以调用 Layer 2（资源管理），但不能调用 Layer 1/4/5。

**关键数据结构**:
- `ConstraintGraph` — 约束图
- `SymbolicCoord` — 符号坐标
- `GeoPoint` / `GeoLine` / `GeoCircle` — 几何原语
- `TopoRelation` — 拓扑关系

---

### 2.4 第4层: 公理推理层（Axiom/Reasoning Layer）

**职责**: 基于几何表示进行公理化推理、定理证明、化简与求解。

| 功能模块 | 描述 |
|---------|------|
| `theorem_matching` | 定理模式匹配 |
| `bidirectional_deduction` | 双向演绎推理（正向/反向） |
| `branch_reasoning` | 分支推理与回溯 |
| `logic_verification` | 逻辑一致性验证 |
| `solver` | 约束求解器（数值/符号） |
| `rewrite` | 项重写系统 |
| `unify` | 合一算法 |
| `normalization` | 表达式规范化 |
| `type_system` | 类型检查与推导 |
| `proof` | 证明构造与管理 |
| `axiom_package` | 公理包加载与管理 |
| `func_block` | 函数块系统（组合、实例化、注册） |
| `smt_backend` | SMT 求解后端 |
| `atp_backend` | 自动定理证明后端 |
| `sat_encoding` | SAT 编码转换 |
| `bdd_encoding` | BDD 编码与操作 |
| `groebner_engine` | Groebner 基引擎 |
| `engine` | 主引擎工作流编排 |
| `preset` | 预设数学理论（群论、环论、拓扑等） |

**依赖关系**: Layer 4 可以调用 Layer 2 和 Layer 3，但不能调用 Layer 1/5。

**关键数据结构**:
- `LV00Engine` — 主引擎
- `ProofTree` — 证明树
- `RewriteRule` — 重写规则
- `UnifyContext` — 合一上下文
- `FuncBlock` — 函数块
- `AxiomPackage` — 公理包

---

### 2.5 第5层: 结果输出层（Output Layer）

**职责**: 将推理结果格式化输出为人类可读或机器可处理的格式。

| 功能模块 | 描述 |
|---------|------|
| `step_formatting` | 推理步骤的格式化输出 |
| `structured_conclusions` | 结构化结论生成（JSON、XML 等） |
| `error_reporting` | 面向用户的错误报告 |
| `data_export` | 数据导出（多种格式） |
| `stream_events` | 流式事件发射与管理 |
| `tikz_export` | TikZ/LaTeX 图形导出 |
| `proof_widget` | 证明可视化组件 |
| `interop` | 外部系统互操作接口 |
| `module` | 模块格式定义与解析 |

**依赖关系**: Layer 5 可以调用 Layer 2、Layer 3、Layer 4，但不能调用 Layer 1。

**关键数据结构**:
- `StreamContext` / `StreamEvent` — 流式输出
- `TikzOutput` — TikZ 输出
- `ProofWidget` — 证明可视化
- `ExportFormat` — 导出格式枚举

---

## 3. 层级关系图

```
                        ┌──────────────────────┐
                        │     LAYER 5          │
                        │   结果输出层          │
                        │  (stream, tikz,      │
                        │   proof_widget,      │
                        │   interop, module)   │
                        └──────────┬───────────┘
                                   │ 依赖 (uses)
                                   ▼
                        ┌──────────────────────┐
                        │     LAYER 4          │
                        │   公理推理层          │
                        │  (engine, solver,    │
                        │   proof, rewrite,    │
                        │   unify, axiom,      │
                        │   func_block, preset)│
                        └──────────┬───────────┘
                                   │ 依赖 (uses)
                        ┌──────────┴───────────┐
                        │                      │
                        ▼                      ▼
              ┌──────────────────┐  ┌──────────────────┐
              │     LAYER 3     │  │     LAYER 2      │
              │   几何拓扑层     │◀─│   资源管理层       │
              │  (constraint,   │  │  (utils, memory,  │
              │   geometry,     │  │   error, debug)   │
              │   symbolic)     │  │                   │
              └────────┬────────┘  └────────┬──────────┘
                       │                    │
                       │  依赖 (uses)       │ 依赖 (uses)
                       ▼                    ▼
              ┌──────────────────────────────────────┐
              │            LAYER 1                   │
              │          输入解析层                   │
              │  (parser, lexer, formula, DSL)       │
              └──────────────────────────────────────┘
```

**说明**:
- Layer 5 同时依赖 Layer 4 和 Layer 3（需要几何数据来渲染输出）
- Layer 4 同时依赖 Layer 3 和 Layer 2（推理需要几何基础 + 资源管理）
- Layer 3 仅依赖 Layer 2
- Layer 1 仅依赖 Layer 2
- Layer 2 依赖 C 标准库和 GMP

---

## 4. 严格规则

### 4.1 核心规则（不可违反）

| 规则编号 | 规则内容 | 违规后果 |
|---------|---------|---------|
| **R1** | 上层可以调用下层，**绝不允许下层调用上层** | 编译错误（CMake 链接顺序阻止） |
| **R2** | **同一层内不允许循环依赖**（文件 A 调用 B，B 又调用 A） | 编译错误或未定义行为 |
| **R3** | 禁止跨层直接调用（如 Layer 5 直接调用 Layer 1） | 架构退化，维护困难 |
| **R4** | 层间通信必须通过明确定义的 API 接口 | 隐式耦合，难以追踪 |
| **R5** | 禁止通过全局变量进行层间通信 | 数据竞争、可测试性差 |
| **R6** | 禁止通过回调函数绕过层级限制 | 隐式反向依赖 |

### 4.2 允许的依赖矩阵

|            | Layer 1 | Layer 2 | Layer 3 | Layer 4 | Layer 5 |
|------------|:-------:|:-------:|:-------:|:-------:|:-------:|
| **Layer 1** |   --    |   是    |   否    |   否    |   否    |
| **Layer 2** |   否    |   --    |   否    |   否    |   否    |
| **Layer 3** |   否    |   是    |   --    |   否    |   否    |
| **Layer 4** |   否    |   是    |   是    |   --    |   否    |
| **Layer 5** |   否    |   是    |   是    |   是    |   --    |

（"是" = 该行对应的层可以依赖该列对应的层）

### 4.3 同层内文件间依赖规则

同一层内的文件允许相互调用，但必须遵守以下规则:

1. **有向无环图 (DAG)**: 同层文件间的依赖关系必须构成有向无环图，即不允许循环依赖。
2. **接口优于实现**: 同层文件间调用优先使用声明在头文件中的公共接口。
3. **最小暴露**: 仅在必要时才将同层内部函数暴露在头文件中（使用 `lv00_internal.h` 或静态函数）。

### 4.4 头文件包含规则

```
Layer 5 的 .c 文件:  可以 #include Layer 2/3/4/5 的头文件
Layer 4 的 .c 文件:  可以 #include Layer 2/3/4 的头文件
Layer 3 的 .c 文件:  可以 #include Layer 2/3 的头文件
Layer 2 的 .c 文件:  可以 #include Layer 2 的头文件 + 标准库
Layer 1 的 .c 文件:  可以 #include Layer 1/2 的头文件 + 标准库
```

---

## 5. 数据流规范

### 5.1 顶层数据流（用户请求 -> 输出结果）

```
用户输入（文本/DSL/交互）
     │
     ▼
┌─────────────────────────────────────────────────┐
│ Layer 1: 输入解析                                │
│ 原始文本 → TokenStream → AST/IR                  │
│ 输出: Lv00Expression, Lv00Constraint[]          │
└───────────────────────┬─────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────┐
│ Layer 3: 几何拓扑                                │
│ IR → ConstraintGraph, SymbolicCoord              │
│ 输出: 可求解的约束图                              │
└───────────────────────┬─────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────┐
│ Layer 4: 公理推理                                │
│ ConstraintGraph → Proof, Solution               │
│ 输出: 推理结果、证明树、求解结果                    │
└───────────────────────┬─────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────┐
│ Layer 5: 结果输出                                │
│ Proof/Solution → StreamEvent, TikZ, JSON         │
│ 输出: 格式化文本、图形、导出文件                    │
└─────────────────────────────────────────────────┘
```

### 5.2 数据传递方式

| 传递方式 | 适用场景 | 使用层级 |
|---------|---------|---------|
| 函数参数（值传递） | 小型不可变数据（坐标、索引） | 所有层 |
| 函数参数（指针传递） | 大型数据结构（图、证明树） | Layer 2+ |
| 返回值（指针） | 新创建的对象（所有权转移） | Layer 2+ |
| 回调注册 | 事件通知、流式输出 | Layer 4 -> Layer 5 |
| 共享上下文 | 引擎级全局状态 | Layer 4 内部 |

### 5.3 所有权规则

1. **创建者拥有**: 分配内存的层负责最终释放。
2. **跨层传递时所有权转移**: 通过函数命名约定指示（`create_*` 返回所有权，`get_*` 不转移）。
3. **禁止隐式共享**: 跨层传递的指针必须明确所有权语义。
4. **深拷贝优先**: 跨层传递可变数据时应深拷贝，避免下层修改影响上层。

---

## 6. API 契约

### 6.1 Layer 1 -> Layer 2 接口

```c
/* 内存分配（Layer 2 提供给 Layer 1） */
void *lv00_malloc(size_t size);
void *lv00_calloc(size_t nmemb, size_t size);
void *lv00_realloc(void *ptr, size_t size);
void  lv00_free(void *ptr);

/* 错误报告（Layer 2 提供给 Layer 1） */
typedef enum { LV00_OK, LV00_ERR_PARSE, LV00_ERR_MEMORY, ... } Lv00ErrorCode;
void lv00_set_error(Lv00ErrorCode code, const char *msg);
const char *lv00_get_last_error(void);

/* 字符串工具（Layer 2 提供给 Layer 1） */
size_t lv00_strlcpy(char *dst, const char *src, size_t size);
char  *lv00_strdup(const char *src);
```

### 6.2 Layer 3 -> Layer 2 接口

Layer 3 使用 Layer 2 的所有内存管理接口，此外：

```c
/* 节点深拷贝（Layer 2 提供给 Layer 3） */
void *lv00_node_deep_copy(const void *src, size_t size);

/* 调试接口（Layer 2 提供给 Layer 3） */
void lv00_debug_assert(bool condition, const char *file, int line, const char *msg);
void lv00_debug_trace(const char *format, ...);
```

### 6.3 Layer 3 对外接口（供 Layer 4 使用）

```c
/* 约束图 API */
ConstraintGraph *constraint_graph_create(void);
void             constraint_graph_destroy(ConstraintGraph *graph);
int              constraint_graph_add_node(ConstraintGraph *graph, NodeType type, ...);
bool             constraint_graph_add_constraint(ConstraintGraph *graph, int node_a, int node_b, ConstraintType type, ...);

/* 符号坐标 API */
SymbolicCoord *symbolic_coord_create(void);
void           symbolic_coord_destroy(SymbolicCoord *coord);
bool           symbolic_coord_set_relation(SymbolicCoord *coord, SymbolicRelation rel, ...);

/* 欧几里得几何 API */
bool euclidean_distance(const GeoPoint *a, const GeoPoint *b, double *out);
bool euclidean_angle(const GeoPoint *a, const GeoPoint *b, const GeoPoint *c, double *out);
```

### 6.4 Layer 4 对外接口（供 Layer 5 使用）

```c
/* 引擎 API */
LV00Engine       *engine_create(void);
void              engine_destroy(LV00Engine *engine);
EngineSolveResult  engine_solve(LV00Engine *engine);

/* 证明 API */
ProofTree *proof_create(void);
void       proof_destroy(ProofTree *proof);
bool       proof_add_step(ProofTree *proof, const ProofStep *step);

/* 重写 API */
RewriteRule *rewrite_rule_create(const char *pattern, const char *replacement);
bool         rewrite_apply(LV00Engine *engine, RewriteRule *rule);

/* 流式上下文（Layer 4 持有，Layer 5 使用） */
StreamContext *engine_get_stream_context(const LV00Engine *engine);
```

### 6.5 Layer 5 对外接口（供外部使用者）

```c
/* 流式输出 API */
StreamContext *stream_context_create(void);
void           stream_register_callback(StreamContext *ctx, StreamCallback cb, void *user_data);
bool           stream_emit(StreamContext *ctx, const StreamEvent *event);

/* TikZ 导出 API */
char *tikz_export_proof(const ProofTree *proof);

/* 模块 API */
Module *module_load(const char *path);
void    module_unload(Module *mod);
```

---

## 7. 文件到层级的映射表

### 7.1 Layer 1: 输入解析层

| 源文件 | 说明 |
|--------|------|
| `src/parser/formula_parser.c` | 数学公式解析器 |
| `src/parser/formula_converter.c` | 公式格式转换 |
| `src/parser/formula_renderer.c` | 公式渲染输出 |
| `src/parser/lexer_shared.c` | 共享词法分析器 |
| `src/core/dsl_compiler.c` | DSL 编译器（GCLC 语言编译） |
| `src/core/math_input.c` | 数学输入处理 |

### 7.2 Layer 2: 资源管理层

| 源文件 | 说明 |
|--------|------|
| `src/utils/lv00_utils.c` | 通用工具函数 |
| `src/core/lv00.c` | 核心初始化与退出 |
| `src/core/error_codes.c` | 统一错误码系统 |
| `src/core/debug.c` | 调试基础设施 |
| `src/core/node_deep_copy.c` | 节点深拷贝（对象生命周期） |

### 7.3 Layer 3: 几何拓扑层

| 源文件 | 说明 |
|--------|------|
| `src/core/constraint_graph.c` | 约束图核心 |
| `src/core/symbolic_coord.c` | 符号坐标系 |
| `src/core/euclidean_geometry.c` | 欧几里得几何 |
| `src/core/interactive_geo.c` | 交互式几何 |
| `src/core/high_dim.c` | 高维几何结构 |
| `src/core/geometry_compress.c` | 几何数据压缩 |
| `src/core/geo_event_detect.c` | 几何事件检测 |
| `src/core/geo_spec.c` | 几何规范定义 |
| `src/core/geometry_csg.c` | 构造实体几何（CSG） |
| `src/core/sparse_linear_algebra.c` | 稀疏线性代数 |
| `src/core/geom_evol.c` | 几何演化 |
| `src/core/float_error.c` | 浮点误差分析 |
| `src/core/mpz_poly_resultant.c` | 多项式结式计算 |

### 7.4 Layer 4: 公理推理层

| 源文件 | 说明 |
|--------|------|
| `src/core/engine.c` | 主引擎（工作流编排） |
| `src/core/engine_scheduler.c` | 引擎调度器 |
| `src/core/solver.c` | 约束求解器 |
| `src/core/solver_core.c` | 求解器核心算法 |
| `src/core/proof.c` | 证明构造与管理 |
| `src/core/proof_optimize.c` | 证明优化 |
| `src/core/proof_multi_strategy.c` | 多策略证明 |
| `src/core/rewrite.c` | 项重写系统 |
| `src/core/unify.c` | 合一算法 |
| `src/core/normalization.c` | 表达式规范化 |
| `src/core/recursion.c` | 递归推理 |
| `src/core/type_system.c` | 类型系统 |
| `src/core/prop_verifier.c` | 属性验证器 |
| `src/core/groebner_engine.c` | Groebner 基引擎 |
| `src/core/smt_backend_impl.c` | SMT 后端实现 |
| `src/core/atp_backend.c` | ATP 自动定理证明后端 |
| `src/core/sat_encoding.c` | SAT 编码 |
| `src/core/bdd_encoding.c` | BDD 编码 |
| `src/core/approx_counter.c` | 近似计数器 |
| `src/core/probabilistic_constraint.c` | 概率约束 |
| `src/core/relation_model.c` | 关系模型 |
| `src/core/algebra_mode.c` | 代数模式 |
| `src/core/numerical_backend.c` | 数值计算后端 |
| `src/core/mini_kernel.c` | 微内核 |
| `src/core/gc_language.c` | GCLC 语言核心 |
| `src/core/ecosystem.c` | 生态系统管理器 |
| `src/core/math_protocol.c` | 数学协议 |
| `src/axiom/axiom_pkg.c` | 公理包系统 |
| `src/func_block/func_block.c` | 函数块核心 |
| `src/func_block/func_block_compose.c` | 函数块组合 |
| `src/func_block/func_block_determinism.c` | 函数块确定性检测 |
| `src/func_block/func_block_instantiate.c` | 函数块实例化 |
| `src/func_block/func_block_preset.c` | 预设函数块 |
| `src/func_block/func_block_preset_ops.c` | 预设操作 |
| `src/func_block/func_block_registry.c` | 函数块注册表 |
| `src/func_block/func_block_selector.c` | 函数块选择器 |
| `src/func_block/func_block_serialize.c` | 函数块序列化 |
| `src/func_block/func_block_utils.c` | 函数块工具 |
| `src/func_block/preset_basic_math.c` | 基础数学预设 |
| `src/func_block/preset_blocks.c` | 预设块系统 |
| `src/func_block/preset_calculus.c` | 微积分预设 |
| `src/func_block/preset_common.c` | 通用预设 |
| `src/func_block/preset_manager.c` | 预设管理器 |
| `src/core/three_valued_logic.c` | 三值逻辑系统 |
| `src/core/modal_operators.c` | 模态算子（必然、可能等） |
| `src/core/quantifier.c` | 量词系统（全称、存在等） |
| `src/core/runtime_monitor.c` | 运行时监控（资源/超时/心跳） |
| `src/core/test_framework.c` | 测试框架（断言、套件、报告） |
| `src/core/geometry_transform.c` | 几何变换（平移、旋转、缩放） |
| `src/core/expr_canonical.c` | 表达式规范化（标准形/排序） |
| `src/core/simd_ops.c` | SIMD 向量化操作 |
| `src/core/thread_pool.c` | 线程池（并行任务调度） |
| `src/core/memory_pool.c` | 内存池（批量分配/回收） |
| `src/core/rational.c` | 有理数精确运算 |
| `src/core/logic_check.c` | 逻辑一致性检查 |
| `src/core/circuit_breaker.c` | 熔断器（异常检测与降级） |
| `src/core/proof_priority.c` | 证明优先级调度 |
| `src/core/proof_trace.c` | 证明轨迹记录与回放 |
| `src/preset/*.c` (全部 55 个文件) | 数学理论预设（群论/环论/拓扑/分析等） |

### 7.5 Layer 5: 结果输出层

| 源文件 | 说明 |
|--------|------|
| `src/core/stream.c` | 流式输出核心 |
| `src/core/stream_context_util.c` | 流式上下文工具 |
| `src/core/proof_widget.c` | 证明可视化组件 |
| `src/core/tikz_export.c` | TikZ/LaTeX 图形导出 |
| `src/interop/interop.c` | 外部系统互操作 |
| `src/core/module.c` | 模块系统 |

### 7.6 总计

| 层级 | 文件数 | 大致代码量 |
|------|--------|-----------|
| Layer 1 (输入解析层) | 6 | ~3,000 行 |
| Layer 2 (资源管理层) | 5 | ~2,500 行 |
| Layer 3 (几何拓扑层) | 13 | ~8,000 行 |
| Layer 4 (公理推理层) | 42 (核心) + 15 (func_block) + 55 (preset) = 112 | ~45,000 行 |
| Layer 5 (结果输出层) | 6 | ~5,000 行 |
| **总计** | **142** | **~63,500 行** |

---

## 8. 迁移指南

### 8.1 当前状态

当前 Lv-00 项目采用扁平结构，所有 `src/core/*.c` 文件在同一个 CMake 目标中编译。这导致：
- 文件间隐式相互依赖，难以追踪
- 新增功能时不清楚应该放在哪里
- 无法利用层级约束防止架构退化

### 8.2 第一阶段: CMake 层级化（当前阶段）

1. **创建 OBJECT 库**: 为每个层级创建 CMake OBJECT 库
   ```cmake
   add_library(lv00_layer1_parser OBJECT ${LAYER1_SOURCES})
   add_library(lv00_layer2_resource OBJECT ${LAYER2_SOURCES})
   add_library(lv00_layer3_geometry OBJECT ${LAYER3_SOURCES})
   add_library(lv00_layer4_reasoning OBJECT ${LAYER4_SOURCES})
   add_library(lv00_layer5_output OBJECT ${LAYER5_SOURCES})
   ```

2. **按序链接**: Layer 5 <- Layer 4 <- Layer 3 <- Layer 2 <- Layer 1
   ```cmake
   target_link_libraries(lv00_layer5_output lv00_layer4_reasoning lv00_layer3_geometry)
   target_link_libraries(lv00_layer4_reasoning lv00_layer3_geometry lv00_layer2_resource)
   target_link_libraries(lv00_layer3_geometry lv00_layer2_resource)
   target_link_libraries(lv00_layer1_parser lv00_layer2_resource)
   ```

3. **保持向后兼容**: 使用聚合目标包裹所有层
   ```cmake
   add_library(lv00_static STATIC $<TARGET_OBJECTS:...>)
   ```

### 8.3 第二阶段: 头文件重组（未来）

1. 按层级重组 `include/lv00/` 目录下的头文件
2. 为每个层级创建独立的内部头文件命名空间
3. 使用层级验证宏在编译时检查依赖违规

### 8.4 第三阶段: 运行时层级检查（未来）

1. 使用 `layer_validation` 编译标志启用运行时层级检查
2. 在 Debug 构建中强制层级约束
3. 生成依赖关系图用于 CI 验证

### 8.5 迁移检查清单

- [x] 分析所有现有 .c 文件并映射到层级
- [x] 创建 `docs/ARCHITECTURE_v3.3.md` 规范文档
- [x] 重构 `CMakeLists.txt` 以支持分层构建
- [x] 在 `engine.h` 中添加层级验证宏
- [ ] 添加 CI 步骤检查层级依赖合规性
- [ ] 重构头文件目录结构
- [ ] 为每个层级编写单元测试

---

## 9. 设计原则与编码标准

### 9.1 新增代码原则

1. **先定层级，再写代码**: 新增任何功能前，先确定它属于哪一层。
2. **最小接口原则**: 只暴露必要的函数到公共头文件。内部函数使用 `static`。
3. **单一职责**: 一个源文件只负责一个明确的功能领域。
4. **命名一致性**: 函数名应体现其所属层级和模块：
   - Layer 1: `parse_*`, `lex_*`, `token_*`, `compile_*`
   - Layer 2: `lv00_*` (通用工具), `error_*`, `debug_*`, `mem_*`
   - Layer 3: `geo_*`, `coord_*`, `graph_*`, `constraint_*`
   - Layer 4: `engine_*`, `proof_*`, `solver_*`, `rewrite_*`, `unify_*`
   - Layer 5: `stream_*`, `export_*`, `format_*`, `widget_*`

### 9.2 编码标准

1. **C11 标准**: 所有代码必须符合 C11 标准（`-std=c11 -Wpedantic`）。
2. **头文件守卫**: 使用 `#ifndef LV00_<MODULE>_H` / `#define` / `#endif` 模式。
3. **错误处理**: 函数返回错误码，不使用全局 `errno`。
4. **内存管理**: 
   - 使用 `lv00_malloc` / `lv00_free` 而非直接 `malloc` / `free`
   - 在文档注释中明确所有权语义（`@note 调用者负责释放`）
5. **文档注释**: 使用 Doxygen 格式 (`@brief`, `@param`, `@return`, `@note`)。

### 9.3 层级归属决策树

新增文件时，按以下顺序判断层级：

```
1. 它处理原始用户输入吗？
   └── 是 → Layer 1 (输入解析层)

2. 它是纯工具函数或基础设施吗？
   └── 是 → Layer 2 (资源管理层)

3. 它涉及几何形状、坐标、空间关系吗？
   └── 是 → Layer 3 (几何拓扑层)

4. 它涉及推理、证明、求解、规则吗？
   └── 是 → Layer 4 (公理推理层)

5. 它处理格式化输出、可视化、数据导出吗？
   └── 是 → Layer 5 (结果输出层)
```

---

## 10. 反模式（禁止事项）

### 10.1 严禁的反向依赖

```c
/* ❌ 反模式: Layer 2 调用 Layer 4 的函数 */
// 文件: src/utils/lv00_utils.c (Layer 2)
#include "engine.h"  // 错误! Layer 2 不能包含 Layer 4 的头文件
void helper_func(void) {
    engine_solve(...);  // 错误! 反向依赖
}

/* ✅ 正确做法: Layer 4 调用 Layer 2 */
// 文件: src/core/engine.c (Layer 4)
#include "lv00_utils.h"  // 正确! Layer 4 可以包含 Layer 2 的头文件
void engine_do_work(LV00Engine *engine) {
    lv00_malloc(256);  // 正确! 正向依赖
}
```

### 10.2 严禁的同层循环依赖

```c
/* ❌ 反模式: Layer 4 内部循环依赖 */
// 文件: src/core/solver.c
#include "proof.h"
void solver_work(void) {
    proof_create_tree();  // solver 调用 proof
}

// 文件: src/core/proof.c
#include "solver.h"
void proof_validate(void) {
    solver_check();  // proof 又调用 solver -> 循环!
}

/* ✅ 正确做法: 提取公共接口到独立模块 */
// 如有需要，抽取公共逻辑到 solver_core.c
// solver.c -> solver_core.c <- proof.c  (无循环)
```

### 10.3 严禁的跨层直接调用

```c
/* ❌ 反模式: Layer 5 直接调用 Layer 1 */
// 文件: src/core/stream.c (Layer 5)
#include "formula_parser.h"  // 错误! 跨层调用
void stream_output(void) {
    parse_formula(...);  // 错误! Layer 5 不能直接调用 Layer 1
}

/* ✅ 正确做法: 通过 Layer 4 间接获取解析结果 */
// 数据流: Layer 1 -> Layer 4 -> Layer 5
// Layer 5 只使用 Layer 4 提供的已解析数据结构
```

### 10.4 严禁的隐式耦合

```c
/* ❌ 反模式: 通过全局变量跨层通信 */
// 文件: src/core/solver.c (Layer 4)
LV00Engine *g_global_engine;  // 全局引擎指针
// 文件: src/parser/formula_parser.c (Layer 1)
extern LV00Engine *g_global_engine;  // Layer 1 不应知道 Layer 4 的类型

/* ❌ 反模式: 通过回调函数绕过层级 */
// Layer 5 不能注册回调到 Layer 1 的内部逻辑中

/* ✅ 正确做法: 通过参数显式传递 */
void parse_and_build(const char *input, LV00Engine *engine);
```

### 10.5 严防的 `#include` 违规

| 模式 | 说明 | 是否允许 |
|------|------|---------|
| Layer 4 的 .c 包含 Layer 1 的 .h | 跨层反向依赖 | 禁止 |
| Layer 1 的 .c 包含 Layer 3 的 .h | 跨层正向依赖（超过一级） | 禁止 |
| Layer 5 的 .c 包含 Layer 1 的 .h | 跨多级依赖 | 禁止 |
| Layer 2 的 .c 包含任意层的 .h | Layer 2 是基础层 | 允许（仅 Layer 2 自身） |
| Layer 3 的 .c 包含 Layer 2 的 .h | 正向一级依赖 | 允许 |
| Layer 4 的 .c 包含 Layer 2 的 .h | 正向依赖 | 允许 |
| Layer 4 的 .c 包含 Layer 3 的 .h | 正向一级依赖 | 允许 |
| Layer 5 的 .c 包含 Layer 2 的 .h | 正向依赖 | 允许 |
| Layer 5 的 .c 包含 Layer 3 的 .h | 正向依赖 | 允许 |
| Layer 5 的 .c 包含 Layer 4 的 .h | 正向一级依赖 | 允许 |

---

## 11. 构建系统集成

### 11.1 CMake OBJECT 库结构

```
lv00_layer1_parser  (OBJECT)
    ├── include: ${CMAKE_SOURCE_DIR}/include/lv00, ${GMP_INCLUDE_DIR}
    ├── link: lv00_layer2_resource
    └── sources: src/parser/*.c, src/core/dsl_compiler.c, src/core/math_input.c

lv00_layer2_resource (OBJECT)
    ├── include: ${CMAKE_SOURCE_DIR}/include/lv00, ${GMP_INCLUDE_DIR}
    ├── link: GMP
    └── sources: src/utils/*.c, src/core/lv00.c, src/core/error_codes.c,
                  src/core/debug.c, src/core/node_deep_copy.c

lv00_layer3_geometry (OBJECT)
    ├── include: ${CMAKE_SOURCE_DIR}/include/lv00, ${GMP_INCLUDE_DIR}
    ├── link: lv00_layer2_resource
    └── sources: src/core/constraint_graph.c, src/core/symbolic_coord.c, ...

lv00_layer4_reasoning (OBJECT)
    ├── include: ${CMAKE_SOURCE_DIR}/include/lv00, ${GMP_INCLUDE_DIR}
    ├── link: lv00_layer3_geometry, lv00_layer2_resource
    └── sources: src/core/engine.c, src/core/solver.c, ...,
                  src/func_block/*.c, src/preset/*.c, src/axiom/*.c

lv00_layer5_output (OBJECT)
    ├── include: ${CMAKE_SOURCE_DIR}/include/lv00, ${GMP_INCLUDE_DIR}
    ├── link: lv00_layer4_reasoning, lv00_layer3_geometry, lv00_layer2_resource
    └── sources: src/core/stream.c, src/core/tikz_export.c, ...,
                  src/interop/*.c

lv00_static (STATIC)  ← 聚合目标（向后兼容）
    └── sources: $<TARGET_OBJECTS:lv00_layer1_parser>
                 $<TARGET_OBJECTS:lv00_layer2_resource>
                 $<TARGET_OBJECTS:lv00_layer3_geometry>
                 $<TARGET_OBJECTS:lv00_layer4_reasoning>
                 $<TARGET_OBJECTS:lv00_layer5_output>
```

### 11.2 构建验证

在 CMake 构建完成后，可通过以下方式验证层级约束：

```bash
# 检查是否有跨层 #include（手动审查）
grep -r "engine.h" src/parser/    # 应无结果（Layer 1 不能包含 Layer 4）
grep -r "formula_parser.h" src/core/engine.c  # 应无结果（Layer 4 不能包含 Layer 1）

# 使用 CMake 的依赖图可视化
cmake --graphviz=build/deps.dot .
dot -Tpng build/deps.dot -o build/deps.png
```

---

## 12. 附录：层级验证宏

在 `include/lv00/engine.h` 中定义了以下编译时常量用于层级验证：

```c
/* 层级标识（用于编译时断言和运行时诊断） */
#define LV00_LAYER_PARSER    1   /* 输入解析层 */
#define LV00_LAYER_RESOURCE  2   /* 资源管理层 */
#define LV00_LAYER_GEOMETRY  3   /* 几何拓扑层 */
#define LV00_LAYER_REASONING 4   /* 公理推理层 */
#define LV00_LAYER_OUTPUT    5   /* 结果输出层 */

/* 层级验证开关（通过 CMake 选项 ENABLE_LAYER_VALIDATION 控制） */
#ifdef LV00_ENABLE_LAYER_VALIDATION

/* 当前编译单元的层级归属（在 CMakeLists.txt 中通过 target_compile_definitions 设置） */
#ifndef LV00_CURRENT_LAYER
#error "LV00_CURRENT_LAYER must be defined when layer validation is enabled"
#endif

/* 层级边界检查宏 */
#define LV00_ALLOW_LAYER(max_layer) \
    _Static_assert(LV00_CURRENT_LAYER >= (max_layer), \
        "Cross-layer dependency violation: this layer may only call layers >= " #max_layer)

#else
#define LV00_ALLOW_LAYER(max_layer) /* 禁用层级验证时为空操作 */
#endif
```

使用示例（在源文件头部）：

```c
/* 文件: src/core/solver.c (Layer 4) */
#define LV00_CURRENT_LAYER LV00_LAYER_REASONING
#include "lv00_utils.h"   /* Layer 2 — 允许 */
#include "constraint_graph.h" /* Layer 3 — 允许 */
/* 如果误包含 Layer 1 或 Layer 5 的头文件，编译时断言会触发 */
```

---

## 变更记录

| 版本 | 日期 | 变更内容 |
|------|------|---------|
| v3.3 | 2026-05-24 | 初始版本：定义五层架构、文件映射、构建系统集成、层级验证宏 |
| v3.3.1 | 2026-05-25 | Layer 4 新增 16 个核心模块（三值逻辑、模态算子、量词系统、运行时监控、测试框架、几何变换、表达式规范化、SIMD操作、线程池、内存池、有理数、逻辑检查、熔断器、证明优先级、证明轨迹等） |
