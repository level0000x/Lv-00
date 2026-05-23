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
