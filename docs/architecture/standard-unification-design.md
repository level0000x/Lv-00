# 项目内部标准统一化设计（v1.7）

> 状态：设计（2026-08-27），**仅设计不执行**
> 触发：用户发现"项目内部标准需要统一化——标准尽量少，一个需求不要多种格式"
> 输入：**七轮共三十五路**子代理并行审计（第一轮：DSL/序列化/导出/证书证明/存储配置；
> 第二轮：错误码API/坐标代数/测试框架/Python绑定/工具链脚本；
> 第三轮：引擎生命周期/推理后端/流协议/内存日志/前端文档；
> 第四轮：公式表达式/导入解析/类型系统/基础工具/证明内部；
> 第五轮：安全限制/缓存/图算法/事件回调/配置全局；
> 第六轮：插件系统/快照回滚/数值线性代数/形式化对齐/Python测试结构；
> 第七轮：进程网络/性能监控/数据结构/IO序列化/层间依赖）
> 原则：**标准尽量少，一个需求一种格式**；不同需求允许不同格式，但不得同需求多格式
>
> **v1.1**：第二轮 15 组（总 30）。**v1.2**：第三轮 13 组（总 43）。
> **v1.3**：前端**刻意留白**（等内核定型），L9 移出执行清单。
> **v1.4**：第四轮 10 组（总 53）。**v1.5**：第五轮 5 组（总 58）。
> **v1.6**：第六轮 5 组（总 63）。
> **v1.7（2026-08-27）**：第七轮五路新增 5 组（总 **68 组**），
> 本版并入 §1.31-1.35 与 §3.31-3.35。

---

## 1. 现状总览：一需求多格式清单

七轮共三十五路审计发现 **68 组"一个需求多种格式"重复点**：

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

---

## 4. 优先级与工作量（v1.7 更新：68 组）

| 批次 | 内容 | 工作量（估） | 风险 |
|---|---|---|---|
| **P0 死代码/冗余清理** | S2-S4 序列化冗余、E1/E2 导出去重、P4 改名、C1 删 setup.py、E11 断言参数序、E15 目录归一、L1 状态机合并、L5 删 tracked 分配器、L7 泄漏检测归一、L10 删重复文档、F1 表达式树二选一、F2 规范形、F3 字符串化、F6 atoi、G1 熔断写入口+解析安全+死错误码、G2 规格对齐、G3 命名澄清、G4 插件广播、G5 删 global_state、H1 插件命名冲突+ecosystem 文档、H2 protocol undo 空壳、H5 删 test_runner+setup.py 排除测试、I2 删第二份 Welford+死计数器、I4 删 2 套无调用方 round-trip+裸 fopen 收编、I5 层验证宏接线+2 处 P0 方向修正 | ~4500-6500 行删除/改名/接线 | 低-中（多为无调用方或纯删除；I5 两处方向修正需回归） |
| **P1 权威格式收敛** | S1 Module→JSON、E4 canonical、C2/C14 预设单一源、C4 注册表、E5 错误码桥接、E8 有理数、E13 DSL 归一、E15 CMakePresets、L2 进度模型、L4 事件契约、L6 内存统计、L8 日志级别、F4 导入共享层、F5 几何枚举四合一、F7 预设容器、G1 常量合一、G2 通用缓存层、G3 BFS/Kahn 收敛、G5 配置单一注册表、H1 后端注册单一化（承接 L3）、H3 稠密 LU 三合一+稀疏直接法入接口、H5 常量对拍 codegen、I1 graph_dot 收编+atp/smt 骨架共享、I2 计时基座单一化+统计分层、I3 增长逻辑单一路由+IntArray 废弃、I4 round-trip 基座单一化+文件 IO 收敛、I5 归属修正（dsl/module_lvz/gc_language/ecosystem/module_export/lvProofObject/proof 双轨） | ~6000-9000 行改动 | 中（需回归测试） |
| **P2 语言统一** | D1-D2：.lv 吸收 dsl_compiler + .lvz 职责收敛 + 语法糖第一批 + L11 语法单一事实源 | ~1800-3000 行 | 中高（语法面） |
| **P3 证明/API/推理 IR 统一** | P1-P3 证明 IR、E6 返回码、E7 API 入口、E12 测试入口、L3 推理注册表、F8 验证入口、F9 策略调度、F10 引擎栈分层、H2 快照分层文档化、I3 FIFO 队列族收敛（L9 前端接内核已移出） | ~4000-6000 行 | 中高（引擎/证明） |
| **P4 项目级合并** | C3 Lean 合并、E9 代数数桥接、E10 区间语义、E14 预设 v3→v4、L10 文档合并、H4 公理单一事实源+formal 去重+CI 对齐 | 视工具链 | 高（外部工具链/形式化） |

> v1.7 工作量上调主因：第七轮新增 I1-I5 涉及层验证机制修复（P0 顶层问题）、
> 计时/统计基座单一化、增长逻辑单一路由、round-trip 基座收敛、归属修正
> （8 处解析/证明/导出/注册归属）等广面改动；I2-I4 另确认日志管道/JSON 序列化/
> 哈希表/堆/回调列表已收敛为健康基线。

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

---

*附：本设计基于七轮三十五路子代理审计（每轮 5 路 ×7 = 68 组重复点），
全部为设计深化，不执行。执行顺序待用户确认。*
