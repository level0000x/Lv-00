# Lv-00 架构手册

> **版本**: 1.1.0  
> **最后更新**: 2026-06-27  
> **适用范围**: Lv-00 几何元语言系统完整架构

---

## 目录

1. [架构概述](#1-架构概述)
2. [十层单向依赖架构](#2-十层单向依赖架构)
3. [Shared 公共基础层](#3-shared-公共基础层)
4. [数据流与通信契约](#4-数据流与通信契约)
5. [核心子系统详解](#5-核心子系统详解)
6. [扩展机制](#6-扩展机制)
7. [性能设计](#7-性能设计)
8. [安全与可靠性](#8-安全与可靠性)
9. [构建与部署](#9-构建与部署)

---

## 1. 架构概述

### 1.1 设计哲学

Lv-00 采用**十层单向依赖学术架构**，核心设计原则包括：

| 原则 | 说明 |
|------|------|
| **语义统一性** | 几何对象同时作为程序执行实体、代数计算操作数、逻辑证明对象 |
| **精确性优先** | 采用符号计算而非浮点计算，避免数值精度损失 |
| **层级化架构** | 严格的十层单向依赖，层间通过稳定数据结构通信 |
| **模块化扩展** | 预设模块系统支持领域扩展，公理包支持版本化管理 |

### 1.2 架构全景

```
┌─────────────────────────────────────────────────────────────┐
│ 第5层：输出证明编译层 (Proof Output Compilation Layer)       │
│ 职责：命题格式化、证明链生成、跨语言导出、可视化转换          │
├─────────────────────────────────────────────────────────────┤
│ 第4层：多策略自动推理层 (Multi-strategy Reasoning Layer)     │
│ 职责：正向演绎、反向溯源、代数消元、SMT/ATP 调度             │
├─────────────────────────────────────────────────────────────┤
│ 第3层：约束拓扑规约层 (Constraint Topology Layer)            │
│ 职责：约束图构建、等价节点化简、拓扑归一化、相容检测          │
├─────────────────────────────────────────────────────────────┤
│ 第2层：基础几何公理层 (Foundational Geometry Axiom Layer)    │
│ 职责：原始几何本体、基础度量关系、固有公理库、退化条件        │
├─────────────────────────────────────────────────────────────┤
│ 第1层：词法语法解析层 (Lexical & Syntax Parsing Layer)       │
│ 职责：BNF 文法、词法规则、AST/Typed IR 生成                  │
└─────────────────────────────────────────────────────────────┘
                              ↑
                    Shared 公共基础层
         (错误码、基础类型、内存管理、日志、诊断)
```

---

## 2. 十层单向依赖架构

### 2.1 第1层：词法语法解析层

**职责**: 将 Lv-00 源文本转换为 Token、AST、Typed AST 和 Geometric IR。

#### 2.1.1 功能模块

| 模块 | 描述 |
|------|------|
| `lexer` | 词法分析、Token 生成、源码位置保留 |
| `parser` | 按 BNF 构建 AST |
| `symbol_table` | 名称绑定、作用域、重复声明检查 |
| `type_checker` | 几何实体、度量值、命题、证明对象类型检查 |
| `operator_precedence` | 表达式优先级和结合性规约 |
| `typed_ir` | 输出稳定 Typed IR 供下游层消费 |

#### 2.1.2 关键数据结构

```c
typedef struct Lv00Token {
    TokenType type;
    char *lexeme;
    Lv00SourceSpan span;
} Lv00Token;

typedef struct Lv00AstNode {
    AstNodeType type;
    Lv00SourceSpan span;
    struct Lv00AstNode **children;
    size_t child_count;
    void *annotation;
} Lv00AstNode;

typedef struct Lv00TypedIR {
    IRNodeType type;
    TypeInfo *type_info;
    void *data;
} Lv00TypedIR;
```

#### 2.1.3 依赖规则

- **允许依赖**: `shared`
- **禁止依赖**: 基础几何公理层、约束拓扑层、推理层、输出层

---

### 2.2 第2层：基础几何公理层

**职责**: 定义几何实体、本体、基础度量关系、固有公理库和退化条件。

#### 2.2.1 几何实体类型

| 实体类型 | 数学表示 | 示例 |
|----------|----------|------|
| `Point` | 二维/三维坐标 | `Point(0, 0)` |
| `Line` | 直线/射线/线段 | `Line(A, B)` |
| `Circle` | 圆心和半径 | `Circle(O, r)` |
| `Segment` | 有向线段 | `Segment(A, B)` |
| `Angle` | 角度度量 | `Angle(A, O, B)` |
| `Triangle` | 三角形 | `Triangle(A, B, C)` |
| `Polygon` | 多边形 | `Polygon([A, B, C, D])` |

#### 2.2.2 度量关系

```c
typedef struct Lv00MetricRelation {
    MetricType type;           /* DISTANCE, ANGLE, AREA, RADIUS 等 */
    GeometryEntity *subject;   /* 度量主体 */
    SymbolicCoord *value;      /* 度量值（符号坐标） */
} Lv00MetricRelation;
```

#### 2.2.3 公理系统

| 公理类别 | 描述 | 示例 |
|----------|------|------|
| 欧氏几何公理 | 平面几何基本公理 | 两点确定一条直线 |
| 隶属关系 | 几何对象间关系 | 点在线段上 |
| 退化条件 | 特殊情形声明 | 重合点、共线、零长度 |

#### 2.2.4 依赖规则

- **允许依赖**: `shared`、第1层输出的稳定 Typed IR 类型定义
- **禁止依赖**: parser 实现、约束归一化、求解器、证明输出

---

### 2.3 第3层：约束拓扑规约层

**职责**: 将几何事实与公理关系组织为约束系统，执行拓扑归一化、等价类合并、冗余剔除和相容性检测。

#### 2.3.1 约束图结构

```
        节点: 几何对象 (点、线、圆等)
        边:   约束关系 (距离、角度、共线等)

            A ───[d=5]─── B
            │             │
           [θ=60°]      [θ=60°]
            │             │
            └────[d=5]────┘
                   C
```

#### 2.3.2 关键数据结构

```c
typedef struct Lv00ConstraintGraph {
    GraphNode **nodes;
    GraphEdge **edges;
    size_t node_count;
    size_t edge_count;
    EquivalenceClassManager *equiv_mgr;
} Lv00ConstraintGraph;

typedef struct Lv00Constraint {
    ConstraintType type;
    GeometryEntity **entities;
    size_t entity_count;
    SymbolicCoord *value;      /* 可选的约束值 */
    ConstraintStatus status;
} Lv00Constraint;
```

#### 2.3.3 四态约束状态

| 状态 | 含义 | 处理策略 |
|------|------|----------|
| `CONSISTENT` | 约束相容 | 继续推理 |
| `INCONSISTENT` | 约束矛盾 | 报告矛盾源 |
| `UNDER_CONSTRAINED` | 欠约束 | 提示需要更多信息 |
| `OVER_CONSTRAINED` | 过约束 | 检测冗余约束 |

#### 2.3.4 归一化流程

```
输入: 原始约束图
  ↓
[等价类合并] ──→ 识别并合并等价节点
  ↓
[冗余检测] ──→ 剔除冗余约束
  ↓
[拓扑归一化] ──→ 图结构规范化
  ↓
[相容性检测] ──→ 四态约束状态判定
  ↓
输出: 归一化约束系统
```

#### 2.3.5 依赖规则

- **允许依赖**: `shared`、基础几何公理层
- **禁止依赖**: 多策略推理层、输出证明编译层

---

### 2.4 第4层：多策略自动推理层

**职责**: 基于约束系统、公理库和证明目标执行多策略自动推理，并生成机器可复核 Proof Object。

#### 2.4.1 推理策略矩阵

| 策略 | 适用场景 | 复杂度 |
|------|----------|--------|
| 正向演绎推理 | 从已知事实推出新事实 | O(n) |
| 几何性质推演 | 使用几何定理推导 | O(n²) |
| 代数坐标化简 | 几何条件转代数表达式 | O(d³) |
| Groebner 多项式消元 | 多项式方程组求解 | O(d⁶) |
| 布尔逻辑拆解 | 逻辑结构分解 | O(2ⁿ) |
| 反证归谬法 | 矛盾证明 | 指数级 |
| 局部矛盾闭包溯源 | 追踪矛盾来源 | O(n²) |
| SMT 模态校验 | 一阶逻辑验证 | NP-Complete |

#### 2.4.2 调度原则

1. **类型/语义合法性优先**
2. **公理直接匹配优先于高成本代数后端**
3. **约束归一化优先于搜索**
4. **Groebner 与 SMT 作为后端验证器，不作为所有问题默认第一路径**
5. **反证法必须绑定假设作用域**

#### 2.4.3 证明对象结构

```c
typedef struct Lv00ProofObject {
    Proposition *goal;              /* 证明目标 */
    ProofStep **steps;              /* 证明步骤序列 */
    size_t step_count;
    AssumptionScope *assumptions;   /* 假设作用域 */
    ContradictionTrace *contradiction; /* 矛盾溯源（如适用） */
    ProofStatus status;             /* 证明状态 */
} Lv00ProofObject;

typedef struct Lv00ProofStep {
    size_t step_id;
    Proposition *proposition;
    ProofRule rule;                 /* 使用的证明规则 */
    size_t *premise_ids;            /* 前提步骤 ID */
    Lv00SourceSpan source_span;     /* 来源位置 */
} Lv00ProofStep;
```

#### 2.4.4 依赖规则

- **允许依赖**: `shared`、基础几何公理层、约束拓扑规约层
- **禁止依赖**: 输出证明编译层

---

### 2.5 第5层：输出证明编译层

**职责**: 将 Proof Object 编译为人类可读或机器可处理的输出，不参与推理，不修改内核状态。

#### 2.5.1 功能模块

| 模块 | 描述 |
|------|------|
| `proof_formatting` | 命题、步骤、规则的自然语言格式化 |
| `proof_trace_archive` | 逻辑溯源存档、来源引用、假设域记录 |
| `visualization` | 证明树、约束图、几何图可视化 |
| `cross_language_export` | Lean、Coq、LaTeX、JSON、TikZ 等导出 |
| `stream_output` | 流式事件和增量证明输出 |
| `error_reporting` | 面向用户的诊断输出 |

#### 2.5.2 导出格式支持

| 格式 | 用途 | 状态 |
|------|------|------|
| Lean 4 | 形式化验证 | 已实现 |
| Coq | 形式化验证 | 规划中 |
| LaTeX | 学术论文 | 已实现 |
| TikZ | 几何图形 | 已实现 |
| JSON | 机器交换 | 已实现 |
| Markdown | 文档展示 | 已实现 |

#### 2.5.3 依赖规则

- **允许依赖**: `shared`、基础几何公理层、约束拓扑规约层、多策略自动推理层的只读结果
- **禁止依赖**: parser 实现；禁止回写推理上下文

---

## 3. Shared 公共基础层

`shared` 是跨层公共基础设施，不属于业务十层之一。

### 3.1 允许包含的内容

```
错误码、状态码、source span、诊断、内存池、日志、配置、
基础容器、字符串工具、平台宏
```

### 3.2 禁止包含的内容

```
几何公理、约束归一化、推理策略、Groebner 调度、
SMT 校验、证明格式化
```

### 3.3 核心组件

| 组件 | 功能 | 关键 API |
|------|------|----------|
| `error_codes` | 统一错误码系统 | `lv00_get_last_error()` |
| `memory_pool` | 内存管理 | `lv00_malloc()`, `lv00_free()` |
| `runtime_guard` | 运行时安全守卫 | 熔断机制、超时控制 |
| `context` | 隔离上下文系统 | `LV00Context` |
| `cross_platform` | 跨平台抽象 | 平台检测、编译器适配 |

---

## 4. 数据流与通信契约

### 4.1 完整数据流

```
Source Text (UTF-8 Lv-00 源文本)
  ↓
Token Stream (词法分析)
  ↓
AST (抽象语法树)
  ↓
Typed AST / Geometric IR (类型检查后的 IR)
  ↓
Geometry Ontology + Axiom References (几何本体)
  ↓
Constraint Graph / Normalized Constraint System (约束系统)
  ↓
Reasoning Context / Proof Object (推理上下文)
  ↓
Proof Trace / Export / Visualization (输出)
```

### 4.2 层间契约

#### 4.2.1 解析层输出契约

```
输入：UTF-8 Lv-00 源文本
输出：AST、Typed IR、诊断列表
不得输出：约束归一化结果、证明结果、可视化对象
```

#### 4.2.2 公理层输出契约

```
输入：Typed IR 中的几何实体和关系引用
输出：几何本体对象、公理规则、退化条件
不得输出：搜索结论、证明文本
```

#### 4.2.3 约束层输出契约

```
输入：几何实体、公理关系、约束事实
输出：Constraint Graph、Normalization Result、Constraint Status
不得输出：最终证明文本
```

#### 4.2.4 推理层输出契约

```
输入：Normalized Constraint System、Proof Goal、Reasoning Options
输出：Proof Object、Reasoning Trace、Contradiction Trace
不得输出：LaTeX/TikZ/自然语言最终文本
```

#### 4.2.5 输出层输出契约

```
输入：Proof Object、Proof Trace、Visualization Model
输出：Text、JSON、LaTeX、TikZ、Lean、Coq 等格式
不得修改：Reasoning Context、Constraint Graph、Axiom Package
```

---

## 5. 核心子系统详解

### 5.1 符号坐标系统

Lv-00 使用精确的符号表示而非浮点数：

| 类型 | 数学表示 | 实现方式 | 精度 |
|------|----------|----------|------|
| `RATIONAL` | a/b | GMP `mpq_t` | 任意精度 |
| `ALGEBRAIC` | 整系数多项式实根 | 表达式树 + 隔离区间 | 符号精确 |
| `QUADRATIC` | a + b*sqrt(n) | 二次扩张结构 | 符号精确 |
| `TRANSCENDENTAL` | pi, e 等 | 符号表达式 | 符号精确 |

### 5.2 求解引擎

| 引擎 | 算法 | 适用问题 |
|------|------|----------|
| Groebner 基 | Buchberger 算法 | 多项式理想求解 |
| SMT 后端 | DPLL(T) | 一阶逻辑求解 |
| ATP 接口 | 归结/表列 | 自动定理证明 |
| SAT/BDD | CDCL | 命题逻辑可满足性 |

### 5.3 证明系统

- **多策略引擎**: 支持 8 种证明策略
- **约束传播**: WFC 风格弧相容算法
- **证明导出**: Lean/Coq 脚本生成

---

## 6. 扩展机制

### 6.1 预设模块系统

```c
// 加载预设模块
lv00_preset_load(ctx, "euclidean_geometry");

// 应用预设定理
Proposition *prop = lv00_preset_apply(ctx, "pythagorean_theorem", A, B, C);
```

### 6.2 公理包系统

```c
// 加载公理包
AxiomPackage *pkg = lv00_axiom_pkg_load("algebraic_geometry");

// 附加到引擎
lv00_engine_attach_axiom_pkg(engine, pkg);
```

### 6.3 函数块系统

```c
// 定义函数块
FuncBlock *midpoint = lv00_fb_create("midpoint", 2);
lv00_fb_define(midpoint, "return point((A.x+B.x)/2, (A.y+B.y)/2);");

// 实例化
Point *M = lv00_fb_apply(midpoint, A, B);
```

---

## 7. 性能设计

### 7.1 优化策略

| 策略 | 实现 | 效果 |
|------|------|------|
| SIMD 友好存储 | 对齐数据结构 | 向量化运算加速 |
| LRU 对象缓存 | 内存池管理 | 减少分配开销 |
| 多核并行调度 | 线程池 | 并行推理 |
| 增量归一化 | 变更追踪 | 避免全量重算 |

### 7.2 复杂度分析

| 操作 | 时间复杂度 | 空间复杂度 |
|------|-----------|-----------|
| 约束图构建 | O(n) | O(n) |
| 等价类合并 | O(α(n)) | O(n) |
| 归一化迭代 | O(k·n) | O(n) |
| Groebner 基 | O(d⁶) | O(d⁴) |
| 证明搜索 | 指数级 | 指数级 |

---

## 8. 安全与可靠性

### 8.1 严格规则

| 编号 | 规则 | 说明 |
|------|------|------|
| R1 | 下层禁止 include 上层头文件 | 防止反向耦合 |
| R2 | 下层禁止调用上层函数 | 防止运行时反向依赖 |
| R3 | 输出层禁止修改推理上下文 | 输出只读 |
| R4 | 解析层禁止执行证明或求解 | 只生成 AST/Typed IR |
| R5 | 公理层禁止执行搜索 | 只定义本体、公理、前置条件 |
| R6 | 约束层禁止格式化证明 | 只处理约束系统 |
| R7 | 推理层禁止生成最终展示格式 | 只生成 Proof Object |
| R8 | shared 禁止承载数学业务逻辑 | 保持基础设施纯净 |

### 8.2 运行时保护

- **熔断机制**: 防止无限循环和过度资源消耗
- **超时控制**: 可配置的计算超时
- **内存限制**: 软内存上限控制
- **断言检查**: 开发期不变量验证

---

## 9. 构建与部署

### 9.1 目录结构

```
Lv-00/
├── core/
│   ├── include/lv00/          # 公共 API 头文件
│   ├── src/layer1_parser/     # 词法语法解析层
│   ├── src/layer2_axiom/      # 基础几何公理层
│   ├── src/layer3_constraint/ # 约束拓扑规约层
│   ├── src/layer4_reasoning/  # 多策略自动推理层
│   ├── src/layer5_output/     # 输出证明编译层
│   └── src/shared/            # 公共基础层
├── lv00-formal/               # Lean 形式化验证
├── doc/docs/                  # 技术文档
├── tests/                     # 测试套件
└── module/axiom_packages/     # 公理包库
```

### 9.2 CMake Target 规划

```cmake
lv00_shared          # 公共基础层
lv00_layer1_parser   # 词法语法解析层
lv00_layer2_axiom    # 基础几何公理层
lv00_layer3_constraint # 约束拓扑规约层
lv00_layer4_reasoning  # 多策略自动推理层
lv00_layer5_output     # 输出证明编译层
```

### 9.3 构建检查

```bash
cmake -S . -B build_verify -DENABLE_LAYER_VALIDATION=ON
cmake --build build_verify
ctest --test-dir build_verify --output-on-failure
```

---

## 参考文档

- [API 快速入门](API_QUICKSTART.md)
- [API 使用指南](API_USAGE_GUIDE.md)
- [语言规范](LV00_LANGUAGE_SPEC.md)
- [编码标准](CODING_STANDARD_v3.4.2.md)
- [贡献指南](../../CONTRIBUTING.md)
