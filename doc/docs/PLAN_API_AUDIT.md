# 规划文档蓝图 API 核对报告

> 核对对象：5 个规划/优化文档代码块中的 `lv_*` 符号（symbol_sync_check 按规划文档豁免的
> CONSTRAINT_PROOF_TEST_PLAN / DOCUMENT_GOVERNANCE_PLAN / PERFORMANCE_OPTIMIZATION /
> TEN_LAYER_INTEGRATION_PLAN / TEN_LAYER_OPTIMIZED_PLAN）。
> 核对基准：`core/include/lv` 全部头文件声明 + `core/src` 内部实现（提交 ca199d46 时点）。
> 结论标注：✅ 已有对应实现/替代；⚠️ 部分替代（语义接近但缺细节）；❌ 纯蓝图未实现。
> 方法：`tools/plan_api_audit.py` 提取符号 → 声明对照 → 按功能域人工语义核对。

**汇总**：151 个蓝图符号 = 40 ✅ + 14 ⚠️ + 97 ❌。
- CONSTRAINT_PROOF_TEST_PLAN / DOCUMENT_GOVERNANCE_PLAN / TEN_LAYER_INTEGRATION_PLAN：
  代码块内无 `lv_*` 符号（0 个），无需核对。
- PERFORMANCE_OPTIMIZATION.md：23 个（20 ✅ + 3 ❌）。
- TEN_LAYER_OPTIMIZED_PLAN.md：128 个（20 ✅ + 14 ⚠️ + 94 ❌）。

---

# 实现状态追加（2026-09，蓝图实现批次 G1-G6）

用户确认「97 ❌ + 14 ⚠️ 全部补齐」，排除：Python 集成 8 个（lv_py_*/lv_python_batch_call，
单独立项讨论）与插件安全组 12 个（G6，实现前单独过设计）。下述为已实现与状态。

## 已实现（G1-G6，ctest 296/296 全绿）

| 批次 | 覆盖 | 提交 | 验证 |
| --- | --- | --- | --- |
| G1a 错误域 | lv_get_error_message / error_category_name(_cn) / register/unregister_error_message / format_error / error_code_count + lvErrorCategory 映射库内 LV_CAT_* + 动态注册表 | df4f6237 | test_error_codes_ext 54 |
| G1b 约束 JSON | lv_constraint_to_json / from_json（含修复 constraint 类型表 size 6→8 潜在 bug） | f990bde3 | test_graph_serialize 126 |
| G1c 图 API | lv_graph_add_point / get_nodes_by_type / get_dependents / register_change_callback / on_node_changed / decompose（并查集连通分量） | 852885f0 | test_graph_traversal_ext 81 |
| G1d 预设 | lvPresetBlockDef + register/unregister/get + create_midpoint/centroid/circumcenter/orthocenter/incenter/reflection/translation | 93f1dae6 | test_func_block_preset 82 |
| G2a func_block 自定义 | CustomFunctionMeta/Registration + register_custom/unregister_custom/is_registered/get_meta/batch ×2 + call_custom 桥 | b9ab6a5f | test_func_block 334 |
| G2b fb_template | FuncBlockTemplate create/destroy/add_param/set_script/set_version/add_dependency/register/query/unregister/instantiate | bccf2c97 | test_func_block 360 |
| G2c DSL 扩展 | DslVersion + register/unregister_extension + version_parse/compare + syntax_transform | fed11fb3 | test_dsl_extension 44 |
| G3 几何 | lv_point_*/segment_*/triangle_*/intersect_* 23 个（坐标分量对适配签名） | bc0a2c1e | test_geometry_ops 114 |
| G4a 元数据/常量/缓存 | constraint_get_meta/type_from_name/python_class + symbolic init/free_constants + lv_SYM_* + lv_cache_*（8）+ insert_for_node | 6553335d | test_blueprint_g4 51 |
| G4b LRU | lv_lru_create/destroy/put/get/count/capacity | 2e50dcc3 | test_lru_cache 397 |
| G5a/b 安全宏+泄漏 | lv_CHECK_COORD/SAFE_DIV/validate_triangle/DEPTH_ENTER/LEAVE/STRCPY/STRCAT/REFCOUNT_* + leak_detector snapshot/report/assert_clean（读 allocator 追踪链表） | edf8ef80 | test_lv_safety 29 |
| G5-internal | lv_ENGINE_INTERNAL 宏 + lv_internal_get/add/remove/update_dependency（既有公开 API 薄转发） | 9ce34fbc | test_graph_traversal_ext 86 |
| G5c solver 增量 | lvIncrementalSolver create/destroy + solve_incremental + invalidate + mark_changed + solve_parallel（blueprint_ 前缀防与 L4 内部 solver_incremental 同名冲突） | 0e5b1fba→48415288 | test_blueprint_solver_incremental 40 |
| G6 插件安全 | lv_plugin_get_descriptor + verify_signature/add_trusted_key/set_enforcement + sandbox readonly/apply/check + permission 3 + audit_log + REQUIRE_PERMISSION 宏 + dsl_security_check + 扩展（设计文档 8a992bc8 确认后实现） | ea83858c | test_plugin_security 51 |

## 状态分组（更新后）

- **G1-G6 已实现**：原 ❌/⚠️ 中除 Python 8 个排除外全部落地为真实 API——函数名与蓝图一致，
  类型按库内适配（头文件注明签名适配）。G6 插件安全 13 API（ea83858c，设计文档确认后实现）：
  描述符登记/查询、SHA-256 签名校验+信任表+强制策略、沙箱配置记录模式（诚实标注非 OS 隔离）、
  三级权限+REQUIRE_PERMISSION 宏+审计日志、DSL 注入检测（6 内置模式+可扩展）。
- **Python 集成 8 个**：单独立项（需 CPython 嵌入产品决策），本批次不实现。
- **SIMD 4 函数导出**（✅ 替代但未导出）：仍登记 K 项待补声明。
- **Python 绑定 CI 修复**：ctypes 缺 c_size_t/c_int64/c_uint64 导入（2be85e36）——历史遗留
  high_dim 绑定 NameError 修复。

---

## PERFORMANCE_OPTIMIZATION.md（23 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_thread_pool_submit_simple | ✅ | `lv_thread_pool_submit`（thread_pool.h） |
| lv_thread_pool_parallel_for | ✅ | `lv_parallel_for`（thread_pool.h，语义等价） |
| lv_thread_pool_wait_all | ✅ | `lv_thread_pool_wait_group`（thread_pool.h） |
| lv_init_global_thread_pool | ✅ | `lv_get_global_thread_pool`（thread_pool.h） |
| lv_cleanup_global_thread_pool | ✅ | `lv_global_thread_pool_destroy`（thread_pool.h） |
| lv_simd_add_array_d | ✅ | src 内部已实现（simd_batch.c），未导出——如需公开需加声明 |
| lv_simd_sum_array_d | ✅ | src 内部已实现（simd_batch.c），未导出 |
| lv_simd_distance_array | ✅ | src 内部已实现（simd_geo_matrix.c），未导出 |
| lv_simd_cross2d_array | ✅ | src 内部已实现（simd_geo_matrix.c），未导出 |
| lv_hash_create | ✅ | `lv_hashtable_str/int/i64_create`（lv_hashtable.h） |
| lv_hash_insert | ✅ | `lv_hashtable_str/int/i64_insert`（lv_hashtable.h） |
| lv_hash_find | ✅ | `lv_hashtable_str/int/i64_get`（lv_hashtable.h） |
| lv_rtree_create | ✅ | `lv_aabb_tree_build` / `lv_aabb2d_build`（geo_aabb_tree.h） |
| lv_rtree_insert | ✅ | AABB 树整树构建（geo_aabb_tree.h）；单点动态插入需重建 |
| lv_rtree_query | ✅ | `lv_aabb_tree_query` / `lv_aabb2d_point/range/ray_query`（geo_aabb_tree.h） |
| lv_bench_suite_create | ✅ | `lv_perf_session_create` / `lv_benchmark_register`（performance_profiler.h / test_framework.h） |
| lv_bench_suite_add | ✅ | `lv_benchmark_register`（test_framework.h） |
| lv_bench_suite_run | ✅ | `lv_benchmark_run` / `lv_perf_benchmark_run` |
| lv_bench_suite_print_report | ✅ | `lv_perf_benchmark_print_result`（performance_profiler.h） |
| lv_bench_suite_to_json | ✅ | `lv_perf_report_to_json`（performance_profiler.h） |
| lv_lru_create | ❌ | 无 LRU 缓存实现（纯蓝图） |
| lv_lru_put | ❌ | 无（纯蓝图） |
| lv_lru_get | ❌ | 无（纯蓝图） |

## TEN_LAYER_OPTIMIZED_PLAN.md（128 个）

### 插件 / 沙箱 / 权限 / 审计（12 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_plugin_get_descriptor | ⚠️ | `lv_plugin_get_info_json`（plugin_system.h）信息近似，无 descriptor 结构 |
| lv_plugin_verify_signature | ❌ | 无签名验证（纯蓝图） |
| lv_plugin_add_trusted_key | ❌ | 无信任密钥管理（纯蓝图） |
| lv_plugin_set_enforcement | ❌ | 无强制策略（纯蓝图） |
| lv_sandbox_readonly / apply / check | ❌ | 无沙箱（纯蓝图） |
| lv_REQUIRE_PERMISSION | ❌ | 无权限宏（纯蓝图） |
| lv_plugin_get_permission | ❌ | 无权限模型（纯蓝图） |
| lv_audit_log | ❌ | 无审计日志（纯蓝图） |
| lv_perm_level_str | ❌ | 无权限级别（纯蓝图） |
| lv_dsl_security_check | ❌ | 无 DSL 安全检查（纯蓝图） |

### 函数块自定义注册（7 个，纯蓝图 ❌）
lv_func_block_register_custom / unregister_custom / is_custom_registered /
get_custom_meta / register_custom_batch / unregister_custom_batch — func_block_registry.h
仅有 `lv_func_block_registry_cleanup`，无注册/查询 API。

### 函数块模板 fb_template（10 个，纯蓝图 ❌）
lv_fb_template_create / destroy / add_param / set_script / set_version /
add_dependency / register / query / unregister / instantiate — 无任何实现。

### Python 集成（8 个，纯蓝图 ❌）
lv_py_register_runtime_hooks / register_constructor / register_solver /
register_rewrite_rule / load_module / get_last_error / clear_error / lv_python_batch_call —
无 Python 绑定（examples/demo.py 已删，印证无绑定面）。

### DSL 扩展（5 个，纯蓝图 ❌）
lv_dsl_register_extension / unregister_extension / version_parse / version_compare /
syntax_transform — 无 DSL 扩展注册机制（lv_LANGUAGE_SPEC 仅为规范文档）。

### 错误域（7 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_get_error_message | ✅ | `lv_get_last_error_message`（error_codes.h）/ `lv_error_message`（lv_error.h） |
| lv_error_category_name | ✅ | `lv_error_name` / `lv_error_category`（error_codes.h） |
| lv_error_category_name_cn | ⚠️ | 英文名已有，无中文名映射 |
| lv_format_error | ✅ | `lv_error_format_chain`（lv_error.h） |
| lv_error_code_count | ✅ | `lv_error_table_size`（lv_internal.h） |
| lv_register_error_message | ❌ | 无错误码注册扩展（纯蓝图） |
| lv_unregister_error_message | ❌ | 无（纯蓝图） |

### 引擎内部 / 图内部（5 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_ENGINE_INTERNAL | ❌ | 无 engine 内部结构体访问宏（纯蓝图） |
| lv_RETURN_IF_OOM | ✅ | `lv_RETURN_ERROR` / `lv_CHECK_ALLOC` 系列（error_codes.h / lv_check.h） |
| lv_internal_get_node / add_node / remove_node / update_dependency | ❌ | 无约束图内部节点 API（纯蓝图） |

### 几何：点 / 线段 / 三角形 / 相交（23 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_point_distance_sq | ❌ | 无（纯蓝图） |
| lv_point_midpoint | ❌ | 无（纯蓝图） |
| lv_point_is_collinear | ✅ | `lv_orient2d`（geo_predicate.h）判零即共线 |
| lv_intersect_lines | ⚠️ | `lv_lines_parallel`（geo_predicate.h）判平行，无交点求解 |
| lv_intersect_circles / lv_intersect_line_circle | ❌ | 无（纯蓝图） |
| lv_triangle_area | ⚠️ | 可由 `lv_orient2d` 结果 /2 推导，无独立 API |
| lv_triangle_circumcenter / orthocenter / incenter / centroid / nine_point_center / excenter / inradius / circumradius | ❌ | 无三角形心/半径 API（纯蓝图） |
| lv_segment_length_sq / midpoint / direction | ❌ | 无（纯蓝图） |
| lv_segment_is_parallel | ✅ | `lv_lines_parallel`（geo_predicate.h） |
| lv_segment_is_perpendicular | ✅ | `lv_lines_perpendicular`（geo_predicate.h） |
| lv_segment_intersection | ✅ | `lv_segments_intersect`（geo_predicate.h） |
| lv_segment_contains_point | ⚠️ | `lv_segment_side`（geo_predicate.h）部分近似（需自行限定范围） |
| lv_segment_distance_to_point | ❌ | 无（纯蓝图） |

### 约束图 API（6 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_graph_register_change_callback | ❌ | 无变更回调注册（纯蓝图） |
| lv_graph_get_nodes_by_type | ❌ | 无按类型查询（纯蓝图；遍历 API 见 lv_graph_traversal.h） |
| lv_graph_add_point | ⚠️ | 约束图节点添加有内部路径（lv_add_node_result_to_error），无公开 add_point |
| lv_graph_on_node_changed | ❌ | 无节点变更通知（纯蓝图） |
| lv_graph_get_dependents | ❌ | 无依赖查询（纯蓝图） |
| lv_graph_decompose | ❌ | 无图分解（纯蓝图） |

### 约束元数据 / JSON（5 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_constraint_get_meta | ⚠️ | `lv_constraint_type_name` / `lv_constraint_type_alias`（constraint_graph.h）部分近似 |
| lv_constraint_type_from_name | ⚠️ | 有 X-macro 类型表 + `lv_error_code_from_string` 模式，无同名函数 |
| lv_constraint_type_from_python_class | ❌ | 无 Python 类映射（纯蓝图） |
| lv_constraint_to_json / from_json | ❌ | 无约束 JSON 序列化（纯蓝图） |

### 预设注册 / 创建（10 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_preset_register | ✅ | `lv_preset_register_helper`（preset_common.h）+ preset_*_geometry.h 注册表 |
| lv_preset_unregister | ❌ | 无按名注销（纯蓝图） |
| lv_preset_get | ❌ | 无按名查询（纯蓝图） |
| lv_preset_create_midpoint / circumcenter / centroid / orthocenter / incenter / reflection / translation | ❌ | 无单点构造函数（纯蓝图；中点/外心等预设块存在与否需查 preset_*_geometry 注册表数据） |

### 符号坐标常量（2 个，纯蓝图 ❌）
lv_symbolic_coord_init_constants / free_constants — symbolic_coord.h 无实现。

### 证明迭代（2 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_proof_get_step_count | ✅ | `lv_proof_object_get_step_count` / `lv_proof_trace_get_step_count`（proof_compiler.h / proof_trace.h） |
| lv_proof_get_step | ✅ | `lv_proof_trace_get_rule` / `lv_proof_trace_get_step`（proof_trace.h） |

### 符号坐标缓存（8 个，纯蓝图 ❌）
lv_cache_create / destroy / lookup / insert / invalidate / invalidate_by_node / hit_rate —
无符号坐标缓存（仅 `lv_cache_line_size` 硬件查询宏）。

### 分层内存池（5 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_tiered_pool_create | ✅ | `lv_arena_create` / `lv_mempool_create`（lv_arena.h / lv_mempool.h） |
| lv_tiered_alloc | ✅ | `lv_arena_alloc` / `lv_mempool_alloc` |
| lv_tiered_free | ✅ | `lv_arena_reset_to_mark` / `lv_mempool_free` |
| lv_temp_alloc | ✅ | `lv_arena_tmp`（lv_arena.h，临时分配语义等价） |
| lv_pool_check_leak | ✅ | `lv_memory_leak_report`（lv_utils.h）/ `lv_mem_print_stats`（memory_pool.h） |

### 增量 / 并行求解（3 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_solve_incremental | ⚠️ | `lv_solver_solve_under_assumptions`（solver_core.h）部分近似，无增量重解 |
| lv_incremental_solver_invalidate | ❌ | 无失效标记（纯蓝图） |
| lv_solve_parallel | ❌ | 无并行求解（纯蓝图；可用 lv_parallel_for 组合） |

### 安全宏（8 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_CHECK_COORD | ❌ | 无坐标校验宏（纯蓝图） |
| lv_SAFE_DIV | ⚠️ | `lv_SAFE_ADD/SUB/MUL`（three_layer_arithmetic.h）有，无除法变体 |
| lv_validate_triangle | ❌ | 无三角形不等式校验（纯蓝图） |
| lv_DEPTH_ENTER / lv_DEPTH_LEAVE | ⚠️ | `lv_DEPTH_LIMIT_*`（depth_limits.h）深度限制宏存在，无 enter/leave 守卫对 |
| lv_STRCPY | ✅ | `lv_strlcpy` / `lv_strlcpy_n`（lv_utils.h） |
| lv_STRCAT | ✅ | `lv_strlcat` / `lv_strncat`（lv_utils.h） |
| lv_REFCOUNT_INIT | ❌ | 无引用计数宏（纯蓝图） |

### 泄漏检测（3 个）

| 蓝图 API | 结论 | 已有对应/替代 |
| --- | --- | --- |
| lv_leak_detector_snapshot | ⚠️ | `lv_mem_get_global_stats`（memory_pool.h）部分近似 |
| lv_leak_detector_report | ✅ | `lv_memory_leak_report`（lv_utils.h） |
| lv_leak_detector_assert_clean | ⚠️ | `lv_memory_check_poison` / `lv_memory_check_magic`（lv_utils.h）部分近似 |

---

## 结论与建议

1. **绝大部分（97/151）是纯蓝图**：集中在插件安全（签名/沙箱/权限/审计）、函数块自定义注册、
   fb_template、Python 集成、DSL 扩展、符号缓存、几何点/三角形高级 API——这些与当前库
   的实际能力边界一致，规划文档作为「未来设计」保留，非幻影 API（与 K5 教学面判定不冲突）。
2. **40 个已有等价替代**：线程池 / SIMD（已实现未导出）/ 哈希表 / AABB 树 / 基准套件 /
   错误域 / 预设注册 / 证明迭代 / 分层内存池 / 字符串安全宏 / 泄漏报告——文档若被当作
   使用手册阅读会误导，建议在文档中补充「实现现状」标注或链接本报告。
3. **SIMD 4 个函数已实现但未导出**（simd_batch.c / simd_geo_matrix.c）：若外部需要，
   可后续在 simd_ops.h 补声明导出（登记 K 项）。
4. **13 个 ⚠️ 部分替代**：多为「有谓词/类型表、无完整组合 API」，属可接受现状；
   如需补齐可单独立项，本报告不展开。
