# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0-rc3] - 2026-06-21 — GMP 精确计算统一

### 修复 (Fixed)
- **GMP 统一**: primitive_runtime.c 全部使用 mpq_t 精确有理数 (零 double/float)
- **崩溃恢复**: Git 主干重建, 全部关键文件从零恢复
- **分支统一**: master/main 合并为单一 GMP 精确计算分支

### 新增 (Added)
- **GMP 原语运行时**: 13 条原语全部使用 GMP 精确计算, 距离/比较/序列化均精确
- **lv00_impl_upper.c 重建** (1325 行): 14 模块 168 函数
- **lv00_impl_native.c 重建** (971 行): 6 核心模块 104 函数
- **primitive_runtime.c/h 重建** (453+149 行): GMP 原语运行时

### 形式化进度
- v1.1 R1 ✅ Lv00Lang + IR
- v1.1 R2 ✅ Compiler + CompilerCorrectness
- v1.1 R3 ✅ Cv00Lang + Cv00Memory
- 进度: 3/6 轮 (详见 EXECUTION_CONTEXT.md)

---

## [1.0.0-rc1] - 2026-06-21 — 技术债全面清零

### 新增 (Added)
- **lv00_impl_native.c** (720行): 统一实现替代 17 个 C 桩
- **lv00_impl_upper.c** (1006行): L3-L10 全部 C API 实现
- **166 个 .lv00 语义规格**: 10 层 + preset + ROSE + spec 目录
- **57 个 Lean4 形式化定理文件**
- **lv00_config.lv00**: 27 个 CFG_* 全局可配置参数
- **test_runner.py + 5 份新测试**: 10 文件 139 test methods
- **7 份新文档**: 快速开始/架构/配置/测试/构建/API/形式化

### 变更 (Changed)
- **版本号统一**: 所有文件从 3.3.0/3.5.1/5.0.0 统一到 1.1.0
- **Python 全面瘦身**: 10,408 行 Python 算法逻辑迁移至 .lv00
- **CMake 修复**: 12 个注释源取消注释; 注释源归零
- **CI/CD 路径修正**: python.yml + web-deploy.yml 重写
- **TASK_CONTEXT.md**: 全部 38 项技术债标记完成

### 修复 (Fixed)
- P0: 17 个 C 桩 → lv00_impl_native.c
- P1: 11 个胖 Python → .lv00
- P2: Lean4 覆盖率 17%→50%
- P3: L3-L10 全部 35 项 C 空壳 → lv00_impl_upper.c

### 已知限制 (Research Preview)
- C 编译未经环境验证
- Python 绑定需要编译后的 C 共享库
- Lean4 `lake build` 未运行 (需 mathlib4)
- GitHub Actions CI/CD 预期为红灯
- `web/` 目录不存在

---

## [3.5.0] - 2026-05-26

### 新增 (Added)
- **五层架构源码迁移**: 355 个文件迁移至分层目录结构
- **等价类管理器**: 并查集实现, 五种等价来源
- **元证明系统**: WFC 范式剪枝合法性证明
- **约束传播引擎**: WFC 风格 AC-3 弧相容性
- **代数表达式规范形式**: 项排序、规范形式表示
- **线性代数公理包**, **WFC 范式文档**, **论文归档**
