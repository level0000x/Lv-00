# 项目内部标准统一化设计（v1.0）

> 状态：设计（2026-08-27），**仅设计不执行**
> 触发：用户发现"项目内部标准需要统一化——标准尽量少，一个需求不要多种格式"
> 输入：五路子代理并行审计（DSL 面 / 序列化面 / 导出面 / 证书证明面 / 存储配置面）
> 原则：**标准尽量少，一个需求一种格式**；不同需求允许不同格式，但不得同需求多格式

---

## 1. 现状总览：一需求多格式清单

五路审计共发现 **15 组"一个需求多种格式"重复点**，按严重度分组：

### 1.1 DSL/语言面（2 组）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| D1 | 用户书写几何语言 | `.lv`（42 关键字，强类型+逻辑+证明+度量）vs `.lvz`（节式：module/deps/exports/axioms/nodes/constraints/func_blocks/presets）vs dsl_compiler（22 关键字+30 IR） | 三套能力高度重叠（point/line/circle、collinear、parallel、intersect、tangent、incidence 全都有）；用户写 `.lv`+`.lvz`，dsl_compiler 是内部通道 |
| D2 | 几何构造简写 | dsl_compiler 的 `midpoint(A,B)`/`constraint{}`/`fix`/`free` vs `.lv` 的 Let 风格 | dsl_compiler 独有：派生中心族、过点平行/垂线、concyclic、check_sat、remove_constraint、label；`.lv` 独有：量词/证明块/强类型/度量比较 |

### 1.2 序列化面（4 组）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| S1 | Module 持久化 | LVZ 文本 + MessagePack 二进制 + JSON 三种载体 | 三种对图字段处理不一致：LVZ 含图但**读写语法不匹配（只写不可读）**、MSGPACK 不含图（**自动保存/恢复会静默丢图**）、JSON 含图走权威序列化器 |
| S2 | ConstraintGraph 持久化 | JSON（`graph_serialize.c` 965 行，权威）+ LVZ 内嵌手写文本序列化器（`module_serialize.c:335-449`，语法与读端不匹配，不可用） | **第二套图序列化器冗余且不可用** |
| S3 | 图 JSON 薄封装 | `module_serialize_graph_to_json`（module_serialize_json.c:158-191）vs `graph_serialize_to_json` | 纯薄封装，生产无调用，仅测试 |
| S4 | 格式枚举 | `ModuleFormat`（module.h:163-166，LVZ/MSGPACK）vs 实际三种载体 | **死枚举**：全库零引用、且漏 JSON |

### 1.3 导出面（4 组）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| E1 | 图→SVG | `interop_export_graph_svg`（26 行瘦版）vs `interop_export_svg`（956 行全功能） | 同一需求两实现，命令路径走瘦版丢信任色 |
| E2 | 图→TikZ | `interop_export_graph_tikz`（22 行）vs `lv_tikz_export`（303 行）vs `lv_render_visitor_tikz`（202 行） | **三套**图→TikZ，纯语义重复 |
| E3 | 证明→Coq/Lean | `interop_export_coq/lean` + `coq_bridge/lean4_bridge` 插件 + `proof_export_enhanced` + `proof_export` + `interop_theorem` | Coq 4-5 个实现、Lean 3-4 个实现，多数仅测试接线 |
| E4 | 图→canonical | ExportGraph `canonical`（=json 别名）vs L9 EXPORT `canonical`（`interop_export_canonical.c` 独立文本 NODES/CONSTRAINTS） | **同名不同义**，最危险 |

### 1.4 证书/证明面（4 组）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| P1 | 证明 JSON 视图 | OPML JSON（opml_codec.c）vs `lv_proof_compiler_to_json`（proof_compiler.c） | 同一逻辑证明两份 schema |
| P2 | 证明内存表示 | ProofStep（ProofNavigator）+ lvProofObject/lvProofStepRecord（proof 链）+ lvProofStep（opml 内部） | ≥3 套内存树，同一证明被重复持有+多次重编码 |
| P3 | .proof.cert 证书正文 | 三层并存：A 规范 ProofStep + B Lean + C Coq/OPML（设计承诺） | 层 A 引用 `interop_export_canonical` 但该函数序列化 **ConstraintGraph 非 ProofStep**——文档承诺未实现（语义漂移） |
| P4 | Lean/Coq 输出文件 | `bootstrap/output/coq_proof.lean`（**实为 Lean 代码**）、`lean4_formal.lean`、`proof.lean` | 命名错位：Coq 导出没产出 `.v`，却产出同名 `.lean` |

### 1.5 存储/配置面（4 组，其中 2 组属工具链绑定应保留）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| C1 | Python 打包 | `pyproject.toml` + `setup.py` | **双格式漂移**：dynamic 字段无法解析，setup.py 被架空但残留元数据 |
| C2 | 预设（preset）数据 | preset_*.c（死代码）→ module/presets/*.lvz（活数据源）→ preset_registry.yaml（无消费方）→ python preset_*.py（镜像） | **4 表示并存**，所有权不清+镜像漂移 |
| C3 | Lean 项目 | formal/（lakefile.toml 声明式）vs lv-formal/（lakefile.lean 命令式）vs TestLake/ | 两个同名 `lvFormal`，工具链/mathlib/lakefile 语法全漂移 |
| C4 | 公理包注册表 | INDEX.json + 各包 manifest.json + package_template.json | 双层 JSON 字段重复，仅 2 包有 manifest |

---

## 2. 统一化原则

1. **标准尽量少**：每个需求收敛到一个权威格式；删除冗余/不可用实现。
2. **一需求一格式，不同需求允许多格式**：图可视化（SVG/TikZ/DOT）、证明导出（Lean/Coq/OPML）、代码生成（C/Python/JS）是**不同需求**，各自保留；但"同一需求的两个实现"必须合并。
3. **权威格式锚定**：每类对象指定唯一权威格式（ConstraintGraph→JSON、Module→JSON、证明→OPML、证书→.proof.cert），其他格式要么去重，要么降级为"别名/薄封装"。
4. **向后兼容优先**：删除冗余实现前确认无生产调用方；有外部消费的格式（.lvz 公理包）保留但收敛内部结构。
5. **不推翻十层架构**：统一化是"格式收敛+死代码清理"，不改变层间依赖方向。

---

## 3. 分面统一方案

### 3.1 DSL/语言面（D1-D2）

**决策**（承接 dsl-syntax-baseline v1.1 三格式图景）：
- **用户语言锚定 `.lv`**（lv_LANGUAGE_SPEC 有完整 BNF + 强类型 + 逻辑 + 证明 + 度量，能力最全）。
- **`.lvz` 保留但职责收敛**：仅作"模块封装格式"（module/deps/exports），其中的 nodes/constraints 节与 `.lv` 声明重复 → 长期由 `.lv` 内嵌模块取代（`.lv` 增加 `Module` 声明）；`axioms` 节保留（按名引用公理包）。
- **dsl_compiler 独有能力反向移植到 `.lv`**：midpoint/circumcenter/orthocenter/centroid/incenter/bisector 派生构造、fix/free、concyclic、过点平行/垂线、check_sat、remove_constraint、label。移植完成后 dsl_compiler 降级为"内部便捷通道"，不再视为语言。
- **语法糖落点**：全部加在 `.lv`（见 dsl-syntax-sugar-design v1.3）。

### 3.2 序列化面（S1-S4）

**决策**：
- **ConstraintGraph 唯一权威 = JSON**（`graph_serialize_to_json`）。删除 LVZ 内嵌手写图序列化器（module_serialize.c:335-449）与 `module_serialize_graph_to_json` 薄封装。
- **Module 唯一权威 = JSON**（`module_serialize_to_json`）：内嵌权威图 JSON，文字友好，与前端对齐。MessagePack 二进制路径**降级/删除**（不含图=不完整，自动保存恢复丢图是真实缺陷）；LVZ 文本路径保留仅当有外部 .lvz 兼容约束（否则并入 JSON）。
- **ModuleFormat 死枚举**：删除或补齐 3 值并真正接线。
- **meta_repr_verify_roundtrip 硬编码分支**：并入注册表分派或删除。

### 3.3 导出面（E1-E4）

**决策**（按需求归类，每类保留权威实现）：
- **图可视化**：SVG → 只留 `interop_export_svg`（956 行全功能），删除 26 行瘦版；TikZ → 只留 `lv_tikz_export`（303 行），删除另两套；DOT 保留（多导出器从不同对象产生 DOT 属正常功能分化）。
- **canonical 语义漂移**：ExportGraph 的 `canonical` 别名移除或改名（避免与 L9 的 canonical 混淆）；L9 `interop_export_canonical` 明确为"约束图规范文本"。
- **证明导出**：Coq/Lean 收敛到 **L10 插件（coq_bridge/lean4_bridge）+ interop_export_coq/lean 命令路径**二选一（建议命令路径），删除仅测试的 proof_export_enhanced/proof_export/interop_theorem 导出分支；OPML 为证明交换标准中间格式。
- **ga_codegen**（6 目标，无调用方）：保留公共 API（供双模式执行设计复用），暂不删除，标注"待接线"。

### 3.4 证书/证明面（P1-P4）

**决策**：
- **证明标准中间表示 = `lvProofObject`**（proof.h，L4）：统一所有导出/导入路径消费这一个 IR；淘汰 opml 内部 lvProofStep（第三份拷贝）；ProofNavigator 物化为 lvProofObject。
- **证明 JSON 统一为 OPML JSON**：proof_compiler 的 lv_proof_compiler_to_json 改为 OPML schema 或删除。
- **.proof.cert 层 A 修正**：文档承诺的"canonical ProofStep"需实现（基于 lvProofObject 的 ProofStep 序列化），或把层 A 改为引用 OPML JSON（避免引用实际是 ConstraintGraph 序列化的函数）。
- **P4 命名错位**：bootstrap/output/coq_proof.lean 改名为 coq_proof.v（或删除，Coq 导出应产出 .v）；Lean 输出统一经 interop_export_lean。

### 3.5 存储/配置面（C1-C4）

**决策**：
- **C1 Python 打包**：删 setup.py，只留 pyproject.toml（PEP 621）；修正 dynamic 字段。
- **C2 预设数据**：锚定 module/presets/*.lvz 为唯一数据源；删 preset_registry.yaml（无消费方）；preset_*.c 标记"仅供 convert 生成"或清理；python preset_*.py 从 .lvz 生成（单一事实源）。
- **C3 Lean 项目**：formal/ 与 lv-formal/ 合并（同名 lvFormal 二选一，建议保留 formal/ 的 lakefile.toml 声明式），统一 mathlib rev 与工具链。
- **C4 公理包注册表**：INDEX.json 为唯一注册表，manifest.json 并入或删除（仅 2 包有）。

---

## 4. 优先级与工作量

| 批次 | 内容 | 工作量（估） | 风险 |
|---|---|---|---|
| **P0 死代码/冗余清理** | S2 删 LVZ 内嵌图序列化器、S3 删薄封装、S4 删死枚举、E1 删 SVG 瘦版、E2 删 TikZ 两套、P4 改名、C1 删 setup.py | ~600-900 行删除 | 低（多为无调用方） |
| **P1 权威格式收敛** | S1 Module→JSON（含修复丢图缺陷）、E4 canonical 改名、C2 预设单一数据源、C4 注册表合并 | ~400-700 行改动 | 中（需回归测试） |
| **P2 语言统一** | D1-D2：.lv 吸收 dsl_compiler 独有能力 + .lvz 职责收敛 + 语法糖第一批 | ~1500-2500 行 | 中高（语法面） |
| **P3 证明 IR 统一** | P1-P3：lvProofObject 为唯一 IR、证明 JSON→OPML、证书层 A 修正 | ~800-1400 行 | 中高（证明引擎） |
| **P4 项目级合并** | C3 Lean 项目合并 | 视工具链 | 高（外部工具链） |

---

## 5. 风险与约束

| 风险 | 缓解 |
|---|---|
| .lvz 有 149 个文件，外部/历史消费 | P0 不删 .lvz；只删 .lvz **内部**冗余图段；格式收敛留 P2 |
| Module 恢复丢图是缺陷非格式问题 | P1 随 JSON 收敛一并修复（恢复路径改走 JSON） |
| 证明引擎改动波及 L8 元验证/L9 证书 | P3 在 lvProofObject 上做，消费方（meta_verify）已用该 IR，改动收敛 |
| Lean 工具链版本差异 | P4 最后做，需 CI 验证两个项目等价后再合并 |
| ga_codegen 无调用方但双模式执行设计复用 | 保留公共 API，标注"待接线"，不删 |

---

## 6. 决策点（待确认）

- **F1**：P0 死代码/冗余清理是否作为独立批次优先执行（纯删除、低风险、符合"不留工程债务"）？
- **F2**：Module 序列化收敛到 JSON 是否确认（MessagePack 路径删除，修复恢复丢图）？
- **F3**：canonical 语义漂移处理方式（ExportGraph 别名改名 vs 删除）？
- **F4**：语言统一是否按 P2 推进（.lv 吸收 dsl_compiler 能力，承接语法糖 v1.3）？
- **F5**：Lean 项目合并（formal/ vs lv-formal/）是否纳入范围？

---

*附：本设计基于五路子代理审计（DSL/序列化/导出/证书证明/存储配置五面 15 组重复点），
全部为设计深化，不执行。执行顺序待用户确认。*
