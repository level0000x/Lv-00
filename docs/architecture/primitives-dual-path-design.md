# Lv-00 原语地基与双路径执行 —— 设计讨论稿（待议）

> **状态**: 待议设计（2026-09-03，用户指示"先文档化作为待议"）
> **范围**: 大规模重构蓝图的前置讨论——13 几何原语 C 地基现状、双路径执行
> （C 加速 vs 自举）、"语言上层→原语层每层可调"路由设想、Lean 锚定层。
> **关联文档**: geometric_primitives.md（13 原语规范）/ dual-mode-execution-design.md
> （解释 vs 编译为机器码）/ dsl-syntax-baseline.md（三格式图景）/ bootstrap_test_framework.md
> （双路径差分测试）/ dsl-syntax-sugar-design.md（语法糖落点）
>
> 本文档**只记录现状与待议问题，不含任何已批准的执行决策**。用户 2026-09-03
> 口头方向：①让 Lv-00 自己的语言能驱动自身全部能力（13 原语为地基）；②大规模
> 重构；③双路径（C 直调 / 自举实现）行为等价、Lean 证明锚定原语语义层不随
> 语法变动；④路由"从实际的语言上层至原语层都是可调的"。均**待议未决**。

---

## 一、现状盘点（调研结论，批次 229 核实）

### 1.1 13 原语 C 实现已存在但被遗忘

`core/src/layer4_reasoning/geometric_primitives.c`（440 行）+ `core/include/lv/geometric_primitives.h`
（221 行）**已完整实现 13 个 geo_* 原语**，基于 doc/docs/geometric_primitives.md，
编译进构建（CMakeLists.txt:1133）。GeoResult/GeoStatus 统一返回 + LV_DISPATCH 状态分发。

| # | 原语 | geo_ API | C 实现 | 契约测试 | .lv 接线 |
|:--:|:--|:--|:--:|:--:|:--:|
| 1-2 | create-node / create-constraint | geo_create_node / geo_create_constraint | ✅ | ✅(批次229) | ❌ |
| 3-4 | solve / normalize | geo_solve / geo_normalize | ✅ | ✅ | ❌ |
| 5-6 | rewrite / unify | geo_rewrite / geo_unify | ✅ | ❌ | ❌ |
| 7-8 | pack / instantiate | geo_pack / geo_instantiate | ✅ | ❌ | ❌ |
| 9-10 | prove / export | geo_prove / geo_export | ✅ | ❌ | ❌ |
| 11-13 | serialize / deserialize / query | geo_serialize / geo_deserialize / geo_query | ✅ | ✅ | ❌ |

**未使用面**：全库零 .lv 接线、未入 lv.h 伞头、无 lv_PUBLIC_API（K59 批次 226
登记"库内部零装饰头"）、批次 229 前零测试。

### 1.2 批次 229 已修复/登记的缺陷

| 缺陷 | 状态 |
|:--|:--|
| GEO_NODE_CIRCLE 枚举空洞（handler 表无 CIRCLE 槽） | ✅ 修复（d2ab55ee） |
| geo_query 缺 type/stats 查询模式 | ✅ 修复（7b186124） |
| geo_* 零测试 | ✅ 修复（test_geometric_primitives，6 契约） |
| geo_export 缺 lean（文档列 lean/dot/json） | 📝 登记：interop.h 属 L5，L4 原语层不可依赖 → lean 由上层包装 |
| geo_instantiate 实调 engine_instantiate_function（非文档 func_block_instantiate） | 📝 登记：引擎级实例化语义，保留现状 |
| 两套 13 名（geo-* vs bootstrap_test_primitive point_construct 占位） | 📝 登记：用途不同不合并 |

### 1.3 现状执行模型（混合型、主体解释）

```
DSL 源码 → Tokenizer → AST → [dsl_compile: AST→IR] → [dsl_ir_to_constraint_graph: IR→图]
                                     ↑ 真正的编译       ↑ 解释执行（VTable 分发 30 操作码）
→ 约束求解 → 证明 → [interop_export_lean/coq: 证明→源码]
```

- `.lv`（lv 家族，用户书写）→ lv_loader（AST 遍历直接驱动引擎，纯解释）
- dsl_compiler（引擎内部通道）→ AST→IR（30 op）→ VTable 分发解释
- 三套语法：lv 家族 `.lv`（140 文件）+ `.lvz`（149 文件）+ dsl_compiler（22 关键字，非用户语言）

### 1.4 .lv 语言对系统能力的触达（缺口精确度量）

- 已接线：声明 Point/Line/Segment → graph_*；Constraint（4 关系+距离 hack）；Prove（微自举 B 验证器）
- 解析即止（空转）：Normalize / Export / Compute / Axiom / Theorem / Module / Import
- **语言不可达**：geo_normalize/rewrite/unify/pack/instantiate/serialize/deserialize/query
  → 即"13 原语约 10 个在语言层摸不到"

### 1.5 Lean 形式化锚定层（三层，与 C 解耦）

| 层 | 位置 | 依赖 | 特征 |
|:--|:--|:--|:--|
| Hilbert 几何公理 | formal/lv + lv-formal Hilbert | 纯 Mathlib | 真实数学，与 C 无关 |
| 自建理论核心 | lv-formal Predicates（六本原谓词）/ConstraintGraph/Normalization | 自建 | 与 C 词汇不同但语义最近 |
| 论文级 DSL/IR | lv-formal lvLang/Compiler/BootstrapDefs | 自建 | compile_lv=[] 占位 |

**结论**：无任何 .lean 引用 geo_*/graph_*/.lv 解析器 → 改语法/原语 C 包装**不破坏
任何 .lean 编译**（用户判断成立）。把 Lean 锚定到"原语语义层"= 新增翻译层，与语法解耦。

---

## 二、既有双模式执行设计（dual-mode-execution-design.md 摘要）

> 状态：草案 v0.1（2026-08-27），动机=用户「CPython 和 Python 的比喻」

| 模式 | 机制 | 适用 |
|:--|:--|:--|
| A 解释（默认） | IR→图 VTable 逐条分发 | 交互/调试/小规模 |
| B 编译（可选） | IR→C 生成 → 编译 .so/.dll → lv_dlopen | 部署/复用/大规模 |
| AUTO | 缓存命中编译，否则解释（类比 JIT 预热） | 默认建议 |

含：IR→C 代码形态示例、模块划分（ir_codegen_c/compile/load/cache ~1750-2650 行）、
缓存（源码 SHA-256 + 版本）、切换 API（lvDslExecMode）。**边界声明**：几何求解是
主要耗时，图构造解释开销小——双模式收益在部署/复用，非图构造微秒级。

**既有可复用设施**：ga_codegen（6 目标代码生成先例）、lv_dlopen/dlsym、lv_external_process_run、
interop_export_lean/coq（源码级编译）、meta_repr_graph_equivalent（双路径等价比较）。

---

## 三、用户新设想（待议核心）

### 3.1 双路径的两个维度（易混淆，先厘清）

| 维度 | 路径 A | 路径 B | 类比 |
|:--|:--|:--|:--|
| **执行模式**（既有 dual-mode） | 解释执行（VTable 分发） | 编译为机器码（IR→C→.so） | CPython 解释器 / Cython |
| **实现后端**（用户新设想） | C 直调（geo_* → graph_*） | **自举实现**（用 lv 语言实现原语语义） | 参考实现 / 自举编译器 |

- 用户新设想 = **后端双路径**：同一 13 原语语义，C 直调 vs 用 Lv-00 自己的语言实现，
  **行为等价**；Lean 证明锚定原语语义层（不随选择的后端变）。
- 既有 dual-mode = **同一 C 实现内的解释/编译**执行方式选择。
- **两者互补**：未来可组合为"后端选择 × 执行模式"矩阵；共享方法论=
  bootstrap_test_framework.md 的差分测试（同输入 C API vs 几何层结果比对）。

### 3.2 "每层可调"路由设想（用户原话：从语言上层至原语层都要可调）

```
┌─ 语言上层（.lv 语法/desugar 策略）←─ 可调？
├─ 中间表示（IR/操作码）←─ 可调？
├─ 原语层（13 geo_*）←─ 可调：每条原语选后端（C 直调 / 自举）
└─ 底层实现 ←─ 可调：执行模式（解释 / 编译）
```

用户表述："会更复杂一点，到达实际的语言上层至原语层都是要可调的"、"一定是需要
完整接原语层的"、"可调参数让用户选择哪一个部分走原语哪一部分直接走 c"。
**待议**：路由粒度模型（per-语句？per-原语？per-阶段？）、配置形态（编译期宏？
运行期注册表？每图配置？）、默认策略。

### 3.3 用户确认的大方向（均待议未决）

1. **大规模重构**已准备；多数既有设计"偏了一堆但大多数设计没问题"
2. DSL 全面升级 = 让 .lv 触达系统全部能力（13 原语为汇编层）
3. 两套 DSL 合一为长期方向（lv 家族为唯一用户语言）
4. geo_* 是否升级为正式导出 API（lv_PUBLIC_API + 入伞头）待定

---

## 四、待议问题清单（供讨论，未决）

### A. 地基接线
- A1. geo_* 13 原语是否升级为正式 API（lv_PUBLIC_API + 入 lv.h 伞头）？
- A2. .lv desugar 终点是否从散装 graph_*/lv_add_* 改为 geo_* 原语调用？
- A3. .lv 的 Normalize/Export/Prove 语义如何接上 geo_normalize/geo_export/geo_prove
  （Prove 现在是微自举 B 验证器 ≠ geo_prove 的 proof_search）？
- A4. geo_export 的 lean/dot/json 分层：lean 属 L5+，是否在 lv_upper_api/interop 层补包装？

### B. 唯一原语注册表
- B1. 是否建立"原语注册表"（名字→C 函数指针→参数 schema→语义引用→后端选择）为唯一事实源？
- B2. bootstrap_test_primitive 旧 13 占位名与 geo-* 的关系（废弃/别名/保留）？

### C. 双路径架构
- C1. 后端双路径（C 直调 vs 自举实现）如何建模？差分测试（bootstrap_diff_test）
  是否按 13 原语逐条建立可执行契约？
- C2. "每层可调"路由的粒度模型与配置形态？
- C3. 自举实现 = 用 lv 语言写原语语义 → 又回到"语言能力"——鸡生蛋问题如何破
  （先语言能表达什么，才能写自举原语）？
- C4. 与既有 dual-mode（解释/编译机器码）的关系：合并为统一执行矩阵 or 分阶段？

### D. Lean 锚定
- D1. 是否把 Lean 锚定到"原语语义层"（新增原语→Predicates/Constraint 模型翻译层）？
- D2. 复用哪套 Lean 资产：lv-formal 自建理论核心（六本原谓词）vs formal 大库？

### E. 两套 DSL 合一（长期）
- E1. dsl_compiler 30 IR 操作码与 13 原语关系：并入/映射/废弃？
- E2. 合一后 lv 家族语法面（声明/约束/证明/函数块/预设）如何补全？

### F. 缺陷遗留
- F1. geo_export lean/dot、geo_query type/stats 已登记；geo_instantiate 底层语义待确认。

---

## 五、参考证据索引

- geometric_primitives.c/.h（13 原语实现）：core/src/layer4_reasoning/、core/include/lv/
- 自举规范：doc/docs/geometric_primitives.md、phase1_meta_representation.md、bootstrap_test_framework.md
- 执行模型：docs/architecture/dual-mode-execution-design.md、design-level-opportunities.md §8
- 语法基线：docs/architecture/dsl-syntax-baseline.md、dsl-syntax-sugar-design.md（v1.3 落点）
- Lean：lv-formal/lvFormal/Theory/{lvLang,IR,Compiler,BootstrapCorrectness,DSLWrappersSoundness}.lean
- 批次登记：TASK_CONTEXT.md 批次 229（调研 + 修复）

---

*本文档为待议讨论稿；用户 2026-09-03 指示先文档化。下一讨论回合将基于
§四待议问题逐项收敛，产出《13 原语 C 地基 + 双路径 DSL》正式设计（若获批准）。*
