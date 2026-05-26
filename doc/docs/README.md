# Lv-00 几何元语言系统 v3.5.0-academic - 模块文档索引

## 概述

Lv-00 是一门以几何为唯一载体的双模数学元语言。几何体本身是计算的执行者、数据的承载者、证明的见证者。

**核心特性**：
- **几何即符号**：点、线、区域本身就是程序的实体
- **数形不二**：数值只能是几何量
- **构造即证明**：构造是否构成证明取决于合一检查
- **公理中立**：内核不内建距离、角度概念
- **可演进**：公理系统可升级，定理可固化为新规则

## 文档结构

本文档目录包含 Lv-00 v3.4-academic 全部核心模块的详细描述：

### 核心模块

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [01_symbolic_coord.md](01_symbolic_coord.md) | 符号坐标系统 | 有理数、代数数、二次根式、超越常数的精确表示和运算 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图核心 | 点、线段、区域、端口、函数块节点及约束管理 |
| [03_normalization.md](03_normalization.md) | 图规范化遍引擎 | 点合并、线段合并、区域合并、幂等性保证 |
| [04_solver.md](04_solver.md) | 符号代数求解器 | 代数方程转化、Gröbner基求解、多解处理 |
| [05_rewrite.md](05_rewrite.md) | 图重写引擎 | 匹配算法、替换操作、循环检测 |
| [06_unify.md](06_unify.md) | 合一检查 | 端口匹配、约束匹配、坐标判等 |

### 高级功能

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [07_func_block.md](07_func_block.md) | 函数块系统 | 打包操作、确定性检查、多解选择器、组合子 |
| [08_type_system.md](08_type_system.md) | 类型系统 | 宇宙层级、类型等价检查、类型推断 |
| [09_proof.md](09_proof.md) | 命题与证明系统 | 命题模式、证明导航器、爆炸原理 |
| [10_recursion.md](10_recursion.md) | 递归与条件 | 测度系统、选择器块、递归深度监控 |
| [11_wfc_paradigm.md](11_wfc_paradigm.md) | 波函数坍缩范式 | WFC 约束生成与几何构造 |

### 架构与工程文档

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [12_context_and_lifecycle.md](12_context_and_lifecycle.md) | 上下文与生命周期 | 引擎上下文管理、资源生命周期 |
| [13_error_handling.md](13_error_handling.md) | 错误处理 | 错误码体系、错误传播、恢复策略 |
| [14_memory_management.md](14_memory_management.md) | 内存管理 | 内存池、分配策略、泄漏检测 |

### 五层架构详细文档

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [15_parsing_layer.md](15_parsing_layer.md) | 解析层 (Layer 1) | 词法分析、语法解析、DSL 编译、AST 生成 |
| [16_geometry_layer.md](16_geometry_layer.md) | 几何层 (Layer 3) | 约束图、符号坐标、规范化、几何拓扑 |
| [17_reasoning_layer.md](17_reasoning_layer.md) | 推理层 (Layer 4) | 求解器、重写、证明、SMT/ATP 后端 |
| [18_output_layer.md](18_output_layer.md) | 输出层 (Layer 5) | TikZ 导出、跨语言互操作、证明格式化 |
| [19_numerical_backends.md](19_numerical_backends.md) | 数值后端 | 区间算术、浮点误差、Herbie/FPTaylor/Gappa |
| [20_preset_registry.md](20_preset_registry.md) | 预设函数块注册表 | 63个领域预设函数块完整索引 |

### 代码驱动补充文档（文档 12~22）

> 以下文档由代码扫描驱动创建，覆盖仅存在于源代码中、此前未被独立文档化的模块。

| 文档 | 模块 | 覆盖头文件 | 功能描述 |
|------|------|-----------|----------|
| [12_context_and_lifecycle.md](12_context_and_lifecycle.md) | 上下文与生命周期 | context.h, circuit_breaker.h, runtime_guard.h, status_codes.h, node_deep_copy.h | 引擎上下文管理、熔断器、运行时守卫、节点深拷贝 |
| [13_proof_engine_enhanced.md](13_proof_engine_enhanced.md) | 增强证明引擎 | proof_engine_enhanced.h, proof_session.h, proof_score.h, proof_priority.h, proof_rule_engine.h, proof_version.h | 多策略证明引擎、证明会话、评分、优先级、规则引擎 |
| [14_solver_backends.md](14_solver_backends.md) | 求解器后端 | solver_core.h, smt_backend.h, smt_bitvector.h, smt_theory_combiner.h, smt_trigger_engine.h, atp_backend.h, bdd_encoding.h, sat_encoding.h, approx_counter.h, engine_scheduler.h | SMT/ATP/SAT/BDD 多后端、引擎调度 |
| [15_geometry_advanced.md](15_geometry_advanced.md) | 高级几何模块 | geometry_types.h, geometry_transform.h, geometry_compress.h, high_dim.h, interactive_geo.h, geom_evol.h, geo_event_detect.h, geo_invariant_type.h, geo_spec.h, geo_topology.h | 几何变换、压缩、高维、交互式、演化、拓扑 |
| [16_logic_verification.md](16_logic_verification.md) | 逻辑验证 | logic_check.h, prop_verifier.h, three_valued_logic.h, modal_operators.h, quantifier.h, meta_proof.h, herbie_eval.h, fptaylor_eval.h, gappa_dsl.h, gappa_propagate.h | 逻辑检查、命题验证、三值逻辑、模态算子、元证明 |
| [17_numerical_analysis.md](17_numerical_analysis.md) | 数值分析 | interval_arithmetic.h, float_error.h, herbie_eval.h, fptaylor_eval.h, gappa_dsl.h, gappa_propagate.h, inequality_reasoning.h, numerical_backend.h, probabilistic_constraint.h, ode_solver.h, autodiff.h | 区间算术、浮点误差、Herbie/FPTaylor/Gappa、ODE求解、自动微分 |
| [18_formula_dsl_ga.md](18_formula_dsl_ga.md) | 公式DSL与几何代数 | formula_parser.h, formula_renderer.h, formula_converter.h, dsl_compiler.h, lexer_shared.h, math_input.h, parser_safety.h, gc_language.h, expr_canonical.h, expr_canon.h, simd_ops.h, benchmark.h, stream.h, stream_context_util.h, math_protocol.h | 公式解析/渲染/转换、DSL编译、GC语言、表达式规范化、SIMD |
| [19_axiom_rewrite_export.md](19_axiom_rewrite_export.md) | 公理·重写·导出 | axiom_pkg.h, axiom_rule_engine.h, axiom_grade.h, rewrite.h, rewrite_strategy.h, proof_export_enhanced.h, relation_model.h, sym_expr.h, fast_index.h, ecosystem.h | 公理包、规则引擎、重写策略、增强导出、符号表达式 |
| [20_preset_registry.md](20_preset_registry.md) | 预设函数块注册表 | func_block_preset.h, func_block_preset_ops.h, func_block_registry.h, func_block_utils.h, preset_*.h (63个) | 63个领域预设函数块完整索引 |
| [21_euclidean_geometry.md](21_euclidean_geometry.md) | 欧氏几何公理包 | euclidean_geometry.h | Hilbert五公理组、Birkhoff/Tarski等价性、EquivalenceProofChain |
| [22_proof_export_trace_widget.md](22_proof_export_trace_widget.md) | 证明导出·追踪·可视化 | proof_export_enhanced.h, proof_trace.h, proof_widget.h | 6格式导出、证明追踪树、8种可视化组件 |
| [23_core_infrastructure.md](23_core_infrastructure.md) | 核心基础设施与配置 | lv00.h, config.h, error_codes.h, debug.h, cross_platform.h, module.h, memory_pool.h | 系统入口、配置管理、错误处理、跨平台抽象 |
| [24_constraint_propagation.md](24_constraint_propagation.md) | 约束传播与等价类 | propagation.h, equiv_class.h, graph_hash.h, probabilistic_constraint.h | WFC传播、AC-3弧相容、等价类合并、图哈希、概率约束 |
| [25_engine_scheduler.md](25_engine_scheduler.md) | 引擎核心与调度系统 | engine.h, engine_scheduler.h | 五状态机、多引擎调度、自动路由决策、回退链 |
| [26_interactive_geometry.md](26_interactive_geometry.md) | 交互式几何与事件系统 | interactive_geo.h, geo_event_detect.h, geo_topology.h, geo_invariant_type.h | 交互几何、事件检测、单纯复形拓扑 |
| [27_quantifier_logic.md](27_quantifier_logic.md) | 量词与关系逻辑 | quantifier.h, relation_model.h | 量词系统、Alloy风格关系模型 |
| [28_number_theory.md](28_number_theory.md) | 数论与多项式系统 | nt_number_theory.h, nt_polynomial.h, mpz_poly.h, rational.h | GMP数论、整数多项式、结式、精确有理数 |
| [29_inequality_approximation.md](29_inequality_approximation.md) | 不等式推理与近似计算 | inequality_reasoning.h, approx_counter.h, herbie_eval.h, fptaylor_eval.h | 符号不等式、近似模型计数、Herbie/FPTaylor误差评估 |
| [30_performance_concurrency.md](30_performance_concurrency.md) | 性能优化与并发系统 | thread_pool.h, simd_ops.h, benchmark.h, test_framework.h, fast_index.h | 线程池、SIMD、基准测试、测试框架、高效索引 |
| [31_stream_interop.md](31_stream_interop.md) | 流处理与互操作系统 | stream.h, stream_context_util.h, interop.h | 实时事件流、上下文分发、外部格式互操作 |
| [32_runtime_monitoring.md](32_runtime_monitoring.md) | 运行时监控与生态系统 | runtime_guard.h, runtime_monitor.h, ecosystem.h, magic.h | 运行时防护、结构化日志、健康检查、生态包、魔法映射 |
| [33_gappa_verification.md](33_gappa_verification.md) | Gappa浮点验证与解析安全 | gappa_dsl.h, gappa_propagate.h, parser_safety.h, path_type.h, algebra_mode.h | Gappa DSL、谓词传播、解析安全、路径类型、代数模式 |
| [34_meta_proof_cache.md](34_meta_proof_cache.md) | 元证明与推理缓存 | meta_proof.h, prop_verifier.h, reasoning_cache.h, node_deep_copy.h | 剪枝合法性元证明、BHK命题验证、推理缓存、节点深拷贝 |

## 模块依赖关系

```
符号坐标系统 (01)
    ↓
约束图核心 (02)
    ↓
    ├─→ 图规范化遍引擎 (03)
    ├─→ 符号代数求解器 (04)
    ├─→ 图重写引擎 (05)
    └─→ 合一检查 (06)
            ↓
    ├─→ 函数块系统 (07)
    ├─→ 类型系统 (08)
    ├─→ 命题与证明系统 (09)
    └─→ 递归与条件 (10)
```

## 快速参考

### 信任颜色图例

| 颜色 | 含义 |
|------|------|
| 🟢 绿色 | 全构造，无任何非常规依赖 |
| 🔵 蓝色 | 待完成的证明义务（未探索/资源受限/超出范围） |
| 🟢 绿色实框 | 已证不可构造 |
| 🟡 黄色虚线框 | 条件性不可构造 |
| 🟠 浅橙色实心 | 依赖非构造性 oracle |
| 🟠 浅橙色虚线 | 爆炸原理步骤 |
| 🟡 橙黄色 | 含数值假设 |
| 🟠 深橙色 | 非构造性依赖与数值假设叠加 |

### 核心 API 速查

**引擎初始化**:
```c
LV00Engine *engine = engine_create();
engine_load_axiom_package(engine, "euclidean.lvz");
```

**创建几何体**:
```c
// 创建点
AddNodeResult result = graph_add_point(graph, x, y);
int point_id = graph->next_node_id - 1;

// 创建线段
int endpoints[] = {point1_id, point2_id};
result = graph_add_line_segment(graph, endpoints, 2);
```

**添加约束**:
```c
int participants[] = {point_id, line_id};
graph_add_constraint(graph, CONSTRAINT_INCIDENCE, participants, 2);
```

**打包函数块**:
```c
FuncBlock *fb;
PackResult result = func_block_pack(
    graph,
    internal_nodes, 2,
    input_ports, 1,
    output_ports, 1,
    NULL, 0,
    &fb
);
```

**合一检查**:
```c
Proposition *prop = proposition_create(1, PROPOSITION_ATOMIC);
proposition_set_pattern(prop, pattern_graph);

UnifyResult result = proof_unify(construction_graph, prop, true);
if (result == UNIFY_OK) {
    printf("命题得证！\n");
}
```

## 文件结构

```
Lv-00/
├── include/lv00/          # 头文件
│   │
│   │  ── 公共API入口 ──────────────────────────────────
│   ├── lv00.h                  # 主头文件
│   ├── config.h                # 编译配置
│   ├── cross_platform.h        # 跨平台宏
│   ├── error_codes.h           # 错误码定义
│   ├── lv00_utils.h            # 通用工具
│   ├── lv00_numeric.h          # 数值类型工具
│   └── lv00_internal.h         # 内部接口
│   │
│   │  ── 引擎与上下文 ────────────────────────────────
│   ├── engine.h                # 主引擎
│   ├── engine_scheduler.h      # 引擎调度器
│   ├── context.h               # 上下文管理
│   ├── circuit_breaker.h       # 熔断器
│   ├── runtime_guard.h         # 运行时守卫
│   └── status_codes.h          # 状态码定义
│   │
│   │  ── 解析层 (Layer 1) ────────────────────────────
│   ├── formula_parser.h        # 公式解析器
│   ├── formula_renderer.h      # 公式渲染器
│   ├── formula_converter.h     # 公式转换器
│   ├── dsl_compiler.h          # DSL 编译器
│   ├── lexer_shared.h          # 共享词法器
│   ├── math_input.h            # 数学输入处理
│   └── parser_safety.h         # 解析安全检查
│   │
│   │  ── 资源层 (Layer 2 / shared) ──────────────────
│   ├── debug.h                 # 调试工具
│   ├── module.h                # 模块系统
│   ├── memory_pool.h           # 内存池
│   ├── runtime_monitor.h       # 运行时监控
│   ├── node_deep_copy.h        # 节点深拷贝
│   ├── test_framework.h        # 测试框架
│   └── exact_arithmetic.h      # 精确算术
│   │
│   │  ── 几何层 (Layer 3) ────────────────────────────
│   ├── constraint_graph.h      # 约束图
│   ├── symbolic_coord.h        # 符号坐标
│   ├── normalization.h         # 规范化
│   ├── graph_hash.h            # 图哈希
│   ├── mpz_poly.h              # 多精度多项式
│   ├── geometry_types.h        # 几何类型定义
│   ├── geometry_transform.h    # 几何变换
│   ├── geometry_compress.h     # 几何压缩
│   ├── euclidean_geometry.h    # 欧氏几何
│   ├── high_dim.h              # 高维几何
│   ├── interactive_geo.h       # 交互式几何
│   ├── geom_evol.h             # 几何演化
│   ├── geo_event_detect.h      # 几何事件检测
│   ├── geo_invariant_type.h    # 几何不变量类型
│   ├── geo_spec.h              # 几何规格
│   ├── geo_topology.h          # 几何拓扑
│   ├── equiv_class.h           # 等价类
│   └── propagation.h           # 传播引擎
│   │
│   │  ── 推理层 (Layer 4) ────────────────────────────
│   ├── solver.h                # 求解器
│   ├── solver_core.h           # 求解器核心
│   ├── rewrite.h               # 重写引擎
│   ├── rewrite_strategy.h      # 重写策略
│   ├── unify.h                 # 合一检查
│   ├── proof.h                 # 证明系统
│   ├── proof_engine_enhanced.h # 增强证明引擎
│   ├── proof_priority.h        # 证明优先级
│   ├── proof_rule_engine.h     # 证明规则引擎
│   ├── proof_score.h           # 证明评分
│   ├── proof_session.h         # 证明会话
│   ├── proof_trace.h           # 证明追踪
│   ├── proof_version.h         # 证明版本
│   ├── proof_export_enhanced.h # 增强证明导出
│   ├── proof_widget.h          # 证明可视化组件
│   ├── axiom_pkg.h             # 公理包
│   ├── axiom_rule_engine.h     # 公理规则引擎
│   ├── axiom_grade.h           # 公理评级
│   ├── func_block.h            # 函数块
│   ├── func_block_internal.h   # 函数块内部
│   ├── func_block_preset.h     # 函数块预设
│   ├── func_block_preset_ops.h # 函数块预设操作
│   ├── func_block_registry.h   # 函数块注册表
│   ├── func_block_utils.h      # 函数块工具
│   ├── type_system.h           # 类型系统
│   ├── recursion.h             # 递归与条件
│   ├── three_valued_logic.h    # 三值逻辑
│   ├── modal_operators.h       # 模态算子
│   ├── quantifier.h            # 量词
│   ├── path_type.h             # 路径类型
│   ├── logic_check.h           # 逻辑检查
│   ├── prop_verifier.h         # 命题验证器
│   ├── meta_proof.h            # 元证明
│   ├── stream.h                # 流式输出
│   ├── stream_context_util.h   # 流上下文工具
│   ├── math_protocol.h         # 数学协议
│   ├── relation_model.h        # 关系模型
│   ├── sym_expr.h              # 符号表达式
│   ├── smt_backend.h           # SMT 后端
│   ├── smt_bitvector.h         # SMT 位向量
│   ├── smt_theory_combiner.h   # SMT 理论组合器
│   ├── smt_trigger_engine.h    # SMT 触发引擎
│   ├── atp_backend.h           # ATP 后端
│   ├── sat_encoding.h          # SAT 编码
│   ├── bdd_encoding.h          # BDD 编码
│   ├── sparse_linear_algebra.h # 稀疏线性代数
│   ├── approx_counter.h        # 近似计数器
│   ├── groebner_engine.h       # Groebner 引擎
│   ├── algebra_mode.h          # 代数模式
│   ├── rational.h              # 有理数
│   ├── nt_number_theory.h      # 数论
│   ├── nt_polynomial.h         # 数论多项式
│   ├── interval_arithmetic.h   # 区间算术
│   ├── float_error.h           # 浮点误差
│   ├── fptaylor_eval.h         # FPTaylor 评估
│   ├── herbie_eval.h           # Herbie 评估
│   ├── gappa_dsl.h             # Gappa DSL
│   ├── gappa_propagate.h       # Gappa 传播
│   ├── inequality_reasoning.h  # 不等式推理
│   ├── numerical_backend.h     # 数值后端
│   ├── probabilistic_constraint.h # 概率约束
│   ├── ode_solver.h            # ODE 求解器
│   ├── autodiff.h              # 自动微分
│   ├── gc_language.h           # GC 语言
│   ├── expr_canonical.h        # 表达式规范化
│   ├── expr_canon.h            # 表达式规范化(旧)
│   ├── fast_index.h            # 快速索引
│   ├── simd_ops.h              # SIMD 操作
│   ├── benchmark.h             # 基准测试
│   ├── thread_pool.h           # 线程池
│   ├── magic.h                 # 魔法模块
│   └── ecosystem.h             # 生态系统
│   │
│   │  ── 输出层 (Layer 5) ────────────────────────────
│   ├── tikz_export.h           # TikZ 导出
│   └── interop.h               # 跨语言互操作
│   │
│   │  ── 预设函数块库 ────────────────────────────────
│   ├── preset_core.h           # 预设核心
│   └── preset_*.h              # 63个领域预设（详见 20_preset_registry.md）
│
├── src/                   # 源文件
├── tests/                 # 测试文件
├── docs/                  # 文档目录（本目录）
│   ├── README.md          # 本文档
│   ├── 01_symbolic_coord.md
│   ├── 02_constraint_graph.md
│   ├── 03_normalization.md
│   ├── 04_solver.md
│   ├── 05_rewrite.md
│   ├── 06_unify.md
│   ├── 07_func_block.md
│   ├── 08_type_system.md
│   ├── 09_proof.md
│   ├── 10_recursion.md
│   ├── 11_wfc_paradigm.md
│   ├── 12_context_and_lifecycle.md
│   ├── 13_error_handling.md
│   ├── 14_memory_management.md
│   ├── 15_parsing_layer.md
│   ├── 16_geometry_layer.md
│   ├── 17_reasoning_layer.md
│   ├── 18_output_layer.md
│   ├── 19_numerical_backends.md
│   ├── 20_preset_registry.md
│   ├── 21_euclidean_geometry.md
│   ├── 22_proof_export_trace_widget.md
│   ├── 23_core_infrastructure.md
│   ├── 24_constraint_propagation.md
│   ├── 25_engine_scheduler.md
│   ├── 26_interactive_geometry.md
│   ├── 27_quantifier_logic.md
│   ├── 28_number_theory.md
│   ├── 29_inequality_approximation.md
│   ├── 30_performance_concurrency.md
│   ├── 31_stream_interop.md
│   ├── 32_runtime_monitoring.md
│   ├── 33_gappa_verification.md
│   └── 34_meta_proof_cache.md
└── CMakeLists.txt         # 构建配置
```

## 编译说明

### 依赖

- C11 编译器
- GMP 库 (GNU Multiple Precision Arithmetic Library)

### 编译步骤

```bash
mkdir build && cd build
cmake ..
make
```

### 运行测试

```bash
./test_basic
./test_comprehensive
```

## 设计原则

### 公理中立

内核只提供关联几何引擎（点、线、区域的关联、之间、相交、包含、连接关系），不内建距离、角度概念。欧氏几何、双曲几何、集合论、类型论全都是可加载的"公理系统包"。

### 信任边界

- **第一可信基**：C内核（关联几何引擎、符号坐标系统、求解器、重写引擎）
- **第二可信基**：核心公理包的安全重写规则集（需外部汇合性证明）

### 自举路线

1. C实现内核
2. 公理系统编辑器
3. 微自举A：几何可表达性验证
4. 微自举B：几何证明能力验证
5. 可行性原型：λ-演算几何表示
6. 首次自举：命题逻辑验证器

## 版本历史

- **v3.5.0-academic** (当前版本)
  - 五层单向依赖学术架构（解析层 -> 几何公理层 -> 约束拓扑层 -> 推理层 -> 输出层）
  - 完整实现 122+ 核心头文件模块
  - 核心基础设施：配置系统、错误处理、跨平台抽象
  - 约束传播引擎：WFC范式、AC-3弧相容、等价类管理
  - 引擎调度系统：五状态机、自动路由决策、回退链机制
  - 函数块系统：打包、例化、确定性检查、预设注册表
  - 类型系统：宇宙层级、类型等价检查
  - 命题与证明系统：合一检查、证明导航器、多策略推理
  - 递归与条件：测度系统、选择器块
  - SMT/ATP/SAT/BDD 多后端推理支持
  - 数值后端：区间算术、Herbie、FPTaylor、Gappa
  - 63个领域预设函数块

- **v3.4-academic**
  - 五层单向依赖学术架构
  - 完整实现 122+ 核心头文件模块
  - 函数块系统：打包、例化、确定性检查、预设注册表
  - 类型系统：宇宙层级、类型等价检查
  - 命题与证明系统：合一检查、证明导航器、多策略推理
  - 递归与条件：测度系统、选择器块
  - SMT/ATP/SAT/BDD 多后端推理支持
  - 数值后端：区间算术、Herbie、FPTaylor、Gappa
  - 63个领域预设函数块

- **v3.0.0**
  - 完整实现18个核心模块
  - 函数块系统：打包、例化、确定性检查
  - 类型系统：宇宙层级、类型等价检查
  - 命题与证明系统：合一检查、证明导航器
  - 递归与条件：测度系统、选择器块

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](../LICENSE) 文件

## 参考文献

- 设计文档：`设计.txt`
- 规划文档：`规划.txt`
- 系统描述：`Lv-00系统描述文档.md`
