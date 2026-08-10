# 21. 欧氏几何公理包

## 模块概述

欧氏几何公理包是 Lv-00 中承载欧氏几何形式化基础的核心模块，将 Hilbert 公理体系落地为可加载、可验证、可分级、可着色信任的公理包（AxiomPackage）。模块连接三层能力：底层以 `formal/lv/HilbertAxioms.lean` 中的 Lean 形式化公理为语义基准；中间以 `axiom_pkg.h` 定义公理包的存储格式、约束模板、不可构造性记录与依赖引用追踪；上层以 `module.h` 与 `lv_loader.h` 负责 LVZ/LV 文件的加载、依赖链验证与证明断言验证，并以 `axiom_grade.h`、`trust_color.h` 提供教育分级与信任颜色映射。

**覆盖头文件**：
- `axiom_pkg.h` —— 公理包格式、约束模板注册与验证、不可构造性记录、依赖引用追踪
- `axiom_grade.h` —— 公理难度分级（基础/中级/高级/专家）与证明风格筛选
- `trust_color.h` —— TrustColor ↔ ProofColor 双向映射与 HTML 颜色呈现
- `module.h` —— 模块创建/销毁、依赖管理、LVZ 加载/保存与内容哈希
- `lv_loader.h` —— `.lv` 文件解析、解析结果应用与 Prove 断言验证

**形式化基准**：`formal/lv/HilbertAxioms.lean` 定义 `HilbertPlane` 类，整合五组公理：关联（Incidence）、序（Order/Betweenness）、合同（HCongruence）、平行（Parallel）、连续性（Continuity）。

## 核心设计原则

1. **公理即模板**：每条公理注册为 `ConstraintTemplate`，通过 `expand` 回调展开为约束图，几何构造与公理应用统一走模板展开路径。
2. **Hilbert 五组公理为不可再分基底**：与 `HilbertAxioms.lean` 一一对应，公理包中的模板要么直接对应某条公理，要么可由其推导（`prerequisite_count` 记录前置公理数量）。
3. **教育分级驱动可用集**：`lvAxiomGradeFilter` 按难度动态收窄可见公理集合，`is_required` 的必修公理不受过滤影响。
4. **信任颜色随依赖链传播**：每条依赖引用记录 `original_color`，内引用失效时由 `axiom_package_auto_degrade_invalidated` 将 GREEN 降级为 AMBER/YELLOW。
5. **验证而非信任**：内引用以 SHA-256 内容哈希重验，外引用视为公认永久有效，作者断言立即标记为黄色基础。

## 关键数据结构

公理包聚合全部模板、不可构造性记录、展开缓存与依赖引用：

```c
struct AxiomPackage {
    char *name;
    char *version;
    lvDArray templates;                  /* lvDArray<ConstraintTemplate> */
    lvDArray known_unconstructibles;     /* lvDArray<KnownUnconstructible> */
    lvDArray unconstructible_templates;  /* lvDArray<UnconstructibleTemplate> */
    char *bottom_geometry;               /* 底层几何类型 */
    char *negation_encoding;             /* 否定编码方法 */
    int contradiction_behavior;          /* 矛盾行为 */
    lvDArray expansion_cache;            /* lvDArray<TemplateExpansionCache> */
    int max_expansion_depth;             /* 默认 8 */
    lvDArray dep_refs;                   /* lvDArray<DependencyRef> */
};
```

约束模板携带参数描述、正则形式与分级状态：

```c
typedef struct ConstraintTemplate {
    char *name;
    int param_count;
    void (*expand)(SymbolicCoord **params, ConstraintGraph *target);
    bool verified;
    TemplateParam *params;      /* 参数描述数组 */
    int param_desc_count;
    NormalFormDesc normal_form; /* 正则形式描述 */
    TemplateLevel level;        /* 一级/二级模板 */
    bool is_compressed;
    ConstraintGraph *compressed_subgraph;
} ConstraintTemplate;
```

**公理包格式**：公理包随模块以 LVZ 文本格式持久化（默认 `MODULE_FORMAT_LVZ`），亦支持 MessagePack 二进制（`module_load_from_binary` / `module_save_to_binary`）与 JSON（`module_serialize_to_json` / `module_deserialize_from_json`）三种载体；`module_compute_content_hash` 对名称、版本、依赖、导出与公理包元数据计算 SHA-256，供增量快照 `module_compute_delta` 与崩溃恢复 `module_recover_from_backup` 使用。

分级元数据与依赖引用：

```c
typedef struct {
    char axiom_name[128];
    lvAxiomGrade grade;     /* GRADE_BASIC..GRADE_EXPERT */
    lvProofStyle style;     /* STYLE_FORWARD/BACKWARD/CONTRADICTION/INDUCTION */
    int prerequisite_count;
    char *description;
    bool is_required;       /* 必修公理不受难度过滤影响 */
} lvAxiomGradeMeta;

typedef struct {
    char ref_id[64];
    char content_hash[65];      /* SHA-256 十六进制 */
    int dependent_node_id;
    int original_color;         /* DEP_TRUST_GREEN..DEP_TRUST_RED */
    RefType ref_type;           /* REF_INTERNAL/REF_EXTERNAL/REF_AUTHOR */
    char external_ref[256];
    char trust_comment[256];
    bool hash_valid;
} DependencyRef;
```

## 主要接口

| 头文件 | 接口 | 说明 |
|--------|------|------|
| axiom_pkg.h | `lv_axiom_package_create(name, version)` / `axiom_package_destroy` | 公理包创建/销毁 |
| axiom_pkg.h | `axiom_package_load` / `axiom_package_save` | 公理包文件加载/保存，状态为 `AxiomLoadStatus` / `AxiomSaveStatus` |
| axiom_pkg.h | `axiom_package_register_template` / `axiom_package_get_template` | 约束模板注册与按名查找 |
| axiom_pkg.h | `axiom_template_expand_lazy` / `axiom_template_compress` | 惰性展开与压缩态恢复 |
| axiom_pkg.h | `axiom_template_run_tests` / `axiom_template_test_run` | 双层测试集（出厂/用户）运行 |
| axiom_pkg.h | `axiom_package_register_dependency_ref` / `axiom_package_add_internal_ref` / `axiom_package_add_external_ref` / `axiom_package_add_author_assertion` | 依赖引用注册 |
| axiom_pkg.h | `axiom_package_validate_dependencies_with_hashes` / `axiom_package_auto_degrade_invalidated` | 哈希重验与失效降级 |
| axiom_pkg.h | `axiom_package_reverify_lemmas` / `axiom_package_mark_lemma_stale` | 引理自动重验循环 |
| axiom_pkg.h | `axiom_package_verify_unconstructible` / `axiom_package_add_unconstructible_template` | 不可构造性验证与模板 |
| axiom_grade.h | `lv_axiom_set_difficulty` / `lv_axiom_get_filter` / `lv_axiom_grade_check` | 难度过滤控制 |
| axiom_grade.h | `lv_axiom_unlock_next_grade` / `lv_axiom_filter_by_style` | 递进解锁与风格筛选 |
| trust_color.h | `trust_color_to_proof` / `proof_color_to_trust` | 双向往返映射 |
| trust_color.h | `proof_color_combine` / `proof_color_to_html_hex` | 颜色叠加与 UI 呈现 |
| module.h | `module_create` / `module_add_dependency` / `module_add_axiom_package` | 模块装配 |
| module.h | `module_load` / `module_save` | LVZ 加载/保存（含 MessagePack 与 JSON 变体） |
| module.h | `module_validate_dependency_chain` / `module_full_cycle_detect` | 依赖链验证与循环检测 |
| module.h | `module_compute_content_hash` | 模块内容 SHA-256 哈希 |
| lv_loader.h | `lv_load_file` / `lv_apply_parse_result` | `.lv` 解析与结果应用 |
| lv_loader.h | `lv_verify_proofs` / `lv_load_file_verified` | Prove 断言验证（PASS/FAIL/SKIP） |

## 工作流程

1. **公理包装配**：`module_create("euclidean", "1.0.0")` 后依次 `module_add_dependency` 声明对 `base` 等模块的版本约束（如 `>=1.0.0`），并以 `module_add_axiom_package` 挂接公理包。
2. **LVZ 加载**：`module_load` 按依赖拓扑加载，遇到循环依赖返回 `MODULE_LOAD_CIRCULAR_DEPENDENCY`，深度超过 `MAX_MODULE_DEPTH`（32）返回 `MODULE_LOAD_DEPTH_EXCEEDED`。
3. **模板验证**：出厂测试（`TEST_CASE_FACTORY`）与用户测试（`TEST_CASE_USER`）经 `axiom_template_run_tests` 比对；`axiom_template_validate_normal_form` 校验展开图的节点/约束类型序列。
4. **依赖重验**：`axiom_package_validate_dependencies_with_hashes` 重算内引用哈希；失效引用由 `axiom_package_auto_degrade_invalidated` 在约束图中将节点信任颜色由 GREEN 降级，`axiom_package_reverify_lemmas` 标记遗留引理。
5. **分级应用**：教学场景下 `lv_axiom_set_difficulty` 设置 `max_grade`，`lv_axiom_grade_check` 逐公理判定；`lv_axiom_unlock_next_grade` 递进解锁。
6. **构造着色**：模板展开后的构造节点携带 TrustColor；证明步骤经 `trust_color_to_proof` 映射到 ProofColor，再经 `proof_color_to_html_hex` 供 UI/导出呈现。

## 模块关系

| 文档 | 关联内容 |
|------|----------|
| [15_geometry_advanced.md](15_geometry_advanced.md) | 几何实体类型、变换推理与信任着色，公理包模板展开的目标图承载 |
| [26_interactive_geometry.md](26_interactive_geometry.md) | 交互构造触发模板展开与不可构造性识别 |
| [29_inequality_approximation.md](29_inequality_approximation.md) | 近似模型计数与构造性判定，支撑不可构造性问题验证 |
| [33_gappa_verification.md](33_gappa_verification.md) | 浮点证明 DSL 与谓词传播，为数值步骤提供信任颜色 |
| [19_numerical_backends.md](19_numerical_backends.md) | 数值后端，公理包求值链路的浮点执行基础 |
| [01_symbolic_coord.md](01_symbolic_coord.md) | TrustColor 枚举权威定义与符号坐标 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图结构，模板展开与构造着色的数据载体 |
| [07_func_block.md](07_func_block.md) | 函数块导出（`module_export_function_block`），公理证明的复用单元 |
| [37_parsing_layer.md](37_parsing_layer.md) | 解析层，`lv_load_file` 的 lex/parse/sema 链路 |
| `formal/lv/HilbertAxioms.lean` | Hilbert 平面五组公理的形式化基准（关联/序/合同/平行/连续） |

## 版本历史

- **v5.0.0**
  - 补全公理包文档：格式、Hilbert 体系映射、分级与信任颜色、加载与验证。
  - 明确 `AxiomPackage`、`ConstraintTemplate`、`DependencyRef` 与模块加载链路的关系。

- **v3.6.0**
  - 引入模板分级管理（一级/二级）与惰性展开缓存。

- **v3.5.0**
  - 增加依赖引用追踪（内/外/作者断言）与引理自动重验循环。
  - 公共符号统一 `lv_` 前缀，`axiom_package_create` 保留为兼容别名。

- **v3.4.0**
  - 引入公理难度分级与证明风格筛选。
  - 引入 TrustColor ↔ ProofColor 双向映射。
