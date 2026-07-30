# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-07-24

### Added
- 11 个新测试文件覆盖 solver/backends/Layer3/Layer5/Layer6，新增 ~9000+ 行测试代码
- λ-演算核心集成：Church 编码、β-归约、Y 组合子、test_lambda_church.c
- 端口作用域系统完整化（namespace_depth/parent_block_id/is_formal_param 三字段）
- 信任颜色 8 色体系完整化（TrustColor/ProofColor/lvTrustColor 映射+传播）
- 约束模板双层测试框架（出厂/用户测试集）
- SHA-256 内容哈希提取为共享模块
- ODE Adams-Bashforth 多步法数值求解器
- 重写策略组合子 API（11 个构造函数+执行+搜索）
- 几何变换预设符号坐标计算
- 交互式几何系统、等价类管理器、WFC 约束传播引擎测试

### Changed
- solver.c 从 8818 行拆分为 18 个子模块（~200 行聚合入口）
- 36 个文件资源释放命名统一（_free → _destroy）
- 11 个文件中 140+ malloc/realloc/free 替换为 lv_* 分配器
- 4 个核心头文件依赖精简，移除 12 个 #include
- 全仓库 'lv00' 前缀消解为 'lv'

### Fixed
- 30+ 预存 bug（头文件不匹配、内存泄漏、变量名错误、flaky 测试等）
- SHA-256 实现重复（从 axiom_pkg.c 和 proof_version.c 提取）
- 3 个孤儿文件删除（approx_counter.c、engine_scheduler.c、geo_visual.c）

---

## [Unreleased]

### 数学严谨性与数值稳定性修复（3 轮，30+ 文件）

#### P0 — 除零/NaN 关键修复
- **除零保护** — `lv_reflect_point` (A,B 重合)、`lv_transform_reflection_line` (a,b 同时为零)、`symbolic_coord_ops.c` 连分数求值 (`mpq_inv` 零值)、`algebraic_number.c` (`alg_rational_mul`/`alg_rational_simplify` gcd=0)
- **NaN/Inf 传播** — `rational.c` mpq_get_d→mpq_set_d 往返 (3 处)、`refine_algebraic_bounds` 无限二分循环、`solver_linear.c`/`solver_symbolic.c` mpz_get_d 后 `isfinite` 检查
- **浮点精确比较→容差比较** — `geo_event_detect.c` 二分法/牛顿法 `fmid==0.0` 改为 `fabs(fmid) < lv_EPSILON_NUMERIC_COMPARE`

#### P1 — 整数溢出/参数验证
- **容量翻倍溢出** — 10 个文件添加 `INT_MAX/2` 前置检查（`proof_widget.c`、`magic.c` x4、`benchmark.c`、`lean4_bridge.c`、`proof_compiler.c`、`lv_impl_native.c` x2、`engine_scheduler.c`、`smt_backend_impl.c`、`func_block_instantiate.c`）
- **分配前溢出** — `smt_backend_impl.c` max_node_id+1、`engine_scheduler.c` point_count*2、`lv_impl_upper.c` degree+1
- **参数验证** — `magic.c` spell_set_input/output_count 负值检查、`fast_index.c` cell_hash capacity<=0 保护

#### P2 — 内存与健壮性
- **GMP 内存泄漏** — `symbolic_coord_ops.c` 嵌套二次根式回退路径中 `disc_num_sq`/`disc_product` 未释放
- **字符串缓冲区** — `proof_widget.c` JSON 序列化 snprintf 截断检测与自动扩容
- **类型系统递归深度** — `type_system.c` type_normalize 添加 4096 深度上限

### 新增 (Added)
- **λ-演算核心集成** — λ-项数据结构 (`LvLambdaTerm`)、β-归约实现、λ-项 ↔ 约束图编译、Church 编码 + Y 组合子测试
- **端口作用域系统完整化** — `GeomNode` 三字段 (`namespace_depth`/`parent_block_id`/`is_formal_param`) 生命周期管理
- **信任颜色系统完整化** — 扩展 `TrustColor` 枚举（8 色），着色传播逻辑，数值假设逃逸出口
- **基础设施补齐** — 约束模板双层测试框架、模块加载器 SHA-256 + DFS 循环检测、跨边界约束检查
- **UI 系统 L1–L6 分层架构** — React 19 + TypeScript 6 + Vite 8 + Zustand，与 C 内核完全解耦
- **内核/UI 通信协议** — `KernelBridge` 接口 + `DrawCmd`/`UserAction` 类型定义
- **Mock Bridge** — `createMockBridge()` 完整模拟内核，前端可独立开发测试
- **UI 新组件** — CanvasToolbar、Checkbox、CommandPalette、ExpressionList、Slider
- **内核配置系统** — `lv_config.h` + `lv_config.c`，独立配置管理
- **新增头文件** — `preset_abstract_algebra.h`、`preset_name_defs.h`、`proof_rule_engine_internal.h`、`proof_session_internal.h`、`proof_version_internal.h`、`smt_theory_combiner.h`、`smt_trigger_engine.h`、`lv_config.h`、`lambda_term.h`
- **CMake 打包配置** — `cmake/lv-config.cmake.in` + `cmake/lv.pc.in`（find_package / pkg-config 支持）

### 变更 (Changed)
- **资源释放命名统一** — 36 个文件的 `_free` 命名统一为 `_destroy`，消除命名歧义
- **内存分配器统一** — 11 个文件中 30+ `malloc`、30+ `realloc`、80+ `free` 替换为 `lv_*` 内存分配器
- **头文件依赖精简** — 4 个核心头文件（`engine.h` 从 10 个依赖减至 5 个 + 2 个前向声明），累计移除 12 个 `#include`
- **全仓库命名统一** — 所有 `lv00` 前缀消解为 `lv`（代码、文件格式、注释）
- **UI 内核完全解耦** — UI 仅通过 `protocol/index.ts` 与内核通信，不再直接依赖内核头文件
- **头文件全面修复** — 11+ 个头文件（`geo_halfedge_mesh.h`、`simd_ops.h`、`interval_arithmetic.h`、`geometry_transform.h` 等）重新编写以匹配 .c 实现
- **源码恢复** — 从 git 历史恢复 63+ 源文件（版本 A `38310ea` 05-23，版本 B `e36f4b6` 06-04）
- **删除过时 stub 头文件** — 清理 `core/include/lv/stubs/` 下的废弃预设桩文件
- **MSVC 构建兼容性** — CMakeLists.txt 添加 C11 atomics 启用、`_CRT_SECURE_NO_WARNINGS`、GMP vcpkg 支持

### 修复 (Fixed)
- `geo_halfedge_mesh.h`：lvPoint3D normal 字段、lv_HE_ITER_VERTEX_OUT_HALFEDGES 宏、HeMeshStats 完整字段
- `simd_ops.h`：lvSimdCapability/Stats/Vec4d/Vec4f/Vec8f 完整类型
- `interval_arithmetic.h`：3 参数 `interval_create()`、`is_exact` 字段
- `geometry_transform.h`：GMP `mpq_t` 字段的 lvTransform/lvTransformSequence/lvTransformGroup
- `geo_event_detect.h`：M_PI 兼容（`_USE_MATH_DEFINES` + fallback）
- `algebraic_number.h`：从 git 恢复完整 670 行文件

### 项目指标（截至 2026-07-24）
| 指标 | v1.1.0 初版 | 当前 |
|:---|---:|---:|
| .c | 232 | 401 |
| .h | ~170 | 229 |
| .lean | 81 | 84 |
| .lv | 138 | 154 |
| .py | 83 | 94 |
| .tsx | 0 | 41 |
| .ts | — | 1011 |
| 测试 | — | 138 (137 通过, 1 预存失败) |

---

## [1.1.0] - 2026-06-21 — CompCert-Lite 编译器形式化验证

### 新增 (Added) — 编译器形式化验证 (6轮)
- **R1: lvLang + IR** — .lv 源语言操作语义 + 中间表示 (193行, 8定理)
- **R2: Compiler + CompilerCorrectness** — 第一次真正的编译正确性证明, 替代 `by rfl` 假证明 (295行, 12定理)
- **R3: Cv00Lang + Cv00Memory** — C11 子集操作语义 + GMP 精确内存模型 (435行, 12定理)
- **R4: Codegen + CodegenCorrectness** — IR→C 结构安全证明 (544行, 14定理)
- **R5: UndefinedBehavior + Evidence** — 7种UB分类 + 零信任证据系统 (670行, 23定理)
- **R6: InteropCorrectness** — Coq/Lean4/OPML/GeoJSON/SVG 互操作正确性 (287行, 17定理)

### 新增 (Added) — 公理化基础
- **Hilbert 公理框架** (10文件): Basic/Incidence/Betweenness/Congruence/Parallel/Continuity/Order/HilbertAxioms/EuclideanPlane/lvMeta
- **定义模块** (6文件): GeometricAlgebraDefs/GeometryPresetDefs/ODESolverDefs/NumericDefs/PresetGeometryDefs/BootstrapDefs
- **覆盖率模块** (8文件): ConstraintPropagation/InteropSoundness/OrchestrationSoundness/AxiomDiscoveryTheory/FormulaSemantics/VisualLayerSoundness/MetaVerificationTheory/StreamingTheory
- **108→138 .lv 语义规格** — 重建全部 bootstrap 规格
- **GMP 原语运行时** — primitive_runtime.c/h 全部 mpq_t 精确有理数 (零 double/float)

### 修复 (Fixed)
- **compiler_semantics_preservation := by rfl** → 真正的 induction/cases/simp 证明
- **GMP 统一**: 全部数学计算使用 mpq_t 精确有理数
- **分支统一**: master/main 合并为单一 GMP 精确计算分支
- **版本统一**: 全部文件 1.1.0 (lv.h VERSION_MAJOR=1, CMake project VERSION 1.1.0)
- **目录整理**: modules→module, docs→doc, 删除空/废弃目录
- **.gitignore**: web/legacy → web/wasm

### 项目指标
| 指标 | v1.0 | v1.1.0 |
|:---|---:|---:|
| .lv | 167 | 138 |
| .lean | 70 | 81 |
| .py | 54 | 83 |
| .c | 44 | 232 |
| .lvz | 0 | 57 |
| 定理数 | ~208 | ~300 |

---

## [1.0.0-rc1] - 2026-06-21 — 技术债全面清零

### 新增 (Added)
- **lv_impl_native.c**: 统一实现替代 17 个 C 桩
- **lv_impl_upper.c**: L3-L10 全部 C API 实现
- **166 个 .lv 语义规格**: 10 层 + preset + spec 目录
- **57 个 Lean4 形式化定理文件**
- **test_runner.py + 5 份新测试**
- **7 份新文档**: 快速开始/架构/配置/测试/构建/API/形式化

### 修复 (Fixed)
- P0: 17 个 C 桩 → lv_impl_native.c
- P1: 11 个胖 Python → .lv
- P2: Lean4 覆盖率 17%→72%
- P3: L3-L10 全部 35 项 C 空壳 → lv_impl_upper.c

### 已知限制 (Research Preview)
- C 编译未经环境验证
- Python 绑定需要编译后的 C 共享库
- Lean4 `lake build` 未运行 (需 mathlib4)
- GitHub Actions CI/CD 预期为红灯
