# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-06-21 — CompCert-Lite 编译器形式化验证

### 新增 (Added) — 编译器形式化验证 (6轮)
- **R1: Lv00Lang + IR** — .lv00 源语言操作语义 + 中间表示 (193行, 8定理)
- **R2: Compiler + CompilerCorrectness** — 第一次真正的编译正确性证明, 替代 `by rfl` 假证明 (295行, 12定理)
- **R3: Cv00Lang + Cv00Memory** — C11 子集操作语义 + GMP 精确内存模型 (435行, 12定理)
- **R4: Codegen + CodegenCorrectness** — IR→C 结构安全证明 (544行, 14定理)
- **R5: UndefinedBehavior + Evidence** — 7种UB分类 + 零信任证据系统 (670行, 23定理)
- **R6: InteropCorrectness** — Coq/Lean4/OPML/GeoJSON/SVG 互操作正确性 (287行, 17定理)

### 新增 (Added) — 公理化基础
- **Hilbert 公理框架** (10文件): Basic/Incidence/Betweenness/Congruence/Parallel/Continuity/Order/HilbertAxioms/EuclideanPlane/Lv00Meta
- **定义模块** (6文件): GeometricAlgebraDefs/GeometryPresetDefs/ODESolverDefs/NumericDefs/PresetGeometryDefs/BootstrapDefs
- **覆盖率模块** (8文件): ConstraintPropagation/InteropSoundness/OrchestrationSoundness/AxiomDiscoveryTheory/FormulaSemantics/VisualLayerSoundness/MetaVerificationTheory/StreamingTheory
- **108→138 .lv00 语义规格** — 重建全部 bootstrap 规格
- **GMP 原语运行时** — primitive_runtime.c/h 全部 mpq_t 精确有理数 (零 double/float)

### 修复 (Fixed)
- **compiler_semantics_preservation := by rfl** → 真正的 induction/cases/simp 证明
- **GMP 统一**: 全部数学计算使用 mpq_t 精确有理数
- **分支统一**: master/main 合并为单一 GMP 精确计算分支
- **版本统一**: 全部文件 1.1.0 (lv00.h VERSION_MAJOR=1, CMake project VERSION 1.1.0)
- **目录整理**: modules→module, docs→doc, 删除空/废弃目录
- **.gitignore**: web/legacy → web/wasm

### 项目指标
| 指标 | v1.0 | v1.1.0 |
|:---|---:|---:|
| .lv00 | 167 | 138 |
| .lean | 70 | 81 |
| .py | 54 | 83 |
| .c | 44 | 232 |
| .lvz | 0 | 57 |
| 定理数 | ~208 | ~300 |

---

## [1.0.0-rc1] - 2026-06-21 — 技术债全面清零

### 新增 (Added)
- **lv00_impl_native.c**: 统一实现替代 17 个 C 桩
- **lv00_impl_upper.c**: L3-L10 全部 C API 实现
- **166 个 .lv00 语义规格**: 10 层 + preset + ROSE + spec 目录
- **57 个 Lean4 形式化定理文件**
- **test_runner.py + 5 份新测试**
- **7 份新文档**: 快速开始/架构/配置/测试/构建/API/形式化

### 修复 (Fixed)
- P0: 17 个 C 桩 → lv00_impl_native.c
- P1: 11 个胖 Python → .lv00
- P2: Lean4 覆盖率 17%→72%
- P3: L3-L10 全部 35 项 C 空壳 → lv00_impl_upper.c

### 已知限制 (Research Preview)
- C 编译未经环境验证
- Python 绑定需要编译后的 C 共享库
- Lean4 `lake build` 未运行 (需 mathlib4)
- GitHub Actions CI/CD 预期为红灯
