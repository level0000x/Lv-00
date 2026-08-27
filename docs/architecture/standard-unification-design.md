# 项目内部标准统一化设计（v1.1）

> 状态：设计（2026-08-27），**仅设计不执行**
> 触发：用户发现"项目内部标准需要统一化——标准尽量少，一个需求不要多种格式"
> 输入：**两轮共十路**子代理并行审计（第一轮：DSL/序列化/导出/证书证明/存储配置；
> 第二轮：错误码API/坐标代数/测试框架/Python绑定/工具链脚本）
> 原则：**标准尽量少，一个需求一种格式**；不同需求允许不同格式，但不得同需求多格式
>
> **v1.1（2026-08-27）**：用户追加"再开子代理找其他方面"——第二轮五路审计新增
> 15 组重复点（总 **30 组**），本版并入 §1.6-1.10 与 §3.6-3.10。

---

## 1. 现状总览：一需求多格式清单

两轮共十路审计发现 **30 组"一个需求多种格式"重复点**：

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

### 3.6 错误码/API 面（第二轮，E5-E7）

**决策**：
- **E5 模块级错误码收敛**：以 `error_codes.h` 的 `lvErrorCode`（68 码 13 类，X-macro 单一事实源）为唯一权威；≥35 个模块级 `*Result`/`*Status` 枚举（EngineStatus/GeoStatus/SolverStatus/ModuleLoadStatus/AxiomLoadStatus/PackResult/ATPResult/VerifyResult 等）**保留业务语义但补 `→ lvErrorCode` 桥接映射**（现状仅 constraint_graph.h 做了），调用方统一消费 lvErrorCode；同域重复枚举合并（prop_verifier 的 VerifyResult vs proof 的 LvProofVerifyResult 成对重复）。
- **E6 返回码约定统一**：7 种约定（int 负哨兵 614 处 / lv_RETURN_ERROR 家族 2243 处 / lvErrorCode / 模块枚举 / lvResult 结构体 / bool / int64 混合）收敛为 **lv_RETURN_ERROR 家族 + lvResult 结构体**两级：API 边界用 lvResult，内部快速失败用 lv_RETURN_ERROR；int 负哨兵直返不设 TLS 的逐步替换。
- **E7 公共 API 入口收敛**：导出/求解/证明各保留 2 级入口（便捷层 + 底层直调），删除/降级重复的中间层（interop 命令层与 lv_impl_upper 层二选一为"命令入口"）；**导出格式枚举本身重复定义 2+ 套**（interop / upper / proof_compiler）合并为单一枚举。

### 3.7 坐标/代数表示面（第二轮，E8-E10）

**决策**：
- **E8 有理数 4→1**：`Rational`（symbolic_coord.h）与 `lvRational`（rational.h）字面同构（都是 `{mpq_t}`）→ 合并为唯一 `lvRational`；`AlgRational`（int64 无依赖栈）保留为"无 GMP 环境降级层"但明确边界；表达式叶子 mpq_t 属表达式的内嵌表示（不同需求，保留）。
- **E9 两套精确代数数实现收敛**：GMP 栈（Algebraic/Quadratic + mpz_poly_t，SymbolicCoord 活跃使用）为**权威**；int64 栈（AlgRational/AlgQuadratic/AlgPoly/AlgInterval，自包含但未接入 SymbolicCoord）保留作"无依赖后端"但**补转换桥**或标记为独立可选后端（与 E8 联动）。
- **E10 区间 3 套语义基准**：`interval_arith.h` 已收敛到 `lvInterval`，但 float_error 语义与 IEEE 1788 空区间语义**并存而非重复**（文档明示）→ 明确 API 前缀分工（`lv_interval_*` vs `interval_*`）并加语义注释防误用；`AlgInterval`（精确隔离）是第三类需求（代数根隔离 vs 浮点误差界），保留分层。

### 3.8 测试框架面（第二轮，E11-E12）

**决策**：
- **E11 断言基座统一**：以 `test_framework.h`（核心库正式路径）的 `lv_ASSERT_*`（8 宏）为**唯一权威断言**；`test_helpers.h` 的 `TEST_ASSERT_*`（163 文件使用）降级为**兼容别名层**，但**必须先修正参数序冲突**：`TEST_ASSERT_EQ(actual, expected)` vs `lv_ASSERT_EQ(expected, actual)` 参数序相反（最高危）；`TEST_ASSERT_DOUBLE/NEAR` vs `lv_ASSERT_FLOAT_EQ` 同样反转；补新式缺失的 `TEST_ASSERT_NULL`/`_MSG`/`_CONTINUE` 能力。
- **E12 测试入口三套归一**：旧式 `TEST_MAIN_*`（287/288 文件）+ 结构化 `lv_TEST`+`run_all`（1 文件）+ 数据驱动混合（54 文件）→ 统一为**数据驱动结构化入口**（`lv_TEST` 自动注册 + `run_all` + `lvTestReport`），旧式 MAIN 保留为薄壳；计数系统 `g_pass/g_fail` 与 `lvTestReport` 对齐（axiom 头已做桥接，推广到全部）。

### 3.9 Python 绑定面（第二轮，E13-E14）

**决策**：
- **E13 链式 DSL 三套归一**：`dsl_context.G`（790 行）+ `py_euclid_style`（1216 行）+ `dsl_algebra`（751 行）三套"链式几何 DSL"语义重复 → 保留 **dsl.py 家族**（G 上下文为唯一用户入口），py_euclid_style 与 dsl_algebra 降级为兼容 re-export 或删除；消除 `dsl_wrappers ↔ dsl_context` 循环 import。
- **E14 预设注册表三套 + 参数入口收敛**：`preset_basic`（v3.3.0）+ `preset_blocks/`（v4.0.0）+ `math_presets`（第三方 spec）三套独立注册表 → 锚定 **.lvz（C 侧唯一数据源）→ preset_blocks（v4.0.0）**，v3.3.0 系降级为 compat 层；同一几何操作（midpoint 等）Python 侧 5-8 个入口（core.Point / G / Wrapper / algebra / euclid / fallback / preset×2）收敛为 **core 权威 + G 便捷**两级。
- **绑定基座健康确认**：`_ctypes_binding.py` 是唯一 CDLL 基座、常量单一来源（✅ 保留）；但补运行时版本校验（docstring 宣称版本检查与实际不符，P3 文档失同步）。

### 3.10 工具链脚本面（第二轮，E15）

**决策**：
- **E15 脚本归一**：
  - `tool/` 与 `tools/` 同级撞名 → **归一为 tool/**（tools/ 并入）；
  - `tool/scripts/`、`tool/tools/`、`tool/report_generators/` 三子目录概念重叠 → 按功能重组；
  - CMakeLists 修复双实现（fix_build.py ≡ fix_cmake.ps1）→ 保留一个（建议 py）；
  - 桩生成（gen_stubs.ps1 硬编码清单 vs fix_build.py 扫 CMakeLists）→ 保留扫 CMakeLists 版；
  - 预设生成链三工具（convert_presets / check_preset_sync / extract_preset_data）各自维护 PRESET_TYPE_MAP → **单一常量源**，且 `check_preset_sync.py` **纳入 CI**（现缺口）；
  - docx 汇报双栈（JS 生成器 vs gen_report.py python-docx）→ 保留一个；
  - 构建 9 个并行目录无 CMakePresets.json → **新增 CMakePresets.json 归一**（build/build3/build_symcheck 等命名化）；
  - **无真平台孪生脚本**（21 个 .sh 全在第三方依赖，项目自身 0 个）——ps1⇄sh 分化不成立，无此负担。

---

## 4. 优先级与工作量（v1.1 更新：30 组）

| 批次 | 内容 | 工作量（估） | 风险 |
|---|---|---|---|
| **P0 死代码/冗余清理** | S2 删 LVZ 内嵌图序列化器、S3 删薄封装、S4 删死枚举、E1 删 SVG 瘦版、E2 删 TikZ 两套、P4 改名、C1 删 setup.py、E11 断言参数序修正、E15 目录归一+双脚本去重 | ~900-1300 行删除/改名 | 低（多为无调用方） |
| **P1 权威格式收敛** | S1 Module→JSON（含修复丢图缺陷）、E4 canonical 改名、C2/C14 预设单一数据源、C4 注册表合并、E5 错误码桥接映射、E8 有理数合并、E13 DSL 三套归一、E15 CMakePresets | ~1200-2000 行改动 | 中（需回归测试） |
| **P2 语言统一** | D1-D2：.lv 吸收 dsl_compiler 独有能力 + .lvz 职责收敛 + 语法糖第一批 | ~1500-2500 行 | 中高（语法面） |
| **P3 证明/API IR 统一** | P1-P3：lvProofObject 为唯一 IR、证明 JSON→OPML、证书层 A 修正、E6 返回码约定收敛、E7 API 入口收敛、E12 测试入口归一 | ~1500-2500 行 | 中高（证明引擎） |
| **P4 项目级合并** | C3 Lean 项目合并、E9 代数数桥接、E10 区间语义分工、E14 预设注册表 v3→v4 | 视工具链 | 高（外部工具链） |

> v1.1 工作量上调主因：第二轮新增 E5-E15 涉及错误码桥接（≥35 枚举）、
> 断言参数序修正（影响 163 测试文件）、Python DSL 三套归一等广面改动。

---

## 5. 风险与约束

| 风险 | 缓解 |
|---|---|
| .lvz 有 149 个文件，外部/历史消费 | P0 不删 .lvz；只删 .lvz **内部**冗余图段；格式收敛留 P2 |
| Module 恢复丢图是缺陷非格式问题 | P1 随 JSON 收敛一并修复（恢复路径改走 JSON） |
| 证明引擎改动波及 L8 元验证/L9 证书 | P3 在 lvProofObject 上做，消费方（meta_verify）已用该 IR，改动收敛 |
| Lean 工具链版本差异 | P4 最后做，需 CI 验证两个项目等价后再合并 |
| ga_codegen 无调用方但双模式执行设计复用 | 保留公共 API，标注"待接线"，不删 |
| 断言参数序修正波及 163 测试文件 | P0 先加编译期静态检查（参数序可静态检测的宏包装），再分批改 |
| 错误码桥接 ≥35 枚举工作量大 | P1 按调用频次分批补映射，先补生产消费点多的（Module/Axiom/Graph） |
| Python DSL 三套归一可能破坏用户脚本 | P1 保留 dsl.py 兼容 re-export，删除前跑 Python 测试套件 |

---

## 6. 决策点（待确认）

- **F1**：P0 死代码/冗余清理是否作为独立批次优先执行（纯删除、低风险、符合"不留工程债务"）？
- **F2**：Module 序列化收敛到 JSON 是否确认（MessagePack 路径删除，修复恢复丢图）？
- **F3**：canonical 语义漂移处理方式（ExportGraph 别名改名 vs 删除）？
- **F4**：语言统一是否按 P2 推进（.lv 吸收 dsl_compiler 能力，承接语法糖 v1.3）？
- **F5**：Lean 项目合并（formal/ vs lv-formal/）是否纳入范围？
- **F6**（第二轮）：错误码桥接（E5）+ 返回码约定收敛（E6）是否纳入 P1/P3？
- **F7**（第二轮）：断言参数序修正（E11）是否优先做（P0，纯测试面安全）？
- **F8**（第二轮）：Python 链式 DSL 三套归一（E13）保留 dsl.py 为唯一入口是否确认？

---

*附：本设计基于两轮十路子代理审计（第一轮 DSL/序列化/导出/证书证明/存储配置 15 组 +
第二轮 错误码API/坐标代数/测试框架/Python绑定/工具链脚本 15 组 = 30 组重复点），
全部为设计深化，不执行。执行顺序待用户确认。*
