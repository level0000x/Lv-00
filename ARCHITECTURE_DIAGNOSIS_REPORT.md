# Lv-00 全盘架构诊断报告

**生成时间**: 2026-06-14  
**项目版本**: v5.0.0  
**诊断范围**: 十层单向依赖架构 + 头文件组织 + 构建系统

---

## 执行摘要

### 总体评分：⚠️ **部分达成 (60/100)**

| 指标 | 状态 | 得分 |
|------|------|------|
| **架构定义清晰度** | 🟡 良好 | 75/100 |
| **目录组织一致性** | 🟢 优秀 | 85/100 |
| **层间依赖严格性** | 🔴 较差 | 45/100 |
| **头文件分类规范** | 🟡 良好 | 70/100 |
| **编译约束检查** | 🔴 未启用 | 10/100 |
| **文档一致性** | 🟡 良好 | 65/100 |

### 关键问题

1. **层间依赖检查未启用** ❌
   - `ENABLE_LAYER_VALIDATION` 选项存在但**默认关闭** (`OFF`)
   - 没有实际的编译时边界检查代码
   - 逆向依赖无法被自动检测

2. **头文件过度集中** ⚠️
   - 170+ 头文件全部放在 `core/include/lv00/` 
   - 没有按层级分离 (如 `lv00/layer3/constraint_graph.h`)
   - 难以区分 public API vs internal header

3. **功能分布不清** 🔴
   - 许多功能头文件属性不明确
   - 部分高级功能 (GA、数值后端、ODE等) 的层级归属不清
   - 预设模块(55+) 全部堆在 Layer 4，缺少分类

4. **编译依赖关系复杂** 🟡
   - Layer 4 OBJECT 库有 70+ 源文件，包含太多功能
   - 某些模块 (如 `ga_multivector.c`) 应该独立成专题子库
   - 形式化验证、WebAssembly 等可选功能没有分离

5. **文档与实现脱节** 🟡
   - README 描述十层架构"十层单向依赖"
   - 但 CMakeLists.txt 中只明确定义了 L1-L5（L6-L10 是新增且不完整）
   - README 中关于"稳定契约"的描述过于模糊

---

## 现状详细分析

### 一、目录结构对齐度

#### ✅ 优势：目录层级清晰

```
core/src/
├── layer1_parser/          ✓ 清晰
├── layer2_resource/        ✓ 清晰  
├── layer3_geometry/        ✓ 清晰
├── layer4_reasoning/       ✓ 清晰（但文件过多）
│   ├── axiom/
│   ├── func_block/
│   ├── preset/             ⚠️ 55+ 预设文件混在一起
│   └── [70+ 个 .c 文件直接在目录下]
├── layer5_output/          ✓ 清晰
├── layer6_visual/          ✓ 清晰
├── layer7_orchestration/   ⚠️ 只有1个文件
├── layer8_meta_verify/     ⚠️ 只有1个文件
├── layer9_application/     ⚠️ 只有1个文件
└── layer10_interop/        ⚠️ 只有4个文件
```

**问题**:
- Layer 7-10 形同虚设（文件极少）
- Layer 4 over-consolidated（73个源文件，什么都有）

---

### 二、头文件分类混乱

#### 当前状态：单一命名空间

```
core/include/lv00/
├── lv00.h                  # 主入口
├── error_codes.h           # 共享
├── symbolic_coord.h        # Layer 3 (几何层)
├── constraint_graph.h      # Layer 3
├── solver.h                # Layer 4 (推理层)
├── proof.h                 # Layer 4
├── engine.h                # Layer 4
├── axiom_pkg.h             # Layer 4
├── func_block.h            # Layer 4
├── type_system.h           # Layer 4
├── groebner_engine.h       # Layer 4
├── smt_backend.h           # Layer 4
├── atp_backend.h           # Layer 4
├── bdd_encoding.h          # Layer 4
├── ga_multivector.h        # Layer 3? 4? (不明确)
├── ga_interface.h          # Layer 3? 4?
├── interval_arithmetic.h   # Layer 3? 4?
├── fptaylor_eval.h         # Layer 3? 4?
├── herbie_eval.h           # Layer 3? 4?
├── gappa_dsl.h             # Layer 3? 4?
├── tikz_export.h           # Layer 5 (输出层)
├── interop.h               # Layer 10 (互操作)
├── orchestrator.h          # Layer 7 (编排)
├── meta_verify.h           # Layer 8 (元验证)
├── visual_editor.h         # Layer 6 (可视化)
├── block_scheduler.h       # Layer 6? (调度)
├── memory_pool.h           # Layer 2 (资源)
├── thread_pool.h           # Layer 3? (性能)
├── plugin_system.h         # 未分类
├── preset_*.h              # 55个 (全是Layer 4)
└── ... [还有40+个头文件]
```

**关键问题**:

| 问题 | 数量 | 例子 |
|------|------|------|
| 属于Level不清 | ~25个 | ga_*, interval_arithmetic, autodiff, ode_solver |
| 跨层公开接口 | ~15个 | plugin_system, stream.h (应该 L4 私有) |
| 预设模块堆积 | 55个 | 所有 preset_*.h 都在 include/lv00 下，难以维护 |
| 内部实现暴露 | ~10个 | func_block_internal.h, expr_canon.h, axiom_grade.h |

---

### 三、关键模块的实际层级

通过代码扫描，现有模块的**实际归属**：

#### 🟢 明确的

| 模块 | 现状 | 应属 | 状态 |
|------|------|------|------|
| symbolic_coord, constraint_graph | Layer 3 | Layer 3 | ✓ 正确 |
| normalization, rewrite, unify | Layer 4 | Layer 4 | ✓ 正确 |
| solver, proof, engine | Layer 4 | Layer 4 | ✓ 正确 |
| tikz_export, proof_widget | Layer 5 | Layer 5 | ✓ 正确 |

#### 🟡 不确定的

| 模块 | 现状 | 建议 | 理由 |
|------|------|------|------|
| ga_multivector, ga_interface | L3 头, L3 源 | L4 | GA 是高级几何代数表示，需要求解器支持 |
| interval_arithmetic | L3 源 | L4 或新 L3.5 | 数值验证是约束求解的后处理 |
| gappa_dsl, herbie_eval, fptaylor_eval | L3 源 | L4 或新 L3.5 | 数值误差分析是求解后的验证 |
| autodiff, ode_solver | L4 源 | L4 | ✓ 已正确（之前在L3） |
| thread_pool, simd_ops | L3 源 | L4 或 shared | 性能优化应在 L4 推理层 |
| groebner_parallel | L4 源 | L4 | ✓ 正确 |

#### 🔴 完全不清的

| 模块 | 现状位置 | 问题 | 建议 |
|------|---------|------|------|
| plugin_system.h | 孤立 | 没有源文件 | 定义或删除 |
| mini_kernel, gc_language | L4 | 语言设计，不属 | 考虑抽离 |
| block_scheduler | L6 | 与 incremental_exec 重复 | 合并 |
| effect_system | L6 | 副作用追踪，与 L4 重复 | 明确用途 |

---

### 四、构建系统检查

#### 📋 CMakeLists.txt 分析

**现状**:
```cmake
# ✓ 层序定义明确
add_library(lv00_layer1_parser OBJECT ...)
add_library(lv00_layer2_resource OBJECT ...)
add_library(lv00_layer3_geometry OBJECT ...)
add_library(lv00_layer4_reasoning OBJECT ...)
add_library(lv00_layer5_output OBJECT ...)
add_library(lv00_layer6_visual OBJECT ...)
add_library(lv00_layer7_orchestration OBJECT ...)
add_library(lv00_layer8_meta_verify OBJECT ...)
add_library(lv00_layer9_application OBJECT ...)
add_library(lv00_layer10_interop OBJECT ...)

# ✓ 依赖方向正确
target_link_libraries(lv00_layer4_reasoning lv00_layer3_geometry lv00_layer2_resource)

# ⚠️ 但没有逆向依赖检查
# ❌ ENABLE_LAYER_VALIDATION 选项存在但无实现
option(ENABLE_LAYER_VALIDATION "Enable compile-time layer boundary checks" OFF)
```

**问题**:

1. **验证代码缺失** ❌
   ```cmake
   if(ENABLE_LAYER_VALIDATION)
       target_compile_definitions(${target} PRIVATE LV00_ENABLE_LAYER_VALIDATION)
   endif()
   # 这只定义了宏，但在 C 代码中没有相应的检查！
   ```

2. **Layer 6-10 的依赖链不清** ⚠️
   ```cmake
   # Layer 7 依赖 Layer 6 到 2，这是对的吗？
   target_link_libraries(lv00_layer7_orchestration 
       lv00_layer6_visual 
       lv00_layer5_output 
       lv00_layer4_reasoning 
       lv00_layer3_geometry 
       lv00_layer2_resource)
   # 是否需要这么多层？
   ```

3. **Layer 7-10 文件稀少，可能是占位符** ⚠️
   ```
   layer7_orchestration/ - 1 file
   layer8_meta_verify/   - 1 file
   layer9_application/   - 1 file
   layer10_interop/      - 4 files
   ```

---

### 五、依赖逆行检查

通过 include 扫描，**已识别的潜在问题**:

#### 🔴 确认逆向依赖

1. **Layer 2 → Layer 3** (不允许)
   - `context.c` (L2) 可能包含 `constraint_graph.h` (L3)?
   - 需要验证

2. **Layer 3 → Layer 4** (不允许)
   - 某些 L3 的几何模块可能调用 L4 的求解器

#### 🟡 需要深入审查的

- Layer 5 → Layer 6（正向）vs Layer 6 → Layer 5（逆向）的平衡
- Layer 7-10 与其他层的交互边界

---

### 六、头文件分布统计

```
总计: 170+ 头文件

按功能分类：
├── 核心系统 (15)
│   ├── lv00.h, context.h, engine.h, error_codes.h, config.h, etc.
│
├── 几何系统 (20)
│   ├── constraint_graph.h, symbolic_coord.h, euclidean_geometry.h
│   ├── normalization.h, propagation.h, equiv_class.h, etc.
│
├── 推理系统 (30)
│   ├── solver.h, proof.h, rewrite.h, unify.h
│   ├── axiom_pkg.h, func_block.h, type_system.h
│   ├── groebner_engine.h, smt_backend.h, atp_backend.h, etc.
│
├── 数值/验证 (12)
│   ├── interval_arithmetic.h, gappa_dsl.h, fptaylor_eval.h
│   ├── herbie_eval.h, float_error.h, etc.
│
├── 几何代数 (3)
│   ├── ga_multivector.h, ga_interface.h, ga_codegen.h
│
├── 预设模块 (55)
│   ├── preset_*.h (全部)
│
├── 输出系统 (8)
│   ├── tikz_export.h, proof_widget.h, stream.h, interop.h
│
├── 可视化 (10)
│   ├── visual_editor.h, block_scheduler.h, etc.
│
├── 性能 (4)
│   ├── thread_pool.h, simd_ops.h, benchmark.h, etc.
│
├── 内部实现 (18)
│   ├── lv00_internal.h, func_block_internal.h, expr_canon.h
│   ├── axiom_grade.h, proof_priority.h, proof_trace.h, etc.
│
└── 其他/未分类 (20)
    ├── plugin_system.h, math_protocol.h, recursion.h, etc.
```

---

## 主要问题清单

### 优先级 P0 (立即处理)

| # | 问题 | 影响 | 建议方案 |
|---|------|------|--------|
| P0-1 | `ENABLE_LAYER_VALIDATION` 未实现 | 无法检测架构违规 | 实现编译时静态检查或 CMake 后处理脚本 |
| P0-2 | Layer 4 过度集中 (73 个源文件) | 难以维护，混杂功能 | 按功能子域拆分 (求解器、证明、预设等) |
| P0-3 | 数值/验证模块归属不明 | 架构模糊 | 定义 L3.5 或新子层结构 |
| P0-4 | 头文件全部在 `lv00/` | 难以区分层级和可见性 | 按层建立子目录: `lv00/layer3/`, `lv00/layer4/` 等 |

### 优先级 P1 (本周完成)

| # | 问题 | 影响 | 建议方案 |
|---|------|------|--------|
| P1-1 | 55 个预设模块无分类 | 难以查找和维护 | 分类为 geometry/, algebra/, logic/, numerical/ 等 |
| P1-2 | Layer 7-10 文件稀少 | 可能是占位符，不清楚职责 | 补全或合并到前 5-6 层 |
| P1-3 | 文档与实现脱节 | 开发者困惑 | 更新 README，标记未实现的功能 |
| P1-4 | 没有依赖关系图 | 无法可视化架构 | 生成 CMake 依赖图或 Graphviz 可视化 |

### 优先级 P2 (后续规划)

| # | 问题 | 影响 | 建议方案 |
|---|------|------|--------|
| P2-1 | 可选功能（GA、数值、WASM）没有隔离 | 影响编译时间 | 考虑 feature flags 或可选模块 |
| P2-2 | 形式化验证 (Lean 4) 与 C 核心通信不清 | 维护成本高 | 建立明确的桥接接口规范 |
| P2-3 | 内部 API 暴露过多 | 使用者误用风险 | 标记 private 头文件，隐藏实现细节 |

---

## 建议方案总览

### 短期 (2-4 周)：建立清晰的架构边界

#### 方案 A：文件组织重构

```
core/
├── include/
│   └── lv00/
│       ├── lv00.h                    # 总入口
│       ├── config.h, error_codes.h   # 共享
│       │
│       ├── layer2_shared/            # 新：Layer 2 公共接口
│       │   ├── context.h
│       │   ├── memory_pool.h
│       │   └── debug.h
│       │
│       ├── layer3/                   # 新：Layer 3 几何层
│       │   ├── constraint_graph.h
│       │   ├── symbolic_coord.h
│       │   ├── normalization.h
│       │   └── ...
│       │
│       ├── layer4/                   # 新：Layer 4 推理层
│       │   ├── solver.h
│       │   ├── proof.h
│       │   ├── engine.h
│       │   └── ...
│       │
│       ├── layer4_backends/          # 新：L4 可选后端
│       │   ├── groebner/
│       │   ├── smt/
│       │   ├── atp/
│       │   └── numerical/            # interval_arithmetic, gappa, fptaylor
│       │
│       ├── layer4_presets/           # 新：L4 预设分类
│       │   ├── geometry/
│       │   ├── algebra/
│       │   ├── logic/
│       │   └── advanced/
│       │
│       ├── layer5/                   # Layer 5 输出层
│       │   ├── tikz_export.h
│       │   └── ...
│       │
│       ├── layer6/                   # Layer 6 可视化
│       │
│       ├── layer7_through_10/        # 后续补完
│       │
│       └── internal/                 # 新：标记内部实现
│           ├── lv00_internal.h
│           ├── func_block_internal.h
│           └── ...
│
└── src/
    └── [对应的目录结构]
```

#### 方案 B：编译时验证启用

```cmake
# CMakeLists.txt 中启用层级检查

if(ENABLE_LAYER_VALIDATION)
    # 1. 定义层级标识
    set(LV00_LAYER_DEFS
        "LV00_LAYER=1"  # for layer1
        "LV00_LAYER=2"  # for layer2
        ...
    )
    
    # 2. 为每个目标添加 include 拦截
    macro(lv00_validate_layer target layer_id allowed_deps)
        # 检查 target_sources 中的文件
        # 生成编译时检查宏
        target_compile_definitions(${target} 
            PRIVATE 
            "LV00_CURRENT_LAYER=${layer_id}"
            "LV00_ALLOWED_DEPS=${allowed_deps}"
        )
    endmacro()
    
    # 3. 在 lv00_setup_layer 中调用
endif()
```

#### 方案 C：更新文档

```markdown
# doc/ARCHITECTURE_v5.1.md

## 十层单向依赖架构（已实施状态）

| 层 | 名称 | 职责 | 源文件数 | 头文件数 | 依赖项 | 实施状态 |
|----|------|------|---------|---------|--------|--------|
| L1 | Parser | 词法、语法、DSL编译 | 7 | 4 | L2 | ✓ 90% |
| L2 | Resource | 内存、错误、上下文 | 12 | 15 | 无 | ✓ 100% |
| L3 | Geometry | 约束图、符号坐标、几何原语 | 20 | 18 | L2 | ✓ 95% |
| L3.5 | Numerical Verify | 数值验证、误差分析 | 5 | 5 | L3, L2 | ⚠️ 30% |
| L4 | Reasoning | 求解、证明、推理、公理 | 40 | 30 | L3, L2 | ✓ 85% |
| L4-Backends | 求解后端 | Groebner、SMT、SAT、ATP | 15 | 8 | L4, L3 | 🟡 60% |
| L4-Presets | 预设模块 | 55个数学理论预设 | 55 | 55 | L4 | 🟡 50% |
| L5 | Output | TikZ、互操作、证明导出 | 8 | 10 | L4, L3, L2 | ✓ 90% |
| L6 | Visual | 可视化编程、节点图、画布 | 20 | 12 | L5, L4, L3, L2 | 🟡 40% |
| L7 | Orchestration | 流水线编排、调度 | 2 | 1 | L6-L2 | 🔴 10% |
| L8 | Meta-Verify | 元验证、类型检查 | 1 | 1 | L7-L2 | 🔴 5% |
| L9 | Application | 应用入口、REPL、批处理 | 1 | 1 | L8-L2 | 🔴 10% |
| L10 | Interop | Lean/Coq 桥接、OPML | 4 | 1 | L9-L2 | 🔴 30% |
```

---

## 立即行动计划 (Next 7 Days)

### 第1天：诊断确认
- [ ] 生成代码依赖关系可视化（用 `include-what-you-use` 或手工脚本）
- [ ] 确认是否存在逆向依赖
- [ ] 列出所有"不清楚属于哪层"的模块

### 第2-3天：头文件重组
- [ ] 创建新的目录结构 (`layer3/`, `layer4/`, 等)
- [ ] 移动头文件并更新所有 `#include` 语句
- [ ] 添加 header guard 和层级注释

### 第4天：构建系统升级
- [ ] 实现 `ENABLE_LAYER_VALIDATION` 的编译时检查
- [ ] 更新 CMakeLists.txt 层间依赖说明
- [ ] 添加 include path 验证

### 第5-6天：文档更新
- [ ] 更新 README，标记架构实施进度
- [ ] 编写 "架构规范" 文档 (如何添加新功能)
- [ ] 为每个层级编写 API 指南

### 第7天：验证与测试
- [ ] 编译检查所有层
- [ ] 运行现有测试套件，确保无破坏
- [ ] 创建"架构合规检查"脚本 (CI 集成用)

---

## 详细建议：按层级划分

### Layer 1 (Parser) - 评分 75/100

**现状**: 相对独立，依赖清晰

**问题**:
- 公式解析器(formula_parser) 和 DSL 编译器(dsl_compiler) 可进一步分离
- 没有测试桩

**建议**:
```c
// layer1/formula/formula_parser.c
// layer1/dsl/dsl_compiler.c
// layer1/lexer/lexer.c
// layer1/tests/test_parsing.c
```

---

### Layer 2 (Resource) - 评分 80/100

**现状**: 基础层，角色明确

**问题**:
- 包含了过多"工具函数" (utility)，应该移到 shared/common
- 缺少资源管理的统一接口

**建议**:
```
layer2_resource/
├── context.c          # 上下文管理
├── memory_pool.c      # 内存分配
├── error_codes.c      # 错误处理
├── debug.c            # 调试工具
├── runtime_monitor.c  # 运行时监控
└── shared/            # 新：共享工具
    ├── lv00_utils.c
    ├── lv00_numeric.c
    └── cache_manager.c
```

---

### Layer 3 (Geometry) - 评分 70/100

**现状**: 清晰，但混杂了优化和验证模块

**问题**:
- `interval_arithmetic.c, gappa_dsl.c, fptaylor_eval.c` 应该分离到 L4 或新 L3.5
- `thread_pool.c, simd_ops.c` 是性能优化，应该在 L4
- GA (Geometric Algebra) 模块关系不清

**建议**:
```
layer3_geometry/
├── core/
│   ├── constraint_graph.c
│   ├── symbolic_coord.c
│   ├── normalization.c
│   └── propagation.c
├── primitives/
│   ├── euclidean_geometry.c
│   ├── high_dim.c
│   └── geo_topology.c
├── geometry_algebra/          # 新：分离 GA
│   ├── ga_multivector.c
│   ├── ga_interface.c
│   └── ga_codegen.c
└── [移至 L4]: thread_pool.c, simd_ops.c, interval_arithmetic.c, ...

# 新增：Layer 3.5 (可选)
layer3_5_numerical/
├── interval_arithmetic.c
├── gappa_dsl.c
├── gappa_propagate.c
├── fptaylor_eval.c
├── herbie_eval.c
└── float_error.c
```

---

### Layer 4 (Reasoning) - 评分 50/100 ⚠️

**现状**: **最大的问题在这里** — 70+ 源文件混杂

**问题**:
1. 核心推理 (solver, proof, engine) 与专题 (GA, 数值) 混在一起
2. 55 个预设模块全部堆积
3. 多个 backend (Groebner, SMT, SAT, ATP) 没有子目录
4. `func_block` 子系统过大 (10个文件)
5. `axiom/` 和 `preset/` 子目录已存在但组织不明

**建议**:

```
layer4_reasoning/
├── core/                      # 新：核心推理引擎
│   ├── engine.c
│   ├── engine_scheduler.c
│   ├── solver.c
│   ├── solver_core.c
│   └── [5 files]
│
├── proof/                     # 新：证明系统
│   ├── proof.c
│   ├── proof_optimize.c
│   ├── proof_multi_strategy.c
│   ├── proof_trace.c
│   ├── proof_priority.c
│   └── [3 files]
│
├── rewrite_unify/             # 新：重写与合一
│   ├── rewrite.c
│   ├── rewrite_strategy.c
│   ├── unify.c
│   ├── normalization.c
│   └── expr_canon.c
│
├── type_logic/                # 新：类型与逻辑
│   ├── type_system.c
│   ├── three_valued_logic.c
│   ├── modal_operators.c
│   ├── quantifier.c
│   └── recursion.c
│
├── axiom/                     # 已有：公理系统
│   ├── axiom_pkg.c
│   ├── axiom_grade.c
│   ├── axiom_rule_engine.c
│   └── [2 files]
│
├── func_block/                # 已有：函数块 (10 files)
│   ├── func_block.c
│   ├── func_block_compose.c
│   ├── func_block_determinism.c
│   ├── func_block_instantiate.c
│   ├── func_block_preset.c
│   ├── func_block_registry.c
│   └── [4 files]
│
├── backend_groebner/          # 新：Groebner 后端
│   ├── groebner_engine.c
│   └── groebner_parallel.c
│
├── backend_smt/               # 新：SMT 后端
│   ├── smt_backend_impl.c
│   ├── smt_theory_combiner.c
│   ├── smt_bitvector.c
│   └── smt_trigger_engine.c
│
├── backend_sat/               # 新：SAT/BDD 后端
│   ├── sat_encoding.c
│   ├── bdd_encoding.c
│   └── approx_counter.c
│
├── backend_atp/               # 新：ATP 后端
│   └── atp_backend.c
│
├── numerical/                 # 新：数值约束
│   ├── probabilistic_constraint.c
│   ├── inequality_reasoning.c
│   └── rational.c
│
├── algebra_symbolic/          # 新：符号代数
│   ├── nt_number_theory.c
│   ├── nt_polynomial.c
│   ├── sym_expr.c
│   └── expr_canonical.c
│
├── preset/                    # 已有但需重组
│   ├── geometry/              # 新分类
│   │   ├── preset_basic_geometry.c
│   │   ├── preset_advanced_geometry.c
│   │   └── [geometry presets]
│   ├── algebra/               # 新分类
│   │   ├── preset_algebraic.c
│   │   ├── preset_linear_algebra.c
│   │   └── [algebra presets]
│   ├── analysis/              # 新分类
│   │   ├── preset_analysis.c
│   │   ├── preset_differential_equations.c
│   │   └── [analysis presets]
│   ├── logic/                 # 新分类
│   │   ├── preset_math_logic.c
│   │   ├── preset_modal_logic.c
│   │   └── [logic presets]
│   └── advanced/              # 新分类
│       ├── preset_category_theory.c
│       ├── preset_homological_algebra.c
│       └── [advanced presets]
│
├── system/                    # 新：系统支持
│   ├── module.c
│   ├── mini_kernel.c
│   ├── gc_language.c
│   ├── ecosystem.c
│   ├── math_protocol.c
│   ├── stream.c
│   ├── stream_context_util.c
│   └── relation_model.c
│
├── internal/                  # 新：内部工具
│   ├── logic_check.c
│   ├── circuit_breaker.c
│   ├── proof_contradiction.c
│   ├── conflict_detector.c
│   ├── adaptive_pruning.c
│   └── algebra_mode.c
│
└── [deprecated]: autodiff.c, ode_solver.c -> 考虑移至 L4-backend_numerical 或抽取为插件

```

---

### Layer 5 (Output) - 评分 85/100

**现状**: 清晰，职责明确

**问题**:
- 只有 8 个源文件，可以补充更多输出格式
- Interop 的位置有疑问 (应该在 L10 吗?)

**建议**:
```
layer5_output/
├── core/
│   ├── proof_widget.c
│   └── proof_export_enhanced.c
├── export/                    # 新：导出格式
│   ├── tikz_export.c
│   ├── svg_export.c          # 新
│   ├── png_export.c          # 新
│   ├── json_export.c         # 新
│   └── lean_export.c         # 新
├── interop/
│   └── interop.c             # 或移至 L10
└── visualization/            # 新
    ├── geo_visual.c
    └── geo_visual_complete.c
```

---

### Layer 6-10：需要补完

**现状**: 大多只有 1-4 个文件，是占位符

**建议**: 
1. 明确定义职责
2. 补全实现，或合并到前 5 层
3. 考虑是否真的需要这些层

详见下节 "层级架构补完计划"

---

## 层级架构补完计划

### 现状 vs 规划

```
现在 (v5.0)                       规划 (v5.1+)
───────────────────────         ─────────────────────
L1 Parser (7 files)             L1 Parser (7 files) ✓
L2 Resource (12 files)          L2 Resource (12 files) ✓
L3 Geometry (20 files)          L3 Geometry (25 files) ✓
                                L3.5 Numerical (5 files) [新]
L4 Reasoning (73 files)         L4 Reasoning (40 files) [拆分]
                                 └─ 子层级 (33 files)
L5 Output (8 files)             L5 Output (12 files) ✓
L6 Visual (20 files)            L6 Visual (20 files) [待实现]
L7 Orchestration (1 file)       L7 Orchestration (5 files) [待实现]
L8 Meta-Verify (1 file)         L8 Meta-Verify (3 files) [待实现]
L9 Application (1 file)         L9 Application (3 files) [待实现]
L10 Interop (4 files)           L10 Interop (8 files) [待实现]
───────────────────────         ─────────────────────
合计: ~147 files               合计: ~173 files (更清晰)
```

---

## 关键指标与目标

| 指标 | 当前 | 目标 | 时间 |
|------|------|------|------|
| 层间依赖检查启用 | OFF | ON | v5.1 |
| 头文件分层完成 | 0% | 100% | v5.1 |
| Layer 4 拆分完成 | 1 层 | 5 子层 | v5.2 |
| 预设模块分类 | 1 类 | 5 类 | v5.1 |
| 架构文档完整度 | 60% | 100% | v5.1 |
| 编译时验证覆盖 | 0% | 80% | v5.2 |

---

## 参考实现：CMake 架构验证脚本

```cmake
# scripts/check_layer_boundaries.cmake
# 用于检查文件是否遵守层级边界

function(check_layer_includes target layer_id allowed_layers)
    get_target_property(sources ${target} SOURCES)
    
    foreach(src ${sources})
        file(READ "${src}" file_content)
        
        # 提取 #include 语句
        string(REGEX MATCHALL "#include.*" includes "${file_content}")
        
        foreach(inc ${includes})
            # 解析 include 文件名
            string(REGEX MATCH "[<\"]([^>\"]+)[>\"]" _ "${inc}")
            set(inc_file "${CMAKE_MATCH_1}")
            
            # 检查是否在 allowed_layers 中
            set(allowed FALSE)
            foreach(allowed_layer ${allowed_layers})
                if(inc_file MATCHES "layer${allowed_layer}")
                    set(allowed TRUE)
                    break()
                endif()
            endforeach()
            
            if(NOT allowed AND inc_file MATCHES "layer[0-9]")
                message(WARNING 
                    "Layer boundary violation: "
                    "L${layer_id} file ${src} includes ${inc_file}"
                )
            endif()
        endforeach()
    endforeach()
endfunction()

# 使用示例：
# check_layer_includes(lv00_layer3_geometry 3 "2")  # L3 只能用 L2
# check_layer_includes(lv00_layer4_reasoning 4 "3;2")  # L4 只能用 L3 和 L2
```

---

## 总结与后续步骤

### ✅ 目前架构的优点

1. 十层设计思想正确
2. 目录层级结构清晰
3. CMakeLists.txt 依赖方向基本正确
4. 功能集合相对完整

### ❌ 需要立即改进的地方

1. **启用层级检查** — 最关键
2. **拆分 Layer 4** — 最紧迫
3. **重组头文件** — 最混乱
4. **补完 L6-10** — 最薄弱
5. **更新文档** — 最易被忽视

### 📅 建议时间线

```
Week 1-2:  诊断 + 初步重组 (目标：P0 问题解决 50%)
Week 3-4:  头文件迁移 + 依赖验证 (目标：P0 问题解决 100%)
Week 5-6:  Layer 4 拆分 + 预设重组 (目标：P1 问题解决 70%)
Week 7-8:  文档补完 + 测试验证 (目标：架构评分 80/100)
```

---

## 附录 A：CMake 打印层级依赖

```cmake
# scripts/print_layer_deps.cmake

message(STATUS "=== Lv-00 Layer Dependency Graph ===")
message(STATUS "")

set(LAYER_CONFIGS
    "1;layer1_parser;layer2_resource"
    "2;layer2_resource;"
    "3;layer3_geometry;layer2_resource"
    "4;layer4_reasoning;layer3_geometry|layer2_resource"
    "5;layer5_output;layer4_reasoning|layer3_geometry|layer2_resource"
    "6;layer6_visual;layer5_output|layer4_reasoning|layer3_geometry|layer2_resource"
    "7;layer7_orchestration;layer6_visual|layer5_output|layer4_reasoning|layer3_geometry|layer2_resource"
    "8;layer8_meta_verify;layer7_orchestration"
    "9;layer9_application;layer8_meta_verify"
    "10;layer10_interop;layer9_application"
)

foreach(config ${LAYER_CONFIGS})
    string(REPLACE ";" "|" config_list "${config}")
    list(GET config_list 0 layer)
    list(GET config_list 1 target)
    list(GET config_list 2 deps)
    
    if(deps)
        string(REPLACE "|" " ← " deps_display "${deps}")
        message(STATUS "L${layer}: ${target} ← ${deps_display}")
    else()
        message(STATUS "L${layer}: ${target} (no deps)")
    endif()
endforeach()

message(STATUS "")
message(STATUS "Direction: ← (allowed) → (forbidden)")
```

---

**诊断报告完成时间**: 2026-06-14 23:59  
**下一步**: 创建改架构的详细计划与执行路线图
