# Lv-00 任务上下文 — v1.9.0

**版本**: v1.9.0 | **日期**: 2026-07-23 | **阶段**: 微自举 A — lv 解析自身 .lv 文件

---

## 一、已完成

| 任务 | 状态 |
|:---|:--:|
| v1.0→v1.1 编译器形式化验证 (R1-R6) | ✅ |
| GMP 精确计算统一 (mpq_t, 零 double/float) | ✅ |
| formal/ 零 sorry (81 .lean, 编译器 pipeline) | ✅ |
| Hilbert 公理框架 (10 文件, 含 EuclideanPlane) | ✅ |
| Phase 14-15: 全部桩函数 → 完整实现 | ✅ |
| v1.2.1 代码质量审计: 0 warning / 0 error | ✅ |
| v1.3.0 桩函数全部消灭: 44 桩 → 真实实现 | ✅ |
| v1.3.1 测试全绿 + 死循环修复 + 代码安全加固 | ✅ |
| v1.4.0 全系统代码优化到最优 | ✅ |
| v1.5.0 输入验证加固 + 魔法数字消除 + 内存分配统一 | ✅ |
| v1.6.0 架构重构：消除代码重复 + 共享基础设施 | ✅ |
| v1.7.0 资源释放命名统一 (_free → _destroy) | ✅ |
| v1.8.0 内存分配统一 + 头文件依赖精简 | ✅ |
| v1.9.0 微自举 A：lv 解析自身 .lv 文件 | ✅ |

## 二、v1.9.0 微自举 A — lv 解析自身 .lv 文件

### 新增文件（14 个）

| 文件 | 说明 |
|:---|:---|
| `core/include/lv/lv_lexer.h` | 词法分析器 API，75 种 Token 类型 |
| `core/include/lv/lv_ast.h` | AST 节点定义，28 种节点类型，12 种实体类型 |
| `core/include/lv/lv_parser.h` | 递归下降解析器 API |
| `core/include/lv/lv_sema.h` | 语义分析 API（符号表 + 类型检查） |
| `core/include/lv/lv_loader.h` | .lv 文件加载与引擎集成 API |
| `core/src/layer1_parser/lv_lexer.c` | 词法分析器实现 |
| `core/src/layer1_parser/lv_ast.c` | AST 节点内存管理 |
| `core/src/layer1_parser/lv_parser.c` | 递归下降解析器实现 |
| `core/src/layer1_parser/lv_sema.c` | 语义分析实现 |
| `core/src/layer1_parser/lv_loader.c` | 文件加载与引擎集成 |
| `test/c/test_lv_lexer.c` | 81 个词法测试 |
| `test/c/test_lv_parser.c` | 33 个解析器测试 |
| `test/c/test_lv_bootstrap.c` | 18 个端到端自举测试 |

### 编译与测试

| 指标 | 值 |
|:---|:---|
| 构建 | 全部 target 0 error / 0 warning |
| 测试 | 126/126 passed（含新增 132 测试点） |
| 覆盖率 | 词法 → 语法 → 语义 → 引擎集成全链路 |

## 三、自举路线图（完整 6 步）

根据 [docs/README.md 自举路线](doc/docs/README.md)，完整路线如下：

| 步骤 | 版本 | 目标 | 状态 |
|:---:|:---:|:---|:--:|
| 1 | ✅ | C 实现内核 | ✅ |
| 2 | — | 公理系统编辑器 | ⏳ |
| 3 | **v1.9.0** | **微自举 A：lv 解析自身 .lv 文件** | ✅ |
| 4 | v2.0.0 | λ-演算内核集成 | ⏳ |
| 5 | v2.1.0 | 微自举 B：lv 验证自身证明 | ⏳ |
| 6 | — | 首次自举：命题逻辑验证器 | ⏳ |

### 各步详述

**步骤 2 — 公理系统编辑器**: 实现图形化/交互式公理系统编辑器，使用户能可视化地定义和修改几何公理、预设规则。这是 C 内核到语言自举之间的过渡工具。

**步骤 3 — 微自举 A** (v1.9.0 ✅): lv 系统能读取、解析、类型检查并加载 bootstrap 目录下的 .lv 规约文件。已完成完整的词法/语法/语义/引擎集成管线。

**步骤 4 — λ-演算内核集成** (v2.0.0): 在推理层集成 λ-演算内核，使几何构造可以用 λ-项表达，构造即证明的基础设施。

**步骤 5 — 微自举 B** (v2.1.0): lv 系统能验证自身的证明。基于微自举 A 的解析能力和 λ-演算内核，实现证明验证自举。

**步骤 6 — 首次自举**: 用 lv 自身编写并验证一个命题逻辑验证器，实现完整的编译器自举闭环。

## 四、v1.8.0 内存分配统一 + 头文件依赖精简

### 原始 malloc/realloc/free → lv_* 统一
| 文件 | 转换内容 |
|:---|:---|
| `layer3_geometry/geo_aabb_tree.c` | 4 `realloc` + 3 `malloc` + 多 `free` → `lv_*` |
| `layer3_geometry/geo_halfedge_mesh.c` | 10 `realloc` + 7 `malloc` + 多 `free` → `lv_*` |
| `layer3_geometry/geo_dynamic.c` | 6 `realloc` + 5 `malloc` + 多 `free` → `lv_*` |
| `layer3_geometry/geo_constraint_solver.c` | 2 `realloc` + 3 `malloc` + 多 `free` → `lv_*` |
| `layer3_geometry/geo_topology.c` | 2 `realloc` + 多 `free` → `lv_*` |
| `layer3_geometry/gappa_dsl.c` | 3 `realloc` + 3 `free` → `lv_*` |
| `layer3_geometry/gappa_propagate.c` | 1 `realloc` + 1 `free` → `lv_*` |
| `layer5_output/proof_export_enhanced.c` | 4 `malloc` + 4 `free` → `lv_*` |
| `layer5_output/proof_widget.c` | 1 `malloc` + 1 `free` → `lv_*` |
| `layer4_reasoning/backends/groebner_parallel.c` | 2 `free` → `lv_*` |
| `layer10_interop/lean4_bridge.c` | 1 `free` → `lv_*` |

**总计**: 11 个文件，30+ malloc, 30+ realloc, 80+ free 全部统一到 lv 内存分配器

### 头文件依赖精简
| 文件 | 优化前 | 优化后 | 精简 |
|:---|:--:|:--:|:--:|
| `engine.h` | 10 include | 5 include + 2 前向声明 | -5 |
| `proof.h` | 9 include | 4 include + 2 前向声明 | -5 |
| `constraint_graph.h` | 3 include | 2 include + 1 前向声明 | -1 |
| `rewrite.h` | 3 include | 2 include + 1 前向声明 | -1 |

**总计**: 4 个核心头文件，减少 12 个 #include，编译依赖链大幅缩短

### 编译与测试（v1.8.0 基线）

| 指标 | 值 |
|:---|:---|
| 构建 | 137/137 targets, 0 error, 0 warning |
| 测试 | 116/118 passed（2 个预存 flaky 测试） |
| raw malloc/realloc/free | 0（layer3_geometry + layer4/layer5/layer10 全部统一） |
| 头文件精简 | 4 个核心头文件，减少 12 个 #include |

## 五、v1.7.0 资源释放命名统一

### _free → _destroy 重命名（18 个函数）
| # | 旧名称 | 新名称 | 涉及文件 |
|:--|:---|:---|:--:|
| 1 | `ga_mv_free` | `ga_mv_destroy` | ga_multivector.h/.c, ga_interface.h/.c, test |
| 2 | `lv_he_mesh_free` | `lv_he_mesh_destroy` | geo_halfedge_mesh.h/.c, test |
| 3 | `lv_aabb2d_free` | `lv_aabb2d_destroy` | geo_aabb_tree.h/.c, test |
| 4 | `lv_aabb3d_free` | `lv_aabb3d_destroy` | geo_aabb_tree.h/.c, test |
| 5 | `lv_dyn_graph_free` | `lv_dyn_graph_destroy` | geo_dynamic.h/.c, test |
| 6 | `lv_geo_spec_free` | `lv_geo_spec_destroy` | geo_spec.h/.c (×2) |
| 7 | `lv_solver_free` | `lv_solver_destroy` | geo_constraint_solver.h/.c, test |
| 8 | `lv_dof_analysis_free` | `lv_dof_analysis_destroy` | geo_constraint_solver.h/.c, test |
| 9 | `dsl_tokens_free` | `dsl_tokens_destroy` | dsl_compiler.h/.c |
| 10 | `dsl_ast_free` | `dsl_ast_destroy` | dsl_compiler.h/.c |
| 11 | `dsl_ir_free` | `dsl_ir_destroy` | dsl_compiler.h/.c |
| 12 | `error_bound_free` | `error_bound_destroy` | float_error.h/.c, fptaylor_eval.c |
| 13 | `lv_perf_record_free` | `lv_perf_record_destroy` | benchmark.h/.c |
| 14 | `approx_count_result_free` | `approx_count_result_destroy` | approx_counter.h/.c (×2) |
| 15 | `atp_result_free` | `atp_result_destroy` | atp_backend.h/.c, lv_impl_upper.c, proof_multi_strategy.c |
| 16 | `preset_validation_result_free` | `preset_validation_result_destroy` | func_block_preset_ops.h/.c |
| 17 | `preset_bindings_free` | `preset_bindings_destroy` | func_block_preset_ops.h/.c |
| 18 | `preset_search_result_free` | `preset_search_result_destroy` | func_block_preset_ops.h/.c |

保留的 _free 函数（语义不同，无需重命名）：`gappa_predicates_free`, `gappa_goals_free`, `gappa_result_free`, `lv_aabb_query_result_free`, `mem_pool_free`
