# Lv-00 代码完善路线图

## TASK_REPORT 2026-05-20：全面优化会话

### 文档修复
- 更新 Lv-00系统描述文档.md：完善文件结构（新增15个模块头文件引用）、修正API示例（graph_add_point使用SymbolicCoord**）、更新测试命令路径、修复许可证为MIT、添加v3.0.1版本历史
- 更新 IMPLEMENTATION_ROADMAP.md：本次详细TASK_REPORT记录

### 示例文件修复
- **triangle_construction.c**：替换sqrt(3)的有理数近似值为exact quadratic坐标；为add_point添加输入验证和错误检查；为graph_add_line_segment返回值添加检查；为确定性检查中的1000参数添加注释说明
- **circle_intersection.c**：修复圆的几何语义（添加CONTAINMENT约束在圆心与半径点之间）；修正相交约束从自相交改为线段与圆心双参与者格式；用quadratic坐标替换sqrt(5)近似值；为所有graph_add_*调用添加错误检查；全文添加中文几何语义注释
- **high_dim_demo.c**：用M_PI常量替换硬编码圆周率；用lv00_strlcpy替换strncpy；为high_dim_create_multi_projection_view添加错误检查；添加投影视图销毁调用
- **function_composition.c**：为func_block_pack后的fb添加NULL检查；为type_create_*系列返回值添加错误检查；为所有辅助函数和main添加中文注释；添加strdup内存管理说明

### 代码质量改进
- 统一错误码定义（error_codes.h/c）
- 词法分析器代码去重（lexer_shared.c/h）
- strncpy → lv00_strlcpy 全量替换
- memcpy → memmove 选择性替换（高维模块重叠区域）
- 构建系统：添加install()规则、target_include_directories、完善.gitignore

## 整理状态（2026-05-20 自动化代码审查与修复）

### 已完成的修复
- 桩函数修复：interop.c（9个函数）、high_dim.c（8个函数）返回 LV00_ERROR_UNSUPPORTED
- 内存安全修复：proof.c setter函数状态一致性、symbolic_coord.c 浮点比较、high_dim.c memcpy→memmove
- 代码重复消除：词法分析器提取为 lexer_shared.c/h、func_block.c 确定性检查合并
- 构建系统：添加 install() 规则、target_include_directories、完善 .gitignore
- 代码风格：LV00_THREAD_LOCAL 统一定义、strncpy→lv00_strlcpy
- GeoJSON导出：从硬编码占位数据改为基于实际图数据

## 当前状态评估

### 已完成的模块 ✅

| 模块 | 完成度 | 测试状态 |
|------|--------|----------|
| 符号坐标系统 - 有理数 | 100% | ✅ 通过 |
| 符号坐标系统 - 代数数基础 | 85% | ✅ 通过 |
| 符号坐标系统 - 二次根式 | 95% | ✅ 通过 |
| 符号坐标系统 - 超越常数 | 100% | ✅ 通过 |
| 约束图核心 - 基础操作 | 100% | ✅ 通过 |
| 约束图核心 - 节点管理 | 100% | ✅ 通过 |
| 图规范化遍引擎 - 点合并 | 100% | ✅ 通过 |
| 图规范化遍引擎 - 并查集 | 100% | ✅ 通过 |
| 模块系统 | 100% | ✅ 通过 |
| 公理包系统 | 100% | ✅ 通过 |
| 合一检查 - 基础 | 85% | ✅ 通过 |
| 引擎核心 | 100% | ✅ 通过 |
| 词法分析器共享 | 100% | ✅ 通过 |
| 错误码系统 | 100% | ✅ 通过 |
| 工具库 (lv00_utils) | 100% | ✅ 通过 |
| 多项式运算 (mpz_poly) | 100% | ✅ 通过 |
| 公式解析器 | 90% | ✅ 通过 |
| 公式渲染器 | 85% | ✅ 通过 |
| 公式转换器 | 85% | ✅ 通过 |
| 交互模块 (interop) | 80% | ✅ 桩函数已修复 |
| 高维模块 (high_dim) | 80% | ✅ 桩函数已修复 |
| 流处理 (stream) | 75% | ✅ 基本实现 |

### 需要完善的模块 🚧

| 模块 | 当前完成度 | 需要完善的内容 |
|------|------------|----------------|
| 符号代数求解器 | 65% | Gröbner基实现、多解处理 |
| 图重写引擎 | 75% | VF2匹配算法完善、循环检测 |
| 函数块系统 | 85% | 确定性检查深度验证、β-归约优化 |
| 函数块选择器 | 80% | 条件分支覆盖、多解选择优化 |
| 函数块组合器 | 75% | 乘积兼容性检查 |
| 类型系统 | 80% | 类型等价检查、类型推断完善 |
| 命题与证明系统 | 80% | 证明导航器、不可构造性证明 |
| 命题验证器 | 75% | 信任颜色判定自动化 |
| 递归与条件 | 65% | 测度系统完善、选择器块 |
| 高维模块 (high_dim) | 80% | 投影保真度优化、多视图管理 |
| 交互模块 (interop) | 80% | 更多导出格式支持（SVG, TikZ） |
| 流处理 (stream) | 75% | 惰性求值优化 |

## 完善优先级

### 高优先级（核心功能）

1. **符号代数求解器完善**
   - 实现完整的Gröbner基算法
   - 添加多解分支处理
   - 完善"超出范围"分析

2. **图重写引擎完善**
   - 完善VF2子图同构算法
   - 实现WL图核哈希
   - 添加循环检测机制

3. **合一检查完善**
   - 实现完整的三层匹配
   - 添加详细的失败报告
   - 优化性能

### 中优先级（高级功能）

4. **函数块系统完善**
   - 完善确定性状态机
   - 优化β-归约实现
   - 添加组合子实现

5. **类型系统完善**
   - 实现类型等价检查
   - 添加类型推断
   - 完善宇宙层级检查

### 低优先级（增强功能）

6. **命题与证明系统完善**
   - 实现证明导航器
   - 添加不可构造性证明支持
   - 完善信任颜色系统

7. **递归与条件完善**
   - 完善测度系统
   - 实现选择器块
   - 添加Y组合子

## 具体实现任务

### 阶段一：核心求解器完善（建议首先完成）

#### 1.1 符号代数求解器

```c
// 需要添加的函数
int solver_solve_groebner_basis(EquationSystem *system, SolutionSet *solutions);
int solver_handle_multiple_solutions(EquationSystem *system, SolutionBranches *branches);
int solver_analyze_out_of_scope(EquationSystem *system, OutOfScopeAnalysis *analysis);
```

**实现要点**：
- 使用Buchberger算法计算Gröbner基
- 实现多项式约简
- 添加S-多项式计算
- 实现多解分支生成

#### 1.2 图重写引擎

```c
// 需要完善的函数
int rewrite_vf2_match(RewriteRule *rule, ConstraintGraph *graph, MatchResult *matches);
uint64_t rewrite_compute_wl_hash(ConstraintGraph *graph);
bool rewrite_detect_cycle(RewriteEngine *engine, uint64_t current_hash);
```

**实现要点**：
- 完善VF2算法的候选选择
- 实现WL图核的迭代标签聚合
- 添加循环检测的环形缓冲区

#### 1.3 合一检查

```c
// 需要完善的函数
int unify_match_ports(ConstraintGraph *construction, Proposition *prop, PortBindings *bindings);
int unify_match_constraints(ConstraintGraph *construction, ConstraintGraph *pattern, ConstraintBindings *bindings);
int unify_match_coords(SymbolicCoord *c1, SymbolicCoord *c2, CoordBindings *bindings);
UnifyFailureReport* unify_create_failure_report(UnifyResult result, ...);
```

**实现要点**：
- 实现端口类型匹配
- 完善约束类型匹配
- 添加符号坐标判等
- 生成详细的失败报告

### 阶段二：高级功能完善

#### 2.1 函数块系统

```c
// 需要完善的函数
int func_block_determinism_check_static(FuncBlock *block);
int func_block_determinism_check_dynamic(FuncBlock *block, ConstraintGraph *instantiated);
int func_block_beta_reduction(ConstraintGraph *graph, FuncBlock *block, IDMapping *mapping);
```

#### 2.2 类型系统

```c
// 需要完善的函数
int type_system_check_equivalence(TypeRegion *t1, TypeRegion *t2, EquivalenceResult *result);
int type_system_infer_type(ConstraintGraph *graph, int node_id, TypeRegion **inferred);
```

#### 2.3 证明系统

```c
// 需要完善的函数
int proof_navigator_create(Proof *proof, ProofNavigator **navigator);
int proof_navigator_next_step(ProofNavigator *navigator);
int proof_navigator_prev_step(ProofNavigator *navigator);
```

### 阶段三：增强功能

#### 3.1 递归与条件

```c
// 需要完善的函数
int measure_system_check_decrease(Measure *measure, SymbolicCoord *before, SymbolicCoord *after);
int selector_block_evaluate(SelectorBlock *selector, ConstraintGraph *graph, SelectorResult *result);
```

## 实施建议

### 建议的实施顺序

1. **首先完成阶段一**（核心求解器）
   - 这些是系统的核心功能
   - 影响其他模块的正确性
   - 测试覆盖率需要提高

2. **然后完成阶段二**（高级功能）
   - 这些功能依赖阶段一
   - 提供更完整的使用体验

3. **最后完成阶段三**（增强功能）
   - 这些是锦上添花的功能
   - 可以在系统稳定后添加

### 测试策略

每个模块完善后，应该：
1. 添加单元测试
2. 添加集成测试
3. 更新文档
4. 运行所有现有测试确保不破坏

### 文档更新

每次代码完善后，同步更新：
1. 模块文档（docs/目录）
2. API文档
3. 使用示例
4. 测试用例

## 当前建议

基于现有代码状态，我建议：

1. **保持现有代码稳定** - 所有测试都通过，这是一个很好的基础
2. **优先完善求解器** - 这是几何系统的核心
3. **逐步添加功能** - 不要一次性改动太多
4. **持续测试** - 每次修改后都运行所有测试

您希望我首先开始完善哪个模块？我建议从**符号代数求解器**开始，因为它是系统的核心计算引擎。

---

## TASK_REPORT 2026-05-23：竞品分析驱动的项目更新

基于 `docs/competitive_analysis.md` 的竞品分析结果，对项目进行了系统性更新。

### P0: 公理分层与证明呈现（借鉴 LeanGeo + GeoCoq）✅

| 更新项 | 详情 |
|--------|------|
| `axiom_packages/` 目录 | 创建公理包组织结构，含 41 个已注册包的 `INDEX.json` |
| `axiom_packages/euclidean/` | 欧氏几何 5 层分层公理文档（关联→顺序→全等→连续→平行），每条公理含命名空间名/形式陈述/直觉解释 |
| `axiom_packages/hyperbolic/` | 双曲几何包（继承机制 + 定理差异对比表） |
| `axiom_packages/package_template.json` | 新包创建模板 |
| 证明策略注释 | proof.h/c 新增 `proof_navigator_set_strategy_note()` / `proof_navigator_get_strategy_note()` — 借鉴 LeanGeo"先展示总体策略，再展开细节" |
| HTML 导出增强 | 策略概述 box 出现在汇总栏之后、时间线之前 |
| `proof_navigator_create/destroy` | 添加 `strategy_note` 字段的初始化和释放 |

### P1: 自然语言证明输出（借鉴 AlphaGeometry）✅

| 更新项 | 详情 |
|--------|------|
| `proof_step_get_natural_language()` | 将单个步骤转为完整自然语言描述（中/英），含动词映射、对象描述、推理依据、信任状态 |
| `proof_export_natural_language()` | 导出完整证明为自然语言文本，策略→步骤→总结 三段式 |
| `proof_step_set_note()` | 为用户提供步骤注释 API |
| HTML 导出增强 | 每个步骤面板新增"📝 自然语言描述"区域，显示推理依据和依赖链 |
| CSS 样式 | 新增 `.strategy-box` / `.nl-description` / `.nl-why` 样式 |

### P2: API 文档组织重构（借鉴 CGAL）✅

| 更新项 | 详情 |
|--------|------|
| `docs/API_USAGE_GUIDE.md` | 从 515 行重构为 1060 行，按 10 个几何概念分章节 |
| CGAL 五段式 | 每个概念含：📖概念定义 → 🏗️模型实现 → 📝API参考 → ⚡复杂度标注 → 💡使用示例 |
| 复杂度标注 | 涵盖 10 个模块的时间/空间复杂度（O(1), O(V+E), O(d³), O(N!·N) 等） |

### P3: 几何对象命名体系（借鉴 GeoGebra）✅

| 更新项 | 详情 |
|--------|------|
| `docs/NAMING_CONVENTION.md` | 819 行命名规范文档，覆盖 10 个章节 |
| 命名规则 | 点（大写字母/前缀）、线（双轨制）、区域（△/□/⊙）、约束（三轨对比表）、函数块（PascalCase）、端口（snake_case）、公理（`namespace.layer.number`） |
| 竞品对比表 | GeoGebra vs LeanGeo vs Lv-00 八维度对比 |

### P4: 包管理机制（借鉴 GAP）✅

| 更新项 | 详情 |
|--------|------|
| `axiom_packages/INDEX.json` | 全包注册表，含版本/分类/分层/依赖/描述 |
| `axiom_packages/README.md` | 包管理系统文档（SemVer、SHA-256 完整性、C API 示例） |
| `axiom_packages/CHANGELOG.md` | 包变更日志 |
| 依赖管理 | 继承机制（hyperbolic 继承 euclidean 0-3 层）、版本化管理、哈希校验 |

### 代码变更汇总

| 文件 | 变更 |
|------|------|
| `include/lv00/proof.h` | +76 行：新增 `ProofNaturalLanguage` 枚举、`proof_step_get_natural_language()`、`proof_export_natural_language()`、`proof_navigator_set_strategy_note()`、`proof_navigator_get_strategy_note()`、`proof_step_set_note()`；`ProofNavigator` 结构体新增 `strategy_note` 字段 |
| `src/proof.c` | +370 行：实现上述 6 个新函数；HTML 导出新增策略概述和自然语言描述区域；`proof_navigator_create/destroy` 添加 strategy_note 生命周期管理 |
| `docs/API_USAGE_GUIDE.md` | 完全重写（515→1060 行），CGAL 风格 |
| `docs/NAMING_CONVENTION.md` | 新建 819 行命名规范文档 |
| `axiom_packages/` | 新建 7 个文件，完整的包管理体系 |

### 待验证事项

- [ ] 编译 proof.c 确认无语法错误
- [ ] 运行 `test_proof` 确认现有测试仍通过
- [ ] Web GUI `ProofPanel.tsx` 集成自然语言显示
