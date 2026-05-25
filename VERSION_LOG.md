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
