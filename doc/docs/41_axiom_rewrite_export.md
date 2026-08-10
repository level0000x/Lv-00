# 41 公理包重写与导出（Axiom Package Rewrite & Export）

## 模块概述

本文档描述 Lv-00 中"公理包 → 安全重写规则 → 证明编译导出 → 版本校验"的完整链路。该链路将几何公理包（`axiom_pkg.h`）中的约束模板与不可构造性记录，转化为可安全应用的重写规则（`rewrite.h`），再经由证明编译器（`proof_compiler.h`）与互操作系统（`interop.h`）导出为 Coq、Lean 与 OPML（JSON 通道）等外部格式，并利用内容哈希与模块版本约束（`module.h`、`proof_version.h`）保证公理包升级后的引用有效性。

**覆盖头文件**：

- `axiom_pkg.h` —— 公理包：约束模板、不可构造性记录、依赖引用追踪
- `rewrite.h` —— 图重写引擎：VF2 匹配、归约度量、WL 循环检测、Maude 风格策略
- `module.h` —— 模块系统：公理包聚合、依赖约束与版本哈希
- `proof_compiler.h` —— 证明编译层：证明对象、证明跟踪、多格式输出
- `interop.h` —— 外部互操作：Coq/Lean 导出、定理交换、插件桥接
- `lv_export_common.h` —— 导出公共工具：XML 转义与文件写出
- `proof_version.h` —— 证明仓库版本控制：提交、分支、Diff
- `proof_step_strategy.h` —— 步骤策略 vtable（Coq 导出回调）

---

## 核心设计原则

1. **信任颜色驱动**：公理包中每个命题具有 `PropositionKind`（构造性 / 非构造性预言 / 爆炸原理），依赖引用区分内引用（内容哈希验证）、外引用（公认文献）与作者断言，未经验证者自动降级为黄色，禁止直接进入导出通道。
2. **安全重写 = 终止性 + 事务性**：每条 `RewriteRule` 携带 `reduction_measure`（归约度量），应用前后通过 `rewrite_validate_measure` 验证度量确实下降；替换以 `GraphSnapshot` 事务包裹，冲突时回滚；WL 图核哈希环形缓冲区检测重写循环。
3. **证明对象机器可复核**：`lvProofObject` 保存完整步骤链、前提 ID、公理/假设集合，`lv_proof_object_verify` 可独立重放校验，导出格式只是同一对象的投影。
4. **版本校验双轨制**：内容层用 SHA-256 内容哈希（`axiom_package_compute_content_hash`、`module_compute_content_hash`）检测变化；语义层用版本约束解析（`module_compare_versions`、`module_parse_version_constraint`）判定依赖是否满足。
5. **导出即提交**：外部导出结果（Coq/Lean/OPML）连同源公理包版本、信任基信息一起进入 `proof_repo` 版本仓库，实现可追溯的证明流水线。

---

## 关键数据结构

```c
/* —— 公理包（axiom_pkg.h，结构已公开） —— */
struct AxiomPackage {
    char *name;                 /* 公理包名称 */
    char *version;              /* 版本号 */
    lvDArray templates;         /* ConstraintTemplate 数组 */
    lvDArray known_unconstructibles;  /* 已知不可构造问题 */
    lvDArray unconstructible_templates; /* 不可构造性证明模板 */
    char *bottom_geometry;      /* 底层几何类型 */
    char *negation_encoding;    /* 否定编码方法 */
    int contradiction_behavior; /* 矛盾行为 */
    lvDArray expansion_cache;   /* 模板展开缓存 */
    int max_expansion_depth;    /* 默认 8 */
    lvDArray dep_refs;          /* DependencyRef 依赖引用 */
};

/* 依赖引用：内引用/外引用/作者断言 */
typedef struct {
    char ref_id[64];
    char content_hash[65];   /* SHA-256 十六进制 */
    int dependent_node_id;
    int original_color;      /* DEP_TRUST_GREEN..DEP_TRUST_RED */
    RefType ref_type;        /* REF_INTERNAL / REF_EXTERNAL / REF_AUTHOR */
    char external_ref[256];
    char trust_comment[256];
    bool hash_valid;
} DependencyRef;

/* —— 安全重写规则（rewrite.h） —— */
typedef struct RewriteRule {
    RewritePattern *pattern;          /* 模式（节点绑定 + 约束模板） */
    RewriteReplacement *replacement;  /* 替换（节点绑定 + 新节点） */
    int reduction_measure;            /* 归约度量（终止性保证） */
    char *name;
    RewritePrecondition condition_func; /* 前置条件回调 */
    void *condition_data;
} RewriteRule;

/* —— 证明对象（proof_compiler.h） —— */
struct lvProofObject {
    int proof_id;
    char *theorem_name;
    Proposition *goal;
    bool is_proved;
    ProofColor final_color;
    lvProofStepRecord **steps;  /* 步骤链 */
    int step_count;
    int *axiom_ids;             /* 使用的公理 ID */
    int axiom_count;
    int *assumption_ids;        /* 假设 ID */
    int max_depth;
    int64_t elapsed_us;
};

/* —— 证明仓库提交（proof_version.h） —— */
typedef struct {
    char oid[lv_OID_LENGTH];   /* 64 hex + NUL */
    char message[lv_COMMIT_MSG_MAX];
    char parent_oid[lv_OID_LENGTH];
    int64_t timestamp;
} lvProofCommit;
```

---

## 主要接口

### 公理包生命周期与持久化

| 接口 | 说明 |
| --- | --- |
| `lv_axiom_package_create(name, version)` / `axiom_package_destroy` | 创建/销毁公理包 |
| `axiom_package_load(pkg, filepath)` / `axiom_package_save` | LVZ 加载/保存，返回 `AxiomLoadStatus` / `AxiomSaveStatus` |
| `axiom_package_register_template` / `get_template` / `get_template_by_index` | 约束模板注册与查询 |
| `axiom_template_set_level` / `expand_lazy` / `compress` | 模板分级管理与惰性展开（二级模板压缩态） |
| `axiom_package_lookup_expansion_cache` / `store_expansion_cache` / `clear_expansion_cache` | 模板展开缓存（参数哈希键） |
| `axiom_template_run_tests` / `axiom_template_test_run` / `test_case_create` | 双层测试集（出厂/用户）执行与结果统计 |
| `axiom_package_add_known_unconstructible` / `lookup_unconstructible` | 已知不可构造问题管理 |
| `axiom_package_add_unconstructible_template` / `lookup_unconstructible_template` / `verify_unconstructible` | 不可构造性归约证明模板 |

### 安全重写

| 接口 | 说明 |
| --- | --- |
| `rewrite_rule_create(name, pattern, replacement, measure)` / `rewrite_rule_destroy` | 创建带归约度量的规则 |
| `rewrite_rules_load_from_file(filepath, ...)` / `rewrite_rule_unload` | `.lvz` 规则热加载/卸载 |
| `find_rewrite_match` / `vf2_find_match` / `find_best_match` | VF2 子图同构匹配 |
| `apply_rewrite` / `rewrite_with_rules` / `rewrite_apply_all_matches` | 应用与批量应用（含快照事务） |
| `rewrite_validate_measure` | 归约度量验证（应用后是否确实下降） |
| `detect_rewrite_loop_wl` / `rewrite_compute_wl_hash` / `wl_history_init` | WL 图核哈希循环检测 |
| `graph_snapshot_create` / `graph_snapshot_restore` / `graph_snapshot_destroy` | 图快照事务回滚 |
| `rewrite_strategy_apply` / `rewrite_search_backward` | Maude 风格策略执行 / 逆向证明搜索 |

### 证明编译导出（proof_compiler.h）

| 接口 | 说明 |
| --- | --- |
| `lv_proof_object_add_step` / `add_axiom` / `add_assumption` / `verify` | 证明对象构建与重放校验 |
| `lv_proof_compiler_compile(compiler, proof, trace)` | 按配置编译为 JSON/LaTeX/TikZ/Text/XML/Graphviz |
| `lv_proof_compiler_to_json` / `to_latex` / `to_tikz` / `to_text` / `to_graphviz` | 单格式快捷导出 |
| `lv_proof_export_to_file(proof, trace, format, filename)` | 导出到文件 |
| `lv_proof_trace_start` / `step` / `backtrack` / `branch` / `contradiction` / `complete` | 证明跟踪事件流 |

### Coq / Lean / OPML 互操作（interop.h）

| 接口 | 说明 |
| --- | --- |
| `interop_export_coq(proof, config)` / `interop_export_lean(proof, config)` | 导出 Coq / Lean 4 证明脚本 |
| `interop_theorem_context_create(trust_base_name, trust_base_version)` / `destroy` | 定理交换上下文（含信任基版本） |
| `interop_theorem_add_call` / `interop_theorem_export_calls` | 调用序列记录与按格式导出 |
| `interop_import_external_theorem(engine, name, hash, desc, &block_id)` | 外部定理导入为可信基块 |
| `lv_interop_register_plugin` / `lv_interop_reset_plugins` | 外部证明系统插件注册（`lv_EXT_COQ` / `lv_EXT_LEAN4` / `lv_EXT_JSON`(OPML)） |
| `interop_parse_export_format` / `interop_export_format_name` | 格式解析与名称映射 |
| `lv_export_xml_escape` / `lv_export_write_file` | 定长缓冲 XML 转义 / 文件写出样板（lv_export_common.h） |
| `proof_step_get_strategy(type)` | 步骤策略 vtable（`ProofStepExportCoqFn` 导出回调） |

### 版本校验（module.h / proof_version.h）

| 接口 | 说明 |
| --- | --- |
| `axiom_package_compute_content_hash(pkg)` | 公理包 SHA-256 内容哈希（65 字符 hex 串） |
| `module_compute_content_hash(mod)` / `module_compute_version_hash(mod)` | 模块内容/版本哈希 |
| `module_compare_versions(v1, v2)` / `module_parse_version_constraint` | 语义版本比较与约束解析 |
| `module_validate_dependency_chain` / `module_detect_circular_dependency` / `module_full_cycle_detect` | 依赖链与循环检测 |
| `module_add_axiom_package` / `module_export_function_block` | 公理包聚合与导出项管理 |
| `axiom_package_validate_dependencies_with_hashes` / `auto_degrade_invalidated` | 失效引用定位与自动降级（GREEN→YELLOW） |
| `axiom_package_add_internal_ref` / `add_external_ref` / `add_author_assertion` / `reverify_lemmas` / `mark_lemma_stale` | 依赖引用注册与引理自动重验循环 |
| `proof_repo_init` / `open` / `commit` / `log` / `diff` / `branch` / `checkout` | 证明仓库版本控制 |

---

## 工作流程

1. **公理包装配**：`lv_axiom_package_create` 创建公理包，注册约束模板与已知不可构造问题；经 `axiom_template_run_tests` 双层测试集验证后置为已验证状态。
2. **安全重写规则生成**：从已验证模板生成 `RewriteRule`（携带 `reduction_measure` 与前置条件），通过 `rewrite_rules_load_from_file` 热加载；匹配用 VF2，应用前建 `GraphSnapshot`，应用后以 `rewrite_validate_measure` 确认度量下降、以 WL 哈希检测循环，冲突即回滚。
3. **证明构建与校验**：`lv_proof_object_add_step/add_axiom/add_assumption` 组装 `lvProofObject`，`lv_proof_object_verify` 重放校验；过程事件写入 `lvProofTrace`。
4. **编译与互操作导出**：`lv_proof_compiler_compile` 生成通用格式；Coq/Lean 经 `interop_export_coq/interop_export_lean` 或 `interop_theorem_export_calls` 导出，OPML 走 `lv_EXT_JSON` 通道经 `lvInteropPlugin.validate` 校验；XML/SVG 类输出复用 `lv_export_xml_escape` 与 `lv_export_write_file`。
5. **版本校验与提交**：`axiom_package_compute_content_hash` + `module_validate_dependency_chain` 校验依赖；公理包升级后 `axiom_package_auto_degrade_invalidated` 自动降级失效节点；导出产物经 `proof_repo_commit` 提交入证明仓库，`proof_repo_diff` 对比版本差异。

---

## 模块关系

| 编号文档 | 关系说明 |
| --- | --- |
| [09_proof.md](09_proof.md) | 提供 `Proposition`、`ProofStep`、`ProofColor` 等证明基础设施，`lvProofObject` 构建于其上 |
| [22_proof_export_trace_widget.md](22_proof_export_trace_widget.md) | 定义导出/追踪/交互组件层；本文档的 `proof_compiler` 为其中导出子系统的编译层 |
| [05_rewrite.md](05_rewrite.md) | 图重写引擎基础文档，本文档聚焦"公理包→安全重写规则"的转化与度量保障 |
| [21_euclidean_geometry.md](21_euclidean_geometry.md) | 欧氏几何公理包即本文档链路的典型上游（公理包来源） |
| [31_stream_interop.md](31_stream_interop.md) | 互操作系统总文档，Coq/Lean/OPML 导出属于其定理交换子域 |
| [34_meta_proof_cache.md](34_meta_proof_cache.md) | 元证明与推理缓存，与 `axiom_template_expand_lazy` 展开缓存互补 |
| [14_solver_backends.md](14_solver_backends.md) | 多后端求解体系；`rewrite_search_backward` 可作为求解后端之一 |
| [OPML_SPECIFICATION.md](OPML_SPECIFICATION.md) | OPML（Open Proof Markup Language）规范，本文档 OPML 导出的目标格式 |

---

## 版本历史

| 版本 | 日期 | 变更 |
| --- | --- | --- |
| 1.0 | 2026-08-10 | 初稿：公理包 → 安全重写 → 证明编译导出（Coq/Lean/OPML）→ 版本校验全链路 |
