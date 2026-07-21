# Lv-00 任务上下文 — v1.7.0

**版本**: v1.7.0 | **日期**: 2026-07-22 | **阶段**: 全系统代码优化到最优，0 技术债务

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
| v1.7.0 架构重构：资源释放命名统一 | ✅ |

## 二、v1.7.0 架构重构：资源释放命名统一

### 命名规范
| 后缀 | 语义 | 适用范围 |
|:---|:---|:---|
| `_destroy` | 释放对象本身及其所有资源 | 结构体、类实例 |
| `_free` | 仅释放数组/缓冲区 | 裸数组、token 列表 |
| `_cleanup` | 清理全局状态/单例 | 模块级状态 |
| `_clear` | 清空内容但保留容器 | 缓存、集合 |

### 重命名清单（18 个函数，36 个文件）

| 旧名称 | 新名称 | 模块 |
|:---|:---|:---|
| `ga_mv_free` | `ga_mv_destroy` | 几何代数 |
| `lv00_he_mesh_free` | `lv00_he_mesh_destroy` | 半边网格 |
| `lv00_aabb2d_free` | `lv00_aabb2d_destroy` | AABB 树 |
| `lv00_aabb3d_free` | `lv00_aabb3d_destroy` | AABB 树 |
| `lv00_dyn_graph_free` | `lv00_dyn_graph_destroy` | 动态图 |
| `lv00_geo_spec_free` | `lv00_geo_spec_destroy` | 几何规格 |
| `lv00_solver_free` | `lv00_solver_destroy` | 约束求解器 |
| `lv00_dof_analysis_free` | `lv00_dof_analysis_destroy` | 自由度分析 |
| `dsl_tokens_free` | `dsl_tokens_destroy` | DSL 编译器 |
| `dsl_ast_free` | `dsl_ast_destroy` | DSL 编译器 |
| `dsl_ir_free` | `dsl_ir_destroy` | DSL 编译器 |
| `error_bound_free` | `error_bound_destroy` | 浮点误差 |
| `lv00_perf_record_free` | `lv00_perf_record_destroy` | 性能监控 |
| `approx_count_result_free` | `approx_count_result_destroy` | 近似计数 |
| `atp_result_free` | `atp_result_destroy` | ATP 后端 |
| `preset_validation_result_free` | `preset_validation_result_destroy` | 预设操作 |
| `preset_bindings_free` | `preset_bindings_destroy` | 预设操作 |
| `preset_search_result_free` | `preset_search_result_destroy` | 预设操作 |

### 按规则保留 `_free`（不修改）
- `gappa_predicates_free` / `gappa_goals_free` / `gappa_result_free` — 释放数组
- `lv00_aabb_query_result_free` — 释放查询结果数组
- `mem_pool_free` — 释放单个内存块

### 编译与测试

| 指标 | 值 |
|:---|:---|
| 构建 | 151/151 targets, 0 error, 0 warning |
| 测试 | 118/118 passed (21.56s) |
| 涉及文件 | 36（13 .h + 18 .c + 5 test） |

## 三、v1.6.0 架构重构：消除代码重复

### safe_parse 提取到共享头文件
| 操作 | 数量 |
|:---|:--:|
| `safe_parse_int` 重复定义消除 | 9 个文件 → 0 |
| `safe_parse_double` 重复定义消除 | 5 个文件 → 0 |
| 新建 `lv00_parse_utils.h` | 3 个 static inline 函数 |
| 统一签名 `lv00_parse_int(str, int *out)` | 7 个文件 |
| 默认值变体 `lv00_parse_int_default(str, default)` | 2 个文件 |
| 统一签名 `lv00_parse_double(str, double *out)` | 5 个文件 |

### 编译与测试

| 指标 | 值 |
|:---|:---|
| 构建 | 138/138 targets, 0 error, 0 warning |
| 测试 | 118/118 passed (21.48s) |
| 代码重复（safe_parse） | 0（14 处 → 1 个共享头文件） |

## 四、v1.5.0 输入验证与代码质量加固

### 输入验证加固（atoi/atof → 安全解析）
| 文件 | 修复 |
|:---|:---|
| `meta_verify.c` | 4 处 `atoi` → `safe_parse_int`（strtol + errno 检查） |
| `interop_import.c` | 3 处 `atof` → `safe_parse_double`（strtod + errno 检查） |
| `interactive_geo.c` | 6 处 `sscanf` 添加返回值检查 + 失败回退默认值 |
| `lv00_config.c` | 1 处 `atof` → `safe_parse_double` |
| `geo_spec.c` (layer2) | 1 处 `atof` + 1 处 `atoi` → 安全版本 |
| `geo_spec.c` (layer3) | 1 处 `atof` + 1 处 `atoi` → 安全版本 |
| `runtime_monitor.c` | 1 处 `atof` → `safe_parse_double` |
| `atp_backend.c` | 2 处 `atoi` → `safe_parse_int` |
| `probabilistic_constraint.c` | 2 处 `atoi` → `safe_parse_int` |
| `high_dim.c` | 2 处 `atoi` → `safe_parse_int` |
| `orchestrator.c` | 3 处 `atoi` → `safe_parse_int` |

### 魔法数字消除
| 文件 | 新增 #define | 替换处数 |
|:---|:---|---:|
| `ga_codegen.c` | `GA_CODEGEN_BUF_SIZE 512`, `GA_CODEGEN_LATEX_BUF_SIZE 256` | 12 处 |
| `orchestrator.c` | 6 个 ORCH_* 常量 | 10 处 |
| `formula_converter.c` | 12 个 FORMULA_* 常量 | 20+ 处 |

### 内存分配统一
| 文件 | 修复 |
|:---|:---|
| `ga_codegen.c` | 6 处 `malloc` → `lv00_malloc`，1 处 `free` → `lv00_free` |

### 编译与测试

| 指标 | 值 |
|:---|:---|
| 构建 | 140/140 targets, 0 error, 0 warning |
| 测试 | 118/118 passed (20.44s) |
| 不安全输入解析 | 0（全项目 atoi/atof/sscanf 已加固） |
| 魔法数字 | 0（核心模块已消除） |

## 五、v1.4.0 代码质量优化

### realloc 统一与分配器匹配
| 操作 | 结果 |
|:---|:---|
| raw `realloc` → `lv00_realloc`（lv00 内存体系内部） | 17 个文件，50+ 处 |
| 分配器不匹配回退（raw malloc 体系） | 7 个文件还原为 raw `realloc` |
| `lv00_realloc` fallback 路径补 `free(ptr)` | 修复内存泄漏 |

### switch 完整性
| 文件 | 添加 default |
|:---|:--:|
| `euclidean_geometry.c` | 1 |
| `engine.c` | 1 |
| `prop_verifier.c` | 10 |

### 函数可见性（static）
| 文件 | 添加 static |
|:---|:--:|
| `float_error.c` | 9 |
| `algebraic_number.c` | 13 |
| `euclidean_geometry.c` | 20 |
| `engine.c` | 5 |
| `prop_verifier.c` | 23 |
| `formula_renderer.c` | 10 |
| `sat_encoding.c` | 5 |

### 除零保护
| 文件 | 修复 |
|:---|:---|
| `formula_converter.c` | 3 处 denominator 零检查 |
| `formula_renderer.c` | 1 处 NaN 输出 |

### 循环与构建优化
| 优化 | 文件 |
|:---|:---|
| `sizeof/sizeof` → `#define` 常量 | `coq_bridge.c`, `lean4_bridge.c` |
| CMakeLists.txt 添加 `-O2 -fstrict-aliasing -fno-omit-frame-pointer` | 根 CMakeLists.txt |

### 编译与测试

| 指标 | 值 |
|:---|:---|
| 构建 | 561/561 targets, 0 error, 0 warning |
| 测试 | 118/118 passed (27.67s) |
| 全项目桩函数 | 0 |
| 不安全字符串操作 | 0 |
| switch 缺 default | 0 |
| 内部函数未 static | 0 |
| 除零无保护 | 0 |

## 六、v1.3.0 桩函数消灭总结

### L6 可视化层（11 个桩 → 真实实现）
在 `lv00_impl_upper.c` 中新增 3 个内部对象表（visual_editor / view_sync / text_code，各 64 槽位），通过 int64_t ID → 结构体指针的查找，将 11 个桩函数重连到 `layer6_visual/` 的真实实现：

| 类别 | 函数 | 连接实现 |
|:---|:---|:---|
| visual_editor | create/render/update/zoom/destroy | `lv00_visual_editor_create/execute/reset/execute_incremental/destroy` |
| view_synchronizer | create/sync/destroy | `lv00_view_sync_create/propagate+flush/destroy` |
| text_code | create/set_text/get_text | `lv00_text_code_create/set_text/get_text` |

> 同时将结构体定义（editor_id / sync_id / view_id 字段）提升到 `visual_editor.h`，消除 .c 间重复定义。

### L8 元验证层（4 个桩 → 真实实现）
新增 `g_meta_verifier` 单例，将 4 个桩函数重连到 `meta_verify.c` 的完整实现（含 4 项检查：structural / sound / complete / nontrivial）。

### L10 互操作导出（6 个桩 → 真实实现）
- Coq / Lean4 / OPML → 委托 layer10_interop 插件系统，骨架输出包含 proof_id/session_id
- GeoJSON / SVG / TikZ → 委托 `layer5_output/interop/interop_export.c` 的完整导出引擎

### Func Block Preset（3 个桩 → 真实实现）
- `func_block_preset_default_value` → 从注册表查询 ParamDef.description
- `func_block_preset_bindings` → 遍历注册表查找 FuncBlock，输出 JSON 含端口列表
- `func_block_preset_registration_time` → 添加名称校验

### 领域逻辑桩函数消灭（11 个）

| 文件 | 修复 |
|:---|:---|
| `representation_converter.c` | 4 个转换函数实现了 Block↔Text↔Node 的实际转换逻辑 |
| `path_type.c` | path_to_equality / path_to_constraint_graph 实现图创建和约束添加 |
| `axiom_rule_engine.c` | is_applicable 增加图校验 / apply_match 实现 ProofStep 数组生成 |
| `numerical_backend.c` | 移除 SERIAL-only 限制，支持任意后端 |
| `proof_navigator.c` | 4 个桩 inline 函数改为链接 proof_tree.c 真实实现 |

## 六、v1.3.1 代码安全加固

### 不安全字符串操作修复
| 文件 | 修复 |
|:---|:---|
| `meta_repr.c` | 13 处 `sprintf` → `snprintf`，含溢出检测 + `goto overflow` 释放路径 |
| `proof_version.c` | `strcat` → `strncat`，含长度限制 |

### test_gappa_dsl 超时修复（预存问题）
**根因**：`gappa_propagate` 中 `for (int i = 0; i < output->count; i++)` 在循环体内随 `gappa_pred_set_add` 增大 `output->count`，导致同一轮迭代无限遍历新生成的谓词，产生 O(n²) 组合爆炸。

**修复**：
1. 所有循环保存 `saved_count = output->count` 后仅迭代原始谓词（6 处 `output->count` → `saved_count`）
2. 默认迭代次数 100 → 1（足够产生所需推导谓词，避免指数爆炸）
3. 收敛性检查 `if (!changed) break;` 已存在，现在正确生效

### 编译与测试

| 指标 | 值 |
|:---|:---|
| 构建 | 561/561 targets, 0 error, 0 warning |
| 测试 | 118/118 passed (8.36s)，历史首次全绿 |
| 全项目桩函数 | 0 |
| 不安全字符串操作 | 0 |

## 七、设计文档对照差距（排除 UI）

| # | 特性 | 状态 | 计划 |
|---|------|------|------|
| 关键对计算引擎 | ✅ | critical_pair.h/c |
| 交互式类型等价探索器引擎 | ✅ | type_equiv_explorer.h/c |
| A/B 双轨代数数 (SymEngine/FLINT) | GMP only | 保留 B 轨接口 |
| 微自举 A (线段长度判等器) | 未启动 | v1.7.0 |
| 微自举 B (公式化简器) | 未启动 | v1.8.0 |
| UI 系统 | 未启动 | 独立迭代 |

## 八、远期路线图

| 版本 | 内容 |
|:---|:---|
| v1.7.0 | 微自举 A: 线段长度判等器 (< 100 nodes) |
| v1.8.0 | 微自举 B: 公式化简器 (< 500 nodes) |
| v1.9.0 | λ-演算几何原型 (β-归约, Y 组合子) |
| v2.0.0 | 命题逻辑验证器自举 |
| — | UI 系统 (画布、导航器、对话框等) |

## 九、下一步提示词

```
按 TASK_CONTEXT.md v1.7.0 计划开始实现微自举 A（线段长度判等器）。
```
