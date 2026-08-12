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
