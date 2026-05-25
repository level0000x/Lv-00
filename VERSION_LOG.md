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

### 源码规整优化
- 为 28 个公开头文件中的 1097 个函数声明添加 LV00_PUBLIC_API 导出宏
- 核心模块：lv00_utils(85), constraint_graph(50), engine(31), context(42), proof(119), symbolic_coord(75)
- 模块 API：func_block(44), module(36), type_system(60), recursion(50), stream(46), debug(51), magic(78)
- 更新 .clang-format IncludeCategories 以匹配项目"对应头文件优先"惯例
- 代码风格检查：0 个 Tab 混入，命名风格统一 snake_case

### 工程化体系标准化
- 三级分支体系确认：main（稳定）/ dev（开发）/ exp（实验）
- 同步 dev 和 exp 分支到最新 main 状态
- 修正本地分支跟踪关系（main → origin/main, dev → origin/dev, exp → origin/exp）
- CI 配置验证：5 个 workflow 均使用 main/dev 分支触发
- PR 模板已存在且规范

### 待用户手动操作
- 在 GitHub Settings → Default branch 中将默认分支从 master 改为 main
- 改完后执行 `git push origin --delete master` 删除远程 master 分支
- 建议启用 main 分支的 Branch Protection Rules（要求 PR + CI 通过）
