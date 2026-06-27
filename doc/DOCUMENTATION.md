# Lv-00 几何元语言 -- 完整技术文档

> **版本**: 1.1.0
> **架构版本**: 十层单向依赖架构
> **发布日期**: 2026-06-27
> **许可证**: MIT

---

## 目录

1. [概述](#1-概述)
2. [设计理念](#2-设计理念)
3. [领域定位](#3-领域定位)
4. [十层架构](#4-十层架构)
5. [核心概念一览](#5-核心概念一览)
6. [项目结构](#6-项目结构)
7. [核心能力摘要](#7-核心能力摘要)
8. [技术要求](#8-技术要求)
9. [快速开始](#9-快速开始)
10. [基础用法示例](#10-基础用法示例)
11. [符号坐标系](#11-符号坐标系)
12. [约束图](#12-约束图)
13. [代数数与精确算术](#13-代数数与精确算术)
14. [证明系统与命题](#14-证明系统与命题)
15. [Groebner 基引擎](#15-groebner-基引擎)
16. [SMT 与 SAT 编码](#16-smt-与-sat-编码)
17. [重写与合一](#17-重写与合一)
18. [功能块系统](#18-功能块系统)
19. [公理包与预设](#19-公理包与预设)
20. [插件系统](#20-插件系统)
21. [Lean 4 形式化概述](#21-lean-4-形式化概述)
22. [本体与原始谓词](#22-本体与原始谓词)
23. [约束图规范化证明](#23-约束图规范化证明)
24. [Python 绑定与 DSL](#24-python-绑定与-dsl)
25. [Web GUI 与流监视器](#25-web-gui-与流监视器)
26. [配置参考](#26-配置参考)

---

## 1. 概述

### 1.1 什么是 Lv-00

Lv-00 是一门以几何为唯一载体的双模数学元语言。几何体本身是计算的执行者、数据的承载者、证明的见证者。

**核心定位**:

```
上层应用（CGAL / CAD / AI求解器 / 教育工具）
        ↑ 它们需要精确语义
   Lv-00：几何元语言（提供精确语义）
        ↑ 它们提供形式化基础
底层框架（Lean / Coq / 一阶逻辑 / 约束求解）
```

- **不是** CGAL 那种供人调用的算法包
- **不是** AlphaGeometry 那种解题 AI
- **不是** LeanGeo 那种依附于外部证明器的数学库
- **而是** 一种同时完成构造、计算、证明的语言本身

### 1.2 版本历史

| 版本 | 日期 | 架构 | 主要变更 |
|------|------|------|----------|
| v3.5.0 | 2026-05-29 | 五层架构 | 学术化整改、函数块增强、55+ 预设模块 |
| v4.0.0 | 2026-05-30 | 五层架构 | 预设函数块增强、Lean 4 框架完善 |
| v5.0.0 | 2026-06-04 | 十层架构 | 从五层扩展为十层，新增 L6-L10 |

### 1.3 v5.0.0 核心变更

**架构扩展**: 从五层架构扩展为十层架构（新增 Visual、Orchestration、Meta-Verification、Application、Interop）。

**L4 推理层增强**:
- 实现 CDCL SAT 求解器（传播、冲突分析、回跳、学习、重启）
- 实现 SMT 后端（Groebner 基、Z3/cvc5 子进程集成）
- 实现 ATP 后端（Vampire/EProver/iProver 子进程集成）
- 实现 BDD 编码（唯一表、计算表、Tseitin CNF 变换）
- 实现 GMRES/BiCGSTAB/CG 迭代求解器
- 实现不等式推理（AM-GM、Cauchy-Schwarz、Jensen、SOS）
- 实现概率约束（DTMC + PCTL 评估）
- 实现多策略证明引擎（8 种策略 + 4 种搜索算法）
- 实现 Groebner 并行引擎（Buchberger + work-stealing）

**L6-L10 新增层**:
- L6 Visual: 节点图、几何画布、积木画布、四视图同步
- L7 Orchestration: 六阶段流水线调度
- L8 Meta-Verification: 五维元验证检查
- L9 Application: 批处理与交互式 REPL
- L10 Interop: Lean 4/Coq/OPML 双向桥接

---

## 2. 设计理念

### 2.1 核心设计原则

| 原则 | 说明 |
|------|------|
| **语义统一性** | 几何对象同时作为程序执行实体、代数计算操作数、逻辑证明对象 |
| **精确性优先** | 采用符号计算而非浮点计算，避免数值精度损失 |
| **层级化架构** | 严格的十层单向依赖，层间通过稳定数据结构通信 |
| **模块化扩展** | 预设模块系统支持领域扩展，公理包支持版本化管理 |
| **构造即证明** | 几何构造通过合一检查验证是否满足命题，遵循 BHK 解释 |
| **公理中立** | 约束图内核不内建距离、角度概念，这些由公理系统包定义 |

### 2.2 声明式建模 + 命令式演算双重特性

Lv-00 具有双重语言特性：

- **声明式部分**: 用于描述几何实体、约束、公理、目标命题
- **命令式部分**: 用于触发计算、归一化、证明、导出等过程

### 2.3 几何实体为一等公民对象

点、线、圆、线段、射线、角、三角形、多边形等几何实体可以被声明、绑定、传参、作为命题对象引用。几何实体不是普通数值的别名，而是具有独立语义域的对象。

### 2.4 强类型形式化专用元语言

- 所有标识符必须绑定到确定类型
- 几何对象、度量值、逻辑命题、证明对象不可隐式混用
- 类型错误必须在进入约束拓扑规约层前被拒绝

---

## 3. 领域定位

### 3.1 生态定位图

```
┌─────────────────────────────────────────────────────────────┐
│  应用层                                                      │
│  CGAL / OpenCASCADE / CAD 软件 / AI 几何求解器 / 教育工具    │
├─────────────────────────────────────────────────────────────┤
│  元语言层                                                    │
│  Lv-00: 几何元语言（精确语义、构造+计算+证明统一）            │
├─────────────────────────────────────────────────────────────┤
│  形式化基础层                                                │
│  Lean 4 / Coq / Isabelle / 一阶逻辑 / 约束求解              │
├─────────────────────────────────────────────────────────────┤
│  计算基础层                                                  │
│  GMP / MPFR / 多精度算术 / 符号计算                          │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 与同类系统的区别

| 系统 | 类型 | 与 Lv-00 的区别 |
|------|------|-----------------|
| GeoGebra | 交互式几何工具 | 不具备符号精确性和形式化证明能力 |
| LeanGeo | Lean 几何库 | 依附于 Lean 证明器，非独立语言 |
| AlphaGeometry | AI 几何解题 | 不可编程，无用户可控的推理过程 |
| CGAL | 几何算法库 | 纯 C++ 库，无语言层面统一 |
| SymPy | 符号计算库 | 非几何原生，几何为附加模块 |
| Cinderella | 交互式几何 | 有几何计算但无形式化证明 |
| JGEX | 几何证明系统 | 有证明但无符号精确坐标和代数推理 |

### 3.3 目标用户

1. **数学研究者**: 需要形式化几何推理和精确计算
2. **计算机代数系统开发者**: 需要几何原生的精确计算后端
3. **形式化验证工程师**: 需要与 Lean/Coq 互操作的几何证明
4. **教育工作者**: 需要可交互、可视化的几何教学工具
5. **AI 几何研究者**: 需要可编程的几何推理基准

---

## 4. 十层架构

### 4.1 架构全景

```
┌─────────────────────────────────────────────────────────────┐
│ Layer 10: 外部集成层 (Interop)                               │
│ 职责：Lean 4 桥接、Coq 桥接、OPML 编解码                    │
├─────────────────────────────────────────────────────────────┤
│ Layer 9: 应用入口层 (Application)                            │
│ 职责：批处理、交互式 REPL                                    │
├─────────────────────────────────────────────────────────────┤
│ Layer 8: 元验证层 (Meta-Verification)                       │
│ 职责：类型一致性、完整性、健全性、非平凡性、往返验证          │
├─────────────────────────────────────────────────────────────┤
│ Layer 7: 编排调度层 (Orchestration)                          │
│ 职责：六阶段流水线调度、会话编排                              │
├─────────────────────────────────────────────────────────────┤
│ Layer 6: 图形化编程层 (Visual)                               │
│ 职责：可视化编辑器、节点图、几何画布、积木画布、四视图同步    │
├─────────────────────────────────────────────────────────────┤
│ Layer 5: 结果输出层 (Output)                                │
│ 职责：流式输出、TikZ/SVG/Cairo/Three.js/PPM 渲染、证明可视化 │
├─────────────────────────────────────────────────────────────┤
│ Layer 4: 公理推理层 (Reasoning)                             │
│ 职责：引擎、求解器、证明、重写、合一、SMT/SAT/ATP/BDD/Groebner│
├─────────────────────────────────────────────────────────────┤
│ Layer 3: 几何拓扑层 (Geometry)                               │
│ 职责：约束图、符号坐标、几何原语、高维结构、拓扑、WFC 范式   │
├─────────────────────────────────────────────────────────────┤
│ Layer 2: 资源管理层 (Resource)                               │
│ 职责：内存分配、错误码、调试、缓存、全局状态、中文本地化      │
├─────────────────────────────────────────────────────────────┤
│ Layer 1: 输入解析层 (Parser)                                │
│ 职责：词法分析、公式解析、DSL 编译、数学输入                  │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 依赖关系表

| 层级 | 名称 | 可依赖项 |
|------|------|----------|
| Layer 1 | Parser | 仅 Layer 2 |
| Layer 2 | Resource | 无上层（基础层）|
| Layer 3 | Geometry | 仅 Layer 2 |
| Layer 4 | Reasoning | Layer 2, Layer 3 |
| Layer 5 | Output | Layer 2, Layer 3, Layer 4 |
| Layer 6 | Visual | Layer 2, Layer 3, Layer 5 |
| Layer 7 | Orchestration | Layer 2, Layer 3, Layer 4, Layer 5, Layer 6 |
| Layer 8 | Meta-Verification | Layer 2, Layer 3, Layer 4 |
| Layer 9 | Application | 所有层 |
| Layer 10 | Interop | Layer 2, Layer 4, Layer 5 |

**依赖方向**: Layer 10 -> Layer 9 -> ... -> Layer 2，Layer 1 依赖 Layer 2。

### 4.3 各层职责详述

#### Layer 1: 输入解析层 (Parser)

**职责**: 将 Lv-00 源文本转换为 Token、AST、Typed AST 和 Geometric IR。

| 模块 | 描述 |
|------|------|
| `lexer` | 词法分析、Token 生成、源码位置保留 |
| `parser` | 按 BNF 构建 AST |
| `symbol_table` | 名称绑定、作用域、重复声明检查 |
| `type_checker` | 几何实体、度量值、命题、证明对象类型检查 |
| `operator_precedence` | 表达式优先级和结合性规约 |
| `typed_ir` | 输出稳定 Typed IR 供下游层消费 |
| `math_input` | 数学符号输入处理 |

**依赖规则**: 仅允许依赖 Layer 2（Resource），禁止依赖任何上层。

#### Layer 2: 资源管理层 (Resource)

**职责**: 提供跨层公共基础设施，不属于业务层。

| 组件 | 功能 |
|------|------|
| `error_codes` | 统一错误码系统（分层 0-999） |
| `memory_pool` | 自定义内存分配器 |
| `runtime_guard` | 运行时安全守卫（熔断机制、超时控制） |
| `context` | 隔离上下文系统 |
| `cross_platform` | 跨平台抽象 |
| `cache_manager` | LRU 对象缓存 |
| `global_state` | 全局状态管理 |
| `debug_trace` | 调试追踪系统 |
| `config` | 集中化配置系统（`LV00_CONFIG_*` 前缀） |
| `error_messages_cn` | 中文错误消息本地化 |

**禁止包含**: 几何公理、约束归一化、推理策略、Groebner 调度、SMT 校验、证明格式化。

#### Layer 3: 几何拓扑层 (Geometry)

**职责**: 约束图构建、等价节点化简、拓扑归一化、相容检测、几何原语、高维结构。

| 模块 | 描述 |
|------|------|
| `constraint_graph` | 约束图核心数据结构 |
| `symbolic_coord` | 符号坐标系统 |
| `euclidean_geometry` | 欧氏几何原语 |
| `high_dim` | 高维几何结构 |
| `interactive_geo` | 交互式几何 |
| `geometry_compress` | 几何压缩 |
| `geo_event_detect` | 几何事件检测 |
| `geometry_transform` | 几何变换（平移、旋转、缩放、反射） |
| `geo_topology` | 拓扑结构 |
| `interval_arithmetic` | 区间算术 |
| `geom_evol` | 几何演化（ODE 求解器集成） |
| `geo_visual` / `geo_visual_simple` | 几何可视化 |
| `geo_aabb_tree` | AABB 碰撞检测树 |
| `geo_halfedge_mesh` | 半边网格数据结构 |
| `geo_constraint_solver` | 几何约束求解器 |
| `geo_dynamic` | 动态几何 |
| `geo_predicate` | 几何谓词 |
| `geo_spec` | 几何规约 |
| `geo_metalogic` | 几何元逻辑 |
| `geo_invariant_type` | 几何不变量类型 |

**依赖规则**: 仅允许依赖 Layer 2。

#### Layer 4: 公理推理层 (Reasoning)

**职责**: 基于约束系统、公理库和证明目标执行多策略自动推理，生成机器可复核 Proof Object。

| 模块 | 描述 |
|------|------|
| `engine` | 核心推理引擎 |
| `engine_scheduler` | 引擎调度器 |
| `proof` / `proof_engine_enhanced` | 证明引擎 |
| `proof_search` | 证明搜索算法 |
| `proof_session` | 证明会话管理 |
| `proof_trace` | 证明追踪 |
| `proof_output` | 证明输出 |
| `proof_proposition` | 命题系统 |
| `proof_rule_engine` | 证明规则引擎 |
| `proof_compiler` | 证明编译器 |
| `proof_contradiction` | 矛盾证明 |
| `proof_navigator` | 证明导航器 |
| `proof_score` | 证明评分 |
| `proof_priority` | 证明优先级 |
| `proof_version` | 证明版本管理 |
| `proof_export_enhanced` | 增强证明导出 |
| `proof_widget` | 证明组件 |
| `rewrite` / `rewrite_strategy` | 重写引擎 |
| `normalization` | 归一化引擎 |
| `groebner_engine` / `groebner_parallel` | Groebner 基引擎 |
| `atp_backend` | ATP 后端（Vampire/EProver/iProver） |
| `bdd_encoding` | BDD 编码 |
| `smt_backend` | SMT 后端 |
| `func_block` / `func_block_internal` | 函数块系统 |
| `func_block_registry` | 函数块注册表 |
| `func_block_preset` / `func_block_preset_ops` | 预设函数块 |
| `axiom_pkg` / `axiom_rule_engine` | 公理包系统 |
| `axiom_grade` | 公理分级 |
| `equiv_class` | 等价类管理 |
| `quantifier` | 量词系统 |
| `modal_operators` | 模态逻辑算子 |
| `recursion` | 递归系统 |
| `logic_check` | 逻辑检查 |
| `prop_verifier` | 命题验证器 |
| `propagation` | 约束传播 |
| `conflict_detector` | 冲突检测 |
| `reasoning_cache` | 推理缓存 |
| `meta_proof` / `meta_repr` / `meta_verify` | 元证明系统 |
| `formula_parser` / `formula_converter` / `formula_renderer` | 公式处理 |
| `dsl_compiler` | DSL 编译器 |
| `ga_codegen` / `ga_interface` / `ga_multivector` | 几何代数 |
| `autodiff` | 自动微分 |
| `ode_solver` | ODE 求解器 |
| `inequality_reasoning` | 不等式推理 |
| `probabilistic_constraint` | 概率约束 |
| `numerical_backend` | 数值后端 |
| `exact_arithmetic` | 精确算术 |
| `algebraic_number` | 代数数 |
| `interval_arithmetic` | 区间算术 |
| `rational` | 有理数 |
| `mpz_poly` | 多精度多项式 |
| `preset_*` | 55+ 数学理论预设模块 |

**依赖规则**: 允许依赖 Layer 2 和 Layer 3。

#### Layer 5: 结果输出层 (Output)

**职责**: 将 Proof Object 编译为人类可读或机器可处理的输出，不参与推理，不修改内核状态。

| 模块 | 描述 |
|------|------|
| `stream_output` | 流式事件和增量证明输出 |
| `proof_output` | 证明格式化与输出 |
| `geo_visual` | 几何可视化（SVG、Cairo、TikZ、Three.js、PPM） |
| `interop` | 互操作接口 |
| `magic` | Magic 模拟器 |
| `representation_converter` | 表示转换器 |

**导出格式支持**:

| 格式 | 用途 | 状态 |
|------|------|------|
| Lean 4 | 形式化验证 | 已实现 |
| Coq | 形式化验证 | 已实现 |
| LaTeX | 学术论文 | 已实现 |
| TikZ | 几何图形 | 已实现 |
| SVG | 网页渲染 | 已实现 |
| Cairo | Cairo 脚本 | 已实现 |
| Three.js | 3D 场景 | 已实现 |
| PPM | 光栅化像素 | 已实现 |
| JSON | 机器交换 | 已实现 |
| OPML | 开放证明交换 | 已实现 |

**依赖规则**: 允许依赖 Layer 2、Layer 3、Layer 4 的只读结果。

#### Layer 6: 图形化编程层 (Visual)

**职责**: 提供可视化编程环境，支持四视图同步编辑。

| 源文件 | 描述 |
|--------|------|
| `node_graph.c` | 节点图引擎（力导向布局） |
| `geometry_canvas.c` | 几何画布引擎 |
| `block_canvas.c` | 积木画布引擎 |
| `block_scheduler.c` | 块调度器（拓扑排序 + 增量执行） |
| `visual_editor.c` | 可视化编辑器核心 |
| `block_to_text.c` | 积木块 -> 文本代码转换 |
| `block_to_node.c` | 积木块 -> 节点图转换 |
| `block_to_geometry.c` | 积木块 -> 几何画布转换 |
| `sync_protocol.c` | 四视图同步协议 |
| `extended_types.c` | 类型系统扩展（List、Map、IO 等） |

**四大子系统**:

1. **多范式可视化编辑器**: 几何画布、节点图、积木画布、文本代码四种视图
2. **函数块运行时扩展**: 通用控制流块（If/While/For/Match）、IO 交互块、数据结构块
3. **类型系统扩展**: 通用类型区域、IO 类型系统、效果追踪（Pure/IO/State）
4. **表示转换层**: 函数块 <-> 节点图 <-> 几何画布 <-> 积木块 <-> 文本代码

**依赖规则**: 允许依赖 Layer 2、Layer 3、Layer 5。

#### Layer 7: 编排调度层 (Orchestration)

**职责**: 六阶段流水线调度，协调各层协同工作。

**六阶段流水线**:

```
阶段 1: 解析验证 (Parse Validation)
  ↓
阶段 2: 资源计算 (Resource Calculation)
  ↓
阶段 3: 几何关键词扫描 (Geometry Keyword Scanning)
  ↓
阶段 4: 证明引擎调用 (Proof Engine Call)
  ↓
阶段 5: 输出生成 (Output Generation)
  ↓
阶段 6: 可视化参数 (Visual Parameters)
```

| 阶段 | 输入 | 输出 | 失败处理 |
|------|------|------|----------|
| 1. 解析验证 | 源文本 | Typed IR + 诊断 | 报告语法/类型错误 |
| 2. 资源计算 | Typed IR | 资源分配计划 | 报告资源不足 |
| 3. 几何关键词扫描 | Typed IR | 几何实体映射 | 标记未识别实体 |
| 4. 证明引擎调用 | 约束系统 + 目标 | Proof Object | 返回 UNKNOWN 或矛盾 |
| 5. 输出生成 | Proof Object | 多格式输出 | 降级为纯文本 |
| 6. 可视化参数 | Proof Object + 输出 | 视觉参数集 | 跳过可视化 |

**依赖规则**: 允许依赖 Layer 2、Layer 3、Layer 4、Layer 5、Layer 6。

#### Layer 8: 元验证层 (Meta-Verification)

**职责**: 对推理结果进行五维度元验证，确保证明质量。

**五维检查**:

| 维度 | 名称 | 检查内容 | 通过条件 |
|------|------|----------|----------|
| 1 | 类型一致性 (Type Consistency) | 所有中间结果的类型是否一致 | 类型推导无矛盾 |
| 2 | 完整性 (Completeness) | 证明是否覆盖所有必要步骤 | 无遗漏的证明义务 |
| 3 | 健全性 (Soundness) | 证明步骤是否逻辑正确 | 每步推导有效 |
| 4 | 非平凡性 (Nontriviality) | 证明是否非平凡（非空证明） | 至少包含一个实质性步骤 |
| 5 | 往返可解析性 (Roundtrip) | 导出后重新导入是否等价 | 双向转换无损 |

**验证流程**:

```
Proof Object
  ↓
[类型一致性检查] ──→ 失败 → 报告类型矛盾
  ↓ 通过
[完整性检查] ──→ 失败 → 报告遗漏义务
  ↓ 通过
[健全性检查] ──→ 失败 → 报告无效推导
  ↓ 通过
[非平凡性检查] ──→ 失败 → 标记为平凡证明
  ↓ 通过
[往返检查] ──→ 失败 → 报告导出损失
  ↓ 通过
验证通过 ✓
```

**依赖规则**: 允许依赖 Layer 2、Layer 3、Layer 4。

#### Layer 9: 应用入口层 (Application)

**职责**: 提供用户交互入口。

| 模式 | 描述 |
|------|------|
| 批处理 (Batch) | 读取 `.lv00` 文件，执行完整流水线，输出结果 |
| 交互式 REPL | 逐行输入 Lv-00 语句，即时反馈 |

**REPL 特性**:
- 支持多行输入（括号匹配）
- 支持上下文保持（变量、约束跨行累积）
- 支持 `:help`、`:load`、`:export`、`:clear` 等元命令
- 支持自动补全（关键字、函数块名、预设模块名）

**依赖规则**: 允许依赖所有层。

#### Layer 10: 外部集成层 (Interop)

**职责**: 与外部形式化系统和格式进行双向桥接。

| 桥接 | 源文件 | 描述 |
|------|--------|------|
| Coq Bridge | `coq_bridge.c` | Lv-00 <-> Coq vernacular 双向转换 |
| Lean 4 Bridge | `lean4_bridge.c` | Lv-00 <-> Lean 4 tactic 脚本双向转换 |
| OPML Codec | `opml_codec.c` | OPML 开放证明交换格式编解码 |

**OPML (Open Proof Markup Language)**:

OPML 是 Lv-00 项目的开放证明交换格式，基于 JSON 编码，支持：
- Lv-00、Lean 4、Coq、Isabelle/HOL、HOL4、Agda 之间的证明导入导出
- 理论定义（原语、公理、定义、定理）
- 证明结构（步骤、规则、前提引用）
- 扩展命名空间

**依赖规则**: 允许依赖 Layer 2、Layer 4、Layer 5。

### 4.4 层间通信契约

每层通过稳定的数据结构进行通信，严格禁止越层调用。

#### 4.4.1 解析层输出契约

```
输入：UTF-8 Lv-00 源文本
输出：AST、Typed IR、诊断列表
不得输出：约束归一化结果、证明结果、可视化对象
```

#### 4.4.2 几何层输出契约

```
输入：Typed IR 中的几何实体和关系引用
输出：Constraint Graph、Normalization Result、Constraint Status
不得输出：最终证明文本、可视化对象
```

#### 4.4.3 推理层输出契约

```
输入：Normalized Constraint System、Proof Goal、Reasoning Options
输出：Proof Object、Reasoning Trace、Contradiction Trace
不得输出：LaTeX/TikZ/自然语言最终文本
```

#### 4.4.4 输出层输出契约

```
输入：Proof Object、Proof Trace、Visualization Model
输出：Text、JSON、LaTeX、TikZ、Lean、Coq 等格式
不得修改：Reasoning Context、Constraint Graph、Axiom Package
```

#### 4.4.5 可视化层输出契约

```
输入：Proof Object、几何实体、函数块图
输出：SVG、节点图、积木画布、文本代码视图
不得修改：推理上下文、约束图内部状态
```

#### 4.4.6 编排层输出契约

```
输入：源文本或 API 调用
输出：完整的六阶段流水线执行结果
协调：按顺序调用 L1-L6 各层，收集中间结果
```

#### 4.4.7 元验证层输出契约

```
输入：Proof Object（来自 L4）
输出：五维验证报告（通过/失败 + 详细信息）
不得修改：原始 Proof Object
```

#### 4.4.8 应用层输出契约

```
输入：命令行参数或 REPL 输入
输出：格式化结果、交互反馈
协调：调用 L7 编排器执行完整流水线
```

#### 4.4.9 互操作层输出契约

```
输入：Lv-00 Proof Object / 外部格式文件
输出：目标格式的翻译结果
保证：双向转换的语义等价性
```

### 4.5 安全与可靠性规则

| 编号 | 规则 | 说明 |
|------|------|------|
| R1 | 下层禁止 include 上层头文件 | 防止反向耦合 |
| R2 | 下层禁止调用上层函数 | 防止运行时反向依赖 |
| R3 | 输出层禁止修改推理上下文 | 输出只读 |
| R4 | 解析层禁止执行证明或求解 | 只生成 AST/Typed IR |
| R5 | 公理层禁止执行搜索 | 只定义本体、公理、前置条件 |
| R6 | 约束层禁止格式化证明 | 只处理约束系统 |
| R7 | 推理层禁止生成最终展示格式 | 只生成 Proof Object |
| R8 | Resource 禁止承载数学业务逻辑 | 保持基础设施纯净 |
| R9 | 图形化编辑器不得绕过函数块系统直接创建几何实体 | 保持封装性 |
| R10 | 通用编程块不得破坏几何块的确定性保证 | 保持确定性 |
| R11 | 表示转换必须保持语义等价（双向可逆） | 保持一致性 |

### 4.5 数据流与通信契约

```
Source Text (UTF-8 Lv-00 源文本)
  ↓
Token Stream (词法分析)                    [Layer 1]
  ↓
AST (抽象语法树)                          [Layer 1]
  ↓
Typed AST / Geometric IR (类型检查后的 IR) [Layer 1]
  ↓
Geometry Ontology + Axiom References       [Layer 3]
  ↓
Constraint Graph / Normalized System      [Layer 3]
  ↓
Reasoning Context / Proof Object           [Layer 4]
  ↓
Proof Trace / Export / Visualization      [Layer 5]
  ↓
Visual Parameters / Interactive Feedback   [Layer 6]
  ↓
Pipeline Orchestration                    [Layer 7]
  ↓
Meta-Verification                         [Layer 8]
  ↓
User Interaction (Batch/REPL)            [Layer 9]
  ↓
External System Bridge (Lean/Coq/OPML)    [Layer 10]
```

---

## 5. 核心概念一览

### 5.1 符号坐标 (Symbolic Coordinate)

精确表示几何体坐标值，支持有理数、代数数、二次根式和超越常数。

### 5.2 约束图 (Constraint Graph)

几何对象（点、线段、区域）及其约束关系的基础数据结构。

### 5.3 归一化 (Normalization)

Weisfeiler-Lehman 图核迭代归一化，自动合并等价节点，保证幂等性。

### 5.4 合一 (Unification)

验证几何构造是否满足命题模式。

### 5.5 函数块 (Function Block)

封装内部约束子图的复合节点，支持打包、实例化、部分应用和组合子。

### 5.6 命题 (Proposition)

几何命题的模式定义，包含输入/输出端口、几何模式、前置/后置条件。

### 5.7 证明对象 (Proof Object)

机器可复核的证明结构，包含证明步骤序列、规则引用、假设作用域。

### 5.8 信任颜色 (Trust Color)

| 颜色 | 含义 |
|------|------|
| Green | 纯构造性证明 |
| Blue | 未探索/资源受限/超出范围 |
| Yellow | 条件性不可构造 |
| Amber | 含数值假设 |
| Orange Oracle | 依赖非构造性 oracle |
| Orange Ex Falso | 爆炸原理步骤 |
| Dark Orange | 非构造性依赖与数值假设叠加 |

### 5.9 公理包 (Axiom Package)

版本化管理的公理集合，支持 SHA-256 校验和模板展开。

### 5.10 预设模块 (Preset Module)

55+ 数学理论预设，涵盖几何、代数、拓扑、逻辑、分析等领域。

---

## 6. 项目结构

### 6.1 顶层目录

```
Lv-00/
├── core/                        # 核心引擎
│   ├── include/lv00/            # 公共 API 头文件 (120+)
│   └── src/                     # 源代码（十层架构）
├── formal/                      # Lean 4 形式化验证
│   ├── Lv00/                    # 形式化理论
│   ├── tests/                   # 形式化测试
│   ├── lakefile.lean            # Lake 构建文件
│   └── lean-toolchain           # Lean 工具链
├── lv00-formal/                 # 独立 Lean 4 项目
├── doc/                         # 文档
│   ├── docs/                    # 技术文档 (35+)
│   ├── papers/                  # 学术论文
│   ├── reference/              # 参考项目分析 (73+)
│   └── reports/                 # 任务报告
├── examples/                    # 示例
│   ├── library/                 # 几何示例库
│   ├── plugin_example/          # 插件示例
│   └── templates/               # 模板
├── module/                      # 模块
│   ├── axiom_packages/          # 公理包库 (60+)
│   ├── python/                  # Python 绑定
│   ├── stream_bridge/           # 流桥接
│   ├── llm_coding_assistant/   # LLM 编码助手
│   └── concurrent_monitor/     # 并发监控
├── web/gui/                     # Web GUI (Tauri)
├── cmake/                       # CMake 配置
├── .github/workflows/          # CI/CD
├── CMakeLists.txt               # 主构建文件
├── CHANGELOG.md                 # 变更记录
├── VERSION_LOG.md               # 版本日志
├── CONTRIBUTING.md              # 贡献指南
├── LICENSE                      # MIT 许可证
└── README.md                    # 项目说明
```

### 6.2 核心源码目录 (十层架构)

```
core/src/
├── layer1_parser/              # Layer 1: 输入解析层
│   ├── lexer.c                 #   词法分析器
│   ├── parser.c                #   语法分析器
│   ├── symbol_table.c          #   符号表
│   ├── type_checker.c          #   类型检查器
│   ├── operator_precedence.c    #   运算符优先级
│   ├── typed_ir.c              #   类型化 IR
│   └── math_input.c            #   数学输入处理
├── layer2_resource/            # Layer 2: 资源管理层
│   ├── error_codes.c           #   统一错误码
│   ├── memory_pool.c           #   内存池
│   ├── runtime_guard.c         #   运行时守卫
│   ├── context.c               #   上下文管理
│   ├── cache_manager.c          #   缓存管理
│   ├── global_state.c          #   全局状态
│   ├── config.c                #   配置系统
│   ├── debug_trace.c           #   调试追踪
│   ├── cross_platform.c        #   跨平台抽象
│   ├── node_deep_copy.c        #   深拷贝
│   ├── fast_index.c            #   快速索引
│   ├── graph_hash.c            #   图哈希
│   ├── performance_profiler.c  #   性能分析器
│   ├── circuit_breaker.c       #   熔断器
│   └── error_messages_cn.c     #   中文错误消息
├── layer3_geometry/            # Layer 3: 几何拓扑层
│   ├── constraint_graph.c      #   约束图核心
│   ├── symbolic_coord.c         #   符号坐标
│   ├── euclidean_geometry.c     #   欧氏几何
│   ├── high_dim.c              #   高维几何
│   ├── interactive_geo.c       #   交互几何
│   ├── geometry_compress.c    #   几何压缩
│   ├── geo_event_detect.c       #   事件检测
│   ├── geometry_transform.c    #   几何变换
│   ├── geo_topology.c           #   拓扑
│   ├── interval_arithmetic.c   #   区间算术
│   ├── geom_evol.c             #   几何演化
│   ├── geo_aabb_tree.c         #   AABB 树
│   ├── geo_halfedge_mesh.c     #   半边网格
│   ├── geo_constraint_solver.c #   约束求解器
│   ├── geo_dynamic.c           #   动态几何
│   ├── geo_predicate.c         #   几何谓词
│   ├── geo_spec.c             #   几何规约
│   ├── geo_metalogic.c         #   几何元逻辑
│   ├── geo_invariant_type.c    #   不变量类型
│   ├── geo_utils.c            #   几何工具
│   └── ...                     #   其他几何模块
├── layer4_reasoning/           # Layer 4: 公理推理层
│   ├── engine.c                #   核心引擎
│   ├── engine_scheduler.c      #   引擎调度
│   ├── proof.c                #   证明系统
│   ├── proof_engine_enhanced.c #   增强证明引擎
│   ├── proof_search.c         #   证明搜索
│   ├── proof_session.c        #   证明会话
│   ├── proof_trace.c          #   证明追踪
│   ├── rewrite.c              #   重写引擎
│   ├── normalization.c        #   归一化
│   ├── groebner_engine.c      #   Groebner 基
│   ├── groebner_parallel.c    #   并行 Groebner
│   ├── atp_backend.c          #   ATP 后端
│   ├── bdd_encoding.c         #   BDD 编码
│   ├── func_block.c           #   函数块
│   ├── axiom_pkg.c            #   公理包
│   ├── quantifier.c           #   量词
│   ├── modal_operators.c       #   模态算子
│   ├── inequality_reasoning.c #   不等式推理
│   ├── probabilistic_constraint.c # 概率约束
│   ├── autodiff.c             #   自动微分
│   ├── ode_solver.c           #   ODE 求解器
│   └── ...                     #   其他推理模块
├── layer5_output/              # Layer 5: 结果输出层
│   ├── stream_output.c         #   流式输出
│   ├── proof_output.c          #   证明输出
│   ├── geo_visual.c           #   几何可视化
│   ├── interop.c              #   互操作
│   └── magic.c                #   Magic 模块
├── layer6_visual/              # Layer 6: 图形化编程层
│   ├── node_graph.c           #   节点图
│   ├── geometry_canvas.c      #   几何画布
│   ├── block_canvas.c         #   积木画布
│   ├── block_scheduler.c      #   块调度器
│   ├── visual_editor.c        #   可视化编辑器
│   ├── block_to_text.c       #   块->文本转换
│   ├── block_to_node.c       #   块->节点转换
│   ├── block_to_geometry.c    #   块->几何转换
│   ├── sync_protocol.c       #   同步协议
│   └── extended_types.c       #   类型扩展
├── layer7_orchestration/       # Layer 7: 编排调度层
│   └── orchestrator.c         #   六阶段流水线编排器
├── layer8_meta_verify/        # Layer 8: 元验证层
│   └── meta_verify.c          #   五维元验证
├── layer9_application/        # Layer 9: 应用入口层
│   └── application.c          #   批处理 + REPL
└── layer10_interop/            # Layer 10: 外部集成层
    ├── coq_bridge.c           #   Coq 桥接
    ├── lean4_bridge.c          #   Lean 4 桥接
    └── opml_codec.c           #   OPML 编解码
```

### 6.3 文件统计

| 层级 | 源文件数 | 头文件数 | 测试数 |
|------|---------|---------|--------|
| Layer 1 (Parser) | 7 | 共享 | - |
| Layer 2 (Resource) | 15 | 共享 | - |
| Layer 3 (Geometry) | 35+ | 共享 | - |
| Layer 4 (Reasoning) | 100+ | 共享 | - |
| Layer 5 (Output) | 6 | 共享 | - |
| Layer 6 (Visual) | 17 | 8 | - |
| Layer 7 (Orchestration) | 1 | 1 | - |
| Layer 8 (Meta-Verify) | 1 | 1 | - |
| Layer 9 (Application) | 1 | 1 | - |
| Layer 10 (Interop) | 4 | 1 | - |
| **formal/** | 13 | - | 1 |
| **test/** | - | - | 70+ |
| **总计** | **200+** | **120+** | **70+** |

---

## 7. 核心能力摘要

### 7.1 几何能力

| 能力 | 描述 | 所属层 |
|------|------|--------|
| 符号坐标系统 | 有理数、代数数、二次扩域、超越数（GMP 任意精度） | L3 |
| 约束图 | 点、线段、区域及其约束关系 | L3 |
| 归一化 | WL 图核迭代归一化，自动合并等价节点 | L3 |
| 几何变换 | 平移、旋转（Rodrigues 公式）、缩放、反射 | L3 |
| 几何代数 | GATr + GAALOP + GeoLogic 落地 | L4 |
| 交互式几何 | 动态拖拽、实时约束更新 | L3 |
| 高维结构 | n 维几何支持 | L3 |

### 7.2 推理与证明

| 能力 | 描述 | 所属层 |
|------|------|--------|
| 多策略证明引擎 | 8 种证明方法（直接构造、面积法、Groebner 基法、向量法、全角法、演绎数据库法、坐标法、Oracle 法） | L4 |
| 搜索算法 | DFS 回溯、BFS 队列、最佳优先启发式、MCTS UCB1 | L4 |
| 三值逻辑 | 真、假、未知三种真值 | L4 |
| 模态逻辑 | 模态推理算子 | L4 |
| 量词系统 | 全称量词与存在量词，含元素代入评估 | L4 |
| 信任颜色 | 7 级信任标记系统 | L4 |
| 元验证 | 五维度验证（类型一致性、完整性、健全性、非平凡性、往返） | L8 |

### 7.3 后端求解引擎

| 引擎 | 状态 | 说明 | 所属层 |
|------|------|------|--------|
| CDCL SAT 求解器 | 已实现 | 冲突驱动子句学习 | L4 |
| SMT 后端 | 部分实现 | Groebner 基 + Z3/cvc5 子进程 | L4 |
| ATP 后端 | 已实现 | Vampire/EProver/iProver | L4 |
| BDD 编码 | 已实现 | 唯一表、计算表、Tseitin CNF | L4 |
| Groebner 基 | 已实现 | 并行 Buchberger + work-stealing | L4 |
| 数值后端 | 已实现 | GMRES(m=30)、BiCGSTAB、CG | L4 |
| 不等式推理 | 已实现 | AM-GM、Cauchy-Schwarz、Jensen、SOS | L4 |
| 概率约束 | 已实现 | DTMC + PCTL 评估 | L4 |

### 7.4 输出与互操作

| 格式/后端 | 状态 | 说明 | 所属层 |
|-----------|------|------|--------|
| OPML | 已实现 | 开放数学证明交换格式 | L10 |
| Lean 4 | 已实现 | 双向桥接 | L10 |
| Coq | 已实现 | 双向桥接 | L10 |
| TikZ | 已实现 | LaTeX 图形输出 | L5 |
| SVG | 已实现 | 几何可视化 | L5 |
| Cairo | 已实现 | Cairo 脚本生成 | L5 |
| Three.js | 已实现 | HTML + 3D 场景 | L5 |
| PPM | 已实现 | 光栅化像素输出 | L5 |

### 7.5 可视化编程

| 能力 | 描述 | 所属层 |
|------|------|--------|
| 四视图同步 | 节点图、几何画布、积木画布、文本代码 | L6 |
| 力导向布局 | Fruchterman-Reingold 算法 | L6 |
| 块调度器 | Kahn 拓扑排序 + 增量脏块执行 | L6 |
| 类型推断 | Hindley-Milner 风格统一算法 | L6 |
| 效果追踪 | Pure/IO/State 效果类型系统 | L6 |

### 7.6 形式化验证

| 能力 | 描述 | 所属层 |
|------|------|--------|
| Lean 4 框架 | Lake 构建系统，mathlib4 v4.14.0 | formal/ |
| Hilbert 公理体系 | 五大公理组 | formal/ |
| 欧氏平面 | 基础定义和定理框架 | formal/ |
| 元验证 | 五维检查 | L8 |

---

## 8. 技术要求

### 8.1 构建依赖

| 依赖 | 版本 | 必需 | 说明 |
|------|------|------|------|
| CMake | >= 3.15 | 是 | 构建系统 |
| C 编译器 | C11 | 是 | GCC/Clang/MSVC |
| GMP | 最新稳定版 | 是* | 任意精度算术（非 WASM 构建必需） |
| Python | 3.x | 否 | DSL 支持 |
| Z3 | >= 4.12 | 否 | SMT 求解（可选后端） |
| cvc5 | >= 1.0 | 否 | SMT 求解（可选后端） |
| Lean 4 | 最新 | 否 | 形式化验证 |
| Node.js | >= 18 | 否 | Web GUI |

### 8.2 平台支持

| 平台 | 编译器 | 状态 |
|------|--------|------|
| Linux (x86_64) | GCC 9+ / Clang 10+ | 完全支持 |
| macOS (x86_64/ARM64) | Clang 12+ | 完全支持 |
| Windows (MSYS2) | MinGW GCC | 完全支持 |
| Windows (MSVC) | MSVC 2019+ | 完全支持 |
| WebAssembly | Emscripten | 实验性 |

### 8.3 构建选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `BUILD_TESTS` | ON | 构建测试 |
| `BUILD_EXAMPLES` | ON | 构建示例 |
| `ENABLE_WASM` | OFF | WebAssembly 构建 |
| `ENABLE_LAYER_VALIDATION` | ON | 层级边界验证 |
| `ENABLE_COVERAGE` | OFF | 代码覆盖率 |
| `ENABLE_SANITIZERS` | OFF | 内存错误检测 |
| `LV00_EXCLUDE_BROKEN_PRESETS` | ON | 排除有依赖问题的预设 |

---

## 9. 快速开始

### 9.1 Linux/macOS

```bash
# 安装依赖
# Ubuntu/Debian
sudo apt-get install cmake libgmp-dev

# macOS
brew install cmake gmp

# 克隆仓库
git clone https://github.com/lv00-project/lv00.git
cd lv00

# 构建
mkdir build && cd build
cmake ..
cmake --build .

# 运行测试
ctest --output-on-failure
```

### 9.2 Windows (MSYS2)

```bash
# 安装 MSYS2 和依赖
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-gmp

# 构建
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .

# 运行测试
ctest --output-on-failure
```

### 9.3 构建配置

```bash
# Debug 模式
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 模式
cmake .. -DCMAKE_BUILD_TYPE=Release

# 启用代码覆盖率
cmake .. -DENABLE_COVERAGE=ON

# 启用 Sanitizers
cmake .. -DENABLE_SANITIZERS=ON

# 仅构建库
cmake .. -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF

# 启用层级验证
cmake .. -DENABLE_LAYER_VALIDATION=ON
```

### 9.4 验证构建

```bash
cmake -S . -B build_verify -DENABLE_LAYER_VALIDATION=ON
cmake --build build_verify
ctest --test-dir build_verify --output-on-failure
```

---

## 10. 基础用法示例

### 10.1 C API 基本使用

```c
#include "lv00.h"
#include <stdio.h>

int main() {
    // 创建约束图
    ConstraintGraph *g = graph_create();

    // 创建两个点
    SymbolicCoord *x1 = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *y1 = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *coords1[] = {x1, y1};
    graph_add_point(g, coords1, 2);

    SymbolicCoord *x2 = symbolic_coord_create_rational(3, 1);
    SymbolicCoord *y2 = symbolic_coord_create_rational(4, 1);
    SymbolicCoord *coords2[] = {x2, y2};
    graph_add_point(g, coords2, 2);

    // 创建线段
    graph_add_line_segment(g, 0, 1);

    // 归一化
    NormalizationResult *result = graph_normalize(g, false);
    printf("合并了 %d 个节点\n", result->merged_count);
    normalization_result_destroy(result);

    // 清理
    graph_destroy(g);
    return 0;
}
```

### 10.2 函数块定义与使用

```c
// 定义中点函数块
FuncBlock *midpoint = lv00_fb_create("midpoint", 2);
lv00_fb_define(midpoint, "return point((A.x+B.x)/2, (A.y+B.y)/2);");

// 实例化
Point *M = lv00_fb_apply(midpoint, A, B);
```

### 10.3 预设模块加载

```c
// 加载预设模块
lv00_preset_load(ctx, "euclidean_geometry");

// 应用预设定理
Proposition *prop = lv00_preset_apply(ctx, "pythagorean_theorem", A, B, C);
```

### 10.4 公理包使用

```c
// 加载公理包
AxiomPackage *pkg = lv00_axiom_pkg_load("algebraic_geometry");

// 附加到引擎
lv00_engine_attach_axiom_pkg(engine, pkg);
```

### 10.5 Lv-00 DSL 示例

```
// 声明点
Point A, B, C;

// 约束
Constraint distance(A, B) == 5;
Constraint distance(B, C) == 5;
Constraint angle(A, B, C) == 60°;

// 证明目标
Prove triangle(A, B, C) is equilateral;
```

### 10.6 REPL 交互

```
$ lv00 --repl
Lv-00 v5.0.0 REPL
> Point A(0, 0), B(3, 4);
Created: A = Point(0/1, 0/1), B = Point(3/1, 4/1)
> Compute distance(A, B);
Result: 5/1 (Rational, Trust: Green)
> Prove distance(A, B)^2 == 25;
Proof: PROVEN (Trust: Green, Steps: 3)
  1. distance(A,B)^2 = (3/1)^2 + (4/1)^2  [Pythagorean]
  2. = 9/1 + 16/1  [Arithmetic]
  3. = 25/1 == 25/1  [QED]
> :export proof lean4
Exported to: triangle_proof.lean
> :quit
```

---

## 11. 符号坐标系

### 11.1 概述

符号坐标系统是 Lv-00 的核心基础设施，负责精确表示和处理几何体的坐标值。系统支持四种类型的符号坐标，确保所有几何计算在符号层面精确执行。

### 11.2 坐标类型

| 类型 | 数学表示 | 实现方式 | 精度 |
|------|----------|----------|------|
| `RATIONAL` | a/b | GMP `mpq_t` | 任意精度 |
| `ALGEBRAIC` | 整系数多项式实根 | 表达式树 + 隔离区间 | 符号精确 |
| `QUADRATIC` | a + b*sqrt(n) | 二次扩张结构 | 符号精确 |
| `TRANSCENDENTAL` | pi, e 等 | 符号表达式 | 符号精确 |

### 11.3 数据结构

```c
typedef enum {
    RATIONAL,        // 有理数 (使用GMP mpq_t)
    ALGEBRAIC,       // 代数数 (极小多项式 + 隔离区间)
    QUADRATIC,       // 二次根式 (a + b√n)
    TRANSCENDENTAL   // 超越常数 (π, e)
} CoordType;

typedef struct Rational {
    mpq_t value;  // GMP 有理数类型
} Rational;

typedef struct Algebraic {
    mpz_poly_t minimal_poly;   // 整数系数极小多项式
    double left_bound;         // 隔离区间左端点
    double right_bound;        // 隔离区间右端点
    int precision_bits;        // 当前隔离区间的精度
    Rational *cached_rational; // 若有理化则缓存
} Algebraic;

typedef struct Quadratic {
    Rational *a;       // 有理数系数
    Rational *b;       // 有理数系数
    unsigned int n;    // 无平方因子正整数
} Quadratic;
```

### 11.4 核心设计原则

1. **精确性优先**: 所有坐标操作在符号层面精确执行，数值近似仅用于显示
2. **可判定性**: 坐标判等必须是可判定的算法
3. **位数熔断**: 当计算复杂度超过阈值时提供优雅的降级路径
4. **A/B 计划切换**: 支持从完整代数数到二次根式的降级方案

### 11.5 有理数操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建 | `rational_create(num, den)` | 从整数创建有理数 |
| 销毁 | `rational_destroy(r)` | 释放资源 |
| 加法 | `rational_add(a, b)` | 返回新的有理数 |
| 减法 | `rational_subtract(a, b)` | 返回新的有理数 |
| 乘法 | `rational_multiply(a, b)` | 返回新的有理数 |
| 除法 | `rational_divide(a, b)` | 检测除零错误 |
| 比较 | `rational_compare(a, b)` | 返回 -1, 0, 1 |

### 11.6 代数数判等算法

1. 先比较极小多项式（整数多项式逐系数判等）
2. 若多项式相同，再比较隔离区间是否相交且足够精确以区分根
3. 若多项式相同且区间重叠，则为同一代数数
4. 若多项式相同但区间不相交，则为不同代数数

### 11.7 位数熔断检测

```c
static CircuitStatus check_rational_circuit(const Rational *r) {
    size_t num_bits = mpz_sizeinbase(mpq_numref(r->value), 2);
    size_t den_bits = mpz_sizeinbase(mpq_denref(r->value), 2);
    if (num_bits + den_bits > BIT_CUTOFF_THRESHOLD) {
        return CIRCUIT_TRIPPED;
    }
    return CIRCUIT_OK;
}
```

---

## 12. 约束图

### 12.1 概述

约束图核心是 Lv-00 的基础数据结构层，负责维护几何体（点、线段、区域、端口、函数块）及其相互关系。

### 12.2 节点类型

| 类型 | 描述 | 维度 |
|------|------|------|
| `GEOM_POINT` | 点 - 由符号坐标定义 | 零维 |
| `GEOM_LINE_SEGMENT` | 线段 - 由两个端点定义 | 一维 |
| `GEOM_REGION` | 区域 - 由边界线段序列形成的闭合环 | 二维 |
| `GEOM_PORT` | 端口 - 函数块的对外接口 | - |
| `GEOM_FUNCTION_BLOCK` | 函数块 - 封装内部约束子图 | - |

### 12.3 约束类型

| 类型 | 描述 |
|------|------|
| `CONSTRAINT_INCIDENCE` | 关联 - 点在线上、点在区域边界 |
| `CONSTRAINT_BETWEENNESS` | 之间 - 点在线段上 |
| `CONSTRAINT_INTERSECTION` | 相交 - 线与线相交 |
| `CONSTRAINT_CONTAINMENT` | 包含 - 区域包含点 |
| `CONSTRAINT_CONNECTION` | 连接 - 端口与端口相连 |

### 12.4 核心数据结构

```c
typedef struct GeomNode {
    int id;                          // 全局唯一 ID
    GeomNodeType type;               // 节点类型
    SymbolicCoord **coords;          // 符号坐标数组
    int coord_count;                 // 坐标数量
    int namespace_depth;             // 嵌套深度
    int parent_block_id;             // 所属函数块 ID
    bool is_formal_param;            // 是否为形式参数
    TrustColor trust;                // 信任颜色
} GeomNode;

typedef struct ConstraintGraph {
    GeomNode **nodes;                // 节点数组
    int node_count;
    Constraint **constraints;        // 约束数组
    int constraint_count;
    HashTable *node_index;           // ID -> 节点指针
    HashTable *constraint_index;     // ID -> 约束指针
} ConstraintGraph;
```

### 12.5 四态约束状态

| 状态 | 含义 | 处理策略 |
|------|------|----------|
| `CONSISTENT` | 约束相容 | 继续推理 |
| `INCONSISTENT` | 约束矛盾 | 报告矛盾源 |
| `UNDER_CONSTRAINED` | 欠约束 | 提示需要更多信息 |
| `OVER_CONSTRAINED` | 过约束 | 检测冗余约束 |

### 12.6 约束图示例

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

---

## 13. 代数数与精确算术

### 13.1 概述

Lv-00 的精确算术系统基于 GMP（GNU Multiple Precision Arithmetic Library），支持任意精度有理数运算和符号代数数计算。

### 13.2 有理数系统

- 使用 GMP `mpq_t` 类型，任意精度分数
- 坐标创建时自动约分到最简形式
- 分母恒为正整数，分子可为负
- 零统一表示为 0/1

### 13.3 代数数系统

代数数使用"极小多项式 + 隔离区间"表示：

- **极小多项式**: 整系数多项式，该数为其实根之一
- **隔离区间**: 包含唯一实根的有理数区间
- **精度管理**: 初始 53 位双精度，判等需要时加倍至 2^100 位上限

### 13.4 优先有理化通路

每次算术运算后自动运行：

1. 对结果代数数计算连分式逼近
2. 产生有理数候选值
3. 在符号层将候选值代入极小多项式精确求值
4. 若多项式值为零，则用有理数节点替换代数数节点

### 13.5 平方根嵌套处理

若运算产生 sqrt(a + b*sqrt(n)) 形式：

1. 检查 a^2 - b^2*n 是否为完全平方数
2. 若是，展开为规范的二次根式形式
3. 否则降级为代数数或标记超界

### 13.6 区间算术

区间算术用于数值验证和边界估计：

- 支持有理数区间和代数数区间
- 自动区间扩展和收缩
- 与符号计算交叉验证

---

## 14. 证明系统与命题

### 14.1 概述

命题与证明系统是 Lv-00 的核心证明机制，实现了"构造即证明"的理念。系统支持命题模式定义、合一检查、证明导航器和不可构造性证明。

### 14.2 命题模式

```c
typedef struct Proposition {
    int id;
    char *name;
    // 端口声明
    struct { int port_id; bool is_input; char *type_description; } *ports;
    int port_count;
    // 几何模式（虚线框内容）
    ConstraintGraph *pattern;
    // 前置/后置条件
    Constraint **preconditions;
    Constraint **postconditions;
    // 证明状态
    ProofStatus status;
    // 证明构造（得证后填充）
    ConstraintGraph *proof_construction;
} Proposition;
```

### 14.3 证明状态

| 状态 | 含义 | 视觉表示 |
|------|------|----------|
| `PROPOSITION_UNPROVEN` | 未证明 | 蓝色虚框 |
| `PROPOSITION_PROVEN` | 已证明 | 绿色实框 |
| `PROPOSITION_UNCONSTRUCTIBLE_GREEN` | 已证不可构造 | 绿色 |
| `PROPOSITION_UNCONSTRUCTIBLE_YELLOW` | 条件性不可构造 | 黄色 |
| `PROPOSITION_IN_PROGRESS` | 证明进行中 | 灰色 |

### 14.4 多策略证明引擎

| 策略 | 适用场景 | 复杂度 |
|------|----------|--------|
| 直接构造法 | 已知构造步骤 | O(n) |
| 面积法 | 面积相关命题 | O(n^2) |
| Groebner 基法 | 代数消元 | O(d^6) |
| 向量法 | 向量关系 | O(n^2) |
| 全角法 | 角度关系 | O(n^2) |
| 演绎数据库法 | 已知定理匹配 | O(n) |
| 坐标法 | 坐标化证明 | O(d^3) |
| Oracle 法 | 非构造性步骤 | 依赖 oracle |

### 14.5 搜索算法

| 算法 | 描述 | 适用场景 |
|------|------|----------|
| DFS 回溯 | 深度优先搜索 | 约束密集问题 |
| BFS 队列 | 广度优先搜索 | 最短证明搜索 |
| 最佳优先 | 启发式评估 | 复杂问题 |
| MCTS UCB1 | 蒙特卡洛树搜索 | 探索-利用平衡 |

### 14.6 证明对象结构

```c
typedef struct Lv00ProofObject {
    Proposition *goal;              // 证明目标
    ProofStep **steps;              // 证明步骤序列
    size_t step_count;
    AssumptionScope *assumptions;   // 假设作用域
    ContradictionTrace *contradiction; // 矛盾溯源
    ProofStatus status;             // 证明状态
} Lv00ProofObject;

typedef struct Lv00ProofStep {
    size_t step_id;
    Proposition *proposition;
    ProofRule rule;                 // 使用的证明规则
    size_t *premise_ids;            // 前提步骤 ID
    Lv00SourceSpan source_span;     // 来源位置
} Lv00ProofStep;
```

### 14.7 证明导出

证明可导出为多种格式：

| 格式 | 用途 | 层 |
|------|------|-----|
| Lean 4 tactic 脚本 | 形式化验证 | L10 |
| Coq vernacular | 形式化验证 | L10 |
| OPML JSON | 证明交换 | L10 |
| LaTeX | 学术论文 | L5 |
| 自然语言 | 人类阅读 | L5 |

---

## 15. Groebner 基引擎

### 15.1 概述

Groebner 基引擎是 Lv-00 代数推理的核心后端，用于多项式理想求解和几何代数消元。

### 15.2 Buchberger 算法

```c
GroebnerResult groebner_compute(
    Polynomial **input_polys,
    int poly_count,
    MonomialOrder order,
    GroebnerOptions *options
);
```

**核心步骤**:
1. 计算所有多项式对的 S-多项式
2. 将 S-多项式约化为零或加入基中
3. 重复直到所有 S-多项式约化为零

### 15.3 并行引擎

v5.0.0 实现了并行 Buchberger 算法：

- **Work-Stealing 负载均衡**: 空闲线程从繁忙线程窃取 S-多项式计算任务
- **线程池**: 可配置线程数
- **步数限制**: `BUCHBERGER_MAX_STEPS = 50000`

### 15.4 单项式序

| 序类型 | 描述 | 适用场景 |
|--------|------|----------|
| 字典序 (LEX) | 按变量字典序比较 | 消元理论 |
| 次数全序 (GRLEX) | 先比总次数再比字典序 | 一般计算 |
| 次数逆字典序 (GREVLEX) | 先比总次数再逆字典序 | 效率最优 |

### 15.5 应用场景

- 几何定理的代数证明
- 多项式方程组求解
- 理想成员判定
- 消元理论（投影）

### 15.6 Groebner 基与几何证明

Groebner 基方法是 Lv-00 几何定理证明的核心代数方法之一。其基本思路是：

1. 将几何条件转化为多项式方程组（坐标化）
2. 将几何结论转化为多项式
3. 计算假设多项式的 Groebner 基
4. 用 Groebner 基约化结论多项式
5. 若余式为零，则结论在假设条件下成立

**示例 -- 三角形内角和定理**:

```
假设: A, B, C 为三角形三个顶点
坐标化: A = (0,0), B = (c,0), C = (u,v)  (v != 0)
结论: angle(A) + angle(B) + angle(C) = 180°

转化为多项式方程后，通过 Groebner 基消元验证。
```

### 15.7 性能优化

| 优化策略 | 描述 |
|----------|------|
| 模块化 Groebner 基 | 利用理想分解减少计算量 |
| 策略轮换 | 动态切换单项式序 |
| 约化检查 | S-多项式约化前预检查 |
| 并行 S-多项式 | Work-stealing 并行计算 |
| 缓存中间结果 | 避免重复计算 |

---

## 16. SMT 与 SAT 编码

### 16.1 CDCL SAT 求解器

Lv-00 实现了完整的 CDCL（Conflict-Driven Clause Learning）SAT 求解器：

| 组件 | 描述 |
|------|------|
| 传播 (Propagation) | 单元传播 |
| 冲突分析 (Conflict Analysis) | UIP 计算与冲突子句学习 |
| 回跳 (Backjumping) | 非顺序回跳 |
| 学习 (Learning) | 冲突子句添加 |
| 重启 (Restart) | 几何/动态重启策略 |

**硬编码阈值**: `CDCL_MAX_STEPS = 1000`, `CDCL_MAX_DECISIONS = 1000`

### 16.2 BDD 编码

二叉决策图（BDD）编码系统：

| 组件 | 描述 |
|------|------|
| 唯一表 (Unique Table) | 共享等价子图 |
| 计算表 (Compute Table) | 缓存中间结果 |
| Tseitin CNF 变换 | 布尔电路 -> CNF |
| Shannon 展开 | BDD 递归构建 |
| ADD 运算 | 代数决策图运算 |

### 16.3 SMT 后端

SMT（Satisfiability Modulo Theories）后端架构：

```
Lv-00 SMT 后端
├── 内置 Groebner 基后端（已实现）
├── Z3 子进程集成（需外部安装）
├── cvc5 子进程集成（需外部安装）
└── Singular 子进程集成（需外部安装）
```

未安装外部求解器时，系统优雅降级为 UNKNOWN 状态。

### 16.5 SMT 编码流程

```
Lv-00 约束系统
  ↓
[命题逻辑提取] ──→ 布尔结构
  ↓
[Tseitin 编码] ──→ CNF 子句集
  ↓
[理论原子提取] ──→ 算术约束
  ↓
[DPLL(T) 框架] ──→ 布尔搜索 + 理论求解器
  ↓
SAT/UNSAT/UNKNOWN
```

### 16.6 SAT 求解器详细流程

```
CNF 子句集
  ↓
[单元传播] ──→ 推导所有必然赋值
  ↓
[决策] ──→ 选择未赋值变量
  ↓
[传播] ──→ 继续单元传播
  ↓
[冲突?] ──→ 是: 冲突分析 → 学习子句 → 回跳
  │              否: 继续决策
  ↓
[所有变量赋值?] ──→ 是: SAT
  ↓
[重启检查] ──→ 几何/动态重启
  ↓
[步数检查] ──→ 超限: UNKNOWN
```

### 16.4 ATP 后端

自动定理证明器（ATP）后端：

| 求解器 | 协议 | SZS 状态解析 |
|--------|------|-------------|
| Vampire | TPTP | 已实现 |
| EProver | TPTP | 已实现 |
| iProver | TPTP | 已实现 |

---

## 17. 重写与合一

### 17.1 图重写引擎

图重写引擎是 Lv-00 的核心计算机制，将几何约束图通过重写规则逐步化简。

### 17.2 重写规则结构

```c
typedef struct RewriteRule {
    int id;
    char *name;
    ConstraintGraph *pattern;        // 匹配模式
    int *pattern_variable_nodes;     // 变量节点
    ConstraintGraph *replacement;    // 替换模式
    int reduction_measure;           // 约简测度
    RewriteCondition *condition;     // 前置条件
    char *axiom_package;             // 所属公理包
    int priority;                    // 优先级
} RewriteRule;
```

### 17.3 核心设计原则

1. **局部等价容忍**: 匹配时基于符号坐标判等，无需全局规范化
2. **约简测度驱动**: 有测度的规则优先应用，确保终止性
3. **事务性回滚**: 替换产生冲突时自动回滚
4. **循环检测**: 基于 WL 图核的哈希检测重写循环

### 17.4 合一系统

合一（Unification）验证几何构造是否满足命题模式。

**合一流程**:

```
命题模式 (虚线框)
  ↓
[模式匹配] ──→ 将构造与模式进行子图同构
  ↓
[端口绑定] ──→ 绑定输入/输出端口
  ↓
[前置条件检查] ──→ 验证前置条件是否满足
  ↓
[后置条件验证] ──→ 验证输出是否满足后置条件
  ↓
合一成功 / 失败
```

### 17.5 步数熔断

```c
#define REWRITE_SOLVE_MAX_ITERATIONS 10000
```

超过最大迭代次数时，重写引擎停止并报告超时。

### 17.6 重写策略

Lv-00 支持多种重写策略：

| 策略 | 描述 | 适用场景 |
|------|------|----------|
| 内侧优先 (Innermost) | 先约化最内层表达式 | 终止性保证 |
| 外侧优先 (Outermost) | 先约化最外层表达式 | 效率优先 |
| 并行 (Parallel) | 同时约化所有可约位置 | 完全展开 |
| 按需 (Lazy) | 仅在需要时约化 | 惰性求值 |

### 17.7 循环检测机制

重写引擎使用基于 WL（Weisfeiler-Lehman）图核的哈希来检测重写循环：

1. 每次重写后计算约束图的 WL 图核哈希
2. 将哈希值存入历史集合
3. 若新哈希值已在历史集合中，则检测到循环
4. 循环检测触发时，引擎停止并报告循环信息

### 17.8 条件重写

重写规则可附带条件，仅在条件满足时应用：

```c
typedef struct RewriteCondition {
    ConditionType type;
    union {
        struct {
            int node1_id;
            int node2_id;
            ComparisonOp op;  // >, <, =, >=, <=, !=
        } algebraic;
        struct {
            int point_id;
            int region_id;
        } containment;
        struct {
            char *custom_predicate;
            int *arg_nodes;
            int arg_count;
        } custom;
    } data;
} RewriteCondition;
```

### 17.9 VF2 子图同构

重写引擎使用 VF2 算法进行模式匹配（子图同构检测）：

- **深度限制**: `VF2_MAX_DEPTH = 100`
- **匹配顺序**: 按度数排序，优先匹配高连接度节点
- **剪枝**: 利用度数序列和标签进行快速剪枝

---

## 18. 功能块系统

### 18.1 概述

函数块系统是 Lv-00 的抽象机制，允许将几何构造封装为可复用的模块。

### 18.2 函数块结构

```c
typedef struct FuncBlock {
    int id;
    char *name;
    ConstraintGraph *internal_graph;  // 内部子图
    int *input_port_ids;              // 输入端口
    int *output_port_ids;             // 输出端口
    DeterminismState determinism;      // 确定性状态
    SolutionSelector *selector;        // 多解选择器
    Constraint **boundary_constraints; // 跨边界约束
    char *axiom_package;               // 所属公理包
} FuncBlock;
```

### 18.3 确定性状态

| 状态 | 描述 |
|------|------|
| `UNVERIFIED` | 打包完成，尚未静态分析 |
| `VERIFIED` | 静态分析确认解唯一 |
| `NON_DETERMINISTIC` | 应用时出现多解 |
| `PARTIALLY_VERIFIED` | 静态分析未完成，但未发现冲突 |

### 18.4 核心操作

| 操作 | 描述 |
|------|------|
| 打包 (Pack) | 将约束子图封装为函数块 |
| 实例化 (Instantiate) | 用实际参数替换形式参数 |
| 部分应用 (Partial Apply) | 仅绑定部分输入端口 |
| 组合 (Compose) | 将多个函数块串联 |
| 选择器 (Selector) | 多解情况下的解选择策略 |

### 18.5 多解选择器

| 策略 | 描述 |
|------|------|
| First | 选择第一个解 |
| All | 返回所有解 |
| Minimal | 选择度量最小的解 |
| Maximal | 选择度量最大的解 |
| Random | 随机选择 |

### 18.6 打包流程

1. 识别内部节点集合
2. 检查跨边界约束
3. 提升或拒绝跨边界约束
4. 执行静态确定性分析
5. 创建函数块对象

### 18.7 函数块与图形化编程的关系

在 v5.0.0 的十层架构中，函数块是 Layer 6 图形化编程层的核心抽象单元：

- **节点图视图**: 函数块作为节点，端口作为连接点
- **积木画布视图**: 函数块作为可拖拽的积木块
- **几何画布视图**: 函数块的内部约束子图可视化
- **文本代码视图**: 函数块展开为 Lv-00 DSL 代码

四种视图通过表示转换层（Layer 6 Subsystem 4）实现双向同步。

### 18.8 函数块组合子

Lv-00 预置了多种函数块组合子：

| 组合子 | 描述 | 签名 |
|--------|------|------|
| `Compose` | 串联两个函数块 | `(B -> C) -> (A -> B) -> (A -> C)` |
| `Product` | 并行执行两个函数块 | `(A -> B) -> (C -> D) -> ((A,C) -> (B,D))` |
| `Identity` | 恒等函数块 | `(A -> A)` |
| `Conditional` | 条件分支 | `(A -> B) -> (A -> B) -> (Bool, A) -> B` |

### 18.9 函数块选择器策略

| 策略 | 描述 | 适用场景 |
|------|------|----------|
| `SELECTOR_FIRST` | 选择第一个解 | 快速求解 |
| `SELECTOR_ALL` | 返回所有解 | 完整枚举 |
| `SELECTOR_MINIMAL` | 选择度量最小的解 | 最优化 |
| `SELECTOR_MAXIMAL` | 选择度量最大的解 | 最优化 |
| `SELECTOR_RANDOM` | 随机选择 | 探索性求解 |
| `SELECTOR_USER` | 交由用户选择 | 交互式场景 |

---

## 19. 公理包与预设

### 19.1 公理包系统

公理包是版本化管理的公理集合：

| 特性 | 描述 |
|------|------|
| SHA-256 校验 | 确保公理包完整性 |
| 模板展开 | 支持参数化公理模板 |
| 依赖管理 | 公理包间依赖追踪 |
| 版本控制 | 语义版本号管理 |

```c
// 加载公理包
AxiomPackage *pkg = lv00_axiom_pkg_load("algebraic_geometry");

// 附加到引擎
lv00_engine_attach_axiom_pkg(engine, pkg);
```

### 19.2 预设模块生态

Lv-00 提供 55+ 数学理论预设模块，涵盖广泛领域：

| 领域 | 预设模块 |
|------|----------|
| **几何** | 欧氏平面、仿射几何、射影几何、微分几何、3D 几何、多边形、三角形 |
| **代数** | 线性代数、矩阵、多项式、环论、域论、群论、格论、泛代数 |
| **拓扑** | 点集拓扑、代数拓扑、同调代数、范畴论、拓扑斯理论 |
| **逻辑** | 命题逻辑、一阶逻辑、模态逻辑、直觉主义逻辑、模型论、证明论 |
| **分析** | 实分析、复分析、测度论、泛函分析、积分变换 |
| **数论** | 数论、编码理论 |
| **概率统计** | 概率论、统计学、随机过程、信息论、博弈论 |
| **微分方程** | 常微分方程、差分方程、动力系统 |
| **范畴论** | 范畴论、笛卡尔闭范畴、表示论 |
| **其他** | 编码理论、计算复杂性、可计算性理论、描述集合论 |

### 19.3 预设函数块注册

```c
// 加载预设模块
lv00_preset_load(ctx, "euclidean_geometry");

// 应用预设定理
Proposition *prop = lv00_preset_apply(ctx, "pythagorean_theorem", A, B, C);
```

### 19.4 公理包目录

公理包存储在 `module/axiom_packages/` 目录下，包含 60+ `.lvz` 文件。

---

## 20. 插件系统

### 20.1 概述

Lv-00 v5.0.0 实现了插件系统，支持运行时加载外部扩展模块。

### 20.2 插件特性

| 特性 | 描述 |
|------|------|
| 通配符匹配 | 支持文件名模式匹配 |
| 目录扫描 | 自动发现插件目录 |
| 语义版本 | 插件版本兼容性检查 |
| 动态加载 | 运行时加载共享库 |

### 20.3 插件接口

```c
// 插件入口点
typedef struct Lv00Plugin {
    const char *name;
    const char *version;
    lv00_error_t (*init)(LV00Context *ctx);
    lv00_error_t (*execute)(LV00Context *ctx, const char *input);
    void (*cleanup)(LV00Context *ctx);
} Lv00Plugin;
```

### 20.4 插件配置

```json
{
    "name": "sample_plugin",
    "version": "1.0.0",
    "match": "*.lvz",
    "scan_dirs": ["./plugins", "~/.lv00/plugins"]
}
```

### 20.5 插件示例

示例插件位于 `examples/plugin_example/`，包含：
- `sample_plugin.c` - 插件源码
- `sample_plugin.conf` - 插件配置
- `CMakeLists.txt` - 构建文件

---

## 21. Lean 4 形式化概述

### 21.1 概述

Lv-00 的形式化验证基于 Lean 4，使用 Lake 构建系统，依赖 mathlib4 v4.14.0。

### 21.2 项目结构

```
formal/
├── Lv00/
│   ├── Basic.lean              # 基础定义
│   ├── Incidence.lean          # 关联公理
│   ├── Betweenness.lean        # 顺序公理
│   ├── Congruence.lean         # 全等公理
│   ├── Order.lean              # 序公理
│   ├── Parallel.lean           # 平行公理
│   ├── Continuity.lean         # 连续公理
│   ├── HilbertAxioms.lean      # Hilbert 公理体系
│   ├── EuclideanPlane.lean     # 欧氏平面
│   └── Lv00Meta.lean           # 元理论
├── tests/
│   └── all_tests.lean          # 形式化测试
├── lakefile.lean              # Lake 构建文件
└── lean-toolchain              # Lean 工具链
```

### 21.3 Hilbert 公理体系

五大公理组：

| 公理组 | 描述 | Lean 文件 |
|--------|------|-----------|
| 关联公理 (Incidence) | 点、线、平面的关联关系 | Incidence.lean |
| 顺序公理 (Betweenness) | 点的介于关系 | Betweenness.lean |
| 全等公理 (Congruence) | 线段和角的全等 | Congruence.lean |
| 平行公理 (Parallel) | 平行线唯一性 | Parallel.lean |
| 连续公理 (Continuity) | 完备性 | Continuity.lean |

### 21.4 Lean 4 桥接

v5.0.0 实现了 Lv-00 <-> Lean 4 的双向桥接（Layer 10）：

- **导出**: Lv-00 证明 -> Lean 4 tactic 脚本
- **导入**: Lean 4 定理 -> Lv-00 公理包
- **同步**: 双向增量更新

### 21.5 已知限制

- 角度度量系统尚未完全形式化
- Hilbert 公理体系的机器可检验证明仍在推进中
- 核心算法正确性证明待完成

### 21.6 Lean 4 桥接详细流程

Lv-00 <-> Lean 4 双向桥接（Layer 10）的工作流程：

**导出方向 (Lv-00 -> Lean 4)**:

```
Lv-00 Proof Object
  ↓
[步骤遍历] ──→ 提取每步的命题和规则
  ↓
[规则映射] ──→ Lv-00 规则 -> Lean 4 tactic
  ↓
[类型翻译] ──→ Lv-00 类型 -> Lean 4 类型
  ↓
[代码生成] ──→ 生成 Lean 4 tactic 脚本
  ↓
[格式化] ──→ 输出 .lean 文件
```

**导入方向 (Lean 4 -> Lv-00)**:

```
Lean 4 定理声明
  ↓
[解析] ──→ 提取定理名称、类型、证明
  ↓
[类型映射] ──→ Lean 4 类型 -> Lv-00 类型
  ↓
[公理包生成] ──→ 生成 Lv-00 公理包
  ↓
[附加到引擎] ──→ 可在 Lv-00 中引用
```

### 21.7 Coq 桥接

Coq 桥接（Layer 10）支持类似的导入导出流程：

- **导出**: Lv-00 证明 -> Coq vernacular 脚本
- **导入**: Coq 定理 -> Lv-00 公理包
- **格式**: 标准 Coq .v 文件格式

### 21.8 OPML 交换格式

OPML (Open Proof Markup Language) 是 Lv-00 的开放证明交换格式：

```json
{
  "opml_version": "1.0.0",
  "source_system": "lv00",
  "target_systems": ["lean4", "coq", "isabelle"],
  "metadata": {
    "title": "三角形内角和定理",
    "author": "Lv-00",
    "date": "2026-06-04",
    "uuid": "550e8400-e29b-41d4-a716-446655440000"
  },
  "theory": {
    "language": "hilbert_geometry",
    "primitives": [...],
    "axioms": [...],
    "definitions": [...],
    "theorems": [...]
  },
  "proof": {
    "method": "algebraic",
    "steps": [...],
    "result": "PROVEN"
  },
  "extensions": {}
}
```

OPML 支持的系统：Lv-00、Lean 4、Coq、Isabelle/HOL、HOL4、Agda。

---

## 22. 本体与原始谓词

### 22.1 几何实体类型

| 实体类型 | 数学表示 | 示例 |
|----------|----------|------|
| `Point` | 二维/三维坐标 | `Point(0, 0)` |
| `Line` | 直线/射线/线段 | `Line(A, B)` |
| `Circle` | 圆心和半径 | `Circle(O, r)` |
| `Segment` | 有向线段 | `Segment(A, B)` |
| `Angle` | 角度度量 | `Angle(A, O, B)` |
| `Triangle` | 三角形 | `Triangle(A, B, C)` |
| `Polygon` | 多边形 | `Polygon([A, B, C, D])` |

### 22.2 原始谓词

约束图内核仅维护五种基本关系：

| 谓词 | 描述 | 参数 |
|------|------|------|
| 关联 (Incidence) | 点在线上 | Point, Line |
| 之间 (Betweenness) | 点在两点之间 | Point, Point, Point |
| 相交 (Intersection) | 线与线相交 | Line, Line |
| 包含 (Containment) | 区域包含点 | Region, Point |
| 连接 (Connection) | 端口相连 | Port, Port |

### 22.3 度量关系

```c
typedef struct Lv00MetricRelation {
    MetricType type;           /* DISTANCE, ANGLE, AREA, RADIUS 等 */
    GeometryEntity *subject;   /* 度量主体 */
    SymbolicCoord *value;      /* 度量值（符号坐标） */
} Lv00MetricRelation;
```

### 22.4 公理中立原则

约束图内核不内建距离、角度概念。这些由公理系统包定义，确保内核的通用性和可扩展性。

---

## 23. 约束图规范化证明

### 23.1 概述

图规范化遍引擎负责在合一检查前对约束图进行标准化处理，通过合并冗余节点确保图的唯一表示形式。

### 23.2 核心性质

1. **幂等性**: 规范化后的图再次运行规范化不会产生任何变化
2. **符号坐标判等**: 仅基于可判定的符号等价合并节点
3. **作用域感知**: 不同作用域的相同坐标点需用户确认
4. **确定性选择**: 每次合并保留最小 ID 作为代表

### 23.3 规范化流程

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

### 23.4 算法步骤

#### 第一阶段：点合并

1. **哈希分组**: 按符号坐标的哈希值分组
2. **组内精确判等**: 两两执行 `coord_equal()` 精确判等
3. **作用域检查**: 确定合并策略（自动/确认/跳过）
4. **并查集合并**: 使用 Union-Find 数据结构

#### 第二阶段：线段合并

1. 检查端点对是否等价
2. 合并方向和长度相同的线段

#### 第三阶段：区域合并

1. 检查边界线段序列是否等价
2. 合并边界相同的区域

### 23.5 规范化结果

```c
typedef struct NormalizationResult {
    bool success;
    int merged_point_count;
    int merged_segment_count;
    int merged_region_count;
    NormalizationLogEntry *log;
    int log_count;
} NormalizationResult;
```

### 23.6 触发时机

1. 合一检查（Unify）执行前自动调用
2. 用户通过界面显式请求"规范化视图"
3. 重写引擎不调用全局规范化（使用内置局部等价容忍）

---

## 24. Python 绑定与 DSL

### 24.1 概述

Lv-00 提供 Python 绑定和 DSL，支持从 Python 环境调用 Lv-00 的几何推理能力。

### 24.2 Python 模块结构

```
module/python/
├── pyproject.toml     # 项目配置
└── setup.py           # 安装脚本
```

### 24.3 DSL 设计理念

Python DSL 借鉴了以下系统的设计：

| 借鉴对象 | 借鉴内容 |
|----------|----------|
| CadQuery/build123d | Workplane 工作平面模式 |
| GAlgebra | AlgebraMode 代数模式 |
| SymPy | 操作符变换链 |

### 24.4 DSL 使用示例

```python
import lv00

# 创建工作平面
wp = lv00.Workplane()

# 定义点
A = wp.point(0, 0)
B = wp.point(3, 4)
C = wp.point(6, 0)

# 添加约束
wp.distance(A, B, 5)
wp.distance(B, C, 5)
wp.distance(A, C, 6)

# 证明
result = wp.prove("triangle_inequality", A, B, C)
print(result.status)  # PROVEN
```

### 24.5 流桥接

`module/stream_bridge/stream_bridge.py` 提供流式数据桥接，支持实时数据交换。

### 24.6 LLM 编码助手

Lv-00 提供 LLM 编码助手模块 (`module/llm_coding_assistant/`)，支持：

- **API 服务器**: RESTful API 接口
- **知识库**: Lv-00 语法和 API 的结构化知识
- **模板系统**: 常用代码模板生成
- **认证**: API 密钥认证

### 24.7 并发监控

`module/concurrent_monitor/` 提供运行时并发监控：

- 性能指标采集
- 资源使用追踪
- 异常检测和报警
- pytest 集成测试

---

## 25. Web GUI 与流监视器

### 25.1 概述

Lv-00 提供 Web GUI（基于 Tauri 框架）和流监视器，支持可视化交互。

### 25.2 Tauri 桌面应用

```
web/gui/
├── src-tauri/          # Tauri 后端
│   ├── icons/          # 应用图标
│   └── ...
└── src/                # 前端源码
```

### 25.3 可视化功能

| 功能 | 描述 | 层 |
|------|------|-----|
| 几何画布 | SVG 渲染的几何对象 | L6 |
| 节点图 | 力导向布局的函数块图 | L6 |
| 积木画布 | 拖拽式编程界面 | L6 |
| 文本编辑器 | Lv-00 DSL 代码编辑 | L6 |
| 证明可视化 | 证明树和步骤展示 | L5 |
| 流监视器 | 实时事件流监控 | L5 |

### 25.4 四视图同步

四种视图（几何画布、节点图、积木画布、文本代码）通过同步协议实时同步：

- 任何视图的修改自动反映到其他视图
- 同步协议保证语义等价
- 支持增量更新（仅同步变更部分）

### 25.5 渲染后端

| 后端 | 输出格式 | 用途 |
|------|----------|------|
| SVG | .svg | 网页渲染 |
| Cairo | .cairo | 矢量图形 |
| TikZ | .tex | LaTeX 文档 |
| Three.js | .html | 3D 交互场景 |
| PPM | .ppm | 光栅化像素 |

### 25.6 流监视器

Lv-00 的流监视器支持 47 种流式事件类型：

| 事件类别 | 示例事件 |
|----------|----------|
| 解析事件 | TOKEN_CREATED, AST_BUILT, TYPE_CHECKED |
| 几何事件 | NODE_ADDED, CONSTRAINT_ADDED, NORMALIZATION_START |
| 推理事件 | PROOF_STARTED, RULE_APPLIED, PROOF_COMPLETED |
| 输出事件 | EXPORT_STARTED, EXPORT_COMPLETED |
| 系统事件 | ERROR_OCCURRED, WARNING_ISSUED, MEMORY_THRESHOLD |

### 25.7 交互式几何

Layer 6 的交互式几何功能（`interactive_geo.c`）支持：

- **动态拖拽**: 实时更新几何对象位置
- **约束实时更新**: 拖拽时自动重新计算约束
- **动画回放**: 证明步骤的动画演示
- **测量工具**: 实时显示距离、角度等度量值

---

## 26. 配置参考

### 26.1 配置系统

Lv-00 使用集中化配置系统，所有配置键以 `LV00_CONFIG_` 为前缀。

### 26.2 构建配置

| 配置键 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `BUILD_TESTS` | BOOL | ON | 是否构建测试 |
| `BUILD_EXAMPLES` | BOOL | ON | 是否构建示例 |
| `ENABLE_WASM` | BOOL | OFF | 是否启用 WASM 构建 |
| `ENABLE_LAYER_VALIDATION` | BOOL | ON | 是否启用层级验证 |
| `ENABLE_COVERAGE` | BOOL | OFF | 是否启用代码覆盖率 |
| `ENABLE_SANITIZERS` | BOOL | OFF | 是否启用内存检测 |
| `LV00_EXCLUDE_BROKEN_PRESETS` | BOOL | ON | 是否排除有问题的预设 |

### 26.3 运行时配置

| 配置键 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `LV00_CONFIG_MAX_PROOF_STEPS` | INT | 10000 | 证明最大步数 |
| `LV00_CONFIG_MAX_REWRITE_ITER` | INT | 10000 | 重写最大迭代 |
| `LV00_CONFIG_TIMEOUT_MS` | INT | 30000 | 计算超时（毫秒） |
| `LV00_CONFIG_MEMORY_LIMIT_MB` | INT | 512 | 内存上限（MB） |
| `LV00_CONFIG_THREAD_COUNT` | INT | 4 | 并行线程数 |
| `LV00_CONFIG_LOG_LEVEL` | STRING | "INFO" | 日志级别 |
| `LV00_CONFIG_LOG_FILE` | STRING | "" | 日志文件路径 |

### 26.4 硬编码阈值

| 参数 | 当前值 | 说明 |
|------|--------|------|
| `VF2_MAX_DEPTH` | 100 | 图匹配深度限制 |
| `BUCHBERGER_MAX_STEPS` | 50000 | Groebner 基计算步数限制 |
| `POLY_REDUCE_MAX_STEPS` | 10000 | 多项式约化步数限制 |
| `REWRITE_SOLVE_MAX_ITERATIONS` | 10000 | 重写求解迭代限制 |
| `CDCL_MAX_STEPS` | 1000 | SAT 求解步数限制 |
| `CDCL_MAX_DECISIONS` | 1000 | SAT 决策次数限制 |

### 26.5 错误码系统

Lv-00 使用分层 0-999 错误码体系：

| 范围 | 模块 |
|------|------|
| 0-99 | 通用/系统错误 |
| 100-199 | 解析层错误 |
| 200-299 | 资源层错误 |
| 300-399 | 几何层错误 |
| 400-499 | 推理层错误 |
| 500-599 | 输出层错误 |
| 600-699 | 可视化层错误 |
| 700-799 | 编排层错误 |
| 800-899 | 元验证层错误 |
| 900-999 | 互操作层错误 |

### 26.6 日志级别

| 级别 | 描述 |
|------|------|
| `TRACE` | 最详细追踪信息 |
| `DEBUG` | 调试信息 |
| `INFO` | 一般信息 |
| `WARN` | 警告信息 |
| `ERROR` | 错误信息 |
| `FATAL` | 致命错误 |

---

## 附录 A: CMake Target 列表

```cmake
# 十层架构 CMake Target
lv00_layer2_resource        # Layer 2: 资源管理层
lv00_layer1_parser           # Layer 1: 输入解析层
lv00_layer3_geometry         # Layer 3: 几何拓扑层
lv00_layer4_reasoning        # Layer 4: 公理推理层
lv00_layer5_output           # Layer 5: 结果输出层
lv00_layer6_visual           # Layer 6: 图形化编程层
lv00_layer7_orchestration    # Layer 7: 编排调度层
lv00_layer8_meta_verify      # Layer 8: 元验证层
lv00_layer9_application      # Layer 9: 应用入口层
lv00_layer10_interop         # Layer 10: 外部集成层

# 聚合目标
lv00_static                  # 静态库（包含所有层）
lv00_shared                  # 共享库（可选）
```

## 附录 B: 关键词列表

```text
Point Line Circle Segment Ray Angle Triangle Polygon Scalar Bool Proposition Proof
Let Constraint Assume Assert Prove Compute Normalize Export Import Theorem Axiom
forall exists not and or implies iff true false bottom
collinear parallel perpendicular equal congruent tangent incident between on inside outside
length distance angle area midpoint center radius diameter intersect
```

## 附录 C: 符号速查表

### 逻辑符号

| 符号 | ASCII | 含义 |
|------|-------|------|
| `forall` | `forall` | 全称量词 |
| `exists` | `exists` | 存在量词 |
| `not` | `not` | 否定 |
| `and` | `and` | 合取 |
| `or` | `or` | 析取 |
| `->` | `implies` | 蕴含 |
| `<->` | `iff` | 等价 |
| `bottom` | `bottom` | 矛盾 |

### 度量符号

| 符号 | ASCII | 类型 | 含义 |
|------|-------|------|------|
| `length(A,B)` | `length(A,B)` | `Scalar` | 线段长度 |
| `distance(A,B)` | `distance(A,B)` | `Scalar` | 两点距离 |
| `angle(A,B,C)` | `angle(A,B,C)` | `Angle` | 角 |
| `area(triangle(A,B,C))` | `area(...)` | `Scalar` | 面积 |
| `radius(c)` | `radius(c)` | `Scalar` | 圆半径 |

### 约束算子

| 算子 | ASCII | 含义 |
|------|-------|------|
| `==` | `==` | 相等约束 |
| `!=` | `!=` | 不等约束 |
| `parallel` | `parallel` | 平行 |
| `perpendicular` | `perpendicular` | 垂直 |
| `congruent` | `congruent` | 全等/同余 |
| `on` / `in` | `on` / `in` | 隶属/在其上 |

## 附录 D: 已知限制

### 外部依赖限制

| 依赖 | 影响 | 说明 |
|------|------|------|
| GMP | 构建必需 | 非 WASM 构建必须依赖 GMP 库 |
| Z3/cvc5 | SMT 求解 | 需外部安装，未安装时优雅降级 |
| SuiteSparse | 稀疏求解 | CHOLMOD/UMFPACK/SPQR 集成待实现 |
| OpenMP/CUDA/HIP | 并行后端 | 需硬件和 SDK，当前仅 SERIAL 后端 |

### 形式化理论限制

- 角度度量系统尚未完全形式化
- Hilbert 公理体系的机器可检验证明仍在推进中
- 核心算法正确性证明（归一化幂等性、VF2 匹配、Groebner 基）待完成

### 模块依赖限制

- `LV00_EXCLUDE_BROKEN_PRESETS` 排除了部分有深层依赖问题的预设模块
- Windows Clang 工具链不支持 libFuzzer
- 覆盖率与 Sanitizer 同时启用可能互相干扰

## 附录 E: 路线图

| 阶段 | 时间 | 重点 |
|------|------|------|
| 2026 Q3-Q4 | 6 个月 | Lean 4 框架完善、Hilbert 公理形式化、C API 接口层、动态阈值框架 |
| 2027 Q1-Q2 | 6 个月 | 推理规则完备性、信任颜色系统形式化、策略调度优化、基准测试 |
| 2027 Q3-Q4 | 6 个月 | 论文撰写投稿、社区反馈、稳定版发布 |

### 关键里程碑

| 里程碑 | 日期 | 交付物 |
|--------|------|--------|
| M1 | 2026.07 | Lean 4 项目框架完善 |
| M2 | 2026.08 | C API 接口层完成 |
| M3 | 2026.09 | Hilbert 公理形式化完成 |
| M4 | 2026.10 | Lean 4 插件 v0.1 |
| M5 | 2026.11 | 动态阈值框架 |
| M6 | 2026.12 | 中期评审 |

### 改进维度

| 维度 | 当前等级 | 目标等级 | 关键任务 |
|------|---------|---------|---------|
| 形式化理论深度 | B- | A- | Hilbert 公理体系形式化证明、算法正确性验证 |
| 学术互通生态 | C+ | B+ | Lean/Coq 双向接口完善、OPML 生态建设 |
| 核心算法性能 | B | A- | 自适应剪枝策略、Groebner 引擎并发优化、推理效率提升 50%+ |

## 附录 F: 性能设计

### F.1 优化策略

| 策略 | 实现 | 效果 |
|------|------|------|
| SIMD 友好存储 | 对齐数据结构 | 向量化运算加速 |
| LRU 对象缓存 | 内存池管理 | 减少分配开销 |
| 多核并行调度 | 线程池 | 并行推理 |
| 增量归一化 | 变更追踪 | 避免全量重算 |
| Work-Stealing | 并行 Groebner | 负载均衡 |

### F.2 复杂度分析

| 操作 | 时间复杂度 | 空间复杂度 |
|------|-----------|-----------|
| 约束图构建 | O(n) | O(n) |
| 等价类合并 | O(alpha(n)) | O(n) |
| 归一化迭代 | O(k*n) | O(n) |
| Groebner 基 | O(d^6) | O(d^4) |
| 证明搜索 | 指数级 | 指数级 |
| SAT 求解 | NP-Complete | O(n) |
| BDD 构建 | O(2^n) 最坏 | O(2^n) 最坏 |

### F.3 运行时保护

- **熔断机制**: 防止无限循环和过度资源消耗
- **超时控制**: 可配置的计算超时（默认 30 秒）
- **内存限制**: 软内存上限控制（默认 512MB）
- **断言检查**: 开发期不变量验证

## 附录 G: BNF 文法摘要

### G.1 程序结构

```bnf
Program             ::= ModuleDecl? ImportDecl* Statement*
ModuleDecl          ::= "module" QualifiedName ";"
ImportDecl          ::= "import" QualifiedName ("as" Identifier)? ";"
Statement           ::= DeclarationStmt
                    |   ConstraintStmt
                    |   AssumeStmt
                    |   AssertStmt
                    |   ProveStmt
                    |   LetStmt
                    |   ComputeStmt
                    |   NormalizeStmt
                    |   ExportStmt
                    |   AxiomStmt
                    |   TheoremStmt
```

### G.2 几何声明

```bnf
DeclarationStmt     ::= EntityType IdentifierList ";"
EntityType           ::= "Point" | "Line" | "Circle" | "Segment"
                    |   "Ray" | "Angle" | "Triangle" | "Polygon"
                    |   "Scalar" | "Bool" | "Proposition" | "Proof"
```

### G.3 约束语句

```bnf
ConstraintStmt      ::= "Constraint" Expr RelOp Expr ";"
RelOp               ::= "==" | "!=" | "<" | ">" | "<=" | ">="
                    |   "parallel" | "perpendicular" | "congruent"
                    |   "collinear" | "tangent"
```

### G.4 证明语句

```bnf
ProveStmt           ::= "Prove" PropositionExpr ";"
PropositionExpr     ::= Expr "is" Property
                    |   Expr RelOp Expr
                    |   "forall" VarList "." PropositionExpr
                    |   "exists" VarList "." PropositionExpr
```

## 附录 H: 示例文件索引

| 文件 | 描述 | 目录 |
|------|------|------|
| `triangle_equilateral.lv00` | 等边三角形构造 | examples/library/ |
| `triangle_right.lv00` | 直角三角形 | examples/library/ |
| `circle_basic.lv00` | 基础圆操作 | examples/library/ |
| `geometry_angle_bisector.lv00` | 角平分线 | examples/library/ |
| `geometry_concyclic.lv00` | 四点共圆 | examples/library/ |
| `geometry_parallel_lines.lv00` | 平行线 | examples/library/ |
| `geometry_perpendicular.lv00` | 垂直 | examples/library/ |
| `geometry_similar_triangles.lv00` | 相似三角形 | examples/library/ |
| `geometry_midpoint_connector.lv00` | 中位线 | examples/library/ |
| `polygon_square.lv00` | 正方形 | examples/library/ |
| `polygon_regular_pentagon.lv00` | 正五边形 | examples/library/ |
| `template_triangle.lv00` | 三角形模板 | examples/templates/ |
| `template_circle.lv00` | 圆模板 | examples/templates/ |
| `template_construction.lv00` | 构造模板 | examples/templates/ |
| `template_proof.lv00` | 证明模板 | examples/templates/ |
| `template_polygon.lv00` | 多边形模板 | examples/templates/ |
| `sample_plugin.c` | 插件示例 | examples/plugin_example/ |

## 附录 I: 参考项目

Lv-00 的设计和实现参考了 73+ 个开源项目，涵盖以下领域：

| 领域 | 参考项目数 | 代表项目 |
|------|-----------|----------|
| 交互式几何 | 8 | GeoGebra, Cinderella, JSXGraph, CindyScript |
| 符号计算 | 10 | SymPy, GiNaC, Maxima, Fricas, Symbolics.jl |
| 形式化验证 | 12 | Lean 4, Coq, Isabelle, Agda, Dafny, F* |
| 约束求解 | 8 | Gecode, OR-Tools, Chuffed, MiniZinc |
| 定理证明 | 6 | Vampire, E, Z3, CVC5, Alt-Ergo |
| 几何处理 | 8 | CGAL, Geometry Central, libIGL, GUDHI |
| 可视化 | 6 | Three.js, Polyscope, Manim, Mermaid |
| 数值计算 | 8 | Eigen, FLINT/ARB, GMP, MPFI, SuiteSparse |
| 代码分析 | 7 | egg, Maude, K Framework, Rosette |

详细参考项目分析见 `doc/reference/` 目录。

---

*本文档基于 Lv-00 v5.0.0 十层架构编写，最后更新: 2026-06-04*
*许可证: MIT*
*文档生成工具: Lv-00 Documentation Generator*
