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
