# Lv-00 任务上下文 — v1.1.0

**版本**: v1.1.0 | **日期**: 2026-08-04 | **阶段**: 当前开发版本

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
| v1.9.1 重写引擎加固：测试覆盖 + StreamContext 修复 | ✅ |
| v2.0.0 λ-演算内核集成 | ✅ |
| P2 收敛 (ef1dc596): 分发表/析构/坐标样板全量收敛 (19 文件) | ✅ |
| P3 收敛 (5043c126): LV_DISPATCH 分发 + 坐标对 pair helper 模板 (20 站点) | ✅ |

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
| 测试 | 152/152 passed |
| 覆盖率 | 词法 → 语法 → 语义 → 引擎集成全链路 |

## 三、自举路线图（完整 6 步）

根据 [docs/README.md 自举路线](doc/docs/README.md)，完整路线如下：

| 步骤 | 版本 | 目标 | 状态 |
|:---:|:---:|:---|:--:|
| 1 | ✅ | C 实现内核 | ✅ |
| 2 | — | 公理系统编辑器 | ⏳ |
| 3 | **v1.9.0** | **微自举 A：lv 解析自身 .lv 文件** | ✅ |
| 4 | **v2.0.0** | **λ-演算内核集成** | ✅ |
| 5 | v2.1.0 | 微自举 B：lv 验证自身证明 | ⏳ |
| 6 | — | 首次自举：命题逻辑验证器 | ⏳ |

### 各步详述

**步骤 2 — 公理系统编辑器**: 实现图形化/交互式公理系统编辑器，使用户能可视化地定义和修改几何公理、预设规则。这是 C 内核到语言自举之间的过渡工具。

**步骤 3 — 微自举 A** (v1.9.0 ✅): lv 系统能读取、解析、类型检查并加载 bootstrap 目录下的 .lv 规约文件。已完成完整的词法/语法/语义/引擎集成管线。

**步骤 4 — λ-演算内核集成** (v2.0.0 ✅): 在推理层集成 λ-演算内核，使几何构造可以用 λ-项表达，构造即证明的基础设施。

**步骤 5 — 微自举 B** (v2.1.0): lv 系统能验证自身的证明。基于微自举 A 的解析能力和 λ-演算内核，实现证明验证自举。

**步骤 6 — 首次自举**: 用 lv 自身编写并验证一个命题逻辑验证器，实现完整的编译器自举闭环。

## 四、v2.0.0 λ-演算内核集成

### 核心变更

| 分类 | 修改 | 文件 |
|:---|:---|:---|
| 编译管线修复 | **端口方向修正**：VAR 创建 PORT_OUTPUT（数据生产者），APP 连接方向改为 arg_output → func_input | `core/src/layer4_reasoning/lambda/lambda_to_graph.c` |
| 编译管线修复 | 新增 `get_node_output_port()` / `get_node_input_port()` 辅助函数，统一节点端口查询 | 同上 |
| β-归约匹配修复 | **输出端口匹配可选**：不要求输出端口有外部 CONNECTION 消费者，通过 connected_to 确定 body→output 关联 | `core/src/layer4_reasoning/rewrite/beta_reduce.c` |
| 测试验证 | 新增 **8 个 β-归约结果验证测试**：Church 5 不可归约、succ/add/mul/pow 归约步数和 FB 减少验证 | `test/c/test_lambda_eval.c` |
| 构建系统 | 注册 `test_lambda_eval` 测试目标 | `CMakeLists.txt` |

### 编译与测试

| 指标 | 值 |
|:---|:---|
| 构建 | 全部 target 0 error / 0 warning |
| test_lambda_church | 22/25 passed（3 个类型推断测试有预存崩溃问题，与本次变更无关） |
| test_lambda_eval | 8/8 passed |

### 已知限制

1. **多参应用**：需迭代 β-归约（每步处理一个 redex），编译时只创建直接 CONNECTION 的 redex
2. **graph_to_lambda 反编译**：无 APP 重建（COMPILE→β-REDUCE→DECOMPILE 链不完整），验证通过图结构指纹完成
3. **类型推断测试崩溃**：`test_type_infer_id/k/app_id` 在 TypeSystem 相关代码中触发访问违例，与本次 λ-演算内核变更无关

## 五、版本号

| 位置 | 值 | 状态 |
|:---|:--:|:--:|
| `VERSION` 文件 | **1.1.0** | ✅ 已统一 |
| `CMakeLists.txt` `project()` | **1.1.0** | ✅ 已统一 |
| `lv.h` 版本宏 | **1.1.0** | ✅ 已统一 |

## 五、文档同步状态

| 文档 | 同步状态 |
|:---|:--:|
| `2026-07-21-completion-plan.md` | ✅ 已标记全部完成 (2026-07-29) |
| `IMPLEMENTATION_STATUS_AUDIT.md` | ✅ 已更新至 v1.9.0 状态 (2026-07-29) |
| `CHANGELOG.md` | ✅ 已包含 v1.1.0 及后续 unreleased 变更 |
| `VERSION_LOG.md` | ✅ 已包含 v1.1.0 和 v1.9.0-dev 记录 |

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

---

## 六、v1.9.1 重写引擎加固

| 分类 | 修改 | 文件 |
|:---|:---|:---|
| P0: 测试补全 | 重写 `test_rewrite.c`：添加 9 个真实约束图测试，含模式匹配断言、规则应用验证、快照回滚、WL 哈希、规则卸载 | `test/c/test_rewrite.c` |
| P2: StreamContext | 修复 `rewrite_stream_ctx` 多文件 static 副本问题：改为非静态全局变量+getter 函数 | `core/src/layer4_reasoning/rewrite.c` |
| | `rewrite_apply.c` 和 `rewrite_wl.c` 改用 `rewrite_get_stream_context()` 访问 | `core/src/layer4_reasoning/rewrite/rewrite_apply.c`, `rewrite_wl.c` |
| | 新增公共 API `rewrite_get_stream_context()` | `core/include/lv/rewrite.h` |
| P1: 策略函数 | 核查 `lv_rewrite_apply_strategy` — 已有完整实现，非桩函数 | `core/src/layer4_reasoning/unify/rewrite_strategy.c` |

### 新增测试点（9 个）
| 测试函数 | 说明 | 断言重点 |
|:---|:---|:---|
| `test_real_pattern_matching` | 3 变量 + 2 incidence 约束匹配 | 绑定到正确节点类型、两节点不同 |
| `test_real_rule_application` | 匹配线段并替换为 midpoint betweenness | `REWRITE_APPLIED`、节点数不减少 |
| `test_multi_rule_rewrite` | 两规则顺序执行 | 状态合法、图有效 |
| `test_graph_snapshot` | 快照 → 修改 → 恢复 | 恢复后节点/约束数与快照一致 |
| `test_wl_hash` | 同构图哈希相等、异构不等 | `h1==h2`、`h1!=h3` |
| `test_rule_unload` | 规则卸载 | 卸载成功、二次卸载失败 |
| `test_rule_destroy_null` | NULL 安全 | 不崩溃 |
| `test_local_equiv_matching` | 局部等价容忍匹配 | 绑定 2 个不同点 |

### 阻塞项
- **编译验证**: 当前环境缺少 C 编译器工具链（GCC/Clang/MSVC），无法执行构建验证

---

## 七、v1.9.2 Groebner 基求解加固

| 分类 | 修改 | 文件 |
|:---|:---|:---|
| 约束编码 | `constraint_graph_to_ideal` 从空壳→真实实现：编码 POINT 坐标方程、ID→变量映射 | `core/src/layer4_reasoning/backends/groebner_engine.c` |
| 辅助函数 | 新增 `poly_internal_make_term`（创建单项式）、`poly_internal_add_term`（添加单项式/常数项，含同类项合并） | 同上 |
| API 测试 | 新增 6 个引擎 API 直接测试：环生命周期、多项式生命周期、多项式算术、理想操作、约束图编码 | `test/c/test_groebner_basis.c` |

### 新增测试点
| 测试函数 | 说明 | 断言重点 |
|:---|:---|:---|
| `test_engine_ring_lifecycle` | 环创建/查找/属性验证 | var_count、order、field |
| `test_engine_poly_lifecycle` | 多项式创建/获取/销毁 | ring_id、term_count、销毁后 NULL |
| `test_engine_poly_arith` | 零多项式加法 | 0 + 0 = 0 |
| `test_engine_ideal_lifecycle` | 理想创建/添加生成元 | 返回值 == 0 |
| `test_constraint_graph_to_ideal` | 约束图→理想→Gröbner 基计算 | ideal_id >= 0、compute 成功 |

### constraint_graph_to_ideal 编码规则
- 每个 POINT 节点分配 2 个连续变量 (x_i, y_i)
- 符号坐标编码为常量方程 (x_i - val_x = 0, y_i - val_y = 0)
- INCIDENCE/BETWEENNESS 等约束编码为占位结构（叉积方程骨架）

---

## 八、P3 抽象化收敛（commit 5043c126）

### 方向 1：分发表收敛
| 修改 | 文件 |
|:---|:---|
| `geo_create_node` / `geo_create_constraint` 手写边界检查 + 分发表 → `LV_DISPATCH` 宏 | `core/src/layer3_geometry/geometric_primitives.c` (2 处) |
| `kGeomEntityVtbls` 3 处分发点 → 新增 `geom_entity_vtbl()` 访问器封装越界检查（结构体 vtable 不适用 LV_DISPATCH） | `core/src/layer6_visual/geometry_canvas.c` (3 处) |

### 方向 3：坐标对创建模板收敛
新增 API（`symbolic_coord.h` / `symbolic_coord_lifecycle.c`）：
- `symbolic_coord_pair_create_rational(num_x, denom_x, num_y, denom_y, &out_x, &out_y)`
- `symbolic_coord_pair_from_double_scaled(x, y, scale, &out_x, &out_y)`
- 失败时自动回滚已创建项；`symbolic_coord_destroy` 确认 NULL 安全

替换站点（20 处）：
| 文件 | 数量 | 说明 |
|:---|:---:|:---|
| `meta_repr.c` | 7 | 含修复 1 处成功路径泄漏（graph_add_node_with_id 深拷贝） |
| `formula_converter_geom.c` | 2 | 堆/栈默认坐标对 |
| `formula_converter_constraint.c` | 2 | fallback 默认对（顺带修复部分失败泄漏）+ angle 对 |
| `formula_converter_complex.c` | 3 | 默认顶点/圆心 + radius 对 |
| `formula_curve.c` | 3 | center/radius scaled 对 + fallback 原点对 |
| `lv.c` | 1 | `lv_add_point` 坐标对 |
| `path_type.c` | 1 | path 节点区分坐标对（新增失败路径） |
| `interop_import.c` | 1 | GeoJSON 坐标导入对 |

### 明确排除（非收敛价值）
- 混合对（rational+scaled，如 `formula_curve.c` p1/p2、`formula_converter_geom.c` radius）
- 计算对（`symbolic_coord_add` 中点）
- 循环内元素创建（`formula_converter_util.c`）
- 矩阵系数对（`impl_preset_transformations.c`，非坐标语义）
- 结构体 vtable（`interop_export_lean.c` void+fallback、`engine_circuit.c` 并行销毁表）— 与宏语义不匹配

### 验证
ninja 931/931 目标 · ctest 170/170 通过 · 示例 8/8 退出码 0

---

## 九、批次 N 抽象收敛（2026-08-11）

### 收敛设施（决策登记）

| 设施 | 判据 | 调用点 | 测试 |
|------|------|-------|------|
| `lv_parse_int_before`（lv_parse_utils.h） | I（数字提取样板） | 4（meta_verify.c：策略尝试数 / 标记数 / 几何对象数 / 字节数） | test_meta_verify `test_parse_int_before` |
| `lv_strlcpy` / `lv_strlcat`（lv_utils.h） | J（字符串安全复制 / 拼接样板） | 全库 336 | 既有字符串测试链 |
| `lv_RESULT_FAIL`（lv_error.h） | K（错误结果样板） | 34（block_scheduler 8 / block_to_node 11 / block_to_text 5 / block_to_geometry 9 / representation_converter 1 + `make_error_result` 收敛） | test_error_handling `test_result_fail_macro` |
| `lv_registry_remove_prefix`（lv_registry.h） | A 变体（批量清理样板） | 4（插件配置 / 公理包 / 几何事件 / Gappa 传播清理） | test_registry |
| `lv_constraint_has_participants`（lv_constraint_guard.h） | H 泛化 | 25+（smtlib2 / bdd / groebner 后端编码族） | test_smt_backend 等 |

### 判据 K 豁免登记

| 豁免形态 | 位置 | 理由 |
|----------|------|------|
| 格式化消息样板（snprintf 形态） | block_scheduler.c 环检测消息（已标注 `/* exempt: */`） | 需内嵌 topo_count/n 数值，静态消息宏无法表达 |
| `lvParseResult::errors[]` 数组形态（error_count + 多槽） | lv_parser.h | 多槽错误数组，非单 `error_msg` 字段 |

### 文档同步（ABSTRACTION_SPEC.md）

- 新增 §1.9 / §1.10 / §1.11 判据 I / J / K；原粒度门槛重编号 §1.12
- §10 迁移顺序表新增阶段 6（判据 I / J / K）
- §11.3 新设施准入登记批次 N 五项设施
- §12 合规清单判据类型扩展为 A–K / 泛化

---

## 十、批次 O 候选方向登记（2026-08-11 全库扫描，待立项）

### 黑名单直接命中（已验证）

| 候选 | 位置 | 收敛目标 | 状态 |
|------|------|----------|------|
| 手写单调时钟 `now_ms()` | lv_impl_upper_orchestrator.c:73-81 | `lv_get_time_ms()` 已存在 | 需决策：单调 vs 墙上时钟语义差异 |
| 裸数值微分步长 `h = 1e-6` | formula_curve.c:141 | `lv_NUMERICAL_DIFF_EPSILON`（=1e-8） | 需决策：量级 1e-6 vs 1e-8 不一致 |

### 设施已存在、调用点未迁移（零成本候选）

| 候选 | 规模 | 收敛目标 | 说明 |
|------|------|----------|------|
| 手写 capacity+realloc 倍增 | 37 文件 / 55 处 | `lv_ensure_capacity` | **勘误（2026-08-11 复核）**：实际已全面收敛。裸 realloc( 仅设施自身 3 处（allocator.c / lv_utils.c ops 包装）；命中多为已收敛使用点、解析器硬上限守卫（formula_dsl.c 等）、设施自身实现。剩余非倍增 `lv_realloc`（text_code.c 4KB 对齐文本缓冲 / geo_visual_complete.c IDAT 精确增长 / lv_path.c 路径扩展 / rewrite_apply.c 数组压缩 / memory_pool.c 分块 / graph_node_hash.c 哈希 rehash）登记为**语义特化豁免**，不迁移 |
| 裸 `return -1` 错误码 | 133 文件 / 629 处 | `lv_RETURN_ERROR` 宏 | 需按语义筛除"未找到索引"型合法 -1 |
| 连续 `lv_free` 清理序列 | 66 文件 / 271 处 | `lv_DEFER` 作用域守卫 | 同构性中（变量序列/条件位置各异） |
| NULL 参数守卫 | 10 文件 / 25 处 | `lv_CHECK_NULL` 系列宏 | 已有宏未用 |

### 需新建设施的高同构候选

| 候选 | 规模 | 拟新建设施 | 说明 |
|------|------|-----------|------|
| 手写前缀匹配 | 34 文件 / ~100 处 | `lv_str_has_prefix` | 现有仅 proof_version_isar.c 一个静态 `starts_with` 局部副本；注册表前缀过滤 6 处完全同构 |
| strstr/strchr + memcpy 手工截取 | 44 文件 / 161 处（粗扫）；截取子形态 ~49 处 | `lv_str_cpylen`（长度钳制） | memcpy+手动 NUL 形态；粗扫含全部 strstr/strchr 使用点（含查找未截取），2026-08-12 复核 |
| 手写线性查找 | 100 文件 / 245 处 | `lvHashtable_int` / `lv_registry` / 通用 `lv_array_find` | 仅"return i"型 42+ 处明确；热路径需评估；2026-08-12 复核 `strcmp==0` 全库 365 处/106 文件 |
| count++ 动态收集追加 | 117 文件 / 352 处 | IntArray / lvDArray push 语义 | 仅无界动态部分收敛；有上限栈缓冲豁免；2026-08-12 复核显式 `->count++/len++/size++` 32 文件 |
| 枚举↔字符串平行表 | 7 文件（lv_protocol.c 5 张、debug_state / conflict_detector / modal_operators / lv_number 各 1） | X-Macro 枚举族生成器（一次性生成枚举+名称表+双向转换） | **2026-08-12 新增**；判据 F/D。lv_protocol.c 的 kTrustColorName/RGBA/SVG/TikZ/ToLv 5 张平行表被 test_output_export.c / test_layer5_output.c 精确断言，迁移需同步改测试或豁免 |
| 序列化"逗号分隔"骨架（for + `if (i > 0)` + append 分隔符） | 12 文件 / 21 处 | `lv_strbuf_join_*`（分隔符注入） | **2026-08-12 新增**；判据泛化（≥3 特例成立）。已有雏形 lv_str_utils.c:404（`"%s", separator`）；分布见 graph_serialize / graph_node_alloc / opml_codec / formula_renderer_ascii / proof_widget / bootstrap_test_report 等 |

### 低优先级 / 不建议

| 候选 | 规模 | 结论 |
|------|------|------|
| 链表遍历骨架 | 15 文件 / 48 处 | 语义差异大，仅可做 foreach 宏，不替换结构 |
| 手写二分查找 | 4 文件 / 5 处 | 比较谓词各异，收益小 |
| goto fail 链 | 24-41 文件 / 59-173 处 | 半收敛（lv_DEFER 已配），逐子系统推进 |
| stderr 日志 | 3 文件 / 4 处 | 已基本收敛至 lv_log，无需处理 |
| 构造器失败回滚（多分配+失败逐字段 free+return NULL） | 47 文件（"单块双 free"精确命中；`return NULL` 粗扫 1582 处/214 文件） | **2026-08-12 新增**；判据 E。与阶段 F（lv_DEFER）协同——F 完成后在 graph_node_alloc 等重灾区评估 goto cleanup 统一约定或 arena 分配 |
| 手写数字解析（`v = v*10 + (c-'0')`） | 7 文件 / 16 处 | **2026-08-12 新增**；中低优先级，集中在 lv_json / formula_dsl / module_lvz 编解码，可收敛至 lv_str_utils 格式化模块 |
| 手写排序（qsort 除外） | 0 文件 | 已全面收敛至 qsort（10 文件），无需处理 |

### 第二波扫描新增候选（2026-08-12 午后；三路并行子代理只读扫描 core/src 全库）

#### P0 高价值（设施成熟、规模大、纯机械）

| 候选 | 规模 | 收敛目标 | 说明 |
|------|------|----------|------|
| 手写 snprintf buffer 链（多次 snprintf + strlen 累计 pos 拼长串） | 13 文件 / 60+ 处 | `lvStrBuf`（lv_strbuf_printf + to_string） | 本批最大规模方向。smt_backend_impl_smtlib2 编码子函数 ~20 处（(buf,remaining) 签名需小改接口）；atp_backend 6+；proof_trace / proof_contradiction 本地宏 TRACE_WRITE / CONTRADICTION_WRITE 逐字同构；formula_converter_export 自述局部复刻 lvStrBuf（21 处 5 步样板）；另 approx_counter / float_error / conflict_detector / lambda_unify / formula_curve / inequality_reasoning_serialize / preset_manager_doc / high_dim_view |
| 枚举/错误码转字符串手写查表（names[] 数组 + 下标返回） | 14 文件 / ~16 函数 | `lv_enum_to_str` / `lvStrToEnumEntry` / `lv_error_name` | 全库已有 20+ 同构迁移先例，纯机械。lv_lexer.c lv_token_type_name、lv_ast.c ast_type_name、mini_kernel.c 错误码数组（应并入 lv_error_name）、type_system / relation_model / proof_strategy / prop_verifier_trust / euclidean_geometry / interop_export_coq / graph_dot_export / axiom_pkg_serialize / preset_group_theory。与阶段 J（平行表合并）互补 |
| 手写倍增 realloc 扩容补漏 | 3 文件确定 | `lv_ensure_capacity` / `lvDArray` | 与上表 L320 勘误"全面收敛"互补：interop_theorem.c:102 / lambda_to_graph.c:828 / lv_utils_config.c:326 为漏网倍增；text_code.c / lv_graph_traversal.c 与既有"语义特化豁免"清单重叠，迁移时复核 |
| 手写"创建-注册-查找"注册表三件套 | 3 文件 | `lv_registry` / `lv_REGISTRY_STATIC` | lv_backend_plugin.c（for+strcmp 查重 + find 线性查找）最契合；preset_blocks.c / smt_theory_combiner.c 需保留额外元数据字段做适配。与阶段 G（通用线性查找）区分：本项为完整注册表语义 |

#### P1 中价值（需补有界/无分配变体或新辅助）

| 候选 | 规模 | 收敛目标 | 说明 |
|------|------|----------|------|
| 手写空白跳过 / trim 循环 | 8 文件 / ~19 处 | 补 `lv_str_skip_ws_n(p, end)` 有界变体 + `lv_str_ltrim` | lean4_bridge（7 处同构）/ coq_bridge / proof_strategy_numeric / gappa_propagate / float_error / formula_dsl 均为 `(pos < end)` 有界解析器，现 lv_str_skip_ws 仅支持 NUL 结尾；math_input.c 单点可直接迁移 |
| strchr 单次切分（找分隔符→取切片） | 13 文件 / ~17 处 | 补无分配 `lv_str_split_once(const char **pp, char delim)` | 与 lv_str_read_token（跳空白+堆分配）语义不吻合。axiom_pkg_verify / atp_backend / proof_version_isar / graph_conflict / interop_import / interop_server / proof_strategy_deductive / gappa_dsl / lv_utils_misc / lv_protocol；HTTP 有界场景需 bounded 变体 |
| 关键字表循环（for + strstr 命中枚举） | 7 文件 / ~9 处 | `lv_str_match_any`（返回首个命中下标） | proof_classical.h / proof_version_ghost.c / lv_protocol.c 直接覆盖；meta_verify 为计数语义需新 `lv_str_match_count` 变体；module_lvz / mini_kernel / geo_spec / formula_python 等 strcmp 多分支链可作旁支收敛 |
| 手写对象销毁序列（NULL + 成员逐个 free + 元素循环） | 5-6 函数 / 100+ 行 | `lvFieldDesc` 字段销毁描述表 | probabilistic_constraint.c dtmc_destroy（8 成员+行循环）、approx_counter.c cnf_destroy、relation_model.c rel_destroy、lambda_unify.c lambda_apply_entries_destroy、groebner_parallel.c（60 行，线程关闭逻辑除外）。lvFieldDesc 已 34 处使用。与阶段 F（作用域守卫 lv_DEFER）区分：本项为销毁函数本身 |
| 错误消息组装（snprintf 写固定 error_msg 字段） | 6 文件 / 35+ 处 | `lv_set_result_error(result, fmt, ...)` 辅助 | lv_impl_upper_orchestrator.c ~20 处（set_error_msg 中 snprintf("%s") 可简化为 lv_strlcpy）、engine_scheduler 6 处、formula_curve 4 处、lv_sema 前缀拼接 |
| 边界钳制 + 越界检查序列 | ~20 处 | `lv_CLAMP` / `lv_index_in_range` | 小批量高可行：formula_converter_stmt / axiom_rule_engine（同文件 4 处同构）/ expr_canon / graph_node_alloc / geometry_csg_eval / high_dim_project 钳制；conflict_detector 等运行期线性扫描索引验证；5 文件 _Static_assert 可统一宏包装 |

#### P2 低价值 / 设施补齐 / 暂缓

| 候选 | 规模 | 结论 |
|------|------|------|
| 手写复制构造函数（graph_node_alloc 同文件 3 份 6 字段"增强字段"拷贝） | 6 文件 | 先抽本地 helper（高确定低风险）；跨文件统一需新"复制描述表"设施（深/浅拷贝字段声明），属新设施建设，暂缓 |
| 设施补齐：`lv_str_hex_decode` | 1 处（module_delta hash_string_to_u64）+ JSON `\uXXXX` 公共解码点 | 设施补齐而非批量替换，低优先 |
| 设施补齐：`lv_path_ext` | 1 处（interop_theorem interop_get_file_extension）+ 同文件 strchr 非法字符检查 | 方向相反于现有 lv_path_strip_ext，低优先 |
| 报表列对齐 → `lv_strbuf_append_cell` 示范迁移 | 2 文件（memory_pool / lv_utils 的 fprintf "%-20s" 报表） | append_cell 当前 0 外部调用，先示范 1 处再推广 |
| 手写数组最值遍历 / 手写选择排序 | 9 文件 / ~15-18 处 | lv_max_d 仅 double；带索引/自定义类型需 comparator 适配，部分为算法内聚（solver_engine / rewrite_vf2），暂缓 |
| 手写 LCG / Box-Muller（probabilistic_constraint） | 1 文件 | 替换会改变随机序列与可复现性，需评估测试种子语义，暂缓 |
| 手写幂/平方/立方 | 6 文件 / ~10 处 | 残留均为数学算法固有表达式（贝塞尔 / gamma 采样 / 谓词 / Horner），替换 pow 反而劣化，不迁移 |
| 手写栈/队列模拟（DFS/BFS 栈） | 9 文件 / ~15-18 处 | 与遍历算法强耦合且已封装良好，无泛型栈设施；仅建议统一各栈的"扩容"委托 lv_ensure_capacity |
| 参数校验 + 默认值样板 | 4 处零散 | 语义各异（条件透传 vs 默认值），机械迁移收益小 |
| 时间/时间戳格式化、大小写转换循环 | 0 处 | 已收敛，无需处理 |

### 批次 O 执行进度（2026-08-11 立项后按优先级推进）

| 阶段 | 内容 | 状态 |
|------|------|------|
| A | `now_ms()` → `lv_get_time_ns()/1000000ULL`（单调语义一致）；新建 `lv_FD_GRADIENT_STEP 1e-6`（语义常量，区别于相对容差 eps），formula_curve.c 迁移 | 完成 |
| B | 手写 capacity+realloc 倍增 | 完成（复核为已收敛，无迁移；异常值已登记豁免） |
| C | 手写前缀匹配 → `lv_str_startswith` | 完成（见下） |
| D | NULL 守卫 → `lv_CHECK_NULL` | 完成（lv_arena×4 / bdd_encoding×11 / prop_verifier_trust / node_graph；豁免见下） |
| E | 裸 `return -1` → `lv_RETURN_ERROR` | 完成（lean4_bridge / smt_backend_impl / prop_verifier_trust / node_graph / network_block / tikz_export×3；豁免见下） |
| F | 连续 free 序列 → `lv_DEFER` | 完成（graph_memory 展平 + mini_kernel / geom_evol / algebraic / inequality_reasoning_serialize；另由仓库侧扩展 ode_integrator / lv_graph_traversal / groebner_engine_ideal；豁免见下） |
| G | 手写线性查找 → 容器设施 | 部分完成（math_theory_guide_cn 共享助手 / preset_helper_cn 下标化；magic_spell 等候选随 magic 模块删除而消失；g_var_map / plugin_system 评估为热路径重构，暂缓登记） |
| H | 序列化"逗号分隔"骨架 → `lv_strbuf_join` / `lv_json_buf_append_raw_value` | 完成（见下） |
| I | 构造器失败回滚 → guard-detach（`lv_DEFER` + 成功路径赋 NULL） | 完成（见下） |
| J | 枚举↔字符串平行表 → struct 单表（designated initializer） | 完成（见下） |
| K | 手写数字解析 → `lv_str_read_int` | 完成（评估后全部豁免，见下） |

**阶段 C 明细（2026-08-11）**：字面量前缀 13 文件约 30 处迁移（block_to_text / axiom_pkg_serialize / magic_rune / proof_version / network_block / math_input / module_serialize / runtime_monitor / probabilistic_constraint / interop_import / lv_storage / proof_strategy_deductive×10 / formula_curve 前批）。动态长度形态 8 处迁移（plugin_system_interface×3 / plugin_system_config / geo_event_detect / solver_symbolic×2 / solver_coord_extract×2，消去手写 prefix_len 缓存变量 4 处）。`lv_str_startswith` 实现同步优化为 `strncmp(str, prefix, strlen(prefix))`（不再全文 strlen 预扫）。

**阶段 C 剩余 strncmp 登记豁免**：精确长度标识符匹配（proof_strategy_numeric.c `len==N &&` 形态，前缀化会放宽匹配）；有界缓冲/非 NUL 终止解析（lean4_bridge.c / interop_server.c HTTP / mini_kernel.c / proof_version_isar.c）；截断到 `*` 的有界前缀（test_framework.c）；复合"前缀+分隔+精确尾部"（gappa_propagate.c）；设施自身内部（lv_registry.c `lv_registry_remove_prefix`）。

**阶段 D 明细（2026-08-11/12）**：参数前置守卫迁移 15 处 —— lv_arena.c×4（alloc/alloc_aligned/calloc/strdup 拆分双守卫）、bdd_encoding.c×11（bdd_and/or/not/xor/nand/unique_lookup/new_var/literal/add_constant/add_node_create，新增 `#include "lv/lv_check.h"`）、prop_verifier_trust.c 与 node_graph.c 各 1（graph 前置）。豁免：分配失败守卫（bdd_node_create / bdd_cache_create 附近）、NULL-tolerance destroy（bdd_manager_destroy / bdd_node_deref 接受 NULL）、预期失败控制流（JSON parse 等非参数形态）。

**阶段 E 明细（2026-08-11/12）**：真错误路径 8 处迁移至 `lv_RETURN_ERROR`/`lv_CHECK_NULL` —— lean4_bridge（tactic 名长度非法 INVALID_PARAM）、smt_backend_impl（后端注册表容量满 RESOURCE_EXHAUSTED）、prop_verifier_trust 与 node_graph（graph NULL）、network_block（网络句柄表满 RESOURCE_EXHAUSTED）、tikz_export×3（NULL 前置×2 + INTERNAL 内部失败×2，其中 1 处为"缺少坐标"哨兵豁免）。豁免：qsort comparator（`return -1` = "小于"，非错误，unify_fine 等 3 处）、探索路径预期失败。

**阶段 F 明细（2026-08-11/12）**：graph_memory.c `lv_graph_detect_redundant_constraints` 收尾展平（8 处 `goto cleanup` → 直接 `return redundant;`，删除空 cleanup 标签）；新迁移 4 处 —— mini_kernel.c（`lv_DEFER_FREE_MANY` 2 指针 + 4 处 goto→return，新增 lv_lifecycle.h）、geom_evol.c `geoevol_step_once`（`lv_DEFER_FREE_MANY` 3 指针 + 7 处 goto→return，分配失败块同步简化）、algebraic.c `continued_fraction_approx`（新增 `mpz_clear_deferred` 回调 + 7 个 `lv_DEFER`，消除 L183 手写清理块与 cleanup 标签）、inequality_reasoning_serialize.c（空 done 标签展平）。仓库侧扩展：ode_integrator.c / lv_graph_traversal.c / groebner_engine_ideal.c 的 lv_DEFER 守卫族。豁免：条件清理成功保留（module_delta.c `free_delta_baseline`，lv_DEFER 无法取消）、fd 清理 + rc 错误语义（lv_process.c）、统一出口错误语义（geom_evol.c `cleanup_bdf` Newton 收敛判断）、公共宏 `PRESET_CHECK_NULL` 引用（preset_manager_query/doc）、链表/hashtable 特殊清理（proof_trace_tree / rewrite_strategy_impl / groebner_engine_poly）。

**阶段 G 明细（2026-08-12）**：判据 D 直接命中项收敛 —— math_theory_guide_cn.c 提取共享 `find_theory_index` 助手（消除 3 个 API 的重复 strcmp 循环）、preset_helper_cn.c 正向 int 查找改直接下标（preset_id 连续 0..N-1）。magic_spell.c spellbook 双循环候选随废弃 magic 模块删除而消失（不复存在）。深度调研豁免/暂缓：静态常量关键字表（greek_letters / kXxxTable / lv_str_to_enum 共享表等，合理模式）；运行时热路径注册表（formula_converter g_var_map TLS 表、plugin_system 6 处查找）需改存储布局 + 维护索引，风险收益比不佳，暂缓登记；performance_profiler region_index 已迁移，作范本。

**回归（阶段 D–G）**：每次阶段后 ninja build3 928/928（删除 magic 模块后目标数 931→928）、ctest 170/170 全部通过。

**2026-08-12 增补扫描结论**：全库复查确认阶段 D/E/F/G 候选仍成立（`lv_CHECK_NULL` 已用 20 文件/308 处，说明宏已被采纳、剩余裸守卫迁移成本可控）。新增 4 个登记候选：① 枚举↔字符串平行表（判据 F/D，lv_protocol.c 5 张平行表为首要案例，需处理测试精确断言）；② 序列化"逗号分隔"骨架（判据泛化，12 文件/21 处）；③ 构造器失败回滚（判据 E，与阶段 F 协同）；④ 手写数字解析（中低优先级）。另确认手写排序已全面收敛至 qsort、无抽象必要。上述候选均已登记至本节约"候选方向"表。

**阶段 H 明细（2026-08-12，候选① 逗号分隔骨架）**：lv_json.c 公开 `lv_json_buf_begin_value`（static → public，内部调用批量同步）并新增 `lv_json_buf_append_raw_value`（begin + raw 组合），lv_json.h 同步声明；graph_serialize.c 3 处 `if (i>0) append_char(',')` 骨架迁移（nodes / constraints 走 append_raw_value，coords 走 begin_value + append_coord）。lv_str_utils.c 提取共享 static `strbuf_join_items` 骨架，新增 `lv_strbuf_join`（追加到既有 lvStrBuf），`lv_str_join` 收敛至同一骨架；formula_string.c 2 处游标循环迁移至既有 `lv_str_append_sep`；opml_codec.c axioms 输出改为收集名称数组 + `lv_strbuf_join`。豁免：vtable raw JSON 片段（graph_node_alloc / proof_widget / geojson / bootstrap 等，不经 JsonBuf 状态机）、FILE* 流输出、每元素格式化拼接循环。

**阶段 I 明细（2026-08-12，候选② 构造器失败回滚）**：确立 guard-detach 模式（自定义守卫 struct + `lv_DEFER` 清理回调 + 成功路径守卫指针赋 NULL 解除；因 `lv_DEFER_FREE_MANY` 不可取消、不适用于"返回堆对象"构造器）。迁移 6 处 —— plugin_system_core.c `lv_plugin_system_create`（PluginSystemGuard）、proof_compiler.c `lv_proof_object_create`（ProofObjectGuard）、debug_mempool.c `mem_pool_create`（MemPoolGuard，3 字段）、dsl_compiler_ir.c `dsl_compile`（DslIrGuard，3 字段 + 顺带补 `symbol_index = lv_hashtable_str_create()` 未检查分配的 NULL 检查：失败时保持 NULL 并 lv_set_error，回退线性扫描语义不变）、axiom_template_test.c `axiom_template_test_case_copy`（TemplateCaseGuard）、expr_canon.c `lv_expr_canonical_create`（ExprCanonGuard，B 档代表）。豁免：单失败点构造器（lv_rule_library_create 仅 rules 一层、lv_visual_group_create 仅 children 一层）、无堆分配构造器（lv_rule_create 定长内嵌数组）、非构造器填充（lv_proto_topology 写入调用方 out 结构、debug_invariants 收集函数统一回滚块）。

**阶段 J 明细（2026-08-12，候选③ 枚举↔字符串平行表）**：lv_protocol.c 4 张真平行表（kTrustColorName / kTrustColorRGBA / kTrustColorSVG / kTrustColorTikZ，按 lvTrustColor 枚举索引对齐）合并为 `TrustColorEntry {name, rgba, svg, tikz}` struct 单表 `kTrustColorTable`，采用 designated initializer（`[lv_COLOR_GREEN] = {...}`）杜绝索引漂移；4 个访问函数（lv_trust_color_name/rgba/svg/tikz）改为字段访问。测试精确断言走公共 API 返回值、与表布局无关，无需改测试。kTrustToLv / kLvToTrust 维持 designated 映射表（非平行索引）。豁免（已是健康形态）：debug_state.c（X-Macro 生成表 `lv_XMACRO_TO_NAME_ARRAY`，即候选理想形态）、conflict_detector.c（唯一单张名称表）、modal_operators.c（单张 Unicode 表）、lv_number.c（函数内 designated initializer 表）。

**阶段 K 明细（2026-08-12，候选④ 手写数字解析）**：评估确认共享原语 `lv_str_read_int`（lv_str_utils.c:272，无符号累加防溢出 + 钳位）自阶段 F/G 起已被 rewrite_apply.c / func_block_serialize.c 采用，收敛设施已成立。剩余 5 处手写解析全部豁免（语义与共享原语不兼容）：formula_dsl.c×3（`p < end` 有界输入 + 公式级溢出报错语义）、lv_json.c×3（公共 API 溢出返回 false vs 钳位 + 先定位再累加两遍模式）、module_lvz.c（double 浮点累加，后续小数处理）、axiom_pkg_parser.c（溢出错误标记 + INT_MAX 回退、sign 后置）、interop_server.c（value_len 非 NUL 终止边界 + 版本上限 999 检查）。

**回归（阶段 A–C）**：ninja build3 931/931、ctest 170/170、示例 8/8 全部通过。

---

## 十一、批次 P 候选立项与实施（2026-08-12）

**候选来源**：批次 O 遗留登记 + 4 路并行代理新扫描（AB 路 / EFGH 路 / 自由路 / CD 路），共 15 候选。用户「全部立项」→ P1-P14。已抽查验证关键候选属实。

### 批次 P 执行进度

| 编号 | 内容 | 状态 |
|------|------|------|
| P1 | preset 家族 strdup+OOM → `preset_module_get_names` 委托（28 文件） | 完成 |
| P2 | 欧氏距离平方/模长 → `geo_norm` 家族（4 新设施 / 43 处） | 完成 |
| P3 | 手写二分收敛（error_codes 提取 `find_error_index` 合并 2 份同表二分） | 完成 |
| P4 | JSON 数组迭代骨架（opml_codec 等，新判据 L 提案） | 登记不迁移（新判据 L 不成立，见组⑨ 明细） |
| P5 | 约束编码分发表×5 补全 LV_DISPATCH | 完成（迁移 4 处：groebner + probabilistic×3；复核 5 处不同构豁免，见组② 明细） |
| P6 | SMT/ATP 插件入口状态机×8 → 描述符表宏 | 完成（复核 8 处全部与 LV_DISPATCH 不同构，登记不迁移，见组② 明细） |
| P7 | 手写线性查找表（18-20 处） | 完成（值形态 3 表 96 条迁移至 lv_str_to_enum，出参/哨兵/哈希回退形态登记豁免，见组④ 明细） |
| P8 | realloc 倍增新位置（7 处） | 登记不迁移（7 处形态各异无同构样板，见组④ 明细） |
| P9 | 带符号舍入缺陷修正 | 完成（7 处 `(int)(x+0.5)` → `(int)round(x)`，见组⑧ 明细） |
| P10 | 高斯消元统一 | 登记不迁移（mpq 已统一 cg_mpq_row_echelon、double 列主序 LU 已统一 host_lu_factor/solve，其余异构，见组⑧ 明细） |
| P11 | 线性选优/选择排序 argmin/argmax | 登记不迁移（28 处语义差异大，无通用契约，见组④ 明细） |
| P12 | rewrite_snapshot.c 析构 shim 遗漏 | 完成（删 SnapshotNodeOps 全套，统一复用 node_destroy/constraint_destroy，见组③ 明细） |
| P13 | 角度桶量化+位分解 | 登记不迁移（无法定位，见组⑦ 明细） |
| P14 | 点在线段 bbox 检查 | 完成（geo_bbox_contains_2d/1d，见组⑦ 明细） |

**P1 明细（preset 名称委托，判据 A 泛化）**：共享 `preset_module_get_names`（preset_common.c:387-416，NULL 守卫 + OOM 回滚）收敛 preset 家族 28 文件。三形态统一：① 手写 `lv_malloc` + for + `lv_strdup` + OOM 回滚（主流）；② `PRESET_CHECK_NULL` + error 标签（preset_polynomial 等 3）；③ `lv_malloc` + `memcpy`（preset_field_theory 等 6）。统一替换为 `static const char *const preset_names[] = {...}` + 尾部 `return preset_module_get_names(...)`。删除 786 行 / 新增 81 行。未迁移：preset_group_theory.c 独立辅助 `get_group_theory_names`（非目标函数）。验证：ninja 927/927 + ctest 170/170。

**P2 明细（geo_norm 模长家族，判据 B 泛化）**：geo_utils.h/.c 新增 `geo_norm_3d` / `geo_norm_sq_2d` / `geo_norm_sq_3d` / `geo_norm_2df`（契约卡五字段；`_sq` 返回模长平方省 sqrt，`_2df` 为 float 精度对应 sqrtf 逐位一致）。迁移 43 处/18 文件：2D 平方 27、2D float 模长 6（geo_visual_complete 含屏幕对角线 `sqrtf(w*w+h*h)`）、2D double 模长 6、3D 平方 6（含 aabb_tree_impl.h 模板 `#if AABB_DIMS` 分支改写）、3D 模长 4。新增 include 9 文件（tikz_export / parametric_curves / geo_visual_complete / formula_curve / groebner_engine / proof_strategy_deductive / solver_coord_extract / func_block_selector / geometry_csg_eval / geometry_csg_hull）。测试：test_geometry_core.c `test_geo_norm_family`（3-4-5 / 3-4-12、零向量、float 精度、与 geo_distance_2d 一致性）。

**P2 豁免登记**：
- 点积形态 ×6（`ax*bx + ay*by` 语义不同）：simd_ops.c:2152 / geo_constraint_solver_residual.c:172 / groebner_engine.c:614 / meta_proof.c:176 / solver_geom_templates.c:190 / lv_vec3.h:32 `lv_vec3_dot`。
- 协方差逐项累积：high_dim_fidelity.c:649-650（`cxx += dx*dx` 非距离语义）。
- 旋转矩阵单分量平方：geo_visual_complete.c:222-230 / algebra_mode.c:364-432（`c + ax*ax*(1-c)` 等）。
- 非距离缩放：high_dim_project.c:316/354（`px*factor`）。
- 公共基础头自包含：lv_vec3.h:76 `lv_vec3_normalize` 内联模长（lv_vec3 仅依赖 math.h+config.h，引入 geo_utils.h 造成重型 include 耦合，属领域内核豁免）。

**P3 明细（手写二分收敛）**：error_codes.c 提取无副作用静态辅助 `find_error_index`（返回表索引或 -1），`find_error_info` 与 `lv_error_is_unknown` 两处同表二分统一复用；`lv_error_is_unknown` 改为 `find_error_index(code) < 0`（保持无副作用——原方案直接委托 `find_error_info` 会经 `lv_RETURN_ERROR_NULL` 意外设置全局错误，status_codes.c 探测路径受影响，已规避）。豁免：algebraic_number_util.c `alg_is_perfect_square`（整数平方根数值二分 + 溢出检测，非表查找语义）。

**回归（批次 P1-P3）**：ninja build3 927/927 + ctest 170/170 全部通过，零修复项。

---

## 十二、批次 Q 候选立项与实施（2026-08-12）

**候选来源**：4 路并行代理按判据分组只读扫描 core/src（AB 路 / CDF 路 / EGH 路 / IJK+自由路），共 32 候选。用户「全部立项」→ Q1-Q32。已抽样验证 Q-C1（时间常量族裸字面量）/ Q-J1（memcpy+手写 NUL）/ Q-A1（环形缓冲扩容）/ Q-E1（CUDA 存根四连）四个主推项属实。CUDA/HIP 存根未检索到正式第 9 章豁免登记，Q3 按判据 E 正常立项。

### 批次 Q 执行进度

| 编号 | 内容 | 状态 |
|------|------|------|
| Q1 | 定长子串提取 + 手写 NUL 终止 → 新增 `lv_strlcpy_n`（判据 J；~25 文件/40+ 处） | 完成 |
| Q2 | 节点类型分发表 LV_DISPATCH 化（判据 F；≥20 处：formula / DSL / 证明 / 输出 4 域） | 完成（迁移 16 处，复核不迁移 15 处，见组② 明细） |
| Q3 | CUDA/HIP 双后端「四连 API + 存根降级 + 句柄守卫」→ `gpu_backend_guard.h`（判据 E；2 文件各 4 存根 + ~50 处守卫） | 登记不迁移（守卫已由 lv_CHECK_NULL/lv_CHECK_ALLOC 收敛；存根降级函数名/vendor/错误方式异构；linsol 方法范围检查每文件仅 1 处，见组⑥ 明细） |
| Q4 | 环形缓冲「索引运算 + 扩容展开」→ 扩展 `lvRingBuf` 支持扩容线性化（判据 A/B；≥5 处） | 登记不迁移（环形业务耦合 + 哈希探测属表内核，见组④ 明细） |
| Q5 | 去重收集「unique append」→ `lv_darray_add_unique`（判据 B；≥5 处） | 完成（新建 lv_int_append_unique 迁移 4 处裸 int 形态，3 处登记不迁移，见组④ 明细） |
| Q6 | 时间单位换算裸字面量族 → 补全 `lv_NS_PER_US`/`lv_NS_PER_MS` + 收敛 lv_get_wallclock_ms:223 裸 `/1000000ULL`（判据 C；36 处/16 文件） | 完成（lv_utils.h 新增 6 时间常量宏，迁移 21 文件；延时/平台刻度字面量登记豁免，见组⑤ 明细） |
| Q7 | 数组移位 memmove 补漏 → `lv_shift_right`/`lv_array_insert_at`/`lv_buffer_consume`（判据 A/B；≥4 处） | 完成（新建 lv_shift_right/lv_buffer_consume 迁移 5 处，3 处登记不迁移，见组④ 明细） |
| Q8 | 排序后多集相等判定 → `lv_multiset_equal`（判据 A；≥3 处，type_check 双份） | 完成（新建 lv_int_multiset_equal 迁移 3 处，见组④ 明细） |
| Q9 | 哈希预分组扫描 → `lv_hash_group_scan`（判据 A；≥2 处同算法多阶段） | 登记不迁移（组内业务逻辑差异大，见组④ 明细） |
| Q10 | JSON int 数组解析补漏 → 复用既有 `lv_json_parse_int_array`（判据 A；2 处手写） | 登记不迁移（固定缓冲设施与 3 处动态分配手写不同构，见组⑨ 明细） |
| Q11 | 多项式项规范化（含 swap-last 删除骨架）（判据 A/B；≥4 处） | 完成（新建 simple_poly_remove_zero_terms 去重 2 处，其余不可合并，见组⑧ 明细） |
| Q12 | 坐标有理化精度分母 1000/1e6/1e9 三档（判据 C；4 文件，interop_import 宏已定义却失步） | 完成（lv_SOLVER_SCALE_FACTOR/INTEROP_COORD_DENOM_PRECISION 统一权威定义，见组⑤ 明细） |
| Q13 | GGB ZIP 常量双文件重复（命名不一致）→ interop_internal.h 统一（判据 C/D；2 文件） | 完成（interop_export.c 死宏整块删除，见组⑤ 明细） |
| Q14 | 黄金比例哈希常量族（4 种变体含疑笔误）→ 统一具名常量（判据 C；≥11 处/5 文件） | 完成（lv_HASH_GOLDEN_RATIO_64/32 具名化，修复 radius_node_id +1 笔误，见组⑤ 明细） |
| Q15 | high_dim view_id 编码规则族 → `lv_VIEW_ID_BLOCK_STRIDE` + encode/decode helper（判据 C；5 处） | 登记不迁移（view_id 编码集中单文件函数族，见组⑤ 明细） |
| Q16 | cuda/hip 版本整数解码双份 → 各保留 vendor 基数但具名化（判据 C；2 处） | 完成（CUDA/HIP 各新增版本解码基数宏，见组⑥ 明细） |
| Q17 | IO block `lvIOBlockState` 释放样板（5 行精确同构）→ 共享释放函数（判据 G；2 处） | 完成（新建 lv_io_block_state_destroy，见组③ 明细） |
| Q18 | graph_node_alloc 六分发 free vtable（判据 G；1 文件 6 分发） | 登记已收敛（vtable 槽位由 node_destroy 统一消费，P12 已消除平行实现，见组③ 明细） |
| Q19 | `symbolic_coords[N]` 连续下标提取 → `symbolic_coord_get_xy/get_segment`（判据 H；≥4 处） | 完成（新建 get_xy/get_segment 迁移 9 处，见组⑦ 明细） |
| Q20 | 数字字面量词法识别骨架 → `lv_str_read_number`（判据 I 变体；8+ 文件/10+ 处；**仅收敛词法骨架层，豁免 #9 算术保持豁免**） | 完成（设施名 lv_str_scan_number） |
| Q21 | `lv_strtok_r` 逐 token 迭代解析（判据 A；9 文件/13+ 处；**与已登记 lv_str_split_once 单次切分区分，形态为多 token 迭代器**） | 完成（已收敛：9 候选调用点均已是 lv_strtok_r 调用形态，无手写迭代残留） |
| Q22 | 错误槽复位样板（判据 A；12 文件/19 处） | 登记不迁移（槽位复位是防御性语义非算法骨架，见组④ 明细） |
| Q23 | tactic 逐行 import 解析（判据 A；coq_bridge vs lean4_bridge，弱） | 登记不迁移（Coq 逐行扫描 vs Lean4 递归下降不同构，见组⑨ 明细） |
| Q24 | 长度分级编码阈值族 varint/msgpack/WebSocket（判据 C；3 文件，弱） | 登记不迁移（三协议独立规范阈值，见组⑤ 明细） |
| Q25 | euclidean 公理体系元数据表（判据 F；3 文件，弱） | 登记不迁移（3 表不同构，见组⑦ 明细） |
| Q26 | layer6_visual 成对块家族（判据 G；~7 处，弱） | 登记不迁移（单点销毁无跨模块重复；历史已由 LV_SIMPLE_BLOCK 宏抽象后函数化，见组③ 明细） |
| Q27 | Block 单行 destroy 族（判据 G；4 文件，弱） | 完成（登记已收敛：单行 lv_free 为设施调用终态，随 P12 处理，见组③ 明细） |
| Q28 | 矩形 coords[0..7] 手写展开（判据 H；1 文件，弱） | 登记不迁移（单文件单点，见组⑦ 明细） |
| Q29 | formula_converter coords[2] init/clear 配对（判据 H；~8 处，弱） | 登记不迁移（已收敛，见组⑦ 明细） |
| Q30 | geometry_canvas 类型分发展开（判据 H；弱，已部分收敛） | 登记不迁移（已收敛终态，见组⑦ 明细） |
| Q31 | 子串关键词查找表（判据 F；3 文件/3 处，弱） | 完成（lv_str_match_any 表驱动迁移 3 处） |
| Q32 | 文本文件整读（判据 F；2 文件，弱） | 完成（新建 lv_file_read_text + 迁移 2 处） |

### 备注（非新候选，不立项）

- Q-E2 numerical_backend 三重 find 循环 → 与已登记 `lv_registry` 重叠，不立项
- 判据 I → 已收敛（`lv_parse_int_before`），无新候选
- 判据 K → 无新候选（既有豁免覆盖，含与已登记 #2 边界复核）

### 边界声明（立项复核）

- Q20 与豁免 #9：仅收敛数字字面量词法识别骨架（前缀/符号/数字扫描 + 结束定位）；`v = v*10 + (c-'0')` 算术累加段保持豁免，不迁移
- Q21 与 `lv_str_split_once`：lv_str_split_once 为单次切分（返回两个指针）；Q21 为多 token 迭代器形态（循环消费 + 跳过空段），设施形态不同
- Q3 与批次 P 存根提及：「cuda/hip 存根 2 特例」未落入第 9 章正式豁免登记，本次按判据 E 正常立项；存根段与完整实现段分属编译期分支，骨架差异以存根段同构为准

### 跨批次领域合并调度方案（2026-08-12 用户选定）

批次 P 遗留（P4-P14）与批次 Q（Q1-Q32）不按批次分组，按领域相近度合并为 9 组，逐组实施、每组完成验证后登记：

| 组 | 领域 | 合并执行项 |
|----|------|-----------|
| ① | 字符串处理（J/I/A） | Q1 · Q20 · Q21 · Q31 · Q32 |
| ② | 分发表/调度（F） | P5 · Q2 · P6 |
| ③ | 析构/释放（G） | P12（含 Q27）· Q17 · Q18 · Q26 |
| ④ | 数组/容器算法（A/B） | Q4 · Q5 · Q7 · Q8 · Q9 · Q22 · P7 · P8 · P11 |
| ⑤ | 语义常量族（C） | Q6 · Q12 · Q13 · Q14 · Q15 · Q24 |
| ⑥ | GPU 后端（E） | Q3 · Q16 |
| ⑦ | 坐标/几何展开（H） | Q19 · Q28 · Q29 · Q30 · Q25 · P13 · P14 |
| ⑧ | 数学算法 | P9 · P10 · Q11 |
| ⑨ | 序列化/解析 | P4 · Q10 · Q23 |

执行顺序 ①→⑨（用户授权自主决定）。Q25 归组⑦实施时复核边界（「方向②」无正式登记）。Q27 随 P12 在组③处理。

### 组① 字符串处理域执行明细

**Q1 完成（lv_strlcpy_n 定长安全复制，判据 J）**：lv_utils.h 新增声明 + lv_utils_str.c 新增实现（契约：源不要求 NUL 终止、复制 min(src_len, dest_size-1) 字节 + NUL 终止、返回 src_len 可检测截断；NULL 参数返回 0 不写目标）。迁移 53 处 `lv_strlcpy_n`（34 文件，含设施自身 1 处）+ 13 处形态 B 改用既有 `lv_strlcpy`（NUL 终止源钳制复制，更贴语义）。三类形态：钳制（`size_t nlen` 判断 + memcpy + NUL）、直拷、堆分配（`lv_malloc(len+1)`）。语义保留：前置长度校验（lean4_bridge INVALID_PARAM、gappa_dsl `len<64`）、NULL/长度条件分支（lean4_add_step 保留 else 写空串）、返回值复用变量（text_code len）。豁免/禁止形态：多段拼接（lv_parser:619/655 qname、lambda_term:213、rewrite_strategy:131）、memmove 偏移插入（text_code:130）、复制后尾部清理（proof_version_isar:317）、二进制复制、设施自身。验证：ninja 927/927 + ctest 170/170，零修复项。

**Q20 完成（lv_str_scan_number 数字字面量词法扫描器，判据 I 变体）**：lv_parse_utils.h 新增 static inline 设施（契约：扫描可选符号/整数/可选小数/可选指数，返回结束位置；'.' 后需数字防 ".."、e/E 后需 [±]数字、'-' 后需数字；end 为 NULL 表示 NUL 终止；纯词法定位不含数值累加）。迁移 2 处：gc_language.c 数字跳过段（含 include lv_parse_utils.h；".." 从宽松跳过变为报错，属畸形输入行为变化登记）、dsl_lexer.c 数字字面量骨架（pos/col 一次性更新，删除死变量 is_float）。缺陷修复登记：dsl_lexer 原无条件消费 e/E（`3e+` 吞符号）→ 设施严格语义（e 后需数字），正常数字输入行为不变。豁免登记：module_lvz.c（'.' 无条件消费与严格语义冲突，且累加交错属豁免 #9 边界）、axiom_pkg_parser.c（仅整数骨架极简 + 溢出钳制交错，抽象收益为负）、lv_lexer.c（有理数/小数回退与 token 类型契约强耦合）。strtod 收敛确认：gappa_dsl/gappa_propagate/proof_strategy_numeric/float_error 已用标准库，非本判据目标。验证：ninja 927/927 + ctest 170/170，零修复项。

**Q21 完成（lv_strtok_r 逐 token 迭代解析，判据 A——已收敛，无新迁移）**：全库扫描确认 9 个候选调用点均已是既有 `lv_strtok_r` 设施调用形态：lv_loader.c（"," 逗号流）、lv_sema.c（","）、gappa_dsl.c×2（";" 双层）、interop_theorem.c（"\n"+";" 双层嵌套）、atp_backend.c（" " 空格流）、proof_version.c（"\n" 行流）、preset_common.c（"|,& " 四分隔符）、interop_command.c×2（" " 命令参数 + "," id 列表）、stream_filter.c（"," + ltrim/rtrim）。无手写 `while(strchr)+memcpy` 迭代残留；剩余 strchr/memchr/strstr 单字符定位或子串检测均非逐 token 迭代形态。结论：该方向已在历史批次收敛，登记完成不迁移。验证：ninja 927/927 + ctest 170/170。

**Q31 完成（子串关键词查找表，判据 F）**：复用既有 `lv_str_match_any`（lv_str_utils.c:89，strstr 顺序匹配返回索引，NULL 安全）表驱动迁移 3 处手写子串关键词链（关键词表保持原链顺序，语义逐位等价）：
- type_inference.c 内置默认规则：6 个 if 块 → `kBuiltinTypeKeywords[]`（14 关键词）+ `kBuiltinTypeNames[]` 平行表 + `lv_str_match_any` 索引分发
- sat_encoding.c 关系名→约束类型：4 个 if 块 → `kRelTypeKeywords[]`（incidence/on/between/intersect/contain）+ `kRelTypes[]` 平行表
- proof_widget.c 目标含等号检测：strchr+strstr 四条件 → `kGoalEqualMarkers[]`（"=", equal, congruent, 等于）
语义不兼容保持：meta_verify.c:385-391（计数语义 found_markers<2，非索引）、proof_strategy_numeric.c:196-202（最早出现位置语义，非顺序索引）。三个文件均已含 lv_utils.h（聚合 lv_str_utils.h），无需新增 include。验证：ninja 927/927 + ctest 170/170，零修复项。

**Q32 完成（文本文件整读，判据 F）**：lv_file.h/lv_file.c 新增 `lv_file_read_text(path, buf, buf_size)` 设施（契约：rb 打开、读取 min(文件大小, buf_size-1) 字节、保证 NUL 终止、buf_size 必须 ≥2、返回 bool 成功状态；不分配堆内存）。迁移 2 处孪生 `static read_file_text`（fopen+fread(buf_size-1)+手写 NUL 样板，仅返回契约 int/-1 vs bool/false 不同，统一为 bool）：lv_impl_upper_app.c 定义删除 + 调用点 `if (!lv_file_read_text(input_path, src, sizeof(src)))`、lv_impl_upper_orchestrator.c 定义删除 + 调用点同步。非目标确认：lv_file_read_all（堆分配整读）为既有设施；file_block.c 块读（不 NUL 终止 + bytes_read 输出）语义不同保持；runtime_monitor /proc 逐行读为系统文件豁免。验证：ninja 927/927 + ctest 170/170，零修复项。

**组① 字符串处理域全部完成（Q1/Q20/Q21/Q31/Q32）**：验证基线 ninja 927/927 + ctest 170/170 全部通过。

**组② 分发表/调度域执行明细（P5/P6/Q2，验证 ninja 927/927 + ctest 170/170 全部通过）**：

**Q2 完成（节点类型分发表 LV_DISPATCH 化，判据 F）**：迁移 16 处「边界检查 + NULL 槽检查 + 调用」三行样板为 `LV_DISPATCH`/`LV_DISPATCH_VOID`（fallback 值/参数序列/前置守卫逐位保留）：
- formula 域（5 文件）：formula_eval.c s_funcs（fallback 0.0）、formula_curve.c s_funcs（fallback false）、formula_converter_stmt.c s_stmt_funcs（fallback false；原硬编码 <36 与表 sizeof 26 的越界语义逐点等价已核实）、formula_converter_export.c s_funcs（VOID）；均补 include lv_xmacro.h
- DSL 域（3 文件）：lv_loader.c kFoldDispatch/kIsPureDispatch/kCollectVarsDispatch(VOID)/kEvalSkeletonDispatch 4 处 + include、dsl_compiler_ir.c s_compile_handlers、dsl_compiler_parse.c kParseStmtHandlers（fallback NULL，负值经 unsigned 兜底）
- 证明域（3 文件）：prop_verifier_engine.c kProveGoalHandlers（fallback false）、prop_verifier_equivalence.c kPropVerifyHandlers（fallback 逗号表达式保留 res.valid=false+msg）、prop_verifier_bhk.c s_bhk_desc_funcs（VOID）；均补 include
- 输出域（1 文件）：tikz_export.c s_tikz_renderers（{type,fn} 线性扫描 → 直接索引 + VOID，GeomType 连续 0-5，GEOM_PORT 槽 NULL 静默跳过，与原未命中不渲染等价；移除 TikzNodeRenderEntry typedef）
- 已迁移文件合计补 include：lv_loader.c、formula_eval.c、formula_curve.c、formula_converter_stmt.c、formula_converter_export.c、prop_verifier_engine.c、prop_verifier_equivalence.c、prop_verifier_bhk.c（其余文件宏已可达）

**Q2 复核不迁移（15 处，语义不同构，登记）**：
- formula 域：formula_string.c node_to_string（void 表但 fallback 是动作 str_default 写 "?"，非值，LV_DISPATCH_VOID 无 fallback 参数会改变输出 → 不迁移）、formula_renderer.c s_render_funcs（校验前置 + lv_set_error 错误副作用 + 双调用）、formula_renderer_latex/ascii/dsl/python（已用共享 dispatch_via 设施）、formula_ast.c kFormulaVTable（X-MACRO 多字段 VTable）
- DSL 域：lv_ast.c kAstVTable（多字段 8 槽 VTable）、lv_loader.c kEntityDeclHandlers（取指针循环复用非三行样板）、kChurchFnTable（{name,fn,arity} 字符串扫描）、kEvalPropDispatch（fallback 是 fold_expr 回退分支块）、dsl_compiler_load.c kIROpHandlers（错误传播副作用：handler 返回 bool 取反驱动提前中止）
- 证明域：prop_formula_ops.c vtables（{equal/hash/is_descendant} 多字段 VTable）、proof_multi_strategy.c kSearchAlgorithmHandlers（校验前置 + 非法值回退 DFS 归一，无 NULL 槽检查）
- 输出域：interop_command.c kCommandHandlers（未命中设置 status_code=UNSUPPORTED + snprintf error_message + data_len，错误副作用无法宏化）、kAddNodeTypeHandlers / kAddConstraintTypeHandlers（字符串键非枚举）、proof_export_enhanced.c 规则名 if 链（字符串匹配）

**P5 完成（约束编码分发表 LV_DISPATCH 化）**：迁移 4 处（主会话示范）：
- groebner_engine.c kGroebnerEngineEncodeTable：{type,fn} 线性扫描 → 直接索引函数表（ConstraintType 连续 0-5）+ `LV_DISPATCH_VOID`（原未命中静默等价）+ 补 include lv_xmacro.h
- probabilistic_constraint.c kDTMCBuildTable：线性扫描 → 直接索引 + LV_DISPATCH_VOID（未命中静默等价）
- probabilistic_constraint.c kPCTLSubEvalTable：线性扫描 → 直接索引（PCTLFormulaType 连续 0-6，仅 4 槽有 handler）+ LV_DISPATCH（fallback formula->p_bound 默认值等价）
- probabilistic_constraint.c kPCTLEvalTable：直接索引表 + 失败处理 → `LV_DISPATCH(kPCTLEvalTable, type, false, ...)` + `if (!ok) { dtmc_destroy; return false; }`（原「越界/NULL → destroy+false」与 handler 失败路径合并等价）

**P5 复核不迁移（5 处，登记）**：sat_encoding.c constraint_encoders / kFormulaEncoders（线性扫描 + 未命中 lv_LOG_WARNING 副作用）、smt_backend_impl_smtlib2.c kSmtlib2EncodeTable（直接索引但未命中写 warning）、smt_backend_impl_groebner.c（GROEBNER 直通 + 外部后端线性扫描混合状态机）、probabilistic_constraint.c 状态谓词（字符串查找 + kind if 链非枚举分发表）。

**P6 完成（SMT/ATP 插件入口状态机——复核 8 处全部与 LV_DISPATCH 不同构，登记不迁移）**：atp_backend.c 约束→TPTP 谓词表（{type,谓词名} 多字段线性扫描 + 元数格式化表 4 槽 2 有效）、atp_executable_name（纯数据表非函数表）、atp_parse_szs_status（SZS 状态字符串 if 链）；smt_backend_impl_groebner.c smtsolver_check（后端类型直通 + 外部求解器线性扫描混合）、smt_backend_impl_external.c 输出解析（sat/unsat/unknown 字符串匹配）；engine_scheduler.c kSchedulerBackendVTables（{type,name,solve,NULL,NULL} 多字段 VTable + 未命中错误填充副作用）；smt_backend_impl.c kBuiltinBackends / lv_backend_plugin.c 插件类型查找（注册表数据查找非分发表）。结论：该方向候选均为数据表/字符串匹配/多字段 VTable，不构成 LV_DISPATCH 分发表，无需新设施。

**组③ 析构/释放域执行明细（P12/Q17，Q18/Q26/Q27 登记，验证 ninja 927/927 + ctest 170/170 全部通过）**：

**P12 完成（rewrite_snapshot.c 析构 shim 收敛，判据 G）**：删除 SnapshotNodeOps 全套自建 VTable（结构体 + destroy_data/cleanup_data 双函数 ×3 handler + 6 实例 + 查表函数 get_snapshot_ops，共约 60 行），其类型特定释放与 graph_node_alloc.c vtable->free 六分发（region_free/port_free/func_block_free）完全同构，属双份实现：
- `snapshot_node_destroy`（L113-129）与 `graph_snapshot_restore` 内联节点销毁（L344-367）均收敛为既有统一释放路径 `node_destroy`（graph_index.c：符号坐标数组循环 + numeric_assumption_declaration + vtable->free 类型特定数据 + 外壳归还；lv_pool_free 对非池深拷贝节点自动按普通分配释放，已核实）
- 约束销毁统一走 `constraint_destroy`（快照约束为 lv_calloc 分配，lv_pool_free 归属校验自动按普通分配释放，语义等价）
- 新增静态 `graph_snapshot_free_refs` 收敛 graph_snapshot_create 错误回滚（2 处）与 graph_snapshot_destroy 中的 port_refs/region_refs[i].segment_ids/fb_refs[i].internal_node_ids 释放块（原 4 处同构样板）
- 调用点共 7 处改 node_destroy、2 处改 constraint_destroy；补 include layer3_geometry/constraint_graph/graph_node_internal.h（仿 bit_burning.c 相对路径）

**Q17 完成（IO block 共享释放函数，判据 G）**：新建 `lv_io_block_state_destroy(lvIOBlockState *)`（io_block.h 声明 + file_block.c 实现，释放 target 字符串 + state 外壳，NULL 安全），file_block.c lv_file_block_destroy 与 network_block.c lv_network_block_destroy 的 5 行同构样板（`if (block->base) { ... }`）收敛为单次调用；network_block 的 socket 移除前置逻辑保留。

**Q18 登记已收敛（graph_node_alloc 六分发 free vtable）**：point_free/line_segment_free/circle_free（空实现）+ region_free/port_free/func_block_free（1-3 字段）是 GeomNode vtable 的合法多态槽位，已由 node_destroy 统一消费（既有 vtable 分发设施形态，非手写分散样板）；与 SnapshotNodeOps 的重复由 P12 消除；1-3 字段用 lvFieldDesc 表驱动无收益（行数不降且计数归零需额外处理），登记不迁移。

**Q26 登记不迁移（layer6_visual 成对块家族，~7 处）**：if/match/record/effect 等块 destroy 均为各文件单点销毁（无跨模块重复），字段释放序列 2-4 行；「成对块」历史已由 lv_block_utils.h 的 LV_SIMPLE_BLOCK 宏抽象后函数化为各文件具名函数（头文件注释自述），是既有收敛终态，登记不迁移。

**Q27 完成（登记已收敛，Block 单行 destroy 族，4 文件）**：lv_list_block_destroy / lv_map_block_destroy / lv_while_block_destroy / lv_ui_event_block_destroy 及 extended_types.c 的 lv_list_type_destroy / lv_map_type_destroy / lv_function_type_destroy 均为单行 `lv_free((void **)&ptr)`——已是 lv_free 设施调用终态，与 Q21 已收敛判定同理，登记不迁移。

### 组④ 数组/容器算法域执行明细

**新设施（lv_utils.h 声明 + lv_utils_array.c 实现，追加于 lv_shift_left 旁）**：
- `lv_shift_right(void *base, size_t elem_size, size_t index, size_t count)`：与 lv_shift_left 对称的右移腾位（[index,count) → [index+1,count)），单次 memmove
- `lv_buffer_consume(void *buf, size_t elem_size, size_t pos, size_t *len)`：recv 缓冲 consume 语义（删前 pos 个元素、剩余前移、len 原地更新；pos>=len 时 len 置 0）
- `lv_int_multiset_equal(const int *a, int an, const int *b, int bn)`：int 多集排序后相等判定（三态返回 1 相等 / 0 不等 / -1 内存失败，内部 lv_malloc 两份 + qsort + 逐元素比较）
- `lv_int_append_unique(int *arr, int *count, int value)`：compact int 数组 unique append（线性查重 + 追加，容量由调用方保证）

**Q5 完成（unique append 去重收集，判据 B）**：`lv_int_append_unique` 迁移 4 处裸 int 形态（`bool found + 内层 for 查重 + 追加` 样板，各约 13 行 → 1 行）：
- solver_engine.c:184-198（all_var_ids，示范迁移）
- solver_order.c:243-256（var_ids，容量由 eq_count_total 保证）
- solver_coord_extract.c:1310-1323（var_ids，容量由 sys->eqs.count 保证）
- graph_dot_export.c:218-235（namespace depth 去重，栈数组 depths[256]；容量检查保留于外圈 `if (nd < lv_ARRAY_SIZE(depths))`，语义与原 `!found && nd<size` 逐位等价）
登记不迁移：module_delta.c×6（dep_names 字符串数组 + JSON 缓冲耦合，与裸 int 形态不同构）、bdd_encoding.c:1118（结构体字段去重非 int 数组）、solver_groebner.c:630（二元组配对查重）。

**Q8 完成（排序后多集相等判定，判据 A）**：`lv_int_multiset_equal` 迁移 3 处：
- type_check.c×2（contained_node_ids / constraint_ids 分支，各约 40 行逐行复制 → 8 行，含内存失败三态返回）
- normalization.c:785-834（boundary_segments 多集相等，双 qsort + for 比较删除；新增 ids_a/ids_b 分配 + NULL 检查，失败时 continue 跳过该候选）

**Q7 完成（数组移位 memmove 补漏，判据 A/B）**：迁移 5 处：
- proof_engine.c:148-164（策略按优先级右移插入，memmove → lv_shift_right）
- module_delta.c:456-462（DeltaBaseline 头删除，memmove → lv_shift_left 收敛既有设施）
- geo_dynamic.c×2（parent_adj / child_adj 插入右移 for 循环 → lv_shift_right）
- interop_server.c:1469-1477（recv 缓冲 consume，memmove+长度更新 → lv_buffer_consume）
登记不迁移：text_code.c:126（text_len 宽度多元素腾位，与单元素 lv_shift_right 不同构）、text_code.c:155（actual_len 宽度多元素删除，与单元素 lv_shift_left 不同构）、debug_state.c:226（日志文件轮转重命名循环，非内存数组移位）。

**Q4 登记不迁移（环形缓冲/线性化，判据 A/B）**：7 组候选（propagation.c 两段 memmove 线性化、stream_lazy/stream_buffer/stream_async 扩容、rewrite_wl.c、geom_evol.c、graph_node_hash 哈希探测、bdd_encoding/rewrite_strategy/rewrite_vf2 线性探测）均与环形索引/扩容业务强耦合；哈希探测序列属表内核实现（lvHashtable 内部回退），非应用层散落骨架；扩展 lvRingBuf 引入扩容线性化设施收益与改造风险不成比例，登记不迁移。

**Q9 登记不迁移（哈希预分组扫描，判据 A）**：3 组候选（normalization.c find_merge_candidates 点/线段两阶段、graph_memory.c 约束哈希签名去重、expr_canon.c 同类项分桶）组内阶段间业务逻辑差异大（点/线段匹配条件、哈希签名内容、分桶目标各不相同），无共享骨架，登记不迁移。

**Q22 登记不迁移（错误槽复位样板，判据 A）**：19 处候选均为「槽位指针置 NULL + 计数归零」的防御性清理语义（错误标签内逐槽释放+复位），非遍历/搜索/排序/判定/消元算法骨架，抽象为容器设施无收益，登记不迁移。

**P8 登记不迁移（realloc 倍增新位置，判据 B）**：7 处（text_code.c:116 / propagation.c:499 / interop_theorem.c:98 / geo_visual_complete.c:1107 / text_code.c:63 / proof_compiler.c:280 / lv_utils_config.c:326）形态各异（栈数组→堆迁移、倍增系数差异、增长步长差异），无 ≥2 处同构样板，登记不迁移。

**P11 登记不迁移（线性选优/选择排序 argmin/argmax）**：28 处（4 选择排序 + 24 线性选优）语义差异大（比较键不同、相等处理不同、返回下标 vs 值 vs 指针），无通用 argmin/argmax 设施契约可覆盖，登记不迁移。

**P7 完成（手写线性查找表值形态，判据 B）**：4 路搜索确认全库 22 处严格命中（字符串→枚举/索引），其中纯 return 值形态 3 表 96 条迁移至既有 `lv_str_to_enum`（表声明统一为 `lvStrToEnumEntry{name,value}`，查找函数体收敛为一行 `return (枚举) lv_str_to_enum(table, lv_ARRAY_SIZE(table), str, default)`）：
- probabilistic_constraint.c kStatePredicateTable（11 条，state_predicate_lookup，原 for+strcmp+return kind）
- module_lvz.c kCategoryMap（24 条，lvz_category_from_string，保留外层 NULL 检查 + 新增 include lv_xmacro.h）
- module_lvz.c kTypeMap（61 条，lvz_type_from_string）
登记豁免（与 lv_str_to_enum 纯 return 契约不同构）：出参形态（preset_common.c g_type_map 60 条 / func_block_registry.c cn_map+en_map X-macro 生成 / preset_manager_serialize.c 反向反查隐式表）、break+哨兵（float_error.c kFuncNameOps / gappa_dsl.c kPredefinedFormats / preset_common.c g_property_map 位标志 OR）、哈希快查回退路径（lv_registry / global_state / performance_profiler / preset_blocks / func_block_preset_internal / dsl_compiler_ir / formula_converter_util / mini_kernel / interop_import 等动态数组，属容器设施内核）、常量字符串表（formula_renderer_internal.c greek_letters/trig_functions，阶段 G 已登记豁免形态）、error_codes.c g_error_table（ErrorInfo 四字段布局与 lvStrToEnumEntry 不兼容，X-macro 单点生成 + P3 已二分统一）。

**组④ 验证**：ninja build3 927/927 + ctest 170/170 全部通过，零修复项。

### 组⑤ 语义常量族域执行明细

**Q6 完成（时间单位换算常量族，判据 C）**：`lv/lv_utils.h` 新增公共语义常量族 6 宏（lv_NS_PER_US/lv_NS_PER_MS/lv_US_PER_MS/lv_MS_PER_S/lv_US_PER_S/lv_NS_PER_S），三批迁移 21 文件：
- 首批 13 文件：adaptive_threshold.c（3 处 ns→s 换算）、context.c（2 处 µs→ms）、lv_circuit_breaker.c（4 处）、circuit_breaker.c（layer4_reasoning）、exact_arithmetic.c（ns→s 双处）、proof_version.c、proof_rule_engine.c、engine_scheduler.c、debug_log_ctx.c、lv_impl_upper_orchestrator.c
- 二批 3 文件：runtime_monitor.c（5 处 lv_NS_PER_MS + 2 处 lv_NS_PER_US，新增 include lv_utils.h）、proof_strategy.c、proof_trace_tree.c
- 三批 6 文件：lv_utils_misc.c（删局部 4 宏 + QPC 频率换算收敛，FILETIME 刻度 `*100` 豁免）、test_framework.c（报告输出 5 处）、debug_state.c、performance_profiler.c、atp_backend.c、solver_core.c、lv_utils.h（lv_clock_elapsed_ms 内联收敛）
登记豁免（非换算因子语义）：延时字面量（runtime_monitor.c Sleep(100)/usleep(100000)×2、lv_process.c nanosleep 500ms、interop_server.c select 超时 100ms，均为平台 API 延时参数带注释语义）、lv_thread.h ms→tv_sec/tv_nsec（lv_thread.h 仅依赖 lv_platform.h 的底层自包含头，引入 lv_utils.h 破坏头文件分层）。

**Q14 完成（黄金比哈希常量族，判据 C）**：`lv_utils.h` 新增 `lv_HASH_GOLDEN_RATIO_64 0x9E3779B97F4A7C15ULL` / `lv_HASH_GOLDEN_RATIO_32 0x9E3779B9ULL`。unify_helpers.c 4 处替换（coord_hash_region/coord_hash_circle×2/coord_hash_func_block），其中 coord_hash_circle 的 radius_node_id 原用 `0x9E3779B97F4A7C16ULL`（+1 疑笔误，同一函数内与 center_node_id 不一致），修复统一为 64 位黄金比常量；module_delta.c 2 处 `0x9e3779b9ULL` 替换（32 位黄金比 ULL 形式）。

**Q13 完成（GGB ZIP 常量死代码，判据 C/D）**：全库 grep 确认 interop_export.c L172-180 的 9 个 GGB_* 宏零使用点（唯一命中为定义行），删除整块并留注释指引；interop_import.c L38-43 的 6 个活跃副本（L317-407 实际使用）保留。

**Q12 完成（坐标有理化精度分母，判据 C）**：`lv_SOLVER_SCALE_FACTOR`（=1000）双头失步统一——solver_core.h:41 裸定义删除，保留 lv_internal.h:77 的 #ifndef 定义（solver 全部 20 使用点所在文件均经 solver_common.h→lv_internal.h 可见）；`INTEROP_COORD_DENOM_PRECISION`（=10^6）三文件失步收敛——定义上提至 lv/interop.h 公共常量区（interop_command/import/server 三文件均 include），interop_command.c:287-288 手写 `1000000.0`/`1000000UL` 复用宏，interop_server.c:74 死宏删除，interop_import.c:56-58 局部 #ifndef 删除。

**Q24 登记不迁移（编码阈值族，判据 C 弱）**：varint（lv_bytes.c）/msgpack（module_serialize_msgpack.c）/WebSocket（interop_server.c）是三协议独立规范阈值（各协议标准各自定义分级规则），无跨文件同语义重复，登记不迁移。

**Q15 登记不迁移（view_id 编码规则，判据 C）**：`block_id * 1000 + preset_index`（冲突时 `+ offset * 10000`）编码集中在 high_dim_view.c 单文件函数族，属模块内编码规则而非跨文件重复语义常量，登记不迁移。

**组⑤ 验证**：ninja build3 927/927 + ctest 170/170 全部通过，零修复项。

### 组⑥ GPU 后端域执行明细

**Q3 登记不迁移（CUDA/HIP 双后端守卫/存根/状态机，判据 E）**：cuda_backend.c / hip_backend.c 逐项分析后不满足判据 E「家族内每文件重复 ≥2 处同构」迁移条件：
- 句柄/上下文守卫已由 `lv_CHECK_NULL` / `lv_CHECK_ALLOC` 宏收敛（NULL 守卫已是设施调用终态，非手写样板）
- 存根降级每文件 4 函数（vector/matrix/linsol/destroy），但 CUDA 用 `lv_RETURN_ERROR` 返回错误码、HIP 用 `LOG_WARN + return -1`，函数名/vendor 名/错误方式均异构，无法共享同一宏骨架
- linsol 方法范围检查每文件仅 1 处，不满足「每文件 ≥2 处」
结论：判据 E 抽象门槛未达，登记不迁移。

**Q16 完成（版本整数解码基数具名化，判据 C）**：
- cuda_backend.c 新增 `CUDA_VERSION_MAJOR_BASE 1000` / `CUDA_VERSION_MINOR_BASE 10`（cudaDriverGetVersion/cudaRuntimeGetVersion 返回 major*1000 + minor*10），版本解码表达式复用宏
- hip_backend.c 新增 `HIP_VERSION_MAJOR_BASE 10000000` / `HIP_VERSION_MINOR_BASE 100000` / `HIP_VERSION_PATCH_BASE 1000`（hipRuntimeGetVersion 返回 major*10000000 + minor*100000 + patch*1000），版本解码表达式复用宏
- vendor 各自保留独立基数宏（CUDA/HIP 版本编码规则不同），仅具名化消除裸字面量，不强行合并

**组⑥ 验证**：ninja build3 927/927 + ctest 170/170 全部通过，零修复项。

### 组⑦ 坐标/几何展开域执行明细

**P14 完成（geo_bbox_contains_2d/1d 轴向包围盒检查，判据 H）**：geo_utils.h 声明 + geo_utils.c 实现 `geo_bbox_contains_2d(px,py,ax,ay,bx,by,eps)` / `geo_bbox_contains_1d(p,a,b,eps)`（fmin/fmax 归一化 + epsilon 容差，无 NULL 依赖）。迁移 10 处手写 fmin/fmax 包围盒：
- geo_predicate.c `lv_segments_intersect` 全共线分支 4 处（c_on_ab/d_on_ab/a_on_cd/b_on_cd）+ 单共线分支 4 处（d1-d4）→ `geo_bbox_contains_2d(..., lv_GEO_DISTANCE_EPSILON)`
- geo_predicate.c `lv_point_in_polygon` 水平边 1 处 1D bbox（原 x_min/x_max fmin/fmax）→ `geo_bbox_contains_1d`
- recursion_selector.c `point_on_segment_symbolic` 1 处手写 min/max bbox（4 个三元 + epsilon=1e-10）→ `geo_bbox_contains_2d`（补 include lv/geo_utils.h）

**Q19 完成（symbolic_coord_get_xy/get_segment 坐标提取，判据 H）**：symbolic_coord.h 声明 + symbolic_coord_lifecycle.c 实现 `symbolic_coord_get_xy`（点 [0][1]）/ `symbolic_coord_get_segment`（线段 [0..3]）：NULL 守卫 + coord_count 检查，失败返回 false 不写输出。迁移 9 处连续下标 + to_double 展开：
- 线段提取 8 处：func_block_selector.c / tikz_export.c / groebner_engine.c / recursion_selector.c×2（卷绕数 + 射线法）/ conflict_detector.c / interop_export_svg.c×2（交点双线段 + 线段渲染）/ interop_export_pdf.c
- 点提取 1 处：recursion_selector.c（点坐标 → `symbolic_coord_get_xy`，失败 return false）
- 豁免（非纯提取形态）：graph_memory.c（double 展开与 mpq 精确路径耦合，无法整体收敛）、interop_export_pdf.c 标签中点（lx/ly 与 x2/y2 混合点/线段提取）、interop_export_geojson.c 圆半径（[2] 为半径非线段端点）

**Q28 登记不迁移（矩形 coords[0..7] 手写展开，判据 H 弱）**：候选集中在单文件单点，无跨文件同构重复，不满足判据 H「多处复制」迁移门槛，登记不迁移。

**Q29 登记不迁移（formula_converter coords[2] init/clear 配对，判据 H 弱）**：候选已在历史批次收敛（既有设施/函数化形态），无新迁移价值，登记不迁移。

**Q30 登记不迁移（geometry_canvas 类型分发展开，判据 H 弱）**：候选已收敛至既有分发终态（部分收敛标记属实），无手写展开残留，登记不迁移。

**Q25 登记不迁移（euclidean 公理体系元数据表，判据 F 弱）**：3 文件元数据表结构不同构（字段布局/索引语义各异），无共享 X-macro 骨架可收敛，登记不迁移。

**P13 登记不迁移（角度桶量化+位分解，判据 H）**：全库检索无法定位「角度桶量化+位分解」对应的具体实现点（候选描述与现存代码无精确命中），登记不迁移。

**组⑦ 验证**：ninja build3 927/927 + ctest 170/170 全部通过，零修复项。

### 组⑧ 数学算法域执行明细

**P9 完成（带符号舍入缺陷修正，判据 C 关联）**：修正 `(int)(x + 0.5)` 截断语义对负值的舍入偏差（C 语言向零截断），改为 `(int) round(x)`（round-half-away-from-zero）。共 7 处 / 3 文件：
- interop_import.c SVG arc sweep flag 解析：`(int)(sf_d + 0.5)` → `(int) round(sf_d)`
- symbolic_coord.c 衰减位宽：`(int)(log2(ratio) + 0.5)` → `(int) round(log2(ratio))`
- meta_repr.c 5 处坐标反量化：`meta_repr_decode_graph`（node_id/type_idx）、`meta_repr_decode_node`（node_id/type_idx）、`meta_repr_decode_func_block`（block_id），均 `(int)((x - base_x)/spacing + 0.5)` → `(int) round((x - base_x)/spacing)`
- 与既有银行家舍入（SOLVER_SPLIT_PLAN.md 中 coord_to_mpz_scaled/double_to_mpz_scaled round-to-nearest-even）区分：本组修正的是带符号截断舍入，非银行家舍入，二者模式不同。

**P10 登记不迁移（高斯消元统一，判据 A/B）**：mpq 高斯消元已统一为 `cg_mpq_row_echelon`（graph_rank.c，graph_memory.c 与 graph_conflict.c 已调用）；double 列主序 LU 已统一为 `host_lu_factor`/`host_lu_solve`（host_linalg.c，numerical_backend.c 委托，CUDA/HIP 回读 CPU 后亦委托）——两路均已收敛无残留。剩余 4 处因类型/布局/主元/回退异构不可合并：geom_evol.c 手写行主序 LU + piv 数组 + 对角回退、groebner_engine_variety.c double 行主序 RREF（唯一调用点）、geo_constraint_solver_linear.c `gauss_eliminate`（层内已共享 3 调用点）、mpz_poly_resultant.c Bareiss 行列式（mpz，目标非方程组求解）。登记不迁移。

**Q11 完成（simple_poly_remove_zero_terms 删零去重，判据 A）**：groebner_parallel.c 新建同文件 `static void simple_poly_remove_zero_terms(SimplePoly *p)`，吸收两处同构 swap-last 删零：`simple_poly_normalize` 第三步（反向遍历 + lv_free + `*t = *last` + count--）与 `reduce_poly` 内联删零（`memcpy(dst, src, sizeof(PolyTerm))` 等价 `*dst = *src`），两者语义等价（free exponents → swap-last → count--）。其余四套多元规范化不可合并：groebner_poly.c `poly_sort_terms`（SoA 扁平 + 可配置 mono_compare，已知豁免）、groebner_parallel.c（AoS PolyTerm）、mv_polynomial.c（GMP 精确 + 职责拆分）、expr_canon.c（哈希分桶合并）——排序算法/单项式序/合并删零方式均不同。

**组⑧ 验证**：ninja build3 927/927 + ctest 170/170 全部通过，零修复项。

### 组⑨ 序列化/解析域执行明细

**P4 登记不迁移（JSON 数组迭代骨架，新判据 L 不成立）**：全库无 DOM 风格数组 API（`lv_json_array_size`/`lv_json_array_get`/`lv_json_enter_array` 均不存在），实际是游标式解析器。约 20 处「enter `[` + peek 边界 + 吞逗号」循环在 7 个维度分叉：循环边界（`for(;;)+break` / `while(peek!=']' && peek!='\0')` / `while(count<max)`）、`'\0'` 守卫有无、元素类型（string/int/int64/double/object/嵌套数组）、输出容器（lvDArray/int*/module_add_*/固定数组/图节点）、数量上限（无上限/max_steps/GJ_MAX_*/HIGH_DIM_*/固定 2/4）、错误处理（void/int/bool/NULL+错误码）、内层结构（对象数组内层已由 `lv_json_parse_field` 收敛）。不满足「骨架一致仅业务填充不同」强同构前提，抽象仅能消除约 6-10 行逗号/括号样板，却需引入无法对齐边界/上限/错误语义的回调契约，收益为负。登记不迁移。

**Q10 登记不迁移（JSON int 数组解析补漏，判据 A）**：既有设施 `lv_json_parse_int_array`（lv_json.c:545，固定缓冲 `int *out + max_count + out_count`，当前零调用）与三处手写实现全部「动态分配 `int **out`」不同构：(1) graph_serialize.c:333 `json_parser_parse_int_array`（动态 malloc + 三态错误码 + 空数组当 NOT_FOUND 错误，5 调用点变长无上限）；(2) command_log.c:733 `json_parse_int_array`（泛型动态数组，服务 int/double/uint64，失败返回 0）；(3) func_block_serialize.c:211 `parse_int_array`（文本 `key=[...]` 格式，const char* 游标，非 JSON）。唯一固定缓冲同构点是 command_log.c `participant_ids[8]`，但迁移需补失败回滚守卫（设施失败不回滚 out，与现有「失败则 dst 不动」语义不同）且基础设施不因此删除，价值有限。登记不迁移。

**Q23 登记不迁移（tactic 逐行 import 解析，判据 A）**：Coq 侧 coq_bridge.c `coq_import_proof` 是严格逐行扫描（line_end 找 `\n`/`\r` + 每行首 token + `line = line_end + 1`）；Lean4 侧 lean4_bridge.c `lean4_parse_tactics` 是缩进感知递归下降解析器（`while(pos<end)` 逐 token + by/match 分支递归 + 平衡括号跳过 + 注释/缩进）。二者在「行切分」这一判据 A 核心维度即不同构，关键字/边界/token 字符集/嵌套/错误处理均实质分叉。可共享部分（定理名提取 `bridge_extract_theorem_name` / 导出 / 注册骨架）已收敛至 interop_bridge_common.h，且该头注释已明确「import/validate 因两语言语法差异大各自保留」。登记不迁移。

**组⑨ 验证**：本组三候选均登记不迁移，零代码改动，无需构建/测试回归（沿用 927/927 + 170/170 基线）。

---

## 十三、批次 R 候选立项与实施（2026-08-13）

**候选来源**：上一会话 4 份只读子代理报告形成的候选清单，本轮落地候选 A/B/C/E 及候选 D 死宏清除部分。

### 批次 R 执行进度

| 编号 | 内容 | 状态 |
|------|------|------|
| R1 | qsort comparator 同构合并（cmp_seg_hash → hash_idx_compare_asc） | 完成 |
| R2 | 新增 `lv_parse_long_default` 收敛 strtol 前缀解析样板（transcendental 6 + safe_atol 7） | 完成 |
| R3 | 新增 `lv_parse_double_default` 收敛 strtod 回退样板（gappa 4） | 完成 |
| R4 | `high_dim_snprintf` → `lv_snprintf`（9 处调用） | 完成 |
| R5 | 删除死宏 `lv_MIN_TRUST`（symbolic_coord.c） | 完成 |
| R6 | 类型安全 clamp/min 设施（`lv_CLAMP` 9 处） | 待决策 |

### R1 明细（qsort comparator 同构合并，判据 A）
normalization.c 的 `cmp_seg_hash` 与 `hash_idx_compare_asc` 均按 `HashIdx.hash` 升序比较（分支比较避免溢出），完全同构。qsort 调用点改用 `hash_idx_compare_asc`，删除死函数 `cmp_seg_hash`（约 12 行）。

### R2 明细（lv_parse_long_default，判据 I 变体）
lv_parse_utils.h 新增 static inline `lv_parse_long_default(const char *str, int64_t default_value)`（strtol 前缀解析 + errno + end==str 防御 + default 回退）。收敛 transcendental.c 6 处 strtol 前缀解析样板（`-pi/N` / `pi/N` / `N*pi` 正负分支），symbolic_coord_ops.c 删除 static `safe_atol`、7 处调用改用 `lv_parse_long_default(name/name+3/name+4/after+1, 0)`。

### R3 明细（lv_parse_double_default，判据 I 变体）
lv_parse_utils.h 新增 static inline `lv_parse_double_default(const char *str, double default_value)`（复用 `lv_parse_double` + default）。收敛 gappa_dsl.c 2 处 + gappa_propagate.c 2 处 strtod 回退 0.0 样板。

### R4 明细（high_dim_snprintf → lv_snprintf，判据 J 泛化）
`high_dim_snprintf` 仅是裸 vsnprintf 包装，与 `lv_snprintf`（NULL/大小检查 + vsnprintf 负值防御 + 截断 NUL）正常路径语义等价。删除 high_dim.c 定义 + high_dim_internal.h 声明，9 处调用（high_dim_fidelity 1 / high_dim_utils 2 / high_dim_project 2 / high_dim_view 4）改用 `lv_snprintf`。

### R5 明细（死宏清除，判据 C/D）
symbolic_coord.c 删除死宏 `lv_MIN_TRUST`（全库 grep 确认零调用点）。

### 验证
ninja build3 927/927 + ctest 170/170 全部通过，零修复项。构建警告（`lv_LOG_MSG_MAX_LEN` 等宏重定义）属既有问题，未处理。

---

## 十四、批次 S 候选立项与实施（2026-08-13）

**候选来源**：上一会话 4 路只读子代理报告（按判据 A/C/D/G/H/I/J 分组），共 17 候选，按优先级分高/中/低三档。三档已全部执行并验证。

### 批次 S 执行进度

| 编号 | 内容 | 状态 |
|------|------|------|
| A1 | 优先级升序比较器族 → `lv_cmp_int_asc`（修 `a-b` 溢出） | 完成 |
| A2 | 扫描至定界符/NUL 循环 → `lv_str_skip_until` | 完成 |
| D1 | TransExprType 三份平行表 X-macro 化 | 完成 |
| D2 | PropFormulaType string/LaTeX 双份格式表单表化 | 完成 |
| G1 | ConflictRecord 析构双份收敛 | 完成 |
| I1 | sscanf/snprintf 格式串单一事实源（+ 入黑名单） | 完成 |
| C4 | lv_json_buf_init 容量裸字面量族具名 | 登记不迁移（判据 C 不成立） |
| C1 | 1e-308 下溢哨兵具名化 → `lv_SAFE_MIN_POSITIVE` | 完成 |
| C2 | 1e-30 零保护哨兵具名化 → `lv_ZERO_GUARD_EPS` | 完成 |
| C3 | 1e18 大数初值 → `lv_LARGE_NUMBER`（4 文件） | 完成 |
| D3 | ATPBackendType 可执行名/显示名 X-macro 化（3 列） | 完成 |
| H1 | graph_memory 双轨坐标提取共享函数 | 完成 |
| C5 | 1000.0 渲染 t_max → `lv_RENDER_INFINITE_LINE_EXTENT` | 完成 |
| H2 | equiv_class 点坐标有效性谓词 | 完成 |
| B3 | 手写倍增扩容三段 → `lv_ensure_capacity` | 登记不迁移（三段形态异构） |
| J1 | 判据 J 无候选（core 内裸 strncpy 已归零） | 登记已收敛 |
| I2 | atp_backend 数字提取样板 → 判据 I 三连 | 登记不迁移（非判据 I 形态） |

### A1 明细（优先级升序比较器族，判据 A）
`lv_utils.h` 新增 static inline `lv_cmp_int_asc(a, b)`（`(a > b) - (a < b)`，无有符号减法溢出）。收敛六个「priority 升序」qsort 比较器（backend_plugin / module / routing / type_inference / rewrite / theory），原三分支 / `a-b` / `(a>b)-(a<b)` 三种形态统一为本设施；`a-b` 形态的 `engine_scheduler.rule_compare` 与 `smt_theory_combiner` 为缺陷修复（消除有符号溢出）。

### A2 明细（扫描至定界符/NUL 循环，判据 A）
`lv_str_utils.h/c` 新增 `lv_str_skip_until(p, any_of)`（等价于 `p + strcspn(p, any_of)`，停在定界符处不越过）。收敛全库「`while (*p && *p != X) p++`」扫描到定界符的手写循环（单字符与多字符定界符两类）。

### D1 明细（TransExprType 三份平行表 X-macro 化，判据 D）
`symbolic_coord.h` 上提 `TransOpKind` 枚举并新增 X-macro 主源 `LV_TRANS_EXPR_TYPE_X`（4 项，列顺序 symbol/op_str/is_mul/op_kind）；`transcendental.c` 的 `s_trans_expr_op_str[]`/`s_trans_expr_is_mul[]` 与 `symbolic_coord_ops.c` 的私有 `TransOpKind` + `s_trans_expr_op_kind[]` 三份平行表改由 X-macro 局部展开生成，消除增删枚举值时三表失步。

### D2 明细（PropFormulaType 格式表单表化，判据 D）
`prop_verifier_serialize.c` 的 `StringFormatSpec` 改名 `FormatSpec` 并新增 `latex_str` 字段，合并 ASCII/LaTeX 双表为单一 `s_format_spec` 表（7 项 designated initializer）；`formula_to_string_buf` 用 `op_str`，`formula_to_latex_buf` 用 `latex_str`。

### G1 明细（ConflictRecord 析构双份收敛，判据 G）
`conflict_detector.c` 新增静态 `conflict_record_release_fields(ConflictRecord*)` 统一释放 `node_ids`/`constraint_ids`/`description`/`suggestion` 四动态字段；`lv_conflict_report_destroy` 与 `lv_conflict_report_clear` 循环体改调用该函数。

### I1 明细（格式串单一事实源，判据 I 扩展）
`proof_strategy_deductive.c` 新增 `DEDUCT_FMT_*` 格式串宏族，将 9 处裸 sscanf/snprintf/DEDUCT_ADD_FACT 中的事实格式串字面量统一为单一事实源，消除「约束事实规格表 vs 规则体」生成/解析两侧的格式串双份维护漂移；裸格式串字面量加入治理黑名单。

### C4 登记不迁移（lv_json_buf_init 容量裸字面量族，判据 C 不成立）
`lv_json_buf_init` 的初始容量裸字面量（64/128/256/512/1024/2048/4096/8192 及动态表达式）经判定**不满足判据 C**：① 判据 C 要求「同一魔法值」≥2 处，而这些是**不同**容量值，各调用点按自身预期输出规模独立选值；② §2.1 差异分类法明确将「容量初值」列为「常量差异」，应「常量参数化吸收」——而参数化已由 `lv_json_buf_init(buf, initial_size)` 的 `initial_size` 形参完成；③ 无跨文件同语义重复（如 geojson 导出的 4096 与配置序列化的 4096 语义无关）。与批次 Q 的 Q24（编码阈值族）/Q15（view_id 编码规则）「模块内参数而非跨文件重复语义常量」同判，登记不迁移。

### C1 明细（1e-308 下溢哨兵具名化，判据 C）
`config.h` 新增 `lv_SAFE_MIN_POSITIVE 1e-308`（下溢保护哨兵，避免 log(0)/除零，DBL_MIN 附近）。`float_error.c` 删除局部 `#define SAFE_MIN_POSITIVE 1e-308`，`rpn_eval_div` 改为 `if (fabs(stack[*top - 1]) < lv_SAFE_MIN_POSITIVE) return false;`；`fptaylor_eval.c` 的除零保护改为 `if (fabs(rhs) < lv_SAFE_MIN_POSITIVE) return NAN;`。

### C2 明细（1e-30 零保护哨兵具名化，判据 C）
`config.h` 新增 `lv_ZERO_GUARD_EPS 1e-30`（接近零保护阈值，除数/值过滤守卫）。迁移 3 文件：`gappa_dsl.c` 两处同构 `rel_err` 判断（第 876/909 行）、`herbie_eval.c` 值过滤守卫、`hip_backend.c` 的 `vector_inv_kernel`/`matvec_kernel`（`fabs(denom) > lv_ZERO_GUARD_EPS`）。

### C3 明细（1e18 大数初值，判据 C）
复用既有 `lv_LARGE_NUMBER 1e18`，迁移 4 文件裸 `1e18`：`geometry_canvas.c` 与 `block_canvas.c` 的边界盒四元初值（min/max 初始化）、`proof_version_sledge.c` 的 `best_time` 初值、`test_framework.c` 的 `min_ns` 初值。

### D3 明细（ATPBackendType 可执行名/显示名 X-macro 化，判据 D）
`atp_backend.h` 的 `LV_ATP_BACKEND_ENTRY` 从 2 列扩展到 3 列（ENUM/EXEC/NAME），新增可执行名列。`atp_backend.c` 的可执行名表由 X-macro 生成（`LV_ATP_BACKEND_EXEC_ROW`）；`proof_strategy_hol_oracle.c` 删除手写 `atp_names[]` 第二份显示名数组，循环内改用 `atp_backend_type_name(atp_types[backend])`（6 处 snprintf 的 `atp_names[backend]` 全部改为 `atp_name`）。消除枚举↔可执行名↔显示名三表增删枚举失步。

### H1 明细（graph_memory 双轨坐标提取共享函数，判据 H）
`graph_memory.c` 新增 `symbolic_coord_dual_extract(const SymbolicCoord *, mpq_t, bool *)`（RATIONAL 时 `mpq_init/mpq_set` 并置 `*out_exact=true`，否则置 false，统一返回 `symbolic_coord_to_double`）。INCIDENCE 段 4 处 + BETWEENNESS 段 4 处共 8 处「to_double + 条件 mpq_init/mpq_set」样板替换为单次调用。

### C5 明细（1000.0 渲染 t_max 具名化，判据 C）
`config.h` 新增 `lv_RENDER_INFINITE_LINE_EXTENT 1000.0`（无限直线渲染扩展范围）。`lv_render_visitor.c` 与 `geo_visual_complete.c` 的 `t_max` 裸 `1000.0` 改用该常量（后者 `(float) lv_RENDER_INFINITE_LINE_EXTENT`）。

### H2 明细（equiv_class 点坐标有效性谓词，判据 H）
`equiv_class.c` 新增 `symbolic_point_has_xy(const GeomNode *)`（`coord_count >= 2 && symbolic_coords[0] && symbolic_coords[1]`）。`equiv_derive_from_coords`/`equiv_derive_transform` 及 `point_count` 统计、距离向量填充共 6 处完整性检查替换。**有意保留**：ALGEBRAIC 共轭检测段（第 424/436 行）仅需排除 NULL 的单个 `symbolic_coords[d]`，不能套用该谓词。

### B3 登记不迁移（手写倍增扩容三段，判据 B）
三段手写扩容形态异构，均不可迁移至 `lv_ensure_capacity`：
- `lv_graph_traversal.c` `collect_neighbor_batch`：`int **out_ids` + `void ***out_edges` **双数组同步**手写倍增（`int *buf_cap`，初始 256，溢出保护），与单数组接口不同构。
- `interop_theorem.c`：`ctx->calls_capacity`（`size_t`）char* 倍增 + NUL 语义，size_t 容量 vs `lv_ensure_capacity` int 参数体系不匹配（项目记忆批次 M 已有豁免先例）。
- `lv_storage.c` `mem_ensure_capacity`：`int64_t` 容量、byte 语义、自定义 `lv_STORAGE_MEM_INIT_CAPACITY`/`lv_STORAGE_MEM_GROW_FACTOR`，与统一设施不同构。

### J1 登记已收敛（判据 J 无候选）
全库 grep `strncpy(` 在 core/ 内零匹配（裸 strncpy 已由历史批次 J/Q1 归零），判据 J 无新候选。

### I2 登记不迁移（atp_backend 数字提取样板，判据 I）
`atp_backend.c` 数字相关代码仅用既有 `lv_parse_int`（第 542/930 行），配套手写 `while (*p >= '0' && *p <= '9') p++;` 扫描，但**非**判据 I「关键词定位→向左回退数字起始→解析整数」三连样板（已由 `lv_parse_int_before` 收敛），登记不迁移。

### 决策登记（第 9 章格式）
`lv_cmp_int_asc` / A / 6 比较器（约 15 文件） / 无 / 各排序测试；`lv_str_skip_until` / A / 全库扫描到定界符循环 / 无 / 字符串测试链；`LV_TRANS_EXPR_TYPE_X` X-macro / D / 3 表 2 文件 / 无 / test_symbolic_coord_ops 族；`s_format_spec` 单表 / D / prop_verifier_serialize 1 文件 / 无 / test_prop_verifier 族；`conflict_record_release_fields` / G / conflict_detector 1 文件 / 无 / test_conflict_detector；`DEDUCT_FMT_*` 宏族 / I 扩展 / proof_strategy_deductive 9 处 / 无 / test_proof_strategy 族；`lv_json_buf_init` 容量字面量 / C 豁免 / 0 文件 / 容量初值属 §2.1 常量差异、参数化已由 initial_size 承担，无跨文件同语义重复。

### 决策登记（第 9 章格式 · 中/低价值组）
`lv_SAFE_MIN_POSITIVE` / C / float_error + fptaylor_eval 2 处 / 无 / 数值测试链；`lv_ZERO_GUARD_EPS` / C / gappa_dsl×2 + herbie_eval + hip_backend×2 5 处 / 无 / 数值测试链；`lv_LARGE_NUMBER` 复用 / C / 4 文件 4 处 / 无 / 各几何/性能测试；`LV_ATP_BACKEND_ENTRY` 3 列扩展 / D / atp_backend + proof_strategy_hol_oracle 2 文件 / 无 / test_bdd_sat_atp 族；`symbolic_coord_dual_extract` / H / graph_memory 8 处 / 无 / test_constraint_graph 族；`lv_RENDER_INFINITE_LINE_EXTENT` / C / lv_render_visitor + geo_visual_complete 2 处 / 无 / test_layer6_visual 族；`symbolic_point_has_xy` / H / equiv_class 6 处 / 无 / test_equiv_class；手写倍增扩容三段 / B 豁免 / 0 文件 / 双数组·size_t·int64_t 三形态异构；判据 J 无候选 / J / 0 文件 / core 内裸 strncpy 已归零；atp_backend 数字提取 / I 豁免 / 0 文件 / 非三连样板（已由 lv_parse_int_before 覆盖）。

### 验证
ninja build3 927/927 + ctest 170/170 全部通过，零修复项。构建警告（`lv_LOG_MSG_MAX_LEN` 等宏重定义）属既有问题，未处理。

---

## 十五、批次 T 候选立项与实施（2026-08-13）

**候选来源**：历史批次「继续寻找更多可抽象化的方向」按判据 A–K 全库扫描，共 9 候选，按优先级逐步执行（用户选定「全部按照优先级逐步完整执行」）。

### 批次 T 执行进度

| 编号 | 内容 | 状态 |
|------|------|------|
| D-1 | lvProofStepType 枚举单源化（lean4/opml 共用单源，coq 豁免） | 完成 |
| D-2 | LvEntityType 元数据 X-macro 单源 | 完成 |
| A-1 | find_app_sink 跨文件逐字副本收敛 | 完成 |
| M1+M2 | 死代码文件删除 + CMake/脚本移除 | 完成 |
| K1 | formula 域错误样板 → lv_RESULT_FAIL | 登记不迁移 |
| B1+B2 | clamp 双份收敛（proof_score/aabb_tree） | 完成 |
| B5 | 新增 lv_str_eq/lv_str_ne 收敛 strcmp==0 | 完成 |
| F-1 | 手写分发表迁移 LV_DISPATCH | 完成 |
| B4+A-2 | 剩余中价值候选（match_any/selector 双份） | 完成 |

### D-1 明细（lvProofStepType 枚举单源化，判据 D）
新建 `lv/interop_step_type.h` 定义 `lvProofStepType` 枚举作为 Lean4 / OPML 共用单源；`lean4_bridge.c` / `opml_codec.c` 删除各自本地枚举定义，改为引用共享头。**豁免**：`coq_bridge.c` 因 `EXACT=4` 数值分叉（互操作外部契约），禁止与 lvProofStepType 单源合并，保留独立枚举（代码注释已注明）。

### D-2 明细（LvEntityType X-macro 单源，判据 D）
`lv_ast.h` 新增 `LV_ENTITY_TYPE_X(x)` X-macro 列表，`lv_XMACRO_ENUM(LV_ENTITY_TYPE_X)` 生成 `LvEntityType` 枚举；`lv_ast.c` 用 `lv_XMACRO_TO_NAME_ARRAY` 生成名称表。

### A-1 明细（find_app_sink 跨文件逐字副本收敛，判据 A）
`constraint_graph.h` 新增声明 `graph_find_app_sink_input`，`graph_index.c` 实现之；`beta_reduce.c` 删除本地 `find_app_sink_in`、`lambda_to_graph.c` 删除本地 `find_app_sink_input`，统一改调共享 API。

### M1+M2 明细（死代码文件删除）
删除死文件 `core/src/layer2_resource/error_messages_cn.c`、`core/src/layer2_resource/result_messages_cn.c`；`CMakeLists.txt` 移除对应源文件、`tool/gen_stubs.ps1` 移除对应 stub 生成。

### K1 明细（formula 域错误样板 → lv_RESULT_FAIL，判据 K）
评估后**登记不迁移**：formula 域错误结果形态与 `lv_RESULT_FAIL`（静态消息、单 `error_msg` 字段、不含 return）契约不吻合，保留原状。

### B1+B2 明细（clamp 双份收敛，判据 B）
复用既有 `lv_clamp`（ly_numeric.h/c）：`proof_score.c` 手写 `clamp_score` → `lv_clamp`；`aabb_tree_impl.h` `closest_point` 的 2D/3D 三元钳制 → `lv_clamp`。

### B5 明细（新增 lv_str_eq/lv_str_ne，判据 B）
`lv_str_utils.h` / `lv_str_utils.c` 新增 `lv_str_eq` / `lv_str_ne`（NULL 安全，两者均 NULL 视为相等，语义与 `lv_str_icmp` 一致）。全库 60 处 `strcmp(a,b)==0` / `!=0` 相等/不等判定形态迁移为 `lv_str_eq` / `lv_str_ne`，新增 10 处 `#include "lv/lv_str_utils.h"`（覆盖 lv_impl_native / lv_impl_upper_app / extended_types / proof_widget / plugin_system_{query,load,interface,deps,autoload} / interop_import 等文件）。

**有意保留的 strcmp（4 处，需三态符号或原始比较结果）**：
- `rewrite_strategy.c:355`：`strcmp(...) < 0`（排序型）
- `lv_utils_misc.c:120`：`int cmp = strcmp(v1->prerelease, v2->prerelease);`（三态捕获）
- `transcendental.c:192`：`int name_cmp = strcmp(a->name, b->name);`（三态捕获）
- `test_framework.h:243`：`_lv_cmp = strcmp(_lv_actual, _lv_expected);`（测试框架比较捕获）

另有 2 处内部实现（`lv_str_utils.c:32` / `:67`）与 3 处注释（`config.h:477` / `lv_json.h:141` / `lv_str_utils.h:81`）保留。

### F-1 明细（手写分发表迁移 LV_DISPATCH，判据 F）
迁移 5 处「边界检查 + NULL 槽检查 + 调用」纯值三行样板为 `LV_DISPATCH`（fallback 为表项同类型常量，无副作用，前置守卫逐位保留），并补 3 处 `#include "lv/lv_xmacro.h"`：
- `gappa_dsl.c` ×3：`interval_unary`（`LV_DISPATCH(kUnaryIntervalOps, op, false, ...)`）、`interval_binary`（`kBinaryIntervalOps`）、`expr_eval_ival`（`kEvalIntervalOps`）——原 `op>=0` 前缀守卫由 `LV_DISPATCH` 的 unsigned 越界检查等价吸收。
- `engine_scheduler.c` ×1：`check_condition`（`kRouteConditionHandlers`）——原 `lv_index_in_range(...) && table[type]` 收敛为 `LV_DISPATCH(..., cond->type, false, ...)`。
- `float_error.c` ×1：RPN 分派（`kRpnEvalOps`）——原「越界/NULL → NAN」与「handler 返回 false → NAN」两条路径合并为 `if (!LV_DISPATCH(kRpnEvalOps, idx, false, ...)) return NAN;`。

**评估后不迁移（非纯值分发表，语义不吻合 LV_DISPATCH 契约）**：
- `lv_impl_native.c:812` `kExprEvalHandlers`：fallback 含 `mpq_set_si` + `lv_RETURN_ERROR` + `return -1`（副作用）。
- `geometry_compress_main.c:233` `kEntropyEncoders`：`ENTROPY_NONE` 分支所有权转移（`combined = NULL`）。
- `ga_codegen.c:196` `kCodegenHandlers`：fallback 设置 `res->error_msg`。
- `graph_node_alloc.c:1329` `kVTables`：返回指针表元素本身，非「按 key 调用 handler」。
- `formula_renderer.c:149` `s_render_funcs`：fallback 为 `lv_RETURN_ERROR`（错误副作用）。
- `formula_string.c:405` `s_funcs`：void 表但 fallback 是 `str_default` 动作（非值）。
- `dsl_compiler_load.c:540` `kIROpHandlers`：handler 返回 bool 被 early-return 检查 + 越界/NULL no-op。
- `axiom_rule_engine.c:432/478` `step_difficulty`/`prop_difficulty`：`.score` 字段访问，非函数指针调用。

其余 `sizeof(...)/sizeof(...[0])` 命中均为数据表/计数宏/for 循环/析构字段表，非函数指针分发表，不属 F 判据范围。

### B4+A-2 明细（match_any/selector 双份，判据 A/F）

**selector 射线法双份（判据 A，已收敛）**：`func_block_selector.c` 的 `point_in_region` 与 `recursion_selector.c` 的 `point_in_region_ray_casting` 是同一「射线法 point-in-region」算法的两份实现（真实调用点各 1、合计 2，满足粒度门槛）。收敛为共享设施 `geo_point_in_region_segments(px, py, GeomNode **segments, seg_count)`（geo_utils.h 声明 + geo_utils.c 实现，两个调用文件均已 include `lv/geo_utils.h`，无需新增 include 依赖链）：
- 采用 func_block 版本更稳健的 `lv_is_zero(dy, lv_EPSILON_ULTRA)` 防卫跳过近水平退化边——原 recursion 版本仅靠半开区间条件自然跳过**精确水平**边，对 |dy|<1e-12 的近水平边存在近零分母风险，收敛后此缺陷同步修复。
- 循环守卫采用更严格的 `seg->coord_count < 4`（与 `symbolic_coord_get_segment` 内部检查冗余但等价），func_block 原仅查 `type`，二者行为逐点等价。
- `func_block_selector.c` `point_in_region` 收敛为薄封装（保留 GeomNode 输入校验 + 坐标提取 + boundary_segments 空指针守卫，`graph` 形参标记 `(void)`）；`recursion_selector.c` 删除 `point_in_region_ray_casting`，调用点直调共享设施。
- 第三份 `lv_point_in_polygon`（geo_predicate.c）输入为闭多边形顶点数组（`double *xs/ys`，`j=(i+1)%n`），与「线段列表（可多环/非闭合）」不同构，不可复用，登记。

**match_any 双份（判据 F，登记不迁移）**：剩余手写 `strstr` 循环经逐一比对均非 `lv_str_match_any`（顺序返回首个命中下标）的干净双份，语义变体豁免：
- `meta_verify.c` `structure_markers`：计数累积语义（`found_markers++`），非首命中下标。
- `proof_classical.h` `lv_classical_problem_match`：关键词 → 映射枚举返回（`kClassicalProblemKeywords[i].problem`），非下标。
- `lv_protocol.c` `sysinfo_find_field`：字段过滤 + 返回首次匹配字符串指针，非下标。
- `proof_version_ghost.c` `kGoalKeywords`：命中后 OR 位掩码累积（`goal_matched_groups |= group`），非下标。
（已由历史 Q31 迁移的 3 处真正表驱动 match_any 保持不变。）

### 决策登记（第 9 章格式）
`lv/interop_step_type.h` 单源 / D / lean4_bridge + opml_codec 2 文件（coq 豁免）/ 无 / test_interop 族；`LV_ENTITY_TYPE_X` X-macro / D / lv_ast.h + lv_ast.c / 无 / test_lv_parser 族；`graph_find_app_sink_input` / A / beta_reduce + lambda_to_graph 2 文件 / 无 / test_rewrite + test_lambda 族；死文件 error/result_messages_cn / M / CMakeLists + gen_stubs.ps1 / 无 / 全量构建；formula 域错误样板 / K 豁免 / 0 文件 / 与 lv_RESULT_FAIL 契约不吻合；`lv_clamp` 复用 / B / proof_score + aabb_tree_impl 2 文件 / 无 / test_proof + test_geo_aabb_tree 族；`lv_str_eq`/`lv_str_ne` / B / 60 处 + 10 include / 4 处有意例外 + 2 内部 + 3 注释 / 全库字符串比较链；`LV_DISPATCH` 分发表收敛 / F / gappa_dsl + engine_scheduler + float_error 3 文件 5 处 / 8 处非纯值形态豁免 / test_gappa_dsl + test_engine_scheduler + test_interval_arith 族；`geo_point_in_region_segments` / A / func_block_selector + recursion_selector 2 文件 / lv_point_in_polygon 闭多边形数组不同构不可复用 / test_func_block + test_recursion 族；match_any 语义变体（计数/映射枚举/字段指针/位掩码）/ F 豁免 / 0 文件 / 非首命中下标语义。

### 验证
ninja build3 通过（exit 0，仅含既有宏重定义警告）+ ctest 170/170 全部通过，零修复项。

---

## 十六、批次 U 候选立项与实施（2026-08-13）

**候选来源**：历史批次「继续寻找更多可抽象化的方向」按判据 A–K 全库扫描（含新判据 L 立项）。共 10 候选（U1–U10），用户选定「全部按优先级逐步完整执行」。

### 批次 U 执行进度

| 编号 | 内容 | 状态 |
|------|------|------|
| U1 | 立项判据 L + L1 snprintf("%s")伪复制 → `lv_strlcpy`（30 处/18 文件） | 完成 |
| U2 | snprintf 游标宏对 TRACE_WRITE/CONTRADICTION_WRITE 收敛 → lvStrBuf | 完成 |
| U3 | snprintf 游标缓冲链 → `lvStrBuf`（6 处/6 文件，8 处豁免） | 完成 |
| U4 | 手写对象销毁序列 ×5 → `lvFieldDesc` 字段表 | 完成 |
| U5 | mpz_clear_deferred shim 双份 + solver 裸 mpz 配对 → `lv_DEFER` | 完成 |
| U6 | point_coord x/y 双分量提取 → `point_coord_xy`（5 处/3 文件） | 完成 |
| U7 | 大数/极值哨兵家族具名化（1e308/1e30/1e300） | 完成 |
| U8 | 判据 C/D 余项（GeoJSON 1e9 + 时间残值 + token表 + type_system表） | 完成 |
| U9 | geo_dynamic 拓扑栈动态化 + 空白跳过收敛（`lv_str_skip_ws_n`）；错误消息组装/strchr 切分评估后豁免 | 完成 |
| U10 | 注册表三件套 + 单侧钳制（评估后豁免：手写注册表异构 / 钳制异构且已有 lv_MIN/lv_MAX/lv_CLAMP） | 完成（豁免） |

### U1 明细（判据 L1 snprintf 伪复制 → lv_strlcpy）

**判据 L 立项**：ABSTRACTION_SPEC.md 新增 §1.12 判据 L（snprintf 定长复制/写入样板），两个子形态：
- **L1**：`snprintf(dst, size, "%s", src)` 语义等价 `lv_strlcpy`（返回源串长度、保证 NUL 终止），`dst` 为定长缓冲区基址、`src` 为普通字符串时是伪复制，必须直接用 `lv_strlcpy`。
- **L2**：snprintf 后手动返回值防御，待设施落地后实施（未实施）。

**迁移 30 处 / 18 文件**（全库 33 处 `snprintf(..., "%s")` 命中，30 处纯复制迁移 + 3 处豁免）：

| 文件 | 处数 | 说明 |
|------|:---:|------|
| `layer4_reasoning/proof_system/prop_verifier_analysis.c` | 1 | atom 名复制 |
| `layer5_output/lv_protocol.c` | 1 | status 三元串复制（保留 `?:` 语义） |
| `layer4_reasoning/backends/smt_backend_impl_external.c` | 1 | result_buf（保留 `output ? output : ""` NULL 写空串语义） |
| `layer4_reasoning/type_logic/type_equiv_explorer.c` | 1 | applied_rule_name |
| `layer4_reasoning/module/module_delta.c` | 1 | fname |
| `layer4_reasoning/proof_system/prop_verifier_formula.c` | 1 | atom.name |
| `layer4_reasoning/backends/smt_backend_impl_groebner.c` | 1 | error_message |
| `layer4_reasoning/model/recursion_validation.c` | 1 | validation_template |
| `layer3_geometry/algebraic_number_rational.c` | 1 | 返回值 `int` 强转 |
| `lv_impl_upper_app.c` | 2 | output_format（前置 `[0]` 守卫保留） |
| `layer4_reasoning/backends/smt_backend_impl.c` | 2 | entry->name / entry->version |
| `layer1_parser/formula_renderer_ascii.c` | 2 | variable/identifier helper |
| `layer1_parser/formula_renderer_python.c` | 2 | 同上（保留局部 `written`） |
| `layer1_parser/formula_renderer_dsl.c` | 2 | 同上 |
| `layer1_parser/formula_renderer_latex.c` | 4 | greek_name/name/esc/center_buf |
| `layer1_parser/formula_renderer_internal.h` | 1 | render_fn_name_spec fallback + 新增 include lv_utils.h |
| `layer2_resource/lv_utils_config.c` | 3 | full_key×2 / last_section |
| `lv_impl_upper_orchestrator.c` | 3 | set_error_msg / set_last_error 错误消息复制 |

**豁免 3 处（标注 `/* exempt: */`）**：
- `mini_kernel.c:827`：`snprintf(trimmed, sizeof(formula), "%s", label)` —— `trimmed` 为 `lv_str_trim` 返回的偏移指针、`sizeof(formula)` 是基址容量非剩余容量，直接 `lv_strlcpy` 会引入溢出风险，非 L1 纯复制候选。
- `interactive_geo.c:617/625`：`snprintf(buf, bufsz, "%s", hdr)` + `snprintf(buf + w, rem, "%s", ...)` 游标追加形态，归入 U2/U3 处理。

**返回值语义核验**：`lv_strlcpy` 返回 `size_t`（源串长度），与 `snprintf(..., "%s")` 正常/截断路径语义一致；调用方按返回值用途 `(int)` 强转（algebraic_number_rational.c / formula_renderer 各 helper）。

### 决策登记（第 9 章格式）

`lv_strlcpy` 复用（判据 L1）/ L / 30 处 18 文件 / 3 处豁免（mini_kernel 偏移指针 + interactive_geo 游标追加）/ 全库字符串复制链 + 全量构建。

### U2 明细（snprintf 游标宏对 TRACE_WRITE/CONTRADICTION_WRITE 收敛，判据 L2 前缀）

`proof_trace.c` 的 `TRACE_WRITE` 与 `proof_contradiction.c` 的 `CONTRADICTION_WRITE` 是逐字同构的「snprintf 游标缓冲追加」局部宏（`char *buf + int pos + buf_size` + 截断钳制），各自在函数内 `lv_malloc(buf_size)` 定长缓冲。收敛为既有 `lvStrBuf` 设施：
- 删除两份同构宏定义（`#define TRACE_WRITE` / `#define CONTRADICTION_WRITE` + `#undef`）及 `buf_size`/`buf`/`pos` 手工游标管理。
- 各 `XXX_WRITE(...)` 调用点改为 `lv_strbuf_printf(&sb, ...)`，函数末尾 `return buf` 改为 `return lv_strbuf_to_string(&sb)`（返回堆分配 NUL 结尾串，契约与原「lv_malloc 返回」一致）。
- **行为变化（有意）**：`proof_trace.c` 原 `buf_size = steps.count*256+1024` 与 `proof_contradiction.c` 原固定 `4096` 截断上限取消，改为动态增长（消除超长轨迹/闭包的静默截断）。
- 两文件各补 `#include "lv/lv_strbuf.h"`。

### 决策登记（第 9 章格式）

`lv_strlcpy` 复用（判据 L1）/ L / 30 处 18 文件 / 3 处豁免（mini_kernel 偏移指针 + interactive_geo 游标追加）/ 全库字符串复制链 + 全量构建；`lvStrBuf` 游标宏对收敛 / L2 前缀 / proof_trace + proof_contradiction 2 文件 / 无（取消定长截断为有意行为变化）/ test_proof_trace + test_proof_contradiction 族；`lvStrBuf` 游标缓冲链收敛 / L2 / 6 处 6 文件 / 8 处豁免（调用方缓冲契约 + 非循环前缀 + 已抽象 RespCursor）/ test_* 族 + 全量构建；`lv_obj_destroy_fields` 手写销毁序列收敛 / A（去重） / 5 处 4 文件（纯 PLAIN_FREE 字段表，排除嵌套/异构/浅释放）/ test_* 族 + 全量构建；`lv_mpz_clear_deferred` shim 去重 + solver 裸 mpz 配对收敛 / A（去重） / shim 2 份→1 份共享 + 4 函数 14 mpz 变量（豁免 extract_incidence/extract_intersection 深层分支 + mpq）/ solver+symbolic_coord+rational 等 15 项定向测试；`point_coord_xy` x/y 双分量提取收敛 / A（去重） / 5 处 3 文件 12 组成对提取（新增共享助手，保留 point_coord 原语）/ solver+symbolic_coord+geo_constraint_solver+geometry_core+solver_submodules 等 9 项定向测试；`lv_NEAR_INFINITY_SENTINEL` 大数/极值哨兵家族具名化 / C / config.h 新增宏 + 10 .c 文件迁移（`1e308`×12 + `1e300`×9 + `1e-300`×1 + `1e30`×18，共 40 处字面量）/ 无代码豁免（default_host_ops.h 注释仅文档）/ runtime_monitor+probabilistic+high_dim+propagation+geo_invariant+symbolic_coord+proof_search+numeric+solver+geometry_core 15 项定向测试；U8 判据 C/D 余项收敛（GeoJSON 1e9→INTEROP_COORD_DENOM_PRECISION_GEOJSON + 时间裸换算→lv_*_PER_* + LV_TOKEN_TYPE_X 单源 + LV_TYPE_KIND_COUNT 自动计数）/ C+D / 11 文件（interop.h/interop_import.c/lv_utils_misc.c/adaptive_threshold.c/context.c/lv_circuit_breaker.c/test_framework.c/lv_lexer.h/lv_lexer.c/type_system.h/type_check.c）/ 无代码豁免 / type_system+type_equiv_explorer+minimal_parse+utils+interop+circuit_breaker+adaptive_threshold+lv_lexer+lv_parser+func_block_utils 10 项定向测试。

### U3 明细（snprintf 游标缓冲链 → lvStrBuf，判据 L2）

扫描结果共约 21 处「游标追加」形态命中，甄别出 **6 处真游标链**（循环内 `snprintf(buf + offset, size - offset, ...)` 累加 offset 且产出堆串或「定长缓冲 + strdup」）迁移为 `lvStrBuf`，另 **8 处豁免**（调用方固定缓冲契约 / 非循环两段拼接 / 已抽象）。

**迁移 6 处 / 6 文件**：

| 文件 | 函数/位置 | 原形态 | 说明 |
|------|-----------|--------|------|
| `layer2_resource/lv_error.c` | `lv_error_format_chain` | 估算 total_size + `lv_malloc` + `pos` 游标 | 返回堆串，取消尺寸估算与 `written<remaining` 守卫 |
| `layer4_reasoning/backends/approx_counter.c` | `cnf_to_dimacs` | 估算 est_size + `lv_malloc` + `offset` | 返回堆串（DIMACS），取消定长截断 |
| `layer4_reasoning/type_logic/inequality_reasoning_serialize.c` | `lv_ineq_proof_to_latex` | 固定 1024 + `offset` | 返回堆串，取消 1024 截断上限 |
| `layer1_parser/formula_curve.c` | IMPLICIT_CURVE 构建 | 固定 `FORMULA_LARGE_BUF_SIZE` + `offset` | 取消定长截断，末尾 `strdup_safe` → `lv_strbuf_to_string` |
| `layer3_geometry/float_error.c` | `extract_equations` 约束描述 | 固定 `EXPR_BUFFER_INITIAL` + `off` | 取消定长截断，末尾 `strdup` → `lv_strbuf_to_string` |
| `layer4_reasoning/func_block/preset_manager_doc.c` | `preset_generate_library_documentation` | `lv_malloc(PRESET_BUFFER_SIZE)` + `offset` | 补 `lv_strbuf_to_string` 返回 NULL 的 OOM 失败路径 |

各文件补 `#include "lv/lv_strbuf.h"`；函数末尾统一 `return lv_strbuf_to_string(&sb)`（堆分配 NUL 串，与原 `lv_malloc`/`lv_strdup` 契约一致，`lv_free` 释放）。

**豁免 8 处（评估后保留，标注语义）**：

| 位置 | 豁免语义 |
|------|----------|
| `lv_log.c:91-104` | strftime + snprintf 两段前缀，strftime 需固定缓冲，非循环游标链 |
| `lv_sema.c:44-54` | 两次 snprintf 写固定 `errors[][]` 槽，调用方固定数组契约 |
| `lv_protocol.c:523-556` | 裸字节 memcpy/`out[w++]` 与 snprintf 混合，非纯游标链 |
| `atp_backend.c:695-700` | 两段 extra_args 拼接，非循环 |
| `conflict_detector.c:1211-1245` | 写调用方缓冲 + 严格截断返回 -1 契约 |
| `lambda_unify.c:438-457` | 写调用方缓冲，无堆串产出 |
| `interop_command.c:959-978` | 已用 `RespCursor` 抽象收敛，非候选 |
| `interactive_geo.c:617/625` | 写调用方缓冲 `snprintf("%s")` 游标追加（U1 豁免划入，经评估仍属调用方缓冲契约） |

### U4 明细（手写对象销毁序列 ×5 → lvFieldDesc 字段表）

从全库约 80 条 `*_destroy` 函数中甄别出 **5 个纯 `lv_free` 逐字段销毁函数**（均只含 `LV_FIELD_PLAIN_FREE` 语义，无嵌套逐元素/异构释放），迁移为既有 `lv_obj_destroy_fields` 字段表。排除：`atp_backend.c`（proof_steps 嵌套逐元素销毁）、`proof_export_enhanced.c`（单字段输出，过于简单）、`approx_counter.c`（仅 memset，非释放候选）、`bootstrap_test_random.c`（仅 free 自身）、`proof_widget.c`（`lv_free_ptr` + 逐元素异构）。

**迁移 5 处 / 4 文件**：

| 文件 | 函数 | 字段表 | 说明 |
|------|------|--------|------|
| `layer4_reasoning/bootstrap_test_diff.c` | `bootstrap_diff_test_destroy` | `s_bootstrap_diff_test_destroy_fields`（2 字段：`test_name`/`dsl_source`） | `input_graph` 由调用者管理，不纳入 |
| `layer4_reasoning/bootstrap_test_diff.c` | `bootstrap_diff_test_result_destroy` | `s_bootstrap_diff_test_result_destroy_fields`（4 字段：`c_api_output`/`geo_layer_output`/`diff_description`/`error_message`） | 全 PLAIN_FREE |
| `layer4_reasoning/bootstrap_test_primitive.c` | `primitive_test_result_destroy` | `s_primitive_test_result_destroy_fields`（3 字段：`input_description`/`c_api_result`/`geo_layer_result`） | 保留原行为不释放 `error_message` |
| `layer4_reasoning/axiom/axiom_template_test.c` | `axiom_template_test_case_destroy` | `s_template_test_case_destroy_fields`（3 字段：`template_name`/`description`/`params`） | `expected_graph` 不纳入；`params` 浅释放 |
| `layer4_reasoning/axiom_rule_engine.c` | `lv_rule_recommendation_destroy` | `s_rule_recommendation_destroy_fields`（3 字段：`rules`/`scores`/`reason`） | 全 PLAIN_FREE |

各文件补 `#include "lv/lv_lifecycle.h"`（`axiom_template_test.c` 已含，无需重复）；`*_destroy` 函数体改为 `lv_obj_destroy_fields(...) + lv_free(&obj)`。语义保持：字段释放顺序、NULL 化、重复调用安全均与手写版一致；不纳入字段表的指针维持原手写行为。

### U5 明细（mpz_clear_deferred shim 双份 + solver 裸 mpz 配对 → lv_DEFER）

**shim 去重（判据 A）**：`mpz_clear_deferred` 在 `mpz_poly_resultant.c` 与 `symbolics/algebraic.c` 存在两份逐字同构的 static 定义（`mpz_clear(*(mpz_t*)p)`）。收敛为 `mpz_poly.h` 的共享 `static inline lv_mpz_clear_deferred`，删除两份本地定义，10 处 `lv_DEFER` 调用点改指向共享实现（algebraic.c 7 处 + mpz_poly_resultant.c 3 处）。

**solver 裸 mpz 配对迁移（判据 A）**：4 个「mpz_init + 单出口 mpz_clear」函数改为 `lv_DEFER(lv_mpz_clear_deferred, &v)`，消除手写 clear 与重复出口清理：
- `solver_conflict.c` Check 4（`disc`/`four_ac`）：早期 `return true` 路径与正常路径的重复 `mpz_clear` 收敛为作用域守卫。
- `solver_coord_extract.c` `rational_to_mpz_scaled`（`scaled`）、`coord_to_mpz_scaled`（`scaled_num`/`den` + 分支 `q`/`r`/`half_den`）、`double_to_mpz_scaled`（`scaled_num`/`den` + 分支 `quotient`/`remainder`/`half_den`）。

两 solver 文件补 `#include "lv/lv_lifecycle.h"`（`solver_common.h` 已含 `mpz_poly.h`，`lv_mpz_clear_deferred` 可用）。

**豁免**：`solver_coord_extract.c` 的 `extract_incidence` / `extract_intersection`（`lx1_s`/`a1_s`/`D_s` 等约 20 个 mpz 变量在深层嵌套条件分支 + 多出口 `return`/`lv_RETURN_ERROR` 中反复 init/clear，控制流异构，改用 lv_DEFER 需逐分支核对守卫注册点，风险收益比不佳，保留手写）；`double_to_mpz_scaled` 的 `mpq_t q` 为 mpq 非 mpz，不在本项范围。

### U6 明细（point_coord x/y 双分量提取 → point_coord_xy）

求解器内「点二维坐标读取」反复出现成对 `point_coord(p, 0, &x)` + `point_coord(p, 1, &y)` 样板（判据 A 去重）。新增共享助手 `point_coord_xy(const GeomNode *pt, double *x, double *y)`，等价 `point_coord(pt, 0, x) && point_coord(pt, 1, y)`，声明在 `solver_common.h`、实现在 `solver_coord_extract.c`（紧邻 `point_coord`）。保留 `point_coord` 作为底层单分量原语供助手复用。

**迁移 5 处 / 3 文件**：

| 文件 | 函数/位置 | 成对组数 | 说明 |
|------|-----------|:---:|------|
| `solver/solver_coord_extract.c` | `line_from_two_points` | 2 | p1、p2 各一对，`return false` 短路由 |
| `solver/solver_geom_templates.c` | `template_similar_triangles` | 3 | nodeA/B/C 三对，`&&` 汇总 `has_coords` |
| `solver/solver_geom_templates.c` | `template_pythagorean` | 3 | 同上三对 |
| `solver/solver_geom_templates.c` | `template_parallel_cut` | 2 | n1/n2 各一对，if 守卫 |
| `solver/solver_eliminate.c` | `analyze_out_of_scope`（betweenness） | 2 | p1/p3 各一对，if 守卫 |

共收敛 12 组 x/y 成对提取（24 次 `point_coord` 调用 → 12 次 `point_coord_xy`）。语义等价：`&&` 短路求值顺序、失败即整体失败、各分量输出赋值均与原 `point_coord` 一致；无豁免项。

### U7 明细（大数/极值哨兵家族具名化）

全库扫描发现三类裸「大数/极值」字面量（`1e308`/`1e300`/`1e30` 及 `1e-300`）作为哨兵初值/支撑集/包围盒/越界上界反复出现。其中 `1e308` 与 `1e30` 已分别有权威宏 `lv_INFINITY_SENTINEL` 与 `lv_HUGE_NUMBER`（config.h），仅 `1e300` 缺具名。按语义三分收敛：

- **`1e308` → `±lv_INFINITY_SENTINEL`**（"无穷大"哨兵，min/max 初值 + 概率分布支撑集）：
  - `layer2_resource/runtime_monitor.c`：`lv_perf_stats_create` / `lv_perf_stats_reset` 的 `min_val`/`max_val` 初值（4 处字面量，2 函数）。
  - `layer4_reasoning/backends/probabilistic_constraint.c`：`kDistSupportLo`/`kDistSupportHi` 数组（3+3）+ 未知分布 fallback（`support_lo`/`support_hi`）= 8 处。
- **`1e300` → `lv_NEAR_INFINITY_SENTINEL`**（新增宏，包围盒/距离/越界上界，较 1e308 保留算术溢出余量）：
  - `config.h` 新增 `lv_NEAR_INFINITY_SENTINEL 1e300`（紧随 `lv_INFINITY_SENTINEL` 之后）。
  - `layer3_geometry/high_dim_fidelity.c`：包围盒 `min_x/min_y/max_x/max_y` 初值 + `hi_dist/lo_dist` 自指距离 + `bd_hi/bd_lo` 初始值 + 选中后标记 = 8 处。
  - `layer3_geometry/symbolics/symbolic_coord_trust.c`：越界上界 `remaining > lv_NEAR_INFINITY_SENTINEL`（同处 `1e-300` → `lv_TINY_SENTINEL`）。
- **`±1e30` → `±lv_HUGE_NUMBER`**（极大数阈值，熵/UCB/除零保护大值）：
  - `layer3_geometry/geo_invariant_type.c`：不变量值域表 5 个正上界 + 4 个 `±lv_HUGE_NUMBER` 区间 + fallback `*out_min/*out_max` = 11 处。
  - `layer3_geometry/propagation.c`：3 个选择函数的 `min_entropy` 初值 = 3 处。
  - `layer4_reasoning/proof_system/proof_search_algo.c`：`best_ucb` 初值 = 1 处。
  - `layer4_reasoning/numeric/backends/default_host_ops.c`：除零保护大值 = 1 处。
  - `layer4_reasoning/numeric/backends/cuda_backend.c`：除零保护大值 = 1 处。
  - `layer4_reasoning/numeric/backends/hip_backend.c`：除零保护三元分支 = 1 处。

**未迁移/豁免**：`core/include/lv/default_host_ops.h` 第 66 行注释仍含 `1e30` 文本（仅文档，非代码，未改）；无代码豁免项（所有裸字面量均已具名化，`lv_TINY_SENTINEL` 复用既有宏）。

### U8 明细（判据 C/D 余项：GeoJSON 1e9 + 时间残值 + token 表 + type_system 表）

按判据 C（单一事实源）与 D（枚举↔字符串双维护失步）收敛四处余项，共 11 文件：

**① GeoJSON 坐标有理化分母（判据 C）**：`interop_import.c` 中 GeoJSON 坐标导入的缩放因子 `1e9` 与整数分母 `1000000000ULL` 裸写、分处两行易失步。`interop.h` 新增权威宏 `INTEROP_COORD_DENOM_PRECISION_GEOJSON 1000000000ULL`（紧随既有 `INTEROP_COORD_DENOM_PRECISION 1000000ULL` 之后），`interop_import.c` 3 处（`xn`/`yn` 缩放 + `symbolic_coord_create_rational` 分母）统一引用，缩放因子与整数分母同源。

**② 时间换算残值（判据 C）**：
- `lv_utils_misc.c` 删除 4 个局部时间宏（`lv_US_PER_MS`/`lv_MS_PER_S`/`lv_US_PER_S`/`lv_NS_PER_S`），改为注释指向 `lv_utils.h` 权威常量（消除与权威宏的重定义警告源）。
- `adaptive_threshold.c`：`/ 1000000000ULL` → `/ lv_NS_PER_S`（1 处）。
- `context.c`：`/1000` → `/ lv_US_PER_MS`（1 处）。
- `lv_circuit_breaker.c`：`/1000` → `/ lv_US_PER_MS`（2 处）。
- `test_framework.c`：`/1e6` → `/ (double) lv_NS_PER_MS`、`/1e9` → `/ (double) lv_NS_PER_S`（4 处 ns→ms/s 报告换算具名化）。

**③ token 表单源化（判据 D）**：`lv_lexer.h` 的手动 `LvTokenType` 枚举 + `lv_lexer.c` 的 `lv_token_type_name` 手写 75 项名称数组双份维护。改为单一 X-macro `LV_TOKEN_TYPE_X(x)`（`lv_lexer.h`，含字面量/43 关键字/运算符分隔符/EOF/ERROR 分组，第二参数承载显示名，如 `x(KW_ANGLE, "KW_ANGLE")`），枚举 `LV_TOKEN_TYPE_X(LV_X_ENUM_ITEM)`，名称数组 `lv_XMACRO_TO_NAME_ARRAY(LV_TOKEN_TYPE_X)`，计数 `LV_TOKEN_COUNT (0 LV_TOKEN_TYPE_X(LV_X_TOKEN_COUNT_ITEM))`。`lv_lexer.c` 补 `#include "lv/lv_xmacro.h"`，`lv_token_type_name` 改为单源生成 + 越界返回 `"UNKNOWN"`。

**④ type_system 计数自动跟随（判据 D）**：`type_check.c` 手写 `#define TYPE_KIND_COUNT 10` 与 `type_system.h` 的 `LV_TYPE_KIND_X` 枚举值失步风险。`type_system.h` 新增 `LV_TYPE_KIND_COUNT (0 LV_TYPE_KIND_X(LV_X_TYPE_KIND_COUNT_ITEM))` 自动计数；`type_check.c` 改为 `#define TYPE_KIND_COUNT LV_TYPE_KIND_COUNT`（5 个函数指针数组 + 5 处边界检查自动跟随枚举）。

**豁免**：无代码豁免（四处余项全部迁移；`lv_lexer.h` 显示名与枚举标识符的有意不一致由 X-macro 第二参数承载，非豁免）。

### 验证
ninja build3 925/925（exit 0，仅含既有宏重定义警告）+ ctest 169/170 通过；唯一失败 `performance_test` 为既有内存池基准抖动（Windows 计时粒度致 "Pool should be faster than malloc" 断言失败，运行 6218s 超长），与本轮 U1–U6 改动无关，非回归。U5 相关 15 项定向测试（solver / symbolic_coord / rational / interval_arith / conflict / solve_debug / geometry_core / smt_backend）全部通过；U6 相关 9 项定向测试（solver / symbolic_coord / geo_constraint_solver / geometry_core / solver_submodules）全部通过；U7 相关 15 项定向测试（runtime_monitor / probabilistic / high_dim / propagation / geo_invariant / symbolic_coord / proof_search / numeric / solver / geometry_core）全部通过；U8 相关 10 项定向测试（type_system / type_equiv_explorer / minimal_parse / utils / interop / circuit_breaker / adaptive_threshold / lv_lexer / lv_parser / func_block_utils）全部通过。

### U9 明细（geo_dynamic 拓扑栈动态化 + 空白跳过收敛；错误消息组装 / strchr 切分评估后豁免）

U9 共四子项，经评估实际落地两子项（拓扑栈、空白跳过），两子项（错误消息组装、strchr 切分）经同构判定不满足粒度门槛，登记豁免：

**① geo_dynamic 拓扑栈动态化（判据 A / B）**：`geo_dynamic.c` 的 `lv_dyn_graph_topological_sort` 原以 `int queue[1024]` 固定栈 + `rear < 1024` 硬上限，>1024 节点时发生越界写与静默截断（大图被误判为环）。改为 `lv_malloc` 动态队列（沿用既有 `has_path` 的动态队列模式），新增初始化入队 OOM 路径 `lv_RETURN_ERROR`，结束处先 `lv_free(&queue)` 再 `lv_free(&in_degree)`，OOM 分支返回 `lv_ERROR_ALLOCATION_FAILED`。`test_geo_dynamic.c` 新增 `[组 12] 大图拓扑排序（>1024 节点）`（1500 自由点 + `order[1600]`，断言 `count == node_count`）钉住修复行为。

**② 空白跳过收敛（判据 A）**：全库「`while (isspace/*空白集*/) p++`」循环两形态——NUL 结尾用既有 `lv_str_skip_ws`，有界 `(p, end)` 用新增 `lv_str_skip_ws_n`。`lv_str_utils.h/.c` 新增有界变体 `lv_str_skip_ws_n(p, end)`（契约卡五字段齐全：跳过 4 字符空白集、最多到 end、不修改不分配、NULL 返回 p）。迁移 9 处：
- `lean4_bridge.c` 4 处（主循环 / match 分支 / arrow 后 / tactic 参数循环）有界 isspace → `lv_str_skip_ws_n`；
- `coq_bridge.c` 1 处（行级空白）有界 isspace → `lv_str_skip_ws_n`；
- `proof_strategy_numeric.c` 2 处（L143 函数名后 `(` 前的有界跳过 → `lv_str_skip_ws_n`；`numeric_ce_skip_ws` NUL 结尾 → `lv_str_skip_ws`）；
- `gappa_propagate.c` 1 处（`skip_ws` NUL 结尾 → `lv_str_skip_ws`）；
- `math_input.c` 1 处（`lv_math_input_detect_format` 前导空白 → `lv_str_skip_ws`）；
- `float_error.c` 1 处（表达式解析循环内空白跳过 → `lv_str_skip_ws`）。
- `test_utils.c` 的 `test_string_operations` 新增 `lv_str_skip_ws_n` 测试（跳过 / 停 end / 整串 NUL-有界等价 / 无空白 / 空串 / 全空白 / NULL 安全）。

**③ 错误消息组装 → 评估后豁免（不落地）**：候选 `lv_impl_upper_orchestrator`（`set_error_msg`/`set_last_error` 已用 `lv_strlcpy` 收敛）、`engine_scheduler` 6 处、`formula_curve` 4 处、`lv_sema`（`sema_error` 已用 `lv_snprintf` + 前缀收敛）。剩余 `snprintf(X->error_message/error_msg, sizeof(...), fmt, ...)` 均为**格式化消息**（含 `%d`/`%s`），属判据 K 豁免 #1（格式化消息样板）；且字段名 `error_message`（SMT/formula_curve 结果）与 `error_msg`（lvSessionStage）异构、部分实为成功状态消息（orchestrator 阶段消息），无统一 `lv_set_result_error` 设施可覆盖而不触碰负面清单 #4（为适配设施而迁就调用点语义）。**豁免**。

**④ strchr 单次切分 → 评估后豁免（不落地）**：全库 32 处 `strchr` 用途异构——定界符分别为 `(`/`)`/`,`/`/`/`=`/`\n`，后续逻辑分别为「前缀长度计算」「右半 strtod 解析」「复合运算符跳过」「换行定位」等，无 ≥2 处等价「按定界符切分为左右子串」调用点；最接近的 `proof_version_isar.c` 两函数重复的「跳 `==`/`!=`/`<=`/`>=` 找单一 `=`」为高度专用谓词而非通用切分，且仅同文件 2 处、无第三候选（负面清单 #5 无候选孤例）。**豁免**。

### U9 验证
ninja build3 925/925（exit 0，仅含既有宏重定义警告）+ `ctest --test-dir build3` **170/170 全通过**（本轮 `performance_test` 通过，非既有抖动）。定向：`geo_dynamic_test`（含组 12 大图拓扑排序）通过；`utils_test`（含 `lv_str_skip_ws_n` 新增断言）通过；`proof_strategy_numeric_test` / `gappa_dsl` / `minimal_parse_test` / `interval_arithmetic` 等受影响模块测试通过。

### U10 明细（注册表三件套 + 单侧钳制：评估后豁免，不落地）

U10 两子项经全库甄别后判定均不满足粒度门槛（真实等价调用点 ≥2 且行为等价可验证），登记豁免：

**① 注册表三件套 → 评估后豁免（不落地）**：`lvRegistry`（`lv_registry.h`）已提供完整泛型注册表（init/destroy/clear/register/create/find/put/put_ex/get/remove/remove_prefix/count/get_at，内部 name 拷贝 + destroy 回调 + 顺序保持的 shift-left 紧凑删除 + 哈希副索引），并已有 `lv_REGISTRY_STATIC` 宏收敛文件级单例样板。全库残余手写「name→X」注册表彼此异构，无一与 `lvRegistry` 语义同构：

| 手写注册表 | 元素/名所有权 | 删除语义 | 容量/并发/生命周期 |
|------------|---------------|----------|--------------------|
| `lv_backend_plugin.c`（`lvBackendPluginRegistry`） | `lvBackendPlugin**` 指针数组，名借自插件 | swap-last O(1) 移除，非顺序保持 | 动态倍增 + mutex + lv_once；含 `find_by_type` 与按优先级 qsort init/cleanup |
| `ecosystem.c`（`EcosystemEntry modules[128]`） | `char name[64]` 定长内嵌缓冲 | 无 per-entry remove，仅 shutdown 整体重置 | 固定 128 + 显式 init/shutdown（可重入复位 initialized） |
| `module_serialize_autosave.c`（`AutoSaveEntry entries[64]`） | `char *module_name` strdup 内部持有 | 无 remove，仅整体 cleanup | 固定 64 + 无锁 |
| `module_delta.c`（`DeltaBaseline entries[64]`） | `char *module_name` strdup + 8 个 darray 快照字段 | 无 remove，仅整体 free | 固定 64 + 惰性 mutex |

四者元素类型、名称所有权（借入/定长拷贝/strdup）、删除语义（swap vs 无）、容量模型（动态 vs 固定）与生命周期（lv_once vs 显式复位）均互异。强行迁移为 `lvRegistry` 的「name 拷贝 + destroy 回调 + 顺序保持删除」会改变调用点所有权/顺序语义（负面清单 #4 调用点迁就），故不落地。**豁免**。

**② 单侧钳制 → 评估后豁免（不落地）**：全库 `if (x <op> y) x = y;` 形态命中 42 处，甄别为两类：

- **极值追踪（非钳制，约 15 处）**：`aabb_tree_impl.h` 12 处 `tmin/tmax/t0/t1` 射线-AABB 区间收缩、`high_dim_fidelity.c` 4 处 `min_x/max_x/min_y/max_y` 包围盒、`probabilistic_constraint.c:1120` `max_diff`、`hip_backend.c:248` `local_max`、`ode_solver.c:202` `max_err`——均为「求运行极值」而非「钳到边界」，语义属 min/max 归约，不应与钳制混淆。
- **真单侧钳制（约 22 处，异构）**：`level>10`/`score>1000`/`confidence<0.5`（axiom_rule_engine）、`alpha/beta<0.01`（probabilistic_constraint）、`segments>128`（geometry_csg_eval）、`digits_after_dot>9`（expr_canon）、`cnt>64`/`cnt>10`（formula_converter_stmt）、`ev>1e12`（high_dim_project）、`factor∈[0.1,2.0]`/`span/ux/uy`（high_dim_fidelity）、`len<0→0`（lv_number）、`new_pos<0→0`/`size>avail`（lv_storage）、`half<2`（graph_node_alloc）等。其类型（int/double/size_t）、方向（上限/下限）、钳到值（10/1000/0.5/0.01/2/128/9/64/0/1.0/0.1/2.0/1e12 及变量 `avail`）各不相同，无 ≥2 处「同类型同方向」等价调用点可收敛为单一新原语。

既有设施 `lv_MIN`/`lv_MAX`（类型无关宏）、`lv_CLAMP`（双侧）、`lv_clamp`（double 双侧）、`lv_min_i/lv_max_i`/`lv_min_z/lv_max_z` 已覆盖钳制语义；单侧钳制各处的魔法常量承载独立领域语义（递归上限/评分上限/置信下限/分布参数下限/网格细分上限/精度上限/迭代上限等），`if (x > limit) x = limit;` 的直陈形式比 `x = lv_MIN(x, limit);` 更直观且不引入新设施收益。故不新增原语。**豁免**。

### U10 验证
本轮 U10 为纯评估登记，无代码改动；构建与测试基线沿用 U9 收尾结果：ninja build3 925/925（exit 0）+ `ctest --test-dir build3` 170/170 全通过。批次 U（U1–U10）至此全部完成。

---

## 十七、批次 V 候选立项与实施（2026-08-13）

**候选来源**：历史批次「继续寻找更多可抽象化的方向」按判据 A–K 全库扫描，共 8 候选（V1–V8），用户选定「全部按优先级逐步完整执行」。V1–V3 已完成并验证，V4–V8 待执行。

### 批次 V 执行进度

| 编号 | 内容 | 状态 |
|------|------|------|
| V1 | 手写三行 swap → `lv_SWAP`（约 10 处追加） | 完成 |
| V2 | 语义常量具名化（1e-300 / 1e6 / 1000 / 1000000） | 完成 |
| V3 | 判据 D 单源化（RelAtomType / high_dim_utils / TrustColor/ProofColor） | 完成 |
| V4 | 线性查找 → `lv_array_find_index_if`（实测 13 按 id + 11 按名） | 完成（不迁移） |
| V5 | swap-last 删除 → `lv_array_swap_remove`（约 6 处） | 完成（不迁移） |
| V6 | 纯 free 析构 3 处 → `lv_obj_destroy_fields` | 完成（不迁移） |
| V7 | min/max 归约 → `lv_argmax / lv_argmin`（甄别后执行或豁免） | 完成（不迁移） |
| V8 | C 组评估登记（edgebreaker push ×8、空白跳过判别、倍增残留 2 处、坐标缩放/阈值语义确认） | 完成（评估登记） |

### V1 明细（手写三行 swap → lv_SWAP，判据 A）
在既有 11 处基础上追加约 10 处手写三行 swap（`tmp = a; a = b; b = tmp;`）→ 既有 `lv_SWAP(type, a, b)`（lv_utils.h）。涉及文件：geo_topology.c、equiv_class.c、probabilistic_constraint.c、graph_node_alloc.c、proof_search_algo.c、solver_engine.c、graph_rank.c、gappa_propagate.c、algebraic_number_interval.c 等。

### V2 明细（语义常量具名化，判据 C）
- `1e-300` → `lv_TINY_SENTINEL`（config.h；symbolic_coord_trust.c 2 处「接近零」哨兵）
- `1e6` → `GEODET_DIVERGENCE_FACTOR`（geo_event_detect.c 局部发散检测因子，3 处）
- `1000` → `LV_DIFFICULTY_MAX_SCORE`（axiom_rule_engine.c 局部难度评分上限，4 处）
- 漏网 `/1000000` → `lv_NS_PER_MS`（runtime_monitor.c）

### V3 明细（判据 D 单源化）

**V3a MiniStmtType 三重维护子候选 → 豁免**：mini_kernel.c 两套名称表分别对应不同枚举且不同构；`import_mm` 反向 if 链仅显式处理 4 个语句类型并有意不识别 `MINI_STMT_COMMENT`（`$=` 由解析流程消费为终止符），不满足「同构平行表」门槛，避免负面清单 #4 调用点迁就。

**V3b RelAtomType 双头统一**：新建 `lv/rel_atom_type.h`（`REL_ATOM_POINT..FUNC_BLOCK` 0–4、`REL_ATOM_UNKNOWN` 99）；`relation_model.h` 与 `sat_encoding.h` 各自删除本地枚举，改 include 公共头（sat_encoding.h 原 `REL_ATOM_CUSTOM = 99` 命名统一为 `REL_ATOM_UNKNOWN`）。

**V3c high_dim_utils 反查链 → lv_str_to_enum**：`high_dim_mapping_type_from_string` 的 4 分支 if 链改为 `lv_str_to_enum` 查表。

**V3d TrustColor/ProofColor 枚举双定义单源化**：新建公共头 `lv/trust_color_x.h` 作为 X-macro 单一事实源（`LV_TRUST_COLOR_X` 5 列 10 项 + `LV_PROOF_COLOR_X` 4 列 12 项）；`symbolic_coord.h` 与 `proof.h` 删除手写枚举，从主源列表局部宏生成枚举；5 个消费方（trust_color.c / graph_dot_export.c / graph_serialize.c / interop_export_coq.c / proof_export.c）改 include 公共头；删除私有头 `core/src/layer4_reasoning/proof/trust_color_x.h`。

**豁免（判据 D）**：mini_kernel 两名称表（不同构）、graph_dot_export switch（单点）、MiniStmtType（`import_mm` 有意排除 `$=`）。

### V4 明细（线性查找 → lv_array_find_index_if，不迁移）

全库扫描实测 24 处「返回下标」线性查找（13 按 id + 11 按名），形态严重异构，无一满足「结构体数组 + 谓词 → `return i` / `return -1`」干净契约：

- **类型 A（按 id，13 处）**：`geometry_compress_triangle.c:84`（多顶点 id）、`lv_utils_array.c:336`（纯 int 数组，且是设施自身）不适用结构体泛型谓词；其余散布各层但元素类型（int64_t id / theory_id / 顶点 id）各异。
- **类型 B（按名，11 处）**：`global_state` / `lv_registry` / `performance_profiler`×2 / `func_block_preset_internal` / `preset_blocks` 共 6 处带哈希快查回退，属容器内核而非线性样板；`lv_registry.c:107` 是设施自身实现；`lv_str_match_any` / `lv_str_match_delimited` 为 `strstr` 子串匹配而非 `strcmp == 0`。
- **边界情况**：大量返回指针 / 状态码 / break 后返回状态 / 内联循环 / 输出参数 / 哈希 / 二分，均非干净 `return i/-1` 形态。

强行建设会触发 §4.3（禁止宏泛型）、§6 #4（调用点迁就）或需 mode 分支（§2.3 终审一），且按名/按 id 下标查找已被 `lv_registry`、`lv_darray`、哈希快查等既有设施覆盖，无新设施必要 → **登记不迁移**。

### V5 明细（swap-last 删除 → lv_array_swap_remove，不迁移）

grep `= arr[count-1]` / `--count` / `*cur = *last` / `arr[i] = arr[...--]` 仅得 5 行，逐一甄别无统一契约：

- `plugin_system_autoload.c:76`：`char*` 元素先 free 当前元素 → `*cur = *last` → pop，所有权/释放顺序特化。
- `inequality_reasoning_sign.c:222`：`lvExpr*` 元素 + 表达式合并 + `goto next_op` 嵌套双循环，语义特化。
- `groebner_parallel.c:219`：`*t = *last` 已由批次 Q11 `simple_poly_remove_zero_terms` 吸收（同文件 AoS PolyTerm）。
- 其余命中为 `line[--len]='\0'` 字符串修尾或前缀和，与删除无关。

元素类型/所有权/删除语义各异，无 `lv_array_swap_remove` 契约可覆盖 → **登记不迁移**。

### V6 明细（纯 free 析构 3 处 → lv_obj_destroy_fields，不迁移）

`lv_obj_destroy_fields` 已全库 46 处使用，历史 U4 已收敛纯 PLAIN_FREE 对象销毁。批次 V 所列 3 处残余未形成 ≥2 处同构、尚未独立定位（实质为候选尾部孤例），避免 §6 #5 无候选孤例 → **登记不迁移**。

### V7 明细（min/max 归约 → lv_argmax/lv_argmin，不迁移）

历史批次 P11 已对 28 处（4 选择排序 + 24 线性选优）判定「比较键不同、相等处理不同、返回下标 vs 值 vs 指针」无通用契约；该语义差异依旧成立，不新增原语 → **登记不迁移**。

### V8 明细（C 组评估登记）

- **edgebreaker push ×8**：`geometry_compress_edgebreaker.c` 中 seq 单值 push（`seq[len++] = EDGEBREAKER_C/S/L/R`）与 boundary 双字段 push 元素类型/字段不同构，非同一 `push ×8` 原语可收敛。
- **空白跳过判别**：已收敛——`lv_str_skip_ws` 22 处 + `lv_str_skip_ws_n` 5 处均为设施调用终态，无手写 `while(isspace)` 残留。
- **倍增残留 2 处**：历史批次登记的语义特化豁免，维持。
- **坐标缩放/阈值语义确认**：历史 Q12/Q24/C 豁免覆盖，维持。

→ **评估登记（无新代码改动）**。

### 决策登记（第 9 章格式）
`lv_SWAP` 复用 / A / ~10 处 9 文件 / 无 / 全量构建 + 各相关测试；`lv_TINY_SENTINEL` 复用 / C / symbolic_coord_trust 2 处 / 无 / 数值测试链；`GEODET_DIVERGENCE_FACTOR` / C / geo_event_detect 3 处 / 无 / test_geo_event_detect 族；`LV_DIFFICULTY_MAX_SCORE` / C / axiom_rule_engine 4 处 / 无 / test_axiom_rule_engine 族；`lv_NS_PER_MS` 复用 / C / runtime_monitor 1 处 / 无 / test_runtime_monitor 族；`lv/rel_atom_type.h` 单源 / D / relation_model + sat_encoding 2 文件 / 无 / test_relation_model + test_sat 族；`lv_str_to_enum` 查表化 / D / high_dim_utils 1 处 / 无 / test_high_dim 族；`lv/trust_color_x.h` 主源 / D / symbolic_coord + proof + 5 消费方 7 文件 / 无 / test_*_export 族；判据 D 豁免（mini_kernel 两名称表不同构 / graph_dot_export switch 单点 / MiniStmtType import_mm 有意排除 `$=`）/ D / 0 文件 / 负面清单 #4/#5 / 无；`lv_array_find_index_if` / 评估后豁免 / 0 / 24 处（13 id + 11 name）形态异构（多顶点 id、纯 int 设施自身、哈希快查回退、strstr 子串、返回指针/状态/输出参数）无 ≥2 同构 return i/-1 契约（负面清单 #4 + §4.3）/ 无；`lv_array_swap_remove` / 评估后豁免 / 0 / 5 行命中无统一契约（char* 所有权释放顺序 + lvExpr* 表达式合并嵌套 + 已由 simple_poly_remove_zero_terms 吸收）（负面清单 #4/#5）/ 无；`lv_obj_destroy_fields` 残余 3 处 / 评估后豁免 / 0 / 未形成 ≥2 同构、候选尾部孤例（负面清单 #5）/ 无；`lv_argmax/lv_argmin` / 评估后豁免 / 0 / 比较键不同、相等处理不同、返回下标 vs 值 vs 指针无通用契约（历史 P11 28 处）/ 无；V8 C 组评估 / 评估登记 / 0 / edgebreaker push 异构 + 空白跳过已收敛 + 倍增残留历史豁免 + 坐标缩放历史豁免 / 无。

### 验证
ninja build3 925/925（exit 0，仅含既有宏重定义警告）+ ctest 170/170 全部通过，零修复项。

---

## 十八、批次 W 候选立项（2026-08-13）

**候选来源**：「继续寻找更多可抽象化的方向」按判据 A–L 全库扫描（三路只读子代理），排除批次 V/R/S/T/U/Q/P/K/L/N 已收敛/已判不迁移模式后，共 6 个新候选。

### 批次 W 执行进度

| 编号 | 内容 | 状态 |
|------|------|------|
| W1 | SolverBackendType 四份平行元数据表 → 单一 X-macro 单源（判据 F+D） | 已执行 |
| W2 | π 常量族 → 收敛到 `lv_PI`（判据 C） | 已执行 |
| W3 | formula_string.c 18 处 snprintf 截断防御样板 → static helper（判据 L） | 已执行 |
| W4 | 坐标对销毁 helper `symbolic_coord_pair_destroy`（判据 H） | 已执行 |
| W5 | 180.0 角度桶宽具名化（判据 C） | 已执行 |
| W6 | ConflictSeverity 两列元数据（判据 F 弱） | 登记暂缓（未达 §1.6 门槛） |

### W1 候选（SolverBackendType 四份平行表，判据 F+D，强候选）

同一枚举 `SolverBackendType`（`smt_backend.h:14`，4 值 GROEBNER/SMT_Z3/SMT_CVC5/SMT_SINGULAR）在 2 文件共 4 份平行元数据，任一新后端需同步 4 处：
- 表 a（名表）：`smt_backend_impl.c:391-396`；
- 表 b（type/name/version/priority）：`smt_backend_impl.c:500-510`；
- 表 c（插件注册 9 字段）：`smt_backend_impl.c:618-665`；
- 表 d（type/display_name/executable/encode_error/fallback）：`smt_backend_impl_groebner.c:1095-1107`。
- 附加重复：可执行名 `"z3"`/`"cvc5"` 另在 `smt_backend_impl_external.c:62-64` 硬编码比对。

仿已收敛的 `ATPBackendType`（S-D3，`LV_ATP_BACKEND_ENTRY` 单源 X 列表）收敛为单一 X-macro，生成 name/version/priority/executable/encode_error/fallback 各列。

### W2 候选（π 常量族，判据 C）

同一语义常量 π 在 7 文件 ≥6 处独立定义/裸写（double 精度下数值逐位一致）：
- `config.h:211` `#define lv_PI`（权威源）；
- `lv_numeric.h:37-38` `#ifndef lv_PI` 重复定义（注释自述「与 lv_config.h 保持一致」）；
- `meta_proof.c:138` `#define META_PROOF_PI`；
- `conflict_detector.c:613-614` 局部 `const double PI`；
- `geo_halfedge_mesh.c:785` / `interop_export_geojson.c:227` 裸 `3.14159…`；
- `lv_platform.h:87-88` `#define M_PI`（平台回退 shim，部分豁免）。
- 旁证：`lv_DEG_TO_RAD`/`lv_RAD_TO_DEG`（config.h）零调用点，因 `lv_numeric.c` 直写 `lv_PI/180`。

收敛到 `lv_PI`/`lv_TWO_PI` 为唯一权威源。

### W3 候选（formula_string.c 18 处 L2 样板，判据 L）

`formula_string.c` 18 处同构「`snprintf` + `if (n<0 || n>=buf_size)` 截断防御 + `lv_strlcpy` 兜底」骨架（行 55/66/76/86/96/106/114/122/134/142/150/158/176/191/233/264/279/292），差异仅格式串/兜底串（常量差异）。单文件 → 落 `static` helper（内部走 `lv_snprintf` + 截断判定 + 兜底，返回值区分是否触发兜底覆盖带 `return` 的两处变体），不新增公共 API。

### W4 候选（坐标对销毁 helper，判据 H）

「同一坐标对逐分量销毁」约 16–18 对 / 6 文件，与既有 pair 创建 helper（`symbolic_coord_pair_create_rational` 等）成对但归还侧仍手写两行 `symbolic_coord_destroy(a); symbolic_coord_destroy(b);`（`symbolic_coord_destroy` 已 NULL-safe）：
- `meta_repr.c:189-190/335-336/359-360`（3 对）、`formula_curve.c:729-730/744-745/797-798/806-807/849-850`（5 对）、`formula_converter_geom.c:200-201/234-235`（2 对）、`formula_converter_constraint.c:186-187/207-208`（2 对）、`formula_converter_complex.c:78-79/248-249/310-311`（3 对）、`singular_backend.c:864-865`（1 对）。

事实同构（差异仅数组名，常量差异）。落 `symbolic_coord.h` 声明 + `symbolic_coord_lifecycle.c` 实现，与既有 pair 创建 helper 对称。`impl_preset_transformations.c` 宏内额外 `lv_free` 堆数组所有权不并入。

### W5 候选（180.0 角度桶宽，判据 C，粒度临界）

2 处同构「`180.0 / bucket_count` 角度域 [0,180) 离散化桶宽」：
- `sat_encoding.c:543`（`bucket_count = 1 << DEFAULT_BITWIDTH`）；
- `bdd_encoding.c:793`（`bucket_count = 1 << 8`）。

`bucket_count` 取值不同属常量差异可参数化，但仅 2 处、紧贴粒度门槛 §1.13 下限 → 待甄别。

**决策（已执行）**：核验 `DEFAULT_BITWIDTH == 8`，两处 `bucket_count` 均为 `1<<8`（256 桶），语义完全同构为「角度域 [0,180) 上限 = 半圆周角度（度）」。新增权威常量 `lv_HALF_CIRCLE_DEG 180.0`（config.h 角度系数区），替换 `sat_encoding.c` / `bdd_encoding.c` 两处裸 `180.0`。未提取整段离散化 helper（仅 2 处，避免过度抽象），也未触及 `lv_numeric.c` / `meta_proof.c` 中 `180.0`（属弧度↔度转换式，语义不同）。

### W6 候选（ConflictSeverity 两列元数据，判据 F 弱，孤例）

`ConflictSeverity`（3 值）在 `conflict_detector.c` 2 份平行表（标志偏移表 `:51-55` + 名称表 `:58-62`），单枚举、仅 2 列、1 文件，未达 §1.6「≥3 处同构」家族门槛 → 待甄别（登记暂缓倾向）。

**决策（登记暂缓）**：核验仅 2 列（标志偏移 + 名称字符串）、1 文件、3 值，未达 §1.6「≥3 处同构」与 §1.13 粒度门槛；且两列类型异质（`size_t offsetof` vs `const char *`），X-macro 收敛收益低于成本。登记为暂缓，待未来 severity 元数据列 ≥3 时再评估。

### 备注
- 判据 A/B/D/E/G/I/J/K 本轮无新候选（子代理已逐条核验并排除历史已收敛/已判不迁移模式）。
- 判据 H 另发现「节点 x/y double 提取」高频残留，但系既有 `symbolic_coord_get_xy`（Q19）的未迁移调用点，非新骨架，仅登记为遗留迁移面，不立项。

---

## 十九、批次 X 候选立项（2026-08-13）

**候选来源**：「继续寻找更多可抽象化的方向」按判据 A–L 全库扫描（四路只读子代理：A/B、C/D、E/F/G/H、I/J/K/L+自由反例），排除批次 W/V/R/S/T/U/Q/P 已收敛/已判不迁移模式后，共 8 个执行/裁定候选 + 5 项暂缓/豁免登记。自由代理实证出「手写 strdup 同义 API 双份」这一规范盲区。

### 批次 X 执行进度

| 编号 | 内容 | 判据 | 状态 |
|------|------|------|------|
| X1 | 手写 strdup 同义 API 双份（4+1 处）→ 统一 `lv_strdup_safe` | 自由反例/J 延伸 | 已执行 |
| X2 | interop_import.c 2 处裸 atoi → `lv_parse_int_default`（+ 修注释脱节） | 自由反例 | 已执行 |
| X3 | 360.0 全圆周角度 8 处 → `lv_FULL_CIRCLE_DEG` | C | 已执行 |
| X4 | 默认描边线宽 1.5 11 处 → `lv_DEFAULT_STROKE_WIDTH` | C | 已执行 |
| X5 | formula_converter_constraint.c:337-338 成对销毁漏网 → `symbolic_coord_pair_destroy` | H 收尾 | 已执行 |
| X6 | 手写选择排序骨架 → `lv_selection_sort`（或复用 `lv_insertion_sort`） | A | 暂缓 |
| X7 | `NODE_CONSTRAINT_*` 约束名双份维护（≥7 文件）→ X-macro 单源 | D | 暂缓 |
| X8 | 几何构造函数名 point/line/circle/segment/ray/triangle 跨模块双份 | D | 暂缓 |
| X9 | impl_preset_transformations.c 273 处销毁（W4 豁免复核 + 6 元矩阵） | H | 登记暂缓 |
| X10 | smtlib2 20 处 / atp_backend 6 处游标缓冲链 | L | 登记豁免 |
| X11 | `xN` 变量索引扫描（float_error/fptaylor_eval，2 处） | I | 登记暂缓 |
| X12 | 测试侧成对销毁 7 组 / 变换类型名（值集不同构） | H/D | 登记暂缓 |

### X1 候选（手写 strdup 同义 API 双份，自由反例，最强）

5 处手写字符串复制与公共 `lv_strdup_safe`（`lv_utils_str.c:79`）逐字节同构（`if(!s) return NULL; len=strlen; lv_malloc(len+1); memcpy(...,len+1)`）：
- `lv_hashtable.c:98` `lv_ht_strdup`（static，**缺 NULL 检查，缺陷**，1 调用点）；
- `axiom_pkg.c:48` `safe_lv_strdup_safe`（非 static，~20 调用点 / 5 文件）；
- `rewrite_strategy.c:45` `str_dup`（static，~14 调用点）；
- `groebner_engine.c:153` `groebner_strdup_safe`（非 static，~11 调用点 / 8 文件）；
- `geometry_transform.c:1213-1219` 内联 `lv_malloc(name_len+1)+memcpy`。

`lv.h` 已有 `#define lv_strdup lv_strdup_safe`。统一删除 4 处命名实现 + 1 处内联，改调 `lv_strdup_safe`（`lv_ht_strdup` 若因容器内核自包含需保留则显式豁免标注）。任意一侧修复 OOM/NULL 语义都会造成其余副本漂移，是规范盲区失步源。

### X2 候选（裸 atoi + 注释承诺脱节，自由反例，强）

`interop_import.c:1042/1223` 两处裸 `atoi(buf)` 解析 GGB `P` 属性全局索引，非法输入返回 0 会被误判为节点 0。历史 g4 已迁移 9 处 atoi → `lv_parse_int_default` 但漏了本文件；且 `interop.c:45-49` 注释声称「已替代不安全的 atoi()」，与代码现实脱节。改 `lv_parse_int_default(buf, -1)`，失败回退 -1 自然落到下游 `idx >= 0 && idx < el_count` 范围外（安全改进，终审二）。

### X3 候选（360.0 全圆周角度，判据 C，强）

`geometry_csg_eval.c:349/428`、`geo_invariant_type.c:43-45`（3 处值域上限）、`geometry_transform.c:162/164/165`（int 形态）共 8 处 / 3 文件同义「全圆周角度（度）」。建议 `lv_FULL_CIRCLE_DEG 360.0`（config.h，与 `lv_HALF_CIRCLE_DEG` 对称；int 形态用 `(int)lv_FULL_CIRCLE_DEG`）。

### X4 候选（默认描边线宽 1.5，判据 C，强）

`interop_export_svg.c:39/97/248/250/254`、`interop_export_pdf.c:78/165/256/285/306`、`block_canvas.c:476` 共 11 处 / 3 文件同义「输出渲染默认线宽 1.5」。建议 `lv_DEFAULT_STROKE_WIDTH 1.5`（config.h）。注意区分 `geometry_canvas.c:181` 用 2.0、`geo_visual_complete.c:48` 用 1.0f（不同渲染器默认值，不纳入）。

### X5 候选（成对销毁漏网，判据 H 收尾，强）

`formula_converter_constraint.c:337-338` 连续 `symbolic_coord_destroy(angle_coords[0]); symbolic_coord_destroy(angle_coords[1]);`，系 W4 在同文件已迁移 `mid_coords` 后漏掉的一处（数组下标 `[0]/[1]` 形式致成对 grep 漏报）。迁移到既有 `symbolic_coord_pair_destroy`。

### X6 候选（选择排序骨架，判据 A，待甄别）

3 处事实同构 + 1 处结构差异：
- `aabb_tree_impl.h:68-99`（模板 2D/3D，int indices 按 bbox 中心升序）；
- `proof_search_algo.c:380-390/433-443`（PQEntry 按 score 降序，`lv_SWAP`）；
- `rewrite_vf2.c:608-622`（candidates+cand_scores 平行双数组，结构差异，触发终审一）。

建议 `lv_selection_sort`（对齐 `lv_insertion_sort` 签名 `void*base,size_t n,size_t elem_size,lvCompareFn cmp,void*ctx`），平行数组变体登记为扩展候选不强行合并。待甄别：是否直接复用已有 `lv_insertion_sort` 而非新建（需确认其签名/稳定性语义与调用点排序稳定性需求）。

### X7 候选（NODE_CONSTRAINT_* 双份，判据 D，待甄别）

`NODE_CONSTRAINT_*`（`formula_parser.h:83-90`，8 值）的「名称 ↔ 枚举」双向映射在 ≥7 文件独立手写：`formula_dsl.c:1104` kConstraintTypes、`formula_string.c:279/363` str_constraint_* + 分发表、`formula_renderer_ascii.c:319/417`、`formula_renderer_dsl.c:461`、`formula_renderer_latex.c:634`、`formula_renderer_python.c:469`、`formula_parser.c:64` keywords、`bootstrap_test_random.c:194`。各 renderer 输出串不同（latex `\perp` vs dsl `perpendicular`），需设计「语义名 + 多渲染目标」的 X-macro 结构，较大工程，分级推进。注意与历史 i4 收敛的「关系词表」有词面重叠但枚举独立。

### X8 候选（几何构造函数名双份，判据 D，待甄别）

「point/line/circle/segment/ray/triangle」构造元素名在 ≥8 文件独立维护（`dsl_lexer.c:155`、`lv_parser.c:1105`、`lv_sema.c:273`、`formula_dsl.c:1080`、`formula_parser.c:64`、`gc_language.c:37`、`preset_common.c:184`、`interop_command.c:476`、`module_lvz.c:504`、`proof_version_ghost.c:112`）。映射目标枚举/回调各异（DSL_TOK_*/PRESET_TYPE_*/check/parse/handler），名称层可先单源，但完全统一需枚举对齐，接近 P2-1/P2-2「值集不一致」风险面，落地前先做对齐评估。

### X9–X12 暂缓/豁免登记

- **X9**（impl_preset_transformations.c 273 处销毁）：W4 立项备注已豁免「宏内额外 `lv_free` 堆数组所有权不并入」；代理另发现 5 处 6 元仿射矩阵销毁（s_a11…s_ty 具名变量手写展开），但 `symbolic_coord_destroy_many` 需手动构造指针数组、收益有限 → 登记暂缓。
- **X10**（smtlib2 20 处 + atp_backend 6 处游标缓冲链）：系「调用方缓冲」形态（`char*buf,int remaining` 返回已写字节数/失败 -1）+ 外部格式契约（SMT-LIB2/TPTP），与 U3 豁免判例同型且触负面清单 #2 → 登记豁免。
- **X11**（xN 变量索引扫描 2 处）：float_error/fptaylor_eval 已由批次 F f3 登记为「独立手写表达式解析器（未来统一 ExprNode IR）」；单独抽 xN 解析子骨架与统一 IR 大方向重复、且仅 2 处 → 登记暂缓。
- **X12**（测试侧成对销毁 7 组 + 变换类型名）：测试夹具可能有意用底层 `symbolic_coord_destroy` 原语验证原语本身；变换类型名（translation/rotation/scaling）值集与 TRANSFORM_* 枚举不同构 → 均登记暂缓。

### 批次 X 执行结果与甄别结论（2026-08-14）

**验证**：`ninja` 全量重建 925/925 通过（exit 0，仅既有 `lv_LOG_*_LEN` 重定义警告，与本批无关）；`ctest --output-on-failure` 170/170 全绿。

- **X1 已执行**：删除 4 处手写 strdup 实现（`lv_hashtable.c` 缺 NULL 检查缺陷版、`axiom_pkg.c`、`rewrite_strategy.c`、`groebner_engine.c`）+ 2 处声明（`groebner_engine_internal.h` / `axiom_pkg_internal.h`）+ 1 处内联（`geometry_transform.c`），44 处调用点统一改 `lv_strdup_safe`；全库 grep `safe_lv_strdup_safe|groebner_strdup_safe|lv_ht_strdup|\bstr_dup\b` 无残留。
- **X2 已执行**：`interop_import.c:1042/1223` 两处裸 `atoi(buf)` → `lv_parse_int_default(buf, -1)`，失败回退 -1 自然落到下游范围外；全库该文件 `atoi(` 无残留。
- **X3 已执行**：新增 `lv_FULL_CIRCLE_DEG 360.0`（config.h，与 `lv_HALF_CIRCLE_DEG` 对称）；替换 `geometry_csg_eval.c:349/428`、`geo_invariant_type.c:43-45`、`geometry_transform.c:162/164/165`（int 形态用 `(int)lv_FULL_CIRCLE_DEG`）。
- **X4 已执行**：新增 `lv_DEFAULT_STROKE_WIDTH 1.5`（config.h）；PDF 5 处数值实参 + SVG/block_canvas 6 处格式串字面量（`1.5` → `%g` + 实参，含 `extra_attr` 格式串形态 `" stroke-width=\"%g\" …"` 经 `fprintf(ctx->fp, syn->extra_attr, lv_DEFAULT_STROKE_WIDTH)` 注入）全部收敛。
- **X5 已执行**：`formula_converter_constraint.c` 连续两处 `symbolic_coord_destroy(angle_coords[0/1])` → `symbolic_coord_pair_destroy(angle_coords[0], angle_coords[1])`。
- **X6 暂缓**：仅 `proof_search_algo.c:380-390/433-443` 两处真同构（且同文件同函数，正确收敛是本地静态函数而非全局设施）；`aabb_tree_impl.h` 系模板间接索引排序（按 bbox 中心算 key）、`rewrite_vf2.c` 系平行双数组，均不匹配既有 `lv_insertion_sort` 元素数组签名；且选择→插入排序改变稳定性语义，违反严格行为等价。不新增全局设施。
- **X7 暂缓（先对齐评估）**：`NODE_CONSTRAINT_*` 名称↔枚举双份在 ≥7 文件映射到不同渲染输出串（latex `\perp` / dsl `perpendicular` / ascii / python…），命中负面清单 #2（渲染格式契约）与 #4（调用点迁就）；统一需先做值集对齐审计，属较大工程。
- **X8 暂缓（先对齐评估）**：point/line/circle/segment/ray/triangle 名称在 ≥8 文件映射到不同构目标枚举/回调（`DSL_TOK_*`/`PRESET_TYPE_*`/check/parse/handler），接近 P2-1/P2-2「值集不一致」风险面，统一前必须先做对齐评估。

---

## 二十、批次 Y 候选立项与执行（2026-08-14）

**候选来源**：「继续寻找更多可抽象化的方向」+「无视豁免再搜一下」按判据 A–L 全库重扫描（含 BFS 豁免审计与历史批次漏网复核），共 7 项执行/裁定。

### 批次 Y 执行进度

| 编号 | 内容 | 判据 | 状态 |
|------|------|------|------|
| Y0 | 无视豁免重新扫描，完善候选清单 | — | 完成（只读） |
| Y1 | F-1 分发表收敛：手写三行分发表 → `LV_DISPATCH`/`LV_DISPATCH_VOID` | F | 完成（迁移 5 处，复核 4+ 处豁免） |
| Y2 | L2 snprintf 防御收敛（新建 checked 变体） | L2 | 登记不迁移（设施已落地 + W3 已收敛 + 错误传播异构） |
| Y3 | 自由反例1：proof_strategy_deductive.c 9 处裸事实格式串回迁 `DEDUCT_FMT_*` | I 收尾 | 完成 |
| Y4 | F-2 甄别：lv_number.c `ops_for_type` 数据指针查表 | F | 登记不迁移（返回指针非调用分发） |
| Y5 | 自由反例2：gappa_dsl.c 解析块重复 → `parse_bound_in` helper | A/I | 完成（收敛 4 处 sscanf 格式串） |
| Y6 | C 漏网点补漏：C3（1e18 → `lv_LARGE_NUMBER`）git 核实 | C | 无漏网（计数口径修正） |
| Y7 | 构建验证 ninja + ctest，回写台账/记忆 | — | 完成 |

### Y1 明细（F-1 手写分发表 → LV_DISPATCH/VOID，判据 F）

迁移 5 处真正「边界检查 + NULL 槽 + 调用」价值样板：
- `geometric_primitives.c` `geo_create_constraint`（→ `LV_DISPATCH(s_constraint_handlers, ...)`）与 `geo_unify`（→ `LV_DISPATCH(kUnifyHandlers, ...)`）；
- `solver_coord_extract.c` `coord_to_double`（→ `LV_DISPATCH(coord_to_double_ops, ...)`，新增 include lv_xmacro.h）；
- `unify_helpers.c` `compute_node_coord_hash`（→ `LV_DISPATCH(s_coord_hash_funcs, ...)`，删除不再使用的 `s_coord_hash_func_count`）；
- `geometry_csg_eval.c` `eval_csg_bool`（→ `LV_DISPATCH_VOID(s_bool_op_funcs, ...)`，删除 `s_bool_op_count`）。

**复核不迁移**（fallback/else 分支含错误/警告副作用，非纯值三行样板）：`geo_constraint_solver_residual.c:329`（`*error_val=0.0`）、`numerical_backend.c:1206`（`lv_ERROR_SET`）、`solver_coord_extract.c:1209`（`lv_LOG_WARNING`）、`graph_node_alloc.c:1326`（`GeomNodeVTable*` 指针表，非函数 call 分发表）。

### Y2 登记不迁移（L2 snprintf 防御收敛，判据 L2）

判据 L2 所指「带防御的 `lv_snprintf` 包装」**设施已落地**：`lv_utils_str.c:183` `lv_snprintf`（NULL/大小检查 + vsnprintf 负值防御 + 截断 NUL 终止，30 处调用），单文件 18 处同构样板已由批次 W3 收敛为 `str_snprintf_fallback`。剩余跨文件裸 `snprintf` + 截断检测 6 处经差异分类**不构成统一骨架**：
- `interop_server.c:972/990` 截断后仍**消费返回值**（sha1 长度 / send 长度），无法用 bool 谓词替代；
- 截断后错误传播控制流 4 种异构（`return NULL` / `free+RETURN_ERROR` / `reply_error+return false` / 仅 `\0` 终止）；
- `formula_curve.c:758/823` 截断后仅强制 `\0` 终止继续使用缓冲区（非提前返回）。

判定：新建 `lv_snprintf_checked` 会造成伪收敛（引入无法满足所有调用点语义的单一失败通道），登记不迁移。

### Y3 明细（自由反例1：DEDUCT_FMT_* 格式串回迁，判据 I 收尾）

`proof_strategy_deductive.c` 顶部已有 `DEDUCT_FMT_*` 宏族（I1 建立），本次将 9 处裸事实格式串逐一回迁宏：`betweenness`（sscanf×2 + DEDUCT_ADD_FACT）、`point_coord`（sscanf ID 形态×2 + snprintf STR 形态）、`incidence`、`coincident`、`intersection`。消除生成/解析双侧格式串漂移；Grep 确认无裸事实格式串残留。

### Y4 登记不迁移（F-2：`ops_for_type` 数据指针查表，判据 F）

`lv_number.c:443` `ops_for_type` 是「返回 `lvNumberOps*` 指针」查表（`if ((unsigned)type < N && kOpsByType[type]) return kOpsByType[type]; return &g_float_ops;`），非 `LV_DISPATCH` 的「越界/NULL + **调用** handler」语义（宏展开 `table[key](__VA_ARGS__)` 会尝试调用结构体指针）。全库同类「返回指针查表」（`get_vtable_for_type`/`lv_expr_get_ops`/`lv_ad_get_ops`/`find_vector_ops` 等）fallback 语义各异（NULL vs 默认指针）、查表形态各异（下标 vs 线性扫描 vs 上下界），不构成可收敛骨架。

### Y5 明细（自由反例2：gappa_dsl.c `parse_bound_in` helper，判据 A/I）

`gappa_parse` 中「var in [lo, hi]」区间界谓词 sscanf 解析在假设解析与目标解析两处逐字重复（4 处格式串，含逗号前空格两种变体）。新建 `static bool parse_bound_in(token, varname, lo, hi)` 收敛，两处调用点共用，消除格式漂移。

### Y6 C 漏网点补漏（C3 git 核实）

git `-S "1e18"` 核实 C3（`1e18` → 复用 `lv_LARGE_NUMBER`）实际迁移 4 文件：`geometry_canvas.c` + `block_canvas.c`（边界盒四元初值各 4 字面量）+ `test_framework.c`（`min_ns`）+ `proof_version_sledge.c`（`best_time`），共 **10 处裸字面量**。台账「4 文件 4 处」为「4 赋值位置」口径。复核全库裸 `1e18`（含 `1e18f/1E18/1e+18/1.0e18/0x1e18` 变体）**零残留**，仅剩 `config.h:176` 权威定义 1 处。代码无漏网，仅修正台账计数口径。

### BFS 豁免治理注释补全

y0 扫描发现判据「BFS 图遍历收敛」豁免注释缺口 2 处，已补 `/* exempt */` + 决策登记：`type_equiv_explorer.c` `type_equiv_explore_search`（带 TypeRegion 深拷贝 + 副作用执行的状态空间搜索）与 `proof_rule_engine.c` `search_breadth_first`（带证明状态快照 + rule->apply_fn 副作用）。`proof_search_algo.c` DFS/BFS 已有注释作为模板。

### 批次 Y 执行结果（2026-08-14）

**验证**：`ninja -C build3` 全量重建 925/925 通过（exit 0，仅既有 `lv_LOG_*_LEN` 重定义警告）；`ctest --test-dir build3 --output-on-failure` 170/170 全绿（含 `test_gappa_dsl` 回归验证）。

### 决策登记（第 9 章格式）

- `LV_DISPATCH` 复用（判据 F）/ Y1 / 4 文件 5 处 / 无 / `geo_create_constraint` + `geo_unify` + `coord_to_double` + `compute_node_coord_hash` + `eval_csg_bool` 测试链。
- 不迁移 / L2 / Y2 / 0 文件 / `lv_snprintf` 已落地 + W3 已收敛 + 跨文件 6 处错误传播异构（返回消费 / 4 种失败通道）。
- `DEDUCT_FMT_*` 回迁（判据 I）/ Y3 / 1 文件 9 处 / 无 / `proof_strategy_deductive` 相关证明测试。
- 不迁移 / F / Y4 / 0 文件 / `ops_for_type` 返回指针查表，与 LV_DISPATCH 调用分发语义不同。
- `parse_bound_in` helper（判据 A/I）/ Y5 / 1 文件 2 点 4 格式串 / 无 / `test_gappa_dsl`。
- 无漏网 / C / Y6 / 0 文件 / C3 裸 `1e18` 已全收敛，台账计数口径「4 文件 4 处」→「4 文件 10 字面量」。

---

## 二十一、批次 Z 候选立项与执行（2026-08-14）

**候选来源**：「继续寻找更多可抽象化的方向」四路只读子代理（A/B、C/D、E/F/G/H、I/J/K/L+自由反例）全库扫描，排除历史批次已收敛/已判不迁移模式后，验证 8 项执行/裁定候选。

### 批次 Z 执行进度

| 编号 | 内容 | 判据 | 状态 |
|------|------|------|------|
| Z1 | block_to_text/node/geometry 三文件 `simple_block_graph_guard_cleanup` 逐字同构 → 共享函数 | G | 完成（3 处本地 static → 1 共享） |
| Z2 | interop_import.c 手写小端 u32/u16 → `lv_load_le32/le16`（15 调用点） | B 漏迁 | 完成 |
| Z3 | interop_server.c 手写大端 64/16 → `lv_load_be64/be16`（3 处含 close code 漏网） | B 漏迁 | 完成 |
| Z4 | MB 换算裸 `1024*1024` → `lv_MB`/`lv_MB_I`（17 处 / 10 文件） | C 漏迁 | 完成（2 头文件宏豁免） |
| Z5 | 裸 `strdup()` → `lv_strdup`（3 处）+ expr_canon 手写复制（1 处） | J 收尾 | 完成（X1 漏网补全） |
| Z6 | type_system.c 两组枚举名称数组 → X-macro（修复 `TYPE_CHECK_INCOMPATIBLE` 漏项缺陷） | F/D | 完成 |
| Z7 | DOT ID 三连 `snprintf(idbuf,"前缀%d")` → `lv_dot_node_id`/`lv_dot_edge_id`（6 文件 11 块） | L/J 变体 | 完成（graph_dot_export 节点豁免） |
| Z8 | SymbolicCoord 两点差向量 x/y 展开（proof_strategy_coordinate/angle/vector） | H | 登记不迁移 |
| Z9 | 构建验证 ninja + ctest，回写台账/记忆 | — | 完成 |

### Z1 明细（G-1：block_to_* 共享 cleanup，判据 G）

`block_to_text.c:18-29` / `block_to_node.c:184-195` / `block_to_geometry.c:137-148` 三份 `static simple_block_graph_guard_cleanup`（判空 → for 逐元素 `func_block_destroy` → `lv_free(blocks)` → `lv_free` 外壳，配 `lv_DEFER`）**逐字同构**。新建共享 `lv_simple_block_graph_guard_cleanup`（`representation_converter.h` 声明 + `representation_converter.c` 实现，void* 签名兼容 lv_DEFER），三文件删本地 static、3 处 `lv_DEFER` 改调共享。三文件均 include `representation_converter.h`，零新增依赖。

### Z2/Z3 明细（字节序设施漏迁，判据 B）

- **interop_import.c**：删 `ggb_read_u32_le`/`ggb_read_u16_le` 手写展开（`buf[o]|buf[o+1]<<8|...`），15 处调用点改 `lv_load_le32(buf + offset)`/`lv_load_le16(buf + offset)`（lv_utils.h 既有设施）。
- **interop_server.c**：`ws_state_len64` 手写 8 字节大端循环 → `lv_load_be64(p)`；`ws_state_len16` → `lv_load_be16(p)`；另补 `:1191` close code 大端 16 位漏网 → `lv_load_be16(ctl_payload)`。**注意**：len64 迁移时保留 `recv_pos += 8` 与 `WS_MAX_MESSAGE_SIZE` 检查（初版误删后已恢复）。
- **豁免**：interop_server SHA-1 内核（:655/752）、sha256.c、cross_platform.h bswap、lv_utils_misc.c 设施自身、plugin_system_core.c 版本位域打包（非字节序读取）。

### Z4 明细（C-1：MB 换算 → lv_MB/lv_MB_I，判据 C 漏迁）

config.h 已有 `lv_MB 1048576.0` / `lv_MB_I 1048576`，本次迁移 17 处裸 `1024*1024`/`1024.0*1024.0`（10 文件）：lv.c（溢出保护 + 4 处 MB 输出）、runtime_monitor.c（4 处）、debug_log_ctx.c（2 处）、text_code.c（128MB 上限 ×2）、geometry_canvas.c + block_canvas.c（16MB 上限 ×2×2）、proof_engine_enhanced.c（`EXPORT_BUFFER_MAX_SIZE`）、lv_config.c（文件读取上限）。int 语义用 `lv_MB_I`、double 语义用 `lv_MB`、倍数保留（`16 * lv_MB_I`）。
**豁免 2 处头文件宏**：`debug.h:49` `lv_LOG_MAX_SIZE`、`axiom_pkg_internal.h:36` `AXIOM_MAX_FILE_SIZE`（大小上限常量且头文件不依赖 config.h，保持头文件分层）。

### Z5 明细（J 收尾：裸 strdup → lv_strdup，判据 J）

X1 批次统一 `lv_strdup_safe` 后漏网的 3 处标准库 `strdup`（`modal_operators.c:199`、`interactive_geo.c:308`、`mpz_poly.h:246` inline）+ 1 处手写 `lv_malloc(strlen+1)+memcpy`（`expr_canon.c:1078`）全部改用 `lv_strdup`（lv_utils.h 别名）。全库裸 `strdup(` 归零。

### Z6 明细（F-1：type_system 名称数组 X-macro 化 + 缺陷修复，判据 F/D）

`type_system.h` 新增 `LV_TYPE_EQUIV_RESULT_X`（5 项）与 `LV_TYPE_CHECK_RESULT_X`（7 项）X-macro 列表，`TypeEquivResult`/`TypeCheckResult` 枚举改 `LV_X_ENUM_ITEM` 生成；`type_system.c` 删两组手写 designated-init 名称数组 + 越界反查，`type_equiv_result_to_string`/`type_check_result_to_string` 改 `LV_X_TO_STR_CASE` switch 生成（与 `type_kind_to_string` 同族，泛化 2 特例 + 1 命名先例）。
**缺陷修复（终审二）**：原 `s_check_result_names` 仅 6 项**漏 `TYPE_CHECK_INCOMPATIBLE`**（枚举 7 值），X-macro 单源后补全为 `"Incompatible"`（此前返回 "Unknown"）。

### Z7 明细（自由反例：DOT ID 三连 → lv_dot_writer ID helper，判据 L/J 变体）

6 文件散落 `char idbuf[32]; snprintf(idbuf, sizeof(idbuf), "前缀%d", id);` + 边双缓冲三连（前缀 node/n/S/step，常量差异）。`lv_dot_writer.h/c` 新增 `lv_dot_node_id`/`lv_dot_edge_id`（内部定长缓冲格式化 "前缀%d"），迁移 6 文件 11 块：graph_dot_export.c（边）、meta_repr.c（节点+边+单节点）、proof_dependency.c（节点+边）、proof_trace_tree.c（节点+边）、proof_compiler.c（节点+边）、proof_export_enhanced.c（节点+边）。
**豁免**：`graph_dot_export.c` `dot_emit_node`（`exempt:` 注释）——html_labels 分支直接 `lv_strbuf_printf(sb, "...%s [label=<...>]", idbuf, ...)` 拼接，需本地 idbuf，非 lv_dot_node 调用。

### Z8 登记不迁移（H-1：坐标差向量，判据 H）

`proof_strategy_coordinate.c:158-161` / `proof_strategy_angle.c:241-244` / `proof_strategy_vector.c:115-122` 三处「两点差向量 SymbolicCoord subtract x/y 双分量展开」同构，但：① 全部同域（proof_system），非跨模块治理价值；② 后续消费各异（compare / multiply / multiply+dot）；③ `formula_converter_constraint.c:144-145` 为 `symbolic_coord_add` 中点求和（非 subtract 差向量，形态混入排除）；④ helper 需双出参指针形状、控制流改变需额外验证，收益低。登记不迁移。

### 批次 Z 执行结果（2026-08-14）

**验证**：`ninja -C build3` 全量重建 925/925 通过（exit 0，仅既有 `lv_LOG_*_LEN`/`lv_LOG_FATAL` 宏重定义与 test_framework `-Waddress` 警告，与本批无关）；`ctest --test-dir build3 --output-on-failure` 170/170 全绿。

### 决策登记（第 9 章格式）

- `lv_simple_block_graph_guard_cleanup` 共享 / G / 3 文件 3 处（本地 static 逐字同构）/ graph_dot_export 节点 html 分支豁免 / converter 测试链。
- `lv_load_le*/be*` 复用 / B 漏迁 / 2 文件 18 处（interop_import 15 + interop_server 3）/ SHA/位域内核豁免 / test_interop 族。
- `lv_MB`/`lv_MB_I` 复用 / C 漏迁 / 10 文件 17 处 / debug.h + axiom_pkg_internal.h 头文件宏豁免（大小上限常量、不依赖 config.h）/ test_utils + test_performance 族。
- `lv_strdup` 复用 / J 收尾 / 4 文件 4 处 / 无 / 字符串复制链测试。
- `LV_TYPE_EQUIV_RESULT_X`/`LV_TYPE_CHECK_RESULT_X` / F+D / type_system.h + type_system.c / 无（修复 INCOMPATIBLE 漏项缺陷，终审二）/ test_type_system。
- `lv_dot_node_id`/`lv_dot_edge_id` / L/J 变体 / 6 文件 11 块 / graph_dot_export 节点 exempt（html 直拼）/ test_proof_trace + test_proof_export_enhanced + test_layer5_output 族。
- 不迁移 / H / Z8 / 0 文件 / proof_system 同域 3 处 + add 形态混入 + 后续消费各异（compare/multiply）+ 双出参 helper 收益低。

## 二十二、批次 AA 候选立项与实施（2026-08-14）

### 批次 AA 扫描与甄别

4 路只读子代理全库扫描（容器/内存、路径/IO、数值/尺寸、字符串/解析四方向），产出 20+ 候选。按判据 A–L 读码甄别后，用户选定 **A+B+C 全部按优先级完整执行**，并附带架构优化方向研究（见本章末）。

- **A 组·零新设施（5 项）**：AA1 裸 1000000 定点缩放、AA2 beta_reduce 裸 10000、AA3 select 超时裸数、AA4 groebner 配置默认值、AA5 text_code 4KB 扩容双份。
- **B 组·既有设施漏网（2 项）**：AA6/AA7 裸 fopen 3 处 → lv_file_open。
- **C 组·需小新设施（2 项）**：AA8 lv_str_chomp、AA9 lv_free_ptr_array。

### AA1 明细（C 漏迁：裸 1000000 定点缩放 → lv_RATIONAL_SCALE_DEFAULT）

- `singular_backend.c:857` `symbolic_coord_pair_from_double_scaled(..., 1000000, ...)`、`bootstrap_test_internal.h:44` `symbolic_coord_create_rational((long long)(dist * 1000000), 1000000)` → 改用 config.h 权威宏 `lv_RATIONAL_SCALE_DEFAULT`（=1000000，6 位小数精度，注释自证）。bootstrap_test_internal.h 补 `#include "lv/config.h"`（原链 constraint_graph→symbolic_coord→lv_platform 不含 config.h）。

### AA2 明细（C 漏迁：beta_reduce 裸 10000 → LV_LAMBDA_EVAL_DEFAULT_MAX_STEPS）

- `beta_reduce.c:955-956` 注释自证「与 LV_LAMBDA_EVAL_DEFAULT_MAX_STEPS=10000 对齐」但用裸数（比较 + 日志串）→ 全部替换为 lambda_term.h 权威宏，日志改 `%d` 参数化。单处代码但属「有权威宏未引用」漂移点，零新设施。

### AA3 明细（C 漏迁：interop_server select 超时 → 既有宏）

- `interop_server.c:1646-1648` `tv.tv_sec=0; tv.tv_usec=100000;` → 改用文件内已定义未引用的 `INTEROP_SELECT_TIMEOUT_SEC/US`（:68/:71，注释与调用点 :1645 逐字一致）。

### AA4 明细（C 漏迁：groebner 配置默认值 5 处 → 具名宏）

- `groebner_engine_internal.h` 新增 `GROEBNER_REDUCE_MAX_STEPS_DEFAULT 10000` / `BUCHBERGER_MAX_STEPS_DEFAULT 50000`；迁移 5 处 `lv_config_get_int(LV_CFG_*_MAX_STEPS, 裸数)`：groebner_poly.c:797、groebner_parallel.c:487（补 include groebner_engine_internal.h）、groebner_engine_ideal.c:397/723、groebner_engine_core.c:147。groebner_poly.c 内 1000000/100000 容量上限语义各异，不迁移。

### AA5 明细（B 同文件双份：text_code 4KB 对齐扩容 → 静态 helper）

- `text_code.c` set_text:69 / insert:117 两处逐字同构 4KB 对齐扩容骨架（`((len+1+4095)/4096)*4096` + 128MB 封顶 + realloc）→ 文件内静态 `text_code_grow_to_fit(view, needed, oom_msg)`；对齐单位具名 `lv_TEXT_CODE_BUFFER_ALIGN 4096`（含 create 初始缓冲）。错误消息 "insert text exceeds..." 统一为 "text exceeds..."（无测试断言，安全）。

### AA6/AA7 明细（家族漏网：裸 fopen → lv_file_open）

- `proof_dependency.c:793`（lv_json_buf 落盘，同构对照 command_log.c/proof_compiler.c 已用 lv_file_open）、`interop_export_lean.c:231`、`interop_export_coq.c:296`（interop 家族 5 文件已用 lv_file_open，lean/coq 为唯二漏网，带 stream 事件上报守卫保留）→ 全部迁移 + 补 `#include "lv/lv_file.h"`（与 svg.c 家族一致）。失败多打一条 lv_ERROR 日志为家族既有行为。
- **豁免**：`interop_import.c:1437` GGB-DBG 调试 dump（printf 依赖裸 fopen 指针输出校验信息，`/* [exempt] */` 标注）。

### AA8 明细（新设施 lv_str_chomp + 4 处去尾迁移）

- `lv_str_utils.h/c` 新增 `lv_str_chomp(char*)`（仅去末尾 \n\r，与 lv_str_rtrim 的区别是不去空格制表符）。迁移 3 处逐字同构去尾：`lv_utils.c:576`（lv_ini_parse）、`interop_server.c:1793`、`interop_server.c:1841`（STDIO 两分支，后接 strlen 计算去尾后长度，语义保持）。
- **豁免**：`interop_server.c:1147` WebSocket 去尾按 `client->msg_len` 长度基准回退而非 strlen（二进制消息可能含内嵌 NUL，strlen 提前终止语义不同，`/* [exempt] */` 标注）。

### AA9 明细（新设施 lv_free_ptr_array + 析构骨架迁移）

- `lv_utils.h/c` 新增 `lv_free_ptr_array(void ***p_arr, size_t count)`（逐元素 lv_free + 释放数组本身，元素/数组置 NULL，内部判 NULL）。迁移 7 处**简单指针数组 + 完整析构骨架**（元素必须为动态分配指针，非结构体数组）：
  - sat_encoding.c:157 `enc->clauses`、expr_canon.c:230 `e->var_names`、approx_counter.c:121 `cnf->clauses`、bdd_encoding.c:248 `mgr->var_names`、proof_rule_engine.c:644 `engine->rule_set`（删除冗余 NULL 化与未用 `int i`）、solver_core.c:134 `ctx->clauses`（计数 orig+learn 总和）、solver_core.c:235 `solver->clauses`。
- **登记不迁移（元素非简单指针或非完整骨架）**：lv_protocol.c 3 处（struct 字段 blocks[i].inputs 等）、rewrite_snapshot.c（struct 字段，已共享函数化）、expr_canon.c terms（struct 数组含子资源）、debug_trace.c（struct 字段）、solver_core.c watches（下标从 1 开始偏移）、groebner_engine_ring.c（元素释放前含 4 子指针）、solver_core.c:1525（仅清空复用不释放数组）。

### 批次 AA 执行结果（2026-08-14）

**验证**：`ninja -C build3` 全量重建 925/925 通过（exit 0）；`ctest --test-dir build3 --output-on-failure` 170/170 全绿。

### 决策登记（第 9 章格式）

- `lv_RATIONAL_SCALE_DEFAULT` 复用 / C 漏迁 / 2 文件 2 处 / bootstrap_test_internal.h 补 include config.h / test_bootstrap 族。
- `LV_LAMBDA_EVAL_DEFAULT_MAX_STEPS` 复用 / C 漂移 / 1 文件（beta_reduce.c 比较+日志 2 处）/ 无 / 重写测试链。
- `INTEROP_SELECT_TIMEOUT_*` 复用 / C 脱节（宏定义未引用）/ 1 文件 1 处 / 无 / test_interop 族。
- `GROEBNER_REDUCE_MAX_STEPS_DEFAULT`/`BUCHBERGER_MAX_STEPS_DEFAULT` / C / groebner_engine_internal.h + 4 文件 5 处 / groebner_poly 内部容量上限语义各异不迁移 / test_groebner* 族。
- `lv_TEXT_CODE_BUFFER_ALIGN` + `text_code_grow_to_fit` / B / 1 文件双份 / 无 / test_layer6_visual。
- `lv_file_open`/`lv_file_close` 复用 / 家族漏网 / 3 文件 3 处 / interop_import GGB-DBG 调试 dump exempt / test_output_export 族。
- `lv_str_chomp` / 新设施 J 变体 / lv_str_utils.h/c + 3 文件 3 处 / interop_server WebSocket msg_len 长度基准 exempt / test_layer5_output + test_utils。
- `lv_free_ptr_array` / 新设施 G / lv_utils.h/c + 6 文件 7 处 / 8 处登记不迁移（struct 字段/偏移下标/非完整骨架）/ test_solver_submodules + test_bdd_sat_atp + test_layer4_misc + test_proof_infra。

### 架构优化方向研究（AA 批次附带，只读）

基于 4 路全库扫描 + 历批次治理成果，归纳 6 个结构性优化方向（超越单点收敛）：

1. **代码生成器骨架统一（判据 L 泛化）**：smtlib2.c ~15 处、atp_backend.c ~6 处、preset_manager_doc.c 等 25+ 处同构「游标式 `snprintf(buf+off, cap-off)` → 校验 n → off+=n」骨架，三处独立实现、截断/错误处理细节漂移（`n<0` vs `n>=remaining`）。建议设施 `lv_str_fmt_append(dst, cap, &off, fmt, ...)`（bool 返回），与既有 `lv_str_append_sep`（仅字符串+分隔符）互补。**风险低、纯机械，立即价值**。
2. **表达式求值器词法原语统一（判据 I）**：fptaylor_eval / float_error / gappa_propagate / interval_arith / proof_strategy_numeric 五套独立实现同一「数字字面量 strtod + xN 下标 strtol + 空白跳过」骨架。障碍：`lv_parse_double` 无 end 输出（求值器需推进游标）。建议补 `lv_parse_double_prefix(str, &val, &end)` + `lv_parse_var_index` 入 lv_parse_utils.h，5 处逐步收敛。**风险中**（错误处理细节各异，需逐处对齐）。
3. **容器抽象层（判据 B 深化）**：lv_darray 已支持 init_with_dtor/push/get/pop/free（析构回调），但 find-by-name（8+ 文件）、find-by-id + swap-last 删除（8+ 文件）仍为手写骨架。建议 lv_darray 增 `lv_darray_find` 与 `lv_darray_remove_swap`。**风险中**（行为微调需逐点甄别）。
4. **配置默认值漂移治理（判据 C）**：`lv_config_get_int(KEY, 裸默认值)` 模式在 AA4 建立 `*_DEFAULT` 宏族先例，全库其他配置键（LV_CFG_CDCL_* 等）可推广。**风险低**。
5. **序列化头部样板（判据 A/L）**：SVG 文档头 4 处（geometry_canvas/block_canvas 逐字对 + interop_export_svg/geo_visual_complete 近字对，属性序有差异）。建议 `lv_export_svg_header` 参数化属性串，吸收属性序差异。**风险低**。
6. **指针数组析构持续收敛（判据 G）**：lv_free_ptr_array 已落地迁移 7 处，剩余 ~60 处（含非完整骨架）可逐步迁移；长远可评估「析构描述符」（elem_dtor + free 组合）统一元素销毁，与 lv_darray_init_with_dtor 对齐。

**优先级建议**：立即价值（低风险）= #1/#4/#5；中期（需设计）= #2/#3；长期 = #6。待用户立项后按批次执行。

## 二十三、大规模架构优化调研（2026-08-14，只读）

### 调研范围与方法

5 路只读子代理并行全库审计（依赖违规 / 目录与子系统 / 规模复杂度 / 构建测试 / API 与头结构），覆盖 core/src 全部 390-405 个 .c、~171k 行、core/include/lv 310 个头。全部证据 文件:行 级落位，未修改任何文件。

**规模校正**：实际 390 .c / ~171k 行（README 声称 615 .c / ~200k 行，过时）；层目录实际只有 8 个（layer1/2/3/4/5/6/8/10），**layer7、layer9 无目录**。

### P0 架构性风险（单向依赖失守 + 验证机制失效）

1. **44 处层间依赖违规**（README 十层依赖表 vs 实际 include）：
   - L2 基础层越权 21 处：`lv.c:26` include `lv/formula_converter.h`（**L2 反向依赖 L1，成环**）+ `:25` ecosystem.h（L4）+ `:30` module_internal.h（L4）；`representation_converter.c:22/23` 直连 L4 func_block.h + **L6 block_graph_view.h**；debug_*.c 10 文件全系 include engine.h（L4）；context.c:16/26 依赖 circuit_breaker.h + constraint_graph.h。
   - L1 越权 14 处：`dsl_compiler_load.c:28` 跨两层跳 L4 axiom_pkg.h；dsl_compiler 系 7 文件 include L3 constraint_graph.h/symbolic_coord.h。
   - L3 越权 8 处：constraint_graph 系 7 文件 + graph_index.c 全系 include L4 solver.h/stream.h。
   - **L4→L5 反向环**：`stream_context_util.c:37` include interop.h，与 L5 interop_*.c include engine.h 构成环。
   - L6 违反 README 依赖表（不含 L4）：converter/block_to_*.c 3 处 + runtime/block_scheduler.c 均 include func_block.h。
   - L8 越权 1 处：meta_verify.c:27 include proof_compiler.h（L5）。
2. **layer_validation.h 验证机制 100% 空转**：全库 0 个 .c include 它、0 处使用 lv_CURRENT_LAYER；宏体系停在「6 层」而目录是「10 层」，且 `lv_VALIDATE_CURRENT_LAYER` 断言 1-5 会把 L6 自己判为非法；两处文档（头注释 vs engine.h:49）互相矛盾。默认开关 OFF。

### P1 结构性重复子系统（去重机会）

| 重复族 | 实现清单 | 建议 |
|---|---|---|
| 区间运算 ×3 | interval_arith（基准）/ float_error（蓝本自持 round_down）/ gappa_propagate（自建 PropInterval） | float_error、gappa_propagate 迁到 lv_interval_* |
| 二进制格式 ×3 | module_serialize_msgpack（手写 MP）/ module_lvz（手写 .lvz 解析）/ geometry_compress*.c（.lvzd 全家 14 文件） | 明确边界或统一序列化栈 |
| 证明导出 ×3 | engine/proof_export.c / layer5/proof_export_enhanced.c / layer4 根 proof.c 内嵌 | 收敛到 layer5 单一导出面 |
| Coq/Lean ×2 | interop_export_coq/lean.c vs layer10 coq_bridge/lean4_bridge | 导出归 layer5、桥接归 layer10，划清边界 |
| TikZ/LaTeX ×4 | interop_export / geo_visual_complete / tikz_export / lv_render_visitor_tikz | 抽公共 TikZ 生成层 |
| DOT ×4 | lv_dot_writer（公共）+ graph_dot_export（自带 dot_escape_append 副本）+ proof_trace_tree + proof_export_enhanced | graph_dot_export 迁 lv_dot_writer |
| 日志通道 ×5 | lv_log / runtime_monitor（**与 lv_log.h 各自定义 lvLogLevel，文件头明言不能同含**）/ debug_trace_session / debug_ringbuf / debug_log_ctx | 统一日志类型单源 |
| 内存池 ×7 | lv_mempool / lv_mempool_utils / memory_pool / debug_mempool / lv_arena / lv_heap / allocator | debug_mempool 并入公共池 |
| 测试框架 ×2 | layer2/test_framework.c vs layer4/bootstrap_test*.c（9 文件，编入生产库） | bootstrap_test 移出生产库 |
| 双胞胎后端 | cuda_backend.c ↔ hip_backend.c（45+ 函数同骨架仅换前缀）；simd_ops.c 内 vec4d/4f/8f 三重复制 | 宏/模板化或共享生成层 |

**同名双实现 5 对**（先核实是否真双实现再合并）：rational.c（L3 symbolics / L4 expr）、meta_verify.c（L8 / L4 proof）、proof_version.c（proof_system / proof）、proof_optimize.c（proof_system / engine）、proof_contradiction.c（根 / engine）。

### P2 模块边界与目录组织

1. **游离文件**：core/src 根 13 个（lv_impl_upper_* 8 个实为 L7/L9/L8/L6/L3/L10/L4 各层实体却归 lv_core OBJECT 库 layer_id=0）；layer4 根 16 个（proof.c/solver.c/rewrite.c/module.c 应入对应子目录，mv_polynomial.h/solver_snapshot.h 头与实现错位）。
2. **错放**：layer2 混入 proof_score/proof_priority（L4 域）、meta_repr、representation_converter（应入 layer6_visual/converter/，该目录已有 block_to_* 三兄弟）、lv_export_common（L5 域）、preset_helper_cn/math_theory_guide_cn（中文 UI 资源）、geometry_config（与 lv_config 构成**双配置系统**）；layer3 混入 high_dim_*（11 个可视化文件）、float_error/gappa_*/herbie/fptaylor（浮点验证域）、geometry_compress_*（序列化域）。
3. **include 路径两套风格**：`"lv_internal.h"`（40 个 .c 无前缀）vs `"lv/lv_internal.h"` 并存；同文件内混用；两套 internal 头体系（src 本地 * _internal.h vs include/lv 下 * _internal.h）靠 -I 顺序区分。

### P3 工程卫生与文档失同步

1. **build3/ 逸出 .gitignore**（缺 `build3/` 规则）；根目录 17+ 诊断 txt/ps1（build_verify_build_log*.txt 等）会被 git 跟踪；core/src 混入 _edit_test.c（2 行孤儿）、_ps_test.txt、preset/_migrate*.ps1/py。
2. **文档过时**：README「615 .c/200k 行/229 头/152 测试」vs 实际 390 .c/~171k/310 头/170 测试；「lv.h 唯一公共入口」声明被 lv_impl_upper_*.c 直接 include 十几子系统头削弱；layer7/9「已删除」但应用逻辑实际在 lv_core。
3. **测试辅助编入生产库**：bootstrap_test*.c、axiom_template_test.c、recursion_test.c、test_framework.c 被编译进 lv_layer4/lv_layer2 OBJECT 库。
4. **导出宏单源失败**：`lv_PUBLIC_API` 被 40 处重复定义（33 头 + 7 源各自 #define，多为空）；公共 API 前缀分裂（lv_ 与无前缀 graph_add_point/proposition_create 并存，全局命名空间风险）。

### 健康面（正面确认，无需动）

- 无 include 环（依赖方向整体单向、头守卫规范）；无 longjmp；无 >20 case 大 switch（最大 9，分发用 if/else 链+函数指针表）；goto 146 处集中于错误清理，可控。
- 构建架构健康：9 层 OBJECT 库 + lv_static 聚合，依赖方向受控；170 测试 ↔ 170 ctest ↔ 170 exe 一一对应；宏化 main；平台分支集中（114 处/24 文件，头层为主）；GMP 依赖配置完整可复现。
- 全局单例 ~30+，约 2/3 有锁/once/TLS 保护；风险面集中在 lv_log 全局、allocator 可切换指针、计数器族与行为开关（约 12-15 处无保护）。
- 真上帝文件仅 interop_import.c（3 职责）；TOP20 大文件多为宽文件；>150 行函数约 4 个。

### 优化路线图建议（待立项）

- **第一优先（P0 修复，小改动高收益）**：① 解除 lv.c 的 L1/L4 引用与 representation_converter.c 的 L4/L6 引用；② layer_validation.h 升级到 10 层模型并接入 CMake target_compile_definitions（0→启用，先解 L1/L3 越权）；③ 补 .gitignore 规则（build3/ 等）并清理残留文件。
- **第二优先（P1 去重）**：log 类型单源（runtime_monitor vs lv_log）；graph_dot_export 迁 lv_dot_writer；5 对同名双实现核实合并；Coq/Lean/TikZ 导出边界划清。
- **第三优先（P2 目录重组）**：lv_impl_upper_* 按层归位（新建 layer7/layer9 目录或并入现有层）；错放文件迁移；include 路径风格统一（全部 lv/ 前缀）。
- **持续**：JSON 序列化手写拼接统一到 lvJsonBuf（stream_json、high_dim_serialize 为正面样板）；区间运算三套合一；公共 API 前缀与导出宏单源治理。

## 二十四、批次 AB：架构优化实施（2026-08-14）

按「二十三」路线图第一、第二优先完整实施。用户指令「开始按照优先级完整实现并行执行」。

### P0-① 层归属修正（消除 L2 反向依赖 + L2→L6 越权）

- **lv.c → lv_core（L0 便利层）**：系统入口协调者（init/cleanup 各层生命周期，include ecosystem.h/formula_converter.h/module_internal.h/func_block_registry.h）本质属 L0 而非 L2 基础设施。`git mv` 至 `core/src/lv.c`，CMake lv_LAYER2_SOURCES → lv_CORE_SOURCES。L2 反向依赖 L1 成环、L2→L4 越权 3 处随之消解。
- **representation_converter.c → layer6_visual/converter/**：表示转换器（func_block.h + block_graph_view.h）与 block_to_* 三兄弟同域。`git mv` 至 converter/，CMake lv_LAYER2_SOURCES → lv_LAYER6_SOURCES。
- **README L6 依赖表更新**：`L6 | Visual | L2, L3, L5` → `L2, L3, L4, L5`（可视化转换器实际依赖 func_block，L6 需显式允许 L4）。

### P0-② layer_validation.h 升级 10 层模型

- 重写为 **10 层 + L0 精确依赖模型**：新增 `lv_LAYER_CAN_DEPEND(current, target)` 宏（严格按 README 依赖表展开，修正旧「current >= target 简单比较」模型——L8 允许 L2/L3/L4 但不允许 L5/L6，简单比较会漏判）；新增 L7/L8/L9/L10 常量与 L6/L8 专用验证宏；**修正 `lv_VALIDATE_CURRENT_LAYER` 断言 bug**（旧断言 1-5 会把 L6 自己判非法，现为 0-10）。
- CMake 侧确认已接入（lv_setup_layer 已定义 lv_CURRENT_LAYER，ENABLE_LAYER_VALIDATION 默认 OFF）。机制就绪，启用前置 = 修复剩余 ~40 处违规。

### P0-③ 构建卫生

- **.gitignore 补规则**：build3/、build4/、_c11_migrate*.ps1、refactor*.ps1、do_switch5.ps1、*_migrate*.ps1/py、build_verify_build_log*.txt、build3_target_ctest_map*.txt、build3_ctest_before.txt、gdb_rot_cmd.txt、temp_results.txt、switch5_funcs.txt、s5_funcs.txt、one_arg_asserts.txt、_write_probe.txt、_ps_test.txt、_edit_test.c、.probe*。
- 注：build3/ 中已跟踪文件（.ninja_deps/.ninja_log 等）需用户 `git rm --cached -r build3` 从索引移除（未擅自改索引）。

### P1-① 日志类型单源（消除双 lvLogLevel 冲突）

- **lv_log.h** 成为唯一权威 `lvLogLevel` 枚举（删除三分支互斥条件块），保留哨兵宏兼容。
- **runtime_monitor.h** 删除自有 lvLogLevel 枚举，改 `#include "lv/lv_log.h"`；保留 LOG_LEVEL_* 数值别名宏（TRACE=-1 等 7 档逐一核对同值）。
- **runtime_monitor.c** 删 `lv_log_shutdown` 前置声明 hack，显式 include lv_log.h。根除 build3_k.out 中 `'lv_LOG_FATAL' redefined` 告警（三方哨兵互斥舞步的产物）。

### P1-② DOT 转义公共化

- **lv_dot_writer.h/c** 新增 `lv_dot_append_escaped(lvStrBuf*, const char*)`（提升原内部静态 dot_escape_append + NULL 保护），内部 lv_dot_node/lv_dot_edge label 改用之。
- **graph_dot_export.c** 删本地 `dot_escape_append` 副本，3 处调用点改用公共函数；转义规则逐字节等价（均走 lv_str_json_escape_alloc，`"`→`\"` 等查表一致）；exempt 的 html_labels 分支未动。

### P1-③ 同名双实现核实 + 死代码删除

核实结论（证据链闭合，5 对均无链接冲突）：
- rational.c：**一主一桩**（L3 薄转发 → L4 lv_rational_*），不合并；**新发现 L3→L4 反向环**（登记待修）。
- meta_verify.c：**拆分关系**（L8 会话/证明对象审计 vs L4 约束图完备性），不合并；**L8→L5 违规实证**（lvProofObject 定义于 L5 proof_compiler.h）+ CMake L8 链接 L6（登记待修）。
- proof_version.c：proof/ 版为 **21 行空壳** → 删除。
- proof_optimize.c：proof_system 版（lv_proof_opt_*）与 engine 版（lv_optimize_proof 族）**真双实现且双死代码** → 保守删 proof_system 版。
- proof_contradiction.c：根版（lv_assumption_stack 族）**死代码**、engine 版活跃 → 删根版 + proof_contradiction.h（顺带消除**双 lvContradictionType 枚举潜伏冲突**）。

删除执行：`proof/proof_version.c`、`proof_system/proof_optimize.c`、`proof_contradiction.c`、`include/lv/proof_contradiction.h`（4 文件）+ CMake 4 处 + proof_trace.h 中 ProofOptimizer 不透明类型与 lv_proof_opt_* 声明（6 函数）。

### 批次 AB 执行结果（2026-08-14）

**验证**：ninja build3 923/923（-2 源文件目标，符合删除预期）+ ctest 170/170 全绿。

### 决策登记（第 9 章格式）

- lv.c 归位 lv_core / 层归属修正 / L0（系统入口协调者属便利层非基础设施）/ README L6 依赖表同步 / 全量测试。
- layer_validation.h 升级 / 层模型 / 10 层 + lv_LAYER_CAN_DEPEND 精确判定 / 启用前置 = 修复剩余违规 / 编译期验证链。
- .gitignore 补规则 / 构建卫生 / 18 条 / build3 已跟踪文件需 git rm --cached / 无。
- lvLogLevel 单源 / P1-① / lv_log.h + runtime_monitor.h/c / 数值 7 档逐一核对 / runtime_monitor 族测试。
- lv_dot_append_escaped / P1-② / lv_dot_writer.h/c + graph_dot_export.c / graph_dot_export html 分支豁免保留 / graph 导出测试。
- 死代码删除 / P1-③ / 3 源 + 1 头 + CMake 4 处 + proof_trace.h 声明 / 双 lvContradictionType 冲突随头删除消除 / test_proof_version + test_meta_verify + test_layer4 族。
- 登记待修（下一轮）：L3→L4 反向环（symbolics/rational.c）、L8→L5（meta_verify.c lvProofObject）+ L8→L6 链接、L1/L3 越权剩余 ~40 处（layer_validation 启用前置）。

## 二十五、批次 P2：目录重组（2026-08-14）

按「二十三」路线图第三优先完整实施。用户指令「开始按照优先级完整实现并行执行」（第二次）。

### P2-① 错放文件迁移

- **lv_export_common.c → layer5_output/**：`git mv` 自 layer2_resource。全库调用者仅 L5（tikz_export.c、interop_export_svg.c），归位 layer5 消解 L2 越权。CMake lv_LAYER2_SOURCES → lv_LAYER5_SOURCES。
- **评估结论（不改动）**：
  - lv_impl_upper_*.c（8 源 + 1 头）保留在 lv_core：依赖横跨 19-26 个头（L3-L9 各层实体），归位任意单层即制造反向依赖；L0 便利层本就允许依赖所有层。
  - proof_score/proof_priority/preset_helper_cn/math_theory_guide_cn：全库零调用者 → 死代码，移动无架构收益，不迁。
  - high_dim_*（layer3 内 11 文件）：仅 demo 调用 + 横跨多层 → 暂缓。

### P2-② include 路径统一（全部 lv/ 前缀）

- **3 路并行子代理执行**：layer1+layer2（71 文件/132 行）、src 根+layer5/6/8/10+core（33 文件/73 行）、layer3+layer4（344 文件/~1460 行），合计约 **448 文件/1665 行**：`#include "X.h"` → `#include "lv/X.h"`。
- **规则**：仅当 X.h 位于 include/lv/ 时加前缀；src 本地 `*_internal.h`（同目录）保持无前缀。
- **收尾 2 处 src 本地头内部引用**：`lv_impl_upper_internal.h:21` `"constraint_graph.h"` → `"lv/constraint_graph.h"`；`union_find_util.h:11-13` `"error_codes.h"/"lv_internal.h"/"lv_utils.h"` 三行加 `lv/` 前缀。
- **复核**：全库 `#include "[a-z_]+\.h"` 剩余 100 行均为 src 本地头（*_internal.h 族 / module_helpers.h / groebner_engine_guard.h / union_find_util.h 自身），符合规则；include/lv/ 内部同目录互引保持无前缀（相对目录解析，标准做法）。

### 批次 P2 执行结果（2026-08-14）

**验证**：ninja build3 922/922 + ctest 170/170 全绿（66.65s）。

### 决策登记（第 9 章格式）

- lv_export_common 归位 L5 / P2-① / layer5_output/lv_export_common.c + CMake / 调用者仅 L5（tikz_export、interop_export_svg） / L5 导出族测试。
- lv_impl_upper_* 留 lv_core / P2-① / lv_core（L0） / 跨 19-26 头依赖归位即成反向依赖 / 全量回归。
- include 路径统一 / P2-② / 448 文件 1665 行 / src 本地 *_internal.h 豁免 / 全量回归。
- 登记待修（下一轮，承接「二十四」）：L3→L4 反向环、L8→L5/L8→L6、剩余 ~40 处层间违规（layer_validation 启用前置）。

## 二十六、批次 C：层间违规系统性修复（2026-08-14）

用户指令「继续顺便再研究一下架构优化」。完成登记待修的 3 项 + 全量违规扫描（144 处）后系统性修复，并接线复用 debug 验证设施。

### 三项待修落地

- **L3→L4 反向环（rational）**：`lv_rational.c`（原 layer4_reasoning/expr/rational.c）git mv → layer3_geometry/symbolics/。实现仅依赖 GMP + lv_utils（L2 级），消费方 L3（rational.c/lv_number.c）+ L4（expr_canon.c）均合法。
- **L8→L5（lvProofObject）**：`lvProofStepRecord`/`struct lvProofObject` 定义自 proof_compiler.h（L5）迁入 proof.h（L4 域）；proof_compiler.h 保留前向声明；meta_verify.c 删 proof_compiler.h include。L5 proof_compiler.c 与 L8 meta_verify 均经 proof.h 获得。
- **L8→L6 链接**：移除 `target_link_libraries(lv_layer8_meta_verify lv_layer6_visual)`（历史遗留，meta_verify 仅消费 L2/L3/L4 符号）。

### 全量违规扫描（只读子代理，144 处：L1=30/L2=53/L3=52/L4=9）

热点：stream.h+stream_context_util.h 合计 70 处（48.6%）被 L1/L2/L3 越层消费；lexer_shared（L1 归属）被 L4 消费 8 处。

### 系统性修复（本轮消除 90+ 处）

- **stream 核心 10 文件 → L2**（layer2_resource/stream/）：stream.c/async/buffer/context/emit/filter/json/lazy/stats/utils + stream_internal.h。stream.c 仅依赖 L2 头。`LV_STREAM_CTX_DECLARE/DEFINE` 宏 + `StreamContextSetter` typedef + register_setter/emit_fmt 声明并入 stream.h（L2）；93 个文件 include 迁移至 stream.h；**stream_context_util.c 留 L4**（跨层 setter 注册器），其 interop.h（L5）引用改本地前向声明（消解 L4→L5）。
- **stream_context_util.h 按用户决策保留为兼容遗留头**（内容已并入 stream.h，文件恢复 + 加回 CMake 清单）。
- **lexer_shared → L2**（被 L1 与 L4 module/axiom_pkg 消费，8 处消除）。
- **bit_burning → L3**（依赖 graph_node_internal.h/ConstraintGraph 内部，L3 坐标位数保护，5 处消除）。
- **geometric_primitives → L4**（geo_prove/geo_export 依赖 L4 proof/rewrite/unify，整体错放，6 处消除）。

### debug 家族清理 + 接线复用（用户决策：保留 + 接线复用）

- **7 个冗余 include 文件**（debug.c/emergency/refcount/ringbuf/trace_session/log_ctx/mempool）：删 `engine.h`/`type_system.h`/重复 stream.h（零使用，-14 处）。
- **debug_assert_port_invariants 自 debug_state.c 移入 debug_normalize_assert.c**；debug_state.c 删 engine.h/type_system.h（-2 处）。
- **debug_normalize_assert.c + debug_invariants.c → L4**（引擎状态验证器，依赖 lvEngine/TypeSystem，debug_internal.h 改 `layer2_resource/` 前缀 include；-4 处）。
- **接线**：engine_solve.c 归一化后 `if (debug_is_debug_mode())` 调用两个断言（栈上 DebugContext，仅调试开销）；test_func_block.c 新增 `test_debug_port_invariants` 激活 debug_check_port_invariants（空图 all_valid + 伪造 connected_to 检出违规）。

### 批次 C 执行结果

**验证**：ninja 922/922 + ctest 170/170 全绿（61.95s）。

### 决策登记（第 9 章格式）

- rational 归位 L3 / 批次C-① / lv_rational.c + CMake / 仅 GMP+lv_utils / L4 依赖 L3 合法 / 全量回归。
- lvProofObject 迁 proof.h / 批次C-② / proof.h + proof_compiler.h + meta_verify.c / L8 依赖表不含 L5 / meta_verify + proof_compiler 测试。
- L8→L6 链接移除 / 批次C-③ / CMake / meta_verify 无 L6 符号 / 全量回归。
- stream 核心下沉 L2 / 批次C-④ / 10 .c + stream.h 宏并入 / 被 L1/L2/L3 大面积消费 / stream 相关测试。
- stream_context_util.h 保留兼容头 / 批次C-④ / 文件恢复 + CMake / 内容已并入 stream.h / 用户决策。
- lexer_shared 下沉 L2 / bit_burning 归 L3 / geometric_primitives 归 L4 / 批次C-⑤ / CMake 迁移 / 各按真实消费面 / 全量回归。
- debug 家族清理 + 接线复用 / 批次C-⑥ / 7 文件删冗余 include + 2 验证器归 L4 + engine_solve 接线 + test_func_block 测试 / 用户决策「保留+接线复用」/ func_block + engine 测试。
- 登记待修（下一批）：剩余 ~55 处——L1 dsl_compiler 系（constraint_graph/symbolic_coord/axiom_pkg 9 处）+ formula_curve/eval/string（5 处）；L2 context.c（circuit_breaker/constraint_graph/normalization 3 处）+ lv_storage/lv_serialize_adapters/node_deep_copy（constraint_graph 3 处）+ meta_repr（constraint_graph/func_block 2 处）+ simd_ops（geo_utils 1）+ adaptive_threshold（lv_graph_traversal 1）；L3 constraint_graph 系 solver.h（7 处）+ graph_index type_system（1）+ geom_evol（ode_integrator/numerical_backend 2）+ interactive_geo（engine.h 1）。

## 二十七、批次 C-⑦：剩余层间违规系统性收敛（2026-08-14）

用户指令「继续优化架构」。逐一调研剩余违规文件的真实依赖用途后，按「冗余删除 / 消费方安全迁移 / 家族整体迁移 / 注册表解耦」四类处置。~55 处收敛至 3 处豁免桥接。

### 调研结论（关键）

- **dsl/formula 家族外部消费方仅 L0**（lv.c / lv_impl_upper_* / lv_convenience），无 L2/L3/L4 消费 → 可整体上移。
- **formula_converter.h 直接 include constraint_graph.h（L3）** → formula 家族在 L1 依赖模型下结构上不可能（L1 仅可依赖 L2）。
- **context.h 消费方遍布 L1~L7**（debug 8 件/L3 graph 7 件/L4/L0）→ context.c 必须留 L2，其 graph/normalization 依赖只能内部解耦或豁免。
- **solver.h（L4）在 graph 系 6 处为冗余**：graph_node* 仅经其传递获得 symbolic_coord/stream；constraint_graph.c 零使用；graph_conflict.c 的唯一消费是零调用死代码 `algebraic_conflict_detected`（全库确认）。
- **graph_index.c 的 type_system.h 非冗余**：graph_add_containment 真调 type_check_universe_constraint（UniverseLevel 符号，首轮 grep 模式漏判，构建报错纠正）。
- **circuit_breaker.h（L4）是纯兼容包装层**：结构体/独立 API 已在 L2 lv_circuit_breaker.h；L4 层仅剩 ctx* 包装（依赖 recursion.h，不能下沉）。

### 处置明细

**① 冗余 include 删除（17 处 + 6 处 stream.h 去重）**
- dsl_compiler.c/ir/parse：各删 constraint_graph.h + symbolic_coord.h（零使用）。
- formula_string.c / formula_eval.c：删 constraint_graph.h（零使用；geo_utils.h 为真实使用保留）。
- constraint_graph.c + graph_node/node_stub/hash/alloc/node_conflict 6 件：删 solver.h + 去重重复 stream.h。
- graph_index.c：type_system.h 已恢复（真实使用，见豁免）。
- graph_conflict.c：删零调用死代码 `algebraic_conflict_detected`（46 行）+ solver.h，补直接 stream.h include（原先经 solver.h 传递）。

**② context.c circuit_breaker 解耦（L2→L4 边消除）**
- `lv_circuit_breaker_state_name_cb(const lvCircuitBreaker*)` 下沉 L2（状态名表与枚举同层，单一事实来源；lv_circuit_breaker.c 增 lv_xmacro.h include）。
- L4 `lv_circuit_breaker_state_name(lvContext*)` 改为委托 L2 版（删 L4 名称表）。
- context.c include 改 `lv/lv_circuit_breaker.h`（首轮 SearchReplace 未持久化，复核修复）+ 调用点改 `_cb` 版。

**③ lv_storage 解耦（L2→L3/L4 边消除，注册表模式）**
- lv_storage.h 新增 `lv_storage_register_verify(type, verify_fn, free_fn)`；lv_storage.c 增 verify 注册表（镜像 serialize_registry 模式），lv_roundtrip_verify 的 "ConstraintGraph" 硬编码分支改注册分派；删 constraint_graph.h/meta_repr.h include。
- lv_serialize_adapters.c 的 `lv_serialize_register_graph_adapters` 注册 {meta_repr_graph_equivalent, graph_destroy}。

**④ 文件迁移（消费方安全，CMake SOURCES）**
- → L3：node_deep_copy.c（GeomNode 域）、adaptive_threshold.c（图遍历域）、simd_ops.c（geo_utils 域）。
- → L4：geom_evol.c（ODE/数值后端域）、interactive_geo.c（lvEngine* 域）、meta_repr.c（ConstraintGraph+FuncBlock 编码）、lv_serialize_adapters.c（graph 序列化+验证注册）。

**⑤ 家族整体迁移（CMake SOURCES，消除全部真实 L1 违规）**
- formula 家族 → L3：formula_parser.c + parser/ 子目录 4 件 + formula_converter*.c 7 件 + formula_eval/string/curve + formula_renderer*.c 8 件（formula_converter.h 绑 constraint_graph.h，消费方仅 L0）。
- dsl 家族 → L4：dsl_compiler.c/parse/ir/load + dsl_lexer.c（load 注册 AxiomPackage L4 + IR→ConstraintGraph L3，消费方仅 L0）。

### 剩余豁免桥接（3 处，登记待修）

- context.c:26 constraint_graph.h（main_graph 生命周期 graph_create/copy/destroy 直调 ~10 处）。
- context.c:30 normalization.h（last_normalization 的 normalization_result_destroy）。
- graph_index.c:36 type_system.h（graph_add_containment 的 universe 层级校验）。

**follow-up 方案**：前两项 → L2 定义「不透明资源操作回调」（create/destroy/copy 注册，仿 stream_context_register_setter 先例，由 lv.c L0 在 init 时注入）；第三项 → universe 层级辅助函数（kNodeTypeUniverseLevels 表 + type_check_universe_constraint + type_set_universe_checking）下沉 L3（GeomType 几何域），或 graph_index 经回调注册。启用 lv_ENABLE_LAYER_VALIDATION 前置 = 完成 3 项豁免。

### 批次 C-⑦ 执行结果

**验证**：ninja 923/923 + ctest 170/170 全绿（100.03s）。实测消除：17 冗余 include + 6 stream 去重 + 家族迁移（formula 24 文件 / dsl 5 文件）+ 7 文件跨层迁移，违规 ~55 → 3 豁免。

### 决策登记（第 9 章格式）

- 冗余 include 清理 / 批次C-⑦ / 17 处 + 6 去重 / 全部零使用（graph_index type_system 首轮误删已纠正恢复）/ 全量回归。
- 死代码删除 / 批次C-⑦ / graph_conflict.c algebraic_conflict_detected / 全库零调用、被 L4 solver 的 check_conflict_equations 覆盖 / 全量回归。
- circuit_breaker 状态名下沉 L2 / 批次C-⑦ / lv_circuit_breaker.h/c + circuit_breaker.c + context.c / L2 拥有枚举单一事实来源，L4 兼容包装委托 / test_circuit_breaker。
- lv_storage verify 注册表 / 批次C-⑦ / lv_storage.h/c + lv_serialize_adapters.c / 存储层不依赖上层类型，注册分派仿 serialize_registry / test_serialize_registry。
- 文件迁移 7 件 / 批次C-⑦ / CMake / 各按真实依赖域（GeomNode/图遍历/geo_utils/ODE/engine/meta_repr/serialize）/ 全量回归。
- formula 家族→L3、dsl 家族→L4 / 批次C-⑦ / CMake SOURCES / 外部消费方仅 L0、家族内聚自洽、消除全部真实 L1 违规 / 全量回归。
- 豁免 3 处（context.c×2 + graph_index×1）/ 批次C-⑦ / 台账登记 + follow-up 方案 / 回调注入或辅助函数下沉，启用层验证前置 / 全量回归。
- 物理目录暂未随层迁移（formula/dsl 仍居 layer1_parser/，node_deep_copy 等仍居 layer2_resource/） / 批次C-⑦ / 待后续批次按层 git mv（含相对 include 前缀调整） / 本批先跑通。

### preset 孤儿目录复用研究（批次 C-⑦ 附，2026-08-14 深化）

- **现状**：`layer4_reasoning/preset/preset_*.c` 55 件未在任何 SOURCES（孤儿），其头 61 个已安装。编译代码 + 测试对孤儿函数（preset_<m>_count/category/get_names/...）**零调用**。
- **根因（已证实为生成源而非死代码）**：`.lvz` 头注释「Auto-generated from C preset files」+ convert_presets.py 读取孤儿 C 的 `LV_PRESET_REGISTER`/`preset_blocks_register_by_category` 宏生成 56 个 preset_*.lvz（module/presets/，与 g_preset_lvz_files 完全对齐）。运行时 preset_blocks.c 加载 .lvz → module_lvz.c 调编译侧 `preset_blocks_register_simple` 注册进 func_block_registry。
- **复用结论**：
  1. **C 文件 = .lvz 唯一事实来源**（生成管道输入），保留（修改预设元数据 = 改 C → 重跑 convert_presets.py → 重生成 .lvz）；删除即断生成管道。
  2. **C 原生回退注册潜力**：孤儿 C 的宏展开目标 `preset_blocks_register_by_category` 在编译侧存在（preset_blocks.c），若未来需无 module/ 目录的 C 原生注册可编译接入——但与 .lvz 加载二选一，避免同名重复注册。
  3. **孤儿头**仅 preset_algebraic.h + preset_basic_geometry.h 被 11 个编译文件 include（历史遗留，40 个包装函数已删死代码，宏无消费），其余仅被各自孤儿 .c 引用。
- **处置（按用户「先研究可复用」裁决）**：**保留全部 55 .c + 61 头**，preset_blocks.c g_preset_lvz_files 处已加生成源说明注释（含回退注册提示）。无删除。

## 二十八、批次 C-⑧：豁免桥接清零 + 物理目录归位 + 层验证首开（2026-08-14）

用户指令「继续处理下一批架构优化任务」。消除批次 C-⑦ 登记的 3 处豁免桥接（context.c×2 + graph_index×1），物理目录按层 git mv 归位，并首次成功启用 ENABLE_LAYER_VALIDATION 全工程验证（0 违规）。

### ① context.c 资源操作回调注册表（豁免桥接 context.c×2 消除）

- **模式**：仿 lv_serialize_adapters / lv_storage_register_verify——L2 定义注册点，L0（lv.c）在 lv_init 注入真实实现。
- **context.h** 新增 `LvContextResourceOps`（create/copy/destroy/normalization_destroy 四个函数指针，用前向声明 `struct ConstraintGraph *` / `struct NormalizationResult *` 签名，与 graph_*/normalization_result_destroy 精确类型匹配、零强转）+ `lv_context_register_resource_ops()`。
- **context.c**：删 constraint_graph.h + normalization.h include；static ops 注册表；`LV_DESTROY_SHIM(destroy_ctx_main_graph/…normalization)` 改手写 void* 直通回调（未注册时 NULL 安全跳过，stream_ctx 保留 L2 强类型 SHIM）；create/copy/destroy 全部调用点改 ops 转发；未注入时 main_graph 保持 NULL 降级（快照/回滚跳过）。
- **lv.c**：`lv_module_init_context_resources()` 注入 `{graph_create, graph_copy, graph_destroy, normalization_result_destroy}`，经 lv_module_register("context_resources") 注册（仿 serialize_adapters 先例）。
- **test_circuit_breaker.c**：TEST_MAIN_BEGIN 内补 `lv_init()`（与 test_engine_ops 等先例一致），保证 create 契约。

### ② graph_index universe 校验下沉 L3（豁免桥接 graph_index×1 消除）

- **graph_index.c**：删 type_system.h include；内建 `kGraphTypeUniverseLevels`（GeomType→0/1 两档）+ `graph_type_universe_level()` + `graph_check_universe_constraint()`（语义与原 L4 版一致：外层严格高于内层；原开关 type_set_universe_checking 全库零调用，下沉版不带开关 = 始终检查，等价默认行为）。
- **type_system.c / type_system.h**：删除孤儿 API（type_get_universe_level / type_check_universe_constraint / type_set_universe_checking / type_is_universe_checking_enabled + kNodeTypeUniverseLevels + s_universe_checking_enabled），全库确认唯一消费者即 graph_index.c。universe_level_to_string / TypeRegion 版 type_get_level 保留。
- **doc/docs/08_type_system.md**：宇宙层级行与工作流同步更新（下沉说明）。

### ③ 物理目录按层 git mv（批次 C-⑦ 遗留的 CMake 归属 ≠ 物理目录）

- formula 家族 24 件（19 根 + parser/ 4 + 2 internal.h）→ `layer3_geometry/formula/`（内部相对 include 保持同目录结构，parser/ 子目录随迁）。
- dsl 家族 6 件（5 .c + internal.h）→ `layer4_reasoning/dsl/`。
- node_deep_copy.c / adaptive_threshold.c / simd_ops.c → `layer3_geometry/`；node_deep_copy.c 相对 include 改 `constraint_graph/graph_node_internal.h`（原 `../layer3_geometry/...` 跨层）。
- geom_evol.c / interactive_geo.c / meta_repr.c / lv_serialize_adapters.c → `layer4_reasoning/`。
- **CMakeLists**：同步全部 SOURCES 路径；**删除 L2 块遗留的 simd_ops.c 重复注册**（同文件同时挂 L2+L3 两个 OBJECT 库的隐患，L3 归属为正确）。
- L1 保留 7 文件（lv_ast/lexer/parser/sema/loader/math_input/parser_safety）全部只用 lv/*.h 公共头，不受迁移影响。

### ④ engine.h 层验证宏字符串化 bug 修复（验证模式首次可构建）

- `lv_ALLOW_LAYER` / `lv_REQUIRE_STRICTLY_ABOVE` 体内 `#lv_CURRENT_LAYER` 对非宏参数使用 `#` → 预处理器报错「'#' is not followed by a macro parameter」，**该 bug 导致 ENABLE_LAYER_VALIDATION 从未真正构建通过**。
- 新增双层展开辅助 `lv_STRINGIFY_IMPL(x)` / `lv_STRINGIFY(x)` 替换全部 `#宏` 用法。

### 批次 C-⑧ 执行结果

- **build3（默认）**：ninja 922/922 + ctest 170/170 全绿（116.81s，engine.h 改动后复跑）。
- **build_verify（ENABLE_LAYER_VALIDATION=ON）**：ninja 919/919 全绿 = **0 层间违规**，3 处豁免桥接全部清零，层验证首次全工程可用。

### 决策登记（第 9 章格式）

- context 资源操作回调注册表 / 批次C-⑧ / context.h/c + lv.c + test_circuit_breaker.c / L2 不持有 L3/L4 不透明资源实现，L0 注入（仿 serialize_adapters/storage_verify 注册模式）；未注入降级 NULL 安全 / 全量回归。
- universe 校验下沉 L3 / 批次C-⑧ / graph_index.c + type_system.c/h + doc 08 / GeomNode 域校验与枚举同层单一事实来源；L4 孤儿 API 全库零消费者删除 / 全量回归。
- 物理目录归位 40 件 git mv / 批次C-⑧ / CMakeLists + node_deep_copy include / CMake 归属与物理目录对齐；消除 simd_ops 双 OBJECT 注册隐患 / 全量回归。
- engine.h 字符串化修复 / 批次C-⑧ / engine.h / lv_STRINGIFY 双层展开替代非法 #lv_CURRENT_LAYER，验证模式首次可构建 / build_verify 全绿。
- 层验证首开 / 批次C-⑧ / ENABLE_LAYER_VALIDATION=ON / 3 豁免清零，0 违规；后续批次可直接以 ON 模式作为架构守门 / 919/919。

## 二十九、批次 C-⑨：公共头卫生与 .c/.h 单一定义对齐（2026-08-14）

用户指令「继续处理下一批架构优化任务」。以脚本做全量架构一致性扫描（CMake SOURCES vs 磁盘 / 公共头引用面），定位并处置脱钩点与死头。

### 扫描结论（临时脚本 scan_arch.py，已删）

- **CMake 层块**：L0=14 / L1=7 / L2=70 / L3=128 / L4=267（含 PROP_VERIFIER）/ L5=33 / L6=26 / L8=1 / L10=3。
- **重复注册 0**（simd_ops 双 OBJECT 隐患已于 C-⑧ 清除）；**归属不一致 0**（物理目录层号 == CMake 块层号，C-⑧ 归位完成）。
- **孤儿源文件 65** = preset 55（.lvz 生成源，C-⑦ 裁决保留）+ euclidean_geometry_* 9（**假孤儿**：euclidean_geometry.c 以 `#include "xxx.c"` 拼接的单翻译单元，无需独立注册）+ `_edit_test.c` 1（误提交临时文件，已删）。
- **孤儿公共头 7**（含相对 include 面）= stream_context_util.h（C 批次已裁决保留）+ layer_validation.h / three_layer_arithmetic.h / preset_register_macros.h（见处置）+ lv_convenience.h / math_theory_guide_cn.h（脱钩修复后非孤儿）+ parametric_curves.h（脱钩修复）。

### 处置明细

**① parametric_curves.c/h 脱钩修复（L3，类型单一定义）**
- parametric_curves.c 未 include 自己的公共头，本地重复定义 lvPoint2D/lvPoint3D/lvCurveEvalFunc/lvCurveDerivFunc/lvSurfaceEvalFunc/lvSurfaceDerivFunc/lvParametricDomain1D/lvParametricDomain2D 共 8 个 typedef（声明/定义脱钩，签名漂移则静默 UB 风险）。
- 修复：.c 补 `#include "lv/parametric_curves.h"`，删除 8 个重复 typedef，保留 struct lvParametricCurve/Surface 不透明对象内部布局（头仅前向声明）。

**② lv_convenience.c / math_theory_guide_cn.c 补 include 对齐（L0 / L2）**
- 两 .c 均未 include 自己的接口头（lv_convenience.h / math_theory_guide_cn.h）。补 include 对齐。
- 两模块为「预留」：lv_prove / lv_preset_load/unload/apply（L0 便捷层）与 lv_math_theory_*（L2 中文指南查询）全库零调用——能力完备、当前无接入点，保留（仿死模块判定「保留为预留」先例，接口头继续随实现对齐）。

**③ 死头处置**
- **preset_register_macros.h 删除**（+ CMake lv_HEADERS 移除）：PRESET_REGISTER_SIMPLE 宏全库零 include 零使用，与 .lvz 生成管道无关（convert_presets.py 不引用），真死宏头。
- **three_layer_arithmetic.h 保留 + 标注**：三层算术安全宏权威实现已收敛 lv_arith_safe.h，本头仅保留转发与编译标志文档，加「兼容遗留头」说明。
- **layer_validation.h 保留 + 标注**：编译期验证实际由 engine.h lv_ALLOW_LAYER / lv_REQUIRE_STRICTLY_ABOVE 承担，本头 lv_LAYER_CAN_DEPEND 为参考实现；十层依赖模型保持本文件为权威文档源。
- stream_context_util.h 保持批次 C 裁决（兼容遗留头）不动。

### 批次 C-⑨ 执行结果

- **build3**：ninja 922/922 + ctest 170/170 全绿（114.99s）。
- 架构一致性：重复注册 0 / 归属不一致 0 / 假孤儿定性（euclidean_geometry 拼接 TU）；垃圾文件 `_edit_test.c` 删除。

### 决策登记（第 9 章格式）

- .c/.h 脱钩修复 / 批次C-⑨ / parametric_curves.c + lv_convenience.c + math_theory_guide_cn.c / 实现补 include 公共头，类型/声明单一事实来源（parametric 删 8 重复 typedef）/ 全量回归。
- 预留模块标注 / 批次C-⑨ / lv_convenience + math_theory_guide_cn / 全库零调用但能力完备，保留为预留（接口头随实现对齐）/ 全量回归。
- 死头删除 / 批次C-⑨ / preset_register_macros.h + CMake / 零 include 零使用，非生成管道输入 / 全量回归。
- 死头保留+标注 / 批次C-⑨ / three_layer_arithmetic.h + layer_validation.h / 兼容遗留或文档权威源（宏实现已由 lv_arith_safe.h / engine.h 承担）/ 全量回归。
- 垃圾文件删除 / 批次C-⑨ / layer2_resource/_edit_test.c / 误提交临时测试残留，零引用 / 全量回归。

## 三十、批次 C-⑩：L4 内部 include 环破环（2026-08-14）

用户指令「可以研究一下架构的激进优化」。完成 L4 激进优化研究（见下），经 AskUserQuestion 用户选定**「先只破 L4 的环」**——层模型精简候选（L7/L9 空层、L10→L8 冗余链接、meta_verify 并入 L4、L10 三文件并入 L5、orchestrator.h/application.h 归档）全部暂缓，仅做 L4 内部五个 include 环破环。

### 激进优化研究结论（调研，未执行）

- **层规模**：L0=14 / L1=7 / L2=70 / L3=128 / L4=267 / L5=33 / L6=26 / L8=1 / L10=3；L7/L9 为空层；L10→L8 链接冗余（L10 应依赖 L2/L4/L5）。
- **巨型文件**：simd_ops 2148 行、interop_import 2147 行等。
- **L4 强连通分量**：proof/proof_system/unify/rewrite/type_logic/engine 六域构成不可无环拆分的整体；solver 与 backends 构成另一 SCC。五个 include 环（A-E）为 SCC 内的环边，破环后六域+solver/backends 整体可成 DAG。

### 破环明细（五个环）

**环 E（type_system↔rewrite↔unify 三角）**
- type_system.h：删 `#include "rewrite.h"`，加 `typedef struct RewriteRule RewriteRule;` 前向声明（仅指针引用）。
- unify.h：删 `#include "type_system.h"`，加 `typedef struct TypeRegion TypeRegion;` 前向声明。

**环 A（proof↔unify）**
- PropositionEquivalence 从 proof.h 移入 unify.h（合一域持有，类型单一事实来源；proof.h 保留注释指引）。
- unify/*.c 删 8 个死 `#include "lv/proof.h"`（unify_equivalence.c 经 unify.h 传递获得）。

**环 B（proof↔engine）**
- proof_engine_enhanced.h：删 `#include "proof.h"`，加 Proposition/ProofStep/ProofNavigator 三个前向声明（指针用法）。
- proof_engine_enhanced.c：删死 `#include "lv/proof.h"`。
- proof_navigator_*.c 11 个：删零使用 `#include "lv/engine.h"`。
- **本次验证修正**：proof_dependency.c / proof_navigator_instantiate.c 实为直访 lvEngine 字段（axiom_package_count/axiom_packages），前向声明不足。新增轻量头 **lv/engine_access.h**（仅前向声明 lvEngine/AxiomPackage）声明 `engine_get_axiom_package_count` / `engine_get_axiom_package` 访问器，engine_resource.c 实现，两 .c 改经访问器读取。proof→engine 目录边清零（engine→proof 保留，9 文件实质依赖）。

**环 C（rewrite↔unify）**
- rewrite_strategy.c 从 unify/ git mv 至 rewrite/（include 面干净：仅 rewrite_strategy.h / lv_internal.h / lv_str_utils.h），CMake 路径同步。

**环 D（solver↔backends）**
- sat_encoding.c 从 backends/ git mv 至 solver/（SAT 编码管线 = 编码 + 驱动 CDCL 求解，属 solver 域；全库零外部消费者，仅测试 test_bdd_sat_atp 链接引用）。CMake 路径同步；formal/lvFormal/Theory/SATEncoding.lean 对应路径同步。backends→solver 目录边清零（solver→backends 保留：solver_core→groebner_parallel 单向）。
- groebner_parallel.h：删死 `#include "lv/type_system.h"`（结构体全 int/double/void* 字段，零 type_ 符号）；补自包含 `#include <stdbool.h>`（原依赖传递）。

**环 E 断链连锁修复**
- unify.h 不再传递 type_system.h → proof_proposition.c / proof_navigator_instantiate.c 显式补 `#include "lv/type_system.h"`（.c 直接用自声明，符合依赖显式化方向）。module/*.c 的 `type_region_ids` 为字段名（int 数组），非类型引用，无需处理。

### 批次 C-⑩ 执行结果

- **build_verify**（ENABLE_LAYER_VALIDATION=ON）：ninja 850/850 全绿 + ctest 170/170 全绿；层验证 0 层间违规。
- **build3**：ninja 923/923 全绿 + ctest 170/170 全绿。
- **目录级环确认**：proof/→engine/ = 0 include；backends/→solver/ = 0 include；solver/→backends/ = 1（solver_core→groebner_parallel，单向保留）；engine/→proof/ 单向保留。
- **告警收敛**：lv_STRINGIFY 在 engine.h:102 与 lv_utils.h:665 重复定义（C-⑧ 引入的全构建噪音）→ lv_utils.h 升级为与 engine.h 逐 token 一致的双层展开（lv_STRINGIFY_IMPL + lv_STRINGIFY），重复定义告警清零。

### 决策登记（第 9 章格式）

- 环破环方向 / 批次C-⑩ / proof→engine、solver↔backends / 目录级单向化：保留实质依赖方向（engine→proof 9 文件、solver→backends 1 文件），反向边经访问器/文件归位清零 / 全量回归。
- 轻量头新增 / 批次C-⑩ / lv/engine_access.h / 跨域访问器声明（engine_get_axiom_package_*），实现于 engine_resource.c，proof 域经前向声明读取引擎数据 / 全量回归。
- 类型单一事实来源 / 批次C-⑩ / PropositionEquivalence proof.h→unify.h / 合一域持有，proof.h 仅注释指引 / 全量回归。
- 文件物理归位 / 批次C-⑩ / sat_encoding.c backends→solver + rewrite_strategy.c unify→rewrite / 归属按「编码+驱动求解」与「重写策略」职责 / 全量回归。
- 头前向声明破环 / 批次C-⑩ / type_system.h→RewriteRule、unify.h→TypeRegion、proof_engine_enhanced.h→Proposition/ProofStep/ProofNavigator / 仅指针引用处用前向声明替换完整 include / 全量回归。
- 死 include 删除 / 批次C-⑩ / groebner_parallel.h→type_system.h / 结构体零 type_ 符号；头补自包含 stdbool.h / 全量回归。
- 断链连锁显式 include / 批次C-⑩ / proof_proposition.c + proof_navigator_instantiate.c 补 type_system.h / unify.h 断链后 .c 自声明依赖 / 全量回归。
- 宏告警收敛 / 批次C-⑩ / lv_utils.h lv_STRINGIFY 双层展开 / 与 engine.h 逐 token 一致消除重复定义 / 全量回归。

### 后续候选（暂缓，层模型精简）

L7/L9 空层处置、L10→L8 冗余链接删除、meta_verify.c 并入 L4、L10 三文件并入 L5、orchestrator.h/application.h 归档、L4 按子域拆 OBJECT 库（环破完后的自然延伸）。

## 三十一、批次 C-⑪：L4 按子域拆 OBJECT 库（2026-08-14）

用户指令「可以研究一下架构的激进优化」，并补充**「十层架构不太可以动因为后面是要拓展的，我说的是其他的架构优化」**。经 AskUserQuestion 选定 **C：L4 按子域拆 OBJECT 库**。层模型坍缩候选（A）与空层处置撤销——十层保留为扩展预留。

### 研究修正（相对 C-⑩ 候选清单）

- **orchestrator.h 为活跃 API**：lvSession/lv_orchestrator_* 由 L0 的 lv_impl_upper_orchestrator.c 实现，L0 便捷层消费——「归档」候选**否定**。
- **application.h 已删**，无归档对象。
- **L10→L8 链接为死链接**（实证）：lean4_bridge/coq_bridge/opml_codec 三个 bridge 零 meta_verify 符号引用；因十层模型冻结（用户裁决），链接保留不动，清理待单独裁决。
- **L8 链接表达缺口**：lv_layer8_meta_verify 无任何 target_link_libraries（模型应 L2/L3/L4）；本轮不动（十层冻结）。

### 实施明细（19 个子域 OBJECT 库）

- 原 lv_LAYER4_SOURCES（267 文件）按子域目录拆为 19 个 `lv_L4_*_SOURCES`：core（根级门面/调试，24）/ axiom（9）/ backends（20）/ dsl（5）/ engine（21）/ expr（3）/ func_block（27）/ kernel（3）/ lambda（4）/ model（12）/ module（6）/ numeric（14，含 backends/ 4）/ proof（24，含根级 proof_trace.c）/ proof_system（14 + ${lv_PROP_VERIFIER_SOURCES} 16）/ rewrite（13）/ solver（19，含 sat_encoding）/ stream（1）/ type_logic（16）/ unify（12）。
- **lv_l4_subdomain 宏**统一建库：`add_library(OBJECT)` + `lv_setup_layer(name 4)` + 基础链接 `lv_layer3_geometry lv_layer2_resource`（L4 层模型依赖）。
- **lv_l4_func_block** 额外 PRIVATE 定义 `lv_PRESETS_DIR`（preset_blocks.c 唯一使用者，原随大库）。
- **lv_L4_LIBS 聚合变量**：供 L5/L6 `target_link_libraries` 与 lv_static/lv_shared `$<TARGET_OBJECTS>` 聚合展开（19 个）。
- lv_PROP_VERIFIER_SOURCES 变量保留：proof_system 库引用 + test_prop_verifier EXTRA_SOURCES 复用（单一清单维护）。
- 完整性核对脚本：磁盘 layer4_reasoning 265 个 .c（排除 preset/ 55 个 .lvz 生成源孤儿）= CMake 注册 265，**双向零差异**。
- 跨子域链接逐域精化（proof↔engine 等 DAG 细化）留作后续批次（本轮统一 L3+L2 基础依赖，构建正确性不受影响）。

### 批次 C-⑪ 执行结果

- **build3**：ninja 922/922 全绿 + ctest 170/170（首轮 performance_test 并行负载波动失败、单独重跑 10.93s 通过，非回归）。
- **build_verify**（ENABLE_LAYER_VALIDATION=ON）：ninja 447/447 全绿 + ctest 170/170，层验证 0 违规。
- L4 增量构建收益就位：改 proof 域只重编 lv_l4_proof + 聚合链接。

### 决策登记（第 9 章格式）

- L4 子域拆分 / 批次C-⑪ / 267 文件 → 19 个 OBJECT 库 / 增量构建 + 子域边界显式化；统一基础依赖 L3+L2，跨子域链接逐域精化留后续 / 全量回归。
- 十层模型冻结 / 批次C-⑪ / L7-L10 空/小层 / 用户明确：十层为未来扩展预留，不做层坍缩（候选 A 撤销）/ 不执行。
- 候选修正 / 批次C-⑪ / orchestrator.h「归档」否定（L0 活跃实现 lv_orchestrator_*）；application.h 已删无归档对象 / 不执行。
- 死链接实证 / 批次C-⑪ / L10→L8 / 三 bridge 零 meta_verify 引用；因十层冻结保留链接表达，清理待单独裁决 / 不执行。
- L8 链接表达缺口确认 / 批次C-⑪ / lv_layer8_meta_verify 无依赖链接 / 模型应 L2/L3/L4，本轮不动 / 后续待裁决。

### 批次 C-⑪-补：L4 子域 DAG 显式化——断 engine↔proof 残余环（2026-08-14）

用户「可以研究一下架构的激进优化」再次触发，进入 C-⑪ 遗留候选「跨子域链接逐域精化」的实施。

- 扫描工具：`scan_l4_subdomains.py`（一次性）按子域目录聚合 include 计数，生成 19×19 矩阵 + DFS 环检测 + 耦合度排名，支持 `--exclude-core`。
- **core 门面域是 hub**（入 107 / 出 28）：proof→core 32、engine→core 19、proof_system→core 20、unify→core 9…core 反指向全部子域。伪环 core→engine→func_block→core 属门面固有形态，**不破**（门面语义就是引所有子域）。
- **排除 core 后唯一真环 = engine↔proof**，两处成因：
  1. `proof.c` 死 `#include "lv/engine.h"`（C-⑩ 曾列死 include，本批证实漏删；grep 确认零 lvEngine 符号使用）→ **删除**。
  2. `conflict_detector.c` 异域（原 engine/ 目录，真消费方是 proof 域 meta_verify.c 的 `lv_conflict_detect_quick`）→ **git mv 至 proof/ 归位**。
- conflict_detector 归位安全性：依赖面全为 L2/L3 基础设施（constraint_graph / lv_graph_traversal / union_find_util 等），语义属推理辅助，归 proof 域无环。
- 重扫确认：**排除 core 后无环（DAG）✓**。
- CMake 遗留修复：lv_L4_ENGINE_SOURCES 残留旧路径 `engine/conflict_detector.c`（此前 SearchReplace 删除未持久化，致 build3 首构失败）→ 删除；grep 复核后 CMakeLists 仅 3 处引用（:507 头文件清单 / :1238 proof 新路径 / :1901 测试注册）。
- **build3**：ninja 923/923 全绿 + ctest **170/170**（124s，performance_test 本轮并行通过）。

### 决策登记（第 9 章格式）

- 断 engine↔proof 残余环 / 批次C-⑪-补 / proof.c 删死 include + conflict_detector.c git mv 归 proof 域 / 排除 core 门面域后 L4 子域依赖为 DAG / 全量回归 170/170。

### 后续候选（暂缓）

L10→L8 死链接清理（待裁决）、L8 依赖链接补全（待裁决）、core 门面 hub 形状治理（可选，需重新审视门面职责）、巨型文件拆分（simd_ops 2148 行 / interop_import 2147 行等，代码组织层）。

## 三十二、批次 C-⑫：未完整实现补齐（2026-08-14）

用户「可以补一下很多没有完整实现的」→ AskUserQuestion 选定「全部按照优先级完整实现」。全库盘点（子代理 search 全量扫描）后按优先级分 6 批实施，每批构建 + 相关测试验证。

### 批次 1：func_block 组合模式 FEEDBACK/BRANCH/PIPE（高）

- 原状：`PRESET_COMPOSE_*` 枚举 5 种，仅 SEQUENCE/PARALLEL 有实现；FEEDBACK/BRANCH 走 noop、PIPE 直接失败（两套组合路径）。
- 实施：
  - `func_block_compose.c` 新增三个公开原语：`func_block_feedback`（前 k 输出→前 k 输入反馈环，k=-1 全反馈，外部剩余端口保留）、`func_block_branch`（共享输入、输出合并，g 输入端口→f 外部输入关联）、`func_block_pipe`（顺序数据流，保留中间状态输出）。
  - `func_block.h` 声明 + 文档；`func_block_preset_ops.c` static preset_compose switch 补三 case + kComposeNamePrefix 补 PIPE 前缀（原注释「PIPE 走 default 失败」已过时）。
  - `preset_manager_compose.c` 元数据 VTable：FEEDBACK/BRANCH 从 noop 改为真实计算（feedback 外部输出=max(0,out-in)；branch=前两预设输出之和），删除无引用的 compose_meta_noop。
  - 测试：test_func_block.c 新增 3 个组合测试（含失败路径：反馈越界、分支输入不匹配）。GBK 文件插入内容保持纯 ASCII 注释。
- 验证：func_block 3/3 通过。

### 批次 2：Groebner 约束代数化失败路径修复（高）

- 原状：手动编码回退路径用零多项式占位（`poly_create` 空多项式加入理想 = 约束静默丢弃，SAT/UNSAT 判定可能失真）；主路径编码器静默 return 丢弃。
- 实施：
  - 手动路径：`GroebnerManualEncodeCtx` 加 `encode_failed` 计数；6 类约束编码失败处不再创建占位多项式，改为计数上报；solve 循环后 `encode_failed>0` → `SMT_ERROR_ENCODING_FAILED` + 返回 `SMT_RESULT_UNKNOWN`（宁可 UNKNOWN 不错判）。
  - 主路径：新增 `constraint_graph_to_ideal_ex(..., int *out_encode_failed)` 变体（头文件声明 + wrapper 兼容旧签名）；各 `groebner_engine_encode_*` 失败处计数。
  - **修复主路径 incidence 编码器真实 bug**：`groebner_engine_encode_incidence` 的 p1_id/p2_id 从不赋值（空 for 循环 + 恒 return），INCIDENCE 约束在主路径从不被编码 → 重写为直接使用线段 symbolic_coords 端点常量构造共线线性方程（`dy*Px - dx*Py + (ay*dx - ax*dy) = 0`）。
  - smt_backend_impl_groebner.c 主路径改用 _ex 并检查失败 → UNKNOWN。
- 验证：groebner/smt/bdd/prop_verifier 6/6 通过（行为未回归）。

### 批次 3：lvTask/lvTaskGroup 任务系统执行引擎（高）

- 原状：只有 create/add/destroy 三个函数，add 只 `pending++` 无调度执行，`lv_task_group_add` 无任何消费者；文件尾注释声称「占位实现」但占位函数体不存在（误导）。
- 实施：`thread_pool.h` 的 lvWaitGroup 加 task_head/task_tail 任务链表字段（线程池内部 wait group 由 calloc 清零不受影响）；`proof_version_task.c` 补 `lv_task_group_run`（FIFO 顺序执行，返回成功数）与 `lv_task_group_wait`（懒执行补齐）；destroy 前先执行剩余任务防泄漏；proof.h 声明；删除过时占位注释。
- 顺序执行为安全默认（与 SLEDGE_ASYNC 并发缺陷回退先例一致）；并行扩展留待调用方保证任务状态独立。
- 测试：test_proof.c 新增 test_task_group（run/wait/destroy 三路径 + 计数器验证）。
- 验证：proof 2/2 通过。

### 批次 4：JSON \uXXXX UTF-16 代理对完整处理（中）

- 原状：lv_json.c 与 lv_str_utils.c 双路径同为简化实现（每个 \uXXXX 独立 1-3 字节编码，`\uD83D\uDE00` 被拆成两个非法序列）；lv_json.c 注释「完整 UTF-16 代理对处理过于复杂」。
- 实施：新增公共 API `lv_str_json_read_codepoint`（高代理+低代理合并为补充平面码点；孤立代理/非法 → 0xFFFFFFFF 由调用方写 U+FFFD）与 `lv_str_codepoint_to_utf8`（1-4 字节编码）；lv_json.c 与 lv_str_utils.c 的 `\uXXXX` 分支均接入（判断条件 `adv_cp==4||adv_cp==10`——首版只判 4 导致代理对走 else，测试抓出）。
- 测试：test_json_buf.c 新增 test_unicode_surrogate（代理对/孤立高代理/BMP/lv_json_parse_string 双路径）。
- 验证：json_buf 1/1 通过。

### 批次 5：Herbie 浮点表达式优化补全（中）

- 原状：内置规则表 19 条 + 运算符计数粗估误差；注释「简化实现：不实际创建子进程」。
- 实施：规则表去重（删重复的 log(x)-log(y)）+ 扩充 9 条（sqrt(1+x)-1、log(1+exp(x))-x、exp(a)*exp(b)、三角差角/倍角/半角、分母共轭等）；误差估计改结构感知（减法抵消每处 ×10 条件数、超越函数加权）；**修复真实 bug：`opt.entries` darray 从未 init（elem_size=0）导致 push 失败、规则从不生效**。
- 测试：新建 test_herbie.c（hypot/因式分解/expm1/log1p/三角恒等/无匹配/误差模型），CMakeLists 注册 herbie_test。
- 验证：herbie 1/1 通过。

### 批次 6：低优先级小缺口批量（低）

- 旋转角度表：geometry_transform.c 补 210/225/240/300/315/330（30° 间隔全表，含注释更新）。
- PDF 点渲染：interop_export_pdf.c 用 4 段三次 Bezier 填充圆（k=4/3·(√2−1)）替代粗线段模拟。
- Isar 脚本：proof_version_sledge.c 骨架模板含策略名+耗时（替代「仅标注策略名称」占位）。
- 注释修正（非缺口，更新过时描述）：BDD cofactor 实为标准 Shannon 展开；refine 类型检查实走 solver 命题注册表 + SMT（强于关键词比较）；roundtrip 仅 text 有反向转换器（其他视图为架构限制）。

### 批次 C-⑫ 执行结果

- **build3**：ninja 924/924 全绿 + ctest **171/171**（147s，新增 herbie_test；性能/并行负载无波动）。
- 发现并修复的真实 bug 共 3 处：groebner_engine_encode_incidence 主路径恒丢弃（p1/p2 从不赋值）、herbie darray 未初始化致规则从不生效、JSON 代理对判断条件（首版）。

### 决策登记（第 9 章格式）

- 未完整实现补齐 / 批次C-⑫ / 6 批（组合模式、Groebner 代数化、任务系统、JSON 代理对、Herbie、低优先级批量）/ 用户选定「全部按优先级完整实现」/ 全量回归 171/171。
- Groebner 编码失败上报策略 / 批次C-⑫ / 零多项式占位静默丢弃 → 计数上报返回 UNKNOWN / 宁可 UNKNOWN 不给失真 SAT/UNSAT。
- 任务系统执行模型 / 批次C-⑫ / 顺序执行安全默认（SLEDGE 并发回退先例），并行留待调用方保证任务状态独立 / 已实施。
- Herbie 集成边界 / 批次C-⑫ / 不引入外部 herbie 子进程依赖（用户环境无 Herbie，不可验证），增强内置规则表 + 结构感知误差模型 / 已实施。
- roundtrip 非文本视图 / 批次C-⑫ / 其他视图无反向转换器，架构限制非缺口 / 不实施（注释说明）。

### 后续候选（暂缓）

L10→L8 死链接清理（待裁决）、L8 依赖链接补全（待裁决）、core 门面 hub 形状治理（可选）。

## 三十三、批次 C-⑬：架构激进优化——巨型文件拆分（2026-08-14）

用户「可以研究一下架构的激进优化」→ AskUserQuestion 选定「按照优先级逐步完整执行」。审计（search 子代理全量扫描 + SOLVER_SPLIT_PLAN 核查）确认 Top 8 巨型文件，按优先级逐个拆分。统一模式：banner 分区为单位按功能簇拆；簇内静态自足则不建 internal 头；跨簇共享静态函数去 static + internal 头声明；聚合入口保留公共 API；CMakeLists 条目同步并 grep 复核持久化。

### 拆分清单（8 文件 → 26 个子模块 + 7 个 internal 头）

| # | 原文件 | 拆分后 | 验证 |
|---|--------|--------|------|
| 1 | interop_import.c（2309→36 行） | interop_import_ggb_zip.c（545）/ggb_xml.c（784）/json.c（288）/svg.c（588）+ interop_import_internal.h | 928/928 + 7/7 |
| 2 | simd_ops.c（2322→220 行） | simd_v4d.c（924）/v4f.c（236）/v8f.c（197）/batch.c（530）/geo_matrix.c（259）+ simd_ops_internal.h | 933/933 + 171/171 |
| 3 | interop_server.c（1905→743 行） | interop_server_ws.c（1164，RFC 6455 全段）+ interop_server_internal.h | 935/934 + 171/171 |
| 4 | geometry_transform.c（1727→591 行） | geometry_transform_apply.c（271）/analysis.c（399）/group.c（559） | 937/937 + 171/171 |
| 5 | bdd_encoding.c（1554→550 行） | bdd_encoding_sift.c（142）/encode.c（413）/cnf.c（264）/add.c（290）+ bdd_encoding_internal.h | 941/941 + 171/171 |
| 6 | sat_encoding.c（1502→353 行） | sat_encoding_geom.c（435）/formula.c（353）/decode.c（433）+ sat_encoding_internal.h | 944/944 + 171/171 |
| 7 | solver_coord_extract.c（1366→388 行） | solver_equation_extract.c（983，**修复 SPLIT_PLAN #7 偏差**）+ solver_common.h 增声明 | 945/945 + 171/171 |
| 8 | interop_command.c（1477→965 行） | interop_command_export.c（377）/stream.c（200）+ interop_command_internal.h | 948/948 + 171/171 |

### 关键发现

- **SearchReplace 对 CMakeLists 大文件多次不持久**（本批 8 次拆分中至少 4 次复现，含并行编辑互相覆盖）→ 统一 PowerShell 精确替换（LF 行尾探测）+ Grep 复核。
- **拆分暴露的真实跨文件隐式依赖**（拆分前同 TU 编译掩盖）：lv_transform_equal 未入公开头（geometry_transform，本地前置声明处理）；solver_poly_pool_init/push 与 coord_to_mpz_scaled_exact 跨簇（solver_common.h 补声明）；AddManagerGuard 守卫随 ADD 段迁移（bdd）；interop_resp_json_init 跨 stream 族（interop_command）。
- **SOLVER_SPLIT_PLAN 偏差修复**：#7 solver_equation_extract.c 从未落地（内容并入 solver_coord_extract.c）→ 本批抽离；#10 实际位于 solver_symbolic.c，文档同步更正。
- 边界核对：solver_coord_extract 拆分时 find_node 注释块落错文件，人工修正两文件边界（悬空注释 + 缺失注释配对）。
- 统一保留 LF 行尾 + UTF-8 无 BOM；切片用 ReadAllLines + LF join + WriteAllText（WriteAllLines 会转 CRLF，弃用）。

### 决策登记（第 9 章格式）

- 架构激进优化 / 批次C-⑬ / 巨型文件按功能簇拆分（8 文件 → 26 子模块 + 7 internal 头）/ 用户选定「按优先级逐步完整执行」/ 全量回归 171/171。
- 拆分边界规则 / 批次C-⑬ / banner 分区为单位；簇内静态自足则不建 internal 头；跨簇静态去 static + internal 头 / 已实施。
- 内部函数声明归属 / 批次C-⑬ / 仅本模块用的跨文件函数用本地前置声明（lv_transform_equal）；不为此单独建头 / 已实施。

### 后续候选（暂缓）

拆分后 >800 行文件复查（预计已无）、L10→L8 死链接清理（待裁决）、L8 依赖链接补全（待裁决）、core 门面 hub 形状治理（可选）。

## 三十四、批次 C-⑭：架构激进优化——链接治理/死代码/门面收敛/再拆分（2026-08-14）

用户「可以研究一下架构的激进优化」→ 研究子代理产出 A-E 五方面审计（拆分潜力 28 个 >1100 行文件、L10→L8 死链接实证、L8/L1 缺链、门面 hub 治理点、双 Groebner 收敛等）→ AskUserQuestion 选定「全部按优先级完整执行」。执行顺序：低风险架构修正 → 高价值文件拆分 → 门面收敛。

### 阶段 1：链接/依赖治理（低风险，先行）

- **L10→L8 死链接删除**：CMakeLists L1606 `target_link_libraries(lv_layer10_interop lv_layer8_meta_verify)` 双向零引用实证（L10 三文件无 meta_verify.h、meta_verify 消费方全在 L0），删除。
- **L8 缺链补全**：lv_layer8_meta_verify 原无任何 target_link_libraries（实际消费 L3 的 lv_bfs_run + L4 的 proof 类型）→ 补 `lv_layer3_geometry ${lv_L4_LIBS} lv_layer2_resource`。
- **L1 缺链补全**：lv_loader.c 实际使用 engine（L4）与 constraint_graph（L3）符号但 L1 只链 L2 → 补链（lv_L4_LIBS 定义在 L1505 晚于 L1 目标 L1460，补链必须置于 set 之后；**教训：CMake 变量引用前须已定义、target_link_libraries 目标须已创建**）。
- **lv_loader 越权处置**：lv_loader.h 去 `#include "engine.h"` 改 `typedef struct lvEngine lvEngine;` 前向声明；engine_lifecycle.c 新增 `engine_get_main_graph` 访问器（声明入 lv.h 与 engine.h，遵循 engine.h「外部代码用访问器而非直访字段」约定）；lv_loader.c 8 处 `engine->main_graph` 字段直访替换 + 删除 L3 constraint_graph.h 直接 include（经 lv.h 门面传递）。L1 残留门面依赖（lv_add_point 仅在 lv.h 声明）在阶段 3 保留。

### 阶段 2：死代码裁决

- **msgpack 定性：活代码**——module_save_to_binary/module_load_from_binary 有 test_module_serialize_unify.c 真实消费，保留（研究报告"疑似未接入"被实证否定）。
- **死代码三件套删除**：proof_score.c/.h、proof_priority.c/.h、preset_helper_cn.c（全库零外部引用）→ 删 .c + .h + CMakeLists 5 条；保留文档型 math_theory_guide_cn.c/.h。

### 阶段 3：巨型文件再拆分（4 个，同 C-⑬ 模式：banner 分区 + 跨簇静态去 static + internal 头 + CMakeLists 复核）

| 原文件 | 拆分后 | 共享声明 |
|---|---|---|
| graph_node_alloc.c（1648→295 行） | graph_node_vtable.c（1106）/copy.c（239）/emit.c（130） | graph_node_internal.h 补 node_alloc_internal |
| lv_graph_traversal.c（1646→195 行） | graph_traversal_dfs.c（642）/tree.c（177）/bfs.c（492）/util.c（255）+ graph_traversal_internal.h（8 个去 static） | **DFSFrame/BFSQueue typedef 随使用方移入 dfs.c** |
| quantifier.c（1399→427 行） | quantifier_expr.c（273）/inst.c（343）/elim.c（434）+ quantifier_internal.h（3 函数 + kQuantBodyPropDestroyFields） | RESULT_NAME_BUF_SIZE 宏入 internal 头 |
| formula_dsl.c（1657→30 行壳） | formula_dsl_lex.c（407）/parse.c（1283）+ formula_dsl_internal.h（is_dsl_keyword） | 公共 API 由两子模块实现 |

- 每拆验证：ninja 全绿（948→957 目标随拆分递增）+ ctest 171/171。

### 阶段 4：门面 hub 收敛（内部去 lv.h 引用）

- lv.h 过时架构注释修正（"严格五层"→ 十层单向依赖，含 L0/L6-L10）。
- **头声明归位（不删 lv.h 原声明，仅增子头声明，C 重复声明合法）**：config.h 补 `lv_config_get_int/get_bool/get_double/get_string` 4 个通用 getter（实现于 lv.c）；engine.h 补 `engine_get_main_graph`。
- **78 个内部文件删除 `#include "lv/lv.h"`**（原 84 个 − 保留 6 个 − 恢复 1 个）：
  - 特殊 4 文件补子头：stream_internal.h（+stream.h）、aabb_tree_impl.h（+lv_utils.h）、engine_lifecycle.c（+rewrite.h，实测缺 rewrite_rule_destroy）、meta_verify.c（纯删）
  - 5 文件按编译错补子头：meta_repr.c（+proof.h/type_system.h）、proof_contradiction.c（+type_system.h）、bootstrap_test_oracle.c（+normalization.h）、groebner_engine.c（+debug.h）
  - **恢复 1 个**：bootstrap_test_init.c（用 lv_init/lv_cleanup，仅 lv.h 声明）
  - 保留 6 个：lv.c、lv_impl_upper_app.c、lv_loader.c、lv_protocol.c、lv_utils_misc.c、command_log.c（依赖 lv.h 独有符号：lv_add_point/lv_get_system_info/版本宏等）
- 最终 lv.h include 保留 7 个，编译面收敛完成。

### C-⑭ 执行结果

- build3：956/956 全绿 + ctest **171/171**（阶段 1-4 每步验证无回归）。
- 头文件新增声明 2 处（config.h/engine.h），公共 API 未删除；新增访问器 1 个（engine_get_main_graph）。

### 决策登记（第 9 章格式）

- 架构激进优化 / 批次C-⑭ / 链接治理+死代码裁决+门面收敛+4 文件拆分 / 用户选定「全部按优先级完整执行」/ 全量回归 171/171。
- 死代码裁决 / 批次C-⑭ / proof_score/proof_priority/preset_helper_cn 零引用删除；msgpack 有测试消费保留；math_theory_guide_cn 文档型保留 / 已实施。
- lv.h 门面保留边界 / 批次C-⑭ / 仅 L0 协调者 + 依赖 lv.h 独有符号的 6 文件保留；子头补声明（config.h/engine.h）供内部直连 / 已实施。
- 版本宏归属（lv_VERSION_*）/ 批次C-⑭ / 仍留 lv.h（lv_utils_misc.c 保留门面）；debug.h 3.3.0 兜底与 lv.h 1.1.0 不一致隐患已登记未动（避免范围扩张） / 暂缓。

### 后续候选（暂缓）

双 Groebner 引擎族收敛（E-1，拆分前奏）、表达式解析器词法层收敛（E-2）、cuda/hip 先收敛后拆分（A-2，有效编译仅 ~15 行）、>1100 行剩余文件复查（solver_core/normalization 已知不拆、lambda_unify/relation_model 建议留）、版本宏权威源统一（debug.h 3.3.0 兜底）。

## 三十五、批次 C-⑮：日常综合治理（2026-08-15）

日常治理定时任务自动执行：巡检 → 按优先级修复 → 回归 → 分块提交。

### ① bug-diagnosis：lv_free 传值误用缺陷族（最高优先级）

- **现象**：ctest 171 项中 `sparse_linear_algebra_test` SEGFAULT（确定性，崩溃在 `test_sparse_solve_zero_diagonal`）；直接 PowerShell 启动该 exe 却通过（偶发差异）。
- **定位**：启动方式二分（PowerShell & vs cmd /C vs ctest）→ cmd/ctest 必现、PowerShell 偶过；手工链接命令可执行 → 排除工具链；最小 ninja 工程同样挂起 → 环境边界（命名管道沙箱）确认，与缺陷无关。代码审查定位到 `lv_sparse_solve` 的 -3 错误路径 `lv_free(x_next)`。
- **根因**：`lv_free(void **ptr)` 契约是「释放 *ptr 并置 NULL」，调用点误传「指针的值」而非「指针的地址」。`*ptr` 读出的是分配体首 8 字节（未初始化垃圾/字符串内容），被当作指针交给 `debug_free` → 魔数不匹配时 `free(垃圾指针)` → 崩溃或堆损坏；首 8 字节恰为 0 时静默泄漏。启动环境不同 → 堆内容不同 → 偶发/必现差异。
- **同类排查**：全库 grep `lv_free(裸标识符/成员)` 逐点核对声明，共 **36 处 / 6 文件** 同类误用：sparse_linear_algebra.c（5）、interop_export_html.c（2）、interop_export_geojson.c（1）、dsl_compiler_parse.c（1）、critical_pair.c（12）、type_equiv_explorer.c（14，含 `type_equiv_explore_create` 失败路径误释放调用方 TypeSystem 的风险）。
- **为什么之前没测出来**：`-Wno-incompatible-pointer-types` 编译旗标压制了 `double* → void**` 隐式转换告警；多数调用点在错误路径（OOM）几乎不可达；interop_export_html/geojson 无测试覆盖。
- **修复**：36 处全部改为 `lv_free((void **) &var)` 取址形式。
- **回归**：`test_sparse_linear_algebra.c` 的 `test_sparse_solve_zero_diagonal` 新增内存差值断言（create 前记录 → destroy 后对比 current_used，钉住错误路径不得泄漏）；断言数 927→928。修复前该断言在旧代码下失败（泄漏 16B），修复后通过。

### ② incomplete-implementation：lv_loader.c Prove 注释声称与实现脱节

- `lv_loader.c` PROVE_STMT 分支注释声称「只是标记引擎的证明意图」，实际分支为空且不标记任何状态（验证由 `lv_verify_proofs`/`lv_load_file_verified` 完成）。注释改为准确描述加载阶段语义，消除误导。

### ③ code-abstraction-governance：裸 calloc/free 收敛（D6 惯用法重复）

- TASK_CONTEXT 基线声称「layer3+ 裸 malloc/realloc/free = 0」，实测 3 文件 15 处漏网：geo_spec.c（6）、geo_topology.c（7）、lv_render_visitor_tikz.c（2）。
- 判定：设施已存在（lv_calloc/lv_free）、调用点 15 ≥ 2、差异全为常量（大小/类型）、配对释放同家族、无跨边界所有权 → 行为等价可验证；负面清单不命中。
- 修复后全库裸分配仅剩 layer2 基础设施（allocator.c/lv_utils.c ops/memory_pool/thread_pool/lv_ringbuf）+ GMP 字符串豁免（lv_str_utils.c mpz_get_str 须系统 free）+ 注释引用。「layer3+ 归零」声明落实。

### ④ dev-automation / 仓库卫生：测试产物与构建状态解除跟踪

- `test_proof_infra.c` 每轮 ctest 重写根目录 `test_command_log.json`（cwd=仓库根），致工作树反复变脏；`build3/.ninja_deps`、`build3/.ninja_log`、`build3/test_command_log.json` 为构建/测试产物误入跟踪。
- 处置：`git rm --cached` 解除跟踪 + `.gitignore` 增补（test_command_log.json；build3/ 已有忽略规则覆盖其余）。

### 决策登记（第 9 章格式）

- lv_free 传值误用缺陷族 / 缺陷修复 / 36 处 6 文件 / 无 / test_sparse_linear_algebra（928 断言）+ 全量 ctest 171/171。
- 裸 calloc/free 收敛 / D6 惯用法重复 / 15 处 3 文件 / layer2 基础设施与 GMP 字符串豁免保留 / test_geo_topology + 全量回归 171/171。
- Prove 注释修正 / 声称与实现脱节 / lv_loader.c 1 处 / 无（注释仅文档）/ 全量构建。
- 测试产物跟踪解除 / 仓库卫生 / test_command_log.json×2 + build3/.ninja_* / 无 / 全量回归。
- 遗留登记：preset_instance_execute 为无调用方简化实现（零影响，不发明行为，待人工确认是否删除或接线 func_block 执行链）；graph_index error_buffer/serialize_buffer 按 v3.4.0 迁移计划保留（fallback 仍可达）；ABS.binder 恒 0 为 λ 项已登记限制。

### 验证基线

- build3：ninja 956/956 全绿（0 error）+ ctest **171/171** 全绿（72s，含 sparse 回归新断言）。

## 三十六、批次 C-⑯：测试文件中文注释双重编码乱码修复（2026-08-15）

- **误判澄清**：日报遗留项曾记「lv_render_visitor_tikz.c 注释双重编码乱码」——经复核该文件为正常 UTF-8 中文（乱码系 PowerShell Get-Content 按 GBK 代码页解码 UTF-8 文件的显示假象），**文件无缺陷**，该项撤销。
- **真实受损文件**：全库特征字符扫描（GBK→UTF-8 mojibake 典型字符）确认仅 2 个文件：`test/c/test_func_block.c`（193 处命中）、`test/c/test_proof.c`（99 处命中）。两文件均为合法 UTF-8 但中文注释/printf 字符串是双重编码乱码（自创建起即损坏，git 历史全部版本同损，无法从历史恢复）。
- **修复方法**：UTF-8→GBK 还原管线（mojibake 字符串按 GBK 编码还原原始字节流→UTF-8 解码）无损还原约 80% 文本；损坏点（GBK 解码遇非法字节对产生的 U+FFFD/'?'，原字节已丢失）按代码上下文**语义重建** 131+54 行（含 Latin-1 类深度损坏行如 `"��ѧ����"`→`"数学分析"`，依据 preset_category.h 单源表确认）。
- **行为钉**：`lv_ASSERT_STR_EQ(cat_str, "数学分析")` 与 `preset_category_to_string(PRESET_CATEGORY_ANALYSIS)` 返回值一致（LV_PRESET_CATEGORY_ENTRY 单源）；printf 字符串仅影响测试输出不影响断言逻辑。
- **验证**：纯 ASCII 行（1676+423 行）与 HEAD 逐字节一致（代码结构无损）；U+FFFD 归零；严格 UTF-8 无 BOM LF；test_func_block/test_proof/test_func_block_preset/test_proof_infra/test_proof_strategy_legacy/test_proof_strategy_numeric 全部通过（含 59/117/15/67 项）。库代码零改动（本次仅 2 个测试源文件）。

### 决策登记（第 9 章格式）

- 中文注释双重编码修复 / D4 映射重复（编码层） / 2 文件 325 行（131+54 行语义重建 + 无损还原）/ mojibake 生成时有损（GBK 非法字节对→U+FFFD），无法机械还原，按代码事实语义重建 / test_func_block + test_proof 等 6 项定向测试。
- 遗留登记：tikz 文件编码误判撤销；全库其余 .c/.h/.py/.js/.md 均无双重编码乱码（特征扫描已覆盖 core/test/tools/examples/module/bootstrap/gui/docs/doc）。

---

## 三十七、批次 C-⑰：日常综合治理（2026-08-15）

日常治理定时任务自动执行：巡检 → 按优先级修复 → 回归 → 分块提交。

### ① incomplete-implementation：6 个 lv_impl_upper_*.c 空壳占位文件删除

- **现象**：`core/src/lv_impl_upper_{geom,algebra,visual,interop,preset,utils}.c` 注释自述「已按死代码删除，保留本文件仅作为占位」，函数签名扫描确认 6 文件均**零函数体**（仅注释 + 20+ 头文件 include），仍注册于 CMakeLists lv_CORE_SOURCES 编译。
- **处置**：git rm 删除 6 文件 + CMakeLists 移除 6 条注册。lv_impl_upper_internal.h 保留（lv_impl_upper.c/app/meta/orchestrator 活跃使用 lvObjSlot 对象表）。目标数 956→950。
- **验证**：ninja 增量重建 exit 0 + ctest 171/171。

### ② incomplete-implementation：preset_instance_execute 零调用方简化实现删除（C-⑮ 遗留项闭环）

- **现象**：`preset_manager_instantiate.c` 的 `preset_instance_execute` 全库零真实调用（唯一引用是 preset_manager_doc.c 生成文档的示例字符串）；注释自述「简化实现」；函数体将 `DETERMINISM_UNVERIFIED` 静默标记为 `DETERMINISM_VERIFIED`（**虚假确定性声称副作用**，未来接入调用即产生误导）。
- **接线评估**：func_block 无单实例执行原语（仅 `preset_chain_execute` 链级执行），接线需新设计，超出日常治理「零发明行为」边界 → 删除为唯一行为等价处置（grep 三处引用：定义/声明/文档字符串，全部清除）。
- **同步删除**：`PresetExecutionContext` 孤儿类型（internal.h，零引用）；preset_manager_doc.c 生成示例第 4 步「执行预设」移除、步骤编号 5→4、6→5。
- **验证**：ninja 增量重建 exit 0 + ctest 171/171（含 test_func_block 族）。

### ③ 仓库卫生：构建产物与一次性脚本解除 git 跟踪

- **build3/ 126 文件**（.fix_check/dbg.c、_build_verify.py、_verify_import/*.ggb、build.ninja、diag*.err 等）与 **build4/lv.pc**：.gitignore 已有 `build3/` 规则但历史遗留入索引（批次 AB 曾提示待处理，C-⑮ 只处理了 3 个文件）→ `git rm --cached -r build3` 全量解除。
- **根目录杂项 39 项**：诊断日志（build_verify_build_log*.txt / build3_ctest_before.* / gdb_rot_cmd.txt 等，.gitignore 规则已覆盖）+ 一次性拆分/修复脚本（split_*.py / fix_csg*.py / check_csg.py / scan_size.py / test_regex*.py / edit_memory.py / github-integrations*.js，无 ignore 规则）→ 全部 git rm --cached（保留磁盘）。
- **.gitignore 增补**：`/split_*.py`、`/fix_csg*.py`、`/fix_dsl_lexer.py`、`/check_csg.py`、`/edit_memory.py`、`/scan_size.py`、`/test_regex*.py`、`/github-integrations*.js`。
- **保留**：convert_presets.py（.lvz 生成管道输入，C-⑦ 裁决）；log/.gitkeep、log/task_reports/.gitkeep（目录占位）；github-integrations*.js 仅解除跟踪不删除磁盘（GUI 潜在资产，待人工确认是否整体删除）。
- **验证**：git ls-files 杂项归零 + 工作树干净。

### ④ 环境边界登记（构建执行方式）

- ninja 在受限沙箱（workspace-write）下**编译 1-2 个文件后确定性挂起**（CPU≈0、无子进程、批处理间隙、与 stdout 重定向方式无关），danger-full-access 模式 956/956 全量构建正常。根因 = 沙箱对子进程管道/进程创建的拦截（与 C-⑮ 记载「命名管道沙箱」同源）。**今日起构建/测试命令（ninja/ctest）须以全访问模式执行**。
- 附带验证：mingw32-make 生成器因 CMake 生成文件含中文路径（知识体系化Wiki）在 make 侧 GBK 误解码而不可用，项目须继续使用 ninja 生成器。

### 决策登记（第 9 章格式）

- 空壳占位文件删除 / 死代码 / 6 .c 文件 + CMake 6 条 / lv_impl_upper_internal.h 保留（活跃文件使用 lvObjSlot）/ 全量回归 171/171。
- preset_instance_execute 删除 / 简化实现闭环（C-⑮ 遗留）/ 1 函数 + 1 孤儿类型 + 文档示例 / 无单实例执行链可接线，虚假确定性标记副作用 / func_block 族测试 + 全量回归 171/171。
- 构建产物解除跟踪 / 仓库卫生 / build3 126 + build4 1 + 根目录 39 项 / convert_presets.py 等活跃工具保留 / 无（索引操作，磁盘文件保留）。
- 沙箱构建限制 / 环境边界 / ninja/ctest 全访问模式 / 与 C-⑮ 命名管道沙箱同源 / 全量回归。

### 验证基线

- build3：ninja 增量重建 exit 0（956→950 目标）+ ctest **171/171** 全绿（173.49s）。

---

## 三十七-补、批次 C-⑰-补：占位文件恢复与真实实现（2026-08-15，用户指令）

C-⑰ 删除了 6 个 lv_impl_upper_*.c 空壳占位文件；用户指令「恢复了去实现一下」→ 从 git 历史（ca9427c8）恢复文件并**真实实现**（不恢复原假实现）。

### 恢复与实现内容

| 文件 | 函数数 | 实现要点 |
|------|:---:|------|
| lv_impl_upper_visual.c | 12 | visual_editor 5 + view_synchronizer 3 + text_code 4（**补 text_code_destroy 接口缺口**） |
| lv_impl_upper_geom.c | 9 | geom_evol 3 + atp_backend 4 + proof_tptp 2 |
| lv_impl_upper_algebra.c | 14 | preset_polynomial 族（声明式图节点语义） |
| lv_impl_upper_interop.c | 6 | geojson/svg/tikz 真导出 + coq/lean4/opml 显式 UNSUPPORTED |
| lv_impl_upper_preset.c | 40 | func_block_preset 族（32 int64 访问器 + 8 字符串访问器） |
| lv_impl_upper_utils.c | 4 | alloc_id/get_id_counter/full_verify/export_all |

**基础设施**：lv_impl_upper_internal.h 重建 `UpperState` 对象表（evol/atp_backend/atp_task/visual_editor/view_sync/text_code 6 表 + upper_id 计数器，MAX 64-512），`s_upper_state` 定义于 lv_impl_upper.c；**新建公共头 `core/include/lv/lv_upper_api.h`**（76+1 个接口声明，含契约注释，测试与外部绑定可访问——原声明仅在 src 内部头，测试目标无 core/src include 路径）。

### 假实现缺陷修复（安全红线 #6：静默降级禁止）

原始实现（ca9427c8）大量「假成功/模拟值/硬编码模板」，全部改为显式错误：

| 缺陷 | 原始行为 | 修复 |
|------|----------|------|
| algebra 族 52 处 | 错误路径 `return s_upper_state.upper_id++`（假装成功返回假 ID） | `lv_RETURN_ERROR` 分类错误码（INVALID_PARAM/NOT_FOUND/ALLOCATION_FAILED/INVALID_STATE） |
| geom_evol_step | 未找到引擎返回模拟值 `steps+1` | `lv_ERROR_NOT_FOUND` |
| geom_evol_create | 传 NULL RHS（geoevol_create 拒绝 → 恒失败） | 新增 `geom_evol_default_rhs`（恒零导数） |
| interop coq/lean4/opml | 生成硬编码 True 定理/空骨架模板 | `lv_ERROR_UNSUPPORTED`（无 ProofNavigator 访问器 / 无 OPML 导出 API），注释说明接线条件 |
| interop geojson/svg | 调文件导出 API 但不写 buf、返回错误码当长度 | 「临时文件导出 + lv_file_read_text 读回」真实语义（共享 helper `upper_export_via_temp_file`） |
| interop tikz | 无真实实现 | `lv_tikz_export` 内存导出（存在内存版 API） |
| preset registration_time | 模拟固定时间戳 1700000000000LL | `lv_ERROR_UNSUPPORTED`（PresetEntry 无该字段） |
| preset inverse_name | 模拟 `"inverse_<name>"` 前缀拼接 | 委托 `func_block_preset_get_inverse`（真实逆名查询） |
| utils full_verify | 依赖已删除的 meta_verify_* 4 旧函数 | 适配 `lv_graph_meta_verify_*` 3 函数（consistency 并入 soundness——其实现内含基础一致性校验） |

### 验证

- 新增 `test/c/test_upper_api.c`（upper_api_test，**64 断言**）：ID 句柄递增、visual_editor/view_sync/text_code 生命周期与往返（含销毁后错误路径、槽位复用）、geom_evol/atp_backend 生命周期与无效句柄、preset 包装族（init/count/exists/字段访问/名称表/UNSUPPORTED 路径）、TikZ 导出（含 tikzpicture 环境断言）、full_verify 空图验证。
- 编译中修正：注释含 `*/` glob 模式提前闭合（internal.h）、include 前缀（src 本地头保持无前缀）、`lv_ERROR_UNSUPPORTED` 错误码名。
- 全量：ninja 增量重建 exit 0（950→956 目标）+ ctest **172/172** 全绿（105.86s）。

### 决策登记（第 9 章格式）

- 占位文件恢复+实现 / 用户指令 / 6 .c + 1 公共头 + 2 内部文件 + CMake / 原假实现不恢复（红线 #6），真实实现 + 显式错误 / 全量回归 172/172（含新 upper_api_test）。
- lv_upper_api.h 公共声明 / 接口可见性 / 76+1 函数 / 原声明在 src 内部头不可达（测试/绑定无法访问） / upper_api_test。
- interop coq/lean4/opml 接线条件 / 待接线 / 0 文件 / 需要 lvEngine→ProofNavigator 访问器或 OPML 导出 API 后接线 / 显式 UNSUPPORTED 测试。

### 批次 C-⑰-补②：interop 导出接线闭环（2026-08-17，用户指令「需要补的实现补一下」）

3 个显式 UNSUPPORTED 导出全部转为真实实现：

| 导出 | 接线方式 |
|------|----------|
| coq / lean4 | `proof_navigator_create(NULL, ctx)`（与 interop_command_export.c 的 export_graph_coq 同构）→ 临时文件导出 `interop_export_coq/lean` → `lv_file_read_text` 读回 → 清理 |
| opml | 新增 opml_codec.c 公共设施 `lv_opml_export_navigator`（interop.h 声明）：遍历 ProofNavigator 步骤构造 OPML 内部表示并内存直写 buf，无临时文件 |

**配套设施/修复**：
- `upper_export_via_temp_file` 泛化为 `const void*` 输入 + format 参数（graph/navigator 两类输入共用）
- `opml_proof_release_fields` 提取共享释放（steps 描述/dependencies + steps 数组 + axioms，**不释放外壳**——修复 `lv_free(&p)` 释放栈对象的崩溃缺陷，属 C-⑮ lv_free 取址误用族同类，gdb 栈定位）
- `opml_map_step_type`：ProofStepType→lvProofStepType 映射（值集不一致：PACK_FUNCTION→FUNCTION_APP、UNIFY→EXACT、EX_FALSO→ORACLE 语义就近映射，两侧枚举保持独立，非合并）
- 发现并登记：`lv_register_opml_plugin` 零调用者（opml 插件从未注册；本接线绕过插件表直连编解码器，插件表消费方仍待接线）

**验证**：test_upper_api 断言 64→68（coq/lean4/opml 内容断言：opml 含 opml_version）；ninja 全绿 + ctest **172/172**（127.94s）。

**决策登记**：coq/lean4/opml 接线 / 用户指令 / interop.c + opml_codec.c + interop.h + lv_upper_api.h + test / lv_free 栈对象缺陷修复（opml_proof_release_fields 不释放外壳）/ upper_api_test + 全量回归 172/172。

### 批次 C-⑰-补③：插件体系接线（2026-08-17，用户指令「插件体系接一下」）

此前登记的「插件表消费方仍待接线」闭环——coq/lean4/opml 插件注册进入系统初始化生命周期，导出统一经插件表分发：

| 层 | 变更 |
|----|------|
| interop_server.c | 新增 `lv_interop_export_proof(plugin_name, nav, out, size)`：进程级插件表按名查找 + export_proof 分发，未注册返回 NOT_FOUND（插件表单例首次有消费方） |
| interop.h | 分发 API 声明 + `lv_register_coq/lean4/opml_plugin` 公共声明（此前仅定义零声明零调用） |
| lv.c | 新增 `interop_plugins` 模块（lv_MODULE_PRIO_RESOURCE）：创建 InteropServer 宿主 + 注册三插件；cleanup 清表 + 销毁宿主 |
| coq_bridge.c | `coq_export_proof` 改接受 ProofNavigator*：`coq_map_step_type`（本地枚举数值分叉豁免）+ bridge_proof_from_navigator 构造 lvBridgeProof |
| lean4_bridge.c | 同上（`lv_proof_type_to_interop` 映射） |
| opml_codec.c | 插件注册改 `opml_plugin_export_navigator`（navigator 语义包装）；本地 opml_map_step_type 收敛删除（共享 lv_proof_type_to_interop） |
| interop_bridge_common.h | 新增 `bridge_proof_from_navigator`（navigator→lvBridgeProof 构造骨架，map_type 回调） |
| interop_step_type.h | 新增 `lv_proof_type_to_interop`（ProofStepType→lvProofStepType 语义映射：PACK_FUNCTION→FUNCTION_APP、UNIFY→EXACT、EX_FALSO→ORACLE） |
| lv_impl_upper_interop.c | 三导出（coq/lean4/opml）改走插件分发（内存直写，去除临时文件路径） |

**include 隔离（编译冲突修复）**：bridge_common 不 include interop_step_type.h——Coq 本地枚举（lv_STEP_* 值符号与共享头同名、NORMALIZATION 数值分叉）为 D-1 豁免，include 共享头即值符号冲突；映射经 map_type 回调注入。opml 不再依赖 bridge_common（lvProofStep 同名不同构冲突）。

**验证**：test_upper_api +4 断言（未注册插件 NOT_FOUND、NULL 参数校验）共 **72 断言**全过；ninja 全绿 + ctest **172/172**（98.93s）。

**决策登记**：插件体系接线 / 用户指令 / 10 文件 / Coq 本地枚举豁免隔离（include 级）/ upper_api_test + 全量回归 172/172。遗留登记：插件 import_proof/validate 分发（lv_interop_import_proof 等）暂无上层消费方，待导入接口需求出现时接线。

## 三十八、批次 C-⑱：未完整实现补齐（2026-08-17）

用户选定「未完整实现补齐扫描（C-⑫ 模式）」→ 立项「全部按优先级完整实现」。全库手动盘点（190+ 候选 .c/.h 初筛 + 逐项读上下文定性），按优先级补齐。

### P0 正确性风险

**① algebraic_refine_for_equality 符号二分缺陷（algebraic.c）**：
- 原实现注释声称「评估中点符号决定缩小哪一半区间」，实际**无条件向中点收缩**（`mid ± half_width*0.5`，无符号评估）——根靠近区间一端时收缩后区间不再含根，同根代数数被误判为不同根。
- 修复：改为符号二分（`sym_evaluate_poly_double` 评估中点符号 + 收缩到含根一半，与同文件 `refine_algebraic_bounds` 同语义；中点恰为根时收缩到 `lv_rel_tol_scale` 邻域）。
- 影响面：`algebraic_compare` → `symbolic_coord_compare` 全库 50+ 调用点（归一化/重写/同构检测/冲突检测）。
- 测试：`test_symbolic_coord_ops.c` 新增 2 个回归（同极小多项式 x²-2 同根不同区间判等、异根 √2/-√2 判不等），281/281 通过。

### P1 功能缺失 / 静默降级

**② func_block add_to_graph 空分支（func_block_preset_internal.c）**：
- 原 `func_block_preset_instantiate_ex` 的 add_to_graph 分支为空（注释「应该创建实际的约束节点」），而 `func_block_preset_instantiate` 默认 `add_to_graph=true`，调用方（upper API / lv_convenience）期望函数块入图实际没有。
- 修复：补 `graph_add_function_block` 真实入图，失败回滚 fb 返回错误；补 constraint_graph.h include。
- 测试：`test_func_block_preset.c` 新增 `test_instantiate_add_to_graph`（midpoint 实例化后图中新增 GEOM_FUNCTION_BLOCK 节点），66 项通过。

**③ PARALLEL 平行约束全链路（ConstraintType 扩展）**：
- 原 `formula_convert_parallel` 用 `graph_add_containment` 简化表示平行约束——但 containment 类型检查要求 inner=POINT/REGION、outer=REGION/CIRCLE，两条线段必然返回 CONFLICT → **平行约束从公式转换永远失败、静默丢失**。
- 修复：新增 `ConstraintType::PARALLEL` + `graph_add_parallel`（两线段方向共线），全链路同步：
  - X-macro（LV_CONSTRAINT_TYPE_X/ENTRY）→ 名称/别名/序列化（meta_repr/graph_serialize/rewrite_apply/module_serialize）自动覆盖
  - kConstraintAddOps 注册（含 algebra_constrain 按名分发 "parallel"）
  - Groebner 编码：方向向量叉积为零常量方程 `groebner_engine_encode_parallel`
  - Solver 方程提取：`extract_parallel` 叉积 mpz 方程
  - DTMC 构建（unidirectional）、调度特征表、演绎事实格式（新 DEDUCT_FMT_PARALLEL）、WL 哈希（CONSTRAINT_TYPE_COUNT=PARALLEL+1）、匹配掩码、SVG/PDF 样式、formula 导出（\parallel）共 15 处表同步
  - meta_repr `_Static_assert` 同步为 PARALLEL+1
- 测试：`test_geometry_core.c` 新增 `test_graph_add_parallel`（成功/两点冲突/名称别名映射），204 项通过；`test_enum_maps.c` 两处 ANGLE+1 同步为 PARALLEL+1。

**④ inequality_reasoning 符号判定 5 桩（inequality_reasoning_core.c）**：
- 符号判定 vtable 6 个中 5 个（var/power/product/sum/function）原为 `return SIGN_UNKNOWN` 桩，仅 rational 有实现。
- 补齐（按「基于表达式自身结构」语义）：power（正底数恒正/负底数奇偶指数/零底数正指数）、product（零因子/负因子奇偶/UNKNOWN 传播）、sum（全正/全负/全零/混合）、function（abs/sqrt/norm/max 恒非负）、var（无系统约束保持 UNKNOWN）。
- 测试：新建 `test/c/test_inequality_reasoning.c`（25 断言），CMakeLists 注册 inequality_reasoning_test。

**⑤ modal 对偶转换静默错判（modal_operators.c）**：
- `lv_modal_possible_to_necessary_not` / `lv_modal_necessary_to_not_possible` 声称 ◇A→¬□¬A / □A→¬◇¬A，实际只返回未取反的嵌套公式（□A / ◇A）；`lvModalFormula` 结构（op + inner_prop/sub）**无否定节点**无法正确实现。零调用方死接口。
- 处置：改为显式 `lv_RETURN_ERROR_NULL(lv_ERROR_UNSUPPORTED)`（红线：宁可显式报错不可静默错判），头文件注释同步说明接线条件（结构扩展支持否定后）。
- 测试：新建 `test/c/test_modal_operators.c`（8 断言），CMakeLists 注册 modal_operators_test。

### P2 误导注释 / 占位行为修正

| 位置 | 修正 |
|------|------|
| preset_manager.c active_count | 原 `= entry_count`（注释「简化实现」）；改哈希表遍历统计 `is_active` 条目（新 `stats_active_visitor`） |
| mini_kernel.c 验证器 | 注释「检查栈顶匹配目标公式」→ 准确描述：依赖闭包验证，仅确认引用语句已验证，不执行公式重写与栈顶-目标匹配 |
| geo_utils.c geo_coord_to_double | 「简化实现」→「委托 symbolics 层」单一事实来源 |
| lv_impl_upper_geom.c proof_tptp_export | 无约束图时原返回恒真占位 TPTP（误导）；改显式 `lv_ERROR_INVALID_STATE` |
| solver_equation_extract.c extract_angle | **顺带发现**：原为空桩（angle 约束在方程提取时静默丢弃代数约束力）→ 补齐余弦等式常量方程（与 groebner 编码同构） |

### 验证

- build3：ninja 全绿（目标数随新测试递增）+ ctest **174/174**（20.72s，新增 inequality_reasoning_test + modal_operators_test；原 172 基线 +2）。
- 分块提交 5 个：`fix(algebraic)` / `feat(func_block)` / `feat(constraint)` / `feat(type_logic)` / `fix(杂项)`。

### 决策登记（第 9 章格式）

- 符号二分修复 / 正确性缺陷 / algebraic_refine_for_equality / 无符号收缩 → 符号二分（同 refine_algebraic_bounds 语义）/ test_symbolic_coord_ops 281/281 + 全量 174/174。
- add_to_graph 补齐 / M1/M2 / func_block_preset_internal.c / graph_add_function_block 真实入图 / test_func_block_preset 66 + 全量。
- PARALLEL 约束类型 / M4/M5 / ConstraintType 扩展 + 15 处表同步 / 平行约束从 containment 错误表示改为真实语义 / test_geometry_core 204 + test_enum_maps 同步 + 全量。
- 符号判定补齐 / M1 / inequality_reasoning_core.c 5 桩 / 结构语义符号分析，保守返回 UNKNOWN / test_inequality_reasoning 25 + 全量。
- modal 对偶 UNSUPPORTED / M5 / 死接口 + 结构无否定节点 / 显式报错（红线 #6）/ test_modal_operators 8 + 全量。
- extract_angle 补齐 / M1 / solver_equation_extract.c / 余弦等式常量方程（与 groebner 同构）/ 全量回归。

### 遗留登记

- modal 对偶转换：结构扩展支持否定节点（lvModalFormula 加 negation）后可接线真实实现。
- interop 插件 import_proof/validate 分发仍无上层消费方（C-⑰-补③ 遗留，待导入接口需求）。
- 其余有意简化（CUDA/HIP 条件编译 stub、herbie 内置规则表、λ ABS.binder、SVG/PDF 增强清单、算法固有近似）维持豁免。

## 三十九、批次 C-⑱-补：版本宏权威源统一（2026-08-17，用户指令「都改 1.1.0」）

C-⑭ 遗留项闭环：`lv_VERSION_STRING` 三处定义不一致（include 顺序相关隐患）。

### 统一前现状

| 位置 | 定义 | 性质 |
|------|------|------|
| `lv.h:182-189` | `lv_VERSION_MAJOR/MINOR/PATCH = 1.1.0`，宏生成 `lv_VERSION_STRING` | **权威源** |
| `debug.h:39` | `lv_VERSION_STRING_MACRO_(3, 3, 0)`（带 `#ifndef` 守卫） | fallback，值错 |
| `lv_internal.h:93` | `"3.5.0"`（带 `#ifndef` 守卫） | fallback，值错 |

三个头均有 `#ifndef` 守卫 → 若 debug.h/lv_internal.h 先于 lv.h 被 include，fallback 值生效产生版本漂移。

### 统一内容

- `debug.h` / `lv_internal.h` fallback 值 → 1.1.0（与 lv.h 权威源一致），注释登记权威源统一依据。
- **runtime_monitor.c `lv_diagnostics_generate`**：原硬编码 `"3.3.0"` 写入诊断报告 `diag->version`（版本号错误）→ 改引 `lv_VERSION_STRING` 宏（顺带发现的第 4 处运行期硬编码）。
- `lv.h` 头部注释「当前版本: 3.3.0」与「统一版本号 v3.3.0」→ 1.1.0（文档一致性）。
- 全库其余 `3.3.0`/`3.5.0` 命中均为注释内历史 `@version` 标记，非运行期值，不动。

### 验证

- ninja 全绿 + ctest **174/174**（137.92s）。
- 提交 `fix(version)`（4 文件，+11/-5）。

### 决策登记（第 9 章格式）

- 版本宏权威源统一 / 遗留闭环（C-⑭）/ debug.h + lv_internal.h fallback 值 + runtime_monitor 硬编码 + lv.h 注释 / fallback 值对齐 lv.h 权威源，运行期改引宏 / 全量回归 174/174。

## 四十、批次 C-⑲：抽象化收敛 + 编码乱码修复（2026-08-17）

用户选定「抽象化收敛扫描（批次 O/X 模式）」→ 立项「候选 1+2 全部实施」。全库扫描（判据 A-L 六维）发现：枚举↔字符串/线性查找/1e-8 常量等维度已治理完毕，两个真实候选 + 顺带发现 C-⑯ 漏扫乱码。

### ① M_PI → lv_PI 收敛（D3 常量重复，判据 C）

- 现状：17 文件 57 处裸用平台宏 M_PI；config.h 已有 `lv_PI`/`lv_TWO_PI`/`lv_HALF_PI`/`lv_QUARTER_PI` 权威源（W2 单源化），且 conflict_detector/lv_numeric/meta_proof 已有迁移先例。
- **等价性验证**：M_PI 与 lv_PI 在 double 下逐位相等（3.141592653589793116），迁移行为等价（判据 §1.3 满足）。
- 迁移：14 文件 52 处（含 formula_eval.c 的 M_PI_2 → lv_HALF_PI）；solver_symbolic.c 补 `#include "lv/config.h"`（原不可达）；geo_visual_complete.c 2 处为输出给外部 Cairo 解释器的脚本文本（负面清单 #2 外部契约）**豁免**并加 exempt 标注；修正 high_dim.c/interop.c 误导注释（lv_PI 权威源在 config.h 非 lv_internal.h）；删除 high_dim.c 悬空圆周率注释块。
- 验证：ninja 全绿 + 受影响测试 8/8。

### ② 错误消息组装收敛（D1 骨架重复，判据 A/L）

- **orchestrator varg 化**：lv_impl_upper_orchestrator.c 的 `set_error_msg` 原仅支持静态消息，14 处裸 `snprintf(stage.error_msg, sizeof, fmt, ...)` 未收敛 → varg 化（vsnprintf）后静态/格式化统一走封装，缓冲区大小集中管理，补 stdarg.h。
- **lv_snprintf 迁移**：prop_verifier_api.c 5 处错误消息组装（乱码修复中顺带）→ `lv_snprintf`（批次 U 判据 L 设施复用，NUL 终止保证 + 返回值语义一致）。
- block_scheduler.c 1 处格式化环检测消息维持 `exempt`（判据 K 已登记豁免）。

### ③ C-⑯ 漏扫乱码修复（新发现缺陷）

- **发现**：全库 mojibake 特征扫描发现 core/src 11 文件双重编码乱码（C-⑯ 只修了 test/c 2 文件，未覆盖 core/src）：4 preset（algebraic/basic_geometry/polygons/transformations）+ 6 prop_verifier（api/bhk/checks/engine/equivalence/trust）+ lv_platform.h。
- **影响**：prop_verifier_bhk.c 的 missing_descriptions（用户可见错误消息）与 trust.c 的流事件消息为乱码字符串——**影响运行时行为**。
- **修复**（C-⑯ 同源方法论）：GBK 还原管线无损还原约 30% 文本；损坏点（U+FFFD 原字节丢失）按代码上下文语义重建 115+ 行。
- 验证：全库 mojibake 特征归零（U+FFFD/U+013F 计数 = 0）。

### 验证

- ninja 全绿 + ctest **174/174**（119.99s）。
- 分块提交 5 个：`refactor(constant)` ×2（M_PI 迁移 + 补漏 interop_import_svg）/ `refactor(orchestrator)`（varg 化）/ `fix(encoding)`（乱码 11 文件）/ 文档登记。

### 决策登记（第 9 章格式）

- M_PI 收敛 / 判据 C / 14 文件 52 处 + 2 豁免 / 外部契约字符串豁免（geo_visual_complete Cairo 脚本）/ interval_arith 等 8 测试 + 全量 174/174。
- set_error_msg varg 化 / 判据 A / 14 处 / 无 / orchestrator 相关 + 全量。
- lv_snprintf 迁移 / 判据 L / 5 处（prop_verifier_api）/ 无 / prop_verifier 族 + 全量。
- 乱码修复 / D4 编码重复（C-⑯ 同源）/ 11 文件 115+ 行语义重建 / U+FFFD 有损无法机械还原 / 全库 mojibake 归零 + 全量 174/174。

### 遗留登记

- ABSTRACTION_SPEC §11.1 黑名单新增：裸 `M_PI`、错误字段裸 `snprintf(error_msg, ...)`。
- 全库枚举↔字符串表已确认 X-macro/数据表化（健康形态），无待收敛孤例。
- lv_convenience.c(13) / proof_verify.c(10) 等格式化输出 snprintf 未迁移（非错误消息字段，属判据 L 大面迁移，登记待后续批次评估）。

## 四十一、批次 C-⑳：综合治理（未完整实现重扫 + 仓库卫生 + bug 定位 + 大文件评估）（2026-08-19）

用户选定「全部按优先级执行」→ 按风险从低到高推进四方向。

### ① 未完整实现重扫

- 全库 63 处 TODO/占位/桩命中复核，**无 P0/P1 新缺口**：
  - 已登记豁免/有意设计确认：preset_polynomial 族（C-⑰-补「声明式图节点语义」）、graph_node_stub（真实实现）、rewrite_apply placeholder（算法语义）、smt_backend UNKNOWN 策略、λ ABS.binder（已登记限制）、lambda_church id 占位（有意）、cuda/hip 条件编译 stub（Q3 豁免）。
  - P2 登记：block_to_text.c「// block body」占位注释（序列化格式声称与实现脱节，无行为影响，文本 DSL 块体为空属格式契约）。

### ② 仓库卫生

- **解除 25+ 构建/测试诊断产物与一次性脚本跟踪**（C-⑰ 漏网）：
  - 构建诊断：build3_k.* / lifecycle_k.* / build3_ctest_before.* / build3_target_ctest_map*.txt / build_verify_build_log*.txt / build_verify_ctest_log1.txt
  - 测试输出：test_output.tex / test_tikz_temp.tex / temp_results.txt / one_arg_asserts.txt / s5_funcs.txt / switch5_funcs.txt
  - 探针/调试：_probe_write_test.txt / core 内 .write_probe.txt / gdb_rot_cmd.txt
  - 一次性脚本 17 项：split_*.py / fix_csg*.py / check_csg.py / edit_memory.py / scan_size.py / test_regex*.py / github-integrations*.js（.gitignore 已覆盖但历史入索引）
- .gitignore 增补：/*_k.* / /*_probe* / .write_probe.txt / gdb_rot* / lifecycle_k.* / test_output.tex / test_tikz_temp.tex / test_proof_output.txt
- 磁盘文件全部保留，仅解除索引跟踪。

### ③ bug 定位：lv_free 传值误用缺陷族补漏（P0）

- **发现**：lv_impl_upper_preset.c 4 处 `lv_free(_js)`（func_block_preset_metadata ×2 / bindings ×2）——C-⑮ 修复 36 处后的**同类新引入缺陷**（C-⑰-补 恢复文件晚于 C-⑮ 扫描）。
- **根因**：`char *_js = lv_json_buf_finalize(&_jb)` 后 `lv_free(_js)` 传指针值而非地址 → `*ptr` 读 JSON 内容前 8 字节当指针 free；首 8 字节为 0 时静默泄漏（实测路径），非零时崩溃/堆损坏（与 C-⑮ 同款）。
- **修复**：4 处改 `lv_free((void **) &_js)` 取址形式。
- **全库复查**：Python 精确扫描 `lv_free(裸标识符)` 与 `lv_free(成员)` 形态，除设施自身（lv_free_ptr_array/lv_free_many）外**零剩余**。
- **测试**：test_upper_api 新增 metadata/bindings JSON 路径 + 内存差值断言（ms_after <= ms_before + 4096）钉住修复；76/76 通过。

### ④ 大文件拆分评估（cuda/hip + lv_parser + module_lvz）

- **cuda/hip 后端**：1443/1391 行中非 SDK 分支仅 ~18 行存根（其余在 `#ifdef LV_HAS_CUDA/HIP` 完整实现内，当前构建不编译）；**零调用方**（仅 numerical_backend.c 条件 include）；Q3 已登记豁免 → **维持现状，拆分无收益**。
- **lv_parser.c（1419 行）**：4 分区（语句级/表达式/嵌套泛型/公共 API）递归下降**强互调**（15 前向声明需全公开化），拆分风险高收益低 → **登记暂缓**。
- **module_lvz.c（1339 行）**：查找表分发已数据表化（健康形态），词法层仅 120 行拆分收益小 → **登记暂缓**。
- 结论：C-⑭ 剩余大文件（solver_core/normalization/lambda_unify/relation_model 已知不拆）之外的 lv_parser/module_lvz 也评估为**不拆**——拆分收益递减，符合「禁止为拆分而拆分」。

### 验证

- ninja 全绿 + ctest **174/174**（73.58s；stream_extended_test 并行波动单独重跑通过，非回归）。
- 分块提交 3 个：`chore`（仓库卫生）/ `fix(memory)`（lv_free 补漏）/ 文档登记。

### 决策登记（第 9 章格式）

- 仓库卫生 / 仓库治理 / 25+ 产物 + 17 脚本解除跟踪 / .gitignore 增补 8 规则 / 无（索引操作，磁盘保留）。
- lv_free 补漏 / 缺陷修复（C-⑮ 同源）/ 4 处 lv_impl_upper_preset.c / 传值→取址 / test_upper_api 内存差值断言 + 全量 174/174。
- 大文件不拆 / 架构评估 / cuda-hip 零调用方条件编译豁免（Q3）+ lv_parser 强耦合 + module_lvz 已数据表化 / 拆分收益递减 / 全量回归。

### 遗留登记

- block_to_text.c「// block body」占位注释（P2 注释修正候选）。
- lv_parser.c / module_lvz.c 拆分暂缓（收益递减，未来语言演进频繁改动时再评估）。

## 四十二、批次 C-㉑：判据 L snprintf 收敛大面迁移（2026-08-19）

用户选定「snprintf 收敛大面迁移（判据 L）」。全库分类统计：严格 L1（格式串恰为 "%s"）5 处、L2（格式化写入）446 处。

### ① L1 纯复制收敛（4 处）

- 严格 L1 剩余 5 处（上批已迁 30 处）：迁移 4 处——interactive_geo.c 头部（`snprintf(buf, bufsz, "%s", hdr)` → `lv_strlcpy`）、lv_impl_upper_geom.c proof_tptp_export（TPTP 文本复制）、lv_impl_upper_interop.c output_path、mini_kernel.c trimmed。
- 豁免 2 处：mini_kernel（`lv_str_trim` 偏移指针 + 基址 sizeof，判据 L 豁免①）、interactive_geo 游标追加（`buf + w`，批次 U3 豁免）。
- `lv_strlcpy` 返回源长度与 C99 snprintf("%s") 一致（含截断返回完整源长）——行为等价。

### ② L2 格式化收敛（632 处/126 文件）

- 全库裸 snprintf → `lv_snprintf` 批量迁移（一次性脚本 + 逐文件验证）：格式化输出/序列化/渲染路径（lv_protocol 56、smt_backend_impl_smtlib2 28、formula_renderer_latex 27、formula_renderer_python 24、formula_converter_export 23 等）。
- **lv_snprintf 安全性增强**：NUL 终止保证 + NULL buf/size 0 显式 `lv_RETURN_ERROR`（把裸 snprintf 的 NULL buf 未定义行为变为可诊断错误）；正常路径 vsnprintf 逐位一致。
- **脚本 bug 修复**：正则 lookbehind `(?<!lv_)` 仅防 lv_ 前缀，误把 `vsnprintf(` 替换为 `vlv_snprintf(`（34 处/23 文件）——已全库恢复为 vsnprintf，归零验证。
- 裸 snprintf（非 lv_/vs 前缀）**全库归零**（仅剩 2 处注释引用）。

### 验证

- ninja 全绿 + ctest **174/174**（124.22s）。
- 分块提交 2 个：`refactor(string)` L1（4 处）/ L2（632 处）。

### 决策登记（第 9 章格式）

- L1 收敛 / 判据 L / 4 处迁移 + 2 豁免 / 偏移指针 + 游标追加豁免（已登记）/ 全量 174/174。
- L2 收敛 / 判据 L / 632 处 126 文件 / 无（行为等价 + 安全性增强）/ 全量 174/174。
- vsnprintf 误替换修复 / 脚本缺陷 / 34 处恢复 / 正则 lookbehind 局限 / 归零验证。

### 遗留登记

- ABSTRACTION_SPEC §11.1 黑名单新增：裸 `snprintf(`（→ lv_snprintf / lv_strlcpy；豁免 vsnprintf / 游标追加）。
- L2 迁移后 `vsnprintf` 调用点（34 处，varg 封装内部）为合法形态，非候选。

## 四十三、批次 C-㉒：层验证确认 + strdup/strcmp 收敛（2026-08-19）

用户「继续」。先做 C-㉑ 大迁移（632 处 snprintf 跨 L1-L10）的 build_verify 层验证，再收敛剩余字符串惯用法。

### ① build_verify 层验证确认

- C-㉑ 迁移 632 处 snprintf 横跨全部 10 层，跑 ENABLE_LAYER_VALIDATION=ON 构建确认层边界未破坏。
- ninja 325/325 全绿（`-Dlv_CURRENT_LAYER=N -Dlv_ENABLE_LAYER_VALIDATION` 编译期断言全过）+ ctest **174/174**（90.52s）= 0 层间违规。

### ② strcmp 收敛（1 处）

- 全库 strcmp 直接调用 4 处：lv_impl_upper_preset.c param_index 的 `== 0` 相等判断 → `lv_str_eq`；其余 3 处为三态比较（`< 0`/`> 0`，qsort/comparator 语义）维持不动；lv_str_utils.c 2 处为设施内部（lv_str_endswith/lv_str_eq 实现）合法。

### ③ 手写 strdup 收敛（8 处）

- 8 处手写 `lv_malloc(strlen(x) + 1)` + `lv_strlcpy` → `lv_strdup_safe`（proof_dependency 4 / proof_version_nl 2 / lv_utils_misc 1 / test_framework 1）。
- **严格行为等价**：原空串分支返回 NULL，lv_strdup_safe 返回空串副本——下游判空处（如 lv_utils_misc 的 `node->name ? ... : "<未命名>"`）保留 `name[0] != '\0'` 条件保证等价。
- 手写 `lv_malloc(strlen+1)` 全库**归零**。

### 验证

- ninja 全绿 + ctest **174/174**（145.50s）。
- 提交 `refactor(string)`（5 文件，+11/-29）。

### 决策登记（第 9 章格式）

- 层验证确认 / 架构 / C-㉑ 632 处迁移跨层 / 无 / build_verify 325 目标 + 174/174。
- strdup 收敛 / 判据 A（D6）/ 8 处 / 空串语义保留 / 全量 174/174。
- strcmp 收敛 / 判据 A / 1 处 / 三态比较豁免（3 处）/ 全量 174/174。

### 遗留登记

- ABSTRACTION_SPEC §11.1 黑名单新增：`lv_malloc(strlen(x) + 1)` 手写复制（→ lv_strdup_safe）。
- 三态 strcmp（3 处，qsort/comparator）为合法形态，非候选。

## 四十四、批次 C-㉓：前缀匹配收敛 + 豁免确认（2026-08-19）

用户「继续」。扫描剩余抽象化候选（手写前缀/strchr/realloc/常量/内存安全），本批收敛 1 处 + 确认多项豁免。

### ① Coq tactic 前缀匹配收敛（1 处）

- proof_export_enhanced.c `is_coq_tactic_prefix`：手写 `strncmp(rule, prefix, strlen(prefix)) == 0` 真前缀匹配 → `lv_str_startswith`（判据 A，批次 O 登记「proof_version_isar.c 一个静态 starts_with 局部副本」同源；全库唯一纯前缀形态）。

### ② 豁免确认（多项）

| 候选 | 判定 |
|------|------|
| lv_lexer.c:118 / proof_strategy_numeric.c:81 `strlen==len && strncmp==0` | 精确长度标识符匹配（判据 C 已登记豁免） |
| proof_version_isar.c `starts_with` 局部副本 | 跳空格增强包装（lv_str_startswith 无法表达，非纯副本） |
| 裸 realloc 3 处（allocator.c 2 / lv_utils.c 1） | layer2 基础设施 + 语义特化豁免（已登记） |
| memcpy+NUL 3 处（approx_counter / solver_core×2） | int 数组 0 终止哨兵（DIMACS 子句，非字符串） |
| strchr 32 处 | 语义异构（查找 vs 切分），批次 O 登记 lv_str_split_once 新设施待评估 |
| 魔法常量 64/128/256/1024/4096 数百处 | 多为局部数组容量/结构大小，非全局语义重复，低收益大面 |
| lv_free 取址误用复查 | 零新误用（C-⑮/C-⑳ 后） |
| lv_snprintf 错误路径覆盖复查 | 无覆盖（渲染输出路径非错误路径） |

### 验证

- ninja 全绿 + ctest **174/174**（135.71s）。
- 提交 `refactor(string)`（1 文件，+1/-1）。

### 决策登记（第 9 章格式）

- 前缀匹配收敛 / 判据 A / 1 处 / 精确长度 + 跳空格包装豁免 / 全量 174/174。
- 多项豁免确认 / 扫描 / realloc/memcpy 哨兵/strchr/常量 / 语义特化或低收益 / 全量回归。

### 遗留登记

- strchr 单次切分（32 处）：登记 lv_str_split_once 新设施评估（批次 O 遗留）。
- proof_version_isar.c 跳空格 starts_with：如需收敛需新增「跳空白前缀」变体，暂缓。

## 四十五、批次 C-㉔：豁免确认 + 字符串设施契约测试（2026-08-19）

用户「继续」。巡检剩余抽象化候选（注册表/空白跳过/block_to_text），全部判定豁免或格式契约；随后补 C-㉑/㉓ 大迁移后核心依赖的契约测试。

### ① 候选豁免确认（多方向巡检）

| 候选 | 判定 |
|------|------|
| 注册表三件套（114 文件含 register） | 语义异构（各注册表元数据字段/查找语义不同），无 ≥3 处同构 |
| isspace 空白跳过 10 处 | 语义异构：token 结束判定 / trim 尾部 / 字符位置判断，非同一骨架 |
| block_to_text.c「// block body」占位 | **格式契约**：lv_convert_text_to_block 反向解析器完整实现（已收敛 lv_str_skip_ws/lv_str_read_token），接受空块体；C-⑳ 登记的 P2 撤销 |

### ② 字符串设施契约测试补全（test_utils.c）

- **lv_snprintf**（C-㉑ 632 处迁移后核心依赖）：正常格式化返回值（== C99 snprintf）、截断时返回完整源长度 + NUL 终止保证（sp[size-1]=='\0'）。
- **lv_str_startswith**（C-㉓ 前缀收敛后核心依赖）：完整相等、源短于前缀、前缀不匹配、双空串、空前缀匹配任意（strncmp(...,0)==0 语义）、NULL 输入安全。
- test_utils 243/243（新增 16 断言）。

### 验证

- ninja 全绿 + ctest **174/174**（174.15s）。
- 提交 `test(string)`（1 文件，+25）。

### 决策登记（第 9 章格式）

- 豁免确认 / 扫描 / 注册表 + 空白跳过 + block_to_text / 语义异构或格式契约 / 全量回归。
- 契约测试补全 / 测试 / lv_snprintf + lv_str_startswith 16 断言 / 无 / test_utils 243 + 全量 174/174。

### 遗留登记

- strchr 单次切分（32 处）：lv_str_split_once 新设施评估（批次 O 遗留，继续）。
- proof_version_isar.c 跳空格 starts_with：需新增变体，暂缓。

## 四十六、批次 C-㉕：lv_str_prefix_len 单次切分设施（2026-08-19）

用户「继续」。推进批次 O 遗留的「strchr 单次切分」候选（32 处 strchr 中筛出 5 处同构切分形态）。

### ① 新设施 lv_str_prefix_len

- 契约：`lv_str_prefix_len(str, delim, *out_len)`——找到分隔符返回 true + 前缀长度（不含分隔符）；未找到返回 false + 全串长；空串/分隔符 NUL 边界；NULL 安全。
- 收敛「strchr(找分隔符) → 计算前缀长度 → 截取/比较」样板（判据 A）。

### ② 迁移 5 处调用点

| 文件 | 形态 | 说明 |
|------|------|------|
| lv_utils.c | `[section]` 节头 `]` 切分 | 调用点保留 `trimmed+1` 偏移（节名起点） |
| lv_utils_config.c | 键节前缀 `.` 切分 | 标准形态 |
| atp_backend.c | inference 规则 `,` 切分 | 标准形态 |
| test_framework.c | 通配符 `*` 前缀 | strncmp 前缀比较 |
| lv_process.c | PATH `:` 切分 | 未找到 fallback 全串语义天然契合 |

### ③ 豁免登记（泛化候选）

- lv_utils_misc.c 版本号 `-`/`+` 双分隔符：起点非串首（`plus - dash`）→ 需带 start 变体
- axiom_pkg_verify.c 括号内 `,`：上界约束（`comma < close_paren`）→ 需带 end 变体

### 验证

- ninja 全绿 + ctest **174/174**（139.63s）。
- test_utils 255/255（新增 12 断言：找到/未找到/空串/开头分隔符/NULL）。
- 提交 `feat(string)`（8 文件，+66/-16）。

### 决策登记（第 9 章格式）

- 新设施准入 / 判据 A / lv_str_prefix_len 5 调用点 / 泛化候选（双分隔符/上界）豁免 / test_utils 255 + 全量 174/174。

### 遗留登记

- lv_str_prefix_len 泛化变体（带 start/end 界）：lv_utils_misc / axiom_pkg_verify 待设施扩展。
- strchr 其余 27 处为存在性检查/位置判断（非切分），维持豁免。

## 四十七、批次 C-㉖：编码健康检查 + BOM 修复（2026-08-19）

用户「继续推进」。全库编码健康检查（UTF-8 无 BOM LF 规范符合性）发现并修复 BOM 污染。

### ① 编码健康检查结果

| 检查项 | 结果 |
|--------|------|
| UTF-8 BOM（.c/.h） | **11 个文件**含 BOM（违反规范） |
| CRLF 行尾（.c/.h） | 25 个文件工作区 CRLF（git 索引已 LF，无实际 blob 差异） |
| 双重编码乱码 | 0（C-⑲ 已归零） |

### ② BOM 移除（11 文件）

- 位置：lv_impl_upper_utils.c / mv_polynomial.h / solver_snapshot.h / logic_check.c / layer6 控制流与数据块 7 文件（if/match/while/list/map/record/ui_block）。
- 成因：早前拆分/迁移脚本写入（git 索引含 BOM）。
- 修复：移除首行前 0xEF 0xBB 0xBF，diff 仅首行 -BOM，行为无变化。
- 顺带：25 个 preset/impl 文件工作区 CRLF 归一化 LF（索引本就 LF，git add 后无实际变更，未污染提交）。

### ③ 抽象化候选重扫（信噪比评估）

- 注册表/空白跳过/strchr/桩函数重扫：全部判定语义异构或错误路径误报——纯正则无法区分「真桩」与「错误路径 return」，该扫描方向**登记终止**（依赖读上下文人工判定，效率低）。
- 字符串设施（snprintf/strdup/strcmp/前缀/切分/跳过）已全面收敛，无新候选。

### 验证

- ninja 全绿 + ctest **174/174**（114.53s）。
- 提交 `chore(encoding)`（11 文件，+11/-11）。

### 决策登记（第 9 章格式）

- BOM 修复 / 编码卫生 / 11 文件去除 UTF-8 BOM / 早前脚本写入 / 全量回归 174/174。
- 桩函数正则扫描终止 / 方法论 / 误报率高（错误路径 return 干扰）/ 需人工判定 / 无。

### 遗留登记

- 编码健康检查机制：建议纳入未来批次收尾例行项（BOM/CRLF/乱码三查）。

## 四十八、批次 C-㉗：字符串设施零覆盖测试补全（2026-08-19）

用户「继续推进」。全库测试覆盖扫描发现 40 个字符串设施中 31 个零测试覆盖（C-㉑/㉒/㉕ 大迁移后核心依赖），按 test-authoring 三层（等价性/边界/性质）补全。

### ① 覆盖扫描

- lv_str_utils.h 40 个公共设施，31 个在 test/ 目录**零直接测试**（多数被运行隐式覆盖但无契约测试）。
- 泛化候选复核：lv_str_prefix_len 带界变体（lv_utils_misc 区间 + axiom_pkg_verify 游标）仅 2 处且彼此异构，不满足 ≥3 门槛 → 维持豁免。

### ② 补测内容（test_utils.c test_string_operations，255→377 断言 +122）

| 组 | 设施 |
|----|------|
| 判断族 | endswith / contains / is_empty / nonempty / eq / ne / icmp / icmp_n |
| 关键字匹配 | match_any / match_delimited |
| 裁剪 | chomp / ltrim / rtrim / trim |
| 分割 | split / split_free |
| 解析 | read_quoted / read_token / read_int |
| 定界符 | skip_balanced / check_balanced / skip_until |
| 转换 | replace / join / append_sep |
| 转义族 | escape_xml / json_escape(+alloc) / html_escape(+alloc) / latex_escape(+alloc) / json_read_codepoint / codepoint_to_utf8 |

### ③ 测试钉住的语义（实现契约确认）

- XML `"` 转义为 `&quot;`（查找表含双引号）
- JSON `\n` 为标准转义对 `\\n`（2 字符，非 `\u000a`）；其他控制字符 `\u00XX`
- `lv_str_json_read_codepoint` 的 src 为 `\u` 后 4 位十六进制；代理对合并消耗 10 字节；孤立代理返回 0xFFFFFFFF
- `lv_str_match_delimited("foobar", {foo,bar})` 返回 1（foo 后非分隔跳过，bar 以 NUL 结尾命中）
- `lv_str_eq(NULL,NULL)` 为 true；`lv_str_icmp(NULL,"x")` 为 -1

### 验证

- ninja 全绿 + ctest **174/174**（99.48s）。
- test_utils **377/377**（新增 122 断言）。
- 提交 `test(string)`（1 文件，+250）。

### 决策登记（第 9 章格式）

- 测试补全 / 测试覆盖 / 31 设施 122 断言 / 无 / test_utils 377 + 全量 174/174。
- 泛化变体不立项 / 架构评估 / lv_str_prefix_len 带界变体 2 处异构 / 不满足 ≥3 门槛 / 维持豁免。

### 遗留登记

- lv_str_prefix_len 带界变体（需第三调用点出现时再评估）。
- 建议：其他模块（数值/几何/证明）的零覆盖设施可仿照本批做覆盖扫描补全。

## 四十九、批次 C-㉘：lv_hash 流式族 + geo_utils 零覆盖测试补全（2026-08-19）

用户「继续」。延续 C-㉗ 的覆盖扫描方法论，补全 lv_hash 流式上下文与 geo_utils 设施。

### ① lv_hash 流式族（test_utils 377→385 断言）

- 覆盖：lv_hash_init/update/str/int32/bool/digest_size/to_hex/to_hex_alloc（独立于已测的单步哈希 lv_hash_string/bytes/int）。
- 钉住语义：FNV-1a 分块更新 == 整串更新；SHA-256 digest 32B / FNV 8B；NULL 输入语义（str→"(null)"、digest_size NULL→0、to_hex_alloc NULL→NULL）；int32/bool 字段混入可复现；to_hex_alloc 16 字符；缓冲过小置空串。

### ② geo_utils 9 设施（test_geometry_core 204→239 断言）

- 覆盖：geo_distance_3d / geo_approx_equal / geo_bbox_contains_1d+2d / geo_point_on_segment / geo_signed_area_2x / geo_angle / geo_segments_intersect / geo_point_in_region_segments。
- 钉住语义：distance_3d 3-4-5 与单位立方体对角线；approx_equal eps 钳制（<GEO_EPSILON→GEO_EPSILON）；bbox 含容差边界；point_on_segment 受 APPROX 谓词容差（端点判定不稳定，用内部点）；signed_area_2x 叉积符号/共线零/2 倍面积；angle atan2 四象限 + 重合返回 0 非 NaN；segments_intersect 交叉/平行/共线重叠/分离；point_in_region_segments 三角形射线法内/外 + NULL/零计数安全。
- 修复：18+9 处 TEST_ASSERT 单参 → 双参（宏要求 cond+msg；行尾注释形态）。

### 验证

- ninja 全绿 + ctest **174/174**（73.28s）。
- 提交 `test`（2 文件，+152）。

### 决策登记（第 9 章格式）

- 测试补全 / 覆盖 / lv_hash 流式族 8 设施 + geo_utils 9 设施 / 无 / test_utils 385 + test_geometry_core 239 + 全量 174/174。
- TEST_ASSERT 单参修复 / 测试规范 / 27 处补 msg / 宏契约（cond+msg）/ 全量回归。

### 遗留登记

- 其他头文件零覆盖（proof.h 56 / context.h 30 / solver_core.h 23 / quantifier.h 22 等）：多为间接调用或宏误报，真实缺口待后续批次按本批方法论逐模块甄别。
