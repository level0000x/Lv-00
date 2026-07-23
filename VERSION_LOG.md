# Lv-00 版本迭代日志

本文件用于记录 Lv-00 项目的正式版本演进、关键整改、兼容性变化与验收状态。

## 记录规范

- 版本号遵循语义化版本：`MAJOR.MINOR.PATCH`。
- 每次有效迭代必须记录：日期、类型、范围、摘要、验证方式。
- Commit 信息统一采用 Conventional Commits：`fix` / `feat` / `perf` / `docs` / `refactor` / `test` / `build` / `ci` / `chore`。
- 禁止无实际文件变更的空提交；禁止使用无语义的自动提交替代版本记录。
- 主干分支保持稳定，功能开发和实验性修改应先在开发分支验证后再合入。

## 2026-05-25

### chore(repo): 建立仓库整改追踪基线

- 建立 `VERSION_LOG.md`，开始常态化记录版本迭代。
- 明确提交类型规范，避免继续产生无意义自动提交。
- 建立标准目录占位：`core/`、`module/`、`tool/`、`web/`、`doc/`、`test/`、`log/`。
- 保留现有源码逻辑，不在本次整改中执行破坏性历史改写或大规模文件迁移。

### refactor(repo): 全仓目录标准化整理

- 将核心 C 源码与公开头文件迁移到 `core/src/` 与 `core/include/`。
- 将 C 测试、模糊测试与示例迁移到 `test/c/`、`test/fuzz/`、`test/examples/`。
- 将 Python、监控、实验分支、公理包等附属模块迁移到 `module/`。
- 将工具脚本与报告生成器迁移到 `tool/`。
- 将新版 Web GUI、旧版 Web 前端与流监控服务统一收敛到 `web/`。
- 将文档、报告与历史任务记录分别整理到 `doc/` 与 `log/`。
- 同步修复 CMake、GitHub Actions、Python 打包配置、`.gitignore` 中的路径引用。
- 清理明显临时/构建缓存文件，包括备份文档、临时报告脚本与 TypeScript 构建缓存。

### 验证记录

- `git diff --cached --check`：通过，无空白错误。
- `cmake -S . -B build_verify -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF`：目录与语法进入配置阶段；本机 MSVC 环境因缺少 GMP 依赖中止，非本次目录整理引入的路径错误。

### 后续待办

- 安装或配置 GMP 后运行完整 CMake build/test。
- 完善 PR 分支保护、CI 检查、安全扫描与覆盖率检测。
- 对核心算法、解析器、拓扑模块补齐关键注释与边界测试。

---

## v3.4.0 -- 2026-05-26 仓库全面整改

### 提交记录整改
- 清理所有备份分支和远程备份引用
- 重命名主分支 master → main
- 建立三级分支体系：main（稳定）/ dev（开发）/ exp（实验）

### 仓库文件清理
- 归档 10+ 过时设计/规划文档至 doc/reports/_archive/legacy_docs/
- 归档 7+ 旧任务报告至 doc/reports/_archive/legacy_task_reports/
- 归档 15+ 散放报告文件至 doc/reports/_archive/
- 删除重复编码规范（保留 CODING_STANDARD_v3.4.2.md）
- 合并魔法系统文档（magic_system.md → MAGIC_MODULE.md 附录）
- 清理 23 个测试保存文件（*_test_save.lvz）
- 清理 10 个本地构建目录
- 竞品设计笔记移至 doc/docs/reference/

### 源码规整优化
- 补齐核心模块 Doxygen 注释
- 消除 coords_equal 重复实现
- 统一代码风格

### 源码安全加固
- 完善错误处理和参数校验

### 工程化标准化
- CI 流水线适配 main 分支
- 新增 PR 模板
- 版本号统一为 3.4.0

---

## v3.5.0 -- 2026-05-26 五层架构迁移与仓库深度清理

### 架构迁移
- 完成五层架构源码迁移：layer1_parser → layer2_resource → layer3_geometry → layer4_reasoning → layer5_output
- 共 355 个源文件（178 .c + 177 .h），总计约 9.87 MB
- 删除旧扁平目录结构（core/src/core/ 等 8 个子目录，约 180 个文件）
- CMakeLists.txt 完全适配五层构建系统

### 新增核心模块
- equiv_class：等价类管理器（并查集，5 种等价来源）
- meta_proof：元证明系统（WFC 剪枝合法性，L1/L2/L3 三层策略）
- propagation：约束传播引擎（AC-3 弧相容性，熵最小化）
- expr_canon：代数表达式规范形式

### 仓库深度清理
- 删除 23 个散落 *_test_save.lvz 测试产物
- 删除 3 个根目录 test_results.* 文件
- 删除 core/src/_deprecated/ 和 module/concurrent_monitor/_deprecated/ 废弃代码
- 删除 doc/reports/current/ 中 31 个重复/过时报告
- 删除 doc/reports/_archive/ 整个深度归档目录
- 合并 docs/ 到 doc/docs/，消除文档目录歧义
- 根目录论文移至 doc/papers/

### .gitignore 增强
- 新增 test_results.* 忽略规则
- 新增 *_test_save.lvz 忽略规则
- 新增 module/axiom_packages/test_saves/ 忽略规则

### 验证记录
- git status：工作目录干净
- CI workflow 检查：5 个 workflow 均使用 main/dev 分支
- 无自动提交脚本存在
- CONTRIBUTING.md 已禁止空提交

---

## v1.1.0-dev -- 2026-06-27 UI 内核解耦 + 头文件修复

### UI 系统重建
- **内核/UI 完全解耦** — UI 仅通过 `ui/L5-core/protocol/index.ts` 的 `KernelBridge` 接口与 C 内核通信
- **UI L1–L6 分层架构** — base / components / modules / shell / core / monitor 六层清晰分离
- **Mock Bridge** — `createMockBridge()` 完整模拟内核响应，前端可独立开发和测试
- **新增 UI 组件** — CanvasToolbar、Checkbox、CommandPalette、ExpressionList、Slider
- **嵌壳方案** — `ui/shells/` 下提供 VS Code 扩展和 Qt 独立窗口方案

### C 内核头文件修复
- 恢复 git 历史中的 63+ 源文件（版本 A `38310ea`、版本 B `e36f4b6`）
- 重写 11+ 个头文件以匹配 .c 实现：`geo_halfedge_mesh.h`、`simd_ops.h`、`interval_arithmetic.h`、`geometry_transform.h`、`geo_event_detect.h`、`algebraic_number.h`、`geo_topology.h`、`geo_invariant_type.h`、`high_dim.h`、`geometry_compress.h`、`lv_internal.h`、`euclidean_geometry.h`
- 新增 8 个头文件：`lv_config.h`、`preset_abstract_algebra.h`、`preset_name_defs.h`、`proof_rule_engine_internal.h`、`proof_session_internal.h`、`proof_version_internal.h`、`smt_theory_combiner.h`、`smt_trigger_engine.h`
- 清理 `core/include/lv/stubs/` 下的废弃预设桩文件

### 内核新增源文件
- `lv_config.c` — 独立配置管理系统
- `sparse_linear_algebra.c`、`status_codes.c`、`tikz_export.c` — 新源码

### 构建与打包
- 新增 `cmake/lv-config.cmake.in` + `cmake/lv.pc.in`，支持 `find_package(lv)` 和 `pkg-config`
- CMake 新增 UI 前端构建目标

### 验证记录
- 头文件修复通过 `cc -fsyntax-only` 逐个验证
- master/main 分支同步至同一提交
- README.md 全面重写：添加项目现状表、修正虚假 API 示例、补充 UI 系统描述

---

## v1.1.0-dev -- 2026-07-22 v1.8.0 工程优化批次

### 资源释放命名统一
- **36 个文件的 `_free` → `_destroy`** — 消除命名歧义，统一资源释放语义
- 涉及 layer3_geometry、layer4_reasoning 等核心层

### 内存分配器统一
- **11 个文件中替换 30+ `malloc`、30+ `realloc`、80+ `free`** — 统一使用 `lv_*` 内存分配器
- 覆盖 layer3_geometry、layer4_reasoning 等模块，消除裸内存操作

### 头文件依赖精简
- **4 个核心头文件** — `engine.h` 从 10 个依赖减至 5 个 + 2 个前向声明
- 累计移除 12 个 `#include`，加速编译

### 命名统一
- **全仓库 `lv00` → `lv`** — 代码、文件格式、注释中的所有 `lv00` 前缀消解为 `lv`

### 验证记录
- 137/137 目标构建通过，0 错误/警告
- 116/118 测试通过（2 个已知 flaky 测试）
- 0 原生 malloc/realloc/free 残留
- 12 个 `#include` 移除

---

## v1.1.0-dev -- 2026-07-23 v2.0.0 功能集成

### λ-演算核心集成
- **λ-项数据结构** — `LvLambdaTerm` 类型（Var/Abs/App），创建/销毁 API
- **β-归约实现** — `beta_reduce_match()` 端口模式检测、`beta_reduce_apply()` 子图复制重定向
- **λ-项 ↔ 约束图编译** — `lambda_to_graph.c`，λ-项编译为约束图函数块，支持双向转换
- **Church 编码测试** — Church 数字、加法、乘法、乘幂，验证 2² 完整归约
- **Y 组合子测试** — 单步展开、多步归约至阶乘（Y F 3 → 6），验证共享子图引用一致性

### 端口作用域系统完整化
- **GeomNode 三字段** — `namespace_depth`、`parent_block_id`、`is_formal_param` 生命周期管理
- **β-归约端口继承规则** — 形式参数引用、自由变量引用、内部局部引用三种情况处理
- **惰性复制（写时优化）** — 节点深拷贝仅在修改时触发

### 信任颜色系统完整化
- **TrustColor 枚举扩展** — 8 色：绿色、蓝色（未探索/受限/超出范围）、黄色、浅橙（oracle/爆炸）、琥珀色、深橙
- **着色传播逻辑** — 沿依赖关系向下游单向传播，颜色叠加规则
- **数值假设逃逸出口** — 位数熔断点永久降级为数值假设

### 基础设施补齐
- **约束模板双层测试框架** — 出厂测试集 + 用户测试集，正则形式强制验证
- **模块加载器增强** — `MAX_MODULE_DEPTH`（默认 32）、DFS 循环依赖检测、SHA-256 内容哈希
- **打包跨边界约束检查** — 检测跨边界非连接约束，提供提升/断开/取消三选项
- **新增头文件** — `lambda_term.h`

### 验证记录
- 全部阶段（A-D）构建通过，0 错误/警告
- 128+ 测试点全部通过
- Church 编码 2² → 4、Y 组合子阶乘 Y F 3 → 6 验证
- 所有 β-归约在 10000 步上限内完成
