# 项目内部标准统一化设计（v1.14）

> 状态：设计（2026-08-27），**仅设计不执行**
> 触发：用户发现"项目内部标准需要统一化——标准尽量少，一个需求不要多种格式"
> 输入：**十四轮共七十路**子代理并行审计（第一轮：DSL/序列化/导出/证书证明/存储配置；
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
> 第十四轮：错误注入容错/UB整数安全/递归栈深度/死锁锁顺序/宏约定类型转换）
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

---

## 4. 优先级与工作量（v1.14 更新：103 组）

| 批次 | 内容 | 工作量（估） | 风险 |
|---|---|---|---|
| **P0 死代码/冗余清理** | S2-S4 序列化冗余、E1/E2 导出去重、P4 改名、C1 删 setup.py、E11 断言参数序、E15 目录归一、L1 状态机合并、L5 删 tracked 分配器、L7 泄漏检测归一、L10 删重复文档、F1 表达式树二选一、F2 规范形、F3 字符串化、F6 atoi、G1 熔断写入口+解析安全+死错误码、G2 规格对齐、G3 命名澄清、G4 插件广播、G5 删 global_state、H1 插件命名冲突+ecosystem 文档、H2 protocol undo 空壳、H5 删 test_runner+setup.py 排除测试、I2 删第二份 Welford+死计数器、I4 删 2 套无调用方 round-trip+裸 fopen 收编、I5 层验证宏接线+2 处 P0 方向修正、J1 8 处 M6 清理接线+once_reset 补齐、J2 锁抽象单一化+9 处惰性锁迁移、J3 产物移出 git+死配置清理、J4 删 lv.utils+build/lib 镜像+顶层导出单一化、J5 lvPlugin/REL_FORMULA/守卫枚举冲突修复、K2 快速幂×3+平方因子×3 统一、K3 4 头 M5 注释修正+README 幻影 API、K5 示例教学代码处置（归档+转正+删除按 v1.9.3）、K6 lv_DEPRECATED 全覆盖+preset 双 compat 合一+黑名单、K7 静默降级 9 项修复+enable_cache 伪配置、K8 删 lvBenchmark dead API+时钟绕道改基座、K9 死开关 8 项清理+lv_PUBLIC_API 双定义修复、K10 3 处所有权注释错误修复+memory-ownership.md、K11 测试设施移出生产库+测试辅助收敛、K12 补 lv_SQRT2 系列权威宏+e 单一权威、K13 solver_symbolic factorize guard-detach+空标签展平、K14 tactic 映射单源化+删孤儿 Lean 注释+lvProofStep 改名、K15 3 个真实竞态修复（lv_config 撕裂读/g_coeff_pool TOCTOU/跨线程 lv_init）、K18 find_package 修复+导出机制统一（config.h:643 删/接线 lv_USE_SHARED/visibility hidden）、K19 容差单一表执行+激活 3 个零消费者 cfg 字段、K20 1 例实现无声明修复+幻影 API 处置+死宏清理、**K21 锁定 clang-format 版本+557 文件全量回填（含 test）+format check CI**、**K22 导出格式单源化+lvExportFormat 三合一+TransformType 单表（修 protective 拼写）+变换预设入生成源**、**K23 序列化注册表单入口扩展+删 ConstraintGraph 2 冗余路径+Module 三载体收敛（删 msgpack 丢图/LVZ 不可读）**、**K24 per-instance RNG+死生成器处置+add_point 副本闭环**、**K25 路径常量单源+共享库加载收敛+公理包 117 处测试路径收敛**、**K27 parser_safety 接线（主解析链闸门）+安全算术单一入口（lv_SAFE_ADD 收敛）+增长逻辑单一路由（IntArray 废弃）+graph_conflict +1 溢出修复**、**K28 A1-A6 死机制接线或删除+无防护点补限（formula 解析器 current_depth/lv_parser 深度/destroy 域显式栈）**、**K29 锁顺序总序文档+平台分裂修复（trylock/timedlock+MSVC 告警）+倒锁防护（registry destroy 回调移锁外/stream 自死锁）** | ~19000-26000 行删除/改名/接线 | 低-中（多为无调用方或纯删除；K13/K15/K18/K19/K23 需回归；K27 解析链接线+K28 无防护点+K29 倒锁修复需专项回归） |
| **P1 权威格式收敛** | S1 Module→JSON、E4 canonical、C2/C14 预设单一源、C4 注册表、E5 错误码桥接、E8 有理数、E13 DSL 归一、E15 CMakePresets、L2 进度模型、L4 事件契约、L6 内存统计、L8 日志级别、F4 导入共享层、F5 几何枚举四合一、F7 预设容器、G1 常量合一、G2 通用缓存层、G3 BFS/Kahn 收敛、G5 配置单一注册表、H1 后端注册单一化（承接 L3）、H3 稠密 LU 三合一+稀疏直接法入接口、H5 常量对拍 codegen、I1 graph_dot 收编+atp/smt 骨架共享、I2 计时基座单一化+统计分层、I3 增长逻辑单一路由+IntArray 废弃、I4 round-trip 基座单一化+文件 IO 收敛、I5 归属修正（dsl/module_lvz/gc_language/ecosystem/module_export/lvProofObject/proof 双轨）、J1 单一生命周期注册表、J2 原子 64 位补齐+统一、J3 CI 报告收敛+docx 生成收敛、J4 预设单一事实源+几何操作入口收敛、J5 命名规范锚定+前缀补齐（版本化）、K1 场景文案规范表+状态名表收敛+语言策略、K2 FNV 双家族+排序残留收敛、K3 @impl-* 声称标记+桩约定+对拍机制、K4 版本分层+单一事实源+读端校验+ABI 治理、K5 USE_CASES 收敛+示例同步机制、K6 退役登记表+黑名单 grep CI、K7 统一降级登记+降级语义统一+4 套求解降级收敛、K8 Welford 收敛 L0+阈值基线表+CI 串行性能 job、K9 feature_gates.h 单一表+_WIN32 迁移 lv_PLATFORM_*、K10 [copy]/[take]/[borrow] 头注释全覆盖+静态检查脚本、K11 mock 机制统一+预言机收敛、K12 字面量收敛（M_PI 30+ 处延伸+角度宏接活）+黄金比去重、K13 锁守卫统一 lv_DEFER+标签规范+清理重复收敛、K14 证明导出锚定 L5+SVG/TikZ 收敛+序列化注册表接入+层归属锚定、K15 全局 getter 化+TLS 容器化+锁补全（G2-G4/G7）+容器收敛、K16 前向声明集中 lv_fwd.h+引用即包含（消 7 传递依赖）+拼写统一、K17 sanitizer 矩阵统一（fuzz 补 UBSan）+corpus 管理+回归进 CI、K18 lv.pc 修复+版本单源+发布物真正产出（CI package 步骤）、K19 测试断言容差表+1e-15 相对化+跨语言收敛、K20 -Wmissing-prototypes 启用+一致性检查脚本+死宏清理、**K21 clang-format 豁免登记制度+Doxygen 对齐+命名/格式合并立项**、**K22 命令枚举对齐+序列化键单表化+协议契约 exempt 标注**、**K23 跨格式 round-trip 测试+OPML 不对称修复+比较器强化**、**K24 确定性 seed 套件级+默认 seed 单一化+构造辅助收敛**、**K25 预设单源+测试输出目录对齐+bootstrap 单基准+资源三级解析**、**K26 内置失败分配器 lv_allocator_fail+OOM 错误路径测试补全+注入盲区修复（tracked 走 vtable/memory_pool 改 lv_malloc）**、**K27 sanitizer 覆盖 ctest（address,undefined）+窄化强制（lv_ASSERT_FITS_INT）+__int128 守卫补齐**、**K28 depth_limits.h 单一权威深度限制表（推理 100/1000/10000 合一+128 四重复合并+LV_DESTROY_MAX_DEPTH 裁决）+无防护点补限（P1 档：lambda_unify 5 族/dsl_compiler_ir/lv_sema/render_*）+栈帧治理**、**K29 锁抽象收敛（runtime_guard 弃用或启用+日志 3 锁 2 管道+组同步 2 套+后端注册表锁 4 份）+潜伏自锁链修复（memory_pool stats_mutex）+粒度统一（lvRegistry/groebner 惯用法）+TSAN 回归**、**K30 宏规范锚定（lv_ARRAY_SIZE 单源+X 命名统一/lv_PUBLIC_API 三定义收敛+宏 lint）+强转检查（lv_cast_size_to_int/装箱拆箱 helper/const 版本）+X-macro 补全（_Static_assert 铺开+硬编码计数 6/12 消除+algebra_mode 别名 switch 引用公共 API）+类型擦除收尾（solver_order 迁 lv_topo_run）+CI 枚举对齐检查** | ~26000-37000 行改动 | 中（需回归测试；J5 前缀补齐+K4 版本校验+K9 开关迁移+K15 锁补全+K18 导出统一+K23 注册表改造+K26 注入+K29 锁抽象+K30 宏改名破坏性） |
| **P2 语言统一** | D1-D2：.lv 吸收 dsl_compiler + .lvz 职责收敛 + 语法糖第一批 + L11 语法单一事实源 | ~1800-3000 行 | 中高（语法面） |
| **P3 证明/API/推理 IR 统一** | P1-P3 证明 IR、E6 返回码、E7 API 入口、E12 测试入口、L3 推理注册表、F8 验证入口、F9 策略调度、F10 引擎栈分层、H2 快照分层文档化、I3 FIFO 队列族收敛、J2 并行骨架评估（L9 前端接内核已移出） | ~4000-6000 行 | 中高（引擎/证明） |
| **P4 项目级合并** | C3 Lean 合并、E9 代数数桥接、E10 区间语义、E14 预设 v3→v4、L10 文档合并、H4 公理单一事实源+formal 去重+CI 对齐 | 视工具链 | 高（外部工具链/形式化） |

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

---

*附：本设计基于十四轮七十路子代理审计（每轮 5 路 ×14 = 103 组重复点），
全部为设计深化，不执行。执行顺序待用户确认。*
