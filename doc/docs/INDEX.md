# Lv-00 文档索引

> 最后更新: 2026-08-10
> 架构版本: v1.1.0 (十层架构)

## 文档分类

### 架构设计
| 文档 | 说明 |
|------|------|
| [archived/ARCHITECTURE_MANUAL.md](archived/ARCHITECTURE_MANUAL.md) | 架构手册 — 十层单向依赖架构详解（已归档） |
| [TEN_LAYER_INTEGRATION_PLAN.md](TEN_LAYER_INTEGRATION_PLAN.md) | 十层架构集成计划 |
| [archived/DIRECTORY_MIGRATION_PLAN.md](archived/DIRECTORY_MIGRATION_PLAN.md) | 目录迁移计划（已归档） |
| [archived/FIVE_LAYER_ACADEMIC_REFACTOR_PLAN.md](archived/FIVE_LAYER_ACADEMIC_REFACTOR_PLAN.md) | 五层学术化整改计划（历史参考） |
| [37_parsing_layer.md](37_parsing_layer.md) | Layer 1 解析层设计 |
| [16_geometry_layer.md](16_geometry_layer.md) | Layer 3 几何层设计 |
| [17_reasoning_layer.md](17_reasoning_layer.md) | Layer 4 推理层设计 |
| [18_output_layer.md](18_output_layer.md) | Layer 5 输出层设计 |
| [35_layer6_visual_programming.md](35_layer6_visual_programming.md) | Layer 6 图形化编程层设计 |

### 规范与标准
| 文档 | 说明 |
|------|------|
| [OPML_SPECIFICATION.md](OPML_SPECIFICATION.md) | OPML 开放证明交换格式规范 |
| [lv_LANGUAGE_SPEC.md](lv_LANGUAGE_SPEC.md) | Lv-00 语言语法与语义定义 |
| [THREE_LAYER_ARITHMETIC_SPEC.md](THREE_LAYER_ARITHMETIC_SPEC.md) | 三层数值算术规范 |

### 入门与 API
| 文档 | 说明 |
|------|------|
| [API_QUICKSTART.md](API_QUICKSTART.md) | API 快速入门 |
| [API_REFERENCE.md](API_REFERENCE.md) | API 完整参考 |
| [TUTORIAL.md](TUTORIAL.md) | 逐步入门教程 |
| [USE_CASES.md](USE_CASES.md) | 使用场景与案例 |

### 系统设计
| 文档 | 说明 |
|------|------|
| [01_symbolic_coord.md](01_symbolic_coord.md) | 符号坐标系统 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图核心 |
| [03_normalization.md](03_normalization.md) | 图规范化遍引擎 |
| [04_solver.md](04_solver.md) | 符号代数求解器 |
| [05_rewrite.md](05_rewrite.md) | 图重写引擎 |
| [06_unify.md](06_unify.md) | 合一检查 |
| [07_func_block.md](07_func_block.md) | 函数块系统设计 |
| [08_type_system.md](08_type_system.md) | 类型系统 |
| [09_proof.md](09_proof.md) | 命题与证明系统 |
| [10_recursion.md](10_recursion.md) | 递归与条件 |
| [12_context_and_lifecycle.md](12_context_and_lifecycle.md) | 上下文与生命周期 |
| [13_error_handling.md](13_error_handling.md) | 错误处理 |
| [14_solver_backends.md](14_solver_backends.md) | 求解后端设计 |
| [15_geometry_advanced.md](15_geometry_advanced.md) | 高级几何模块 |
| [21_euclidean_geometry.md](21_euclidean_geometry.md) | 欧氏几何公理包 |
| [23_core_infrastructure.md](23_core_infrastructure.md) | 核心基础设施 |
| [24_constraint_propagation.md](24_constraint_propagation.md) | 约束传播 |
| [25_engine_scheduler.md](25_engine_scheduler.md) | 引擎调度器 |
| [31_stream_interop.md](31_stream_interop.md) | 流式互操作 |
| [36_memory_management.md](36_memory_management.md) | 内存管理 |
| [39_numerical_analysis.md](39_numerical_analysis.md) | 数值分析 |
| [40_formula_dsl_ga.md](40_formula_dsl_ga.md) | 公式 DSL 与几何代数 |

### 推理与证明
| 文档 | 说明 |
|------|------|
| [11_wfc_paradigm.md](11_wfc_paradigm.md) | WFC 范式 |
| [27_quantifier_logic.md](27_quantifier_logic.md) | 量词逻辑 |
| [28_number_theory.md](28_number_theory.md) | 数论模块 |
| [29_inequality_approximation.md](29_inequality_approximation.md) | 不等式近似 |
| [33_gappa_verification.md](33_gappa_verification.md) | Gappa 验证 |
| [34_meta_proof_cache.md](34_meta_proof_cache.md) | 元证明缓存 |
| [22_proof_export_trace_widget.md](22_proof_export_trace_widget.md) | 证明导出与追踪组件 |
| [38_logic_verification.md](38_logic_verification.md) | 逻辑验证 |
| [41_axiom_rewrite_export.md](41_axiom_rewrite_export.md) | 公理包重写与导出 |
| [42_proof_engine_enhanced.md](42_proof_engine_enhanced.md) | 增强证明引擎 |
| [INFERENCE_STRATEGIES_SPEC.md](INFERENCE_STRATEGIES_SPEC.md) | 推理策略规范 |

### 可视化与交互
| 文档 | 说明 |
|------|------|
| [26_interactive_geometry.md](26_interactive_geometry.md) | 交互式几何 |
| [geometric_primitives.md](geometric_primitives.md) | 几何图元 |
| [32_runtime_monitoring.md](32_runtime_monitoring.md) | 运行时监控 |

### 工程与质量
| 文档 | 说明 |
|------|------|
| [PERFORMANCE_OPTIMIZATION.md](PERFORMANCE_OPTIMIZATION.md) | 性能优化指南 |
| [CONSTRAINT_PROOF_TEST_PLAN.md](CONSTRAINT_PROOF_TEST_PLAN.md) | 约束证明测试计划 |
| [bootstrap_test_framework.md](bootstrap_test_framework.md) | 自举差分测试框架 |
| [30_performance_concurrency.md](30_performance_concurrency.md) | 性能与并发 |
| [DOCUMENT_GOVERNANCE_PLAN.md](DOCUMENT_GOVERNANCE_PLAN.md) | 文档治理计划 |
| [competitive_analysis.md](competitive_analysis.md) | 竞品分析 |
| [GAP_ANALYSIS.md](GAP_ANALYSIS.md) | 差距分析 |
| [stable_release_gap_analysis.md](stable_release_gap_analysis.md) | 稳定版差距分析 |

### 设计参考
| 文档 | 说明 |
|------|------|
| [phase1_meta_representation.md](phase1_meta_representation.md) | 自举 Phase 1 元表示层 |
| [MAGIC_MODULE.md](MAGIC_MODULE.md) | Magic 模块设计 |

### 变更记录
| 文档 | 说明 |
|------|------|
| [../../CHANGELOG.md](../../CHANGELOG.md) | 版本变更记录 |
| [../../VERSION_LOG.md](../../VERSION_LOG.md) | 版本迭代日志 |
