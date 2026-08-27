# 项目内部标准统一化设计（v1.18）

> 状态：设计（2026-08-27），**仅设计不执行**
> 触发：用户发现"项目内部标准需要统一化——标准尽量少，一个需求不要多种格式"
> 输入：**十八轮共九十路**子代理并行审计（第一轮：DSL/序列化/导出/证书证明/存储配置；
> 第二轮：错误码API/坐标代数/测试框架/Python绑定/工具链脚本；
> 第三轮：引擎生命周期/推理后端/流协议/内存日志/前端文档；
> 第四轮：公式表达式/导入解析/类型系统/基础工具/证明内部；
> 第五轮：安全限制/缓存/图算法/事件回调/配置全局；
> 第六轮：插件系统/快照回滚/数值线性代数/形式化对齐/Python测试结构；
> 第七轮：进程网络/性能监控/数据结构/IO序列化/层间依赖；
> 第八轮：init清理生命周期/线程并发/构建产物CI/Python包结构/全局符号命名；
> 第九轮：错误消息文案/基础算法重复/文档代码一致性/版本兼容/示例教学代码；
> 第十轮：废弃API兼容层/降级回退路径/基准测试/特性开关/内存所有权约定；
> 第十一轮：测试替身/数学常量/错误处理模式/适配器桥接/全局状态线程安全；
> 第十二轮：头文件包含卫生/fuzz测试配置/打包发布/数值稳定性/声明实现一致性；
> 第十三轮：代码风格格式/魔法字符串/序列化互转矩阵/测试数据生成/硬编码路径；
> 第十四轮：错误注入容错/UB整数安全/递归栈深度/死锁锁顺序/宏约定类型转换；
> 第十五轮：字符串处理设施/状态机实现模式/文件IO资源管理/命令参数解析/编码字符处理；
> 第十六轮：调试诊断追踪/数学函数数值原语/时间日期时钟/协议编解码消息帧/构建系统组织；
> 第十七轮：几何判定谓词/网络套接字IO/对象池资源池复用/配置项定义校验/API参数校验契约；
> 第十八轮：哈希表查找设施/树形结构遍历/符号表名称绑定作用域/依赖获取服务定位/算法复杂度性能标注）
> 原则：**标准尽量少，一个需求一种格式**；不同需求允许不同格式，但不得同需求多格式
>
> **v1.1**：第二轮 15 组（总 30）。**v1.2**：第三轮 13 组（总 43）。
> **v1.3**：前端**刻意留白**（等内核定型），L9 移出执行清单。
> **v1.4**：第四轮 10 组（总 53）。**v1.5**：第五轮 5 组（总 58）。
> **v1.6**：第六轮 5 组（总 63）。
> **v1.7**：第七轮 5 组（总 68）。
> **v1.8**：第八轮 5 组（总 73）。
> **v1.9**：第九轮 5 组（总 78）。
> **v1.9.1**：K5 决策改"示例教学代码全删"（用户初判）。
> **v1.9.2**：用户确认——**文档类走归档不删除**、**代码教学类先调研再转正**。
> **v1.9.3**：调研完成（13 项目）——K5 定稿：test/examples 8 个 C 示例
> **保留+转正**、根 examples 假示例**删除**、API_QUICKSTART 转正引用式、
> TUTORIAL/USE_CASES **归档**、加 CI 同步护栏。
> **v1.10**：第十轮 5 组（总 83）。
> **v1.11**：第十一轮 5 组（总 88）。
> **v1.12**：第十二轮 5 组（总 93）。
> **v1.13（2026-08-27）**：第十三轮五路新增 5 组（总 **98 组**），
> 本版并入 §1.61-1.65 与 §3.61-3.65。
> **v1.14（2026-08-27）**：第十四轮五路新增 5 组（总 **103 组**），
> 本版并入 §1.66-1.70 与 §3.66-3.70；K27 与 I3/F27（增长逻辑）、K28 与 K27
> （解析安全接线）、K29 与 J2/F29（锁抽象）交叉合并立项。
> **v1.15（2026-08-27）**：第十五轮五路新增 5 组（总 **108 组**），
> 本版并入 §1.71-1.75 与 §3.71-3.75；K31 与 I3/F27（lv_dstr 增长）、K32 与
> L1/F8-F10（状态机合流+证明 IR）、K33 与 I4（文件 IO 闭环）、K34 与 K22
> （命令枚举对齐）、K35 与 E3/K14/K27（导出转义+词法器骨架）交叉合并立项；
> 第 15 轮确认 4 个疑似真实缺陷（命令层 Circle/约束坍缩、SVG 自转义双写、
> 证明→LaTeX 转义三缺一）。
> **v1.16（2026-08-27）**：第十六轮五路新增 5 组（总 **113 组**），
> 本版并入 §1.76-1.80 与 §3.76-3.80；K36 与 L8/K15/K30/K9（日志级别/宏规范/
> 死开关）、K37 与 K2/K12/K19（数学原语/常量/容差）、K38 与 I2/K8（时钟/计时
> 基座）、K39 与 L4/K22/K34（协议契约/命令）、K40 与 J3/J4/K17/K18（构建组织）
> 交叉合并立项；第 16 轮确认 4 个真实缺陷（FATAL 紧急保存双重失效、sledge 超时
> 形同虚设+solver 超时门恒不触发、LVZD 头部越界写/读、lv_number_is_integer
> 截断 UB）。
> **v1.17（2026-08-27）**：第十七轮五路新增 5 组（总 **118 组**），
> 本版并入 §1.81-1.85 与 §3.81-3.85；K41 与 K19/E8-E10（容差/几何谓词）、
> K42 与 I1/L4/K9（进程网络/WS 双栈/平台宏）、K43 与 I3/K26/K15（池/注入/
> 竞态）、K44 与 G5/K19/K22（配置/容差/键单源）、K45 与 K36/K26/K1（宏族/
> 参数覆盖/文案）交叉合并立项；第 17 轮确认 5 个真实缺陷候选（垂直约束被实现为
> betweenness 占位、interop 平行映射 CONTAINMENT、coeff_pool 尺寸失配越界、
> graph_conflict segments_can_intersect 空壳恒 true、lv.h:695 setter 校验声称
> 与实现脱节）。
> **v1.18（2026-08-27）**：第十八轮五路新增 5 组（总 **123 组**），
> 本版并入 §1.86-1.90 与 §3.86-3.90；K46 与 K2/K12/K30/K22（哈希/FNV/黄金比/
> 表）、K47 与 K28/F3/K14（树/递归/导出）、K48 与 F7/K43/I3/K3（预设容器/
> id_map/声明）、K49 与 K15/K11（全局状态/测试替身）、K50 与 K7/K2（降级/插入
> 排序）交叉合并立项；第 18 轮确认 5 个真实缺陷候选（预设枚举名三份表致 UNKNOWN
> 行为缺口、lv_tree_traverse 生产零调用 M6、量词绑定泄漏、engine_bind_context
> 幻影注释、graph_memory 声称 O(n log n) 实际 O(n²)）。

---

## 1. 现状总览：一需求多格式清单

八轮共四十路审计发现 **73 组"一个需求多种格式"重复点**：

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

### 1.11 引擎生命周期面（第三轮，L1-L2）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| L1 | 问题生命周期状态机（IDLE→PARSING→REASONING→COMPLETE/ERROR） | `lvContextState`（context.h，5 态）vs `EngineState`（engine.h:196，5 态） | **同一副骨架实现两份**：状态集完全一致、转移图逐位一致、状态名映射几乎逐字重复（连"不合并"豁免注释都是复制粘贴的）；`engine.h:189` 自认"语义对齐" |
| L2 | 解析→推理→完成/失败进度 | 模型 A `lvContextState`（lv_prove 便捷路径用）+ 模型 B `EngineState`（**除定义文件外全项目零生产调用，几乎死代码**）+ 模型 C `lvStageStatus`+session->success（L7 编排真实主路径） | **同一需求被建模三次，其中两次未接入真实 pipeline**——最典型"一需求多实现" |

### 1.12 推理后端面（第三轮，L3）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| L3 | "判定约束可满足/可解" | ~9 条平行路径：DSL check_sat（仅 DOF 预检）/ engine_rewrite_and_solve / engine_scheduler / solve_algebraic_system（8 步流水线）/ CDCL SAT lvSolver / SAT 编码管线 / SMT 后端（仅 Groebner 分支可用）/ ATP 后端（全桩）/ relation_model 有界判定 / Newton 数值求解 | **真正被使用的求解后端只有 GROEBNER**；注册机制 5+ 套并存（SMTBackendRegistry/ATPBackendRegistry/EngineScheduler.backends[]/lvBackendPluginRegistry/数值注册表），引擎真实分发走 engine_scheduler.c 硬编码静态 VTable **绕开所有注册表**；GROEBNER 在三套注册表+两张静态表各一份 |

### 1.13 流/交互协议面（第三轮，L4）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| L4 | 引擎输出流/事件推送 | C `stream_json.c`（13 字段权威）vs `lv/stream_bridge.py`（14 字段）vs `stream_bridge/stream_bridge.py`（16 字段，**自述与 stream.h 一致但已漂移**）；`stream.event` JSON-RPC 序列化器 **3 套**；事件类型元数据 C x-macro 权威 vs module/stream_bridge `_EVENT_META`（40 项无 PRESET，**已漂移**）；WS 服务器 **2 个栈**（C 自实现 RFC6455 vs Python websockets 库）；进程内 StreamContext 语义 3 份重复 | 同一份引擎事件流被 C interop 直接推、又被 Python 透过另一套 ctypes 绑定拉出再次分发——**C 与 Python 各做一遍事件→JSON-RPC**；interop 命令词汇表（21 命令）与 L9 五命令（Load/Verify/Batch/Export/Visualize）是**两套不同协议** |

### 1.14 内存/日志面（第三轮，L5-L8）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| L5 | 调试分配器 | `allocator.c` debug_alloc（走 AllocatorOps vtable，权威）vs `lv_utils.c` `lv_malloc_tracked/lv_calloc_tracked`（**内联重写同一套**"裸 malloc+魔数+追踪链表+统计"骨架，绕过 vtable，不响应分配器切换；core/src 内零调用点） | **同一调试分配器逻辑两份实现**，一份休眠 |
| L6 | 内存统计 | `MemoryStats`（lv_utils.h，扁平全局计数器，实际使用）vs `lvMemoryStats`（memory_pool.h，按 type_id 分类型数组，**生产零调用**） | 双写两套统计，第二套已失效；README 引用 `lv_get_memory_stats_ex` **无实现** |
| L7 | 泄漏检测 | lv_cleanup 实际路径（简单 current_used 检查）vs `lv_memory_leak_report`（精确未释放块遍历，生产零调用）vs 文档声称的 lv_mem_print_stats（未被生产调用） | 泄漏检测三路径，文档描述与运行时实现不一致，最精确工具被弃置 |
| L8 | 日志级别 | `lvLogLevel`（lv_log.h，值越小越详细）vs `LogLevel`（debug.h，条件编译别名）vs `lv_LOG_LEVEL_*`（lv_internal.h:103，**值越大越详尽，方向相反**）vs runtime_monitor 带 tag 又一套 | **3-4 套日志级别词汇表，语义顺序互相打架**；lv_log.h 自认用魔法数字规避同名冲突 |

### 1.15 前端/文档面（第三轮，L9-L11）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| L9 | 前端数据通道 | 前端 `protocol/index.ts` KernelBridge 接口 + createMockBridge（**无任何组件消费**）；`hooks/useExport.ts` 定义未引用；M1-Canvas 读 geometryStore 硬编码 mock | **前端 100% mock 数据，与 C 内核零连接**——proof_widget.c 的 C API 数据契约（为前端声明）成为未使用孤岛 |
| L10 | 证明/求解器/几何文档 | 证明系统 5 篇（09/22/34/38/42，同一模块被多篇覆盖）、求解器 5 篇（04/14/17/19/25）、几何 6 篇（15/16/21/26+）、数值 7 篇 | 同主题多文档泛滥；`stable_release_gap_analysis.md` 自述与 GAP_ANALYSIS.md 重复；INDEX.md 死链接；版本号互相打架（v1.1.0 vs v3.5.0 vs v5.0.0） |
| L11 | 语言语法定义 | `doc/docs/lv_LANGUAGE_SPEC.md` vs 根 `docs/architecture/` DSL 三篇 vs `bootstrap/` .lv 语义规格 | **同一语言语法定义 ≥3 处**；导出格式枚举双套（InteropExportFormat 10 种 vs lvExportFormat 6 种）互不引用 |

### 1.16 公式/表达式面（第四轮，F1-F3）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| F1 | double 表达式树 + 求导 | `lvSymExpr`（sym_expr.h，符号求导，生产零调用）vs `lvADExpr`（autodiff.h，数值自动微分，生产零调用） | **两套 double 表达式树 + 一阶导能力**（节点种类高度重叠），均不在规范文档内，均仅测试消费 |
| F2 | 精确代数规范表示 | `lvExprCanonical`（expr_canon.h 稀疏多项式，文档指定"唯一规范形"）vs `lvExpr`（expr_canonical.h 树） | **双轨规范表示但规范形零生产调用**（仅测试）；命名冲突 expr_canon vs expr_canonical；文档声称的 FormulaNode→lvExpr→lvExprCanonical 流水线无生产桥接 |
| F3 | FormulaNode 字符串化 | `node_to_string`（formula_string.c，拆分遗留独立 walker）vs `formula_renderer`（6 后端表驱动共享骨架） | **两套 AST 字符串遍历器**，node_to_string 未入共享骨架 |

### 1.17 导入解析面（第四轮，F4）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| F4 | double→SymbolicCoord | `ggb_double_to_rational`（1e6 精度，ggb+svg 共享）vs GeoJSON 内联（1e9 精度，json.c:178-181）vs 命令面内联（interop_command.c:281-282） | **同一算法 3 份**，精度语义分裂（1e6 vs 1e9）固化；点序列→折线图构建 3 份（ggb_import_point_sequence / svg_import_samples 几乎逐行复制 / json 内联循环）；XML 属性提取 2 份（拷贝版 vs 切片版） |

### 1.18 类型系统面（第四轮，F5）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| F5 | 几何概念类型枚举 | `GeomType`（constraint_graph.h，运行时节点权威）vs `TypeKind`（type_system.h 类型理论）vs `LvSemanticType`+`LvEntityType`（lv_sema.h/lv_ast.h，.lv 表面，互为镜像）vs `NODE_GEOM_*`（formula_parser.h 重述） | **同一几何概念 4 种枚举编码**（Point/Line(Segment)/Region/Circle 命名取值不一致）；`LvEntityType` 与 `LvSemanticType` 几乎一一对应（后者=前者+UNKNOWN/ERROR）；Port 内联 type_region vs 外部 NodeTypeMapping 双通道（文档承诺失步） |

### 1.19 基础工具面（第四轮，F6-F7）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| F6 | 字符串→数字解析 | `lv_parse_*`/`lv_str_scan_number`（权威）vs 25 处/14 文件语义等价残留 + 1 处 atoi（test_serialize_registry.c:43 高危） | **半收敛**：25 处可机械收敛到权威解析；1 处 atoi 首选替换 |
| F7 | 命名预设库容器 | `preset_blocks.c` / `preset_manager.c` / `func_block_preset_internal.c` 各维护并行容器（各带 lv_hashtable 索引） | **同一"命名预设库"概念 3 容器并存**，未统一到 lv_registry |

### 1.20 证明内部面（第四轮，F8-F10）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| F8 | 证明/命题验证入口 | `prop_verifier_verify`（VerifyResult）/ `lv_verify_proof`（lvVerifyResult）/ `proof_minimal_verify`（LvProofVerifyResult）/ `mini_kernel_verify_all`（MiniVerifyResult）/ `lv_proof_object_verify`（bool）/ `simple_proof_check`（bool） | **≥6 个独立验证入口**，各自独立枚举/结构/语义；proof.h 与 prop_verifier.h 曾共用守卫导致 include 顺序漂移，官方注释承认重复 |
| F9 | 证明策略调度 | `ProofMultiStrategy`（JGEX 12 + legacy 桥接策略）vs 经典 `lvProofEngine` 10 策略（proof_strategy.c） | **两套策略引擎**：多策略引擎（ProofNavigator 调度面）vs 经典引擎（System A） |
| F10 | 证明引擎栈 | 4 套并存：ProofNavigator 工作流（Stack 1）+ 多策略引擎（JGEX）+ 经典 lvProofEngine（Stack 2）+ 逻辑内核（mini_kernel/prop_verifier） | **同一证明需求 4 套引擎栈互相桥接**；kernel/ 目录混入 ecosystem.c/gc_language.c 与逻辑内核无关 |

### 1.21 安全/限制面（第五轮，G1）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| G1 | 防失控（深度/步数/超时/熔断） | 熔断器**一套数据三套写入口**（lv_circuit_breaker.c 独立 API + circuit_breaker.c context 转发层 + context.c 内联直接改字段）；递归/推理深度 **6+ 套限制互不知晓**（parser 256 / circuit_breaker 100 / reasoning_stack 1000 / context 10000 / recursion 128 / lambda_unify 1024）；"递归 128"常量 3 处（recursion.h/config.h/runtime_guard.h）；超时 30000 常量 3 处；解析安全 4 函数（输入长度/净化/AST 深度/节点数/token 长度）**4 个未接线 + 2 个被 formula_dsl_lex 内联重复**；`lv_ERROR_PARSER_POOL_EXHAUSTED` **死错误码**（无代码产生） | **"防失控"6+ 套独立互不知晓机制**；解析安全函数声明了但未接线，内联重复版本反而生效 |

### 1.22 缓存面（第五轮，G2）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| G2 | 缓存重复计算结果 | 推理/子问题去重缓存 5 套：`prop_verifier_memo`（真实，lvHashtable_i64）/ `smt_trigger_engine.instance_cache`（真实，参照前者）/ `axiom_pkg.expansion_cache`（真实，**lvDArray 线性扫 O(n)**）/ `lvReasoningCache`（**仅规格**，源码不存在）/ `proof_engine.enable_cache`（**仅旗标**，结构无缓存字段）；对象惰性派生值缓存 4+ 处（symbolic_coord cached_value / quantifier cached_truth / groebner cached_basis / render_cache）各自手写失效 | **"缓存"5 套不同实现**，底层容器/失效/容量全不一致；lvHashtable 是容器无缓存语义（无 LRU/TTL/驱逐）；文档引用多个源码不存在的缓存（reasoning_cache.h/cache_manager.c/lvLRUCache 等） |

### 1.23 图算法面（第五轮，G3）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| G3 | 约束图 BFS/Kahn 拓扑 | `lv_bfs_run`/`lv_topo_run`（graph_traversal_bfs.c 回调驱动，权威）vs `bfs_traverse_from`/`lv_graph_topological_sort`（graph_traversal_dfs.c/util.c 约束图内部实现） | **约束图域两套 BFS + 两套 Kahn**（骨架同构可参数化吸收）；其余图域（模块依赖/类型/BDD/证明树）多为已评估豁免或已收敛——**治理基线健康**；`graph_topological_sort_stable` 名字误导（实为确定性排序非拓扑序） |

### 1.24 事件回调面（第五轮，G4）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| G4 | 回调注册+多订阅者分发 | `lv_callback_list`（权威基座）已收敛 StreamContext / lvEventBus / setter 注册表（均内嵌该基座）；**插件系统 `lv_plugin_broadcast_event` 仍手写 for 循环广播**（lv_event_bus.h:61-64 自认未迁移）；Python `_event_handlers` 手写订阅表 | **回调基座已统一**，仅插件广播一处未收敛 + Python 跨语言边界应显式豁免 |

### 1.25 配置/全局面（第五轮，G5）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| G5 | 配置/全局状态 | 两套运行时注册表：`lvConfig`（config.h/lv_config.c，JSON 持久化，A）vs `ConfigManager`（lv_utils_config.c，INI 持久化，B，LV_CFG_* 键与 A 大量同名同义，B 被影子化）；模块级配置自成一套且**默认值矛盾**：lvSessionConfig 默认 timeout 5000/depth 8 **硬编码且覆盖**全局配置默认 30000/100；`global_state.c` 第三套 key-value **闭环死代码**（initialized 恒 false） | **同一参数（超时/深度）多份默认值互不知晓且会话覆盖全局**；配置来源 JSON(A)/INI(B) 两套持久化格式；一个死单例 |

### 1.26 插件系统面（第六轮，H1）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| H1 | 插件/扩展/后端注册 | **7 套互不知晓**：`lvPluginSystem`（动态库 C 插件，10+ 文件含状态机/依赖/接口/事件，**未接入运行时生命周期**，仅测试实例化）/ `lvBackendPluginRegistry`（"统一"后端描述符，真实分发不查它）/ `SMTBackendRegistry` / `ATPBackendRegistry`（按域，与前者重复登记）/ `EngineScheduler.backends[]`+静态 VTable（**引擎真实分发**，硬编码 GROEBNER）/ `lvInteropPlugin`（证明互操作，lv.c 注册 coq/lean4/opml）/ `lv_ecosystem_*`（元数据注册表，lv.c 接线） | **"插件"概念 4 种语义**（动态库 C 插件/静态后端描述符/证明互操作/生态模块）互不共用；**同一后端（GROEBNER）硬编码进 4 套注册表**；高危命名冲突 `interop.h:228 typedef lvInteropPlugin lvPlugin` vs `plugin_system.h:37 typedef struct lvPlugin lvPlugin`；ecosystem 文档声称 527 行/16 API 实际 20 行/5 函数（文档失同步） |

### 1.27 快照/回滚面（第六轮，H2）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| H2 | 保存图状态+恢复 | **图级保存+恢复 3 套**：`graph_copy`（graph_node_copy.c，唯一公共深拷入口，context/engine_frozen/algebra/modal 已收敛）/ `graph_snapshot_create`（rewrite_snapshot.c，**自建深拷不调 graph_copy**，rewrite 事务回滚）/ `bit_burning`（JSON 序列化-反序列化，熔断回滚）；撤销/重做 5 处（algebra 成对 undo+redo / path_explorer 单向 / lambda 局部 / cdcl trail / **protocol 空壳**——命令名暴露但 undo_depth 硬编码 0 无 handler） | **graph_snapshot 是收敛后的残存重复**（graph_copy 注释宣称已收敛 engine_frozen/critical_pair 但漏了它）；protocol undo/redo 是 M5 空壳 |

### 1.28 数值线性代数面（第六轮，H3）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| H3 | 解线性方程组 | 稠密 LU/高斯消元 **3 份**：`host_lu_factor/solve`（host_linalg.c，后端复用）/ `dense_lu_solve`（sparse_linear_algebra.c，**注释明言"自包含避免 L3→L4 依赖"刻意复制**）/ `gauss_eliminate`（geo_constraint_solver_linear.c，**生产唯一路径**，n≤20）；后端分发两套并存（numerical_backend 注册表 vs 几何约束硬编码 gauss_eliminate，**注册表无生产消费方**）；稀疏直接法（LU/Cholesky/QR）**未声明进头文件**游离于标准接口之外 | **同一算法 3 份**因"避免跨层依赖"刻意复制；SUNDIALS 式注册表搭好了但生产不用；BiCGSTAB/GMRES 已收敛为共享内核（正面范例） |

### 1.29 形式化对齐面（第六轮，H4）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| H4 | 欧氏几何公理描述 | **同一概念 ≥6 处独立维护互不一致**：formal/lv/*.lean（Hilbert 型类）+ lv-formal/Classical/Hilbert/*.lean（**与前者逐字相同**，仅注释编码不同，误导性复制）+ euclidean_plane.lvz（Hilbert 风格 22 模板）+ euclidean/manifest.json（**Tarski 风格 12 公理**）+ euclidean/README.md（Tarski）+ lv-formal/Theory/Axioms/Instances_Geometry.lean（照抄 22 模板）+ test_axiom_euclidean_plane.c（硬编码 22 模板） | **风格漂移**（同一包 Hilbert vs Tarski 两套公理化）+ **编号漂移**（C1 在 formal/lv=线段自反、.lvz=线段搬运、manifest=线段构造）+ **层间无对齐机制**（CI 三个 job 完全独立，无 .lean↔.lv↔.lvz↔C 交叉验证）；Continuity.lean 空公理占位（∃ n, True）与 .lvz 真实签名不一致 |

### 1.30 Python 测试结构面（第六轮，H5）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| H5 | Python 测试基座/常量对拍 | **3 套测试基座**：pytest（3 文件）+ unittest（test_streaming_e2e.py，IsolatedAsyncioTestCase）/ 自写 subprocess runner（test_runner.py，**pytest 收集 0 用例，仅 CHANGELOG 提及**）；**无 conftest/无 fixture**；枚举常量**三重硬编码**（C 头文件 → _ctypes_binding.py → test_constants_consistency.py 基准表，测试注释自认"改 C 头需手工同步"）；流式事件类型**二重硬编码无对拍**（test_streaming_e2e.py 魔术数字 0/1/3/4/5/12/15/16）；tests/ 被打进构建产物（build/lib/tests/ 副本） | **测试基座一需求三实现**；常量对拍靠人肉；**流事件对拍缺口**（C 宏插入事件 Python 静默错位）；test_graph.py 后端未钉死（有库测 C 无库测 fallback 不确定） |

### 1.31 进程/网络面（第七轮，I1）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| I1 | 外部程序调用 | `lv_external_process_run`（lv_process.c 统一底座：spawn/管道/超时/退出码）已服务 ATP/SMT；**graph_dot_export.c:292 唯一裸 `system()`**（graphviz dot→SVG，无超时无 stdout 捕获，完全绕过底座）；atp_run_subprocess 与 smt_external_solver_check 的"argv→超时→执行→降级"骨架同构（lv_process.h 头注释自认待收敛）；Python concurrent_monitor 的 terminate→wait→kill 跨语言独立实现 | **底座已就位**，仅 graph_dot 绕底座 + atp/smt 骨架同构待收敛；Coq/Lean bridge 纯文本互操作天然豁免；lv_dlopen+_ctypes_binding 是 FFI 跨语言孪生非重复 |

### 1.32 性能监控面（第七轮，I2）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| I2 | 计时/统计/监控 | 起止计时 **6 套**（lvTimer/lvPerfSession/lv_event_trace/SchedulerStats 内联/proof_engine 内联/TraceSession，4 套无生产调用）+ 2 个时钟基座（CLOCK_MONOTONIC vs clock()）；Welford 在线统计 **2 份拷贝**（runtime_monitor vs performance_profiler 逐行同构）；求解统计 **4 路**（SchedulerStats 活 / PerformanceCounters 记录侧死 / lvDiagnostics 字段恒置 0 / solver_stats 命名误导实为 DOF）；内存统计 3 套；事件追踪 2 套（lv_event_trace vs trace_session）；时间戳格式化 2 套；日志文件管理 2 套（debug 管道 vs runtime_monitor 残留状态机） | **计时/统计/事件追踪/日志文件管理"一需求多实现"**；日志管道与 JSON 序列化已统一（v3.3-3.5 收敛痕迹）；debug 状态族是健康横向拆分非重复 |

### 1.33 数据结构面（第七轮，I3）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| I3 | 容器/增长逻辑 | 增长逻辑 **5 套 + 3 处漏网**：lv_ensure_capacity（权威 int 容量）/ lv_ENSURE_ARRAY_CAP 宏（12 处独立实现）/ array_grow_to_fit（size_t）/ lv_dstr_grow（size_t）/ lv_strbuf_grow（size_t+**lv_malloc 非 realloc**）+ interop_theorem/lambda_to_graph/lv_utils_config 手写倍增；FIFO 队列 **8+ 套模块自建**（BFSQueue/propagation/type_equiv/WorkQueue/BfsQueue 链表/定长/thread_pool 链表/worklist）；环形缓冲 4 套（lv_ringbuf 权威/HashHistory/stream_lazy/propagation）；内存池 2 套（lvMemPool vs lvObjectPool 互不委托）；IntArray vs lvDArray 同语义 | **增长逻辑单一路由 + 队列族收敛是最高价值动作**；哈希表/堆/回调列表/遍历/注册表已收敛成形（健康基线）；lv_heap/lvHashtable 三形态/lvArena/thread_pool/lvReasoningStack 是不同语义保留 |

### 1.34 IO/序列化面（第七轮，I4）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| I4 | 文件 IO/round-trip 验证 | round-trip 验证 **3 套**：`lv_roundtrip_verify`（权威，mem:// 完整往返+注册表分派）/ `meta_repr_verify_roundtrip`（无调用方，memcmp 兜底可疑）/ `test_oracle_verify_serialize_roundtrip`（无调用方，第三套图同构比较器）；图等价比较多实现（meta_repr_graph_equivalent / graph_isomorphism_* / 测试自写 compare）；文件原语两套（lv_file 权威 vs **lv_storage file:// 后端内嵌 fopen/fread/fwrite/fseek/fclose 全套**）；裸 fopen **18 处**（10 文件，2 处显式豁免 + 6 处无标注）；lv_bytes/lv_json 已收敛（健康基线） | **round-trip 基座已存在但 2 套冗余无调用方**；文件原语两套；裸文件 IO 收编 + 豁免登记；LVZD/PNG/msgpack 长度字段属格式契约不抽象其编解码 |

### 1.35 层间依赖面（第七轮，I5）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| I5 | 层验证/归属 | **ENABLE_LAYER_VALIDATION 形同虚设**：CMake 正确定义 lv_CURRENT_LAYER + 宏，但 `lv_ALLOW_LAYER`/`lv_REQUIRE_STRICTLY_ABOVE` **全库零调用**、layer_validation.h **全库零 include**（头注释自认参考实现）——"0 违规"是空转结果；**2 处 P0 真实逆向依赖**：L2 context.c → L3 lv_reasoning_stack（推理栈提取破坏 L2 隔离）、L1 lv_loader.c → L3 constraint_graph + L4 engine（伞形头 lv.h + CMake 补链双重违规）；**功能归属混乱 ~8 处**：解析面 5 个解析器偏离 L1（dsl/gc_language/module_lvz/formula，均为"解析+装载被拖入高层"模式）、lvProofObject 类型-实现分居 L4/L5、proof 导出双轨 L4/L5、L4 module_export 越权做 SVG/TikZ/PDF、lv_registry vs ecosystem vs module 三撞车 | **层验证机制空转是 P0 顶层问题**；先修机制（依赖表接入+缺省只允许本层+CI include 矩阵）再清零；"引用方向检查"与"归属层检查"分开治理；健康面：L3→L4/L4→L5/L5→L6/L6→L8/L10 直接 include 全干净、lv_dot_writer 方向正确 |

### 1.36 init/清理生命周期面（第八轮，J1）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| J1 | 进程级初始化/清理 | **4 套生命周期表达**：模块注册表（lv_module_register，qsort 优先级+逆序清理）+ lv_cleanup 硬编码手动序列（engine/func_block/perf/health/...TLS/threadpool/delta 10+ 行）+ 模块自管 once_reset + 手写 initialized 豁免模式；**8 处 M6 未接线清理**：LV_REGISTER_MODULE 宏 0 调用点、lv_log_shutdown（runtime_monitor）从不被 lv_cleanup 调、lv_event_trace init/shutdown 仅测试、lv_ecosystem_init 仅测试（生产生态注册静默全失败）、lv_config_reset 仅测试、模块注册表 cleanup 不重置 count（二次 init 重复注册被吞）、lv_global_thread_pool_destroy 不重置 once（**破坏 init/cleanup 循环**）、lv_application_shutdown 空实现；失败路径无回滚（ERROR 态不可重入）；两套日志系统并存（debug_trace_session 接注册表 vs runtime_monitor lv_log_* 未接） | **"清理可重入"承诺与实现脱节**；进程级清理 3 套并列约定（注册表/硬编码/模块自管），新子系统无规则可循；thread_pool once 不重置直接威胁 90+ init/cleanup 循环测试 |

### 1.37 线程/并发面（第八轮，J2）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| J2 | 锁/原子/线程池 | **锁抽象 3 层叠加**：lv_mutex_t（活跃 20+ 模块，唯一实现）vs runtime_guard.h 自有 lvMutex typedef+lvRwLock（SRWLock/pthread_rwlock 平台分支，**从未启用**，与 lv_ATOMIC 同名潜在重定义冲突）vs context.h void* 锁字段（**死代码**）；**手写惰性锁 9 处**（groebner_engine/high_dim_view/module_delta/proof_navigator_breakpoint/preset_manager/debug_state/memory_pool/lv_registry/test_framework 各自 once+mutex+ensure，lv_lazy_lock 已有 13 模块在用）；原子 **3 套**（lv_ATOMIC_* 缺 64 位 LOAD/STORE/CAS / 裸 C11 stdatomic 无豁免标注 / 互斥模拟 debug_refcount）；并行骨架 2 套未复用 thread_pool（proof_version_task lvTaskGroup / groebner_parallel WorkerArg 顺序驱动，有缺陷回退背景） | **平台分支已收敛**（pthread/CRITICAL_SECTION 仅存在于 lv_thread.h/lv_platform.h/cross_platform.h 三权威点）；锁抽象 3 层 + 惰性锁 9 处手写 + 原子 3 套是主要重复；线程池唯一实现 ✅ |

### 1.38 构建产物/CI 面（第八轮，J3）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| J3 | 生成产物/CI 报告 | **bootstrap/output 15 文件全部 git 跟踪、零消费者、无再生成驱动**（2026-06-21 恢复性提交一次性带入，纯展示快照，内部 Lean×3 含 P4 命名错位/TikZ×2/同构 JSON×6）；**CI 失败报告 8 处内联 grep+awk**（ci.yml 3 + python.yml 5，正则重叠）；**docx 汇报 4 个生成器**（tool/report_generators/ 3 JS + tool/scripts/gen_report.py，ui 仅链 1 个）；git 策略不一致（build*/ 忽略但 output/ 提交、triangle_output.tex 提交而 test_output.tex 忽略、报告 .md 跟踪/未跟踪混杂）；2 处死配置（TestLake 嵌套 workflow 不执行 / web-deploy if:false） | **生成产物无再生成驱动 = 纯快照**；CI 报告/docx 生成"一需求多实现"；git 跟踪策略需按"生成物/golden/测试中间物/日志"四类归一 |

### 1.39 Python 包结构面（第八轮，J4）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| J4 | Python 导出/镜像数据 | 顶层常量**双通道**（`from lv._ctypes_binding import *` 星号暴露 ~60 原始常量 + core 白名单 re-export）；**中点实现 ≥8 处**（core.Point.mid_point 权威 + LineSegment/Wrapper/G/AlgebraMode/P/preset×2/fallback，preset_basic 与 preset_blocks/geometry 是**同一函数体两代逐字复制**）；**预设注册表 3 套**（v3.3.0 dict / v4.0.0 PresetRegistry 单例 / math_presets 自持）；**预设元数据全链 ≥5 份镜像**（孤儿 C 56 文件不编译 → convert_presets.py → .lvz 149 个中文乱码=生成漂移实证 → preset_registry.yaml 3095 行无消费方 → Python 两代镜像 + C 侧 g_builtin_presets 75 条编译表第二源）；lv.utils 346 行孤儿死代码；**build/lib/ 38 文件过期镜像**（缺 LineSegmentFb 等导出）；fallback 部分回退缺口（20+ 子模块 from _ctypes_binding import _lib 无守卫） | **数据单一事实源缺失**（预设全链 ≥5 份漂移）；顶层导出面双通道；build/lib 过期镜像 + lv.utils 孤儿应删；绑定层/核心层/域模块分层健康 |

### 1.40 全局符号/命名面（第八轮，J5）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| J5 | 全局符号/命名空间 | **"统一 lv_ 前缀"宣称与事实不符**：lv_PUBLIC_API 导出 1500 函数中 lv_ 仅 44.9%，模块裸前缀 55%（proof_/func_/graph_/stream_ 等 ~70 个）；typedef 裸名 51%；枚举常量 lv_ 仅 22.8%；**LV00_ 前缀全域 0 命中**（该约定不存在）；**10 个裸 extern 全局泄漏**（interop_stream_ctx/graph_stream_ctx/g_coeff_pool/g_fail_count/_stream_ctx 前导下划线等）；**同名冲突**：lvPlugin（interop.h 别名 vs plugin_system.h typedef，潜伏硬冲突）、RelFormulaType/REL_FORMULA_*（sat_encoding vs relation_model **同名同常量值不同** OR=6 vs 9）、func_block_registry/preset 守卫式重复枚举 54 常量×2、expr_canon.h vs expr_canonical.h 文件名与内容错位；**同义不同名**：create 314 vs make/new 3、destroy 277 vs free 36/cleanup 30/delete 1（同对象双动词并存 lv_proof_trace_destroy+free）、解析四风格（parse/scan/read/from_string，lv_parse_int 与 lv_str_read_int 并存）、ConstraintGraph vs lvConstraintGraph 双名 | **命名规范未锚定**（前缀双轨 45%/55%、动词三轨、同名异义潜伏冲突）；10 裸 extern 泄漏；命名 lint 缺失（clang-tidy readability-identifier-naming 可强制） |

### 1.41 错误消息文案面（第九轮，K1）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K1 | 错误消息/用户可见文案 | **码→文本表已单一事实源**（LV_ERROR_CODES_X 68 码 → g_error_table 宏展开，✅ 健康）；但表外 **3 大类手写文本源 ~2000+ 处**：lv_RETURN_ERROR 1757 个手写字面量（238 文件，85% 英文/15% 中文，同文件混用）、ctx->error_message 直写（lv_convenience 13 处）、实例 last_error（引擎/插件/visual_editor/smt）；**同场景多文案**：状态转移非法 4 种措辞、INVALID_PARAM ≥6 种、OOM 中英冲突（表"内存不足" vs lv_ERR_MSG_OOM "out of memory" 21 处 vs "push failed (OOM)"×16）、NULL/未找到/未初始化/越界/超时各家族多措辞；**跨模块逐字复制**（coq/lean4/opml 桥"empty input"×2 等、Python "三点共线"×9）；状态名显示表 ≥6 处各自维护（context/engine/circuit_breaker/proof_trace 格式各异）；**完整描述渲染两套**（lv_get_error_description vs lv_error_format_chain）；**无 i18n 机制**（C 中文表 / Python 中文 / UI 全英文跨层不一致） | **表本身健康，缺"场景文案规范"**——调用点自创新措辞无约束；语言策略未单点决策；状态名表 D4 映射重复；测试钉死文案（test_context 断言"超时"）有回归约束 |

### 1.42 基础算法重复面（第九轮，K2）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K2 | 基础算法/数学原语 | **已收敛 ✅**：gcd/欧几里得权威在 lv_arith_safe.h（lv_gcd_u64 等，全库无手写取模循环）、MIN/MAX/CLAMP/ABS 唯一权威在 lv_utils.h（无重复定义）、矩阵转置/二项式/素数判定单实现；**真同构残留（应统一）**：整数快速幂 ×3（lv_safe_pow/lv_alg_rational_pow/lv_number_pow 同平方求幂骨架，mul 回调可参数化）、**平方因子提取 ×3**（symbolic_coord.c:352 int64 已被 2 文件共享 vs quadratic.c/algebraic.c static 复制，solver_symbolic GMP 版不同域）、lv_utils_misc 内部 FNV 双家族（lv_hash_* 与 lv_fnv1a_* 同骨架零调用）、normalization 手写 int 插入排序×2、geometry_compress 手写三态比较器、preset_common lv_hash_int_array 零调用死代码；**不同变体保留**：qsort vs 插入排序算法选择、并行数组排序（axiom_rule/groebner_poly）、djb2×31 指纹派生、FNV 粗粒度混合 | **gcd/数学原语已收敛（健康基线）**；排序/哈希权威齐备但 4 处真同构 + 2 类残留待迁移；新发现快速幂×3 与平方因子×3 两个 D1 高价值立项 |

### 1.43 文档代码一致性面（第九轮，K3）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K3 | 文档声称 vs 实现 | **14 个核心头声称与实现真实对齐**（engine/context/proof/rewrite/module/lv/stream/interop/type_system/constraint_graph/unify/normalization/func_block/config/error_codes）；**4 个脱节**：solver.h（SuiteSparse/GraphBLAS **从未实现**，实际纯 C CSR，M5）、dsl_compiler.h（".lv 文件流水线"+"多目标代码生成 C++/Python/JS/WASM"均空壳 M5，**真实 .lv 加载在 lv_loader.c**）、ecosystem.h（20 行/5 函数生产零调用 vs competitive_analysis.md 声称 527 行/16 API ✅落地，M5+M6）、proof_engine_enhanced.h（enable_cache 默认 true 但全库无缓存读取点，M6）；**README 4 个幻影 API**（lv_get_memory_stats_ex/lv_get_memory_limit_ex/lv_set_memory_limit_ex/lv_get_version_info 无声明无实现）；layer_validation.h 全库零 include（诚实空转）；31_stream_interop StreamEvent 声称 proof_step_id/json_payload 实际 var_id/merge_pairs（M5）；**头文件 TODO/stub 字面标记为 0**，13 处"简化版"无机器可 grep 桩约定；版本锚点 1.1.0 但 11+ 活文档标 v5.0.0 | **"声称脱节"集中在 4 头 + README 幻影 + 文档漂移**；头注释声称清单化（@impl-* 标记）+ 文档-代码单源对拍机制缺失 |

### 1.44 版本兼容面（第九轮，K4）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K4 | 版本/兼容/序列化版本 | **"项目当前版本"12 处冲突**：lv.h 1.1.0 权威 vs 文档 3.3.0/3.5.0/5.0.0 vs VERSION_LOG 1.9.0-dev vs CHANGELOG 双 [1.1.0] vs README 同版本"已完成+计划中"并存；LVZ 格式版本 **3 事实源**（module.c 未用 + module_lvz.c 读端 + module_serialize.c:466 写端硬编码不引用宏）；PRESET_SYSTEM_VERSION 双处；Python 包版本三处硬编码无 C 绑定；同名 lvFormal Lean 双项目工具链不兼容（4.14.0 vs 4.33.0-rc1）；**运行时版本校验缺失**：lv_check_version_compat 自比较恒真、lv_get_version_info/lvVersionInfo 幻影 API、绑定层无版本关联（lv_get_version static inline 不在 DLL 导出）；**序列化读端校验薄弱**：LVZ/LVZD minor 忽略、msgpack/JSON 无格式版本、OPML 仅 strstr 存在性不比较、.proof.cert 信封未实现；**ABI 无保护**：lv_PUBLIC_API 被 config.h:643 无条件空定义覆盖失效（靠 WINDOWS_EXPORT_ALL_SYMBOLS 兜底）、Python 13 ctypes 结构体镜像无 sizeof/offsetof 校验 | **版本事实源多头发散 + 运行时校验缺失 + 格式版本读端门弱 + ABI 无保护**；唯一完整校验链路是插件系统（lv_plugin_check_version semver） |

### 1.45 示例教学代码面（第九轮，K5）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K5 | 示例/教学/演示代码 | **19 处示例与 API 脱节**：API_QUICKSTART 整篇基于 lv_context_create 当引擎的错误模型 + **6 个不存在函数**（lv_get_version_info/lv_SOLVE_SUCCESS/lv_get_memory_stats_ex/lv_set_memory_limit_ex/lv_get_memory_limit_ex/normalization_result_free）+ 双 destroy/越作用域/字段错；USE_CASES 15 份示例预设名（euclidean_geometry/pythagorean_theorem 等）**全部不在注册表**（真实名 midpoint/circumcenter）运行时必失败；根 README 公共 API 清单 4 个不存在函数；examples/demo.py 导入 lv.lang/ir/compiler/theorem 4 个不存在模块；streaming_example.py 调 engine.solve(graph) 与无参签名冲突；lv.h:590 注释示例引用不存在枚举 lv_SOLVE_SUCCESS；**多份示例重复 6 组**：lv_context+lv_prove+preset 骨架 21 份拷贝（D1+D4）、3-4-5 三角形引擎骨架 10+ 份（D1+D3）、add_point 辅助 4 份、流式 API 面 4 处重叠、Python Graph 用法 3 处（3 个 examples 目录并存）、版本号 3.2.0/3.3.0 vs 1.1.0 | **示例面严重脱节**（6 不存在函数 + 15 失效预设 + 假示例）；21 份同骨架重复；**同步机制缺失**（API 清单手写、md 代码块不进 CI、无符号存在性检查） |

### 1.46 废弃API/兼容层面（第十轮，K6）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K6 | 废弃 API/兼容层/旧接口退役 | **lv_DEPRECATED 宏定义齐全但全库 0 使用**——C 侧废弃全靠注释，无编译期信号；Python DeprecationWarning 仅 2 处，唯一"有期限"项（preset_func_blocks_compat → v5.0.0）**无人 import（dead）**；**4 个多形态退役案例**：lvMutex（typedef 别名 + runtime_guard 平行二次 typedef 潜伏重定义冲突 + 宏映射 + J2 计划，3 套机制）、preset_func_blocks（同一旧 API 两个 compat 模块信号矛盾，新模块 docstring 还反向广告旧导入）、名称表函数三层命名（lv_geom_type_name 权威 → interop_* 包装 → 无前缀宏别名 13+ 调用点）、lvPlugin 守卫式 typedef 别名 vs plugin_system.h 同名不同义；**同名新旧并存约 20 组**（C ~17 + Python ~5，3 组高危：lvMutex 双定义/lvPlugin 双类型/NODE_TYPE_CIRCLE 映射 `(GeomType)-1` 语义漂移）；废弃标注 4 种并存（编译期 0 用/@deprecated 2 处/DeprecationWarning 2 处/中文注释 88 处） | **退役机制缺失且不统一**：lv_DEPRECATED 0 使用、唯一有期限项是 dead 模块、绝大多数无限期保留；健康反例：constraint_graph.c "旧双轨错误系统已彻底移除不再保留兼容层"——项目具备彻底退役先例未沉淀为机制 |

### 1.47 降级/回退路径面（第十轮，K7）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K7 | 降级/回退/失败容错 | 降级路径 **40+ 处 5 大场景**：绑定降级（C DLL 缺失→fallback.py）+ 数值回退（符号→double 近似 ~7 处独立实现）+ 后端降级（SMT/ATP→Groebner/UNKNOWN）+ 信任降级（位熔断/依赖失效→AMBER/YELLOW）+ 资源降级（哈希索引→线性扫描 ~10 处）；**"主路径失败→降级"至少 6 种独立实现互不调用**（scheduler fallback_chain / SMT 表驱动回退 / engine_solve 主路径回退 / solver_core SAT→Groebner / 模块级 Python fallback / 逐函数 try/except）；**静默降级 9 项**（_FakeBinding noop 吞调用 / sparse→dense 无日志 / symbolic_coord_auto_degrade 丢弃 reason 参数 / high_dim except: pass / png_render_fallback 全白图 / proof_engine enable_cache=true 声称开启无实现 / 索引→线性扫描 ~10 处全静默 / 数值回退 7 处无日志 / engine_solve 静默回退）；**无统一降级登记机制**（降级计数器 4 套互不打通：scheduler.fallback_count / g_degrade_total / adaptive_fallback / scalar_fallbacks）；降级命名混用（degrade/downgrade/fallback/retry） | **降级语义未统一**（失败后 5 种表达：false/UNKNOWN/None/异常/静默吞）+ **无统一登记**（无降级事件总线，PerformanceCounters 无降级字段）；Python fallback 与 C 降级是两套独立体系（触发条件不同可保留，记录风格割裂需收敛） |

### 1.48 基准测试面（第十轮，K8）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K8 | 基准测试/性能回归 | **函数级微基准 3 份实现**：test_framework lvBenchmark（固定迭代+朴素统计，**全项目零调用 dead API**，P2-1 豁免注释前提已不成立）/ performance_profiler lv_perf_benchmark_run（10 次预热+~100ms 校准+Welford，**唯一活性**）/ test_adaptive_threshold 手写 QPC/clock_gettime 平台分支（绕道）；**Welford 2 份逐行同构**（performance_profiler vs runtime_monitor，I2 决策未落地）；**性能回归阈值 3 处硬编码**（<1us / >1x / <1ms 口径各异，已多次 flaky）；**CI 无性能回归门**（无独立 benchmark job、无 baseline 对比，TEN_LAYER_OPTIMIZED_PLAN 规划的 tests/benchmarks/ 从未落地） | **微基准一需求三实现**（2 死/绕道 + 1 活性）；阈值 3 处硬编码无统一基线；CI 无性能回归门；时钟基座绕道违反 I2"测试改用基座" |

### 1.49 特性开关面（第十轮，K9）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K9 | 特性开关/编译期配置 | **死开关 8 项**：lv_ENABLE_RUNTIME_GUARDS（全仓无定义，启用路径引用 lvContext 不存在的 guard 字段连编译都过不了）/ lv_CONTEXT_THREAD_SAFE（3 个 void* 死字段）/ lv_LOG_GUARD（文档名写错）/ LV_USE_ZLIB（zlib 分支永不编译）/ lv_WASM_BUILD+lv_NO_GMP（CMake 定义代码零引用）/ lv_HAS_OPENMP（宏零引用）/ 4 个幽灵引用宏（lv_CONFIG_RUNTIME_GUARD_* 等）；**一需求多开关 7 组**：运行时守卫 5 种承载（编译宏/阈值宏/config compat/运行时字段零消费/文档幽灵）、线程锁 3 套、后端加速族命名三分裂（lv_ENABLE_* vs LV_HAS_* vs lv_HAS_OPENMP）、层验证 CMake 名无前缀 vs 宏有前缀且机制空转、lv_PUBLIC_API 双定义（config.h:643 无条件空覆盖）、zlib 双方案、平台检测 4 套并存（cross_platform 统一层 vs ~81 处直接 #ifdef _WIN32）；**默认值不一致**（ENABLE_LAYER_VALIDATION CMake=OFF 但多份文档记 ON） | **死开关 8 项 + 一需求多开关 7 组**；平台检测直接 _WIN32 81 处未收敛到 lv_PLATFORM_*；分层合理面（编译期尺寸常量/运行时配置/构建选项/平台自动检测）保留 |

### 1.50 内存所有权约定面（第十轮，K10）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K10 | 内存所有权 copy/take/borrow 三态 | **三态契约承诺未落地**：design-optimizations-vs-languages.md §1.2 提案的 P0 三项全未执行（memory-ownership.md 不存在 / [copy]/[take]/[borrow] 头注释标注全库 0 处 / 无静态检查脚本）；实际约定散落 5 个来源互相不一致（三态提案 / API_QUICKSTART _create/_get 表 / TEN_LAYER_OPTIMIZED_PLAN _create/_alloc 后缀 / 224 处头注释自由文本 / Python _PtrOwner 运行期强制版）；**12 项核心 API 抽查 2 项脱节**：func_block_register（头注释一处说深拷贝一处说注册表接管=声称 take 实际 copy，照注释操作会泄漏）、module_compute_content_hash（头注释"调用者负责 free()"实际 lv_calloc 分配须 lv_free=UB）；另 API_QUICKSTART normalization_result_free 不存在（实际 destroy）；~40+ 头注释"调用者负责 free"与 lv_free 混用无法审计；graph_get_node 借用语义只写 design doc 未进头注释；module_add_axiom_package 等 4 处 API 无所有权声明 | **三态契约（S1 提案）未落地为单源文档+机器可审计标注**；2 处声称-实现脱节（M5，func_block_register 泄漏风险 + module_compute_content_hash UB）；11 项核心 API 实际语义健全（copy/take/borrow 已执行） |

### 1.51 测试替身面（第十一轮，K11）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K11 | 测试替身/辅助基座 | 无第三方 mock 框架/链接期 wrap；唯一 mock 机制是"struct 函数指针注入 + 全局计数/硬编码"，但**每文件手写、命名四套**（stub_/fake_/dummy_/mock_）；测试辅助基座三件套（test_helpers.h 163 文件 add_point 权威 / lv_test_geom_graph_builder.h 3 文件 / axiom_test_common.h 56 文件数据驱动）；**7 处一需求多实现**：R1 add_point 私有副本（recursion_demo.c:34）/ R2 图构建内联构造器 ×13（10 文件）vs 共享 builder / R3 verify_single ≡ verify_prove_src（test_lv_bootstrap vs test_lv_parser 同构）/ R4 插件替身 3 处手写 / R5 断言宏双轨 + 3 处 #undef override / R6 预言机 3 处手写未收敛生产权威 / R7 **测试设施反向耦合进生产库**（test_framework.c + bootstrap_test*.c ×8 无条件编入 lv_static，bootstrap_test_internal.h:29 在生产头重定义 CONSTRAINT_DISTANCE） | **测试替身机制未统一**（mock 四套命名 + 图构建 13 内联 + 断言双轨）；**P0 生产库混入测试设施**（test_framework/bootstrap_test 编入 lv_static，bootstrap_test_internal 生产头重定义约束常量）；生产代码无测试专用 #ifdef 分支（干净 ✅） |

### 1.52 数学常量面（第十一轮，K12）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K12 | 数学常量/符号 | **4 处真重复**：π 家族多源（config.h lv_PI 23 位权威 vs lv_platform.h M_PI shim vs 测试手写 21 位 vs Python 11 位/6 位）、e 双头+权威架空（lv_numeric.h lv_E **零调用** vs lv_platform.h M_E 测试在用 vs 手写）、GEOEVOL_PI_SMOOTH_FACTOR 双定义（geom_evol.h vs config.h compat 段，注意是 PI 控制器因子非 π）、黄金比哈希本地重定义（normalization.c:72 NORM_GOLDEN_RATIO_MIX 绕过 lv_HASH_GOLDEN_RATIO_64，Q14 漏网）；**缺权威定义**：无 lv_SQRT2/lv_SQRT1_2/lv_SQRT3 → MCTS_C 1.41421356（8 位有效损失 7 位精度）+ 测试 1.41421/1.4142/1.414 多精度散落；**绕过权威宏**：180.0/360.0 手写（lv_numeric.c/meta_proof.c/geometry_csg_eval.c）；角度换算死宏 lv_DEG_TO_RAD/lv_RAD_TO_DEG 零调用 vs 表达式直写；裸 M_PI/M_PI_2/M_E 黑名单命中未清零 30+ 处（测试侧） | **一常量多定义 4 处**（π/e/GEOEVOL/黄金比）+ **缺权威宏致 √2 手写精度损失** + 角度宏绕过/死宏并存；已治理基线（M_PI 收敛 14 文件 52 处、黄金比 Q14）勿重复报告 |

### 1.53 错误处理模式面（第十一轮，K13）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K13 | 错误清理模式 | **goto 164 处 / 31 文件**（错误/清理 ~138 + 控制流 ~26），**标签词汇 ~28 种**（fail/cleanup/error/out/_gcleanup/cleanup_bdf/prepare_fail/commit_fail/txn_rollback/txn_cleanup/pack_cleanup/push_error/gmres_cleanup/OOM/extract_fail/overflow/prove_depth_exceeded/static_done/dynamic_done/测试 tN_end·gN_end 等）；**lv_DEFER 家族 ~159 调用点 / 33 文件**（lv_auto_free 已休眠 0 处）；**一需求多风格**：构造器失败回滚 ≥5 写法（guard-detach / goto fail+destroy / goto fail+内联分散 / goto pack_cleanup 条件清理 / 提前 return+内联）、锁守卫同子系统双形式（groebner 3 文件宏+goto _gcleanup 36 处 vs groebner_engine_ideal lv_DEFER 10+ 处）、FuncBlock 失败销毁双写法（func_block_compose lv_DEFER vs func_block goto pack_cleanup）；**清理代码重复**：solver_symbolic mpz_clear 组合手写 4 次、lambda_unify destroy+free 3 次、preset_manager_query/path_trace_tree/lv_process 双路径重复；同函数混用（geom_evol geoevol_step_once lv_DEFER(10) + goto cleanup_bdf(6)） | **清理机制未统一**：lv_DEFER 为唯一推荐（单点注册零重复）vs goto 需手动枚举成功/失败双路径；构造器回滚 5 写法、锁守卫双形式、28 种标签词汇；合理差异保留（事务回滚/链表逐节点/MSVC 回退） |

### 1.54 适配器桥接面（第十一轮，K14）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K14 | 适配器/桥接/转换层 | **证明→Coq/Lean 一需求 ≥4 实现**（L4 proof_export / L5 proof_export_enhanced / L5 interop_export_coq/lean / L10 coq_bridge/lean4_bridge），**步骤类型→tactic 映射表 ≥8 处独立维护**（同一语义 4 层 8 表 D4 映射重复）；**图→SVG/TikZ 各 3 套**（L5 权威 vs 瘦版/visitor vs L4 module_export 越权）；ConstraintGraph 序列化多路径（graph_serialize 权威 vs LVZ 内嵌手写不可用 vs 薄封装仅测试，**lv_storage 注册表只注册 1 格式** 7+ 序列化模块未接入）；**lvProofStep 同名双定义**（interop_bridge_common.h:29 vs opml_codec.c:63）；证明导出双轨（L4 proof_export vs L5 proof_export_enhanced，I5 已列）；L0 门面三模式并存（L10 插件 / L5 文件+临时文件 / 内存直写）；upper_interop_export_* 三函数逐字同构 | **转换层未锚定**（证明→Coq/Lean 4 实现 + tactic 8 表 + SVG/TikZ 3 套 + 序列化多路径 + L0 三模式）；健康面：lv_dot_writer 统一被 7 消费方复用、L6 converter 守卫已收敛、L10 桥接骨架已收敛、bootstrap 双份 .lv 有显式豁免 ✅ |

### 1.55 全局状态线程安全面（第十一轮，K15）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K15 | 全局状态/单例/线程安全 | **一需求多容器 6 组**：配置 5 容器（A lvConfig 进程级 / B ConfigManager TLS 同逻辑键跨线程读不同值 / 死 KV g_state / geometry_config 独立 / SMT 默认配置）、内存统计双容器（TLS vs 进程级）、预设库同型双容器（g_library 仅 init 有锁 vs g_preset_library 完全无锁）、日志状态 4 容器（s_debug_state/s_runtime_state.log/lv_log.c g_log_state*/日志级别双权威）、后端注册表样板 ×5、stream_ctx ×14（宏已存在未收敛）；**线程安全缺口 8 个**：G1 lv_config 读写锁不一致（撕裂读）、G2 s_upper_state 零锁可写、G3 g_preset_library 零锁、G4 g_library 读路径无锁、G5 g_coeff_pool check-then-create TOCTOU 双建、G6 TLS 平台回退静默变进程全局、G7 系统状态机 TLS 守卫进程级事实（跨线程 lv_init 双重 init）、G8 lv_log_shutdown 检查-动作无锁；**初始化顺序**：模块注册表有序 ✅ 但惰性单例（lv_once 38 + lv_lazy_lock 43）无显式依赖图；裸 extern 可写全局 ~12 个（s_upper_state/s_debug_state/g_library/g_coeff_pool 等） | **全局状态管理未统一**（6 组重复容器 + 8 个线程安全缺口含 3 个真实竞态：lv_config 撕裂读/g_coeff_pool TOCTOU/跨线程 lv_init）；健康范本：memory_pool/runtime_monitor/geometry_config（进程权威+TLS 快照）/lv_error（TLS getter）✅ |

### 1.56 头文件包含卫生面（第十二轮，K16）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K16 | 头文件包含/依赖管理 | **三策略混用**：伞形（12 公共头 include lv.h，lv.h 扇出 41 头）+ 精确（33 头引 lv_utils.h 等基础头）+ 前向声明（20 类型 ≥2 处共 ~73 行重复 typedef：ConstraintGraph×15/lvEngine×8/StreamContext×4 等，**lv_fwd.h 不存在**）；**7 处传递依赖**（需要但未包含，靠"恰好被别的头带回"编译通过：lv_internal.h:209 宏体引 StreamContext 未包含、plugin_system.h 字段 lvContext 靠伞形、module.h/func_block_preset.h/proof_engine_enhanced.h/coeff_pool.h/solver_core.h 等）；**1 处内部头泄漏公共面**（proof_session.h → proof_session_internal.h → proof_rule_engine_internal.h）；**26 目标多拼写**（引号/带前缀/尖括号三态）；6 个内部头放公共目录；守卫命名 3 变体（lv_XXX_H/LV_XXX_H/lv_lv_XXX_H） | **包含规范未锚定**（三策略混用 + 7 传递依赖脆弱契约 + 前向声明 73 行重复）；健康面：守卫 305/305 全覆盖零 pragma once 零冲突、公共头图零真实环、lv_internal.h 公共头 0 引用（内部边界语义成立仅物理错位）✅ |

### 1.57 fuzz/测试工具面（第十二轮，K17）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K17 | fuzz/sanitizer 配置 | **2 个 fuzz target**（fuzz_constraint_graph/fuzz_symbolic_coord，模块边界清晰），每周日 cron 的 fuzz.yml 独立驱动，**完全游离于 ctest 与常规 CI**；**sanitizer 三处来源不一致**：全局 ENABLE_SANITIZERS=address,undefined vs fuzz target 硬编码 fuzzer,address（**fuzz 从不跑 UBSan**）vs fuzz.yml 显式 OFF 掩盖分叉；编译器选择写两遍、运行参数/corpus 目录 3 处硬编码仅 workflow、两 target 旗标逐条复制无生成函数；**缺口**：corpus 无 seed/无回灌（每周空目录起跑）、fuzz 回归不进 push/PR CI、crash 无回归归档、test_verification_report.md "3 个 fuzz 测试"陈旧 | **sanitizer 分叉（fuzz 缺 UBSan）+ fuzz 配置硬编码/复制 + 回归缺口**；分层合理：fuzz vs ctest vs integration 三层互补不算重复 ✅ |

### 1.58 打包/发布面（第十二轮，K18）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K18 | 打包/安装/导出 | **find_package 必坏**：install(EXPORT) DESTINATION 含 "build/build"（笔误）+ 与 lv-config.cmake 不同目录 + 导出目标 lv::lv_static vs 检查/文档 lv::lv 永不命中 + 导出文件烘焙 GMP 绝对路径；**lv.pc 路径错误**（prefix=C:/Program Files (x86)/lv、libdir=lib 相对未拼 prefix、lv_PC_LIBDIR 死代码）；**导出机制三处宏互相覆盖**：config.h:643 无条件空 #define lv_PUBLIC_API 覆盖 lv.h dllexport + 数十头 #ifndef 兜底 + lv_USE_SHARED 全仓零定义（dllimport 死代码）+ 非 Windows 无 -fvisibility=hidden，**实际唯一生效 WINDOWS_EXPORT_ALL_SYMBOLS（内部符号裸奔导出）**；**CPack 配置完整但从未生成**（无 release workflow、无 package 步骤）；**WASM 打包三处描述零实现**（CMake 标志+CI cp+文档声称，web/ 目录空且 gitignore）；版本号 ≥6 处硬编码无单源（preset_blocks 4.0.0 偏离） | **C 库消费接口三套两套坏 + 导出机制冲突 + 发布物从未生成**；健康面：安装规则集中在 CMakeLists 2239-2334、CPack 命名 lv-1.1.0.* 一致（纸面） |

### 1.59 数值稳定性面（第十二轮，K19）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K19 | 数值容差/精度/稳定性 | **权威基线已建**：config.h lv_EPSILON_* 分级 + lvGeometryConfig 运行时表 + 16_geometry_layer.md 第 5 条"容差单一来源" + lv_rel_tol_scale 相对容差共享设施 ✅；但**"一需求多容差"高发**：共线判定 7 值并存（1e-9 权威/1e-10 EUCLID+meta_proof+interop/1e-6 algebra_mode/1e-12/1e-15）、垂直/平行 5 值、角度 3 值、距离 3 值；**致命断链：lvGeometryConfig 的 perpendicular/parallel/angle_epsilon 三字段全库零消费者（只写不读）**，实际判定散落 solver(1e-6)/meta_proof(1e-6)/conflict_detector(1e-6)/interop(1e-10)/algebra_mode(1e-6)；1e-12 双源（lv_EPSILON_DOUBLE vs lv_EPSILON_ULTRA）；测试断言 287 处 tol 全裸字面量（1e-9×91/1e-12×66/1e-6×34，bicgstab 1e-6 vs sparse 1e-4 不一致）；跨语言 Python/Lean FFI 第四层定义；geo_utils.c:204 注释陈旧 | **容差单一来源已建但未执行**（7 值共线/3 字段断链/287 裸字面量）；数值稳定性有相消规则 6 条+hypot/缩放惯例但无集中规范文档+无裸魔法数黑名单；分层保留：精确 GMP/近似 double/区间三语义 + 谓词 EXACT/APPROX/ADAPTIVE 两层阈值 |

### 1.60 声明实现一致性面（第十二轮，K20）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K20 | 头声明 vs 实现 | **5 核心头 242 个 lv_PUBLIC_API 声明↔实现 100% 对齐**（0 真幻影，2 个名义缺项 proof_set_stream_context/module_set_stream_context 由 LV_STREAM_CTX_DEFINE 宏生成非缺陷）✅；**全库 527 个非 static 全局函数 0 符号泄漏、0 漏 static** ✅；**1 例实现无声明**：_symbolic_coord_degrade_check_algebraic（本地 extern 前向声明绕过头）；**死宏**：lv_LAYER_VALIDATION_FLAG_* 0 使用、PROOF_STRATEGY_* 数字常量仅 VECTOR 在用、MAX_MODULE_DEPTH 双头重复定义（config.h:655=module.h:48）；**跨头重复声明 56 组 34 组签名一致**（合理多声明，仅维护重复）；**幻影 API 汇总**：lv_get_version_info（结构体有 getter 无，替代 lv_get_system_info）、lv_get_memory_stats_ex/limit_ex（仅 README）、lv_get_version（从未存在）、normalization_result_free（真实 destroy）、lv_check_version_compat 恒真自比较（M6） | **声明-实现一致性健康**（5 头 100% 对齐 + 0 符号泄漏 ✅）；风险集中在文档层幻影（4 + check_version_compat 恒真）+ 死宏/死子系统（层验证）；1 例实现无声明待补 |

### 1.61 代码风格格式面（第十三轮，K21）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K21 | 代码风格/格式执行 | **唯一 .clang-format**（根目录，Google 基础 + 4 空格 + 120 列 + K&R + Regroup include）；**接入几乎为零**（CMake 无 format target、4 workflow 无格式化检查、无 pre-commit，唯一接入 ui/package.json 手动脚本非强制）；**42.6% 文件（557/1306）存在 clang-format 违规**；**9e6366f1"全项目格式化"从未触碰 test/**（core 536/test 0 文件，test 302 文件几乎全违规）；**版本漂移**（旧版本格式化文件用 22.1.7 复查 40% 重新违规）；**同文件 K&R/Allman 混用**（formula_renderer 4 拆分子模块各半，拆分后未格式化）；>120 行 2922 行、>200 行 429 行；include 乱序（lv.c/geometry_csg.c/test）；**全仓库 0 处 clang-format off 豁免**；注释：core 以 /** Doxygen（13438）为主、test 以纯 /*（9563）为主脱节 | **格式化覆盖率不足 + 接入缺失 + 版本漂移**；Doxygen @ 标签 100% 统一 ✅、缩进 4 空格主流 ✅；命名双轨（J5）/守卫三变体（K16）/动词三轨（J5）与格式化漂移同源建议合并立项 |

### 1.62 魔法字符串面（第十三轮，K22）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K22 | 魔法字符串/硬编码 key | **导出格式名 5 个独立分发点**（interop_theorem X-macro 权威 / kExportFormatHandlers / kProofExportFns / do_export / upper_interop 插件名，"coq" 5 处 "lean"/"lean4" 拼写并存）；**lvExportFormat 同类型名 3 处定义数值互斥**（proof_export_enhanced.h EXPORT_HTML=0 6 值 vs lv_view.h lv_EXPORT_COQ=0 12 值 vs 文档副本）；**变换预设名 7 个 × 5 文件 ≈30 字面量**（不在 preset_name_defs.h 单源）；**TransformType 名称 switch 12 字面量 vs 平行表含 "protective" 拼写错误与 GLUING 映射分歧**；**InteropCommandType 21 值 vs 名表 19 项**（GET_NODE/GET_CONSTRAINT 死值）+ 注释"18 种"过期；命令/步骤名 6 套词汇表（Rewrite 6 处 5 种拼写、AddNode 4 拼写）；序列化字段键散落（axiom_rule_engine ~40 键 if/else、interactive_geo 读写双份键集） | **字符串比较→查表战役基本获胜**（生产 strcmp 链=0、~50 张 name 表 ✅）；残留集中在"导出格式/lvExportFormat/变换预设/TransformType"4 簇跨表重复（D4，已产生真实数值/拼写错误）+ 序列化键读写双份；LV_CFG_* 35 键单源化是标杆 ✅ |

### 1.63 序列化互转矩阵面（第十三轮，K23）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K23 | 序列化格式互转 | **LVZ 内嵌图只写不可读**（写 graph 节读端无分支，S1/S2 实证）；**msgpack 缺图**（往返 graph=NULL，autosave 恢复优先二进制静默丢图，生产缺陷）；**JSON↔LVZ 无转换器**（两套独立 codec，module_delta"JSON 兜底"是依次试解析器非转换器）；**LVZD 有损子集**（只留坐标+POINT，读回重建失败，仅测试消费）；**OPML 导出→导入不对称**（步骤类型双表仅 5 项重叠 4 处错位、description/name 键错位、id/dependencies 丢失、导入产物是内部 lvOpmlProof 非 ProofNavigator）；**ConstraintGraph→JSON 3 条路径**（权威+薄封装仅测试+适配器）；**证明→JSON 3 套 schema**（OPML/export_json/compiler_to_json）；**图等价比较 4 套 + round-trip 3 套**（权威弱守护 meta_repr_graph_equivalent 不比坐标/信任色——坐标全丢也通过）；**跨格式等价验证为零**（同格式往返齐备、无格式A==格式B 交叉验证）；注册表只注册 1 格式 | **互转矩阵缺口**（只写不可读/缺图丢缺陷/OPML 不对称/跨格式验证零）；强等价比较器存在（meta_repr_isomorphic 含坐标）但未用于注册表分派；分层保留：LVZD 压缩子集/OPML/Coq/Lean 外部契约/展示只写格式 |

### 1.64 测试数据生成面（第十三轮，K24）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K24 | 测试数据/随机化 | **唯一 C 随机源 lv_random_***（xorshift64*，无 libc rand ✅）；**进程全局 RNG 状态被 4 个消费方互相抢占**（lv_init 播种 / approx_counter 每轮重播种 / interactive_geo 重播种 / bootstrap 生成器），任一调用破坏他人序列；**默认 seed 三处不一致**（approx solutions=12345 vs projected=42 vs time(NULL)）；**add_point 权威副本泄漏**（recursion_demo.c:34 私有副本，K11 R1 未闭环）；**圆构造双路径**（builder containment vs example graph_add_circle，注释还声称一致）；死生成器 bootstrap_test_random.c 零调用方（K11 R7 关联）；**测试数据组织**：断言/main 骨架 100% 收敛 ✅ 但数据构造复用不足（add_point 33/288、mk_rat 14、geom builder 3、axiom 数据驱动 56），内联硬编码 >80% 文件、生成器 0 生效 | **随机源状态管理"一个随机需求 4 状态所有者"** + 默认 seed 双值 + 构造辅助残留；单元内联/集成 fixture/fuzz corpus 分层合理 ✅（fuzz 与测试数据零交集） |

### 1.65 硬编码路径面（第十三轮，K25）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K25 | 硬编码路径/文件引用 | **共享库加载 Python 3 份独立实现目录池互斥**（_ctypes_binding build3+build4+build+Release+bin+lib / stream_bridge 只认 build / 另一 stream_bridge 引入 build_mingw）；**公理包测试路径 117 处字面量**（62 文件 test_axiom_*.c，同数据已在 INDEX.json 一份无消费方）；**预设 .lvz 目录双来源**（CMake lv_PRESETS_DIR 绝对路径不可重定位 vs CWD fallback）+ 56 文件名第二事实源 + 手写 memcpy 拼接（preset_blocks.c:133）；**测试输出 build_verify/ 硬编码 vs .gitignore 声称 test_outputs/ 脱节**；bootstrap .lv 4 变体探测（含 "Lv-00/" 前缀）；build3 写死 core 源码（interop_import_ggb_xml.c:823 dump） | **路径常量未单源**（构建目录名逐文件不一致 + 数据路径 117 处 + 预设双基准）；拼接已高度收敛 lv_path_join（~31 处 ✅，残留 1 处真问题）；平台分隔符已统一 lv_PATH_SEPARATOR ✅；资源定位根因：CTest WORKING_DIRECTORY 源码根 + CWD 相对依赖 |

### 1.66 错误注入容错面（第十四轮，K26）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K26 | 故障注入/容错路径 | **注入机制"齐全但零使用"**：lv_allocator_set vtable 切换（allocator.h:66/allocator.c:361）可注入但**从未用于失败注入**（仅 test_allocator_ext.c 测切换契约）；lv_set_memory_limit（lv_utils.c:424）唯一验证断言 test_utils.c:88-94 被注释禁用；**全库 grep oom_inject/fail_alloc/fail_point/mock_alloc/fault_inject/FAIL_POINT 零命中**（docs 唯一提及未实施）；**平行实现**：debug_alloc（含 memory_limit 检查）vs lv_malloc_tracked（绕过 vtable 直 malloc，tracked 旁路注入）；**注入盲区**：memory_pool 块分配用原生 malloc（:253）、GMP 分配不可注入；**错误路径测试分层**：NULL/非法参数覆盖数百处 ✅ 但**分配失败/OOM 路径覆盖为 0**（无任何测试让分配真实失败断言上层错误码）；错误报告机制（error ctx/链/宏）覆盖良好但测的是"如何记录"非"如何触发"；fuzz 2 target 纯内存安全无注入无错误码断言；OOM 文案中英冲突；熔断模拟靠 API 直调 | **一需求多注入未收敛**（allocator_set vs memory_limit 双通道均未接入测试、debug_alloc vs lv_malloc_tracked 平行、memory_pool 绕过）；fuzz/unit-NULL/错误报告三层语义合理是"分层不完整"（资源失败注入层整体缺失）非重复；1808 处生产错误返回（NULL 变体 701 处）是"错误产生"非"注入" |

### 1.67 UB 整数安全面（第十四轮，K27）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K27 | 整数溢出/UB/窄化安全 | **权威已建**：lv_arith_safe.h（lv_safe_{mul,add,sub}_i64 bool+out 唯一权威、lv_gcd/lcm 收敛 alg_gcd 等）✅、lv_ensure_capacity（lv_utils_misc.c:441 三层溢出检查，250+ 调用点全库扩容唯一入口）✅；**"一需求多防护"残留**：lv_SAFE_ADD 宏（lv_internal.h:232，int+饱和返回值）与 lv_safe_add_i64 双语义并存；10+ 处手写「capacity>INT_MAX/2 预检+×2」循环（graph_node_hash/bdd_encoding/rewrite_vf2/solver_groebner/engine_scheduler 等）与 lv_ensure_capacity 同构；array_grow_to_fit（lv_utils_array.c:38，size_t 版）vs lv_ensure_capacity（int 版）双口径（I3 残留）；func_block_utils.c:96/nt_polynomial.c:180 手写加法溢出检查；formula_dsl_lex.c vs axiom_pkg_parser.c 两套字面量解析预检（int64 vs int 宽度）；lv_SIZE_MUL_OVERFLOW 宏仅 memory_pool.c 内用；字符串缓冲倍增 3 类（lvStrBuf SIZE_MAX/2 vs lv_dstr_grow 转发 vs interop_theorem.c/graph_hash.c 自管）；**防护缺失（高危）**：主 .lv 解析链（lv_load_file→lexer→parser）无输入长度/递归深度/节点数闸门（parser_max_* 上限字段零消费）；parser_safety.c 的 lv_check_ast_depth/node_count/token_length/input_sanitize 全死代码（连头声明都没有）；graph_conflict.c:612-629 `max_node_id+1` 唯一具体有符号溢出 UB；text_code.c:136 先加后查（32 位可回绕）；**窄化**：全库 (int) 窄化 1539 处、(size_t) 1523 处，0 个 safe-cast 辅助，防御是直转/转后比较/转前比较三种零散写法；GCC -Wno-conversion/-Wno-sign-compare（CMakeLists:171-191）关掉窄化检查、Clang 保留但无 -Werror（两编译器行为不一致）；**sanitizer**：常规 ctest/CI 无任何 sanitizer（ci.yml 未开 ENABLE_SANITIZERS），fuzz 只跑 ASan 从不跑 UBSan（硬编码 fuzzer,address） | **整数安全权威层已建但未全量收敛**（lv_SAFE_ADD/手写倍增/双口径增长/两套字面量预检）；防护缺失集中解析链（死代码 + 未接线字段 = "文档宣称防护、生产裸奔"）；UB 模式干净（移位/INT_MIN 除法/除零均无，唯一 +1 溢出 graph_conflict）；__int128 守卫不一致（algebraic_number_io.c:48 无 SIZEOF_INT128 守卫，MSVC 无法编译）；分层保留：分配器 SIZE_MAX 检查（分配层）/lvStrBuf 倍增（缓冲层）/bit-burning SIZE_MAX 哨兵（熔断层）/sanitizer（动态层）合理 |

### 1.68 递归栈深度面（第十四轮，K28）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K28 | 递归深度/栈安全 | **30+ 深度常量/机制，其中 6 套核心机制全部是"死机制"（生产零调用）**：① parser AST 深度 lv_check_ast_depth（parser_safety.c:256，256）定义后从未被任何解析器调用；② circuit_breaker lv_CB_DEFAULT_MAX_DEPTH=100 生产零调用（仅测试）；③ reasoning_stack lv_REASONING_STACK_MAX_DEPTH=1000 push/pop 仅测试调用；④ context recursion_depth 字段 lv_CONTEXT_MAX_RECURSION_DEPTH=10000 生产从不递增；⑤ 全局 lv_MAX_RECURSION_DEPTH=128 lv_recursion_enter/leave 仅测试/示例调用；⑥ recursion_context 测度验证（10000/100000）生产零调用 + recursion_test_suite.c 死文件；**常量重复**："递归 128" 4 处（recursion.h:58/config.h:793/lv_RUNTIME_GUARD_MAX_RECURSE config.h:818+runtime_guard.h:53）；推理深度 100/1000/10000 三套并存互不知晓；lv_DEFAULT_MAX_DEPTH 同名不同值（lv_internal.h:90=64 vs proof_rule_engine_internal.h:35=100）；DEFAULT_MAX_DEPTH 无前缀 50 vs 20；MAX_MODULE_DEPTH 32 双处；销毁深度文档声称 10000（PROOF_TREE_DESTROY_MAX_DEPTH）vs 代码 200 差 50 倍；**无防护递归函数族**：DSL 递归下降解析（lv_parser.c 14 层、lv_sema check_expr）、formula 三解析器（current_depth 死字段，node_count 上限拦不住纯括号嵌套→可构造无限深递归）、formula_node_destroy/copy、lv_ast_destroy、lv_tree_release_recursive（被 proof_tree/proof_dependency/lv_protocol 共用）、lv_trace_node_destroy、lambda_unify 5 族子递归（occurs_check/apply_subs/is_pattern 等）、lv_lambda_destroy、sym_expr 7 族递归、dsl_compiler_ir compile_node；**栈风险**：render_latex_internal 每层 1-2KB 局部缓冲 × 无防护递归（深度 100 ≈ 100-400KB）；lv_impl_upper_app 16KB 帧；lambda_unify bound_args[256]（1KB）× 可达 1024 层；**显式栈样板已成熟**：graph_traversal_dfs/tree（DFSFrame/TreeFrame）、proof DFS/MCTS、graph_conflict（DfsFrame 4096）、prop_verifier destroy、module path 动态——全部堆上栈帧 | **递归深度"一需求多限制"**：推理深度 100/1000/10000 三套 + 128 四重复 + 同名不同值——应统一为单一深度限制表（depth_limits.h + 配置表，按递归域分层）；A1-A6 六套死机制要么接线要么删除（当前"文档宣称防护、生产裸奔"比无防护更危险）；无防护点按 destroy→解析→合一→渲染顺序补限；图/树遍历显式栈已是健康基线（默认 max_depth=0 不限需裁决） |

### 1.69 死锁锁顺序面（第十四轮，K29）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K29 | 锁抽象/锁顺序/嵌套锁 | **锁抽象双族**：lv_thread.h（lv_mutex_t 唯一实现 + lvLockGuard/LV_SCOPE_LOCK/lv_lazy_lock/lv_once）vs runtime_guard.h（lvRwLock + 重复 lvMutex typedef + lv_RUNTIME_LOCK/lv_READ_GUARD/lv_WRITE_GUARD，**默认编译为 no-op**，deadlock_warnings 字段无任何代码递增）；**编译器分裂**：LV_SCOPE_LOCK（lv_thread.h:163）与 lv_DEFER（lv_lifecycle.h:76）在 MSVC 下退化为手动配对/no-op——同一宏 GCC 自动解锁、MSVC 不解锁/守卫不生效；**非递归锁平台分裂**：Windows CRITICAL_SECTION 可重入、POSIX 默认互斥锁不可重入 → 同一代码 Windows 不死锁、POSIX 自锁，lv_mutex_t 无递归变体/无 trylock/timedlock（唯一超时=lv_cond_timedwait thread_pool.c:265）；**日志 3 锁 2 管道**（debug_state log_mutex / runtime_monitor log.mutex / lv_log g_log_state_lock）；**组同步 2 套**（lvWaitGroup vs lvTaskGroup）；**后端注册表锁 4 份复制**（atp/smt/numeric/singular 各自 lv_lazy_lock）；**lvRegistry 加锁/无锁双约定**（lv_registry_find 明确无锁 vs get/put 加锁）；**groebner g_data_mutex 3 种加锁惯用法**（GROEBNER_LOCK_GUARD_BEGIN / lv_DEFER cleanup / singular 手写 guard 直锁，初始化语义不同）；**跨模块双锁链无总序**：test/atp→registry、singular→g_data_mutex、g_data_mutex→log_mutex/config_init；**持锁回调倒锁**：lv_registry_remove/remove_prefix 持锁调 destroy 回调（lv_registry.c:268/306）；stream_async 消费者回调重入 stream_flush/stream_set_async_mode(false)/stream_context_destroy → 自等待/自 join 死锁（stream_async.c:169-175/137）；**潜伏自锁链**：memory_pool stats_mutex 持锁 lv_strdup（lv_mem_record_alloc 无生产调用方，一旦 lv_malloc 接上即自锁，memory_pool.c:24 头注释过期）；**锁粒度**：日志/组同步/注册表三处"同需求多锁"；high_dim_view _locked 分层 + preset_manager 解锁后回调为良好范本 | **"一需求多锁约定"7 项**（锁抽象双族/编译器分裂/平台重入分裂/日志 3 锁/组同步 2 套/注册表 4 份/Registry 双约定）；**最高危**：stream 自死锁 + registry destroy 回调倒锁 + POSIX 不可重入平台分裂；合理隔离（thread_pool 顺序获取不嵌套/各后端独立锁/memory_pool per-pool 锁/stream 单锁 dispatch 锁外）保留但缺书面总序 |

### 1.70 宏约定类型转换面（第十四轮，K30）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K30 | 宏命名/强转/X-macro | **宏风格分布**（core/ 3592 个 #define）：LV_ 大写对象式 53.6% / lv_ 前缀大写对象式 25.7% / LV_ 函数式 9.0% / lv_ 函数式 7.5% / 无前缀 compat 3.6%；**多命名点**：lv_ARRAY_SIZE（271 用，lv_utils.h:645）vs lv_ARRAY_COUNT（2 用，config.h:53 残留）同语义两宏名；X 列表命名两族 `*_X`（41 个）vs `*_ENTRY`（13 个）；守卫宏三变体（lv_XXX_H/LV_XXX_H/lv_lv_XXX_H，K16 复核仍在）；lv_PUBLIC_API 仍 ~30 处（config.h:643 无条件空定义 + lv.h 6 分支 + 数十头 #ifndef，K18 未收敛）；日志宏两族 LOG_*（lv_internal.h）vs lv_LOG_*（lv_log.h）；**强转**：≈4700 强转点（(int)1539/(size_t)1523/(int64_t)212/(uint64_t)202/(void*)118/(char*)222），**0 个 safe-cast 辅助、断言仅测试框架**，防御是直转/转后比较/转前比较三种零散写法；_Static_assert 表对齐守卫仅 5 处（好范式未铺开）；**X-macro 三档并存**：A 档完全派生（LV_TOKEN_TYPE_X 范本）/ B 档枚举手写+X 列表并行+_Static_assert（GeomType，但 graph_serialize.c:320/324 硬编码 6、lv_sema.c:104 硬编码 12）/ C 档手写表与 X 列表重复（algebra_mode.c:625-633 别名 switch 与 LV_GEOM_TYPE_ENTRY ALIAS 列逐字重复、prop_verifier_trust.c TRUST_* 手写名表）；计数三态（+1 技巧/lv_ARRAY_SIZE/硬编码）；85 张 lvStrToEnumEntry 表 + ~34 个 X-macro 消费点；**类型擦除**：void* 回调 typedef 42 个/25 头已统一 ✅、lv_topo_run/lv_bfs_run 8+ 消费方 ✅，但 solver_order.c 手写 Kahn（头注释声称已消除——承诺未兑现）、geo_dynamic.c:858 ~70 行自实现、装箱拆箱手写 35 处（24 装箱+11 拆箱，`(void*)(intptr_t)` 下标+1 防 NULL 约定无 helper）、lv_str_ltrim const 剥离 12 处（API 签名为 char* 所致）；**危险宏三类均 0 真阳性**（无括号参数/多语句无 do-while/副作用宏）——宏参数安全总体健康 | **"一需求多风格"8 项**（ARRAY_SIZE vs COUNT/X 命名两族/枚举三档+计数三态/窄化无统一检查/装箱拆箱手写/const 剥离/lv_PUBLIC_API/Kahn 承诺未兑现）；合理分层保留：LV_ 大写 vs lv_ 前缀命名分层、LV_GEOM_TYPE_X 2 列 vs ENTRY 6 列（信息量不同）、行为分发 switch vs 名称查找、通用 int-id 遍历+ConstraintGraph wrapper、lv_MIN/lv_MAX/lv_CLAMP/lv_SWAP 唯一权威；LV_SCOPE_LOCK（声明+调用两语句故意不 do-while）/lv_LOCALTIME（参数换序桥接）/LV_DISPATCH 双胞胎（void 不能作三元操作数）为文档化例外 |

### 1.71 字符串处理设施面（第十五轮，K31）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K31 | 字符串处理/动态串/通配符 | **权威已建**：lvStrBuf（76 文件接线唯一权威）+ lv_str_utils/lv_parse_utils 双权威族 + lv_str_prefix_len 收敛 + strtok 单路由 lv_strtok_r + lv_path_join 单实现 + 0 裸 sprintf/strcpy + 生产 strcmp 链=0 ✅；**"一需求多实现"新点**：**通配符匹配 ×3**（plugin_system_interface.c:187-218 迭代 *? vs preset_manager_query.c:40-68 递归 *? 语义等价双 static + test_framework.c:618-627 弱前缀匹配）；**标识符扫描惯用法 ×8**（lv_lexer.c:294-297/dsl_lexer.c:245-248/axiom_pkg_parser.c:143-146/module_lvz.c:131-135/gappa_dsl.c:193-195/gc_language.c:70-77/preset_common.c:117-124/lean4_bridge.c:151，字符集差异可参数化）；**空串惯用法残留 ×6**（coq_bridge/lean4_bridge/mini_kernel/interop_theorem 直写 strlen==0 绕过 lv_str_is_empty 权威）；**JSON 反转义解码双实现**（lv_json.c:34-47 表+101-164 内联两遍 vs lv_str_utils.c:824-886 lv_str_json_unescape，\u 码点原语已共享仅简单转义表双份）；**lv_dstr 僵尸设施**（lv_utils.h:1522 + lv_utils_misc.c:767-881，生产 0 调用仅测试，append_fmt 转发 lvStrBuf 但 grow/append 独立自写倍增 lv_realloc；头注释自称"已收敛薄封装"与实现矛盾=M5 声称与实现脱节；lv_utils.h:1522 与 lv_strbuf.h:14-16 双头各自宣称"统一"） | **字符串主体设施已收敛成形**（复核一致 ✅）；新增 4 个未登记重复点（通配符 ×3/标识符扫描 ×8/空串残留 ×6/反转义双实现）+ lv_dstr 处置（随 I3 增长单一路由合并）；分层保留：lvStrBuf/lv_asprintf/lv_fmt_tmp 所有权分层、lv_str_startswith vs lv_str_prefix_len 双语义、JSON/HTML/XML/LaTeX 转义四族、数字解析三原语、ctype (unsigned char) 惯用法；缺口：lv_str_hex_encode 声明(lv_str_utils.h:296)/实现(lv_hash.c:94)错位、interop.c:42-55 死注释 |

### 1.72 状态机实现模式面（第十五轮，K32）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K32 | 状态机/状态转换/阶段推进 | **全库 16 个显式状态机 + 2 组视图状态枚举**；转换分发形态 3 类：表驱动 handler（CDCL 10 态 solver_core.c:1080-1090 / WS 帧 5 态 interop_server_ws.c:847-852 / L7 编排 stage_dispatch，均配 LV_DISPATCH，最规范）、转移表查表（context 5 态位掩码 kValidTransitions context.c:280-286 / engine 5 态二维矩阵 engine_state.c:44-51）、if-chain/直写（SystemState/lvCircuitBreaker/lvPluginState/lvSessionStatus/ProofState/lvEditorState/lvTimerState 7 处）；**"一需求多实现"新点**：**证明进度状态双枚举**（ProofState proof.h:325-329 ONGOING/COMPLETED/CONTRADICTORY **死字段**——只写不读 + COMPLETED 从未赋值，完成语义由平行 bool nav->is_complete 承载 vs lvSessionStatus proof_session_internal.h:27-31 ACTIVE/COMPLETE/ABANDONED/ERROR 真实使用，语义重叠）；**确定性状态概念双枚举**（DeterminismState determinism_state.h:19-23 X-macro 单源 vs lvDeterminismState control_flow_blocks.h:12-18，VERIFIED 两边同名 + **NON_DETERMINISTIC vs NONDETERMINISTIC 拼写漂移**，全库无桥接）；**阶段完备性不变量重复**（orchestrator 内 ×2 自算 vs meta_verify.c ≥7 个手写循环重推同一"全部 COMPLETED/无 FAILED/prefix 顺序"不变量，且 meta_verify 用 **strstr 文本探测**状态语义——"尝试"/矛盾标记表 :168/:233，测试同样 strstr "proved" test_orchestrator.c:56）；**状态守卫 5+ 种手写形态**（lv_CHECK_ARG / if+return / if+set_error / bool 返回 / 直写，错误文案 K1 已登记 4 种"状态转移非法"措辞）；**状态强制覆写 4 处绕过转移表**（context.c:305 ctx_force_to_error 文档化 / context.c:841 未文档化且丢 previous_state / lv_convenience.c:151 未文档化 / engine_lifecycle.c:147 未文档化） | **"一需求多状态机"复核一致**：L1 context/engine 同义 5 态 + 两张名表 + test_engine_ops.c:491-510 奇偶测试逐格钉死两侧矩阵一致（该测试是 L1 合并连带件，设计文档未提及）；L2 进度模型 A/B/C、K1 状态名表、K15 G7 系统状态机 TLS、K22 假 engine_state 均已登记；**新增 5 项**：ProofState 死字段、确定性双枚举+拼写漂移、阶段完备性不变量 ×7 重复+文本探测、状态守卫 5+ 形态、强制覆写 4 处（2 未文档化）；分层保留：CDCL/WS 表驱动范本、熔断器单实现+委托、DeterminismState X-macro 一源多视图、不同对象生命周期（进程/引擎/编排/编辑器/计时器）、UI 状态枚举、lvTaskGroup 别名 |

### 1.73 文件 IO 资源管理模式面（第十五轮，K33）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K33 | 文件 IO/资源管理 | **权威基座已建**：lv_file.c 8 API（open/close/read_all/limited/write_all/read_text/exists/size，31 处打开调用点大部分已走封装）+ read-all 16 调用点全走 lv_file_read_all（手写 fopen+fseek+ftell+malloc+fread 在 core/src **归零**）+ 目录遍历唯一 lv_dir_foreach + 路径族收敛（K25 复核一致）✅；**I4 遗留未闭环**：**裸 fopen 18 处/10 文件**（I4 指名的 proof_version_nl.c:79/proof_navigator_export.c:191,223/sat_encoding.c:314 至今未切；日志族 debug_*+runtime_monitor 未补 exempt 标注）；**裸 fclose 24 处**（**高于 I4 登记 8 处需扩项**，含 5 处混合配对：module_export.c:96→140 等 lv_file_open 开+裸 fclose 关，丢失 fclose 失败日志）；**lv_storage file:// 后端未走封装**（fopen/fclose/file_size 内联，file_size 与 lv_file_size 逐行同构）；**打开失败错误处理 5 种约定**（lv_ERROR 日志权威 / lv_set_error_ctx / 静默 false 零诊断 proof_navigator_export.c:191 / stderr+降级 / 返回码+流事件）；**文件大小获取 4 生产 + 5 测试实现**（lv_file_size 权威仅 1 调用 / lv_storage file_size 同构复制 / debug_trace_session.c:111 / debug_state.c:264 / 测试 fseek+ftell>0 ×5）；**临时文件命名 3 形态**（lv_temp_path 权威 / lv_impl_upper_interop.c:70-71 内联 "lv_%s_%lld.tmp" 绕过 / graph_dot_export.c:281-282 "%s.tmp.dot" 合理需注释）；**裸 remove/rename 14 处**（module_delta.c:112-129 等 vs lv_path_remove 完整语义） | **文件 IO 收敛大头已落地**（read-all/写 API/目录遍历/路径族 ✅）；剩余重复高度集中且机械——P0 三项（11 处打开/关闭收编 + lv_storage 后端对齐）可一次提交闭环 I4 遗留；**核心风险是黑名单 grep 机制未落地**（I4 要求裸 fopen(/fclose( 进黑名单未执行）导致混合配对这类新漏网持续产生；无新增 P0 级发现；分层保留：流式 fprintf vs 缓冲单次写双惯用法、二进制/文本分层+PDF/LVZD 格式契约、fgets 行读 4 场景语义不同、lv_storage 存储抽象层；lv_DEFER 延伸至 FILE* 资源 0 处（K13 关联项，量小） |

### 1.74 命令/参数解析模式面（第十五轮，K34）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K34 | 命令分发/参数解析 | **入口全景**：C 侧 15 处（interop 命令层 VTable+ X-macro 名表 19 命令 / engine_scheduler 名→fn 表 4 任务 / lv_impl_upper_app switch 5 命令 / geometric_primitives geo_query if-else / test_framework 报告格式 if-else / smt_backend_impl_external z3-cvc5 if-else / lv_protocol lv_builtin_commands 纯字符串表 27 项+空壳执行）+ Python 侧 7 处（两 stream_bridge JSON-RPC if-elif / ws_server if-elif / monitor 键 if-elif / config getenv 4 手写 if）；**"一需求多实现"新点**：**命令分发表实现形态 5 种并存**（VTable 函数指针数组 / 名→fn 描述符表 / switch 枚举 / if-else 字符串链 / 纯字符串表+无执行）；**Python JSON-RPC 分发 if-elif 链 ×2**（stream_bridge.py:1000-1042 6 方法 vs ws_server.py:214-230 5 方法，同 {"jsonrpc","id","error"} 响应骨架+同 -32601 未知方法分支）；**参数个数校验+Usage 文案样板 ×10**（interop_command.c:331/376/420/482/506/654/682/767/843 + interop_command_stream.c:102 同骨架，仅文案与 N 值不同）；**流桥接服务器 CLI 双入口**（lv/stream_bridge.py 平铺 flags --port 3456 vs stream_bridge/stream_bridge.py 子命令 --port 5801，词汇默认值全不同）；**2 个疑似真实缺陷**：① interop_command.c:381 AddNode Circle 实际调 graph_add_line_segment（graph_add_circle 完整存在却未被命令层调用，test_interop 只测名解析不测执行）；② interop_command.c:593/604/615 Parallel/Perpendicular/EqualLength 全部映射 CONTAINMENT（ConstraintType 有 PARALLEL 枚举却无法经命令层产生）；**空壳**：Solve/Rewrite/Unify 命令返回罐头 JSON 不调 engine_solve/engine_unify（PackFunction/Instantiate 却真实调引擎）、lv_proto_terminal_exec 空壳全库零调用、lv_test_main 忽略 argc/argv（250+ 测试无法按名过滤）；**参数解析失败静默回落**（全部 int 用 lv_parse_int_default(,0)、AddNode Point 坐标失败置 0.0） | **"一需求多命令表"5 形态并存** + Python if-elif ×2 + Usage 样板 ×10（K22 命令名词汇表 6 套/命令枚举对齐已登记复核一致，不重复立项）；**健康面**：interop 命令层内部已收敛（VTable+X-macro+单一 parse/execute 路径，C WS 服务器复用）、lv_str_to_enum/lv_parse_* 解析底座唯一、lv_ini_parse 单一 INI 解析器、TEST_MAIN 单一测试入口、Python 工具全统一 argparse、C 侧 argv 选项解析需求本不存在、L9 5 命令 vs interop 21 命令协议分层文档已定；G1/G2 为逐行证据可复现的实现缺陷（命令名与调用目标不一致为事实，"应为 bug"为推测需测试钉住） |

### 1.75 编码/字符处理面（第十五轮，K35）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K35 | 编码/转义/字符处理 | **权威已建**：lv_str_utils.c 转义四族（JSON/HTML/XML/LaTeX 各有唯一查找表，`'` HTML &#39; vs XML &apos; 有意差异）+ lv_str_json_escape ~18 消费 + lv_dot_writer 7 消费统一 + lvJsonParser 读端全库唯一 + lvJsonBuf 写端 7+ 消费 + hex 编码 3 消费统一 + ctype 全库 (unsigned char) 惯用法 ✅；**"一需求多实现"新点**：**XML 转义重复**（module_export.c:70-83 手写 switch 版 module_export_xml_escape ≡ lv_str_utils.c:517 lv_str_escape_xml 实体集逐字相同）；**module_export.c:133 SVG <text> 自转义双写真实缺陷**（lv_strbuf_printf 追加语义误用——先 printf 再对同缓冲 append 转义结果 → 输出 "CIRCLE #3CIRCLE #3" 首份未转义；test_module_ext.c:244 只断言返回 bool 测不出）；**JSON 反转义两遍法 + 简单转义解码表三张**（lv_json.c:94-166 两遍法+自建表 s_json_escape_steps/:47 s_json_escape_decode vs lv_str_utils.c:824 lv_str_json_unescape 同转义集；lexer_shared.c:112 s_escape_decode 5 项子集；代理对已接 lv_str_json_read_codepoint K23 复核一致 ✅）；**字符串字面量语义分裂**（.lv 只剥引号不解码转义 lv_parser.c:569-579 vs .lvz/.pkg 解码 lexer_shared.c:120-195——"字符串字面量"一个概念两种行为）；**标识符字符类判定 ≥8 处手写**（lv_lexer.c:294/dsl_lexer.c:245/gappa_dsl.c:193/gc_language.c:70/module_lvz.c:131/axiom_pkg_parser.c:143/preset_common.c:117/axiom_pkg_serialize.c:234，字符集常量差异；coq/lean 桥接外部契约豁免）；**节式词法器骨架重复**（module_lvz.c:63-160 vs axiom_pkg_parser.c:62-164 数字/标识符/标点扫描近逐段同构）；**high_dim_view.c:208/255-295 手写 high_dim_json_append+手动逗号 vs lvJsonBuf 公共写入器**；**Python 解码不对称**（编码 _str_enc 收敛 ~12 文件，解码无 helper ~25 处内联 .decode('utf-8') + 2 处直连 encode 绕过） | **"一需求多转义"分层基本健康**（四族语义不同合理保留）；**新登记 9 项**：XML 转义手写版+自转义缺陷（P0）、JSON 反转义两遍法（P1）、三张简单转义表（P2）、.lv/.lvz 转义语义分裂（P2）、标识符字符类 ≥8（P2）、词法器骨架重复（P1）、high_dim 手写 JSON 写出（P2）、Python 解码无 helper（P1）、Python 2 处绕过 _str_enc（P1）；**一致性缺口**：证明→LaTeX 转义三缺一（proof_export_enhanced 全面转义 vs proof_export.c:309/proof_compiler.c:437/proof_navigator_export.c:206 原样写入）、运行时 BOM 处理缺失（lv_loader 原样读入，U+FEFF 表在死代码 lv_input_sanitize 里，C-㉖ 静态清库复核一致）、非法 UTF-8 无校验（约定"UTF-8 无 BOM"故可能有意，需决策）；健康面：转义族测试覆盖（test_utils.c:422-472/test_json_buf.c:345-416 双路径回归）、coq/lean 外部契约豁免标注到位 |

### 1.76 调试/诊断/追踪设施面（第十六轮，K36）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K36 | 调试/诊断/追踪/断言 | **权威已建**：debug_log 主管道（LOG_* 141 处，级别过滤+轮转+环形缓冲+FATAL 紧急保存）+ lv_log_message 委托层（lv_LOG_* 100 处）+ debug_* 族 11 文件水平拆分（I2 复核一致 ✅）+ SchedulerStats 活 + debug_invariants 端口/归一化不变量（engine_solve.c:219 活）+ proof_navigator 断点（生产活）；**"一需求多实现"新点**：**日志级别状态 6 容器**（g_log_level TLS 活 / g_min_level lv_log.c 半活 / runtime_monitor min_level 死子系统 / g_debug_level lv_impl_native.c:875 int 裸类型死 / s_lv_state.log_level lv.c:802 **断链死字段**——存入从不被任何管道读取）+ **词汇表 4 套**（lvLogLevel/LogLevel/lv_LOG_LEVEL_* 方向相反/LOG_LEVEL_*）；**级别映射表 ×3**（lv_log.c:45-51 g_lvlog_level_map / lv_utils_misc.c:359-370 kLogLevelMap / runtime_monitor.c:227-242 kLogLevelToLvLog，同语义各自维护 FATAL 均降 ERROR）；**lv_impl_native.c 第三调试输出族**（debug_trace/g_debug_level/debug_breakpoint/debug_dump 全族零调用零头声明）；**前置检查宏双族+同名冲突**（error_codes.h:439-511 4 参用 lv_set_error_ctx vs lv_check.h:26-196 3 参用 lv_ERROR，**lv_CHECK_RANGE 同名 3/4 参数**，lv_check.h:136-138 #undef 覆盖——include 顺序不同得不同语义宏，编译期陷阱，tikz_export.c:198/:238 同文件混用两族）；**日志行格式 ≥3 套**（debug_log 完整时间戳 / 遗留 [DEBUG] / lv_log HH:MM:SS）；**verbose 死/空开关 ×2**（geo_constraint_solver_newton.c:269 空 if 块 / proof_compiler.c:648 只写不读）；**环境变量命名分裂**（lv_MONITOR_THREADS vs LV_GROEBNER_PARALLEL）；**性能报告文本格式 4 套**（debug_counters_report/lv_get_system_info 内嵌/lv_diagnostics_write_file/lv_perf_report_print）；**FATAL→紧急保存双重失效（P0）**：全库生产零 FATAL 调用 + 唯一调用点 debug_trace_session.c:268 传 NULL（debug_emergency.c:42 `if(!filepath) return false` 声称"使用默认路径"但无默认路径处理——**崩溃保护实际从不触发 M5**）；**g_emergency_handler 从未被任何路径调用**（声称"在信号处理程序中调用"但全库无信号处理器接线）；**lv_set_log_level 断链**（lv.c:802 存入 s_lv_state.log_level 从不驱动实际过滤，config "debug.*" 三键只写不读）；**双 TraceSession 记账**（s_debug_state.trace_session 恒 NULL vs debug_trace.c g_trace_state）；debug_trace.c 断点 3 函数无头声明零调用（与 K20"0 符号泄漏"口径出入待复核） | **"一需求多日志"高度集中**：日志级别 6 容器/词汇 4 套/映射 3 表/行格式 3 套（L8/K15 已登记 3-4 套，本轮增量：g_debug_level 第 5 容器+s_lv_state.log_level 第 6 容器断链）；**P0 缺口 2 项**（FATAL 紧急保存双重失效、g_emergency_handler 未接线——崩溃保护失效属 M5 高危害）；**P1 死代码/断链**（lv_impl_native debug 族整族删、lv_set_log_level 接线或删、debug.* 三只写键、lv_CHECK_RANGE 消歧随 K30）；**分层保留**：debug_* 11 文件水平拆分、证明域追踪 vs 调试域追踪、proof_navigator 断点、lv_event_trace→事件总线→Stream 桥接、lvLogRingBuffer→lvRingBuf 泛化、SchedulerStats/PerformanceCounters/lvPerfStats/lvGuardStats 统计分层、conflict_detector 活 verbose；lv_log_with_context/lv_LOG_CTX 声称"结构化日志核心 API"生产零调用（P2） |

### 1.77 数学函数/数值原语实现面（第十六轮，K37）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K37 | 数学函数/数值原语 | **权威已建**：超越函数全走 libm 无自实现 ✅、幂按数域分层（GMP/int64/double/区间）、区间三语义 lv_interval_* 权威+空区间兼容层+AlgInterval、lv_random_* 唯一随机源（K24）、ODE 单表+参数曲线回调求值收敛、geo_predicate EXACT/APPROX/ADAPTIVE 分层（K19）、lv_vec3 点积叉积权威+CSG 委托、kAngleTable 豁免（K12）、serial 点积保序（H3）、有理数 4→1（E8）；**"一需求多实现"新点**：**lvVec3 模长无公共 helper**（geo_halfedge_mesh.c:816-817/879/906-907/946-947 + :613/:615 共 6+ 处内联 sqrt(dot(v,v))，geometry_transform_apply.c:323 同形态）；**3D 归一化三实现**（lv_normalize_3d lv_numeric.h:421 / lv_vec3_normalize lv_vec3.h:75 / geometry_transform_apply.c:323-327 内联 1e-15 裸魔法数绕过权威）；**3D 叉积手写 ×2 绕过 lv_vec3_cross**（parametric_curves.c:328-330/:383-385，文件已 include lv_vec3.h）；**2D 方向角 geo_angle（geo_utils.c:253）全库 0 生产调用**（死 API）+ 同语义内联 geo_constraint_solver_residual.c:188-189；**两向量夹角 acos+clamp[-1,1] 骨架 ×3**（geo_halfedge_mesh.c:815-825 与 :905-918 同文件近逐行复制、meta_proof.c:174-179 2D 输出度且 :179 内联 *180.0/lv_PI 绕过 lv_rad_to_deg）；**double 近似整数判定双实现**（lv_is_integer_double lv_numeric.c:73 vs lv_number_is_integer lv_number.c:702-708——后者 (int64_t) 截断对 |d|≥2^63 为 UB + 有理数路径 to_double 近似判定，**正确性隐患 P1**）；**近似相等双函数**（lv_is_equal 活跃 vs geo_approx_equal geo_utils.c:123 生产 0 调用+eps 下限钳制）；**区间幽灵头**（interval_arithmetic.h 生产零 include，重复定义 lvInterval typedef，实现为 interval_arith.c 弃用兼容层）；**loader op_pow 整数幂第 4 变体**（lv_loader.c:640-646 饱和线性循环非平方求幂，K2 延伸）；**lv_simd_dot_product_array vs serial_bicgstab_dot**（simd_batch.c:270 vs numerical_backend.c:758 刻意保序，H3 延伸）；**梯形积分两实例**（parametric_curves.c:178-211 1D / 346-393 2D）；**死 API 三件**（lv_sign/lv_sign_int lv_numeric.c:145-164 0 调用、geo_angle、geo_approx_equal）；lv_deg_to_rad 已激活（8 调用）但 lv_rad_to_deg 仅 1 调用 + 死宏 lv_DEG_TO_RAD/lv_RAD_TO_DEG（config.h:204-208）仍死（K12 延伸） | **"一需求多向量原语"高发**：模长/归一化/叉积/夹角 4 类几何数值原语各有 2-6 处手写/内联（N1-N5 建议合并为「几何向量原语收敛」P1 立项）；**正确性隐患 1 项**（lv_number_is_integer 截断 UB，P1）；**清理 7 项 P2**（近似相等双函数/内联模长绕过/区间幽灵头/角度换算残留/loader 第 4 种整数幂/点积双实现/死 API 三件）；**健康面**：超越函数 libm、幂按域分层、随机源唯一、曲线求值收敛、谓词分层、kAngleTable 豁免——不重复报告 |

### 1.78 时间/日期/时钟处理面（第十六轮，K38）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K38 | 时间/日期/时钟 | **权威已建**：三基座分层本身正确——单调 lv_get_time_ns/us/ms（50+ 调用点唯一权威）+ 墙钟 lv_get_wallclock_ns/ms（仅 6 处使用欠接线）+ CPU lv_clock_elapsed_*（6 处，I2 保留）+ 时间单位常量族 lv_NS_PER_MS 等全库一致 + lv_LOCALTIME 线程安全宏单源 + stream_timestamp_ms 单源（30+ 事件发射点统一）+ lvTimer 唯一计时器对象 + 基座有测试钉住 ✅；**"一需求多实现"新点**：**超时判断 3 时钟基座混用（P0）**：同一 timeout_ms 配置在不同引擎不同基座——prop_verifier_engine.c:33-35 用墙钟 time(NULL)（可被 NTP 跳变+秒级分辨率）、proof_version_sledge.c:121 用 CPU 时间 clock()（**I/O 等待/多线程下不推进 → 超时形同虚设**）、其余用单调（context/proof_rule_engine/lv_process/engine_scheduler ✅）；**死超时 4 处（P0）**：solver cdcl.time_ms（solver_core.h:161 声明无任何赋值，solver_core.c:1208 读取判超时 → **求解超时门恒不触发静默失效**）、meta_proof timeout_ms 配置+setter 无消费点、lv_protocol last_solve_time_ms 恒 0.0、conflict_detector max_check_time_ms 无读；**timestamp_ms 同名字段 3 语义（跨语言漂移）**：C stream 单调 ms（stream_utils.c:18-20）vs command log 墙钟 ms（command_log.c:120）vs Python 模拟事件墙钟 ms（stream_bridge.py:300 int(time.time()*1000)——与 C 同 schema 基座不同数值差 1e9 量级）；**追踪事件时间戳 3 样**（proof_trace 墙钟秒 vs proof_trace_tree 单调 ns vs debug_trace.c:96 **CPU 时间冒充事件时间戳**）；**墙钟秒获取 3 形态 8+ 处**（time(NULL) 直用 8+ 处 / time(NULL)*1000 伪毫秒 prop_verifier_context.c:37 / time(NULL)*lv_US_PER_S debug_state.c:204）+ **PROP_TIME_MS_PER_SEC 1000 双处定义**（prop_verifier.c:69 与 prop_verifier_internal.h:44，可复用 lv_MS_PER_S）；**休眠 3 形态 4 处绕过 lv_thread_sleep**（runtime_monitor.c:615 Sleep/:641/:669 usleep、lv_process.c:164-165 裸 nanosleep）；**lvTimestamp/lv_timestamp_now 死 API 语义歧义**（exact_arithmetic.h:14 单调 ns 拆秒/纳秒冒充 Unix 时间戳命名，全库 0 调用）；**elapsed timespec 绕道**（adaptive_threshold.c:286-289/367-376 拆进 timespec 手写差分，本可 uint64 直接差分）；**薄包装族 6+ 个**（orchestrator now_ms/lv_circuit_breaker now_us/lv_simd_now_us——**宏双处定义 simd_ops.c:155-156 vs simd_ops_internal.h:54-55**/axiom_template_test get_current_time_ms/path_type.c:38 内联，均为 lv_get_time_* 纯转发，I2"4 个薄包装"计划实存 6+）；**Python 超时原语双实现**（async_stream.py:50-73 自建 _async_timeout 3.10 手写 call_later+cancel vs 同模块 :423 及 ws_server.py:425/452 直用 asyncio.wait_for）；**复核未闭环**：K8 test_adaptive_threshold.c:439-465 手写 QPC+clock_gettime 双平台分支+裸字面量仍在 | **"一需求多时钟基座"调用点纪律问题**（三基座分层本身合理，问题是 R2 超时混用+R4 字段语义漂移）；**P0 2 项**：超时基座三混用（sledge 超时实际可能永不触发）、死超时 4 处静默失效（与 K28 死机制同族但属时间域新点）；**P1 4 项**：墙钟秒收敛 8+ 处、timestamp_ms 契约统一、休眠 4 处收编、K8 收尾；**P2 4 项**：lvTimestamp 删或改墙钟语义、timespec 绕道、simd 宏双处、Python 超时助手单源化；**分层保留**：单调/墙钟/CPU 三基座设施并存、stream_timestamp_ms 单源、时间单位常量族、lvTimer 唯一计时器、PDF D:%Y%m%d%H%M%S 外部契约豁免、Python 墙钟 TTL 缓存单实现；日期解析（strptime）全库不存在→无重复风险 |

### 1.79 协议编解码/消息帧/校验面（第十六轮，K39）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K39 | 协议编解码/帧/校验 | **权威已建**：字节序唯一权威 lv_store_/lv_load_{be,le}{16,32,64}（lv_utils_misc.c:977-1041 移位实现与主机序解耦，WS 解码/ggb ZIP/LVZD/LVZC/huffman 全收敛其上 ✅）+ lvByteWriter/Reader 打包层（lv_bytes.c 带游标有界读写，msgpack 全量构建其上）+ lvJsonBuf 命令响应主体统一 + lvJsonParser 读端唯一 + sha256.c 唯一 SHA-256；**"一需求多实现"新点**：**LVZD 容器头部布局缺陷（P0）**：写端在 uint8_t header[16]（geometry_compress.h:18 LVZD_HEADER_SIZE=16）中写 magic(4)+ver_major(4)+ver_minor(4)+**lv_store_le64(header+12,size) 8 字节写到字节 19**+**lv_store_le64(header+20,size) 8 字节写到字节 27**——**栈缓冲越界写 12 字节**；读端 :95 lv_load_le64(header+20) 从仅 16 字节读缓冲越界读 8 字节（fread 只填 16 字节 :72）；头注释自称 "Magic+Header+Payload+Checksum"（:28）实际**既无 Checksum 两个 size 字段与 16 字节缓冲自相矛盾**（28 布局 vs 16 缓冲）；生产零调用仅测试 test_geometry_core.c:1062-1068 且 :1063 断言 `write_ok==true||write_ok==false` **恒真**测不出；**AxiomPackage 内容哈希双实现**（axiom_pkg_serialize.c:117-206 权威字段集含 name/version vs axiom_pkg_depref.c:218-258 compute_lemma_block_hash 字段集含 lemma_block_id 缺 name/version，两种消费风格直用 lv_sha256_* vs lvHashCtx，:67 注释自认"人工同步"——字段集漂移则引用校验与内容哈希失配）；**字节序写入 3 处手写**（geo_visual_complete.c:1005-1010 png_write_be32 ≡ lv_store_be32 逐字节同构 / interop_server_ws.c:506-513 WS 帧头编码手写大端**但解码 :753/:771 走 lv_load_be16/64 同文件写读不对称**）；**PNG CRC-32 手写表**（geo_visual_complete.c:972-1002 全库唯一 CRC，与声称的 LVZD Checksum 无共享——校验和领域无公共设施）；**interop 命令 JSON 响应 3 处 snprintf 手拼**（interop_command_export.c:243/247、interop_server.c:477-480 vs 同层 30+ 处 lvJsonBuf）；**hex 解码无权威**（module_delta.c:252-276 手写 hex→u64 / u64_to_hash_string 用 lv_snprintf("%016llx") 绕过 lv_str_hex_encode，interop_theorem.c:300-330 手写 hex 校验+*31 派生 block_id）；**interop 输入"JSON-RPC 声称"与实现脱节（M5）**：interop_server.c:448/700 与 interop_command.c:116-118 声称支持 JSON-RPC，但 interop_parse_command :123-155 **仅空格分词**——全库 C 侧无任何 JSON-RPC 请求解析器（lvJsonParser 在 layer5 仅用于 GeoJSON 导入），外部客户端按 JSON-RPC 调用会拿 Parse error；**STDIO 帧无半行累积**（interop_server.c:620-632 fgets 固定 4096 超长行被切分每段当独立命令，与 WS 路径全消息累积+WS_MAX_MESSAGE_SIZE 上限不对称）；**常量双定义**（INTEROP_CMD_BUFFER_SIZE config.h:822-823 vs interop.h:62-63 同值 4096）；**死 API**（varint/zigzag lv_bytes.c:144-324 与 lv_BSWAP* cross_platform.h:372-376 core 零调用） | **字节序/打包层是已收敛范本**（lv_store/lv_load+lvByteWriter+msgpack 唯一权威底座 ✅）；**P0 1 项**：LVZD 头部越界写/读+恒真断言（缺陷真实但生产零调用处置成本低——修复或删除）；**P1 4 项**：AxiomPackage 哈希单源化、interop JSON-RPC 声称对齐（二选一）、deserialize_clers 长度非法静默成功（geometry_compress_decompress.c:48-52）、协议编解码测试缺口（WS 帧/msgpack/LVZD/OPML 无专门单测）；**P2 5 项**：字节序手写收编、snprintf 手拼、hex 解码补权威、SHA-1/base64 外部契约豁免标注+PNG CRC 登记、STDIO 长行累积/常量双定义/死 API 裁决；**分层保留**：不同传输协议（WS/JSON-RPC/JSONL/SSE/文本）、LVZD/LVZC 双层容器与 magic 探测、ggb deflate/PNG/ZIP/OPML 外部契约豁免（单一实现边界检查严谨）、三态心跳、响应/通知消息类别、SHA-1 单一实现 |

### 1.80 构建系统组织面（第十六轮，K40）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K40 | 构建系统/CMake 组织 | **权威已建（健康面）**：单根 CMakeLists.txt 2394 行管理 27 个 OBJECT 库 + lv_static/lv_shared + **288 测试注册全覆盖**（add_lv_test_auto 288 次 vs test/c 磁盘 288 个 .c **0 遗漏** ✅）+ 2 fuzz + 8 example；lv_PROP_VERIFIER_SOURCES 单源化 ✅、L4 子域宏嵌套复用 ✅（18 次调用 lv_l4_subdomain 内嵌 lv_setup_layer）、lv_L4_LIBS 聚合变量 ✅、全手写源清单风格统一 ✅、lv ALIAS 链 ✅、CI 平台分 job ✅、euclidean 内部包含模式 ✅；**"一需求多实现"新点**：**聚合目标清单逐字重复两份（P1）**：lv_static 内联 27 行 $<TARGET_OBJECTS:...>（1638-1667）与 lv_shared 同 27 行（1689-1718）**逐字一致**（$<TARGET_OBJECTS: 全文件 54 次=27×2）——新增/移动 OBJECT 库必须改两处；**公共依赖三元组手写三处**（${lv_L4_LIBS} lv_layer3_geometry lv_layer2_resource 在 1543/1602/1617 逐字，与 L4 宏传递依赖重叠冗余）；**平台/外部依赖链接散落 15 处**（GMP 7 处/Threads 3 处/ws2_32 3 处/libm 2 处未抽象 interface 库，测试/fuzz/example 的 GMP 与 lv_static PUBLIC 重复）；**L4 子域库创建宏 vs 手写双实现**（lv_l4_subdomain 宏 1498-1502 vs lv_l4_func_block 手写展开 1511-1515——额外 compile_definitions 完全可在宏调用后追加）；**include 目录集合 3 处声明且不一致**（宏 5 路径 1442-1448 含 core/src vs 聚合库 3 路径 1668-1673/1719-1724 不含——消费者直接 include core/src 下内部头会失败，当前恰好全走 core/include/lv 属侥幸）；**lv_HEADERS 手写清单与磁盘漂移 83 个 .h**（core/include/lv 305 个 vs lv_HEADERS 222 个，缺 algebraic_number.h/allocator.h/geo_utils.h 等公开模块头——仅影响 IDE 索引不影响编译）+ **64 个 .c 未列**（56 preset 属 J4 范畴 + 8 euclidean 内部包含）；**add_lv_test vs add_lv_example 脚手架 body 重复**（1762-1771 vs 2217-2224，example 缺 WIN32 ws2_32 分支）；**测试注册命名双轨 + 文档失同步**（显式 CTEST_NAME ~273 处 vs 缺省 ~15 处；docs:1028 引用 add_lv_test_and_register 代码实为 add_lv_test_auto）；**编译器 ID 分支重复 8 处 + 优化选项双轨冲突**（if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang") 6 处 + STREQUAL 2 处；**lv_core 无条件 -O2（1474-1480）覆盖 Debug/coverage 的 -O0**——断点失效/coverage 数据失真，真实缺口）；**CI 与 CMake 双份 include 路径硬编码**（ci.yml:121-124 -Icore/include... vs CMake 宏，CMAKE_EXPORT_COMPILE_COMMANDS 已开启（:8）但 CI 从未使用）；**MSVC 分支无 CI 覆盖**（113-139 /W4 /guard:cf /sdl 从未被验证——ci.yml windows-build 用 MSYS2 MinGW）；**G2 include 缺 core/src**（聚合库未提供） | **"分层思路健康、注册路径统一"但最终产物层抽象缺失**（P1 五项：R1 聚合清单单源化+R2 依赖三元组变量化+R3 外部依赖 interface 库 lv_platform_libs+R4 func_block 回归宏+R5 include 集变量化——占工作量绝大部分；R9 优化选项按配置生成器表达式限定）；**P2 六项**：add_lv_program 抽公共、头清单单一权威+漂移校验、测试命名统一+修 docs、编译器分支集中+MSVC CI job、CI 用 compile_commands.json、磁盘 vs CMakeLists 清单 CI 校验；**已登记复核一致**：J4 孤儿 56 个 preset_*.c 实测 56 完全吻合、K17 fuzz 旗标复制+sanitizer 分叉、K18 find_package build/build、E15 无 CMakePresets（9 个 build* 两套命名）、I5 L1 补链 1538、K27 窄化警告不一致、J3 CI 内联报告、J 系列 fix_build.py≡fix_cmake.ps1、K9 option 前缀混用、K21 无 format target；**健康确认**：测试注册 288/288 单一化、lv_PROP_VERIFIER_SOURCES 单源、L4 宏嵌套、lv_L4_LIBS 聚合、OBJECT 分层思路、euclidean 内部包含、lv ALIAS、CI 平台分 job |

### 1.81 几何判定/谓词实现面（第十七轮，K41）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K41 | 几何判定/谓词 | **权威已建**：geo_predicate.c EXACT/APPROX/ADAPTIVE 三层谓词（lv_orientation_2d/3d、lv_line_side/lv_segment_side、lv_side_of_circle、lv_segments_intersect、lv_point_in_polygon、lv_four_points_concyclic，ADAPTIVE 用 lv_rel_tol_scale+区间回退，K19 健康分层 ✅）+ geo_utils 已收敛委托链 6 条（geo_utils→predicate、func_block_selector→region、graph_node_vtable/geo_event_detect→intersect、proof_strategy_vector/euclidean_helpers→symbolic_coord_are_collinear ✅）+ 符号层 symbolic_coord_are_collinear（GMP 精确收敛）+ 编码层三态（GMP 方程/groebner 多项式/SAT 布尔）；**"一需求多实现"新点**：**三点共线浮点域 4 套**（权威 lv_orientation_2d 1e-9 / meta_proof.c:127 叉积 1e-10 / meta_proof.c:188 叉积 1e-9 / ga_interface.c:334 行列式 1e-6）；**点在线上 5 套**（权威 / meta_proof.c:227/:219 / recursion_selector.c:27 符号叉积+硬编码 1e-10 bbox / conflict_detector.c:484 **eps×1000=1e-6 阈值**）；**点在圆上 3 套**（权威 1e-8 / meta_proof.c:249 1e-9 / algebra_mode.c:665 **硬编码 1e-6**）；**线段相交 3 套**（权威方向谓词法 / interop_export.c:133 参数方程法 1e-10 / interop_export_svg.c:152/:181 二次方程法）；**垂直/平行浮点域无权威 ≥5 套散落**（geo_constraint_solver_residual.c:145/:161 叉积点积直接判零无容差 / solver_geom_templates.c:297/:412 1e-6 / algebra_mode.c:592-614 **硬编码 1e-12/1e-6** / proof_strategy_vector 符号 / 编码层三态合理——**lvGeometryConfig.perpendicular_epsilon/parallel_epsilon 字段全库零消费者**（K19 复核一致，权威表建成但无谓词函数接线））；**点在区域内 2 算法 3 实现**（geo_point_in_region_segments 射线法已收敛 / lv_point_in_polygon 数组版 / recursion_selector.c:111 卷绕数独立实现且 :294 同时调双轨）；**距离计算 3 套**（geo_event_detect.c:172 投影钳制 / simd_geo_matrix.c:31 SIMD 法向量 / residual.c:88 |cross|/len）；**方位角死 API + 3 内联**（geo_angle 死 / meta_proof.c:167 acos+*180.0/lv_PI / recursion_selector.c:89 atan2 / residual.c:177 atan2，两处已用 lv_angle_diff_pi 共享归一化 ✅）；**平行/垂直约束接线 3 套（P0 正确性）**：graph_add_parallel 真实 PARALLEL ✅ vs **interop_command.c:592-593 平行用 CONTAINMENT 创建（语义错误）** vs **formula_converter_constraint.c:59 垂直用 graph_add_betweenness 实现（垂直=点在两点之间，语义完全错误）** + **graph_add_perpendicular 全库不存在**；**缺口**：meta_proof.c:361 [PARALLEL]=eval_default **恒 false（平行/垂直约束 L1 直接矛盾证明不验证）**、graph_conflict.c:567 segments_can_intersect **空壳恒 true（"for now assume they can intersect"，Type 5 相交冲突永不触发）**、conflict_detector eps×1000 松 1000 倍无注释、硬编码容差散落（recursion_selector 1e-10/algebra_mode 1e-12/interop lv_EPSILON_HIGH）、recursion_selector 符号精确层+数值 bbox 混用 | **"一需求多谓词"浮点域高发**（共线 4/点线 5/点圆 3/相交 3/垂直平行 ≥5）；**P0 3 项**：新增 lv_perpendicular/lv_parallel 浮点谓词激活 2 个零消费者字段（承接 K19 P0，重复面仍在扩大）、垂直约束接线修复（graph_add_perpendicular 新增+formula betweenness 占位+interop CONTAINMENT 占位改真实类型）、meta_proof 平行/垂直补坐标级验证；**P1 5 项**：meta_proof 两处叉积+点线+点圆委托权威、algebra_mode 硬编码委托、recursion_selector 命名常量+卷绕数主从明确、方位角收敛（激活 geo_angle 或 lv_vec2_angle）、距离 API 或豁免登记；**P2 4 项**：conflict_detector eps×1000 具名、segments_can_intersect 空壳实现或登记桩、interop 容差 lv_EPSILON_HIGH→distance_epsilon、SIMD/残差豁免；**分层保留**：三层谓词（K19）、符号层 vs 浮点层（E8-E10）、编码层三态、求解器残差语义、2D vs 3D 域、SIMD 性能域、已收敛委托链 6 条 |

### 1.82 网络/套接字 IO 模式面（第十七轮，K42）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K42 | 网络/套接字 IO | **权威已建**：C 侧 socket 触点仅 3 文件（interop_server.c 监听脚手架 + interop_server_ws.c WS 服务器 + network_block.c 客户端栈）；interop_server_internal.h:19-39 条件编译头（__has_include 检测 + WsSock=intptr_t 统一句柄）方向健康但仅覆盖 interop 子域；socket 侧字节流缓冲唯一实现（interop_server_ws.c:905-927 recv 缓冲累积 + lv_buffer_consume 压缩）；select 单线程+16 客户端槽模型唯一；WS 断开检测（recv==0→清理）合理；file_block vs network_block 共享 lvIOBlockState 底座健康；**"一需求多实现"新点**：**TCP 监听服务器生命周期平台双分支整段手写 ×2**（interop_server.c:285-335 Winsock vs :336-378 POSIX——**Winsock 缺 SO_REUSEADDR 而 POSIX 有（行为不对称）**、协议参数 IPPROTO_TCP vs 0、错误码取法不同）；**socket 句柄表示 3 套并存**（WsSock intptr_t interop_server_internal.h:42 / lvNetSocket+LV_NET_SOCKET_INVALID network_block.c:17-20 / 裸 SOCKET+INVALID_SOCKET interop_server.c）；**平台分支判定 9 处 3 风格**（interop_server #if/#elif/#else 三态 / interop_server_ws #if/#else 假定 else=POSIX / network_block #ifdef _WIN32 未走 INTEROP_HAS_WINSOCK 检测——K9 迁移范畴复核一致）；**阻塞全量发送循环 ×2 语义漂移**（ws_sock_send_all interop_server_ws.c:252-263 **不处理 EINTR** vs lv_network_block_send :335-361 **处理 EINTR 但不防 n==0**——POSIX 信号打断时 WS 整帧失败连接被当错关闭）；**WSAStartup/WSACleanup 管理 ×2**（interop_server 每实例一次无引用计数 vs network_block 全局计数 g_network_wsa_count——若同时接线前者可能提前拆除 Winsock）；**select 超时常量双定义+一侧死定义**（interop_server.c:40,43 本文件零使用 vs interop_server_ws.c:33,34 同名同值——"移走未删净"）；**平台 socket 错误码映射 3 处**（WSAGetLastError vs errno，interop_server/ws/network_block 各一）；**network_block.c 整套客户端 socket 栈零接线（P0）**：connect/send/receive/set_url 无头声明（io_blocks.h 仅 create/destroy）全库零调用，仅 create/destroy 被 test_io_blocks_ext.c 测试——**完整但完全不可达的第二套 socket 栈，R1-R5 全部重复点的载体**；**C 侧 socket 全部阻塞且无读写超时（P0）**：SO_RCVTIMEO/SO_SNDTIMEO 零使用，唯一超时是 WS 主循环 select 100ms，network_block connect/recv 可无限阻塞；socket 路径测试 0 覆盖（test_interop_ext.c:5-6 明言"socket 阻塞循环登记遗留"） | **"一需求多 socket 实现"集中且第二套栈是死代码载体**；**P0 2 项**：裁决 network_block 删除或接线（死代码是 R1-R5 收敛前置）、C 侧 socket 超时策略决策（接线 network_block 前先定 connect/recv 超时方案）；**P1 5 项**：socket 平台抽象层（lv_socket.h 句柄/invalid/close/last_error/recv/send 包装统一 EINTR 语义）、lv_socket_send_all 收敛（统一 EINTR/0 字节防护）、SO_REUSEADDR 两侧对齐、WSAStartup 引用计数单点化、socket 路径测试补齐；**P2 3 项**：平台分支风格统一（#ifdef _WIN32→统一宏）、network_block recv 文档/实现对齐（单次 vs 循环/EOF/EINTR 重试）、超时策略登记；**分层保留**：WS 帧状态机+socket 缓冲（唯一实现 K39 已登记）、select 单线程服务器模型、lv_process poll（I1 范畴）、Python websockets/SSE/FastAPI（L4 已登记跨语言栈）、file/network block API 对称、WS 断开检测；**复核一致**：I1 外部进程调用、L4 WS 双栈、K39 帧/字节序、K32 WS 表驱动范本、K9 _WIN32 迁移范畴 |

### 1.83 对象池/资源池/缓冲复用面（第十七轮，K43）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K43 | 对象池/资源池/复用缓冲 | **权威已建**：lvMemPool（debug_mempool.c 空闲索引栈+used 标志+防双释放，无锁无扩容）+ lvObjectPool（memory_pool.c 空闲链表可锁可扩容带统计，生产接线 graph_node_alloc/graph_index）+ lvArena（bump 分配器 mark/rollback/TLS 临时，**生产零消费**仅测试）+ formula_pool（TLS 8×1024B 配对借用池，7 渲染子模块 15 处借用）+ lv_scratch_buf（TLS 覆盖式 scratch 已接 lv_cleanup）+ lv_bfs_run 共享设施 + dirty_set 已收敛转发 + groebner_engine 领域 ID 注册池（有锁自洽）+ expr_canon/geometry_transform 文档化不迁移决策；**"一需求多实现"新点**：**通用固定大小内存池两套互不委托（I3 复核一致未收敛）**：lvMemPool 空闲索引栈无锁无扩容 vs lvObjectPool 空闲链表可锁可扩容，分配底 lv_calloc（走 vtable）vs 原生 malloc（**绕过注入 K26 复核一致**）；**预设对象池 4 建 2 用**：lv_init_preset_pools 预分配 4 池（node/constraint/symbolic_coord/proof_step 各 1024 起步）但 **lv_get_symbolic_coord_pool/lv_get_proof_step_pool 全库零生产调用**（2/4 预分配固定开销无消费方）；**"MemPool" 名称三套 API 并存**（debug.h mem_pool_* / lv_mempool.c 薄包装 / lv_impl_native.c:842-866 pool_create/pool_alloc/pool_reset/pool_destroy **typedef lvArena MemPool 无头声明零调用**——同一标识符指向两种语义）；**TLS 临时缓冲三种形态**（覆盖式 lv_scratch_buf vs 配对式 formula_pool vs 各模块静态 TLS 定长缓冲 7 处——契约完全不同，formula_pool"同尺寸匹配"注释 :77 与实现 :87-93 不符首循环不比较 size 全槽恒 1024B）；**固定槽位数组+in_use+线性扫描池模式三份**（network_block 16 槽句柄表 / high_dim_view 视图槽 / formula_pool 8 槽）；**id_map 重映射表 7 处独立实现**（dsl_compiler_load/func_block_instantiate/module_lvz/graph_node_vtable/graph_node_copy/beta_reduce/solver_groebner 各自 calloc+置-1+扩容，全同构）；**池级统计三套均零生产消费**（lvObjectPool total_allocs/lvMemPool mem_pool_stats/lvArena 统计，与 L6 全局统计再叠加）；**ID/句柄分配多形态**（递增计数器 8+ 处 int/atomic/TLS 三形态，各域语义不同不建议强收敛）；**P0 真实缺陷候选：coeff_pool 尺寸失配 → 池块越界写**：coeff_pool_alloc(count)（coeff_pool.h:40-49）池路径**忽略 count 恒返回 1 个池块=8 元素**，回退路径按 count 分配——symbolic_coord_transform.c:1001 sqrt_poly.coeffs=coeff_pool_alloc(new_deg+1)（new_deg=2*deg，deg≥4 时请求 ≥9 元素超 8 元素上限），随后 mpz_init(:1007)/mpz_clear(coeff_pool.h:59-61) **越界读写池内相邻块/越出整块 malloc 区域**（触发概率取决于输入多项式次数，需复现验证）；**g_coeff_pool 永不销毁**（lv_mempool_static_destroy 全库零调用，未接入 lv_cleanup——J1 同族缺口）；**lvMemPool 完全无锁含 TOCTOU**（lv_mempool_static_init check-then-create 无锁，K15 G5 复核一致）；formula_pool 无清理路径（无 TLS 析构钩子，lv.c:360 注释自认遗留）；network_block 句柄表无锁（L6 单线程假设未文档化） | **"一需求多池"高度集中**：两套通用池互不委托（I3 未收敛）+ MemPool 三壳 + id_map 7 处 + 槽位池 3 份 + TLS 缓冲 3 形态 + 池级统计 3 套零消费；**P0 2 项**：coeff_pool 尺寸失配越界修复（count>8 强制回退或池块按请求对齐，补 deg≥4 测试）、两套池二选一互委托（随 I3/§3.33+K26 注入盲区）；**P1 3 项**：g_coeff_pool 生命周期接线（lv_mempool_static_destroy 接 lv_cleanup + init 内建 lv_once 承接 K15）、预设池按需化（删/懒建 SymbolicCoord/ProofStep）、统一三池线程安全语义（lvMemPool 补 thread_safe 或文档化单线程限定）；**P2 4 项**：临时缓冲契约分层文档化+formula_pool 修注释补 TLS 清理、id_map 共享工具（随 I3 容器族）、池级统计随 L6 收敛、槽位池样板登记（第 4 处再抽 lvSlotPool）；**分层保留**：lvArena/lvMemPool/lvObjectPool 分配器家族分层本身合理（但 lvArena 生产零消费需决策）、lv_scratch 覆盖式 vs formula_pool 配对式契约分层、图遍历每调用工作区、dirty_set 已收敛、expr_canon/geometry_transform 文档化决策、groebner 领域 ID 注册池、局部缓冲复用（ode_solver 环形历史/solver_core conflict_clause 复用） |

### 1.84 配置项定义/默认值/校验模式面（第十七轮，K44）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K44 | 配置项/默认值/校验 | **权威已建**：lvConfig A（17 子系统结构体 + 107 键 X-macro 四元组表生成默认值/getter/setter/JSON）+ LV_CFG_* 35 键宏单源（K22 标杆 ✅）+ lvGeometryConfig 进程权威+TLS 快照（K15 健康范本 ✅，默认引用 config.h 权威常量）+ lv_env_get_int/bool 环境变量唯一设施带钳位（K16 复核一致 ✅）+ A 优先/B 回落统一分发机制（v4 归一已消除系统 C）+ lvPluginConfig per-instance 合理分层；**"一需求多实现"新点**：**lvConfig 同一字段三套读取路径 + 107 个类型安全 getter 死表面（P0）**：lv_config_get_<key>() 生成的生产零调用（仅测试）/ 直接字段访问 lv_config_current()->field 18 处 13 字段 / 字符串键 A 优先分发——**lv_init 不加载 JSON 不应用 A（lv_config_load_json 生产零调用），107 键表生产仅读 ~13 字段，"A 权威"注册表大半键从未被消费**；**同一语义默认值多源（P0）**：rewrite 步数上限 1000 达 **6+ 处**（A 表 default_rewrite_limit/rewrite_default_max_iterations、compat 宏 lv_DEFAULT_REWRITE_STEP_LIMIT、B 键种子 rewrite.step_limit、engine 实例字段 engine_lifecycle.c:43/engine_resource.c:180/194、geo_rewrite 局部回落 geometric_primitives.c:241-242、Python EngineConfig engine.py:148、文档 API_REFERENCE.md:1158——独立字面量互不知晓改一处其余漂移）；**compat 段 #ifndef 宏与 X-macro 表 50 对成对重复（P1）**：表键（solver_max_var_id 100000/max_module_depth 32/log_max_files 5/health_score_max 100）与 compat 宏逐对同值，compat 注释自认"值固定"（:646）表变更不更宏——默认值漂移结构性风险源（当前 50 对数值一致未实际漂移）；**模块局部回退宏重复 A 表默认（P1 实际死代码）**：A 表 buchberger_max_steps 50000/groebner_reduce_max_steps 10000 vs 模块宏 BUCHBERGER_MAX_STEPS_DEFAULT/GROEBNER_REDUCE_MAX_STEPS_DEFAULT（groebner_engine_internal.h:77-78）作为 lv_config_get_int 回退——A 同名键恒命中回退值永不生效；**同名类型 lvSolverConfig 双定义（P1 潜伏地雷）**：geo_constraint_solver.h:98（几何约束）vs solver_core.h:171（CDCL SAT）**同名不同类型** + 默认函数 lv_solver_default_config vs lv_solver_config_default 近名——今天无 TU 同含两头（7 个 include 点互斥）但为编译冲突地雷；**lvApplicationConfig→lvSessionConfig 字段映射逐字重复两处（P1）**：lv_impl_upper_app.c:106-116 与 :143-152 同一 6 行映射块；**GROEBNER_ZERO_THRESHOLD 同文件双读路径（P1）**：groebner_engine.c:272/297 走 lv_config_get_double vs :662/:699 直接用宏（=lv_EPSILON_SUPERTINY 1e-15）——配置覆盖只影响一半调用点；**LV_CFG 注册表 5 个死键 + 伴生模块宏同死（P1）**：GROEBNER_SOLVE_MAX_ITER/GROEBNER_DEFAULT_VAR_CAPACITY/GROEBNER_SMT_ZERO_THRESHOLD/SMT_DEFAULT_TIMEOUT_MS/AABB_INITIAL_CAPACITY 零消费者（K19 零消费者模式在注册表层新实例）；**配置文档全面脱节（P1）**：API_REFERENCE.md 常用配置项 5 键中 **4 个幻影/死键**（solver.timeout_ms/solver.allow_approximation/log.level 不存在、memory.limit_bytes 实为 limit_mb、唯一存在的 rewrite.step_limit 是死键）、23_core_infrastructure.md 文档化宏名 config.h 中不存在（K9 幽灵宏扩展为整份宏名表漂移）、键名格式文档"模块.参数"与 A 下划线/B 点号三分裂、真实 107+35 键无文档列表；**同一键读写入口分裂（P1）**：solver 迭代上限 3 拼写（A 键 solver_max_iterations / B 键 solver.max_iterations 零读取 / 直接字段实际读取）；**校验语义三态并存 + 文档声称校验实为无校验（P2）**：A/B setter 静默接受任意值 vs env 钳位 vs 消费端钳位，**lv.h:695 声称 lv_config_set_int"值超出范围返回 false"但实现无任何范围校验（M5）**；Python EngineConfig 死配置数据类（定义+__all__ 导出零消费者，_ctypes_binding 无 lv_config_* 绑定跨语言通道不存在）；lvGroebnerConfig.enable_cache 只写不读（K19 模式新实例） | **"一需求多配置源"六级默认值漂移风险**（A 表/compat 宏/模块回退宏/模块默认函数/Python/文档）；**P0 2 项**：默认值单一事实源落地（A 表为唯一权威，compat 50 宏派生/删除、模块回退宏删、B 种子键删或改 A 拼写、lvSessionConfig 默认改读全局承接 G5）、lvConfig 读路径二选一（锚定字符串键公共面，107 getter 接线或降级 test-only 登记，lv_config_load_json 接 lv_init 或明确测试专用）；**P1 6 项**：lvSolverConfig 改名消歧、映射块抽公共函数、GROEBNER_ZERO_THRESHOLD 统一配置读、LV_CFG 5 死键清理、配置键单源表+文档对拍机制（修幻影键/宏名表/键名格式）、setter 校验语义锚定+修 lv.h:695；**P2 3 项**：enable_cache 接线或删、Python EngineConfig 删或接线、B 种子 5 键零读取清理；**分层保留**：lvGeometryConfig 独立子系统（K15 ✅）、lvPluginConfig per-instance、模块局部算法参数（ODE/traversal/render 类）、lv_env_get_* 环境变量设施（K16 ✅）、LV_CFG 键名单源（K22 ✅）、A 优先/B 回落分发机制、global_state 维持 G5 删除决策（A9 复核一致） |

### 1.85 API 参数校验/契约模式面（第十七轮，K45）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K45 | API 参数校验/契约 | **权威已建**：lv_str_utils/lv_parse_utils 前置条件文档+实现守卫一致（契约文档与实现对齐范本 ✅）+ 测试用 lv_ASSERT_* 与生产检查分离（0 测试文件用生产 CHECK ✅）+ 解析器结果结构体错误通道（域内合理）+ module.c 三元防 NULL 自洽 + 内部热路径免校验分层；**"一需求多实现"新点**：**空指针校验 4 套实现并存**（error_codes.h 宏族 A ~363 处可定制返回值含消息 / lv_check.h 宏族 B 91 处硬编码 -1 日志+ctx 双记录仅 37 文件 include / 手写 if+return 占绝大多数文件 / geometric_primitives.c:40-49 文件私有 CHECK_GRAPH/CHECK_ENGINE 第三族——同文件混用 tikz_export.c:198 vs :238、bdd_encoding.c include B 族却调 A 族，8 个 B 族宏 0 调用）；**同模块同一参数 4+ 种失败约定（graph 域最强证据）**：graph_index.c 相邻 4 getter 4 种——graph_get_node_count:985 **静默 0** / graph_get_node:1014 错误+英文消息+NULL / graph_get_constraint:1034 **静默 NULL** / graph_deactivate_constraint:784 错误码无消息；graph_add_point(graph_node_conflict.c:247) **NULL 图静默映射 ADD_NODE_CONFLICT（与几何冲突不可区分，误导性诊断链，interop_command.c:306 连带）** vs graph_add_node_with_id 错误+消息；**头契约 vs 行为/测试契约矛盾（M5 类）**：context.h/constraint_graph.h/circuit_breaker.h 声称"非 NULL"但实现容忍 NULL 静默降级（context.c getter 返回默认值、lv_circuit_breaker.c:56 等 9 处静默 no-op），**且被契约测试钉死**（test_context.c:99-106 含 **get_name(NULL)=="null" 魔法串**——声称校验但实现不校验是"有意为之但未文档化"）；**失败返回约定 5 种并存**（错误码/NULL/bool/默认值/静默 no-op——lv_reasoning_stack.c:68-71 错误码 vs :165-168 静默 0 同族可见差异）；**校验消息语言/记录分裂**（A 族中文"空指针: %s" vs B 族"CHECK: ..."日志+ctx 双记录 vs 手写英文 "graph is NULL" vs parser 英文）；**契约断言设施零接线（P0）**：lv_ASSERT_RUNTIME（runtime_guard.h:409）与 lv_verify_data_integrity（runtime_guard.c:148，6 项结构不变量检查 198 行实现）**全库 0 调用**，且真实实现受 #ifdef lv_ENABLE_RUNTIME_GUARDS 保护而该宏**全仓无定义**——**真实实现永不编译，生效的是恒 true 桩（runtime_guard.h:385）**（K9 死开关复核一致+调用侧零接线延伸证据）；生产 assert() 仅 3 处 GMP 自检——**无任何 API 用断言兜底调用方前置条件**；**契约测试缺口**：K26"NULL 覆盖数百处"复核一致（246/288 文件）✅ 但**仅 16 个测试文件断言具体错误码**，test_edge_cases.c:118-125 对 graph_add_point(NULL) 断言"任意非 OK 即 PASS"——**误导性 ADD_NODE_CONFLICT 被"任意失败即 PASS"钉住无法察觉**，无测试断言错误消息已记录；**明显缺校验**：graph_add_angle(graph_index.c:201) 无角度范围校验、lean4_bridge.c:84 半界校验（只查 step_type>=0 不查上界）、interop_add_node_point 不查 engine->main_graph 非空（NULL 图→CONFLICT→lv_ERROR_UNSUPPORTED 错误语义误导）；**冗余校验**：geo_create_node→graph_add_point 双层各查一次（防御性分层可保留需书面化） | **"一需求多校验"调用侧 4 套 + graph 域失败约定 4 种 + 头契约矛盾 3 家族**；**P0 3 项**：校验宏族调用侧收敛（给 B 族补返回值定制能力吸收 A 族 ret 语义后机械迁移 ~25 文件+geometric_primitives 私有宏+同文件混用+8 死宏）、graph 域失败语义统一（NULL 路径不再映射 ADD_*_CONFLICT 改 NULL 参数语义）、头契约与行为契约对齐（context/constraint_graph/circuit_breaker 二选一文档化 NULL 容忍或统一错误化+修正 getter 默认值不一致）；**P1 3 项**：失败约定契约表（按返回型家族定义 NULL 失败约定并入 K1 文案规范表）、契约断言接线或删除（lv_ASSERT_RUNTIME/lv_verify_data_integrity 接入调试开关+至少 1 调用点或随 K9 删——避免 198 行永不运行）、契约测试补强（错误码断言 16→全覆盖+消息断言+graph_add_point(NULL) 改钉具体语义）；**P2 3 项**：消息语言统一（承接 K1）、半界/缺失校验修正（lean4_bridge 补上界+graph_add_angle 补范围）、分层原则书面化（公共入口必校验/内部热路径免校验/双层防御性重复校验）；**分层保留**：lv_parse_utils 契约范本、测试/生产检查设施分离、解析器结果结构体错误通道、module.c 三元防 NULL、双层公共 API 防御性重复校验、内部辅助静默返回（性能分层） |

### 1.86 哈希表/查找设施实现面（第十八轮，K46）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K46 | 哈希表/查找设施 | **权威已建**：lv_hashtable 三形态（int/i64 开放寻址+墓碑宏模板单实现 ~90 处接线 / str 分离链式，I3 复核一致 ✅）+ lv_registry（哈希副索引+线性回退+锁，K15 复核一致）+ lv_str_to_enum/lv_enum_to_str 共享设施（~100 处调用，85 张 lvStrToEnumEntry 表，K30 复核一致 ✅）+ symbolic_coord_hash 唯一权威 + lv_hash 内容哈希抽象（SHA-256/FNV-1a 双算法统一上下文）+ hash_history 已收敛 + 三处完备豁免（expr_canon.c:84-112/fast_index.c:19-51/bdd_encoding.c:42-43 均带完整三次收敛评估注释 ✅）+ 静态小表线性查找合理（K22 健康基线 ✅）；**"一需求多实现"新点**：**R10 预设枚举名三份表 + 手写二分绕过共享设施（P1 最高价值）**：lv_impl_upper_preset.c:113-181 upper_NameEntry+upper_name_lookup **手写二分与 lv_xmacro.h lv_enum_to_str 逐行同构**，三张手写表（4/12/6 项英文）**违反 preset_category.h:10 "禁止在其他文件重复定义同枚举的任何名称表"明文禁令**且项数少于宏单源（25/16/7 项）——**UI 查询 PRESET_CATEGORY_ANALYSIS 等高级类别得 "UNKNOWN" 行为缺口**（vs func_block_preset_query.c:223-268 宏生成完整中文表）；**R1 约束图哈希指纹三实现+一死代码（P1）**：graph_hash.c:37 compute_complete_graph_hash（字符串序列化 FNV+聚合 64 位）/ graph_hash.c:150 compute_quick_graph_hash（二进制 **全库零调用死代码**）/ rewrite_hash.c:44 compute_graph_hash（二进制 FNV 混入 **32 位**）——#1 与 #3 混入方式/位宽互不兼容同一图两实现产出不同哈希（WL 图核哈希 rewrite_wl.c:523 不同算法合理保留）；**R2 节点坐标哈希聚合三实现（P1）**：unify_helpers.c:219-280 XOR+类型标记常量+黄金比 / normalization.c:594-623 XOR+FNV×prime+NORM_GOLDEN_RATIO_MIX / module_delta.c:410-421 与 **:634-645 同文件逐行复制**（XOR+黄金比+位移+type*0x9e3779b9）——混入策略互不兼容（graph_node_hash.c:313 注释记录了历史上 Knuth 乘数不同导致恢复后假阴性的真实事故）；**R3 "哈希索引+线性扫描回退"样板 ×7（P2）**：lv_registry.c:107-121/axiom_rule_engine.c:193-219/preset_blocks.c:284-296/performance_profiler.c:97-104/:149-156/func_block_preset_internal.c:43-51/dsl_compiler_ir.c:60-68/graph_index.c:1017-1031（graph_get_node）同骨架各手写（与 K43 id_map 相关但不同）；**R4 ConstraintGraph 内嵌索引同文件两套删除算法（P2）**：graph_node_hash.c node_index_remove（重插法 :136-177）vs constraint_index_remove（移位法 :265-303）同开放寻址表两种实现；**R5 黄金比裸字面量两处（P1，K12 延伸漏网）**：module_delta.c:420/:643 裸 0x9e3779b9ULL（同函数 :414/:638 用 lv_HASH_GOLDEN_RATIO_32 宏——同文件宏/裸字面量混用）、unify_helpers.c:260-261 裸 0x9E3779B97F4A7C15ULL（:251/:276 用宏，:261 是"+1 变体"无对应宏）；**R6 lv_hashtable 内部索引双策略（P2）**：int/i64 走位掩码 vs str 走 % 取模（容量恒 2 的幂等价仅性能/风格差异）；**R7 死代码（P2）**：compute_quick_graph_hash 零调用；**R8 查找失败语义三表达（P2）**：NULL（lv_hashtable_*_get/graph_get_node/lv_plugin_find）vs -1（id_hash_find/lv_registry_find/find_preset_index）vs 哨兵字符串（upper_name_lookup "UNKNOWN"/"ANY"/func_block_preset_query "未知类别"）；**R9 手写 next_pow2 ×3（P2）**：lv_hashtable.c:89 lv_ht_next_pow2/expr_canon.c:121 merge_bucket_count/graph_node_hash.c:332/:349 内联 while | **"一需求多查找"集中**：预设枚举名三份表+手写二分（R10 已产生真实行为缺口 UNKNOWN）、图哈希指纹三实现+死代码（R1）、坐标哈希聚合三实现（R2）；**P1 3 项**：R10 三张手写表改宏生成+upper_name_lookup 删改调 lv_enum_to_str（承接 K22/K30）、R1 图哈希家族收敛（删死代码+抽图指纹公共骨架或标注互斥语义，承接 K2 lv_fnv1a_mix_u64）、R2 坐标哈希统一入口（module_delta 同文件复制先抽函数）；**P2 6 项**：R3 回退样板登记/抽辅助（并入 K30/K43 id_map 工具）、R4 双删除算法统一或豁免、R5 黄金比收编权威宏（承接 K12）、R6 str 取模→位掩码、R7 死代码清理（并入 K2）、R8 失败语义登记（并入 K7）+ R9 lv_next_pow2 抽取（并入 I3/K27）；**分层保留**：lv_hashtable 三形态（I3）、WL 图核哈希（算法不同）、symbolic_coord_hash 权威、formula_hash 族（自洽单实现）、lv_hash 内容哈希抽象、expr_canon/fast_index/bdd_encoding 豁免标注完备、静态小表线性查找（K22）、插件/后端/模块注册表多份（K15/K29/H1 已登记范畴）、test 侧统一消费 |

### 1.87 树形结构/遍历模式面（第十八轮，K47）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K47 | 树形结构/遍历 | **权威已建**：lv_tree_release_recursive（lv_graph_traversal.h:123 回调驱动，3 消费方 proof_tree/proof_dependency/lv_protocol ✅ 树销毁收敛正确样板）+ FormulaNode 宏 VTable（FORMULA_NODE_FIELDS_X 单源 copy/destroy ✅）+ TypeRegion 字段清单驱动（type_system.c:524-570 单事实源 ✅）+ PropFormula 显式栈 destroy（K28 样板 ✅）+ BDD 引用计数 DAG（语义不同）+ AABB 2D/3D 宏泛型文档豁免（K28 B14 复核一致）+ Huffman 树已收敛 3 拷贝（geometry_compress_huffman.c:26-34 注释 ✅）+ lv_loader AST 求值族（LV_DISPATCH 表驱动同树不同语义健康分层）+ 全库无四叉树（0 命中）；**"一需求多实现"新点**：**T1 通用树遍历 lv_tree_traverse 生产零调用（P0 M6）**：graph_traversal_tree.c:28-176 显式栈 TreeFrame+BFS 队列完整实现，头注释 lv_graph_traversal.h:3-6 宣称"为证明树、表达式树等提供统一遍历抽象，消除各模块重复实现的遍历逻辑"，但**全库唯一消费是 test_graph_traversal_ext.c:310-319**——各树仍手写遍历（lv_loader 求值族/csg_evaluate/proof 导出 walker/verify_node_recursive/compute_color）接线或删除二选一；**T2 树销毁骨架 1 共享设施 + ≥4 手写变体（P1）**：lv_trace_node_destroy（proof_trace_tree.c:151-172，特例"不释放 proposition/step/rule"）/proof_dependency_destroy（proof_navigator_dependency.c:54-69）/csg_node_destroy（geometry_csg_node.c:87-100）/csg_bsp_node_destroy（geometry_csg_bsp.c:50-58）同一后序骨架逐行重写（与 K28 destroy 域补限合并）；**T3 证明域树族 ≥6 套其中 2 套生产零调用（P0/P1）**：lvProofTree（proof_tree.c 仅 test_proof_trace.c 消费）+ ProofSearchTree/BacktrackNode（proof_dependency.c 仅 test_proof_infra.c:744-809 消费）+ **proof_widget_get_search_tree（proof_widget.c:507-517）与 proof_widget_get_dependency_graph（:520-528）返回硬编码 JSON 桩恒空骨架（M6 声称可视化实际不落地）**+ lv_proto_tree（lv_protocol.c:574 C 侧生产零调用仅 test_output_export.c:197-218，可能经 Python 桥接需确认）+ 三结构（lvProofTree/ProofSearchTree/lvProofTraceTree）同构仅引擎来源不同；**T4 证明树导出 walker 三形态（P2，与 K14 合并）**：JSON 递归（proof_dependency.c:655）vs 平铺 all_nodes（proof_trace_tree.c:638）vs 递归文本（proof_tree.c:254），DOT 同（:690 vs :556）——"树→DOT/JSON 导出"骨架同构可参数化共享；**T5 ProofColor 合成格双实现（P2）**：proof_navigator_compute_final_color（proof_navigator.c:45-89 平铺）vs proof_dependency_compute_color（proof_navigator_dependency.c:78-127 递归），叠加规则 DARK_ORANGE>AMBER>ORANGE>YELLOW>BLUE>GREEN 逐段同构（TrustColor min 格不同语义保留）；**T6 树深/叶子计数分散维护（P2 弱）**：AABB height+tree_depth/leaf_count（aabb_tree_impl.h:601-615）vs 证明树 max_depth 增量+重扫（proof_trace_tree.c:378-398）vs proof_optimize.c:160-191 重算分支因子；**T10 FormulaNode 节点类型分发表 9+ 套手工维护（P2，K30/F3 延伸）**：唯一宏派生 copy/destroy（FORMULA_NODE_FIELDS_X），eval 表（formula_eval.c:245-270 **24 项 vs 36 枚举漏项经 LV_DISPATCH 静默返回 0.0——M5 静默降级候选 K7 关联**）+ render ×6 后端 + node_to_string（F3）各自手写 per-type；**风险**：4 个手写递归销毁在深树（恶意 .lv/DSL 输入）下可爆栈（与 K28 无防护递归族同源） | **"一需求多树实现"证明域最集中**（≥7 个树结构，P2 登记的 3 套之外新增 4 套）；**P0 2 项**：T1 lv_tree_traverse 接线（至少 1 个真实消费方如 csg 求值遍历）或标注删除消除 M6、T3 证明树去重（lvProofTree/ProofSearchTree 删或接线二选一建议删——主证明树已由 lvProofTraceTree 承担；proof_widget 两桩补实现或标注未接线）；**P1 2 项**：T2 树销毁 4 手写变体接入共享设施（get_children+cleanup 适配含 trace_node 所有权特例，与 K28 destroy 域深度补限合并）、T4 导出骨架共享（并入 K14）；**P2 3 项**：T5 lv_proof_color_merge 原语、T10 分发表由 FORMULA_NODE_FIELDS_X 派生（修复 eval 24/36 漏项静默 0.0，K7 关联）、T6/T9 登记（树深计数现状豁免；PropFormula 登记为第 4 公式树豁免——纯命题 7 节点 vs FormulaNode 36 节点语义不同）；**分层保留**：T7 树路径查找 vs 祖先链（语义不同已有评估注释 proof_trace_tree.c:8-29）、T9 PropFormula 第 4 公式树、BDD 引用计数 DAG、TypeRegion 字段清单树、Huffman 显式栈、AABB 2D/3D 宏泛型+深度 64、lv_loader AST 求值族、lv_tree_release_recursive 3 消费方、K28 显式栈样板族 |

### 1.88 符号表/名称绑定/作用域面（第十八轮，K48）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K48 | 符号表/名称绑定/作用域 | **权威已建**：lv_registry（lv_registry.c:107-121 哈希+线性回退+锁+destroy）+ lv_hashtable_str 权威容器（loader S2/axiom S9 正确复用 ✅）+ 公理包复合 key 包级隔离（axiom_pkg_core.c:36-45 "pkg:kind:name" 命名空间健康范本 ✅）+ namespace_depth 图编译作用域（20+ 文件语义一致 ✅）+ λ De Bruijn 绑定（lambda_unify.c:814-854 ✅）+ 模式匹配绑定（rewrite_binding.c:45 3 文件共享 ✅）+ 公式 x/y 变量表 + 5 种不同 DSL 各自关键字表（合理保留）+ .lv 三张共享几何词表已收敛（lv_lexer.c:133-155 ✅）；**"一需求多实现"新点**：**D1 name→ID 绑定表多实现（P0）**："平行数组+哈希索引+线性回退"模式 ×4（mini_kernel.c:391-459 symbol_names+symbol_index / dsl_compiler_ir.c:36-72 symbols+symbol_to_ir_id / preset_blocks.c:280-297 / func_block_preset_internal.c:37-56，含"值存 index+1 避开 NULL"技巧逐字同构）+ 纯哈希 ×2（lv_sema.c:115-142 / preset_manager.c:133-139）——权威 lv_registry 存在未复用；**D4 量词绑定泄漏（P0 风险）**：lv_sema.c:410-430 check_expr_quantifier 对 vname add_symbol 后**从不解绑**，check_let(:186) 同样只加不弹，无作用域栈/遮蔽/解绑原语——同一 analyze 内 forall x 之后顶层 Point x 将误报 duplicate declaration 或引用错绑类型（需最小测试钉住后修复）；**D2 .lv 关键字/类型名 4-5 处手工维护（P1）**：12 个实体名散落 5 处（lv_lexer.c:64-107 s_keywords 44 词 / lv_parser.c:155-168 s_is_entity_type_tokens / lv_sema.c:82-94 LV_SEMANTIC_TYPE_X / lv_sema.c:58-71 kEntityToSemanticType / lv_ast.h:70-82 **LV_ENTITY_TYPE_X 已是 X-macro 单一事实源但其余未从它派生**）+ s_statement_start_tokens 25 词手写列——新增关键字需改 4-5 处（lv_token_type_name 已由 X-macro 生成 lv_lexer.c:531-538 说明机制可行）；**D3 重名处理语义分裂 4+ 种（P1）**：sema 报错拒绝（lv_sema.c:125-127）/ loader 静默保首（lv_loader.c:106-109）/ mini_kernel 静默更新重绑（:416-435）/ dsl_compiler_ir 静默追加双条（:36-51 hash insert 失败忽略）/ preset_blocks 警告拒绝（:501-505）——同一"重定义"需求 5 种行为，Metamath 重绑 vs .lv 报错语义相反；**D5 .lv 名称绑定链路双轨（P1）**：文本链路（sema S1+loader_names S2）**全库生产零调用**（lv_sema_analyze/lv_apply_parse_result/lv_load_file 全部 12 处调用点都在 test_lv_bootstrap.c）vs 生产 .lvz 链路（module_lvz→func_block_registry/preset_blocks + dsl_compiler IR）——两链路各维护一套独立绑定表（K3 已登记 dsl_compiler 声称脱节，本次确认 sema/loader 链路整体仅测试消费）；**D6 派生预设名生成双系统（P2，随 F7）**："%s_bound_%d" 两处语义不同（preset_manager_compose.c:257 用 param_index vs func_block_preset_ops.c:699 用全局计数器）+ compose "%s%d"/"recursive_%s_%d"（:980/:1081）；**缺口**：R1 量词变量泄漏、R2 重名 5 分裂、R3 DSL IR 重名静默双条目、R4 .lv 模块/import 无作用域隔离（lv_sema.c:559-560 check_stmt_noop MODULE_DECL/IMPORT_DECL 解析了但绑定语义为零）、R5 loader_names 全局静态 256 上限、R6 LvSymbol.name[64] 冗余字段（lv_sema.c:31-35 自认"冗余保留"） | **"一需求多名称表"6 处新发现**；**P0 2 项**：D4 量词解绑修复（check_expr_quantifier body 检查后解绑+补测试钉住"量词变量不出作用域"）、D1 立项"name→ID 绑定表统一设施"（以 lv_registry 为基座扩展整型值形态或抽 lvNameTable，迁移 mini_kernel/dsl_compiler_ir，与 F7 预设容器三合一+K43 id_map+I3 合并）；**P1 3 项**：D2 关键字/类型名单源化（s_keywords 从 LV_ENTITY_TYPE_X+LV_TOKEN_TYPE_X 派生+s_is_entity_type_tokens/s_statement_start_tokens 改 X-macro 生成+LV_SEMANTIC_TYPE_X 复用实体名字列+_Static_assert 对拍）、D3 重定义语义统一（.lv 系收敛单一策略建议报错拒绝+登记跨域三选一策略表）、D5 .lv 文本链路定性（标注"自举/测试专用"承接 K3 或并入统一设施）；**P2 3 项**：D6 派生预设名随 F7、LvSymbol.name[64] 冗余清理、module/import no-op 文档化（@impl-none 承接 K3）；**分层保留**：权威容器层（lv_registry/lv_hashtable_str）、公理包复合 key 隔离、namespace_depth、λ De Bruijn、模式匹配绑定、公式变量表、5 种 DSL 各自关键字表、.lv 三张共享几何词表 |

### 1.89 依赖获取/服务定位模式面（第十八轮，K49）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K49 | 依赖获取/服务定位 | **权威已建**：分配器单事实源（lv_allocator_get + lv_malloc/calloc/free 全库统一 ✅）+ 全局线程池唯一（lv_get_global_thread_pool lv_once ✅）+ lv_error TLS getter（lv_error.c:103 K15 目标范本 ✅）+ geometry_config 进程权威+TLS 快照（K15 ✅）+ lv_registry 统一设施被 8+ 子系统复用（数据结构层已收敛 ✅）+ context 资源操作回调注入（L0 注入 L3/L4 解耦模式 ✅）+ instance 级 getter 族（对象级规范已成形）+ interop_server persistent_engine 缓存+快照回滚（场景合理需登记）+ 配置 A getter/B 回落分发机制（K44 已决策）；**"一需求多实现"新点**：**N1 interop 模块内 StreamContext 获取双轨并存（P1 高价值）**：轨道 A 直接读模块 TLS 全局 interop_stream_ctx（interop.h:28 extern，interop_export_coq/lean/pdf/html 等导出子模块全走此路）vs 轨道 B engine_get_stream_context(engine)（engine_stream.c:20，interop_command_stream/interop_theorem/interop_server 命令子模块全走此路）——同一依赖同一模块内"TLS 全局注入"与"引擎实例 getter"两种机制，导出侧无 engine 参数走全局命令侧有参数走 getter，接线路径历史分化非分层必然；**N2 StreamContext 三形态并存（P1）**：① 模块 TLS 变量（14+ 模块）② engine_get_stream_context（引擎实例字段直读）③ lv_context_get_stream（lv_stream_context.c:31 懒创建）——三个不同持有/创建策略（proof_get_stream_context/rewrite_get_stream_context/prop_verifier_get_stream_context 仅 3 模块有 getter 范本其余 11+ 直接 extern）；**N3 流上下文三件套宏双份/三份定义（P0）**：LV_STREAM_CTX_DECLARE/DEFINE 在 stream.h:338-347 与 stream_context_util.h:35-44 **逐字重复** + 第三套 lv_DECLARE_STREAM_CTX（lv_internal.h:209 仅声明变量无 setter）+ 5+ 处手写（axiom_pkg.c/formula_converter_util.c/debug.c/high_dim.c/solver_engine.c 各带"不适用宏"注释）；**N4 进程级单例 getter 命名风格 4 种（P1）**：_current()（lv_config_current/lv_error_context_current）vs _get()（lv_allocator_get）vs get_global_（lv_get_global_thread_pool）vs get_global_state/_global()（bit_burning_get_global_state/lv_backend_plugin_registry_global）；**N5 engine_bind_context() 幻影注释（P0 M5）**：engine_lifecycle.c:45 注释"后续可通过 engine_bind_context() 设置"——**全库 grep 无该函数声明或定义**，引擎↔上下文绑定实际走 orchestrator 直接字段赋值（lv_impl_upper_orchestrator.c:208）+ lv_convenience 借图（:172-178）两条路，engine->context 迁移中（engine.c:9-28 自认第 2 阶段）无绑定 API 也无 getter；**N6 引擎实例获取 4 种模式（P1）**：① 工厂 engine_create() 经 lv_engine_create()（lv.c:500 含 lv_init 守卫，测试双入口混用 test_engine_ops.c:37 vs test_engine_ext.c:40）② server 懒创建缓存 persistent_engine（interop_server.c:504-515）③ 每次临时创建/销毁（lv_convenience.c:164-179）④ **TLS 隐式指针 g_tls_engine（engine_scheduler.c:62 旧 API 专用注释自认 legacy，lv_engine_schedule:1174 从 TLS 取出）**；**N7 注册表"获取"承载 4 种（P1，补充 L3/H1）**：统一设施 lv_registry_get（8+ 子系统）vs 专用 global getter（lv_backend_plugin_registry_global）vs 模块内 static 状态（SMT/ATP/数值/Singular 各持 static 无对外获取 API）vs 实例字段注册表（engine->scheduler->routing_rule_registry）——同类"注册表服务定位"四种承载；**N8 依赖获取失败处理约定不一致（P2）**：interop_command_stream.c:40-44 检查 NULL 设错误码 vs interop_export_coq.c:292-300 仅 if 静默跳过 vs engine_stream.c:22 lv_RETURN_ERROR_NULL vs lv_stream_context.c:31 静默返回 NULL vs proof_get_stream_context 无处理直接返回；**N9 测试双入口混用（P1，随 K11）**：test_engine_ops.c 用 lv_engine_create() vs test_engine_ext.c/test_basic.c/test_interop.c 用 engine_create() 内部符号直用 | **"一需求多获取方式"集中**：StreamContext 模块内双轨+全库三形态+宏双份（N1/N2/N3 最有价值，与 K15 已登记的"容器未收敛"互补——建议独立立项承接 K15 宏收敛决策的执行层规范）；**P0 2 项**：N5 幻影 bind API 修正（补 engine_bind_context/engine_get_context 或删除注释文档化"不绑定仅借图"契约，orchestrator:208 字段直写加注释或转 API）、N3 宏重复定义收敛（锚定 stream.h 单头单宏族，删 stream_context_util.h 重复定义与 lv_DECLARE_STREAM_CTX，手写 5 处迁移）；**P1 5 项**：N1/N2 StreamContext 获取规范（模块内部统一 xxx_get_stream_context() 补齐 11+ 缺失 getter，跨模块统一引擎 getter，interop 导出侧补 engine 参数消除双轨，lv_context_get_stream 懒创建与引擎 stream_ctx 语义合并决策）、N4 getter 命名族统一（承接 K15 产出命名规范表 lv_<module>_current() 或 lv_get_global_<name>() 二选一）、N7 注册表获取层规范（进程级→global getter 实例级→实例 getter 禁止新增承载并入 L3/H1）、N6 g_tls_engine 登记 legacy 黑名单（J5/K6 退役机制承接）、N9 测试入口统一（随 K11）；**P2 2 项**：N8 失败处理约定登记（可选增强依赖静默 NULL 豁免命令语义路径必须报错，并入 K1 失败约定契约表）、引擎获取 4 模式场景登记表（纳入 K7 降级文档）；**分层保留**：分配器/线程池/lv_error/geometry_config 单事实源与范本形态、lv_registry 统一设施（数据结构层）、context 资源操作回调注入、instance 级 getter 族、interop persistent_engine 缓存策略、配置 A/B 分发机制、模块生命周期注册表（J1 已决策） |

### 1.90 算法复杂度/性能标注面（第十八轮，K50）

| # | 需求 | 现状格式 | 审计结论 |
|---|---|---|---|
| K50 | 算法复杂度/性能标注 | **权威已建（健康基线）**：哈希 O(1)+回退线性诚实标注已成项目惯例（graph/registry/solver_system/func_block_registry/dsl_compiler 均自述回退语义 ✅）+ solver_core.c:588 CDCL 线性 BCP 自述 + mv_polynomial 插入排序自述 + geometry_csg_hull 暴力（顶点<200 自述）+ rewrite_snapshot 线性搜索自述 + fast_index/lv_registry/graph 哈希+回退线性（K7 范畴复核一致）+ constraint_graph.h:394/fast_index.c 诚实条件标注为范本；**"一需求多实现"新点**：**graph_memory.c 声称 O(n log n)/qsort 实际纯 O(n²)（P1 M5）**：graph_memory.c:148/:176-177/:197 声称 "O(n log n) instead of O(n²)"+"simple insertion sort for small arrays, qsort for large"但 lv_insertion_sort（lv_utils_array.c:72-100）是**纯 O(n²) 插入排序、无 qsort 分支**——lv_insertion_sort 共 7 调用点（graph_memory/expr_canon/type_system/mv_polynomial/rewrite_strategy/proof_rule_engine）；**复杂度词汇表三套并存（P1）**：PresetComplexity 枚举（func_block_preset.h:158-166）+ func_block_preset_query.c:257-265 表（"O(1) - 常数时间"）+ lv_impl_upper_preset.c:170-177 表（"O(1)"格式不同）+ **JSON 序列化双格式**（lv_impl_upper_preset.c:423 输出 %d 数字 vs preset_manager_serialize.c:112 输出字符串）；**InternalPresetEntry.complexity 只写不读（P1 M6）**：preset_blocks.c:184/:572 写字段但 preset_blocks_get_metadata 返回的 PresetBlockMetadata 无复杂度字段 + **preset_blocks.h:376-698 30+ 处宏注释"复杂度：O(1)/O(n)..."无任何可执行对应**；**dsl_lexer.c 二分声称 vs 线性实现（P2 M5）**：:147/:154/:254 声称二分/O(log N) 但 :258-264 实为线性 for 循环（:258 注释自认线性同函数注释自相矛盾）；**bdd_encoding.c:177 注释过期（P2）**：声称"线性扫描实现，完整版应使用哈希表"但 bdd_unique_lookup（:53-108）已是开放寻址哈希；**PERFORMANCE_OPTIMIZATION.md:109-115 幻影结构复杂度表（P2）**：声称 lvBloomFilter O(k)/lvSkipList O(log n)/lvRTree O(log n)/lvHashTable API 复杂度但全库无这些结构（lvLRUCache 已登记 G2 其余新增）；37_parsing_layer.md:28 "O(1) 遍历"表述错误（链表遍历 O(n)） | **性能标注面整体健康度中等**（绝大多数声称-实现匹配，诚实回退标注已成惯例）；**P1 3 项**：复杂度词汇表统一（PresetComplexity 单源+JSON 数字/字符串格式二选一）、graph_memory M5 修正（实现 qsort 分支或改注释为 O(n²)）、InternalPresetEntry.complexity 死字段接线或删（30+ 宏注释无机制）；**P2 3 项**：dsl_lexer 注释修正、bdd_encoding 注释修正、幻影文档清理（PERFORMANCE_OPTIMIZATION.md 删幻影结构表+37_parsing_layer.md 改 O(n)）+ 起草"复杂度注释惯例"（以 constraint_graph.h:394/fast_index.c 诚实条件标注为范本）；**复核一致**：G2 expansion_cache 线性扫、K7 索引→线性扫描 ~10 处、K8 基准/阈值、I2 计时/Welford、K45 graph getter 契约、K3 dsl_compiler 流水线 M5、K2 normalization 手写插入排序×2——不重复报告 |

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

### 3.11 引擎生命周期面（第三轮，L1-L2）

**决策**：
- **L1 状态机二合一**：`lvContextState` 与 `EngineState` 合并为**单一问题生命周期状态机**（IDLE→PARSING→REASONING→COMPLETE/ERROR + 转移表 + 名称表 + 合法性判定共享一份实现）；差异化部分（错误枚举返回类型、熔断强转）参数化为配置而非复制骨架。删除复制粘贴的"豁免注释"。
- **L2 进度模型收敛**：主流程锚定**模型 C**（lvStageStatus + session 编排，真实路径）；模型 B（EngineState）若合并后仍零生产调用则**删除**；模型 A（lvContextState）保留为 context 自身状态（数据容器状态），与编排进度解耦。

### 3.12 推理后端面（第三轮，L3）

**决策**：
- **统一推理后端注册表**：`lvBackendPluginRegistry` 为唯一事实源（补 ops vtable 绑定），删除 `SMTBackendRegistry`/`ATPBackendRegistry` 手工枚举与 `kSchedulerBackendVTables` 硬编码静态表；GROEBNER 单实例派生（新增后端只改一处）。
- **求解主链收敛**：`solve_algebraic_system()`（唯一被多模块复用的真实求解器）为权威，调用方统走调度器；3 处壳（engine_solve/engine_scheduler/func_block_determinism）收敛。
- **桩后端明确标注**：SMT（Z3/cvc5/Singular）、ATP（Vampire/E/iProver）、BDD、approx_counter、probabilistic 标"框架/桩，未接线"，不删但不得声称可用。
- **保留分层**：rewrite（规范化）/unify（证明检查）/证明策略（12 策略）/模型计数/数值后端/Newton 交互求解是**不同推理策略**，不合并。

### 3.13 流/交互协议面（第三轮，L4）

**决策**：
- **事件→JSON 契约单一化**：以 C `stream_json.c`（13 字段，x-macro 权威）为唯一事件序列化契约；删除 `module/stream_bridge/stream_bridge.py` 的 16 字段漂移副本与 `_EVENT_META`（40 项漂移）；`lv/stream_bridge.py` 改为运行时调 C `stream_event_type_*`（已同步，保留）。
- **`stream.event` JSON-RPC 序列化器 3→1**：以 C `stream_event_to_jsonrpc` 为权威，Python ws_server 复用同一格式（不再各自实现）。
- **WS 双栈收敛**：C 自实现 RFC6455（interop_server_ws）与 Python websockets 二选一为**网络层标准**（建议 Python websockets 服务端 + C 进程内出口），C WS 降级或删除；SSE 保留为可选传输。
- **协议词汇表分离明确**：interop 21 命令（AddNode/ExportGraph/StreamStart…）为 L5 引擎协议；L9 五命令（Load/Verify/Batch/Export/Visualize）为应用层编排方言——**文档标注两者不同**，不合并但不得混用。
- **文档同步**：`31_stream_interop.md` 的 StreamEvent 结构更新为与代码一致（现写 proof_step_id/json_payload，代码实为 var_id/detail_json/graph_json/merge_pairs）。

### 3.14 内存/日志面（第三轮，L5-L8）

**决策**：
- **L5 调试分配器二合一**：删除 `lv_malloc_tracked/lv_calloc_tracked`（内联重写版，零调用点），统一走 `AllocatorOps` vtable（allocator.c debug_alloc 为权威）。
- **L6 内存统计二选一**：锚定 `MemoryStats`（lv_utils.h 扁平全局，实际使用）；`lvMemoryStats`（memory_pool.h 按类型统计，生产零调用）删除或真正接线；README 引用的 `lv_get_memory_stats_ex` 补齐实现或删引用。
- **L7 泄漏检测归一**：`lv_cleanup` 接 `lv_memory_leak_report`（精确未释放块遍历，最可靠工具）；文档更新为与实际实现一致。
- **L8 日志级别单一词汇表**：锚定 `lvLogLevel`（lv_log.h，值越小越详细）；`lv_LOG_LEVEL_*`（lv_internal.h 方向相反）改名或废弃；`LogLevel`（debug.h）收敛为别名；删除魔法数字规避（lv_log.h:87-92 注释自认）；runtime_monitor 带 tag 宏与主入口打通。

### 3.15 前端/文档面（第三轮，L9-L11）

**决策**：
- **L9 前端刻意留白（用户确认，非债务）**：前端占位符/mock（geometryStore
  demo 场景、createMockBridge 硬编码数据、M2/M6 空桩）是**有意为之**——
  用户明确"等后端内核彻底定型后再写前端"。因此：
  - 前端接真实内核**不作为清理批次**，不列入 P0-P4；
  - `proof_widget.c` 的 C API 数据契约（孤岛）保留为**未来前端的对接契约**，
    内核定型时按此契约实现；
  - `protocol/index.ts` KernelBridge 接口保留（已是设计好的协议面），
    createMockBridge 保留为开发模式；
  - 记录为"L9 状态：刻意占位，待内核定型后实施"，移出统一化执行清单。
- **L10 文档收敛**：同主题多文档合并（证明 5 篇→1 权威 + 指向、求解器 5 篇→1、几何 6 篇→1、数值 7 篇→1）；删 `stable_release_gap_analysis.md`（自述重复）；修 INDEX.md 死链接；统一"当前版本"事实源（v1.1.0 十层为权威，其他版本号引用纠正）。
- **L11 语言语法单一事实源**：`lv_LANGUAGE_SPEC.md` 为唯一权威语言规范；根 `docs/architecture/` DSL 三篇改为"设计文档"（引用而非定义语法）；`bootstrap/` .lv 语义规格与规范对齐；导出格式枚举双套（InteropExportFormat vs lvExportFormat）合并为单一枚举。

### 3.16 公式/表达式面（第四轮，F1-F3）

**决策**：
- **F1 double 表达式树二合一**：以能力更全的 `lvSymExpr` 为基底，`lvADExpr` 的前向/反向数值求导作为其上一对算子（ad_forward/ad_reverse）；或保留其一删除另一（两者均零生产调用，收敛成本低）。推荐前者（符号求导 + 数值求导互补）。
- **F2 规范形二选一（禁止半吊子）**：① 兑现文档——补 FormulaNode→lvExpr 与 lvExpr→lvExprCanonical 生产桥接，让 lvExprCanonical 真正成为代数等价判定基；或 ② 删除孤儿——短期无消费则移除或明确标"待接入"。**现状两者都不是（文档声称流水线无代码）**，需先决策。
- **F3 字符串化收敛**：`node_to_string` 并入 `formula_renderer` 共享骨架（作为第 7 后端或单行后端），删除独立 walker。

### 3.17 导入解析面（第四轮，F4）

**决策**：
- **共享坐标转换**：`import_coord_from_double(value, denom)` 单一函数（round(value×denom)+create_rational），两个精度宏降为调用参数（`INTEROP_COORD_DENOM_PRECISION` vs `_GEOJSON`）；ggb_double_to_rational 变薄包装或删除；json 内联与命令面内联统一改调。
- **共享图构建**：`import_add_point_node(g,x,y,denom)` + `import_add_polyline(g,pts,n,close,dx,dy,denom)` 收敛"单点节点"与"点序折线"三份实现（svg_import_samples 与 ggb_import_point_sequence 几乎逐行复制）。
- **共享属性提取**：XML 开标签属性提取拷贝版/切片版合一（保留切片版免拷贝）。
- **格式解析器保留**：GGB XML / GeoJSON / SVG path 语法各不同，各自解析器保留，只输出点序列/元素语义，不承担图构建。

### 3.18 类型系统面（第四轮，F5）

**决策**：
- **几何概念枚举四合一**：以 `GeomType`（运行时节点权威）为唯一来源，`TypeKind`/`LvSemanticType`/`LvEntityType`/`NODE_GEOM_*` 收敛为映射/别名（经 X-macro 派生，禁独立枚举）。
- **LvEntityType 与 LvSemanticType 合并**：同一概念表两份枚举（后者=前者+UNKNOWN/ERROR）→ 单一枚举 + 错误态。
- **类型附加双通道收敛**：Port 内联 type_region vs 外部 NodeTypeMapping 二选一（文档承诺外部映射表，则移除 Port 内联字段或同步文档）。
- **分层保留**：.lv 语言类型检查（L1 sema，表面语法）≠ 运行时类型论（L4 TypeSystem，Martin-Löf 依赖类型）——不同需求各自保留；Python type_system.py 是 #1 的 API 门面（非独立实现）；lvNumberType（数值域）独立。

### 3.19 基础工具面（第四轮，F6-F7）

**决策**：
- **F6 数字解析收敛**：25 处/14 文件语义等价残留收敛到 `lv_parse_*`/`lv_str_scan_number`（多数需 end 指针推进游标，用 lv_str_scan_number + lv_parse_* 组合）；1 处 atoi（test_serialize_registry.c:43）**首选替换**为 lv_parse_int。
- **F7 预设库容器三合一**：preset_blocks/preset_manager/func_block_preset_internal 的并行"命名预设库"容器统一到 `lv_registry` 单一容器。
- **已收敛面确认**：字符串（0 裸 strcpy/sprintf）、动态数组（仅 1 处 snprintf 残留 + 13 处特殊豁免）、哈希表（仅 BDD interner 合法豁免）——**不需要动作**，记录为健康基线。

### 3.20 证明内部面（第四轮，F8-F10）

**决策**：
- **F8 验证入口收敛**：6 个验证入口统一到单一验证 API（建议以 `lvProofObject` 为唯一证明 IR + 单一 verify 入口，各入口降级为适配器）；结果枚举合并（VerifyResult/lvVerifyResult/LvProofVerifyResult/MiniVerifyResult → 单一）。
- **F9 策略调度归一**：多策略引擎（ProofMultiStrategy，JGEX 12+10）与经典引擎（lvProofEngine，10 策略）收敛为**单一策略注册表**（策略注册 + 调度表），两栈保留策略实现但共用调度面。
- **F10 引擎栈明确分层**：4 套栈按职责定界——ProofNavigator 工作流（用户交互）+ 策略引擎（推理调度）+ 逻辑内核（TCB 验证）+ 导出（已在前几轮收敛）；`kernel/` 目录清理（ecosystem.c/gc_language.c 移出，与逻辑内核无关）。

### 3.21 安全/限制面（第五轮，G1）

**决策**：
- **熔断器单一写入口**：context.c 内联改字段的 `lv_context_record_step/record_error/check_timeout` 收敛为唯一 API（lv_circuit_breaker.c），删除第三套写入口；context 转发层保留为薄适配。
- **深度限制单一词汇表**：6+ 套深度常量收敛为**单一"限制栈"**（解析期/推理期/递归期各一层，参数化默认值单一来源）；"递归 128"3 处合一；超时 30000 3 处合一。
- **解析安全接线**：parser_safety.c 的 4 个未接线函数（输入长度/净化/AST 深度/节点数/token 长度）接到真实解析入口；formula_dsl_lex 内联重复版改调权威函数；删 `lv_ERROR_PARSER_POOL_EXHAUSTED` 死错误码或接线。
- **步数/迭代上限收敛**：10+ 处逐模块硬编码步数上限（solver_max_iterations/buchberger_max_steps 等）收敛到 config.h 单一字段 + 统一查询。

### 3.22 缓存面（第五轮，G2）

**决策**：
- **通用缓存层**：建立单一缓存抽象（create(key_fn, value_fn, capacity, evict)），底层用 lvHashtable + 容量驱逐/LRU；5 套推理缓存（prop_verifier_memo/smt_trigger_engine/axiom_pkg_expand/规格 lvReasoningCache/旗标 proof_engine）收敛到该层。
- **键类型各异保留**：goal^hash / quantifier 对 / (name,param_hash) 是不同对象键，缓存层参数化吸收。
- **规格-实现对齐**：文档引用的 reasoning_cache.h/cache_manager.c/lvLRUCache（源码不存在）删除或实现；proof_engine.enable_cache 旗标要么接线要么移除（现为"声称开启但不缓存"的 M5）。
- **对象惰性缓存**：4+ 处对象派生值缓存（symbolic_coord/quantifier/groebner/render）统一失效约定（cache_valid 旗标模式），不强行合并容器（对象不同，容量 1）。

### 3.23 图算法面（第五轮，G3）

**决策**：
- **约束图 BFS/Kahn 收敛**：`bfs_traverse_from` 成为 `lv_bfs_run` 的薄适配（约束邻居回调）；`lv_graph_topological_sort` 重建于 `lv_topo_run`（successors 回调）；环检测已由 lv_graph_has_cycle 外包保持。
- **治理基线健康确认**：其余图域（模块依赖/类型/BDD/证明树）多为已评估豁免或已收敛——**不需要动作**，记录为健康基线。
- **命名澄清**：`graph_topological_sort_stable` 改名或文档澄清（实为确定性排序非拓扑序），消除命名混淆。

### 3.24 事件回调面（第五轮，G4）

**决策**：
- **插件广播收敛**：`lv_plugin_broadcast_event` 改委托 `lvCallbackList`（或插件订阅 lvEventBus），删除手写 for 循环广播（lv_event_bus.h:61-64 自认未迁移）。
- **Python 显式豁免**：`_event_handlers` 跨语言边界（Python 函数无法存 C 基座），显式豁免 + 登记。
- **基座确认**：lv_callback_list 为唯一多订阅者回调基座（StreamContext/lvEventBus/setter 已内嵌）——记录为健康基线。

### 3.25 配置/全局面（第五轮，G5）

**决策**：
- **配置单一注册表**：lvConfig（A，JSON）为唯一权威；ConfigManager（B，INI）删除或彻底降级（LV_CFG_* 键并入 A）；JSON/INI 两套持久化格式合一。
- **默认值单一来源**：lvSessionConfig 默认（timeout 5000/depth 8）改为**读全局配置**（30000/100），消除"会话覆盖全局"的矛盾；所有 timeout_ms/depth 默认值从单一事实源派生。
- **global_state.c 死代码删除**：lv_global_state_* 闭环死代码（initialized 恒 false）删除或接线。
- **分层保留**：编译期尺寸宏（lv_CONFIG_*/lv_EPSILON_*）vs 运行时 vs 会话级是合理分层，不强行合并。

### 3.26 插件系统面（第六轮，H1）

**决策**：
- **求解后端注册单一化**：以 `lvBackendPluginRegistry` 为唯一后端注册表（补 ops vtable 绑定），删 `SMTBackendRegistry`/`ATPBackendRegistry` 手工枚举与 `kSchedulerBackendVTables` 静态表；`EngineScheduler` 分发改查注册表（承接 L3 推理注册表决策）；GROEBNER 单实例派生。
- **插件概念命名收敛**：解决 `interop.h` 与 `plugin_system.h` 的 `lvPlugin` 高危同名冲突（互操作插件改名 `lvInteropPlugin`，删除 typedef 别名）。
- **lvPluginSystem 接线决策**：通用 C 插件系统（10+ 文件）未接入运行时——若未来需要动态库插件则接线，否则标记"预留，未启用"（不得半吊子）；ecosystem 文档失同步修正（527 行声称 vs 20 行实际）。
- **分层保留**：动态库 C 插件 / 后端描述符 / 证明互操作插件 / 生态模块是 4 种不同对象模型，可共用注册设施但语义不合并。

### 3.27 快照/回滚面（第六轮，H2）

**决策**：
- **graph_snapshot 收敛到 graph_copy**：`rewrite_snapshot.c` 的 `graph_snapshot_create` 改调 `graph_copy`（消除残存重复，graph_copy 注释已宣称收敛但漏了它）；bit_burning JSON 快照保留为跨场景后备（序列化格式不同需求）。
- **快照分层模型**：上下文级（lv_context_snapshot）/ 推理帧级 / 求解器级（坐标子集）/ 图级事务 / 内存级（arena mark）粒度不同，**保留分层**并文档化。
- **protocol undo/redo 空壳修复**：M5——命令名暴露但 undo_depth 硬编码 0 无 handler → 接线或撤掉命令名。
- **撤销栈不强并**：algebra 成对 undo+redo / path_explorer / lambda / cdcl trail 语义不同（几何步骤/类型区域/局部连接/SAT trail），硬统一为通用 undo 框架触发负面清单，**不建议合并**。

### 3.28 数值线性代数面（第六轮，H3）

**决策**：
- **稠密 LU 三合一**：`gauss_eliminate`（生产）与 `dense_lu_solve`（sparse 兜底）指向 `host_linalg.h` 的 `host_lu_factor/solve`；`host_linalg.c` 下沉或做成公共头（消除"为避免跨层依赖而复制"的动机）。
- **稀疏直接法入标准接口**：`sparse_lu/cholesky/qr_solve` 声明进 `sparse_linear_algebra.h`，与 `lv_sparse_solve`（Jacobi 迭代）统一返回码契约；numerical_backend CSR 分支按 `lv_LINSOL_DIRECT_SPARSE`/`lv_LINSOL_ITERATIVE_*` 分发。
- **后端分发打通**：生产几何约束求解从 gauss_eliminate 切换到 `lv_linsol_create()`/`lv_matrix_create()`（numerical_backend 注册表成为唯一分发点）——本轮最核心打通动作。
- **补齐或声明不支持**：CSC/BANDED 格式、CUDA/HIP 的 CG、`lv_BACKEND_SINGULAR` 数值操作表——实现或显式返回 `lv_BACKEND_UNSUPPORTED`。
- **保留分层**：直接法（LU/Cholesky/QR）vs 迭代法（GMRES/BiCGSTAB/CG）是不同数值策略，保留在 `lvLinearSolverMethod` 枚举下分层。

### 3.29 形式化对齐面（第六轮，H4）

**决策**：
- **公理单一事实源**：以 `module/axiom_packages/euclidean_plane.lvz`（运行期真实数据源）为唯一公理定义；formal/.lean 与 lv-formal/.lean 改为**引用/生成**而非独立维护（先消 formal/lv 与 lv-formal/Classical/Hilbert 的逐字相同重复——两目录选一为权威）。
- **风格统一决策**：euclidean 包 Hilbert vs Tarski 两套公理化——选一为标准（建议 Hilbert，.lvz 已是）并统一 manifest.json/README。
- **对齐机制**：CI 增加"规格↔实现"交叉验证（formal 类型检查结果与 C 测试结果对拍；至少加 .lvz 模板名 ↔ Lean 实例映射一致性检查）。
- **占位公理修正**：Continuity.lean 空公理（∃ n, True）与 .lvz 真实签名对齐（实现或显式标"待证明"）。
- **命名错位修正**：bootstrap/output/coq_proof.lean 改名或删除（承接 P4 决策）。

### 3.30 Python 测试结构面（第六轮，H5）

**决策**：
- **测试基座单一化**：test_streaming_e2e.py 从 unittest 迁到 pytest（pytest-asyncio/pytest-mock，CI 已装）；删 test_runner.py（pytest 收集 0 用例的冗余基座）；Python 侧单一测试基座 = pytest。
- **常量对拍单一来源**：首选构建期从 C 头生成 Python 常量 + 基准表（解析 typedef enum/LV_STREAM_EVENT_X 宏）；暂不做 codegen 则收敛为单一共享数据文件（lv/_c_enum_snapshot.json）共同 import。
- **补流事件对拍**：把 stream 事件枚举纳入对拍（现缺口，C 宏插入事件 Python 魔术值静默错位）。
- **钉死后端**：C 绑定测试显式断言加载到 C 绑定否则 skip（与 test_constants_consistency 一致）；fallback 测试显式 import lv.fallback——消除"有库测 C 无库测 fallback"的不确定性。
- **消除构建产物冗余**：setup.py 用 find_packages(exclude=['tests*']) 排除测试打包。

### 3.31 进程/网络面（第七轮，I1）

**决策**：
- **graph_dot 收编底座**：`graph_dot_export.c:292` 裸 `system()` 改走 `lv_external_process_run`（加超时 + stdout 捕获），消除唯一绕底座点。
- **atp/smt 执行骨架共享**：只共享"argv 构造→超时→执行→退出码→降级"骨架（lvExternalRunner 或共享辅助），外部协议解析/降级留各适配器。
- **Python 跨语言豁免登记**：concurrent_monitor 的 terminate→wait→kill 跨语言独立实现显式豁免登记。
- **健康确认**：Coq/Lean bridge 纯文本互操作、lv_dlopen+_ctypes_binding FFI 孪生非重复；WS 双栈与事件序列化 3 套已在 L4 登记（承接）。

### 3.32 性能监控面（第七轮，I2）

**决策**：
- **计时基座单一化**：`lv_get_time_ns/us` 为唯一单调计时源；`lv_clock_elapsed_*`（clock()）保留为 CPU 时间专用并显式标注；6 套起止计时收敛为 1 个计时原语（begin/end + 自动累积）；4 个薄包装改宏或直接调用；测试代码改用基座。
- **统计采集分层**：L0 唯一在线聚合器 lvPerfStats（移除 benchmark 第二份 Welford）；L1 领域对象统计（×12）从 L0 原语构建；L2 统一上报（JSON 走 lvJsonBuf、文本走 lvStrBuf），lv_diagnostics_generate 作为汇总入口填充恒 0 字段。
- **求解统计收敛**：以 SchedulerStats 为唯一求解计时统计；删/修 PerformanceCounters 死代码；solver_stats.c 更名或补注释（实为 DOF 计算）。
- **事件追踪收敛**：保留 lv_event_trace（Chrome trace 导出），合并/移除 debug_trace 的 trace_session。
- **日志收敛**：runtime_monitor 日志子系统仅留级别映射，删独立 log_file/rotation/callback 状态机，全量委托 debug 管道。

### 3.33 数据结构面（第七轮，I3）

**决策**：
- **增长逻辑单一路由（最高价值）**：① lv_ENSURE_ARRAY_CAP 宏改委托 lv_ensure_capacity（12 处机械替换）；② lv_ensure_capacity 增 size_t 形态（或 size_t 包装），让 lv_dstr_grow/lv_strbuf_grow/array_grow_to_fit 路由过去；③ lv_strbuf_grow 改用 lv_realloc 语义（消灭"新分配+拷贝"第三条路径）；④ 3 处漏网（interop_theorem/lambda_to_graph/lv_utils_config）迁移补齐。
- **IntArray 废弃/包装**：生产调用点 ≈0 → 废弃或改为 lvDArray 上的 int 便捷包装，消灭 array_grow_to_fit 整套增长。
- **FIFO 队列族收敛**：建立权威可扩容 FIFO（lv_ringbuf 语义 + lv_ensure_capacity），优先收敛纯数组形态（BFSQueue/propagation/type_equiv/stream_lazy）；链表形态（thread_pool/proof_rule_engine）与 work-stealing（groebner_parallel）逐点判定（线程/上限语义）。
- **HashHistory 评估**：改建在 lv_ringbuf 之上（uint64 特化+预筛为参数差异）或文档标注特化容器。
- **内存池二选一**：lvObjectPool vs lvMemPool 二选一或互相委托；lvArena 保留为分配器家族分层。
- **保留**：lvHeap/lvHashtable 三形态/lvCallbackList/lvArena/thread_pool/lvReasoningStack（不同语义）；栈族不新建泛型 Stack（与遍历算法强耦合，维持"仅统一扩容"基线）。

### 3.34 IO/序列化面（第七轮，I4）

**决策**：
- **round-trip 基座单一化**：`lv_roundtrip_verify`（mem:// 完整往返 + 注册表分派）为唯一权威；`meta_repr_verify_roundtrip` 无调用方 → 删除或降为内部静态（顺带修复 memcmp(…,sizeof(void*)) 兜底）；`test_oracle_verify_serialize_roundtrip` 无调用方 → 删除或改薄包装委托权威；图等价比较收敛到注册表分派（graph_isomorphism_* 仅保留为独立算法）。
- **文件 IO 收敛**：lv_storage file:// 后端 open/close 改走 lv_file_open/lv_file_close（模式转换保留）；read/write/seek 可保留 fread/fwrite/fseek 但错误处理收敛或给 lv_file 补流式接口；file_size 复用 lv_file_size；无豁免标注的裸 fopen（proof_navigator_export/proof_version_nl/sat_encoding）切 lv_file；8 处裸 fclose → lv_file_close；豁免登记 lv_export_common（文本模式/字节数语义）+ 日志调试类补显式 exempt 标注。
- **建立黑名单 grep**：裸 fopen(/fclose(（非 lv_file_close）/memcpy 写整数进文件缓冲。
- **格式契约不抽象**：LVZD/PNG/msgpack 长度字段属外部格式契约，只收敛 IO 骨架与验证基座；lv_bytes/lv_json 已是收敛态勿再动。

### 3.35 层间依赖面（第七轮，I5）

**决策（先修机制再清零）**：
- **层验证机制修复（P0）**：lv_LAYER_CAN_DEPEND 依赖表接入 engine.h 宏实现（替换 current>=min 简单模型）；强制"每源文件显式声明允许层，缺省只允许本层"（0 声明 = 编译错误）；layer_validation.h 自动检查在启用时强制 include；TASK_CONTEXT"0 违规"结论修正为"宏启用+断言实际展开"条件。
- **CI 自动依赖矩阵**：按"头→层归属表"扫描每层 .c 的 include 与符号引用，阻断 L1/L2 对高层引用；伞形头 lv.h 排除清单化（L1 编译单元含 lv.h 须经白名单放行）。
- **2 处 P0 修正**：① 推理栈归位 L2（本就从 context.c 提取）或 context.c 经回调/不透明指针解耦；② lv_loader 只产 AST，"装载入图"改由 L0 编排层调 L3/L4，解除 lv_layer1_parser 的 L3/L4 补链。
- **归属修正（P1）**：dsl 词法/语法阶段归 L1（IR→图装载留 L4 或经回调注入，先"解析"与"装载"分层）；module_lvz 词法/语法归 L1；gc_language 迁 L1 或删除（孤儿）；ecosystem 收敛 L2 lv_registry 或删除；module_export 迁 L5 或删除（无消费者）；lvProofObject 实现随类型归位 L4 proof 域；proof 导出双轨收敛（通用导出核心锚定 L5）；lv_serialize_adapters 归属写入层归属表。
- **权威层锚定登记**：解析=L1、序列化=类型层+JSON 唯一 L2、导出/格式化=L5、证明对象=L4（类型+实现）、L1/L2 不得引用 L3+ 头或符号（含经 lv.h 传递）。
- **健康面确认**：L3→L4/L4→L5/L5→L6/L6→L8/L10 直接 include 干净、lv_dot_writer 方向正确——不需要动作。

### 3.36 init/清理生命周期面（第八轮，J1）

**决策**：
- **单一生命周期注册表**：lv_cleanup 硬编码手动序列（10+ 行）全部转模块注册表条目（用"阶段"概念 SHUTDOWN 前/中/后）；lv_cleanup 收敛为状态机 + cleanup_all + 内存统计；新子系统一律注册表，禁止往 lv_cleanup 加硬编码行（黑名单）。
- **注册表补复位语义**：新增 `lv_module_registry_reset()`（cleanup_all 后清 count/entries）；lv_init 检查 lv_module_register 返回值（或幂等 upsert）；修复"cleanup 后 count 恒 9 / 重复注册被吞"。
- **once_reset 统一契约**：thread_pool g_pool_once、module registry once、config、ecosystem 未重置点逐个处理；"cleanup 必须重置自身 once 守卫否则不得声明可重入"写入 lv_thread.h/注册表头契约。
- **8 处 M6 清理修复**：LV_REGISTER_MODULE 删除或接线；runtime_monitor log/event_trace 二选一（接注册表或正式标注测试专用）；lv_ecosystem_init 接入 lv_init（或删 cleanup 调用）；lv_config_reset 接入 lv_cleanup；lv_application_shutdown 删除或实现。
- **失败路径回滚**：init_all 失败逆序回滚已 init 条目 + 状态机允许 ERROR 重入。
- **双日志收敛**：以生产生效的 debug 管道为唯一语义，runtime_monitor lv_log_* 降级或删除（承接 L8 日志级别决策）。

### 3.37 线程/并发面（第八轮，J2）

**决策**：
- **锁抽象单一化（P0）**：lv_thread.h 为唯一锁基座；删 runtime_guard.h 自有 lvMutex typedef + lvRwLock（从未启用休眠层；若有读写锁需求移植为 lv_thread.h 统一抽象）；清 context.h void* 死锁字段。
- **lvMutex 旧别名退役**：6 处使用点（lv_arena/lv_registry/lv_backend_plugin/preset_manager/proof_navigator/runtime_guard）迁移 lv_mutex_t；旧别名黑名单。
- **惰性锁收敛（P1）**：9 处手写 once+mutex+ensure 迁移 lv_lazy_lock（黑名单 grep：`_once = lv_ONCE_INIT` + 同文件 `_mutex` + init_once 手写回调）；手写 _initialized 标志随迁移删除。
- **原子统一（P1）**：lv_ATOMIC_* 补 64 位 LOAD/STORE/CAS/EXCHANGE（当前缺口是 simd_ops/geo_predicate 退回裸 stdatomic 的直接原因）；迁移无豁免标注裸 stdatomic；统一 lv_ATOMIC_CAS_BOOL 跨平台语义（Windows 不更新期望值 vs POSIX 更新，graph_node_alloc 手动规避）；debug_refcount 互斥模拟迁移。
- **并行骨架评估（P2）**：groebner_parallel/proof_version_task 复用 lv_parallel_for/lvWaitGroup 或显式登记豁免（有缺陷回退背景，先保证共享状态同步设计）。
- **平台分支收敛**：只允许 lv_thread.h/lv_platform.h/cross_platform.h 三权威点（已收敛 ✅）；黑名单 core/src 内 pthread_*/CRITICAL_SECTION/Interlocked*/SRWLock 直接调用（lv_process 进程等待豁免）。

### 3.38 构建产物/CI 面（第八轮，J3）

**决策**：
- **产物四类分策**：可再生成 → 移出 git（bootstrap/output 15 文件，补再生成脚本指向 lv_impl_upper_app EXPORT/VISUALIZE）；golden → tests/fixtures；测试中间物 → 忽略（test_output.tex 等）；日志 → 忽略。
- **CI 报告收敛**：8 处内联 grep+awk 抽 1 脚本/composite action（ci.yml + python.yml 共享）。
- **docx 生成收敛**：4 个生成器（3 JS + 1 py）收敛到 1 个（建议保留 ui 已链的 generate_report.js 或 gen_report.py 二选一）。
- **死配置清理**：TestLake 嵌套 workflow 删除或上移（GitHub 不执行）；web-deploy.yml if:false 删除或启用。
- **P4 命名修正**：bootstrap/output/coq_proof.lean 改名（承接）；Lean 导出×3/TikZ×2/JSON×6 产物级重复随移出 git 一并消解。

### 3.39 Python 包结构面（第八轮，J4）

**决策**：
- **顶层导出单一化**：删 `from lv._ctypes_binding import *` 星号导入；__init__.py 定义显式 __all__；fallback 别名与 C 路径合并为单一 if/else 选择点。
- **预设单一事实源**：锚定 .lvz（先修 convert_presets.py 编码乱码再重生成）→ Python 仅保留 preset_blocks（v4.0.0）单一注册表；v3.3.0 系降 compat re-export（合并进 preset_func_blocks_compat）；math_presets 标注第三方 spec 或迁移；删 preset_registry.yaml；C 侧 g_builtin_* 与 .lvz 二选一或明确"无 module/ 目录回退"。
- **几何操作入口收敛**：core.Point.mid_point 为唯一数学实现；preset create_* 两代复制体改为委托 core（删公式复制）。
- **删除项**：lv.utils（346 行孤儿）、build/lib/（38 文件过期镜像）、setup.py 或 pyproject.toml 二选一（承接 C1）。
- **fallback 边界**：补"仅核心子集"文档；20+ 子模块 `from ._ctypes_binding import _lib` 统一守卫逐模块降级或文档明确仅顶层可用。
- **健康确认**：绑定层/核心层/域模块/兼容 re-export 分层合理；test_constants_consistency 探针守护合理。

### 3.40 全局符号/命名面（第八轮，J5）

**决策**：
- **命名规范锚定**：函数 `lv_<模块>_<动词>_<对象>`（统一前缀二选一，禁模块裸前缀直出）；类型 `lv<Module><Name>`（消灭 435 裸 typedef）；生命周期锚定 create/destroy 一对（free 仅留 libc malloc 配套、delete/release 全收敛）；解析锚定 parse（整串）/scan（前缀扫描）两语义；枚举常量统一 `LV_<域>_<名>`。
- **冲突修复（P0/P1）**：lvPlugin 二选一改名删别名（建议 interop.h 删 typedef，全量迁移）；REL_FORMULA/RelFormulaType 语义分裂分别改名（sat_encoding 侧 RelExprType 或 relation_model 侧）；守卫式重复枚举单源化（func_block_registry.h 为唯一权威）；expr_canon.h/expr_canonical.h 文件名与内容对齐。
- **裸 extern 清理（P2）**：10 个裸全局加 lv_ 前缀（_stream_ctx 同时去前导下划线）。
- **命名 lint 强制**：clang-tidy readability-identifier-naming 或脚本扫 core/include：导出函数 ^lv_、typedef ^lv[A-Z]、枚举常量 ^LV_、extern ^lv_、禁止 ^_[a-z]；接入 CI/pre-commit。
- **风险**：前缀补齐属 API 破坏性改动（lv_ 仅覆盖 45% 导出函数），分级推进——P0/P1 编译级冲突 → P2 前缀补齐（版本化迁移）→ P3 上 lint。

### 3.41 错误消息文案面（第九轮，K1）

**决策**：
- **保留 LV_ERROR_CODES_X 为唯一码→默认文本表**（已单一事实源 ✅），其上追加**场景文案规范表**：为通用场景（INVALID_PARAM/NULL/OOM/NOT_FOUND/NOT_INITIALIZED/越界/超时/状态转移非法）各定 1 句标准文案 + 参数槽；lv_RETURN_ERROR 调用点禁止自创新措辞（复用标准短语或仅追加函数名前缀）。
- **语言策略单点决策并固化**：建议"用户可见默认文本=中文（维持现状）、内部日志/技术细节=英文"；lv_ERR_MSG_OOM 与表内 OOM 文案二选一；全库 OOM/分配失败字面量收敛统一短语。
- **格式规范**（写 doc + 黑名单 grep）：统一半角冒号、`函数名: ` 前缀、无句末标点、码值统一呈现之一。
- **状态名表收敛**：context/engine/circuit_breaker/proof_trace 等 ≥6 处显示名表并入共享枚举名设施（lv_enum_to_str 已存在，共享键表 + 统一 "EN（中文）" 显示格式）——D4 映射重复，不受 context 转移矩阵豁免影响。
- **Python 文案收敛**：新建 lv/_messages.py 收敛重复文案（"三点共线"×9 等）；engine.py 内联 dict 改调 lv_error_string。
- **回归约束**：改文案须同步更新钉死文案的测试（test_context 断言"超时"等）。

### 3.42 基础算法重复面（第九轮，K2）

**决策**：
- **整数快速幂 ×3 → 单设施（P0）**：以 lv_safe_pow 为语义基线提供 mul 回调参数化设施，lv_alg_rational_pow/lv_number_pow 委托；注意 lv_number_pow 负指数缺陷修复以正确语义为准 + 回归。
- **平方因子提取 ×3 → 单设施（P0）**：以 symbolic_coord.c:352（int64，已被 2 文件共享）为权威提升为 lv_arith_safe.h lv_squarefree_i64，quadratic/algebraic static 复制委托；solver_symbolic GMP 版标注豁免。
- **FNV 双家族收敛（P1）**：lv_utils_misc.c lv_hash_string/bytes/int 改薄委托 lv_fnv1a_*（保持 NULL 语义差异）；preset_common lv_hash_int_array 零调用 → 确认死代码后移除或登记；graph_hash/normalization 粗粒度混合新增 lv_fnv1a_mix_u64 设施或标注。
- **排序残留收敛（P1）**：normalization 手写 int 插入排序×2 → lv_insertion_sort；geometry_compress 三态比较器 → lv_cmp_int_asc；并行数组排序/收缩融合冒泡等变体保留+标注。
- **健康确认**：gcd/数学原语已收敛——不需要动作；黑名单"新代码禁用裸三元 min/max、手写 (a>b)-(a<b)、手写 FNV 常量"。

### 3.43 文档代码一致性面（第九轮，K3）

**决策**：
- **4 头 M5 修正（P0，低成本注释修正）**：dsl_compiler.h ".lv 源文件"→"DSL 源码字符串" + 多目标代码生成标注"预留未启用"；solver.h 删"SuiteSparse/GraphBLAS 加速"或改"纯 C CSR 后端（未接线）"；proof_engine_enhanced.h enable_cache 注"预留未接线"或删字段；ecosystem 文档 527 行声称改 20 行实际。
- **README 幻影 API 修正（P0）**：删 4 个不存在函数（lv_get_memory_stats_ex 等）或实现；渲染状态更新（TikZ/SVG/PDF 已完成）；目录/计数更正。
- **文档漂移修正**：31_stream_interop StreamEvent 按实际结构重写；lv_LANGUAGE_SPEC §9 命名对齐或标注设计；opml_codec 头注"仅 JSON 子集"；37_parsing_layer 目录声称修正。
- **声称-实现对齐机制**：头注释声称加机器可读标记 `@impl-real / @impl-pending(owner,date) / @impl-none`，CI 扫描核对符号存在性；统一桩约定 `LV_STUB(owner)/TODO(owner)`（13 处"简化版"逐一定性）；文档-代码单源（版本/事件枚举/API 清单以头为唯一事实源 + 对拍 CI）。
- **layer_validation.h**：@note 升级为"过期参考，实际验证宏在 engine.h 且全库未接线"或删除（承接 I5 层验证修复）。

### 3.44 版本兼容面（第九轮，K4）

**决策**：
- **版本分层 + 单一事实源**：库版本 lv.h 为唯一源（CMake/VERSION/Python 包派生）；格式版本每格式单常量集中格式头（LVZ 删 module.c 未用定义 + 修 module_serialize.c:466 写端引用宏 + PRESET_SYSTEM_VERSION 收敛 preset_common.h）；文档版本统一"当前版本"事实源（清理 3.3.0/3.5.0/5.0.0 残留 + CHANGELOG 双条目 + README 矛盾）；Lean 双项目二选一（建议 lv-formal 为纠偏后路线）。
- **读端校验补齐**：msgpack/JSON 顶层加 format_version + 读端 major 门；OPML 读端解析比较版本；LVZ/LVZD 定义 minor 兼容规则；.proof.cert 实现 L9 信封或明确标注未实现。
- **真实运行时版本校验**：lv_check_version_compat 改比对真实 DLL 版本；Python 绑定加载后调 lv_get_version_string 与包版本比对；实现或删 lv_get_version_info/lvVersionInfo 幻影 API。
- **ABI 治理**：修复 config.h:643 无条件空定义（#ifndef 或删除），恢复 lv_PUBLIC_API 语义；新增 ABI 版本常量 + 跨 DLL 结构体 _Static_assert；Python 13 ctypes 结构体加 sizeof/offsetof 运行时校验。

### 3.45 示例教学代码面（第九轮，K5）

**决策（v1.9.3 定稿：用户"文档走归档、代码先调研再转正" + 调研结论）**：

**调研依据**（子代理调研 SQLite/Redis/libgit2/curl/jq/zlib/ncurses/Lua/musl/
OpenSSL/CPython/GStreamer/Rust 生态）：
- 独立 examples/ 是主流非普适；**共同底线 = "示例必须能被构建校验"**
  （curl make check / libgit2 CI / cargo test / OpenSSL enable-demos）；
- "示例即测试"（Lua testes / ncurses test / zlib test/ / GStreamer
  tests/examples）是 C 生态另一主流，腐化成本最低；
- 教学文档与 API 脱节是普遍现象；对策 = doctest / CI 编译 / 引用式文档
  （代码零复制）/ 分仓纪律；删除先例充分（next.js/pomerium/live_vue）；
- 纯 C 无标准 doctest；Lv-00 可用 Python 文档示例进 pytest（CPython 先例）。

**分项处置（定稿）**：
1. **test/examples/ 8 个 C 示例 → 保留+转正**：已接 CMake 编译 = 已具备业界
   底线；补 3 点——CI 运行冒烟（退出码 0）、README 主题索引、doc 全部引用
   这 8 个文件替代内联代码（引用式文档，代码零复制防脱节）。
2. **根 examples/ Python 假示例（demo.py 等）→ 删除**：违反"示例必须可运行"
   底线（导入不存在模块 = 纯负资产，先例 next.js/pomerium）；
   **唯一例外**：若 38 个 Python 测试背后有可复用 ctypes/cffi 绑定，
   则重写示例对准真实 API 并纳入 pytest——二选一，不许保留现状。
3. **API_QUICKSTART → 转正（重写为引用式）**：代码全改为引用
   test/examples/*.c，删除内联漂移片段，保留为唯一教学入口。
4. **TUTORIAL → 归档**（无维护者；或按 OpenSSL"教程→demos"模式重写）。
5. **USE_CASES → 归档**（19 处脱节 + 与示例重复度高；README 对照表替代）。
6. **归档位置**：doc/docs/archived/（不删除，保留历史）。
7. **最小同步护栏**：CI 脚本对 doc 内联 C 代码块做抽取编译或 API 符号
   交叉检查，防止再次脱节（承接 K3 @impl-* 机制）。

### 3.46 废弃API/兼容层面（第十轮，K6）

**决策**：
- **退役机制统一（三形态）**：① C 公共头旧 API 强制挂 `lv_DEPRECATED(msg)`（宏已定义 0 使用），CI 加 `-Werror=deprecated-declarations` 于内部构建；② Python 兼容 re-export 模块统一 DeprecationWarning 模板（对齐 preset_func_blocks_compat）；③ 单一退役登记表（旧 API/新 API/形态/引入版本/计划移除版本/使用点/黑名单 grep，可并入本设计或新建 deprecation-registry.md）。
- **preset_func_blocks 双 compat 合一**：删/合并 preset_func_blocks_compat.py（dead）与 preset_func_blocks.py，保留一个带警告+v5.0.0 期限的 compat；清理新模块 docstring 反向广告旧导入。
- **4 个多形态退役收敛**：lvMutex（J2 迁移 lv_mutex_t + 删 runtime_guard 平行 typedef + 黑名单）；lvPlugin（J5 删别名全量迁移）；名称表函数三层命名收敛（13+ 调用点直连权威）；NODE_TYPE_CIRCLE 语义漂移修复（映射 (GeomType)-1 → GEOM_CIRCLE）。
- **黑名单 grep 化**：应退役未退役符号进 CI 黑名单（lvMutex/lv_MUTEX_*/interval_*/无前缀旧函数宏），新代码触黑即失败。
- **健康反例沉淀**：constraint_graph.c "彻底移除不保留兼容层"作为退役标准模式写入规范。

### 3.47 降级/回退路径面（第十轮，K7）

**决策**：
- **降级语义统一**：失败后 5 种表达（false/UNKNOWN/None/异常/静默吞）收敛为统一约定（C 侧返回码 + Python 异常 + 显式日志）；降级命名统一（fallback/degrade/downgrade/retry 混用收敛为 degrade_total 体系）。
- **统一降级登记**：扩展 PerformanceCounters 增统一降级计数（degrade_total + 按种类 degrade_backend/numeric/trust/binding/resource 细分），Python get_counter_report 透出；所有降级点最低要求 lv_LOG_WARNING + 计数器 + 可选 stream event。
- **收敛重复降级**：求解后端 4 套降级收敛到 scheduler fallback_chain 单一语义；符号→double 7 处收敛统一 helper（symbolic_coord_to_double_safe(coord,&ok)）；索引→线性扫描 ~10 处收敛统一宏（参照 LV_DISPATCH 先例）；"C 库不可用"两套收敛（_FakeBinding 提供 _lib stub 真覆盖或统一绑定降级层）。
- **修复静默降级 9 项**：_FakeBinding noop 至少 warn once；sparse→dense 加日志；symbolic_coord_auto_degrade 修复 (void) reason（原因进日志）；high_dim except: pass 改 logger；png 全白图加提示；proof_engine enable_cache 补实现或删字段；索引→线性扫描加登记。
- **分层保留**：绑定/后端/数值/信任/资源 5 场景语义不同可保留多级降级结构，但每处必须统一登记。

### 3.48 基准测试面（第十轮，K8）

**决策**：
- **微基准单一框架**：保留 performance_profiler lv_perf_benchmark_run（预热+校准+Welford 唯一活性），补齐注册/批量运行/JSON 导出；删 test_framework lvBenchmark 全套 dead API + 撤销 P2-1 豁免注释；test_adaptive_threshold 手写 QPC 改走框架。
- **Welford 收敛**：承接 I2 决策落地（单一 L0 统计原语，B 与 F 共享）。
- **性能回归门**：3 处硬编码阈值迁入统一基线表（bench 名→阈值/宽松度）；CI 加专用串行性能 job（避免并行 flaky）；可选 baseline.json 对比（落地 TEN_LAYER_OPTIMIZED_PLAN 规划形态）。
- **分层保留**：单元微基准（B 测试用）/ 会话剖析（C lvPerfSession 调试用）/ 生产在线监控（F lvPerfStats 运行时用）三层语义不同保留，统计原语共享 L0。

### 3.49 特性开关面（第十轮，K9）

**决策**：
- **特性开关单一表**：新增 feature_gates.h（宏名/CMake 名/默认值/权威处/消费点五元组），命名统一 lv_ENABLE_* 废除三套前缀（lv_ENABLE_*/LV_HAS_*/lv_HAS_*）。
- **死开关清理**：lv_ENABLE_RUNTIME_GUARDS 接线或整体删除（连编译都过不了）；lv_CONTEXT_THREAD_SAFE 删死字段；lv_LOG_GUARD 修名或删；LV_USE_ZLIB 接线或删；lv_WASM_BUILD/lv_NO_GMP/lv_HAS_OPENMP 接线或删；幽灵引用宏清理。
- **收敛重复开关**：运行时守卫 5 种承载 → 单一（编译宏 + feature_gates 表）；线程锁 3 套 → lv_thread.h 唯一（承接 J2）；层验证 CMake 名与宏名对齐（ENABLE_LAYER_VALIDATION → lv_ENABLE_LAYER_VALIDATION）+ 接线（承接 I5）；lv_PUBLIC_API 双定义修复（config.h:643 加 #ifndef，承接 K4）。
- **平台检测收敛**：~81 处直接 #ifdef _WIN32 迁移到 lv_PLATFORM_*（cross_platform.h 统一层）。
- **文档对齐**：ENABLE_LAYER_VALIDATION 默认值（CMake OFF vs 文档 ON）修正。
- **分层保留**：编译期尺寸常量（lv_CONFIG_*/lv_EPSILON_*）/ 运行时配置（lvConfig）/ 构建选项 / 平台自动检测四层合理保留。

### 3.50 内存所有权约定面（第十轮，K10）

**决策**：
- **补写单源契约文档**：落地 design doc P0 项①——编写 docs/architecture/memory-ownership.md（三态定义 + 示例 graph_add_point=copy/module_set_graph=take/graph_get_node=borrow + 反例 func_block_register/module_compute_content_hash + 分配器配对表 lv_malloc↔lv_free 杜绝 free/lv_free 混用）；36_memory_management.md 保持分配器层规范并互链。
- **注释对齐（机械可审计）**：公共头关键 API 统一加 [copy]/[take]/[borrow] 前缀（P0 项②，grep 可查）；统一 free/lv_free 词汇二选一明确写出；修复 3 处已知错误：func_block_register @param（改"深拷贝，调用方保留 fb 所有权"）、module_compute_content_hash（free→lv_free）、API_QUICKSTART（normalization_result_free→normalization_result_destroy，随 K5 归档消解）。
- **补声明缺口**：graph_add_node_with_id/graph_add_constraint_with_id/module_add_axiom_package（take）/graph_get_node（borrow）等无注释 API 补三态标注。
- **静态检查**（P0 项③ 可选）：脚本扫描 *_copy 必须含 copy 标注、*_destroy 必须含 take/owned 配对、graph_add_* 坐标参数必须标 copy；以 C1/C3 为回归用例。
- **健康确认**：12 项核心 API 抽查 11 项实际语义健全（copy/take/borrow 已执行，含 Python _PtrOwner 与 C 侧逐一对齐）——三态契约已在实现层执行，只缺文档化与标注。

### 3.51 测试替身面（第十一轮，K11）

**决策**：
- **测试设施移出生产库（P0）**：test_framework.c + bootstrap_test*.c ×8 加 BUILD_TESTS 门控或移出 lv_static；bootstrap_test_internal.h:29 生产头重定义 CONSTRAINT_DISTANCE 移除（CONSTRAINT_DISTANCE→INCIDENCE 映射归测试侧）。
- **测试辅助收敛**：add_point 私有副本（recursion_demo.c:34）删；图构建内联构造器 ×13（10 文件）收敛到 lv_test_geom_graph_builder.h；verify_single ≡ verify_prove_src 合一；断言宏双轨 + 3 处 #undef override 修复（承接 E11）。
- **mock 机制统一**：四套命名（stub_/fake_/dummy_/mock_）收敛为统一约定；插件替身 3 处手写收敛共享构造器；预言机 3 处手写收敛 bootstrap_test_oracle（归一化已收敛，扩展其余）。
- **分层保留**：单元级函数指针 mock / 集成数据驱动 fixture（axiom_test_common/lv_test_geom_graph_builder）/ 自举差分 oracle 属不同测试类型保留。

### 3.52 数学常量面（第十一轮，K12）

**决策**：
- **补权威宏（P0，修复精度损失）**：config.h 新增 lv_SQRT2/lv_SQRT1_2/lv_SQRT3 权威宏；MCTS_C（proof_search_algo.c:480）改引 lv_SQRT2（修复 8 位有效损失 7 位精度）；测试 1.41421/1.4142/1.414 收敛。
- **e 常量单一权威**：lv_E（lv_numeric.h 零调用）并入 config.h 数学常量区或明确 lv_numeric.h 为权威 + lv_platform.h M_E 改 shim 注释指向。
- **字面量收敛**：测试侧裸 M_PI/M_PI_2/M_E 30+ 处 → lv_PI/lv_HALF_PI/lv_E（M_PI 收敛批次延伸，逐位等价已证）；全精度手写 π → lv_PI；180.0/360.0 → lv_HALF_CIRCLE_DEG/lv_FULL_CIRCLE_DEG；lv_DEG_TO_RAD/lv_RAD_TO_DEG 接活（lv_numeric.c 改引）或删除（死宏二选一）。
- **去重**：normalization.c:72 NORM_GOLDEN_RATIO_MIX → 引 lv_HASH_GOLDEN_RATIO_64（Q14 漏网补齐）；GEOEVOL_PI_SMOOTH_FACTOR 保留 geom_evol.h 权威、删 config.h compat 段该行。
- **豁免登记**：stream 演示数值（3.14159/2.71828/1.41421）标 `/* exempt: 演示数值 */`；geometry_transform kAngleTable 补 √2/2、√3/2 来源注释（不同用途查找表）。
- **健康确认**：M_PI 收敛 14 文件 52 处已登记、黄金比 Q14 已登记——不重复报告。

### 3.53 错误处理模式面（第十一轮，K13）

**决策**：
- **lv_DEFER 为唯一推荐清理机制**（159 调用点 + test_lifecycle 专项测试钉住）；goto 仅保留事务回滚（rollback 语义）+ MSVC 回退两类。
- **构造器回滚统一 guard-detach（P0）**：solver_symbolic.c factorize（U5 同类遗留，lv_mpz_clear_deferred/mpz_poly_clear_deferred 设施现成，判据 A 同构，mpz_clear 组合手写 4 次收敛）→ P1 func_block_pack（guard-detach 可覆盖条件销毁）→ P2 空标签 goto 展平 2 处（preset_manager_query error:/solver_equation_extract push_error:）。
- **锁守卫统一 lv_DEFER（P1）**：groebner_engine/poly/variety 三文件 36 处宏+goto _gcleanup 迁移到 lv_DEFER 锁守卫（groebner_engine_ideal 为范本）。
- **清理重复收敛**：lambda_unify destroy+free 3 次、preset_manager_query/proof_trace_tree/lv_process 双路径重复 → lv_DEFER 单点后自然消除（P3）。
- **标签规范**：若保留 goto 规范为 {error, cleanup, done} + 事务 {rollback}，禁止新造标签（28 种收敛）。
- **新代码规范**：分配即注册 lv_DEFER、构造器 guard-detach、禁止同资源双路径清理（黑名单 grep 形态）、豁免必须 `exempt:` 标注。
- **合理差异保留**：lv_process fd/pid+fork、rewrite_apply txn 快照回滚、lambda_unify 两阶段事务、链表/hashtable 逐节点清理、module_delta 按值移交、MSVC 回退（需确认平台支持范围）。

### 3.54 适配器桥接面（第十一轮，K14）

**决策**：
- **证明导出锚定 L5 interop_export_coq/lean 命令路径**（承接 E3）；L10 coq_bridge/lean4_bridge 保留为"外部工具注册面"但 export 骨架复用同一发射设施；删仅测试接线的 proof_export_enhanced/proof_export/interop_theorem 导出分支。
- **tactic 映射单源化（P0）**：以 LV_PROOF_STEP_TYPE_X X-macro（interop_export_coq.c:37 雏形）为单一事实源派生 Coq/Lean/Isar/OPML 各格式表，删 ≥8 处独立映射表；外部契约差异（coq 本地枚举）保留 exempt 标注登记。
- **SVG/TikZ 收敛（P1）**：各保留 L5 权威实现（interop_export_svg/tikz_export）；删 L4 module_export 的 svg/tikz/pdf（Module 导出只出 JSON/LVZ，可视化委托 L5）。
- **序列化统一注册表（P1）**：删 LVZ 内嵌图序列化器 + module_serialize_graph_to_json 薄封装；func_block/axiom/prop_verifier 等 7+ 序列化模块接入 lv_storage 注册表。
- **层归属锚定**：ConstraintGraph→L3、证明域→L4、输出/命令→L5、外部工具→L10、可视化→L6；lv_serialize_adapters 明确"L4 注册适配"或下沉验证回调二选一；L0 门面统一走单一分发（删临时文件+读回模式）；upper_interop_export_* 三函数同构抽公共 helper。
- **清理**：lvProofStep 同名双定义（interop_bridge_common vs opml_codec）改名；interop_export_coq.c 孤儿 Lean 注释；LV_HAS_LAYER6_CONVERTER 旧桩宏退役；L10 补 target_link_libraries。
- **健康确认**：lv_dot_writer 统一 7 消费方、L6 converter 守卫已收敛、L10 桥接骨架已收敛、bootstrap 双份 .lv 显式豁免——不需要动作。

### 3.55 全局状态线程安全面（第十一轮，K15）

**决策**：
- **全局 getter 化（P0）**：跨文件可写全局一律 static + lv_<module>_current()/get_global()；头文件禁 extern 非 const 变量（J5 黑名单续）；s_upper_state/s_debug_state/g_library/g_preset_library/g_coeff_pool 收敛。
- **TLS 容器化模板**：lv_DECLARE_TLS_STATE(type,name) + getter，归并散点 TLS（error_codes 5 个→1、high_dim_view 4→1、symbolic_coord 3→1）；bit_burning_get_global_state 更名（消除"global=TLS"命名反模式）；stream_ctx ×14 全量收敛宏（axiom_pkg 命名不统一修复）。
- **锁补全（P0，3 个真实竞态）**：lv_config 读写统一（目标=geometry_config 进程权威+TLS 快照模式，修复撕裂读 G1）；g_coeff_pool lv_mempool_static_init 内建 lv_once（修 TOCTOU G5）；s_upper_state/g_preset_library/g_library 读路径补 lv_lazy_lock（G2-G4）；系统状态机从 TLS 提升进程级+锁（修跨线程 lv_init G7）。
- **容器收敛**：配置 5 容器→A 进程级权威（B ConfigManager 并入或明确历史工具，g_state 待删 G5，几何键消除重叠删 sync 桥接）；内存统计双容器统一一套；预设库 g_library/g_preset_library 合一；日志状态 4 容器收敛（级别消除双权威）；后端注册表样板 ×5 用统一 lazy lock 模板。
- **初始化顺序**：惰性单例登记显式初始化依赖清单；lv_cleanup 的 TLS 清理并入注册表逆序流程。
- **分层保留**：进程级权威（配置/分配器/注册表/线程池/日志 sink）+ TLS 快照（geometry_config 范本）+ 线程级上下文（error/stream/scratch）+ 模块 static const 表；目标模式：memory_pool/runtime_monitor/geometry_config/lv_error。
- **健康确认**：lv_allocator_get 锁外读（对齐指针原子性声明）、static const 表、g_simd_stats 原子——不需要动作。

### 3.56 头文件包含卫生面（第十二轮，K16）

**决策**：
- **引用即包含（IWYU）锚定**：公共头自包含（用到的类型在本头 include 或 lv_fwd.h 可解析）；消除 7 处传递依赖（lv_internal.h 宏体引 StreamContext 补 include、plugin_system.h 字段 lvContext 补 context.h、module.h/func_block_preset.h/proof_engine_enhanced.h/coeff_pool.h/solver_core.h 等补精确 include）；拼写统一 "lv/<file>.h" 消灭 26 多拼写 + <lv.h> 尖括号。
- **前向声明集中（P1）**：新建 lv_fwd.h，收敛 20 类型 ~73 行重复 typedef（ConstraintGraph/lvEngine/StreamContext/lvContext/TypeRegion/lvHashtable/ProofNavigator 高发）；各头只保留 include lv_fwd.h 或定义头（J5 落地）。
- **伞形治理（P2）**：lv.h 保留唯一对外伞（应用/test 合法入口）；12 公共头改精确包含（status_codes.h 只需 error_codes.h）；func_block_internal.h 移除 lv.h；lv_loader.c/lv_utils_misc.c 移除 lv.h 解除 L1/L2 越层（I5 落地）。
- **内部头物理归位（P3）**：6 个内部头移出公共目录或明确标记不随公共头安装；proof_session.h 内部链解除（公共头禁 include *_internal.h）；拼写/守卫命名统一（LV_XXX_H×8/lv_lv_XXX_H×3 → lv_XXX_H）。
- **验证机制**：公共头自包含编译检查（每头独立 TU）或 IWYU 进 CI；依赖方向守卫在清零后启用。
- **健康确认**：守卫全覆盖/零环/内部边界语义成立——不需要动作。

### 3.57 fuzz/测试工具面（第十二轮，K17）

**决策**：
- **sanitizer 单一权威矩阵**：fuzz target 补 UBSan（与 ENABLE_SANITIZERS=address,undefined 对齐）；fuzz.yml 不再 OFF 掩盖分叉；考虑 TSan/MSan/优化级矩阵。
- **fuzz target 生成统一**：CMake 函数/循环生成（消灭两 target 旗标逐条复制）；workflow 运行参数/corpus 目录变量化（消灭 3 处硬编码）；编译器选择单点。
- **回归缺口补齐**：corpus 进仓库 seed + CI 回灌 + crash 归档为回归用例；fuzz 回归纳入 push/PR CI（缩短一周暴露窗口）。
- **文档修正**：test_verification_report.md "3 个 fuzz 测试"改 2 + 更新 add_lv_test_and_register 引用。
- **分层保留**：fuzz vs ctest 单测 vs integration 三层互补——不算重复。

### 3.58 打包/发布面（第十二轮，K18）

**决策**：
- **find_package 修复（P0）**：install(EXPORT) DESTINATION 去 "build/build"（笔误）+ 与 lv-config.cmake 同目录；导出目标 lv::lv_static 与检查/文档 lv::lv 对齐（加 lv::lv 别名或改检查名）；导出文件 GMP 绝对路径改 find_dependency(GMP)；lv-config.cmake.in 注释残留 3.5.0 清理。
- **lv.pc 修复（P1）**：模板用 lv_PC_LIBDIR/lv_PC_INCLUDEDIR（现死代码）或 ${pcfiledir} 相对化；prefix 可重定位。
- **导出机制统一（P0）**：删 config.h:643 无条件空 #define lv_PUBLIC_API + 各头 #ifndef 兜底（三处定义收敛一处）；lv_BUILD_SHARED 改 INTERFACE 传播 + 接线 lv_USE_SHARED（消费端 dllimport）；非 Windows 设 CMAKE_C_VISIBILITY_PRESET=hidden；Windows 显式宏 vs WINDOWS_EXPORT_ALL_SYMBOLS 二选一（保留显式宏则去掉全量导出，杜绝内部符号裸奔）。
- **版本单源（P1）**：VERSION 文件或单一 CMake 变量派生 project()/CPack/lv.pc/Python（pyproject+setup.py 合并）；preset_blocks 4.0.0 偏离对齐。
- **发布物真正产出（P2）**：CI 加 package 步骤（CPack + wheel 构建核对命名 lv-1.1.0.*）；WASM 补真实 CMake target（lv_web）或删三处声称/CI 引用（web/ 目录空且 gitignore）。
- **分层保留**：C 库（liblv.a/.so/.dll + 头 + 包配置）/ Python 包（ctypes wheel）/ WASM（前端部署）三个不同发布物并行，同一版本单源取号。

### 3.59 数值稳定性面（第十二轮，K19）

**决策**：
- **容差单一表执行（P0）**：补 lv_GEO_PERPENDICULAR/PARALLEL_EPSILON 编译期常量；消 1e-12 双源（lv_EPSILON_DOUBLE vs lv_EPSILON_ULTRA 二选一）；**激活 3 个零消费者 cfg 字段**（perpendicular/parallel/angle_epsilon）并让 solver_geom_templates/meta_proof/conflict_detector/interop/algebra_mode 改引用（共线 7 值/垂直 5 值/角度 3 值收敛）。
- **测试断言容差表（P1）**：287 处裸字面量 tol → 命名容差表（1e-9×91/1e-12×66/1e-6×34 收敛）；bicgstab/gmres 1e-6 vs sparse 1e-4 不一致修复；approx_eq 1e-10 入表。
- **1e-15 判零相对化（P1）**：大坐标下绝对判零点改相对缩放（结合 lv_rel_tol_scale）。
- **跨语言桥接收敛（P2）**：Python dsl 1e-9/1e-12/1e-15 与 Lean FFI.lean:74 EPSILON=1e-9 第四层定义 → 引用单一表。
- **数值稳定性规范文档 + 黑名单**：集中规范（相消规则 6 条 + hypot/缩放惯例）+ 裸魔法数黑名单；geo_utils.c:204 陈旧注释修正。
- **分层保留**：精确 GMP/近似 double/区间三语义（E8-E10）+ 谓词 EXACT/APPROX/ADAPTIVE 两层阈值 + 哨兵族 + 豁免点（geom_evol 1e-30/float 1e-6f）。

### 3.60 声明实现一致性面（第十二轮，K20）

**决策**：
- **一致性检查机器守门（P1）**：构建期启用 -Wmissing-prototypes -Wmissing-declarations（GCC/Clang，抓声明无实现/实现无声明/漏 static 三类）；脚本化对拍（头原型集 vs .c 定义集，宏生成白名单 LV_STREAM_CTX_DEFINE/LV_DESTROY_SHIM/伞头重导出）。
- **1 例实现无声明修复（P0）**：_symbolic_coord_degrade_check_algebraic 补头声明（symbolic_coord.h 或 internal）或改 static + 文件内转发（4 处调用）。
- **幻影 API 处置（P0，承接 K3/K4）**：lv_get_version_info 补实现（结构体已在 lv.h，从 lv_get_system_info 抽取字段）或删文档引用；lv_get_memory_stats_ex 家族删 README 清单（真实 API lv_get_memory_stats + MemoryStats）；normalization_result_free 文档改 destroy（随 K5 归档消解）；lv_check_version_compat 改真实校验（比对外部 DLL/绑定版本）或删除（M6）。
- **死宏清理（P1）**：删 lv_LAYER_VALIDATION_FLAG_* 与 PROOF_STRATEGY_* 数字宏（仅 VECTOR 在用）；MAX_MODULE_DEPTH 单源化（config.h ↔ module.h 二选一）；layer_validation 子系统"接线或删"二选一（承接 I5）。
- **重复声明收敛（P2）**：sat_encoding.h↔relation_model.h（13 函数）、proof_session_internal↔proof_rule_engine_internal（7 函数）收敛单一权威头；伞头 lv.h 重导出保留但加注释锚定权威头。
- **健康确认**：5 头 242 声明 100% 对齐、527 全局 0 泄漏 0 漏 static——记录为健康基线。

### 3.61 代码风格格式面（第十三轮，K21）

**决策**：
- **锁定 clang-format 版本 + CI 强制（P0）**：固定 22.x 版本（版本漂移是 40% 复查违规根因）；ci.yml 增 format check job（clang-format --dry-run --Werror 覆盖 core/ + test/）；ui/package.json format 脚本移到根级纳入 CI。
- **一次全量回填格式化**：对 557 个违规文件跑 clang-format -i（含 test/ 302 文件，9e6366f1 从未触碰），提交"全项目 clang-format v2"。
- **豁免登记制度**：为合理差异（#ifdef 块缩进 lv_platform.h / LaTeX 字符串超长 / 横幅间大空行 / 头文件短 // 注释）建 `// clang-format off/on` 白名单 + doc/CLANG_FORMAT_EXEMPTIONS.md，禁止无登记豁免。
- **注释对齐**：test 目录按 core 标准补 /** Doxygen（@brief/@param/@return）或登记 test 允许 /* 简化注释。
- **合并立项**：命名双轨（J5）/守卫三变体（K16）/动词三轨（J5）与格式化漂移同源，格式+命名统一合并立项。

### 3.62 魔法字符串面（第十三轮，K22）

**决策**：
- **导出格式单源化（P0）**：以 INTEROP_EXPORT_FORMAT_X 为唯一格式名表（补 json/json-pretty/dot/latex 别名列，仿 LV_GEOM_TYPE_ENTRY ALIAS 列）；kExportFormatHandlers/kProofExportFns/do_export/upper_interop tag 全改由该表派生。
- **lvExportFormat 三定义合一（P0）**：保留一个类型名（proof_export_enhanced.h 引用 lv_view.h 单源 + 迁移映射或并入 InteropExportFormat），补 name 表，删文档副本——修复数值互斥（HTML=0 vs COQ=0）。
- **TransformType/变换预设名（P0）**：提升为头部级 X-macro（display+json+预设名三列）；lv_transform_type_name 改表驱动；修 "protective" 拼写错误与 GLUING 分歧；基础 7 名并入 preset_name_defs.h 生成源。
- **命令枚举对齐（P1）**：GET_NODE/GET_CONSTRAINT 补名表与 handler 或从枚举移除；修"18 种"注释；lv_protocol.c "Solve"/"Health"/"idle/running/error" 改用 proof_step_registry/engine_status_to_identifier()。
- **序列化键单表化（P1）**：axiom_rule_engine ~40 键、interactive_geo 读写双份键集、module_delta 15 键 → 各序列化器内一张字段表（契约豁免 + 单表防漂移）。
- **协议契约显式标注**：lv_builtin_commands/OPML/JSON/LVZ 字段键/tactic 表/DSL 关键字统一补 `/* exempt: 外部格式契约 */` 标注并登记（黑名单 grep）。
- **健康确认**：生产 strcmp 链=0、~50 张 name 表、LV_CFG_* 单源标杆——不需要动作。

### 3.63 序列化互转矩阵面（第十三轮，K23）

**决策**：
- **序列化注册表单入口（P0）**：注册表从 1 格式扩展到全对象×格式（ConstraintGraph:json/bin、Module:json/lvz/msgpack、FuncBlock:text、AxiomPackage:text、HighDim:json），各模块 save/load 改薄包装调注册表；删 ConstraintGraph→JSON 2 条冗余路径（module_serialize_graph_to_json 薄封装）。
- **Module 三载体收敛（P0，承接 S1）**：删/降级 msgpack（丢图缺陷）与 LVZ 内嵌图写端（不可读）；唯一权威 JSON。
- **跨格式 round-trip 测试（P1）**：同一对象经各格式往返后用同一强等价比较器断言；对 msgpack 缺图、LVZD 子集等格式契约差异显式断言而非静默。
- **OPML 不对称修复（P1）**：步骤类型双表合一（LV_STEP_TYPE_OUT_X vs LV_STEP_TYPE_X 仅 5 项重叠 4 处错位）、description/name 键统一、id/dependencies 补读、导入产物回 ProofNavigator。
- **round-trip 比较器强化（P1）**：meta_repr_graph_equivalent 升级为含坐标/信任色/namespace 的强等价（meta_repr_isomorphic 语义），坐标全丢不再静默通过。
- **分层保留**：LVZD 压缩子集（登记为有损格式补差异断言）、OPML/Coq/Lean 外部契约、HTML/LaTeX/DOT/公式字符串展示只写、manifest/INDEX 元数据。

### 3.64 测试数据生成面（第十三轮，K24）

**决策**：
- **per-instance RNG（P0）**：保留 lv_random_* 唯一 API；新增实例化 RNG（lv_rng_t 结构体，bootstrap RandomGenerator.current_seed 为雏形）；approx_counter/interactive_geo 改实例化使用，**禁止抢占式全局 lv_random_init**。
- **确定性 seed 套件级管理（P1）**：TEST_MAIN_BEGIN 默认 lv_random_init(固定 seed) + 可覆盖（LV_TEST_SEED 环境变量/宏），把 test_lv_utils.c 复现模式推广为套件级约定（使 propagation 等生产随机路径可复现）。
- **默认 seed 单一定义**：approx 12345/42 统一一处具名常量。
- **数据构造辅助收敛（P1）**：删/登记豁免 recursion_demo.c add_point 副本（K11 R1 闭环）；统一圆建模单一 API 修 builder 注释（containment vs graph_add_circle）；裸 graph_add_point 内联 14 文件按语义迁移或显式豁免。
- **死设施处置**：bootstrap_test_random.c 零调用方——接线自举差分测试或随 K11 R7 移出生产库。
- **分层登记**：单元内联/集成 fixture/fuzz corpus/examples 独立四层 + 现有豁免写入决策登记；黑名单 grep（私有 add_point 定义、第二个随机默认值、生产/测试混合随机源）。

### 3.65 硬编码路径面（第十三轮，K25）

**决策**：
- **路径常量单源（P0）**：新增项目级路径常量层（lv_DATA_DIR 资源根可环境变量覆盖 / lv_AXIOM_DIR / lv_PRESETS_DIR / lv_TEST_OUT_DIR，C 宏 + Python 常量同源）。
- **共享库加载收敛（P0）**：Python 3 处搜索收敛单一函数 + 单一目录常量表（含全部现存名 build3/build4/build/build_mingw/Release 杜绝互斥），权威位置走 lv_LIBRARY_PATH 或 CMake 生成 lv.pc。
- **公理包测试路径（P0）**：117 处 define 收敛单一可生成测试常量头或读 INDEX.json（62 文件）。
- **预设单源（P1）**：lv_PRESETS_DIR 单源 + 56 文件名清单生成 + preset_blocks.c 手写 memcpy 改 lv_path_join。
- **测试输出目录（P1）**：build_verify/ 硬编码 vs test_outputs/ 约定对齐（lv_TEST_OUT_DIR）。
- **bootstrap .lv（P1）**：4 变体探测（含 "Lv-00/" 前缀）改单一基准解析。
- **资源定位三级解析（P2）**：环境变量 → 编译期注入 → CWD fallback，替换多候选探测模式。
- **健康确认**：lv_path_join ~31 处统一、lv_PATH_SEPARATOR 平台分隔符统一、lv_temp_path/lv_path_home_dir 日志临时文件收敛——不需要动作。

### 3.66 错误注入容错面（第十四轮，K26）

**决策**：
- **内置失败分配器（P1）**：新增 `lv_allocator_fail()`（fail_nth 计数注入：第 N 次分配失败后恢复），作为测试侧标准失败注入通道；lv_malloc/calloc/realloc/free 全走 vtable 已具备注入面，只缺失败分配器本身。
- **注入盲区修复（P1）**：lv_malloc_tracked 改走 vtable（消灭与 debug_alloc 的平行实现，tracked 不再绕过注入）；memory_pool 块分配改 lv_malloc（原生 malloc 绕过注入）；GMP 分配不可注入登记为豁免。
- **OOM 错误路径测试补全（P1）**：核心 API（Module/Axiom/ConstraintGraph 加载、证明引擎入口、序列化读写）补"分配真实失败→断言 NULL/错误码"用例；激活 lv_set_memory_limit 被注释禁用的测试断言（test_utils.c:88-94）。
- **LV_FAULT_INJECT fail-point 宏（P2）**：统一失败点标注宏（含启用开关），熔断模拟从 API 直调改走 fail-point；OOM 文案中英冲突随 K1 场景文案规范表一并收敛。
- **三层登记（P2）**：fuzz（内存安全）/ unit NULL-参数（参数校验）/ 注入（资源失败）三层语义不同——分层合理，但需登记表明确各层职责，资源失败层不得再缺位。
- **健康确认**：1808 处生产错误返回 + error ctx/链/宏机制覆盖良好——是"错误产生"机制，与"错误注入"不同层，保留。

### 3.67 UB 整数安全面（第十四轮，K27）

**决策**：
- **安全算术单一入口（P0）**：以 lv_safe_{mul,add,sub}_i64 为唯一权威，废弃/收敛 lv_SAFE_ADD 宏（lv_internal.h:232）→ int 调用方改 lv_safe_add_i64+饱和语义（或薄 int 包装）；func_block_utils.c:96、nt_polynomial.c:180 手写加法溢出改调权威；GCC/Clang 下可顺带换 `__builtin_add_overflow` 为同一语义实现细节（接口保持 lv_arith_safe.h）。
- **字面量解析收敛（P0）**：formula_dsl_lex.c 与 axiom_pkg_parser.c 的「digit 累加预检」收敛为公共 `lv_str_to_i64_safe`（含符号/钳位语义），消除两处独立宽度实现。
- **增长逻辑单一路由（P0，承接 I3/F27）**：执行 IntArray 废弃（lv_utils_array.c array_grow_to_fit），10+ 处手写 INT_MAX/2 倍增循环统一迁 lv_ensure_capacity；为 size_t 口径提供 lv_ensure_capacity64 权威，结束 int/size_t 双口径桥接（geo_topology.c:142 无守卫直转等 10+ 桥接点一并治理）。
- **parser_safety 接线（P0，防护缺失修复）**：lv_load_file/主解析链接入 lv_input_validate（parser_max_input_length）；lv_parser 递归深度接入 lv_check_ast_depth（或 parse 层计数，与 K28 深度表联动）；节点数/语句数/参数数/Token 长度闸门接线到 lvConfig 已存在的上限字段；parser_safety.h 补齐声明，删除或接线死代码 lv_check_*。
- **唯一 +1 溢出修复（P0）**：graph_conflict.c:612-629 `(size_t)max_node_id + 1` 先提升再运算；text_code.c:136 先查后加。
- **sanitizer 覆盖 ctest（P1，与 K17 合并执行）**：CI 增加 `-DENABLE_SANITIZERS=ON` 的 ctest job（address,undefined 覆盖常规测试）；fuzz target 补 undefined（fuzz 侧对齐 UBSan）；fuzz.yml 不再 OFF 掩盖；解析器纳入 fuzz 或至少让 UBSan 跑解析链用例兜底递归/算术类问题。
- **窄化强制（P1）**：新增窄化辅助（lv_cast_size_to_int/lv_ASSERT_FITS_INT：DEBUG 断言 + release 饱和，与 K30 强转检查合并立项）；开启 -Wconversion/-Wsign-compare 的关键模块或建 lint 清单跟踪 1539 处 (int) 桥接并标注守卫；统一 lv_ensure_capacity int 口径问题（见增长逻辑）。
- **可移植性缺口（P1）**：algebraic_number_io.c:48 补 `#if defined(__SIZEOF_INT128__)` 守卫（MSVC 无法编译）。
- **分层保留**：分配器 SIZE_MAX 检查（分配层）、lvStrBuf SIZE_MAX/2（缓冲层）、bit-burning SIZE_MAX 哨兵（熔断层）、sanitizer（动态层）、lv_arith_safe.h 判定式（已逐分支验证无 UB）。
- **健康确认**：有符号移位/INT_MIN 除法/除零均干净、lv_gcd 收敛（K2）✅——不重复报告。

### 3.68 递归栈深度面（第十四轮，K28）

**决策**：
- **单一权威深度限制表（P1）**：新建 `depth_limits.h` + 配置表，按"递归域"分层，每层单一默认值来源，禁止散落 #define：解析域（256，A1 接线）、推理/证明域（100/1000/10000 三套**统一为一个推理深度层**，建议 1000 语义：外层引擎迭代不递归实际 C 栈安全）、λ/公式递归域（1024）、全局熔断硬限（128，四重复定义合并一处 + lv_RUNTIME_GUARD_MAX_RECURSE 并入）、销毁/复制域（补统一 LV_DESTROY_MAX_DEPTH，裁决文档 10000 vs 代码 200 的 50 倍差）；各搜索器局部语义（B3 16/B4 50/100/B9 32）保留但登记进统一表。
- **A1-A6 死机制接线或删除（P0）**：六套核心机制全部生产零调用——要么接线（reasoning_stack/circuit_breaker/recursion_enter 接入真实引擎循环、lv_check_ast_depth 接入解析入口）要么删除并统一到新表；当前"文档与测试宣称防护、生产裸奔"比无防护更危险。
- **无防护点补限（按优先级）**：P0（可构造爆栈）= formula 三解析器补 current_depth 递增+上限检查（256，node_count 拦不住纯括号嵌套）+ lv_parser.c 递归下降补 depth 参数 + 入口调统一深度检查；formula_node_destroy/copy、lv_ast_destroy、lv_trace_node_destroy、lv_tree_release_recursive、lv_lambda_destroy、sym_expr_* 统一补深度参数或改显式栈（destroy 域优先显式栈，样板已存在：graph_traversal_dfs/tree、proof DFS/MCTS、graph_conflict、prop_verifier destroy、module path）；P1 = lambda_unify 5 族子递归（occurs_check/apply_subs/is_pattern 等 depth 参数贯通）、dsl_compiler_ir compile_node、lv_sema check_expr、render_* 深度限制。
- **栈帧治理（P2）**：渲染器每层 1-2KB 缓冲 × 无防护递归改显式栈或降帧；lv_impl_upper_app 16KB 帧、lambda_unify bound_args[256]×1024 层登记评估。
- **分层保留**：图/树遍历显式栈（graph_traversal_dfs/tree、proof_search_algo、graph_conflict DfsFrame）已是健康样板——默认 max_depth=0 不限需登记为有意语义或设上限。
- **接线决策（最重要）**：与 K27 parser_safety 接线、K16 守卫/命名统一合并立项，避免同一处重复接线。

### 3.69 死锁锁顺序面（第十四轮，K29）

**决策**：
- **锁顺序规范文档（P0）**：新增 `concurrency_locking_standard.md`（或并入 30_performance_concurrency.md）：全局总序 `registry锁 → 业务大锁(g_data等) → 叶子锁(log/config)`；双锁一律要求"公共入口单锁、内部 `_locked` 函数不加锁"（high_dim_view 范式）；锁内禁止调用其他公共加锁 API（groebner_engine.c:806 注释升级为全局规则）；检测脚本扫描"同一函数内两把不同 mutex 的 lock"。
- **锁抽象收敛（P1，承接 J2/F29）**：runtime_guard 族（lvRwLock/lvMutex 重复 typedef/lv_RUNTIME_LOCK）要么默认启用要么明示废弃；删除 deadlock_warnings/lock_timeout_count 死字段或实现计数；日志收敛单锁单管道（lv_log_write 已委托 debug_log，可删 runtime_monitor log.mutex setter 锁）；lvWaitGroup/lvTaskGroup 二选一；后端注册表锁 4 份复制收敛统一 lazy lock 模板（承接 K15）。
- **平台分裂修复（P0）**：lv_mutex 文档标注非递归 + 新增 `lv_mutex_trylock`/`lv_mutex_lock_timed`（统一超时原语，供死锁防护）；LV_SCOPE_LOCK/lv_DEFER 在 MSVC 下退化加编译期告警或 static_assert 提示（承接 J2 编译器分裂）；非递归锁重入语义差异写入锁文档。
- **倒锁防护（P0，真实死锁风险）**：lv_registry_remove/remove_prefix 的 destroy 回调移出锁（先摘出快照再回调，lv_registry.c:268/306）；stream_async 文档约定"回调内禁止 stream_flush/set_async_mode(false)/context_destroy"或提供 reentrant-safe 变体（否则消费者线程自等待/自 join 死锁，stream_async.c:169-175/137）；回调统一"锁外回调"约定（preset_manager 为范本，推广）。
- **潜伏自锁链修复（P1）**：memory_pool stats_mutex 持锁 lv_strdup 链——lv_mem_record_alloc 若接入生产 lv_malloc 即自锁；修正 memory_pool.c:24 过期头注释，明确 stats 只统计不自分配。
- **粒度统一（P1）**：lvRegistry 全 API 统一加锁（或显式声明 lv_registry_find 仅初始化期）；groebner g_data_mutex 3 种加锁惯用法收敛为单一（lv_once 已保证初始化，删残留 g_data_mutex_initialized 手写标志）；singular_backend.c:388 绕开 groebner_lock_guard_init 直锁改为统一入口。
- **死锁检测（P2）**：调试构建给 lv_mutex_lock 加持有时间超时/告警钩子（复用 lvGuardStats）；CI 增加 TSAN 任务（现 build_san 仅 ASan/UBSan）；为 stream 回调重入、registry destroy 回调写并发回归测试。
- **合理隔离（保留但文档化顺序）**：thread_pool 队列锁/组锁顺序获取不嵌套（正确模式，补文档）、各后端 registry 锁独立、memory_pool per-pool 锁、stream async_mutex 单锁 + dispatch 锁外、high_dim _locked 分层——各自保留，仅缺书面总序。

### 3.70 宏约定类型转换面（第十四轮，K30）

**决策**：
- **宏规范锚定（P1，承接 K16/J5/K21 合并立项）**：删除 config.h:53 `lv_ARRAY_COUNT` → 全仓收敛 `lv_ARRAY_SIZE`（271 用）；X 列表统一 `*_X(x)` 命名（`*_ENTRY` 并入或显式标注"全字段族"）；守卫宏统一 `lv_XXX_H`；lv_PUBLIC_API ~30 处三定义收敛（config.h:643 无条件空定义删除，承接 K18/F45）；日志宏两族 LOG_* vs lv_LOG_* 锚定 lv_log.h；补宏 lint（clang-tidy readability + 自写脚本查无括号参数/多语句无 do-while/ARRAY_SIZE 唯一）。
- **强转检查（P1，与 K27 窄化合并立项）**：新增窄化辅助 lv_cast_size_to_int/lv_ASSERT_FITS_INT（DEBUG 断言 + release 饱和）；先清 upper 层 ~20 处 `(int) buf_size/(int) strlen` 直转；装箱拆箱收编 lv_box_index/lv_unbox_index（消 35 处手写）；lv_str_ltrim 加 const 版本（消 12 处 const 剥离，API 签名缺陷修复）。
- **X-macro 补全（P1）**：GeomType/ConstraintType 改 lv_XMACRO_ENUM 派生或把 _Static_assert 对齐铺开到全部 X 列表（现仅 5 处）；graph_serialize.c:320/324、lv_sema.c:104 硬编码计数 6/12 → lv_ARRAY_SIZE；algebra_mode.c:625-633 别名 switch → 引用 meta_repr 公共 API lv_geom_type_alias（LV_GEOM_TYPE_ENTRY ALIAS 列已存在，消除逐字重复）；prop_verifier_trust.c 手写 TRUST_* 表 → trust_color_x.h 生成或注释声明"中文描述另类数据"。
- **类型擦除收尾（P1）**：solver_order.c:36-102 手写 Kahn 迁移 lv_topo_run（兑现头注释承诺 lv_graph_traversal.h:159）；评估 lv_dyn_graph_topological_sort（geo_dynamic.c:858）/lv_graph_topological_sort 垫在 lv_topo_run 上（保留 ConstraintGraph 专用 API 作 wrapper）；normalization.c:1004 stable 变体语义不同豁免。
- **验证机制（P1）**：CI 加 _Static_assert 枚举对齐覆盖率检查 + 硬编码表计数 grep 护栏；计数三态（+1 技巧/lv_ARRAY_SIZE/硬编码）收敛为 lv_ARRAY_SIZE 单态。
- **分层保留**：LV_ 大写（编译期常量/配置/X 列表）vs lv_ 前缀（公共 API/工具宏/守卫）vs 无前缀 c 内模板宏（AABB_*/GA_* 局部参数化）命名分层——锚定规则即可；LV_GEOM_TYPE_X（2 列）vs LV_GEOM_TYPE_ENTRY（6 列全字段）信息量不同合理（声明主从并铺开 _Static_assert）；行为分发 switch（graph_node_emit/lambda_term）vs 名称查找职责不同；通用 int-id 遍历设施 + ConstraintGraph wrapper 两层 API；lv_MIN/lv_MAX/lv_CLAMP/lv_SWAP 唯一权威、42 个 void* 回调 typedef、LV_DISPATCH 安全分发（59 用）为健康范本；LV_SCOPE_LOCK/lv_LOCALTIME/LV_DISPATCH 双胞胎为文档化例外（登记豁免）。

### 3.71 字符串处理设施面（第十五轮，K31）

**决策**：
- **通配符收敛（P1）**：新建 `lv_str_glob_match` 入 lv_str_utils.h（纯函数，对拍 plugin_system_interface.c:187-218 迭代版与 preset_manager_query.c:40-68 递归版语义等价后收敛双 static）；test_framework.c:618-627 弱前缀匹配登记语义差异或并入。
- **lv_dstr 处置（P1，随 I3/F27 增长单一路由合并立项）**：lv_dstr 生产 0 调用——删或纯转发 lvStrBuf；修 lv_utils_misc.c:767-772 头注释"已收敛薄封装"与实现（grow/append 独立自写倍增）矛盾（M5）；lv_utils.h:1522 与 lv_strbuf.h:14-16 双头各自宣称"统一"改单头权威声明。
- **标识符扫描收编（P2）**：内部 8 处手写字符类判定收敛 `lv_str_scan_ident(p, extra_chars)`（字符集作参数吸收 module_lvz 续字符含 `- .`、axiom_pkg 仅 `_`、preset 校验含 `-` 等常量差异）；coq/lean 桥接外部契约豁免登记。
- **空串惯用法替换（P2）**：coq_bridge.c:151,268 / lean4_bridge.c:378,430 / mini_kernel.c:827 / interop_theorem.c:407 直写 strlen(x)==0 → lv_str_is_empty（机械替换 + 黑名单 grep）。
- **JSON 反转义双实现（P2，与 K35-2 合并）**：lv_json.c 简单转义表与 lv_str_json_unescape 收敛（\u 码点原语已共享，仅表双份）。
- **缺口清理（P2）**：lv_str_hex_encode 声明（lv_str_utils.h:296）/实现（lv_hash.c:94）错位归位；interop.c:42-55 死注释删除。
- **分层保留**：lvStrBuf/lv_asprintf/lv_fmt_tmp 所有权分层、lv_str_startswith vs lv_str_prefix_len 双语义、lv_str_icmp/icmp_n、formula_is_* ASCII 字符类、数字解析三原语、hex 单实现（位置错位仅归位）、JSON 转义写出已收敛（lv_json.c:713-721 委托）。
- **健康确认**：0 裸 sprintf/strcpy/strcat、strcmp 链=0（生产 6 处直接 strcmp 均合理：2 设施内部+3 排序比较器+1 动态输出）、strtok 单路由、lv_path_join 单实现——不重复报告。

### 3.72 状态机实现模式面（第十五轮，K32）

**决策**：
- **L1 合流连带件（P0，随 L1 批次执行不新增）**：① test_engine_ops.c:491-510「组 11」奇偶测试删除/改造为单一来源转移矩阵的完整覆盖测试（现它钉住两份实现）；② 4 处状态强制覆写直写（context.c:841/lv_convenience.c:151/engine_lifecycle.c:147，context.c:305 文档化除外）收敛为单一 `lv_force_state(obj,state,reason)` 原语并统一记录 previous_state（context.c:841 现丢审计链）；③ EngineState 合并后若仍零生产调用则删除（承接 L2）。
- **阶段完备性判定单源化（P1）**：orchestrator 暴露 `lv_session_stage_all_complete(session,&fail_idx)` 不变量判定函数；meta_verify.c ≥7 个手写循环（:134-153/:160-164/:167-181/:206-216/:219-227/:241-249/:183-187）改为调用；阶段结果补结构化字段（strategy_tried_count 等）替换 error_msg 文本 strstr 探测（meta_verify.c:168,233；test_orchestrator.c:56,58——改文案即破坏契约，K1 文案收敛批次需同步）。
- **证明进度状态二合一（P1，与 F8/F10 证明 IR 收敛合并）**：ProofState 死字段（proof.h:325 只写不读 + COMPLETED 死枚举值）接线或删除；证明会话生命周期统一到 lvSessionStatus；禁止半吊子。
- **状态机模板单一化（P2）**：新状态机一律"枚举 + handler 表 + LV_DISPATCH"（CDCL/WS 范本）；转移表用位掩码单一形态（context 为范本）；禁止新 if-chain 状态机（现状 7 处 if-chain/直写机器存量登记豁免）。
- **状态守卫原语（P2）**：`lv_state_guard` 家族统一"状态前置检查 + 统一错误文案"（收编 5+ 种手写形态：lv_CHECK_ARG/if+return/if+set_error/bool/直写；文案并入 K1 场景文案规范表）。
- **概念枚举桥接（P2）**：lvDeterminismState ↔ DeterminismState 二选一权威 + 映射函数或显式分层豁免登记；消除 NON_DETERMINISTIC/NONDETERMINISTIC 拼写漂移（control_flow_blocks.h:17 vs determinism_state.h:21，J5 同名异义家族）。
- **分层保留**：CDCL 10 态表驱动机、WS 帧 5 态表驱动机、熔断器 3 态（单实现+委托）、SystemState 进程生命周期（1-B 语义异构，TLS 问题已登记 K15 G7）、lvEditorState/lvTimerState/lvDynNodeState 不同对象域、DeterminismState X-macro 一源多视图、lvTaskGroup 别名、UI 状态枚举、状态机测试分层形态（hand-written + 奇偶测试，奇偶件随 L1 改造）。

### 3.73 文件 IO 资源管理模式面（第十五轮，K33）

**决策**：
- **I4 遗留闭环（P0，一次提交）**：① I4 指名的 3 文件 4 处裸 fopen → lv_file_open（proof_version_nl.c:79、proof_navigator_export.c:191/223、sat_encoding.c:314）+ 配套 4 处裸 fclose → lv_file_close（proof_version_nl.c:144、proof_navigator_export.c:215/252、sat_encoding.c:336），保留各自返回码约定仅换入口；② 5 处混合配对收口（module_export.c:140/191/244、interop_export_lean.c:313、interop_export_coq.c:382 的 lv_file_open+裸 fclose → lv_file_close）；③ lv_storage.c file:// 后端 open/close 改走 lv_file_open/lv_file_close（323-333/346-351，模式转换保留）+ file_size（397-408）委托 lv_file_size。
- **exempt 标注补全（P1）**：日志族裸 fopen（debug_emergency.c:49/debug_state.c:253/debug_trace_session.c:105/runtime_monitor.c:154,200）与 /proc 读（runtime_monitor.c:631/642/720）补显式 exempt 标注（I4 要求未实施）；interop_import_ggb_xml.c:823 已有 ✅。
- **黑名单 grep 落地（P1）**：裸 `fopen(` / 裸 `fclose(` 进黑名单 CI（I4 要求未执行，module_export 混合配对即黑名单缺失下的新漏网）；静默失败路径（proof_navigator_export.c:191/proof_version_nl.c:79 裸 fopen+静默 false 零诊断）消除或补 exempt。
- **文件大小收编（P1）**：debug_trace_session.c:111-112 / debug_state.c:264 / lv_storage file_size 收编 lv_file_size；测试侧 fseek+ftell>0 探测 ×5 保留（"断言非空"语义）或抽共享 helper（P2）。
- **临时文件命名（P1）**：lv_impl_upper_interop.c:71 内联 "lv_%s_%lld.tmp" 迁移 lv_temp_path 或随 K14"删临时文件+读回模式"一并删除（二选一）；graph_dot_export.c:281-282 "%s.tmp.dot" 保留补 exempt（graphviz 需同目录语义）。
- **低价值清理（P2）**：裸 remove/rename 14 处返回值规范（忽略处补"设计如此"注释——module_delta 轮转依赖文件可能不存在 / 或改 lv_path_remove）；module.c:9-16 死 include 清理；lv_file_read_text "rb" 读文本的 Windows \r\n 语义单点注释；lv_utils.c:581 fgets 固定缓冲截断注记；test_geo_visual.c:25-36 file_contains 只读前 4096 字节截断缺陷修复；lv_DEFER 延伸至 FILE* 资源（K13 关联项，量小）。
- **分层保留**：流式 fprintf vs 缓冲单次写双惯用法（两种输出策略）、二进制/文本分层 + PDF/LVZD/PNG 格式契约、fgets 行读 4 场景语义不同（INI//proc/stdin）、lv_storage 存储抽象层（mem/file 双后端）、lv_export_write_file 已登记豁免。
- **健康确认**：FILE* 静态配对检查未发现泄漏（34 处 lv_file_open + 18 处裸 fopen 全路径配对关闭含错误分支）、read-all 收敛完成、lv_dir_foreach 唯一实现、路径族收敛、lv_render_visitor_tikz 所有权交接范例——不重复报告。

### 3.74 命令/参数解析模式面（第十五轮，K34）

**决策**：
- **命令注册设施 + arity 校验（P0）**：以 interop `kCommandHandlers` 为蓝本建统一命令表（name/enum/handler/min_params/usage），覆盖 interop 19 命令、engine_scheduler 4 任务、geo_query 3 查询、do_export 3 格式、SMT 求解器名；10 处手写 Usage 样板（interop_command.c:331 等）由表驱动生成；**先修 G1/G2 两个语义缺陷再收敛**（以正确语义为唯一语义）。
- **G1 修复（P0）**：interop_command.c:381 AddNode Circle 改调 `graph_add_circle`（graph_add_circle 完整存在 constraint_graph.h:571）；补 AddNode Circle 执行级测试（断言节点类型为 GEOM_CIRCLE）。
- **G2 修复（P0）**：AddConstraint Parallel 改走 PARALLEL（ConstraintType 有枚举 constraint_graph.h:159），或明确登记"命令层暂不支持返回 UNSUPPORTED"而非静默 CONTAINMENT。
- **G3 修复（P0）**：命令层 int/double 参数改用严格解析 + 显式错误（不再 `_default(,0)` 静默回落）。
- **命令表收敛（P1）**：if-else 链 → 查表（geo_query/test_framework 报告格式/smt_backend_impl_external z3-cvc5/lv_impl_upper_app do_export 改调权威表，承接 K22 单源化 INTEROP_EXPORT_FORMAT_X）；lv_builtin_commands 27 项复用 interop 命令表或删除空壳执行层（K22 立项内顺带处置语义重叠）。
- **Python JSON-RPC 收敛（P1）**：统一 method→handler 字典表（可注册式），两桥（stream_bridge.py:1000-1042 / ws_server.py:214-230）各自注册方法集；错误响应骨架收敛到一个 helper（-32601 分支）。
- **空壳接线或撤除（P1）**：Solve/Rewrite/Unify 命令接线 engine_solve/engine_unify，或与 H2 同案（接线或撤掉命令名）；lv_proto_terminal_exec 空壳删除或接入 interop 表。
- **测试 CLI 补参数（P1，承接 K24）**：TEST_MAIN 支持 argv 过滤（--filter/--report-format）；test_new_modules.c:424-426 硬编码写 3 报告文件走统一入口。
- **低优先级（P2）**：scheduler 未知任务名改返回明确错误码（engine_scheduler.c:1194-1199 一行改动）；两 stream_bridge CLI 词汇统一或文档显式标注产品身份差异（N6 判断空间）；C15 系统信息解析双函数可合并。
- **分层保留**：interop 命令层内部收敛形态（样板）、lv_str_to_enum/lv_parse_* 解析底座、lv_ini_parse 单一 INI 解析器、TEST_MAIN 单一测试入口、Python 工具统一 argparse、C 侧 argv 解析需求本不存在、L9 5 命令 vs interop 21 命令协议分层（文档已定 §3.6）、命令失败冻结点快照回滚。
- **复核一致**：K22 命令枚举对齐（GET_NODE/GET_CONSTRAINT 死值、19 vs 21、"18 种"注释过期）、K22 命令名 6 套词汇表、K22 健康确认（strcmp 链=0/~50 name 表）——不重复立项。

### 3.75 编码/字符处理面（第十五轮，K35）

**决策**：
- **XML 转义收编 + 自转义缺陷修复（P0）**：module_export.c:70-83 手写 switch 版 module_export_xml_escape 删除，改调 `lv_str_escape_xml`（实体集逐字相同）；修 :133 自转义双写 bug（lv_strbuf_printf 追加语义误用——先转义再 printf 或删冗余转义，源为内部枚举名安全 ASCII）；test_module_ext.c:244 补 SVG <text> 内容断言（现只断言返回 bool 测不出）。
- **证明→LaTeX 转义补齐（P0，挂靠 E3/K14 既有立项）**：proof_export.c:309/342/346/350、proof_compiler.c:437-441/503-504/520-521/535-536、proof_navigator_export.c:206-208 原样写入路径统一补 `lv_str_latex_escape`；新增转义一致性回归测试（`_ % \` 会破坏 LaTeX 编译）。
- **JSON 反转义收敛（P1）**：lv_json_parse_string 第二遍解码改调 `lv_str_json_unescape`（定界子串），消 s_json_escape_steps/s_json_escape_decode 表与两遍骨架双份；test_json_buf.c:345-416 双路径测试保留作回归钉。
- **词法器骨架共享（P1，与 K27 数字预检项合并立项）**：module_lvz.c:63-160 与 axiom_pkg_parser.c:62-164 共享 token 扫描骨架（数字/标识符以回调或参数吸收差异；数字求值语义差异各自保留——只共享"扫描+分配"骨架）；标识符字符集差异并入 K31 lv_str_scan_ident。
- **Python 解码 helper（P1）**：新增 `_str_dec`（bytes/ptr→str）收编 ~25 处内联 .decode('utf-8')（core.py:323 等）；core.py:290 / stream_bridge.py:292 直连 encode 改调 `_str_enc`；ws_server.py:431 errors='replace' 为 HTTP 请求行宽容解码语义不同豁免登记。
- **决策项（P2）**：字符串字面量转义策略二选一并文档化（.lv 是否启用转义——.lv 只剥引号不解码 vs .lvz/.pkg 解码，格式契约差异需显式豁免登记或统一）；运行时 BOM 决策（接 parser_safety U+FEFF 表先接线 lv_input_sanitize，或 lv_loader 显式剥离/拒绝，二选一；C-㉖ 静态清库复核一致）；非法 UTF-8 校验决策（约定"UTF-8 无 BOM"故可能有意不校验，需确认登记）。
- **低价值（P2）**：high_dim_view.c:208/255-295 手写 JSON 写出迁移 lvJsonBuf（仅 1 调用点）；3 个同名 json_escape_string 局部包装（opml_codec.c:39/proof_session.c:50/preset_manager_serialize.c:42）统一命名或直调公共 API；lexer_shared s_escape_decode 5 项子集保留（DSL 字符串 vs JSON 语言契约差异）仅登记。
- **分层保留**：转义四族（JSON/HTML/XML/LaTeX 目标格式不同语义不同合理）、lv_dot_writer 7 消费统一、lvJsonParser 读端唯一、lv_str_hex_encode 统一（hex 解码单点 module_delta.c:252 粒度不足不立项）、ctype (unsigned char) 惯用法、coq/lean 外部契约豁免标注、Python 编码方向 _str_enc 收敛。
- **健康确认**：K23 JSON \uXXXX 代理对双路径均已接 lv_str_json_read_codepoint + test_json_buf.c:345-416 回归、C-㉖ 11 文件去 BOM 静态修复、转义族测试覆盖（test_utils.c:422-472/test_lv_export_common_ext.c/test_lv_dot_writer_ext.c）——复核一致不重复报告。

### 3.76 调试/诊断/追踪设施面（第十六轮，K36）

**决策**：
- **FATAL→紧急保存链路修复（P0，崩溃保护失效高危害）**：debug_trace_session.c:268 改传实际路径（如 `~/.lv/logs/lv_emergency_save.log`）并给 debug_emergency_save 补默认路径处理（现 :42 `if(!filepath) return false` 声称"使用默认路径"但无处理——M5）；或登记"FATAL 触发紧急保存"为未实现并删去；g_emergency_handler 补信号处理器接线或删 API。
- **lv_CHECK_RANGE 同名消歧（P0，随 K30 宏规范批次优先）**：error_codes.h:504 4 参数版 vs lv_check.h:139 3 参数版 #undef 覆盖（include 顺序敏感陷阱）——改名其一或统一参数序；lv_CHECK_* 双族合一（error_codes.h:439-511 vs lv_check.h:26-196，建议保留 lv_check.h 族为参数检查权威）；同文件混用点（tikz_export.c:198/238、node_graph.c:120/274）机械收编。
- **日志级别状态收敛（P1，承接 L8/K15）**：锚定 g_log_level（debug_state.c:69）为唯一事实源；删除第 5/6 容器（lv_impl_native.c g_debug_level int 裸类型、lv.c:802 s_lv_state.log_level 断链）；g_min_level/runtime_monitor min_level 降级为映射别名；词汇表 4 套收敛（lv_LOG_LEVEL_* 方向相反并入 K1 文案/命名）。
- **级别映射表 3→1（P1，随 L8）**：g_lvlog_level_map（lv_log.c:45-51）/kLogLevelMap（lv_utils_misc.c:359-370）/kLogLevelToLvLog（runtime_monitor.c:227-242）收敛单表单函数（FATAL 映射三处均降 ERROR 需裁决是否保留）。
- **lv_impl_native.c debug 族删除（P1）**：debug_trace/g_debug_level/debug_set_level/debug_get_level/debug_breakpoint/debug_dump（lv_impl_native.c:872-901）整族零调用零头声明——删除同时消灭第 5 个级别状态；需要门槛功能改 debug_set_log_level 既有开关。
- **lv_set_log_level 接线或移除（P1）**：lv.c:801-811 要么调 debug_set_log_level 真正生效，要么删除并标注废弃；同步清理 config "debug.*" 三只写键（lv.c:176-177,804,817——debug.trace_enabled/debug.assertions_enabled/debug.log_level 全部只写不读）。
- **执行 I2 已决"移除 trace_session"（P1）**：删除 debug_trace.c 全文件（TraceSession+死断点+debug_get_trace_session）+ s_debug_state.trace_session 字段（debug_internal.h:57）+ debug_log_shutdown:157-159 清理段；复核 K20"0 符号泄漏"计数口径（debug_trace.c 3 个无头声明非 static 函数出入）。
- **verbose 死开关清理（P1，随 K9 死开关批次）**：geo_constraint_solver_newton.c:269 空 if 块补实现或删字段；proof_compiler.c:648 只写字段删除；conflict_detector 活 verbose 保留。
- **日志行格式统一（P2）**：锚定 debug_log 完整时间戳格式（debug_trace_session.c:234）为唯一；lv_log.c:94-107 的 HH:MM:SS 前缀并入主管道开关；lv_log_message 的 [file:line] 进消息体问题随 L8 规范。
- **低价值（P2）**：lv_log_with_context/lv_LOG_CTX 生产零调用接线或标注测试专用；性能报告 4 套格式收敛（I2 统计分层落地时）；环境变量命名分裂（lv_MONITOR_THREADS vs LV_GROEBNER_PARALLEL）登记豁免或随 J5；solver_eliminate.c:23 重复 extern 声明删除。
- **分层保留**：debug_* 族 11 文件水平拆分、证明域追踪（lvProofTrace/lvProofTraceTree）vs 调试域追踪、proof_navigator 断点（生产活）、lv_event_trace→事件总线→Stream 桥接、lvLogRingBuffer→lvRingBuf 泛化、SchedulerStats/PerformanceCounters/lvPerfStats/lvGuardStats 统计分层、conflict_detector 活 verbose、runtime_monitor 统一 init_lock。
- **复核一致**：L5/L8/I2/J1/K8/K9/K11/K15/K20/K29/K30/K32 相关表述与代码现状一致（唯一出入：K20 计数口径与 debug_trace.c 无头声明函数）——不重复报告。

### 3.77 数学函数/数值原语实现面（第十六轮，K37）

**决策**：
- **几何向量原语收敛（P1，N1-N5 合并立项）**：lv_vec3.h 增 `lv_vec3_length/length2`（收编 geo_halfedge_mesh.c 6+ 处内联 sqrt(dot(v,v)) + geometry_transform_apply.c:323）；归一化三实现收敛（geometry_transform_apply.c:323-327 内联 1e-15 裸魔法数版改调 lv_normalize_3d）；叉积手写 ×2（parametric_curves.c:328-330/:383-385）改调 lv_vec3_cross；夹角计算骨架 ×3（geo_halfedge_mesh.c:815-825/:905-918 同文件两份 + meta_proof.c:174-179）收敛 `lv_vec3_angle`（meta_proof :179 内联 *180.0/lv_PI 改 lv_rad_to_deg）；geo_angle（geo_utils.c:253 死 API）激活（residual.c:188-189 内联 atan2 改调）或删除。
- **lv_number_is_integer 修复（P1，正确性隐患）**：lv_number.c:702-708 (int64_t) 截断对 |d|≥2^63 为 UB——改先判范围或用安全比较；有理数路径 to_double 近似判定改精确分母==1。
- **清理（P2，并入既有 K 系列）**：近似相等双函数（geo_approx_equal 生产 0 调用+eps 下限钳制——激活或删除，保留 lv_is_equal 权威）；interop_export_svg.c:157/:185 内联模长（≡geo_norm_sq_2d）收编；区间幽灵头 interval_arithmetic.h（生产零 include + 重复 lvInterval typedef）收口防潜伏冲突；lv_rad_to_deg 激活（8→全调用点）+ 死宏 lv_DEG_TO_RAD/lv_RAD_TO_DEG 删除（K12 延伸）；loader op_pow 第 4 种整数幂（lv_loader.c:640-646 饱和线性循环）登记决策（并入 K2 快速幂收敛）；lv_simd_dot_product_array vs serial_bicgstab_dot 并入 H3 对拍路线；梯形积分两实例（parametric_curves.c:178-211/346-393）抽公共或豁免；死 API 三件（lv_sign/lv_sign_int/geo_angle/geo_approx_equal）激活或删除。
- **分层保留**：超越函数全走 libm（无自实现）、幂按数域分层（GMP/int64/double/区间）、区间三语义（lv_interval_* 权威+空区间兼容层+AlgInterval）、lv_random_* 唯一随机源（K24）、ODE 单表+参数曲线回调求值收敛、geo_predicate EXACT/APPROX/ADAPTIVE 分层（K19）、lv_vec3 点积叉积权威+CSG 委托、kAngleTable 豁免（K12）、serial 点积保序（H3）、有理数 4→1（E8）。
- **健康确认**：K2 快速幂×3/平方因子×3、K12 数学常量、K19 容差分层——复核一致不重复报告。

### 3.78 时间/日期/时钟处理面（第十六轮，K38）

**决策**：
- **超时判断统一单调基座（P0）**：prop_verifier_engine.c:33-35 的 get_time_ms（墙钟 time(NULL) 伪毫秒）改 lv_get_time_ms（或 lv_get_wallclock_ms 并注释墙钟语义）；proof_version_sledge.c:121 超时改单调差分（CPU 时间 clock() 在 I/O/多线程下不推进——超时形同虚设）；沉淀单一"超时 elapsed"惯用法（如 lv_timeout_elapsed_us(start) 或复用 lvTimer）。
- **死超时接线或删除（P0）**：solver cdcl.time_ms（solver_core.h:161 声明无赋值，solver_core.c:1208 读取判超时恒不触发——补 lv_get_time_us() 差分赋值或删检查）；meta_proof timeout_ms 配置+setter（:493/:805）接线或删配置；lv_protocol last_solve_time_ms 恒 0.0 补计算路径或删字段；conflict_detector max_check_time_ms（:119）接线或删字段。
- **墙钟秒获取收敛（P1）**：新增 lv_get_wallclock_sec()（或迁移点直用 wallclock ms），收敛 8+ 处 time(NULL) 直用 + 2 种手写转换（time(NULL)*1000 伪毫秒 prop_verifier_context.c:37、time(NULL)*lv_US_PER_S debug_state.c:204）；删 PROP_TIME_MS_PER_SEC 双处定义（prop_verifier.c:69/prop_verifier_internal.h:44 改 lv_MS_PER_S）。
- **事件时间戳契约统一（P1）**：`timestamp_ms` 全库统一语义（建议单调，墙钟另立字段名）——C stream（单调）vs command log（墙钟）vs Python 模拟事件（墙钟 stream_bridge.py:300）三语义收敛；追踪事件时间戳 3 样收敛（proof_trace 墙钟秒 vs proof_trace_tree 单调 ns vs debug_trace.c:96 CPU 时间冒充——debug_trace 改单调）；plugin_system 事件墙钟 ns 对齐。
- **休眠封装收编（P1）**：runtime_monitor.c:615 Sleep/:641/:669 usleep + lv_process.c:164-165 裸 nanosleep 4 处改 lv_thread_sleep（纯机械）。
- **K8 收尾（P1，承接已登记决策）**：test_adaptive_threshold.c:439-465 手写 QPC+clock_gettime 双平台分支+裸 1000.0/1000000.0 改用基座。
- **低价值（P2）**：lvTimestamp/lv_timestamp_now（exact_arithmetic.h:14 单调 ns 冒充 Unix 时间戳命名+0 调用）删除或改 lv_get_wallclock_ns 语义并登记；adaptive_threshold.c timespec 绕道改 uint64 差分；simd 计时宏双处定义（simd_ops.c:155-156 vs simd_ops_internal.h:54-55）去重；async_stream.py 超时助手单源化（含 3.10 兼容，ws_server.py wait_for 直用改走它）；薄包装族 6+ 个（I2"4 个薄包装"计划实存 6+）统一改宏或直接调用。
- **分层保留**：单调/墙钟/CPU 三基座设施并存（lv_clock_elapsed_* 用于 CPU 耗时统计如 atp_backend.c:706/groebner_engine_ideal.c:319 合理）、stream_timestamp_ms 单源事件时间戳、时间单位常量族+lv_LOCALTIME 单源、lvTimer 唯一计时器对象（无 watchdog/periodic/deadline 重复）、PDF D:%Y%m%d%H%M%S 外部契约豁免（时间获取可走墙钟辅助）、Python 墙钟 TTL 缓存单实现。
- **健康确认**：基座有测试钉住（test_lv_utils.c:393-414 单调+量级+wallclock>0、test_thread_ext.c:58、test_runtime_monitor_ext.c:79-106）、proof_version.c:225-230 收敛先例、debug_state.c:200-205 已委托——不重复报告。

### 3.79 协议编解码/消息帧/校验面（第十六轮，K39）

**决策**：
- **LVZD 容器修复或删除（P0）**：geometry_compress_io.c + geometry_compress.h:18 二选一——`LVZD_HEADER_SIZE` 改 28 + 落实现真 Checksum；或砍重复第二个 size 字段（4+4+4+4=16 对齐）+ 读端 fread 后严格边界校验；test_geometry_core.c:1063 恒真断言（`write_ok==true||write_ok==false`）改为真实 round-trip 断言；**建议直接删（生产零调用）或修后进黑名单**。
- **AxiomPackage 内容哈希单源化（P1）**：axiom_package_compute_content_hash 为唯一权威（改 lvHashCtx 风格），compute_lemma_block_hash（axiom_pkg_depref.c:218-258）改为"权威函数+固定前缀/排除项"派生；删除 axiom_pkg_serialize.c:67 人工同步注释负担；字段集漂移风险随 K4 版本治理一并收敛。
- **interop 输入格式声称对齐（P1，与 K34 命令表治理联动）**：删"JSON-RPC 支持"声称（文本命令+文档标注）或输入侧接入 lvJsonParser 实现 JSON 分支（interop_parse_command :123-155 现仅空格分词）。
- **deserialize_clers 静默成功修复（P1）**：geometry_compress_decompress.c:48-52 长度字段非法（len<=0||len>剩余）改返回失败而非 return true 空序列。
- **协议编解码测试补齐（P1，承接 C1）**：WS 帧状态机/msgpack/LVZD/OPML 补专门单测（现仅 test_bytes.c/test_lv_utils.c:337 覆盖端序设施）；LVZD 恒真断言替换。
- **字节序手写收编（P2）**：png_write_be32（geo_visual_complete.c:1005-1010）→ lv_store_be32；WS 帧头编码（interop_server_ws.c:506-513 手写大端 vs 解码走 lv_load_be16/64 同文件不对称）→ lv_store_be16/64。
- **snprintf JSON 手拼收编（P2）**：interop_command_export.c:243/247、interop_server.c:477-480 → lvJsonBuf（同层 30+ 处已统一）。
- **hex 解码补权威（P2）**：新增 lv_str_hex_decode，收编 module_delta.c:252-276（u64_to_hash_string 的 lv_snprintf("%016llx") 绕过 + hash_string_to_u64 手写）；interop_theorem.c:300-330 的 *31 哈希标注"block_id 派生非完整性"。
- **豁免标注（P2，随 K22 黑名单机制）**：SHA-1/base64（interop_server_ws.c:82-213 WS 握手 RFC 契约）补"外部契约豁免"；PNG CRC-32（geo_visual_complete.c:972-1002 全库唯一 CRC）登记为唯一实现——如 LVZD Checksum 落地抽公共 lv_crc32。
- **低价值（P2）**：STDIO 帧加长行累积（对齐 WS 上限语义）或文档化 4096 上限（interop_server.c:620-632）；INTEROP_CMD_BUFFER_SIZE 双定义（config.h:822-823 vs interop.h:62-63）收敛；varint/zigzag（lv_bytes.c:144-324）与 lv_BSWAP*（cross_platform.h:372-376）零消费者裁决删除或登记预留。
- **分层保留**：lv_store/lv_load + lvByteWriter/Reader + msgpack 唯一权威底座、不同传输协议（WS/JSON-RPC/JSONL/SSE/文本——L4/K34 已定分层）、LVZD/LVZC 双层容器与 magic 探测、ggb deflate/PNG/ZIP/OPML 外部契约豁免（单一实现边界检查严谨，ggb 为范本）、三态心跳（RFC 控制帧/应用层方法/SSE 注释）、响应/通知消息类别（interop 信封 vs stream.event notification）、SHA-1 单一实现。
- **复核一致**：L4 三套事件→JSON 序列化、K34 Python JSON-RPC if-elif ×2+双 CLI+Usage 样板+lv_proto_terminal_exec 空壳、K4 msgpack/JSON 无 format_version+LVZ 版本 3 事实源+LVZD minor 忽略——全部确认仍在代码中，不重复立项。

### 3.80 构建系统组织面（第十六轮，K40）

**决策**：
- **聚合清单单源化（P1）**：`set(lv_AGGREGATE_OBJECTS ...)` 变量或 foreach 拼接生成 27 项 $<TARGET_OBJECTS:...>，lv_static（1638-1667）与 lv_shared（1689-1718）共用单一清单——消灭 27×2 逐字复制，新增/移动 OBJECT 库只改一处。
- **依赖三元组变量化（P1）**：`lv_LAYER45_DEPS "${lv_L4_LIBS} lv_layer3_geometry lv_layer2_resource"` 收敛 3 处（1543/1602/1617）；验证 L4 宏传递依赖后精简冗余。
- **外部依赖 interface 库（P1）**：`lv_platform_libs` INTERFACE 库（GMP + Threads + 平台条件 ws2_32/m）一次声明；15 处 target_link_libraries 收敛到宏/interface；可执行程序只链 lv（消除测试/fuzz/example 4 处冗余 GMP 链接，lv_static 已 PUBLIC GMP）。
- **lv_l4_func_block 回归宏（P1）**：1511-1515 手写展开改 `lv_l4_subdomain(lv_l4_func_block ${lv_L4_FUNC_BLOCK_SOURCES})` + 追加 compile_definitions（lv_PRESETS_DIR）——消除宏 vs 手写双实现。
- **include 目录集变量化（P1）**：宏（1442-1448 5 路径）与聚合库（1668-1673/1719-1724 3 路径）共用变量或 lv_includes INTERFACE 库；决策聚合库是否补 core/src（当前依赖恰好全走 core/include/lv 属侥幸 G2）。
- **优化选项按配置限定（P1）**：lv_core 无条件 -O2（1474-1480）改生成器表达式按 build type（Debug/coverage 保持 -O0——断点失效/coverage 失真真实缺口）。
- **低价值（P2）**：抽 `add_lv_program` 公共函数（add_lv_test 1762-1771 / add_lv_example 2217-2224 body 重复，example 补 WIN32 ws2_32 分支）；头清单单一权威（lv_HEADERS 222 个 vs 磁盘 305 个——file(GLOB) 或全覆盖手写+漂移校验，83 个 .h 归位，L6/L8/L10 局部清单决策）；测试注册命名统一（显式 CTEST_NAME ~273 vs 缺省 ~15）+ 修 docs:1028（add_lv_test_and_register → add_lv_test_auto）；编译器分支集中（8 处 ID 判断收敛一块）+ 新增 MSVC CI job 验证 113-139 分支（现 windows-build 用 MSYS2 MinGW，/W4 /guard:cf /sdl 从未验证）；CI 语法检查改用 compile_commands.json（CMakeLists.txt:8 已开启但未消费，ci.yml:121-131 硬编码 -I 双份）；"磁盘 vs CMakeLists 清单"CI 校验（新增 .c/.h 未入清单即失败，防再漂移）。
- **分层保留**：测试注册 288/288 单一化（add_lv_test_auto 正面范式）、lv_PROP_VERIFIER_SOURCES 单源化、L4 子域宏嵌套复用（主体）、lv_L4_LIBS 聚合变量、全手写源清单风格统一（无 GLOB 混用）、27 个 OBJECT 库分层+$<TARGET_OBJECTS> 聚合思路本身、euclidean 内部包含模式、lv ALIAS 链、CI 平台矩阵分 job（MinGW/MSYS2/Clang）。
- **复核一致**：J4 孤儿 56 个 preset_*.c 实测吻合、K17 fuzz 旗标复制+sanitizer 分叉、K18 find_package 必坏、E15 无 CMakePresets（9 个 build* 两套命名）、I5 L1 补链 1538、K27 窄化警告不一致、J3 CI 内联报告、J 系列 fix_build.py≡fix_cmake.ps1、K9 option 前缀混用、K21 无 format target——不重复立项。

### 3.81 几何判定/谓词实现面（第十七轮，K41）

**决策**：
- **新增 lv_perpendicular/lv_parallel 浮点谓词（P0，承接 K19 P0 项）**：geo_predicate.c 内新增两谓词，激活 lvGeometryConfig.perpendicular_epsilon/parallel_epsilon 两个零消费者字段；solver_geom_templates.c:297/:412、algebra_mode.c:592-614、geo_constraint_solver_residual.c:145/:161 改引用（residual 残差语义可豁免登记但容差引用权威）。
- **垂直约束接线修复（P0 正确性）**：新增 graph_add_perpendicular（现全库不存在）；formula_converter_constraint.c:59 的 betweenness 占位（垂直=点在两点之间语义完全错误）改真实类型；interop_command.c:592-593 的 CONTAINMENT 占位改 graph_add_parallel/新 API——消除"平行/垂直约束被降级为占位"的语义丢失链（solver/conflict 下游按错误类型解释）。
- **meta_proof 平行/垂直补坐标级验证（P0）**：meta_proof.c:361 [PARALLEL]=eval_default（恒 false="不矛盾"——L1 直接矛盾证明对 PARALLEL/PERPENDICULAR 不验证）改为委托新谓词。
- **meta_proof 收敛委托权威（P1）**：check_incidence_contradiction(:127) 与 is_point_between_segment(:188) 两处手写叉积 → lv_segment_side/lv_orientation_2d；point_on_line_segment(:227)/point_on_circle(:249) → lv_segment_side/lv_side_of_circle；消除 1e-10/1e-9/1e-8 三容差并存。
- **algebra_mode 硬编码委托（P1）**：selector_node_contains(:648-680) 硬编码 1e-6/1e-12 → 谓词层；消除点在圆上 3 套。
- **recursion_selector 收敛（P1）**：point_on_segment_symbolic 硬编码 1e-10 → 命名常量；compute_winding_number 卷绕数与射线法主从关系明确（豁免标注或统一）。
- **方位角收敛（P1，K37 N 系列延续）**：激活 geo_angle 或新增 lv_vec2_angle，收编 residual.c:188/recursion_selector.c:100 两处 atan2 内联 + meta_proof.c:179 *180.0/lv_PI 改 lv_rad_to_deg。
- **低价值（P2）**：conflict_detector.c:488 eps×1000 魔数缩放 → 具名常量+注释（或改权威容差）；graph_conflict.c:567 segments_can_intersect 空壳（恒 true 注释"for now assume"）实现或登记未完成桩；interop 求交（interop_export.c:133+svg:152/181）容差 lv_EPSILON_HIGH → distance_epsilon 判定部分委托；新距离 API lv_point_segment_distance2 或对 geo_event_detect/simd/residual 三份距离实现豁免登记；ga_interface 共线在 GA 域内豁免或委托 2D 权威。
- **分层保留**：EXACT/APPROX/ADAPTIVE 三层谓词（K19）、符号层 symbolic_coord_are_collinear vs 浮点层 lv_orientation_2d（E8-E10）、编码层三态（GMP 方程/groebner 多项式/SAT 布尔）、求解器残差 eval_*（优化目标语义≠布尔判定）、2D vs 3D 域、SIMD 性能域、已收敛委托链 6 条。
- **复核一致**：K19 容差分层/共线 7 值、K37 向量原语（geo_angle 死 API）、E8-E10 坐标代数——不重复报告。

### 3.82 网络/套接字 IO 模式面（第十七轮，K42）

**决策**：
- **network_block 裁决（P0，收敛前置）**：删除（当前唯一 C 网络需求是 interop 服务器）或接线（则必须并入 socket 平台抽象层统一）——死代码是 R1-R5 全部重复点的第二载体；io_blocks.h:67-68 仅保留 create/destroy 则删除其余无头声明函数。
- **socket 平台抽象层（P1）**：新建 `core/include/lv/lv_socket.h`：句柄 typedef（统一 WsSock/lvNetSocket/裸 SOCKET 三套）、invalid 判定、close、`lv_socket_last_error()`（WSAGetLastError/errno 映射，消 3 处手写）、recv/send 包装（统一 EINTR 语义）；消费方：interop_server.c start/stop、interop_server_ws.c 包装、network_block.c（若保留）；平台判定单点化（消 9 处 3 风格分支）。
- **lv_socket_send_all 收敛（P1）**：ws_sock_send_all（不处理 EINTR——POSIX 信号打断整帧失败连接被当错关闭）与 network_block send 循环（处理 EINTR 但不防 n==0）合一，统一 EINTR 重试与 0 字节防护语义。
- **SO_REUSEADDR 对齐（P1）**：Winsock 分支补 setsockopt（interop_server.c:297-330 缺 vs POSIX :347 有——服务器重启 TIME_WAIT 场景可能 bind 失败）或文档化差异登记豁免。
- **WSAStartup 引用计数单点化（P1）**：interop_server 无计数 WSACleanup（若与 network_block 全局计数同接线可能提前拆除 Winsock）收敛到单一引用计数管理器。
- **socket 路径测试补齐（P1）**：start/stop 双平台、send_all、断开检测——解除 test_interop_ext.c:5-6 的"socket 阻塞循环登记遗留"。
- **低价值（P2）**：平台分支风格统一（#ifdef _WIN32 → 统一宏，K9 迁移范畴；#if/#else 假定 else=POSIX 改显式三态或收敛进抽象层）；network_block recv 文档/实现对齐（单次 vs 循环、EOF 语义、EINTR 重试次数）；超时策略登记（阻塞 socket+select 为唯一超时机制，SO_RCVTIMEO/SO_SNDTIMEO 零使用；接线 network_block 前先定 connect/recv 超时方案）；删 interop_server.c:40,43 死定义（INTEROP_SELECT_TIMEOUT_* 归一 interop_server_internal.h——"移走未删净"）。
- **分层保留**：WS 帧状态机+socket 侧缓冲（唯一实现，K39 已登记）、select 单线程 16 槽服务器模型、lv_process poll（I1 范畴）、Python websockets/SSE/FastAPI（L4 已登记跨语言栈）、file/network block API 对称（共享 lvIOBlockState 底座）、WS 断开检测（协议角色）。
- **复核一致**：I1 外部进程调用、L4 WS 双栈、K39 帧/字节序/缓冲上限、K32 WS 表驱动范本、K9 _WIN32 迁移——不重复立项。

### 3.83 对象池/资源池/缓冲复用面（第十七轮，K43）

**决策**：
- **coeff_pool 尺寸失配修复（P0，真实缺陷候选）**：coeff_pool_alloc 池路径按请求元素数校验（count>8 时强制走回退分配，或池块按请求对齐）；symbolic_coord_transform.c:158/:1001 两处可变尺寸调用点补测试（构造 deg≥4 minimal_poly 断言无越界）——消除池路径忽略 count 恒返回 8 元素 vs 回退路径按 count 分配的双路径尺寸失配（mpz_init/mpz_clear 越界读写池内相邻块/越出 malloc 区域）。
- **两套通用池互委托（P0，随 I3/§3.33 + K26）**：lvMemPool/lvObjectPool 二选一（建议 lvObjectPool 为唯一"归还式对象池"功能超集，lvMemPool 以"无锁有界"特例参数表达或删）；同时消灭 MemPool 三壳（删 lv_impl_native.c:842-866 pool_* 死包装——typedef lvArena MemPool 无头声明零调用；mem_pool_*/lv_mempool_* 合并单入口）；内部块统一走 vtable 可注入路径（修 K26 memory_pool 原生 malloc 盲区）。
- **g_coeff_pool 生命周期接线（P1）**：lv_mempool_static_destroy 接入 lv_cleanup 或模块注册表（现全库零调用永不销毁）；lv_mempool_static_init 内建 lv_once（承接 K15 G5 TOCTOU）。
- **预设池按需化（P1）**：删或懒创建 SymbolicCoord/ProofStep 池（lv_init_preset_pools 4 建 2 用，零生产消费的固定预分配消除——4×1024 对象起步含大结构固定开销）。
- **线程安全语义统一（P1）**：lvMemPool 补 thread_safe 选项或文档化"单线程限定"，与 lvObjectPool/lvArena 能力对齐。
- **低价值（P2）**：临时缓冲契约分层文档化（覆盖式 lv_scratch vs 配对式 formula_pool）+ formula_pool 修"同尺寸匹配"注释与实现（:77 vs :87-93 首循环不比较 size 全槽恒 1024B）+ 补 TLS 清理（lv.c:360 注释自认遗留）；id_map 共享工具（dsl_compiler_load/func_block_instantiate/module_lvz/graph_node_vtable/graph_node_copy/beta_reduce/solver_groebner 7 处重映射表 → lv_id_map_alloc/init/free，随 I3 容器族）；池级统计随 L6 单一内存统计方案（lv_pool_get_stats/mem_pool_stats/lvArena 统计三选一，当前均零生产消费）；槽位池样板登记（network_block/high_dim_view/formula_pool 三处 in_use 槽位数组——第 4 处再抽 lvSlotPool）；ID 计数器形态规范（int/atomic/TLS 选择规则承接 J2）+ "池满策略"决策点（增长/耗尽回退/回退堆三选一登记）。
- **分层保留**：lvArena/lvMemPool/lvObjectPool 分配器家族分层本身合理（bump 一次性释放 vs 归还式固定块 vs 归还式对象池；lvArena 生产零消费需补"接线或标注待接线"决策）；lv_scratch 覆盖式 vs formula_pool 配对式契约分层；图遍历每调用工作区（lv_bfs_run 共享设施）；dirty_set 已收敛转发；expr_canon 合并桶文档化不迁移；geometry_transform 不用池决策（临时对象生命周期短大小多变）；groebner_engine 领域 ID 注册池（有锁自洽）；局部缓冲复用（ode_solver 4 槽环形历史/solver_core conflict_clause 复用/stream_async realloc 复用）。
- **复核一致**：I3 内存池 2 套互不委托（新增证据分配底 lv_calloc vs 原生 malloc）、K26 memory_pool 绕过注入、K15 G5 g_coeff_pool TOCTOU、L6 lvMemoryStats 零调用（池级统计同零消费）——不重复报告。

### 3.84 配置项定义/默认值/校验模式面（第十七轮，K44）

**决策**：
- **默认值单一事实源落地（P0，承接 G5 主决策）**：A 表为运行时默认唯一权威；compat 段 50 宏改为派生/删除（#define X lv_CONFIG_TABLE_X 或整段入黑名单，承接 K30）；模块回退宏删除或注释指向（BUCHBERGER_MAX_STEPS_DEFAULT/GROEBNER_REDUCE_MAX_STEPS_DEFAULT——A 同名键恒命中回退值永不生效纯重复源）；lv_init B 种子键删除或改 A 拼写（solver.max_iterations 零读取 vs solver_max_iterations 字段实际读取 3 拼写分裂）；lvSessionConfig 默认改读全局（承接 G5 原文决策）。
- **lvConfig 读路径二选一（P0）**：锚定字符串键公共面（lv.h:658-727）；107 个类型安全 getter 接线或降级登记为 test-only（生产零调用）；lv_config_load_json 接入 lv_init 或明确"测试专用"——解决"A 权威注册表 107 键生产仅读 ~13 字段、JSON 持久化仅测试覆盖"的接线薄弱。
- **改名消歧（P1）**：lvSolverConfig 双定义（geo_constraint_solver.h:98 vs solver_core.h:171 同名不同类型潜伏编译地雷）改 lvGeoSolverConfig/lvCdclSolverConfig + 默认函数近名（lv_solver_default_config vs lv_solver_config_default）+ 登记黑名单。
- **映射块抽公共函数（P1）**：lvApplicationConfig→lvSessionConfig 字段映射（lv_impl_upper_app.c:106-116 与 :143-152 逐字重复）→ lv_session_config_from_app()。
- **GROEBNER_ZERO_THRESHOLD 统一（P1）**：groebner_engine.c:662/:699 直接用宏改 lv_config_get_double（:272/:297 已走配置读——同文件双读路径配置覆盖只影响一半）。
- **LV_CFG 死键清理（P1）**：5 个死键（GROEBNER_SOLVE_MAX_ITER/GROEBNER_DEFAULT_VAR_CAPACITY/GROEBNER_SMT_ZERO_THRESHOLD/SMT_DEFAULT_TIMEOUT_MS/AABB_INITIAL_CAPACITY）+ 伴生死宏接线或删除（K19 零消费者模式在注册表层新实例）。
- **配置键单源表 + 文档对拍机制（P1，承接 K3/K22）**：修 API_REFERENCE.md 幻影键列表（5 键中 4 幻影）、23_core_infrastructure.md 宏名表整份漂移（K9 幽灵宏扩展）、键名格式规范（文档"模块.参数" vs A 下划线 vs B 点号三分裂）、真实 107+35 键文档列表；对拍/生成机制防再漂移。
- **setter 校验语义锚定（P1）**：setter 范围校验+错误码或明确"静默接受"契约并修 lv.h:695 文档声称（现声称"值超出范围返回 false"但实现无范围校验，M5）；锚定一种校验语义（建议 setter 范围校验或显式契约）。
- **低价值（P2）**：lvGroebnerConfig.enable_cache 只写不读接线或删（groebner_parallel.c:722）；Python EngineConfig 死配置删除或接线 C 配置绑定（_ctypes_binding 无 lv_config_* 绑定跨语言通道不存在）；B 种子 5 键零读取清理（debug.* 三键承接 K36）。
- **分层保留**：lvGeometryConfig 独立子系统（K15 ✅）、lvPluginConfig per-instance（A 无法表达按实例键空间）、模块局部算法参数（ODE/traversal/render 类纯局部语义）、lv_env_get_* 环境变量设施（K16 ✅）、LV_CFG 键名单源（K22 ✅）、A 优先/B 回落分发机制、global_state 维持 G5 删除决策。
- **复核一致**：G5 会话默认覆盖全局/JSON-INI 双持久化/global_state 死代码、K19 几何 3 字段零消费者、K22 LV_CFG 单源、K25 路径、K36 debug.* 只写键、K16 getenv 收敛、K9 lv_CONFIG_RUNTIME_GUARD_* 幽灵宏——不重复报告。

### 3.85 API 参数校验/契约模式面（第十七轮，K45）

**决策**：
- **校验宏族调用侧收敛（P0，承接 K36 双族合一的落地清单）**：给 lv_check.h 族（B 族）补"返回值定制"参数（吸收 error_codes.h A 族 ret 语义，适配任意返回型——B 族占 20% 的原因是硬编码 -1 不匹配任意返回型）；机械迁移 ~25 个 A 族活跃文件 + geometric_primitives.c:40-49 私有宏（GeoResult 结构体返回需宏支持或该文件登记豁免）+ 同文件混用（tikz_export.c:198/:238、node_graph.c:120/:274）+ bdd_encoding include/使用错配；删除 8 个 0 调用死宏（lv_CHECK_STATE/lv_CHECK_BOUNDS/lv_CHECK_RANGE(3p)/lv_PROPAGATE 等）。
- **graph 域失败语义统一（P0）**：graph_add_point/graph_add_* 的 NULL 路径不再映射 ADD_*_CONFLICT（改 NULL 参数语义或状态码——消除"NULL 图=几何冲突"误导性诊断链，interop_command.c:306 连带受益）；graph_get_node_count/graph_get_constraint 补齐消息或显式文档化静默；graph_deactivate_constraint 补消息；graph_index.c 相邻 4 getter 锚定唯一失败语义（建议 NULL→lv_ERROR_NULL_POINTER+消息与 graph_get_node 对齐）。
- **头契约与行为契约对齐（P0）**：context.h/constraint_graph.h/circuit_breaker.h 二选一——文档化 NULL 容忍（"NULL 安全返回默认值"）或统一错误化；修正被测试钉死的 getter 默认值不一致（get_max_depth→DEFAULT vs get_max_steps→0 vs get_name→"null" 魔法串 test_context.c:102）。
- **失败约定契约表（P1，并入 K1 文案规范表）**：按返回型家族定义 NULL 失败约定（错误码型→lv_ERROR_NULL_POINTER；指针型→NULL+错误 ctx；bool→false；void→静默+日志），作为 graph 域统一的判定基准。
- **契约断言接线或删除（P1）**：lv_ASSERT_RUNTIME/lv_verify_data_integrity（runtime_guard.c:148 198 行实现）二选一——接入调试构建开关（真实检查）+ 至少 1 个调用点/测试，或随 K9 死开关批次整体删除（避免"写了 198 行永不运行"的死实现，现受 lv_ENABLE_RUNTIME_GUARDS 全仓无定义保护真实实现永不编译生效恒 true 桩）。
- **契约测试补强（P1）**：非法参数测试补"具体错误码断言"（16/288 → 全覆盖）+ "错误消息已记录"断言；graph_add_point(NULL) 测试改钉具体语义（现"任意非 OK 即 PASS"钉住误导性 CONFLICT）；静默默认值约定（graph_get_node_count→0 等）补显式契约测试防漂移。
- **低价值（P2）**：消息语言统一（承接 K1——手写英文校验消息迁移规范文案；B 族双记录日志+ctx 行为文档化）；半界/缺失校验修正（lean4_bridge.c:84 补上界 lv_CHECK_ENUM 或等价、graph_add_angle 补角度范围契约+校验）；分层原则书面化（"公共入口必校验/内部热路径免校验/双层公共 API 防御性重复校验"登记为治理原则）。
- **分层保留**：lv_parse_utils/lv_str_utils 契约文档+守卫一致范本（应推广）、测试用 lv_ASSERT_* 与生产检查设施分离（0 测试文件用生产宏）、解析器结果结构体错误通道（域内差异）、module.c 三元防 NULL 自洽、双层公共 API 防御性重复校验（geo facade→graph API 分层防御）、内部热路径免校验/私有 helper 静默（性能分层）。
- **复核一致**：K36 lv_CHECK 宏双族/同文件混用、K26 NULL 参数覆盖数百处（246/288 测试文件 ✅）、K10 所有权契约、K20 声明一致性、K1 文案规范——不重复报告。

### 3.86 哈希表/查找设施实现面（第十八轮，K46）

**决策**：
- **预设枚举名单源化（P1，R10 最高价值，承接 K22/K30）**：lv_impl_upper_preset.c:113-181 三张手写表（4/12/6 项）改由 preset_category.h 宏生成（或删/改委托 func_block_preset_query.c 侧）；upper_name_lookup 手写二分删除改用 lv_enum_to_str；补全 25/16/7 全项——修复"UI 查询高级类别得 UNKNOWN"真实行为缺口。
- **图哈希家族收敛（P1，承接 K2）**：删 compute_quick_graph_hash 死代码（graph_hash.c:150）；compute_complete_graph_hash（64 位）与 compute_graph_hash（32 位）标注互斥语义或抽"图指纹公共骨架"（节点/约束 id+type+坐标回调式混入，用 lv_fnv1a_mix_u64）；WL 图核哈希保留（不同算法）。
- **坐标哈希统一入口（P1）**：unify_helpers/normalization/module_delta（同文件复制 ×2）三处节点坐标哈希聚合收敛单一入口（如 lv_fnv1a_mix_u64 设施 + 统一 coord-hash 聚合）；修复 graph_node_hash.c:313 注释记录过的 Knuth 乘数假阴性事故同类隐患。
- **黄金比收编（P1，承接 K12 延伸）**：module_delta.c:420/:643 裸 0x9e3779b9ULL（同函数宏/裸混用）与 unify_helpers.c:260-261 裸 0x9E3779B97F4A7C15ULL（:261 "+1 变体"补宏）→ 权威 lv_HASH_GOLDEN_RATIO_32/64。
- **低价值（P2）**：R3 "哈希+线性回退"样板 ×7 登记/抽辅助（并入 K30/K43 id_map 工具）；R4 graph_node_hash 双删除算法统一或豁免标注（实施时确认字段类型是否可共享）；R6 str 形态取模→位掩码（保留取模防御分支注释）；R7 死代码清理（并入 K2）；R8 查找失败语义登记（NULL/-1/哨兵三表达并入 K7 失败约定）；R9 lv_next_pow2 抽取（并入 I3/K27 增长逻辑）。
- **分层保留**：lv_hashtable 三形态（I3，int/i64 开放寻址+墓碑 vs str 分离链式语义不同）、WL 图核哈希（算法不同）、symbolic_coord_hash 权威（类型表分发）、formula_hash 族（自洽单实现）、lv_hash 内容哈希抽象（SHA-256/FNV-1a 双算法统一上下文）、expr_canon/fast_index/bdd_encoding 豁免标注完备（三次收敛评估注释）、静态小表线性查找（K22 健康基线）、插件/后端/模块注册表多份（K15/K29/H1 已登记范畴）。
- **复核一致**：I3 lvHashtable 三形态（含 3 处完备豁免）、K2 FNV 双家族、K15 lvRegistry 加锁/无锁、K30 lvStrToEnumEntry+lv_enum_to_str 共享设施、K22 name 表健康（R10 为漏网延伸）、G2 缓存双实现、K39 AxiomPackage 哈希双实现——不重复报告。

### 3.87 树形结构/遍历模式面（第十八轮，K47）

**决策**：
- **T1 lv_tree_traverse 接线或删除（P0，消除 M6）**：通用树遍历设施（graph_traversal_tree.c:28-176 显式栈 TreeFrame+BFS 队列完整实现）接线至少 1 个真实消费方（csg 求值遍历、AST 求值族可通过 get_children 适配）或标注删除——头注释 lv_graph_traversal.h:3-6 宣称"统一遍历抽象消除各模块重复"但生产 0 调用。
- **T3 证明树去重（P0/P1，随 P2/F10 证明 IR 收敛）**：lvProofTree/ProofSearchTree（测试专用两树）删或接线二选一（建议删——主证明树已由 lvProofTraceTree 承担）；proof_widget_get_search_tree/proof_widget_get_dependency_graph 两桩（proof_widget.c:507/:520 硬编码 JSON 恒空骨架）补实现（接入 lvProofTraceTree）或明确标注未接线；lv_proto_tree 确认 Python/GUI 桥接是否消费。
- **T2 树销毁收敛（P1，与 K28 destroy 域合并）**：lv_trace_node_destroy/proof_dependency_destroy/csg_node_destroy/csg_bsp_node_destroy 4 个手写后序递归接入 lv_tree_release_recursive（get_children+cleanup 回调适配，trace_node"不释放 proposition/step/rule"所有权特例用 cleanup 表达）；与 K28 destroy 域深度补限/显式栈合并执行——消除深树爆栈风险。
- **T4 导出骨架共享（P2，并入 K14）**：证明树→DOT/JSON 导出共享骨架（递归 vs 平铺 all_nodes 两形态，字段/着色由回调提供）。
- **低价值（P2）**：T5 抽 lv_proof_color_merge 原语（proof_navigator.c:45 平铺 vs proof_navigator_dependency.c:78 递归叠加规则逐段同构）；T10 FormulaNode 分发表由 FORMULA_NODE_FIELDS_X 派生（收编 eval/render/to_string 手工枚举，修复 eval 24/36 漏项静默 0.0——K7 降级登记关联）；T6 树深/叶子计数维持现状登记豁免；T9 PropFormula 登记为第 4 公式树豁免（纯命题 7 节点 vs FormulaNode 36 节点语义不同）。
- **分层保留**：T7 树路径查找 vs 祖先链（语义不同已有评估注释 proof_trace_tree.c:8-29）、T9 PropFormula 第 4 公式树、BDD 引用计数 DAG（DAG 语义不同）、TypeRegion 字段清单树（单事实源）、Huffman 显式栈（已收敛 3 拷贝）、AABB 2D/3D 宏泛型+深度 64（K28 B14）、lv_loader AST 求值族（LV_DISPATCH 表驱动同树不同语义）、lv_tree_release_recursive 3 消费方（正确样板）、K28 显式栈样板族。
- **复核一致**：K28 显式栈样板/递归补限、F1/F2 表达式树、F3 字符串化、P2 证明内存树、G3 图遍历（非树跳过）——不重复报告。

### 3.88 符号表/名称绑定/作用域面（第十八轮，K48）

**决策**：
- **D4 量词绑定解绑修复（P0）**：check_expr_quantifier（lv_sema.c:410-430）body 检查后解绑（或引入 push/pop 绑定对）；补测试钉住"量词变量不出作用域"（现无任何测试覆盖该行为）——消除 forall x 之后顶层 Point x 误报 duplicate declaration 的泄漏。
- **D1 name→ID 统一设施（P0，随 F7/K43/I3 合并）**：以 lv_registry 为基座扩展整型值形态或抽 lvNameTable（name→value 唯一设施），迁移 mini_kernel（symbol_names+symbol_index）/dsl_compiler_ir（symbols+symbol_to_ir_id）"平行数组+哈希索引+线性回退"骨架（含"值存 index+1 避开 NULL"技巧）；预设三容器随 F7 收敛。
- **D2 关键字/类型名单源化（P1）**：s_keywords（lv_lexer.c:64-107）从 LV_ENTITY_TYPE_X+LV_TOKEN_TYPE_X 派生或加词面 _Static_assert；s_is_entity_type_tokens/s_statement_start_tokens（lv_parser.c:155-168/:127-152）改 X-macro 生成；LV_SEMANTIC_TYPE_X（lv_sema.c:82-94）复用实体名字列——消除 12 个实体名散落 5 处、新增关键字需改 4-5 处。
- **D3 重定义语义统一（P1）**：.lv 系（sema/loader/DSL IR）收敛单一策略（建议报错拒绝）；登记跨域三选一策略表（报错/保首/更新）——当前 sema 报错、loader 保首、DSL IR 追加是同一语言家族内行为分裂，Metamath 重绑 vs .lv 报错语义相反。
- **D5 .lv 文本链路定性（P1，承接 K3）**：sema+loader 文本链路（全库生产零调用仅 test_lv_bootstrap 12 处）正式标注"自举/测试专用"（@impl-* 机制）或并入统一 name→ID 设施；两链路绑定语义不得长期双轨。
- **低价值（P2）**：D6 派生预设名生成器随 F7 收敛（统一 "%s_bound_%d" 数值语义 param_index vs 全局计数）；LvSymbol.name[64] 冗余字段删除或改注（lv_sema.c:31-35）；module/import no-op 语义文档化（@impl-none 承接 K3——check_stmt_noop MODULE_DECL/IMPORT_DECL 解析了但绑定语义为零）。
- **分层保留**：权威容器层（lv_registry/lv_hashtable_str）、公理包复合 key 包级隔离（命名空间健康范本）、namespace_depth 图编译作用域（20+ 文件一致）、λ De Bruijn 绑定、模式匹配绑定（3 文件共享）、公式 x/y 变量表、5 种不同 DSL 各自关键字表、.lv 三张共享几何词表（已收敛）。

### 3.89 依赖获取/服务定位模式面（第十八轮，K49）

**决策**：
- **N5 幻影 bind API 修正（P0 M5）**：engine_lifecycle.c:45 注释声称的 engine_bind_context() 全库不存在——补 engine_bind_context/engine_get_context API 或删除注释文档化"不绑定仅借图"契约（engine->context 迁移中 engine.c:9-28 自认第 2 阶段）；orchestrator:208 字段直写（in->engine->context = in->ctx）加注释或转 API。
- **N3 流上下文宏收敛（P0）**：LV_STREAM_CTX_DECLARE/DEFINE 双份定义（stream.h:338-347 vs stream_context_util.h:35-44 逐字重复）锚定单头（stream.h）单宏族；lv_DECLARE_STREAM_CTX（lv_internal.h:209 仅声明变量无 setter）合并或标注 legacy；5+ 处手写（axiom_pkg/formula_converter_util/debug/high_dim/solver_engine 各带"不适用宏"注释）迁移统一命名。
- **N1/N2 StreamContext 获取规范（P1，承接 K15 宏收敛决策执行层）**：模块内部统一 xxx_get_stream_context()（proof/rewrite/prop_verifier 为范本，补齐 11+ 缺失 getter）；跨模块统一引擎 getter；interop 导出侧补 engine 参数消除模块内双轨（导出子模块 TLS 全局 vs 命令子模块引擎 getter）；lv_context_get_stream 懒创建与引擎 stream_ctx 语义合并决策。
- **N4 getter 命名族统一（P1，承接 K15）**：进程级单例 getter 命名 4 风格（_current()/_get()/get_global_/_global()）统一为 lv_<module>_current() 或 lv_get_global_<name>() 二选一写入治理文档。
- **N7 注册表获取层规范（P1，并入 L3/H1）**：进程级注册表一律经 lv_<name>_registry_get_global()；实例级一律经实例 getter；禁止新增第 4 种承载（现 4 种：统一设施/global getter/static 状态/实例字段）。
- **N6 g_tls_engine 登记 legacy（P1，随 J5/K6 退役机制）**：TLS 隐式引擎指针（engine_scheduler.c:62 旧 API 专用注释自认 legacy）登记黑名单，新代码禁止 TLS 隐式引擎；引擎获取 4 模式（工厂/缓存/临时/隐式）场景登记表（纳入 K7 降级文档）。
- **低价值（P2）**：N8 失败处理约定登记（可选增强依赖静默 NULL 豁免、命令语义路径必须报错，并入 K1 失败约定契约表）；N9 测试入口统一（engine_create → lv_engine_create 随 K11 测试替身治理）。
- **分层保留**：分配器/线程池/lv_error/geometry_config 单事实源与范本形态、lv_registry 统一设施（数据结构层）、context 资源操作回调注入（L0 注入 L3/L4 解耦）、instance 级 getter 族（对象级规范）、interop persistent_engine 缓存+快照回滚（场景合理需登记）、配置 A/B 分发机制（K44 已决策）、模块生命周期注册表（J1 已决策）。
- **复核一致**：K15 全局状态/单例/TLS/裸 extern/锁缺口、J2 懒锁、L3/H1 注册表本身、G5 配置、K43 池——本报告新证据均落在"获取方式/接线路径"维度未覆盖已登记结论的注册表/容器本身。

### 3.90 算法复杂度/性能标注面（第十八轮，K50）

**决策**：
- **复杂度词汇表统一（P1）**：PresetComplexity 单源（枚举 func_block_preset.h:158-166 派生两处名称表——func_block_preset_query.c:257-265 "O(1) - 常数时间" vs lv_impl_upper_preset.c:170-177 "O(1)" 格式不同）；JSON 序列化数字/字符串格式二选一（lv_impl_upper_preset.c:423 %d vs preset_manager_serialize.c:112 字符串）。
- **graph_memory M5 修正（P1）**：lv_insertion_sort（lv_utils_array.c:72-100）声称 "O(n log n)"+"qsort for large" 但纯 O(n²) 无 qsort 分支——实现 qsort 分支（7 调用点）或改注释为 O(n²)（graph_memory.c:148/:176-177/:197）。
- **InternalPresetEntry.complexity 死字段处置（P1 M6）**：preset_blocks.c:184/:572 只写不读（metadata 无复杂度字段）+ preset_blocks.h:376-698 30+ 处宏注释"复杂度：O(1)/O(n)..."无任何可执行对应——接线（补 metadata 字段+查询 API）或删字段+注释降为文档说明。
- **低价值（P2）**：dsl_lexer.c:147/:154/:254 二分声称 vs :258-264 线性实现注释修正（同函数自相矛盾）；bdd_encoding.c:177 注释过期修正（已是开放寻址哈希）；幻影文档清理（PERFORMANCE_OPTIMIZATION.md:109-115 lvBloomFilter/lvSkipList/lvRTree 全库不存在的结构复杂度表删除 + 37_parsing_layer.md:28 "O(1) 遍历"改 O(n)）；起草"复杂度注释惯例"（以 constraint_graph.h:394/fast_index.c 诚实条件标注为范本——声称必须与实现一致，低效处自述规模可辩护）。
- **分层保留**：哈希 O(1)+回退线性诚实标注惯例、CDCL 线性 BCP 自述、mv_polynomial 插入排序自述、geometry_csg_hull 暴力（顶点<200）、rewrite_snapshot 线性搜索、fast_index/lv_registry/graph 哈希+回退线性（K7 范畴）。
- **复核一致**：G2 expansion_cache 线性扫、K7 索引→线性扫描 ~10 处、K8 基准/阈值、I2 计时/Welford、K45 graph getter 契约、K3 dsl_compiler 流水线 M5、K2 normalization 手写插入排序×2——不重复报告。

---

## 4. 优先级与工作量（v1.18 更新：123 组）

| 批次 | 内容 | 工作量（估） | 风险 |
|---|---|---|---|
| **P0 死代码/冗余清理** | S2-S4 序列化冗余、E1/E2 导出去重、P4 改名、C1 删 setup.py、E11 断言参数序、E15 目录归一、L1 状态机合并、L5 删 tracked 分配器、L7 泄漏检测归一、L10 删重复文档、F1 表达式树二选一、F2 规范形、F3 字符串化、F6 atoi、G1 熔断写入口+解析安全+死错误码、G2 规格对齐、G3 命名澄清、G4 插件广播、G5 删 global_state、H1 插件命名冲突+ecosystem 文档、H2 protocol undo 空壳、H5 删 test_runner+setup.py 排除测试、I2 删第二份 Welford+死计数器、I4 删 2 套无调用方 round-trip+裸 fopen 收编、I5 层验证宏接线+2 处 P0 方向修正、J1 8 处 M6 清理接线+once_reset 补齐、J2 锁抽象单一化+9 处惰性锁迁移、J3 产物移出 git+死配置清理、J4 删 lv.utils+build/lib 镜像+顶层导出单一化、J5 lvPlugin/REL_FORMULA/守卫枚举冲突修复、K2 快速幂×3+平方因子×3 统一、K3 4 头 M5 注释修正+README 幻影 API、K5 示例教学代码处置（归档+转正+删除按 v1.9.3）、K6 lv_DEPRECATED 全覆盖+preset 双 compat 合一+黑名单、K7 静默降级 9 项修复+enable_cache 伪配置、K8 删 lvBenchmark dead API+时钟绕道改基座、K9 死开关 8 项清理+lv_PUBLIC_API 双定义修复、K10 3 处所有权注释错误修复+memory-ownership.md、K11 测试设施移出生产库+测试辅助收敛、K12 补 lv_SQRT2 系列权威宏+e 单一权威、K13 solver_symbolic factorize guard-detach+空标签展平、K14 tactic 映射单源化+删孤儿 Lean 注释+lvProofStep 改名、K15 3 个真实竞态修复（lv_config 撕裂读/g_coeff_pool TOCTOU/跨线程 lv_init）、K18 find_package 修复+导出机制统一（config.h:643 删/接线 lv_USE_SHARED/visibility hidden）、K19 容差单一表执行+激活 3 个零消费者 cfg 字段、K20 1 例实现无声明修复+幻影 API 处置+死宏清理、**K21 锁定 clang-format 版本+557 文件全量回填（含 test）+format check CI**、**K22 导出格式单源化+lvExportFormat 三合一+TransformType 单表（修 protective 拼写）+变换预设入生成源**、**K23 序列化注册表单入口扩展+删 ConstraintGraph 2 冗余路径+Module 三载体收敛（删 msgpack 丢图/LVZ 不可读）**、**K24 per-instance RNG+死生成器处置+add_point 副本闭环**、**K25 路径常量单源+共享库加载收敛+公理包 117 处测试路径收敛**、**K27 parser_safety 接线（主解析链闸门）+安全算术单一入口（lv_SAFE_ADD 收敛）+增长逻辑单一路由（IntArray 废弃）+graph_conflict +1 溢出修复**、**K28 A1-A6 死机制接线或删除+无防护点补限（formula 解析器 current_depth/lv_parser 深度/destroy 域显式栈）**、**K29 锁顺序总序文档+平台分裂修复（trylock/timedlock+MSVC 告警）+倒锁防护（registry destroy 回调移锁外/stream 自死锁）**、**K33 I4 遗留闭环（3 文件 4 处裸 fopen/fclose 收编+5 处混合配对+lv_storage file:// 后端走封装）**、**K34 命令注册设施+arity 校验（10 处 Usage 样板表驱动）+G1 Circle 语义修复（改调 graph_add_circle）+G2 约束坍缩修复（PARALLEL 接线或 UNSUPPORTED）+G3 严格参数解析**、**K35 module_export XML 转义收编+SVG <text> 自转义双写缺陷修复+证明→LaTeX 转义补齐（三缺一）**、**K36 FATAL→紧急保存链路修复（崩溃保护失效）+g_emergency_handler 接线+lv_CHECK_RANGE 同名消歧**、**K38 超时判断统一单调基座（sledge clock() 超时形同虚设）+死超时 4 处接线或删除（solver cdcl.time_ms 恒不触发）**、**K39 LVZD 容器修复或删除（头部越界写/读+恒真断言）**、**K41 垂直/平行约束接线修复（graph_add_perpendicular 新增+formula betweenness 占位+interop CONTAINMENT 占位改真实类型）+新增 lv_perpendicular/lv_parallel 谓词激活 2 零消费者字段+meta_proof 平行/垂直补坐标级验证**、**K43 coeff_pool 尺寸失配越界修复（count>8 强制回退）+两套通用池互委托（随 I3）**、**K45 校验宏族调用侧收敛（B 族补返回值定制+迁移 25 文件）+graph 域失败语义统一（NULL 不再映射 CONFLICT）+头契约与行为契约对齐**、**K47 T1 lv_tree_traverse 接线或删除（消除 M6）+T3 证明树去重（lvProofTree/ProofSearchTree 删或接线+proof_widget 两桩）**、**K48 D4 量词绑定解绑修复+D1 name→ID 统一设施（随 F7/K43/I3）**、**K49 N5 幻影 bind API 修正（engine_bind_context 不存在）+N3 流上下文宏双份收敛** | ~27000-37000 行删除/改名/接线 | 低-中（多为无调用方或纯删除；K13/K15/K18/K19/K23 需回归；K27 解析链接线+K28 无防护点+K29 倒锁修复+K33 文件 IO 收编+K34 命令语义修复+K36 崩溃保护+K38 超时基座+K39 LVZD+K41 几何语义+K43 池越界+K45 契约对齐+K47 树遍历接线+K48 量词解绑需专项回归） |
| **P1 权威格式收敛** | S1 Module→JSON、E4 canonical、C2/C14 预设单一源、C4 注册表、E5 错误码桥接、E8 有理数、E13 DSL 归一、E15 CMakePresets、L2 进度模型、L4 事件契约、L6 内存统计、L8 日志级别、F4 导入共享层、F5 几何枚举四合一、F7 预设容器、G1 常量合一、G2 通用缓存层、G3 BFS/Kahn 收敛、G5 配置单一注册表、H1 后端注册单一化（承接 L3）、H3 稠密 LU 三合一+稀疏直接法入接口、H5 常量对拍 codegen、I1 graph_dot 收编+atp/smt 骨架共享、I2 计时基座单一化+统计分层、I3 增长逻辑单一路由+IntArray 废弃、I4 round-trip 基座单一化+文件 IO 收敛、I5 归属修正（dsl/module_lvz/gc_language/ecosystem/module_export/lvProofObject/proof 双轨）、J1 单一生命周期注册表、J2 原子 64 位补齐+统一、J3 CI 报告收敛+docx 生成收敛、J4 预设单一事实源+几何操作入口收敛、J5 命名规范锚定+前缀补齐（版本化）、K1 场景文案规范表+状态名表收敛+语言策略、K2 FNV 双家族+排序残留收敛、K3 @impl-* 声称标记+桩约定+对拍机制、K4 版本分层+单一事实源+读端校验+ABI 治理、K5 USE_CASES 收敛+示例同步机制、K6 退役登记表+黑名单 grep CI、K7 统一降级登记+降级语义统一+4 套求解降级收敛、K8 Welford 收敛 L0+阈值基线表+CI 串行性能 job、K9 feature_gates.h 单一表+_WIN32 迁移 lv_PLATFORM_*、K10 [copy]/[take]/[borrow] 头注释全覆盖+静态检查脚本、K11 mock 机制统一+预言机收敛、K12 字面量收敛（M_PI 30+ 处延伸+角度宏接活）+黄金比去重、K13 锁守卫统一 lv_DEFER+标签规范+清理重复收敛、K14 证明导出锚定 L5+SVG/TikZ 收敛+序列化注册表接入+层归属锚定、K15 全局 getter 化+TLS 容器化+锁补全（G2-G4/G7）+容器收敛、K16 前向声明集中 lv_fwd.h+引用即包含（消 7 传递依赖）+拼写统一、K17 sanitizer 矩阵统一（fuzz 补 UBSan）+corpus 管理+回归进 CI、K18 lv.pc 修复+版本单源+发布物真正产出（CI package 步骤）、K19 测试断言容差表+1e-15 相对化+跨语言收敛、K20 -Wmissing-prototypes 启用+一致性检查脚本+死宏清理、**K21 clang-format 豁免登记制度+Doxygen 对齐+命名/格式合并立项**、**K22 命令枚举对齐+序列化键单表化+协议契约 exempt 标注**、**K23 跨格式 round-trip 测试+OPML 不对称修复+比较器强化**、**K24 确定性 seed 套件级+默认 seed 单一化+构造辅助收敛**、**K25 预设单源+测试输出目录对齐+bootstrap 单基准+资源三级解析**、**K26 内置失败分配器 lv_allocator_fail+OOM 错误路径测试补全+注入盲区修复（tracked 走 vtable/memory_pool 改 lv_malloc）**、**K27 sanitizer 覆盖 ctest（address,undefined）+窄化强制（lv_ASSERT_FITS_INT）+__int128 守卫补齐**、**K28 depth_limits.h 单一权威深度限制表（推理 100/1000/10000 合一+128 四重复合并+LV_DESTROY_MAX_DEPTH 裁决）+无防护点补限（P1 档：lambda_unify 5 族/dsl_compiler_ir/lv_sema/render_*）+栈帧治理**、**K29 锁抽象收敛（runtime_guard 弃用或启用+日志 3 锁 2 管道+组同步 2 套+后端注册表锁 4 份）+潜伏自锁链修复（memory_pool stats_mutex）+粒度统一（lvRegistry/groebner 惯用法）+TSAN 回归**、**K30 宏规范锚定（lv_ARRAY_SIZE 单源+X 命名统一/lv_PUBLIC_API 三定义收敛+宏 lint）+强转检查（lv_cast_size_to_int/装箱拆箱 helper/const 版本）+X-macro 补全（_Static_assert 铺开+硬编码计数 6/12 消除+algebra_mode 别名 switch 引用公共 API）+类型擦除收尾（solver_order 迁 lv_topo_run）+CI 枚举对齐检查**、**K31 通配符收敛（lv_str_glob_match）+lv_dstr 处置（随 I3 合并）+标识符扫描收编 lv_str_scan_ident**、**K32 阶段完备性判定单源化（lv_session_stage_all_complete 替代 meta_verify 7 循环+结构化字段替代 strstr 文本探测）+证明进度状态二合一（ProofState 死字段接线或删除）+L1 合流连带件（奇偶测试改造/强制覆写原语）**、**K33 exempt 标注补全+黑名单 grep 落地（裸 fopen/fclose）+文件大小收编+临时文件命名迁移**、**K34 命令表收敛（if-else→查表+lv_builtin_commands 并入 interop 表）+Python JSON-RPC 字典表+空壳接线或撤除（Solve/Rewrite/Unify/lv_proto_terminal_exec）+测试 CLI 补 argv**、**K35 JSON 反转义收敛（lv_json 改调 lv_str_json_unescape）+词法器骨架共享（module_lvz/axiom_pkg）+Python _str_dec helper+encode 绕道修复**、**K36 日志级别状态收敛（6 容器→1 锚定 g_log_level+词汇 4 套合一+映射表 3→1）+lv_impl_native debug 族删除+lv_set_log_level 接线或删+trace_session 移除（I2 已决）+verbose 死开关清理**、**K37 几何向量原语收敛（lv_vec3_length/lv_vec3_angle 收编模长 6+/归一化 3/叉积 2/夹角 3）+lv_number_is_integer UB 修复**、**K38 墙钟秒收敛（lv_get_wallclock_sec 收编 8+ 处 time(NULL)）+timestamp_ms 契约统一（3 语义）+休眠 4 处收编+K8 收尾**、**K39 AxiomPackage 哈希单源化+interop JSON-RPC 声称对齐+deserialize_clers 静默修复+协议编解码测试补齐**、**K40 聚合清单单源化（27×2→1）+依赖三元组变量化+lv_platform_libs interface 库+func_block 回归宏+include 集变量化+优化选项按配置限定**、**K41 meta_proof 收敛委托权威（叉积/点线/点圆）+algebra_mode 硬编码委托+recursion_selector 命名常量+方位角收敛（lv_vec2_angle）**、**K42 socket 平台抽象层（lv_socket.h 句柄/invalid/close/last_error/EINTR 语义）+lv_socket_send_all 收敛+SO_REUSEADDR 对齐+WSAStartup 引用计数单点化+socket 路径测试补齐**、**K43 g_coeff_pool 生命周期接线+预设池按需化+线程安全语义统一+id_map 共享工具+池级统计随 L6**、**K44 默认值单一事实源（compat 50 宏派生/模块回退宏删/B 种子键删）+lvConfig 读路径二选一+lvSolverConfig 改名消歧+配置键单源表+文档对拍机制+setter 校验锚定**、**K45 失败约定契约表（并入 K1）+契约断言接线或删除（lv_verify_data_integrity）+契约测试补强（错误码断言 16→全覆盖）**、**K46 预设枚举名单源化（R10 手写表改宏生成修复 UNKNOWN）+图哈希家族收敛（删死代码）+坐标哈希统一入口+黄金比收编（K12 延伸）**、**K47 树销毁 4 手写变体接入共享设施（与 K28 合并）+证明树导出骨架共享（并入 K14）+ProofColor merge 原语+FormulaNode 分发表派生（修 eval 漏项）**、**K48 关键字/类型名单源化（LV_ENTITY_TYPE_X 派生）+重定义语义统一+ .lv 文本链路定性（承接 K3）**、**K49 StreamContext 获取规范（模块内 getter+跨模块引擎 getter+interop 双轨消除）+getter 命名族统一+注册表获取层规范+g_tls_engine legacy 黑名单**、**K50 复杂度词汇表统一（PresetComplexity 单源+JSON 格式二选一）+graph_memory M5 修正+complexity 死字段处置+复杂度注释惯例** | ~41000-56000 行改动 | 中（需回归测试；J5 前缀补齐+K4 版本校验+K9 开关迁移+K15 锁补全+K18 导出统一+K23 注册表改造+K26 注入+K29 锁抽象+K30 宏改名+K34 命令语义修复+K36 日志级别收敛+K38 超时基座+K40 CMake 重构+K41 谓词委托+K44 配置单源+K46 哈希收敛+K48 符号表迁移+K49 获取规范破坏性） |
| **P2 语言统一** | D1-D2：.lv 吸收 dsl_compiler + .lvz 职责收敛 + 语法糖第一批 + L11 语法单一事实源 | ~1800-3000 行 | 中高（语法面） |
| **P3 证明/API/推理 IR 统一** | P1-P3 证明 IR、E6 返回码、E7 API 入口、E12 测试入口、L3 推理注册表、F8 验证入口、F9 策略调度、F10 引擎栈分层、H2 快照分层文档化、I3 FIFO 队列族收敛、J2 并行骨架评估（L9 前端接内核已移出） | ~4000-6000 行 | 中高（引擎/证明） |
| **P4 项目级合并** | C3 Lean 合并、E9 代数数桥接、E10 区间语义、E14 预设 v3→v4、L10 文档合并、H4 公理单一事实源+formal 去重+CI 对齐 | 视工具链 | 高（外部工具链/形式化） |

> v1.18 工作量上调主因：第十八轮新增 K46-K50 涉及哈希/查找收敛（预设枚举名
> 单源化+图哈希家族收敛+坐标哈希统一）、树形结构收敛（树遍历接线+树销毁 4 变体
> 接入共享+证明树去重+导出骨架）、符号表收敛（量词解绑+name→ID 统一设施+关键字
> 单源化）、依赖获取规范（StreamContext 三形态+getter 命名族+注册表获取层）、
> 复杂度标注治理（词汇表统一+M5 修正）等广面改动；K46-K50 另确认 lv_hashtable
> 三形态收敛（含 3 处完备豁免）、lv_tree_release_recursive 3 消费方共享、
> 分配器/线程池/lv_error 单事实源、哈希 O(1)+回退线性诚实标注惯例为健康基线；
> 第 18 轮含 5 个真实缺陷候选（UNKNOWN 行为缺口、lv_tree_traverse 零调用 M6、
> 量词泄漏、幻影 bind API、graph_memory M5）需修复后回归。

> v1.17 工作量上调主因：第十七轮新增 K41-K45 涉及几何谓词收敛（lv_perpendicular/
> lv_parallel 新增+垂直/平行约束接线修复+meta_proof 委托权威）、socket 平台抽象层
> （lv_socket.h+send_all 收敛+WSAStartup 单点）、池收敛（coeff_pool 越界修复+
> 两套池互委托+id_map 共享）、配置单一事实源（compat 50 宏派生+107 getter 决策+
> 文档对拍机制）、API 校验调用侧收敛（B 族补返回值定制+graph 域失败语义统一+
> 头契约对齐）等广面改动；K41-K45 另确认三层谓词健康分层、socket 侧缓冲唯一
> 实现、lv_parse_utils 契约范本、已收敛委托链 6 条、LV_CFG 键单源为健康基线；
> 第 17 轮含 5 个真实缺陷候选（垂直=betweenness、平行=CONTAINMENT、coeff_pool
> 越界、segments_can_intersect 空壳、setter 校验声称脱节）需修复后回归。

> v1.16 工作量上调主因：第十六轮新增 K36-K40 涉及日志级别状态收敛（6 容器→1+
> 映射表 3→1）、几何向量原语收敛（模长/归一化/叉积/夹角 4 类 15+ 处收编）、
> 超时基座统一（sledge/prop_verifier 改单调+死超时 4 处接线）、协议编解码
> （LVZD 容器修复+AxiomPackage 哈希单源化+字节序手写收编）、构建系统重构
> （聚合清单 27×2→1+依赖 interface 库+include 集变量化+优化选项按配置限定）
> 等广面改动；K36-K40 另确认字节序权威底座（lv_store/lv_load）、测试注册
> 288/288 全覆盖、超越函数全走 libm、三基座分层本身正确为健康基线；第 16 轮
> 含 4 个真实缺陷（FATAL 紧急保存双重失效、sledge 超时形同虚设+solver 超时门
> 恒不触发、LVZD 头部越界写/读、lv_number_is_integer 截断 UB）需修复后
> 回归。

> v1.15 工作量上调主因：第十五轮新增 K31-K35 涉及通配符收敛（lv_str_glob_match）、
> 阶段完备性判定单源化（meta_verify 7 循环+strstr 文本探测替换）、I4 遗留闭环
> （11 处文件 IO 收编+lv_storage 后端对齐）、命令注册设施（10 处 Usage 样板表
> 驱动+2 个命令语义缺陷修复）、XML 转义收编+SVG 自转义缺陷修复+证明→LaTeX
> 转义补齐等广面改动；K31-K35 另确认字符串主体设施收敛成形（0 裸 sprintf/
> strcpy、read-all 归零、转义四族分层、CDCL/WS 表驱动范本）、FILE* 配对无泄漏
> 为健康基线；K34 含 2 个疑似真实缺陷（AddNode Circle 建 LineSegment、
> Parallel/Perpendicular/EqualLength 坍缩 CONTAINMENT）需测试钉住后修复。

> v1.14 工作量上调主因：第十四轮新增 K26-K30 涉及错误注入通道（内置失败分配器+
> OOM 错误路径测试）、整数安全收敛（parser_safety 接线+安全算术单一入口+增长逻辑
> 单一路由）、递归深度统一（depth_limits.h 单一权威表+A1-A6 死机制接线或删除+
> 无防护点补限 20+ 文件）、锁约定收敛（锁顺序总序文档+平台分裂修复+倒锁防护+
> 锁抽象双族合一）、宏规范锚定（ARRAY_SIZE 单源+X 命名统一+强转检查+X-macro
> 补全）等广面改动；K27-K29 另确认 lv_arith_safe.h 判定式无 UB、图/树遍历显式栈
> 样板、宏参数安全三类 0 真阳性、thread_pool 顺序获取不嵌套为健康基线。

> v1.13 工作量上调主因：第十三轮新增 K21-K25 涉及格式化全量回填（557 文件）、
> 导出格式/变换预设名单源化（lvExportFormat 三合一）、序列化注册表单入口
> （全对象扩展+跨格式 round-trip）、per-instance RNG、路径常量单源
> （共享库/公理包 117 处/预设双基准）等广面改动；K21-K25 另确认生产 strcmp 链=0、
> ~50 张 name 表、lv_path_join/lv_PATH_SEPARATOR 收敛、断言骨架 100% 覆盖、
> Doxygen @ 标签统一为健康基线。

> v1.12 工作量上调主因：第十二轮新增 K16-K20 涉及引用即包含（7 传递依赖 +
> 前向声明 73 行收敛）、fuzz sanitizer 矩阵 + corpus 管理、find_package/lv.pc
> 修复 + 导出机制统一（三处宏覆盖收敛）、容差单一表执行（7 值共线 + 3 字段
> 断链激活）、声明一致性检查（-Wmissing-prototypes + 幻影处置）等广面改动；
> K16/K20 另确认守卫全覆盖/零环/5 头 100% 对齐/0 符号泄漏为健康基线。

> v1.11 工作量上调主因：第十一轮新增 K11-K15 涉及测试设施移出生产库、
> 数学常量权威宏补齐（修复 MCTS_C 精度）、清理机制统一（goto 164 处收敛
> lv_DEFER）、证明导出锚定（tactic 8 表单源化）、全局状态线程安全（6 组
> 容器 + 3 真实竞态）等广面改动；K13/K15 另确认 goto 合理差异（事务/MSVC
> 回退）与全局分层范本（memory_pool/geometry_config/lv_error）。

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
| EngineState 零生产调用但被文档引用 | L1 合并前 grep 确认无外部依赖；删除后同步文档 |
| 推理注册表统一可能影响求解路径 | L3 以 GROEBNER 单实例验证等价后再切调度器分发 |
| 前端接内核是功能开发非纯去重 | L9 已确认**刻意留白**（等内核定型），不列入清理批次；契约保留待内核定型后对接 |
| F2 规范形二选一影响代数等价判定 | 先决策（兑现桥接 vs 删除孤儿），不得半吊子；测试 test_expr_canon 作回归锚 |
| F8 验证入口 ≥6→1 波及证明引擎 | 以 lvProofObject + meta_verify 为锚，各入口降级适配器并跑全证明测试 |
| F9 策略调度归一波及两栈策略 | 共用调度面先加注册表（策略注册+调度表），策略实现不动，回归 proof_multi_strategy 测试 |
| G1 熔断写入口收敛影响防护行为 | 收敛前对拍三套入口触发语义（context 内联 vs 独立 API），回归熔断测试 |
| G2 通用缓存层影响缓存命中语义 | 各缓存收敛后跑 prop_verifier/smt/axiom 缓存测试，容量驱逐新语义需验证 |
| G5 配置默认值收敛改变运行行为 | 会话默认改为读全局后，观察 smoke/CI 测试超时/深度表现，必要时调整全局默认 |
| H1 后端注册单一化影响求解分发 | 承接 L3：以 GROEBNER 单实例验证等价后再切；lvPlugin 命名冲突先修（纯改名） |
| H3 生产路径切 numerical_backend | gauss_eliminate→host_lu 先做数值对拍（n≤20 案例逐例比对），再切分发 |
| H4 公理去重可能影响形式化验证 | formal/lv 与 lv-formal 二选一后 CI 重跑 lake build 两处，验证等价后再删 |
| H5 常量对拍 codegen 改动面 | 先补流事件对拍（最小缺口），codegen 作为二期；setup.py 排除测试低风险 |
| I2 计时基座收敛影响性能行为 | 6 套起止计时收敛前对拍（begin/end 语义等价），回归性能测试 |
| I3 增长逻辑单一路由影响容器 | lv_ensure_capacity size_t 形态先加测试（等价/边界），再逐容器切换 |
| I5 层验证开闸可能暴露新违规 | 先修 2 处 P0 + 归属修正后再开闸；CI 依赖矩阵先于编译断言启用 |
| J1 生命周期统一影响 init/cleanup 循环 | thread_pool once 重置先做（直接威胁 90+ 循环测试）；注册表复位语义先加测试再迁移手动序列 |
| J2 锁抽象单一化影响并发行为 | lvMutex→lv_mutex_t 纯改名低风险；runtime_guard 从未启用可直接删；原子 64 位补齐先加测试再迁移 |
| J4 预设单一事实源影响 Python 用户 | 保留 preset_func_blocks_compat 兼容层 + DeprecationWarning；.lvz 编码修复后重生成并跑 Python 测试 |
| J5 命名前缀补齐属 API 破坏 | 分级推进：P0/P1 编译级冲突（lvPlugin/REL_FORMULA）先行；前缀补齐版本化迁移（旧别名 compat 期）；lint 最后上 |
| K1 文案统一影响测试钉死文案 | 改文案须同步更新 test_context 等断言；先建场景文案规范表再逐家族收敛 |
| K4 版本校验补齐可能拒绝旧文件 | msgpack/JSON format_version 门先定兼容策略（缺省视为 v1）；Python 版本比对先加测试再启用 |
| K5 示例修正范围广（19 处脱节） | P0 先修根 README 4 幻影 + API_QUICKSTART 整篇；P1 USE_CASES 失效骨架删除/收敛；示例同步机制（符号存在性脚本）先行 |
| K6 lv_DEPRECATED 全覆盖可能大量告警 | 分批挂载（先公共头后内部）；-Werror 仅内部构建；黑名单先于编译期告警启用 |
| K7 降级登记扩展影响性能 | 降级计数走原子/relaxed；登记只加日志+计数器不改路径；enable_cache 伪配置先删字段 |
| K8 性能回归门可能 flaky | CI 专用串行 job 避并行抖动；阈值基线表宽松度可调；不设严格 baseline（可选增强） |
| K9 开关迁移 _WIN32 81 处 | 先建 lv_PLATFORM_WINDOWS 宏再批量替换；feature_gates.h 五元组先行；死开关清理纯删除低风险 |
| K10 [copy]/[take]/[borrow] 标注覆盖 224 头注释 | 先写 memory-ownership.md 单源再机械标注；3 处错误注释修复 P0 先行 |
| K11 测试设施移出生产库影响构建 | test_framework/bootstrap_test 加 BUILD_TESTS 门控或移出 lv_static 前先确认无生产消费（bootstrap_test_internal 重定义先删） |
| K13 goto 迁移 lv_DEFER 影响清理语义 | solver_symbolic factorize 迁移前对拍（U5 设施现成）；groebner 锁守卫 3 文件分批；test_lifecycle 钉住多出口/LIFO |
| K14 tactic 映射单源化影响外部契约 | 以 LV_PROOF_STEP_TYPE_X 为源派生各表；coq 本地枚举保留 exempt；导出分支删除前跑证明导出测试 |
| K15 锁补全影响并发行为 | 3 真实竞态（lv_config 撕裂读/g_coeff_pool TOCTOU/跨线程 lv_init）先修；全局 getter 化纯改名低风险；容器收敛分批 |
| K16 引用即包含波及 305 头 | 先建 lv_fwd.h + 消 7 传递依赖（脆弱契约优先）；12 公共头去伞分批；每头自包含 TU 检查先行 |
| K18 导出机制统一可能破坏 DLL | 删 config.h:643 前先验证 lv_BUILD_SHARED 传播；visibility hidden 需跑共享库测试（build_symcheck）；WINDOWS_EXPORT_ALL_SYMBOLS 二选一需回归 Python 绑定 |
| K19 容差收敛影响数值判定 | 激活 3 字段前对拍（solver 1e-6 vs cfg 1e-8 语义差异需确认）；测试断言容差表分批替换；1e-15 相对化先加测试 |
| K20 -Wmissing-prototypes 启用可能大量告警 | 先修 1 例实现无声明 + 宏生成白名单；按头分批启用；幻影处置（文档引用删除）先行 |
| K21 全量格式化 557 文件影响 git 历史 | 独立提交"全项目 clang-format v2"（不与功能改动混）；锁定版本后再格式化防二次漂移；test 与 core 分批 |
| K22 导出格式单源化可能破坏外部契约 | INTEROP_EXPORT_FORMAT_X 为源派生各表；lvExportFormat 三合一先加迁移映射；TransformType 修 protective 前对拍两表 |
| K23 序列化注册表单入口可能影响存量序列化 | 删 msgpack/LVZ 内嵌图前确认 autosave 恢复路径改 JSON；跨格式 round-trip 测试先行；OPML 双表合一前对拍导出/导入 |
| K25 路径单源可能破坏运行时定位 | 共享库搜索收敛前跑 Python 全测试（三平台）；117 处测试路径收敛后跑 test_axiom 套件；资源三级解析兼容 CWD 相对 |
| K32 阶段完备性单源化可能改变判定语义 | meta_verify 改调用前对拍（结构化字段替换 strstr 文本探测需同步测试断言）；ProofState 删除前确认无外部读取 |
| K33 文件 IO 收编可能影响日志/调试路径 | 日志族裸 fopen 迁移前确认 stderr 语义保留；lv_storage 后端改走封装前跑存储 round-trip 测试；黑名单启用前先补 exempt 标注防误伤 |
| K34 命令语义修复可能改变命令层行为 | G1/G2 修复前先写执行级测试钉住现状（AddNode Circle/约束类型），再改 graph_add_circle/PARALLEL；Usage 表驱动化前对拍 10 处文案 |
| K35 XML 转义收编+LaTeX 补齐可能改变导出输出 | module_export SVG <text> 先补内容断言再改；证明→LaTeX 转义补齐后跑导出测试确认外部契约（coq/lean 豁免不变） |
| K36 日志级别收敛影响诊断行为 | 锚定 g_log_level 前对拍 lv_log 与 debug_log 双管道语义（lv_set_log_level 接线或删需确认 Python 消费）；FATAL 紧急保存修复前确认崩溃路径可测 |
| K38 超时基座统一可能改变超时行为 | sledge 改单调前对拍耗时语义（CPU 时间 vs 墙钟差异）；solver cdcl.time_ms 接线后跑求解回归确认超时门真实生效 |
| K39 LVZD 修复可能影响格式兼容 | LVZD 生产零调用可直删；若保留则修后跑 test_geometry_core round-trip；AxiomPackage 哈希单源化前对拍两实现字段集 |
| K40 CMake 重构可能影响构建 | 聚合清单变量化前确认 lv_static/lv_shared 产物一致；lv_platform_libs 引入后跑全 target 链接验证；-O2 限定前对拍 Debug 构建行为 |
| K41 几何谓词委托可能改变判定结果 | 新增 lv_perpendicular/lv_parallel 前对拍现有 5 套实现（残差/模板/algebra_mode 容差差异需确认语义）；meta_proof 补验证后跑证明测试确认新剪枝不误杀；垂直约束接线前补测试钉住现有 betweenness 行为 |
| K42 socket 抽象层可能影响网络行为 | network_block 裁决先行（删或接线）；EINTR 语义统一前对拍 WS 帧发送路径；SO_REUSEADDR 对齐后跑服务器重启测试 |
| K43 池互委托可能改变分配行为 | coeff_pool 修复前构造 deg≥4 minimal_poly 复现测试；两套池合并前对拍分配底（vtable vs malloc）与扩容语义；g_coeff_pool 接 lv_cleanup 后跑清理回归 |
| K44 配置单源可能改变运行时默认 | compat 宏派生前对拍 50 对数值；107 getter 接线或降级前确认无生产消费；B 种子键删除前确认字符串路径读取者 |
| K45 契约对齐可能改变 API 行为 | graph 域失败语义统一前补测试钉住现有 4 种约定；头契约改文档化 NULL 容忍后同步 test_context 断言；B 族补返回值定制前对拍 A 族调用点语义 |
| K46 哈希收敛可能改变查找行为 | 预设枚举名单源化前对拍两表项数（4/12/6 vs 25/16/7）；图哈希收敛前确认无跨实现混用（互不兼容当前无正确性缺陷）；坐标哈希统一前跑 hash 分桶回归 |
| K47 树遍历接线可能改变遍历语义 | lv_tree_traverse 接线前对拍各树手写遍历行为；树销毁接入共享设施前对拍 trace_node 所有权特例；证明树删除前确认无外部消费 |
| K48 符号表迁移可能改变解析行为 | 量词解绑前补最小测试钉住现状；name→ID 迁移前对拍 mini_kernel/dsl_compiler 查重语义；关键字派生前对拍 s_keywords 44 词 |
| K49 获取规范可能改变接线路径 | StreamContext getter 补齐前确认各模块 TLS 注入路径；g_tls_engine 黑名单前确认旧 API 消费方；宏收敛前对拍双份定义使用点 |
| K50 复杂度标注不影响行为 | 纯注释/文档改动无行为风险；complexity 死字段接线或删前确认无消费方 |

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
- **F9**（第三轮）：lvContextState 与 EngineState 合并（L1）+ EngineState 若零调用则删除（L2）是否确认？
- **F10**（第三轮）：推理后端统一注册表（L3，lvBackendPluginRegistry 为唯一事实源）是否立项？
- **F11**（第三轮）：流事件 JSON 契约单一化（L4，删除 Python 漂移副本）是否确认？
- **F12**（第三轮）：前端接真实内核（L9）已确认**刻意留白**（等内核定型），不立项；契约保留待内核定型后对接。
- **F13**（第四轮）：F2 规范形二选一（兑现 FormulaNode→lvExpr→lvExprCanonical 桥接 vs 删除孤儿 lvExprCanonical）？
- **F14**（第四轮）：F8 验证入口收敛（6 入口→1，以 lvProofObject + meta_verify 为锚）是否立项？
- **F15**（第四轮）：F9 策略调度归一（两栈共用策略注册表）是否立项？
- **F16**（第五轮）：G1 解析安全函数接线（parser_safety 4 未接线函数 + 删死错误码）是否优先做（P0，安全面低风险）？
- **F17**（第五轮）：G2 通用缓存层（5 套推理缓存收敛）是否立项（P1，需设计容量/驱逐语义）？
- **F18**（第五轮）：G5 配置默认值收敛（lvSessionConfig 改读全局配置，消除会话覆盖全局矛盾）是否确认？
- **F19**（第五轮）：G5 删除 ConfigManager（B，INI）与 global_state 死代码是否确认？
- **F20**（第六轮）：H1 后端注册单一化（lvBackendPluginRegistry 唯一 + 删 SMT/ATP/静态 VTable，承接 L3）是否确认？
- **F21**（第六轮）：H3 生产路径切 numerical_backend（gauss_eliminate→host_lu + 注册表分发）是否立项（需数值对拍）？
- **F22**（第六轮）：H4 公理单一事实源（.lvz 为权威，formal/lv 与 lv-formal 去重 + CI 对齐）是否立项？
- **F23**（第六轮）：H5 Python 测试基座单一化（unittest→pytest + 删 test_runner + 补流事件对拍）是否优先做（P0，测试面低风险）？
- **F24**（第七轮）：I5 层验证机制修复（依赖表接入 + 缺省只允许本层 + CI include 矩阵）是否作为独立 P0 立项（先修空转机制再清零）？
- **F25**（第七轮）：I5 两处 P0 方向修正（context.c 推理栈归位 L2 / lv_loader 只产 AST 由 L0 装载）是否确认？
- **F26**（第七轮）：I2 计时/统计基座单一化（6 套起止计时→1 + Welford 二合一 + 求解统计收敛 SchedulerStats）是否立项？
- **F27**（第七轮）：I3 增长逻辑单一路由（lv_ensure_capacity 增 size_t 形态 + 12 处宏机械替换 + IntArray 废弃）是否优先做（P0，纯机械低风险）？
- **F28**（第八轮）：J1 生命周期统一（单一注册表 + 8 处 M6 接线 + thread_pool once 重置修复 init/cleanup 循环）是否作为独立 P0 立项？
- **F29**（第八轮）：J2 锁抽象单一化（lvMutex 旧别名退役 + 9 处惰性锁迁移 + 原子 64 位补齐）是否确认？
- **F30**（第八轮）：J4 预设单一事实源（锚定 .lvz 先修编码 + Python 只留 v4.0.0 + 删 lv.utils/build/lib）是否确认？
- **F31**（第八轮）：J5 命名规范锚定（P0/P1 冲突修复先行 + 前缀补齐版本化 + lint 强制）是否立项？
- **F32**（第九轮）：K1 场景文案规范表（通用场景标准文案 + 语言策略单点决策 + 状态名表收敛）是否立项？
- **F33**（第九轮）：K2 快速幂×3 + 平方因子×3 单设施（mul 回调参数化 + lv_squarefree_i64）是否优先做（P0，纯算法统一）？
- **F34**（第九轮）：K4 版本分层单一事实源 + 读端校验补齐 + ABI 治理（lv_PUBLIC_API 修复 + Python ctypes sizeof 校验）是否立项？
- **F35**（第九轮）：K5 示例面修正（根 README 4 幻影 + API_QUICKSTART 重写 + USE_CASES 收敛 + 示例同步机制）是否优先做（P0，误导性最强）？
- **F36**（第十轮）：K6 退役机制统一（lv_DEPRECATED 全覆盖 + preset 双 compat 合一 + 退役登记表 + 黑名单 grep CI）是否立项？
- **F37**（第十轮）：K7 统一降级登记（PerformanceCounters 扩展 + 降级语义统一 + 4 套求解降级收敛到 fallback_chain + 静默降级 9 项修复）是否立项？
- **F38**（第十轮）：K8 基准框架单一化（保留 lv_perf_benchmark_run + 删 lvBenchmark dead API + Welford 收敛 L0 + 阈值基线表 + CI 串行性能 job）是否立项？
- **F39**（第十轮）：K10 所有权三态契约落地（memory-ownership.md 单源文档 + [copy]/[take]/[borrow] 头注释全覆盖 + 3 处错误注释修复）是否优先做（P0，含 func_block_register 泄漏风险 + module_compute_content_hash UB）？
- **F40**（第十一轮）：K11 测试设施移出生产库（test_framework/bootstrap_test 加 BUILD_TESTS 门控 + bootstrap_test_internal 重定义删除 + 测试辅助收敛）是否优先做（P0）？
- **F41**（第十一轮）：K13 清理机制统一（lv_DEFER 唯一推荐 + solver_symbolic factorize guard-detach + groebner 锁守卫迁移 + 标签规范）是否立项？
- **F42**（第十一轮）：K14 证明导出锚定 L5 + tactic 映射单源化（LV_PROOF_STEP_TYPE_X 为源）+ SVG/TikZ 收敛 + 序列化注册表接入是否立项？
- **F43**（第十一轮）：K15 全局状态线程安全（3 真实竞态修复：lv_config 撕裂读/g_coeff_pool TOCTOU/跨线程 lv_init + 全局 getter 化 + 容器收敛）是否优先做（P0，真实竞态）？
- **F44**（第十二轮）：K16 头文件包含卫生（lv_fwd.h 前向声明集中 + 引用即包含消 7 传递依赖 + 12 公共头去伞）是否立项？
- **F45**（第十二轮）：K18 打包/导出修复（find_package + lv.pc + 导出机制统一：删 config.h:643 + 接线 lv_USE_SHARED + visibility hidden + 版本单源）是否优先做（P0，find_package 必坏）？
- **F46**（第十二轮）：K19 容差单一表执行（补 perpendicular/parallel 编译期常量 + 激活 3 个零消费者 cfg 字段 + 测试断言容差表 + 1e-15 相对化）是否立项？
- **F47**（第十二轮）：K20 声明一致性检查（-Wmissing-prototypes + 1 例实现无声明修复 + 幻影 API 处置 + 死宏清理）是否立项？
- **F48**（第十三轮）：K21 代码格式统一（锁定 clang-format 版本 + 557 文件全量回填含 test + format check CI + 豁免登记制度）是否立项？
- **F49**（第十三轮）：K22 魔法字符串单源化（导出格式 INTEROP_EXPORT_FORMAT_X 为源 + lvExportFormat 三合一 + TransformType 单表修 protective + 变换预设入生成源）是否优先做（P0，已产生真实数值/拼写错误）？
- **F50**（第十三轮）：K23 序列化注册表单入口（全对象扩展 + 删 ConstraintGraph 冗余路径 + Module 三载体收敛 + 跨格式 round-trip 测试 + OPML 不对称修复）是否立项？
- **F51**（第十三轮）：K25 路径常量单源（共享库加载收敛 + 公理包 117 处测试路径收敛 + 预设双基准单源 + 资源三级解析）是否立项？
- **F52**（第十四轮）：K26 错误注入容错（内置失败分配器 lv_allocator_fail + LV_FAULT_INJECT fail-point 宏 + OOM 错误路径测试补全 + 注入盲区修复：tracked 走 vtable/memory_pool 改 lv_malloc）是否立项（P1）？
- **F53**（第十四轮）：K27 整数安全收敛（parser_safety 接线主解析链闸门 + lv_SAFE_ADD 收敛 lv_safe_add_i64 + 字面量解析公共 lv_str_to_i64_safe + 增长逻辑单一路由 + graph_conflict +1 溢出修复 + sanitizer 覆盖 ctest）是否优先做（P0，含唯一具体 UB + 解析链防护缺失）？
- **F54**（第十四轮）：K28 递归深度统一（depth_limits.h 单一权威深度限制表 + A1-A6 六套死机制接线或删除 + 无防护点按 destroy→解析→合一→渲染补限）是否立项（P0，当前"文档宣称防护、生产裸奔"）？
- **F55**（第十四轮）：K29 锁顺序规范（锁顺序总序文档 + 平台分裂修复：trylock/timedlock + MSVC 退化告警 + 倒锁防护：registry destroy 回调移锁外/stream 自死锁 + 锁抽象双族收敛）是否优先做（P0，含 2 个真实死锁风险点）？
- **F56**（第十四轮）：K30 宏约定类型转换（lv_ARRAY_SIZE 单源 + X 命名统一 + 强转检查 lv_cast_size_to_int + X-macro 补全：_Static_assert 铺开/硬编码计数消除/algebra_mode 别名 switch + 类型擦除收尾：solver_order 迁 lv_topo_run + 宏 lint）是否立项（P1）？
- **F57**（第十五轮）：K31 字符串处理收敛（通配符 lv_str_glob_match + lv_dstr 删或纯转发 + 标识符扫描 lv_str_scan_ident + 空串惯用法黑名单）是否立项（P1，随 I3 合并）？
- **F58**（第十五轮）：K32 状态机收敛（阶段完备性 lv_session_stage_all_complete 替代 meta_verify 7 循环 + ProofState 死字段接线或删除 + 强制覆写原语 + 状态机模板单一化）是否立项（P1，随 L1/F8-F10 合并）？
- **F59**（第十五轮）：K33 文件 IO 闭环（I4 遗留 11 处裸 fopen/fclose 收编 + lv_storage 后端走封装 + 黑名单 grep 落地 + exempt 标注补全）是否优先做（P0，一次提交闭环 I4 遗留）？
- **F60**（第十五轮）：K34 命令/参数收敛（命令注册设施+arity 校验 + G1/G2 语义缺陷修复：AddNode Circle 改调 graph_add_circle/约束改走 PARALLEL + G3 严格参数解析 + Python JSON-RPC 字典表 + 空壳接线或撤除）是否优先做（P0，含 2 个疑似真实命令语义缺陷）？
- **F61**（第十五轮）：K35 编码/字符收敛（XML 转义收编+SVG 自转义缺陷修复 + 证明→LaTeX 转义补齐 + JSON 反转义收敛 + 词法器骨架共享 + Python _str_dec）是否优先做（P0，含 2 个真实输出缺陷）？
- **F62**（第十六轮）：K36 调试诊断收敛（FATAL→紧急保存链路修复 + g_emergency_handler 接线 + lv_CHECK_RANGE 同名消歧 + 日志级别状态 6 容器→1 + lv_impl_native debug 族删除 + lv_set_log_level 接线或删 + trace_session 移除）是否优先做（P0，含崩溃保护失效 M5）？
- **F63**（第十六轮）：K37 数学原语收敛（几何向量原语 N1-N5 合并立项：lv_vec3_length/lv_vec3_angle 收编模长 6+/归一化 3/叉积 2/夹角 3 + lv_number_is_integer UB 修复）是否立项（P1，含 1 个正确性隐患）？
- **F64**（第十六轮）：K38 时间/时钟收敛（超时判断统一单调基座 + 死超时 4 处接线或删除 + 墙钟秒收敛 8+ 处 + timestamp_ms 契约统一 + 休眠 4 处收编）是否优先做（P0，含 sledge 超时形同虚设+solver 超时门恒不触发）？
- **F65**（第十六轮）：K39 协议编解码收敛（LVZD 容器修复或删除 + AxiomPackage 哈希单源化 + interop JSON-RPC 声称对齐 + 字节序手写收编 + 协议编解码测试补齐）是否优先做（P0，含 LVZD 头部越界写/读真实缺陷）？
- **F66**（第十六轮）：K40 构建系统收敛（聚合清单单源化 + 依赖三元组变量化 + lv_platform_libs interface 库 + func_block 回归宏 + include 集变量化 + 优化选项按配置限定 + 清单漂移 CI 守卫）是否立项（P1，构建组织重构）？
- **F67**（第十七轮）：K41 几何判定收敛（新增 lv_perpendicular/lv_parallel 激活 2 零消费者字段 + 垂直约束接线修复：graph_add_perpendicular 新增/formula betweenness 占位/interop CONTAINMENT 占位 + meta_proof 补坐标级验证 + 浮点域共线 4/点线 5/点圆 3/相交 3 委托权威）是否优先做（P0，含 2 个语义错误占位 + 1 个验证缺口）？
- **F68**（第十七轮）：K42 网络套接字收敛（network_block 死代码裁决删除或接线 + socket 平台抽象层 lv_socket.h + send_all EINTR 语义统一 + SO_REUSEADDR 对齐 + WSAStartup 引用计数单点化 + socket 路径测试补齐）是否立项（P1，含 1 个死代码载体裁决）？
- **F69**（第十七轮）：K43 对象池收敛（coeff_pool 尺寸失配越界修复 + 两套通用池互委托随 I3 + g_coeff_pool 生命周期接线 + 预设池按需化 + id_map 共享工具）是否优先做（P0，含 1 个真实越界缺陷候选）？
- **F70**（第十七轮）：K44 配置收敛（默认值单一事实源：compat 50 宏派生/模块回退宏删/B 种子键删 + lvConfig 读路径二选一：107 getter 接线或降级 + lvSolverConfig 改名消歧 + 配置键单源表 + 文档对拍机制 + setter 校验锚定修 lv.h:695）是否优先做（P0，含六级默认值漂移风险）？
- **F71**（第十七轮）：K45 API 校验收敛（校验宏族调用侧收敛：B 族补返回值定制+迁移 25 文件 + graph 域失败语义统一：NULL 不再映射 CONFLICT + 头契约与行为契约对齐 + 失败约定契约表并入 K1 + 契约断言接线或删除 + 契约测试补强）是否优先做（P0，含误导性诊断链与 198 行永不运行实现）？
- **F72**（第十八轮）：K46 哈希/查找收敛（预设枚举名单源化：lv_impl_upper_preset 手写表改宏生成修复 UNKNOWN + 图哈希家族收敛：删 compute_quick_graph_hash 死代码 + 坐标哈希统一入口 + 黄金比收编承接 K12）是否立项（P1，含 1 个真实行为缺口）？
- **F73**（第十八轮）：K47 树形结构收敛（lv_tree_traverse 接线或删除消除 M6 + 证明树去重：lvProofTree/ProofSearchTree 删或接线 + proof_widget 两桩 + 树销毁 4 手写变体接入共享设施随 K28 + 导出骨架共享随 K14）是否优先做（P0，含 2 个 M6 声称-实现脱节）？
- **F74**（第十八轮）：K48 符号表收敛（量词绑定解绑修复 + name→ID 统一设施随 F7/K43/I3 + 关键字/类型名从 LV_ENTITY_TYPE_X 派生单源化 + 重定义语义统一 + .lv 文本链路定性承接 K3）是否优先做（P0，含 1 个量词泄漏风险）？
- **F75**（第十八轮）：K49 依赖获取收敛（engine_bind_context 幻影注释修正 + 流上下文宏双份收敛 + StreamContext 获取规范：模块内 getter/跨模块引擎 getter/interop 双轨消除 + getter 命名族统一 + 注册表获取层规范 + g_tls_engine legacy 黑名单）是否优先做（P0，含 1 个幻影 API 声称）？
- **F76**（第十八轮）：K50 复杂度标注治理（复杂度词汇表统一：PresetComplexity 单源+JSON 格式二选一 + graph_memory M5 修正：qsort 分支或改注释 + complexity 死字段处置 + 复杂度注释惯例起草）是否立项（P1，纯注释/文档面低风险）？

---

*附：本设计基于十八轮九十路子代理审计（每轮 5 路 ×18 = 123 组重复点），
全部为设计深化，不执行。执行顺序待用户确认。*
