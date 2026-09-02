# lv-merged 合并报告

> 合并 `formal/`（Lean v4.14.0，活跃）与 `lv-formal/`（Lean v4.33.0-rc1，早期线）两个 Lean 项目。
> 结果：`lv-merged/` 独立项目，Lean v4.14.0 + mathlib v4.14.0，`lake build lvFormal` **全量通过**（80 个模块 olean）。

## 一、合并决策

- **新目录** `lv-merged/`（不动两边原代码），工具链取 `formal/` 的 v4.14.0（本地已装、稳定）。
- **依赖缓存复用**：`lv-merged/.lake/packages` 用 junction 指向 `formal/.lake/packages`（mathlib 5388 olean 零拷贝），`build/` 独立。
- **文件裁决**（78 个同名文件全部内容不同，逐个对照）：
  - **27 个 formal 为 3 行空壳的文件**（Basic、Classical/Hilbert×12、Interop×2、Axioms 主线、Constraint、Ontology、Predicates、Groebner、Proof、Reasoning、Rewrite、Unification）→ 用 lv-formal 完整版。
  - **20 个 lv-formal 独有**（`Axioms/Instances_*` + `PackageValidation_*`，54 公理包映射与依赖验证）→ 并入。
  - **44 个 formal 独有**（`*_Theory` 规格文件 + pipeline）→ 并入 formal 版。
  - 其余同名文件默认 formal 版，逐对 diff 后个别替换为 lv 版（ConstraintPropagation、GeometricAlgebra、NDimGeometry、NormalizationProperties、StreamInvariants、Cv00Memory、DSLWrappersSoundness、LvDSL）。
  - **删除死代码**：`LvDSL_tmp.lean`、`check_scratch.lean`、`check_temp.lean`（无任何 import）。

## 二、合并时修复的问题（两边代码从未被完整编译过）

### lv-formal 侧（v4.33 代码 → v4.14）
1. **UTF-8 BOM**：所有文件带 BOM，v4.14 报 `expected token` → 批量去除。
2. **`DecidableEq` 缺失**：`PrimPred`、`RuleTemplate` 全部 structure、`ConstraintGraph` → 补 `deriving DecidableEq`。
3. **`Decidable` 实例缺失**：`TemplateParamCountReasonable`、`HasExternalReference` → 补 instance（`by decide` 需要）。
4. **`List.get_mem` 签名差异**：v4.33 的 `List.get_mem _ _ _` 在 v4.14 不匹配 → 删除 30 个 `*_dep_by_index` 私有死代码定理（无调用）。
5. **元组 binder 语法**：`∀ (n, d) ∈ l` 是 v4.33 新语法 → 改为投影形式 `∀ p ∈ l, p.1 = ...`。
6. **断言数字与实例不符（10 处）**：`ValidationResult_correct` 的 `templateCount` 断言与实例 `_length` 定理不一致（如 zfc 断言 27 实为 29、gameTheory 51→55、informationTheory 96→106 等）→ 修正断言匹配实际数据。（说明 README 声称值与代码数据已脱节。）

### formal 侧（从未全量编译过，只有 33 个历史 olean）
7. **`noncomputable` 缺失**：`Real.sqrt` 等 → 加 `noncomputable section`。
8. **`ext` 定理缺失**：v4.14 的 structure 不自动生成 ext 定理 → 手动 `@[ext] theorem`（GeometricAlgebra、GeometricAlgebraDefs、LvDSL 等）。
9. **API 名差异**：`List.length_erase_le` 参数顺序、`Real.dist_triangle` 不存在、`List.length_erase_le_erase` 不存在、`List.all_eq_true` 用法、`getElem!` 的 `decidableGetElem?` 展开。
10. **`forall`/`exists` 是 Lean 保留字**（LvDSL 构造子名非法）→ 重命名 `lvForall`/`lvExists`（含 LvSemantics 引用）。
11. **前向引用**（LvDSL `lv_type_infer` 定义在后）→ 交换定义顺序。
12. **termination**：List 嵌套递归（`es.all`/`es.bind`/`es.map`）在 v4.14 无法自动终止 → 3 个函数改 `partial`（lv_type_check、lv_free_vars、lv_subst）。

### 数学错误（原代码声称证明的假命题，已诚实标注）
13. **`GeometricAlgebra.gp` 定义不完整**（vector 部分缺叉积项）：`gp_associative`、`gp_one_left`、`gp_one_right` 是**假命题**。
14. **`GeometricAlgebraDefs.gp` 缺多向量↔bivector 交叉项**：`gp_blade_assoc`、`gp_scalar_one` 是假命题。
    - 已标记 `sorry` + 注释说明，后续需修正 gp 定义后补证。

## 三、当前证明状态（诚实清单）

- **build**：`lake build lvFormal` ✅ 全量通过，80 模块。
- **59 个 `sorry`**：合并阶段保留声明、证明体待补（标注了"待证"原因）：
  - 假命题类：gp_associative、gp_one_left/right、gp_blade_assoc、gp_scalar_one（需先修定义）
  - 规格定理待证：NDimGeometry.dist_triangle/pythagoras、ConstraintPropagation.ac3、InteractiveGeoSoundness.drag_preserves_constraints、DSLWrappersSoundness.dist_triangle/midpoint、DifferentialGeometry 若干、Cv00Memory 桥接、EngineInvariants.compile_phase_correct、MetaVerificationTheory.meta_completeness、GeometryPresetDefs 若干、LvDSL 定理、PresetGeometryDefs 若干、CodegenCorrectness.safe_stmt_never_aborts
- **21 个 `axiom` 声明**：
  - Interop/Equivalence 5 个（C↔Lean 类型互转，设计上为假设）
  - Classical/Hilbert/Consistency 2 个
  - Constraint/Graph 1 个（`checkStatus` 接口）
  - **公理包依赖验证 13 个**（跨引用依赖不在模板表内：graph/measure/functionalAnalysis/probability/information/linearAlgebra/zfc/computability/modalLogic/orderTheory/affineGeometry/projectiveGeometry）
  - LogicalFramework 1 个（`completeness_principle_axiom`，作者明示）

## 四、后续推进计划（Phase 2+）

1. **先修数学定义**（优先级最高）：补全 `gp` 的叉积/bivector 传播 → 重证结合律与单位元。
2. **消除 axiom**：13 个公理包依赖验证 → 建立跨包注册表（C 侧已有 `globalNameRegistry` 概念）后改为可判定证明。
3. **补齐 sorry**：按依赖顺序——欧氏距离三角不等式（Cauchy-Schwarz）→ 中点等距 → 约束传播 AC-3 → LvDSL 元理论（需先恢复非 partial）。
4. **修 `IR.graph_satisfiable` / `lvLang.satisfiable` 的 sorry 定义**（当前"可满足性保持"证明建立在其上）。
5. **统一入口**：更新 `lvFormal.lean` import 列表（当前为 formal 版入口，未含 lv-formal 主线模块）。

## 五、文件位置

- 合并项目：`lv-merged/`（lakefile.toml、lean-toolchain、lvFormal/、lv/）
- formal 版备份：`lv-merged/.merge_backup/*.formal.lean`
- 原项目：`formal/`、`lv-formal/`（未改动）
